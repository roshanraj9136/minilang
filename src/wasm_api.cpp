#include <emscripten.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include "opcode.h"
#include "vm.h"
#include "disassembler.h"
#include <cmath>
#include <algorithm>

static std::string last_source;
static CompiledProgram last_program;
static std::vector<Value> debug_stack;
static std::vector<CallFrame> debug_frames;
static std::vector<Value> debug_globals;
static std::string debug_output;
static std::string string_buffer;

class WebErrorReporter {
public:
    std::string error_log;
    int error_count = 0;

    void report_error(const std::string& source, int line, int column, const std::string& message) {
        std::vector<std::string> lines;
        std::string current_line;
        std::istringstream stream(source);
        while (std::getline(stream, current_line)) {
            lines.push_back(current_line);
        }

        std::ostringstream ss;
        ss << "error[line " << line << ", col " << column << "]: " << message << "\n";
        if (line > 0 && line <= static_cast<int>(lines.size())) {
            ss << "    " << lines[line - 1] << "\n    ";
            for (int i = 1; i < column; ++i) {
                if (i - 1 < static_cast<int>(lines[i - 1].size()) && lines[line - 1][i - 1] == '\t') {
                    ss << "\t";
                } else {
                    ss << " ";
                }
            }
            ss << "^\n";
        }
        ss << "\n";
        error_log += ss.str();
        error_count++;
    }

    bool has_errors() const {
        return error_count > 0;
    }
};

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* wasm_compile(const char* source_code) {
    last_source = source_code;
    WebErrorReporter errors;
    Lexer lexer(last_source);

    std::stringstream buffer;
    std::streambuf* old_cerr = std::cerr.rdbuf(buffer.rdbuf());

    std::vector<Token> tokens;
    while (true) {
        Token tok = lexer.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::TOKEN_EOF) {
            break;
        }
        if (tok.type == TokenType::TOKEN_ERROR) {
            std::cerr.rdbuf(old_cerr);
            errors.report_error(last_source, tok.line, tok.column, tok.lexeme);
            string_buffer = errors.error_log;
            return string_buffer.c_str();
        }
    }

    ErrorReporter dummy_reporter;
    Parser parser(tokens, dummy_reporter, last_source);
    std::vector<StmtPtr> ast;
    try {
        ast = parser.parse();
    } catch (const std::exception& e) {
        std::cerr.rdbuf(old_cerr);
        string_buffer = "Parser error: " + std::string(e.what()) + "\n";
        return string_buffer.c_str();
    }

    if (dummy_reporter.has_errors()) {
        std::cerr.rdbuf(old_cerr);
        string_buffer = buffer.str();
        return string_buffer.c_str();
    }

    SemanticAnalyzer semantic(dummy_reporter, last_source);
    semantic.analyze(ast);

    if (dummy_reporter.has_errors()) {
        std::cerr.rdbuf(old_cerr);
        string_buffer = buffer.str();
        return string_buffer.c_str();
    }

    CodeGenerator codegen(dummy_reporter, last_source);
    last_program = codegen.generate(ast);

    if (dummy_reporter.has_errors()) {
        std::cerr.rdbuf(old_cerr);
        string_buffer = buffer.str();
        return string_buffer.c_str();
    }

    if (last_program.main_index < 0) {
        std::cerr.rdbuf(old_cerr);
        string_buffer = "Compilation error: Function 'main' not found.\n";
        return string_buffer.c_str();
    }

    std::cerr.rdbuf(old_cerr);
    string_buffer = "";
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_run() {
    if (last_program.main_index < 0) {
        string_buffer = "No compiled program to run.\n";
        return string_buffer.c_str();
    }

    VM vm(last_program);
    VMResult res = vm.run();

    if (res != VMResult::OK) {
        string_buffer = "Runtime Error:\n" + vm.output();
    } else {
        string_buffer = vm.output();
    }
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_disasm() {
    if (last_program.main_index < 0) {
        string_buffer = "No compiled program to disassemble.\n";
        return string_buffer.c_str();
    }
    string_buffer = Disassembler::disassemble(last_program);
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
int wasm_debug_start() {
    if (last_program.main_index < 0) return 0;
    debug_stack.clear();
    debug_frames.clear();
    debug_output.clear();
    debug_globals.clear();
    debug_globals.resize(last_program.global_count, Value::make_int(0));
    debug_frames.push_back(CallFrame{last_program.main_index, 0, 0});
    return 1;
}

EMSCRIPTEN_KEEPALIVE
int wasm_debug_step() {
    if (debug_frames.empty()) return 0;
    CallFrame& frame = debug_frames.back();
    Chunk& chunk = last_program.functions[frame.fn_index].chunk;

    if (frame.ip >= static_cast<int>(chunk.code.size())) return 0;

    auto read_byte = [&]() -> uint8_t {
        return chunk.code[frame.ip++];
    };

    auto read_short = [&]() -> uint16_t {
        uint8_t hi = read_byte();
        uint8_t lo = read_byte();
        return (hi << 8) | lo;
    };

    auto pop = [&]() -> Value {
        Value v = debug_stack.back();
        debug_stack.pop_back();
        return v;
    };

    auto peek = [&]() -> const Value& {
        return debug_stack.back();
    };

    auto push = [&](const Value& value) {
        debug_stack.push_back(value);
    };

    uint8_t instruction = read_byte();
    switch (static_cast<OpCode>(instruction)) {
        case OpCode::PUSH_INT: {
            uint16_t idx = read_short();
            push(chunk.constants[idx]);
            break;
        }
        case OpCode::PUSH_FLOAT: {
            uint16_t idx = read_short();
            push(chunk.constants[idx]);
            break;
        }
        case OpCode::PUSH_TRUE:
            push(Value::make_bool(true));
            break;
        case OpCode::PUSH_FALSE:
            push(Value::make_bool(false));
            break;
        case OpCode::PUSH_STRING: {
            uint16_t idx = read_short();
            push(chunk.constants[idx]);
            break;
        }
        case OpCode::POP:
            pop();
            break;
        case OpCode::ADD: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_int(a.as_int() + b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_float(a.as_float() + b.as_float()));
            } else if (a.is_string() && b.is_string()) {
                push(Value::make_string(a.as_string() + b.as_string()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::SUB: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_int(a.as_int() - b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_float(a.as_float() - b.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::MUL: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_int(a.as_int() * b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_float(a.as_float() * b.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::DIV: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                if (b.as_int() == 0) return 0;
                push(Value::make_int(a.as_int() / b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                if (b.as_float() == 0.0) return 0;
                push(Value::make_float(a.as_float() / b.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::MOD: {
            Value b = pop();
            Value a = pop();
            if (b.as_int() == 0) return 0;
            push(Value::make_int(a.as_int() % b.as_int()));
            break;
        }
        case OpCode::NEGATE: {
            Value a = pop();
            if (a.is_int()) {
                push(Value::make_int(-a.as_int()));
            } else if (a.is_float()) {
                push(Value::make_float(-a.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::EQ: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_bool(a.as_int() == b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_bool(a.as_float() == b.as_float()));
            } else if (a.is_bool() && b.is_bool()) {
                push(Value::make_bool(a.as_bool() == b.as_bool()));
            } else if (a.is_string() && b.is_string()) {
                push(Value::make_bool(a.as_string() == b.as_string()));
            } else if (a.is_pointer() && b.is_pointer()) {
                push(Value::make_bool(a.as_pointer() == b.as_pointer()));
            } else if (a.is_array() && b.is_array()) {
                push(Value::make_bool(a.as_array() == b.as_array()));
            } else {
                push(Value::make_bool(false));
            }
            break;
        }
        case OpCode::NEQ: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_bool(a.as_int() != b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_bool(a.as_float() != b.as_float()));
            } else if (a.is_bool() && b.is_bool()) {
                push(Value::make_bool(a.as_bool() != b.as_bool()));
            } else if (a.is_string() && b.is_string()) {
                push(Value::make_bool(a.as_string() != b.as_string()));
            } else if (a.is_pointer() && b.is_pointer()) {
                push(Value::make_bool(a.as_pointer() != b.as_pointer()));
            } else if (a.is_array() && b.is_array()) {
                push(Value::make_bool(a.as_array() != b.as_array()));
            } else {
                push(Value::make_bool(true));
            }
            break;
        }
        case OpCode::LT: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_bool(a.as_int() < b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_bool(a.as_float() < b.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::GT: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_bool(a.as_int() > b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_bool(a.as_float() > b.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::LTE: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_bool(a.as_int() <= b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_bool(a.as_float() <= b.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::GTE: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                push(Value::make_bool(a.as_int() >= b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                push(Value::make_bool(a.as_float() >= b.as_float()));
            } else {
                return 0;
            }
            break;
        }
        case OpCode::NOT: {
            Value a = pop();
            push(Value::make_bool(!a.as_bool()));
            break;
        }
        case OpCode::AND: {
            Value b = pop();
            Value a = pop();
            push(Value::make_bool(a.as_bool() && b.as_bool()));
            break;
        }
        case OpCode::OR: {
            Value b = pop();
            Value a = pop();
            push(Value::make_bool(a.as_bool() || b.as_bool()));
            break;
        }
        case OpCode::LOAD_LOCAL: {
            uint16_t slot = read_short();
            push(debug_stack[frame.base_pointer + slot]);
            break;
        }
        case OpCode::STORE_LOCAL: {
            uint16_t slot = read_short();
            debug_stack[frame.base_pointer + slot] = peek();
            break;
        }
        case OpCode::LOAD_LOCAL_ADDR: {
            uint16_t slot = read_short();
            uint32_t addr = frame.base_pointer + slot;
            push(Value::make_pointer(addr));
            break;
        }
        case OpCode::LOAD_DEREF: {
            Value ptr = pop();
            if (!ptr.is_pointer()) return 0;
            uint32_t addr = ptr.as_pointer();
            if (addr >= debug_stack.size()) return 0;
            push(debug_stack[addr]);
            break;
        }
        case OpCode::STORE_DEREF: {
            Value ptr = pop();
            Value val = peek();
            if (!ptr.is_pointer()) return 0;
            uint32_t addr = ptr.as_pointer();
            if (addr >= debug_stack.size()) return 0;
            debug_stack[addr] = val;
            break;
        }
        case OpCode::NEW_ARRAY: {
            uint8_t type_code = read_byte();
            Value size_val = pop();
            int64_t size = size_val.as_int();
            if (size < 0) return 0;
            Value default_val;
            if (type_code == 1) default_val = Value::make_float(0.0);
            else if (type_code == 2) default_val = Value::make_bool(false);
            else if (type_code == 3) default_val = Value::make_string("");
            else default_val = Value::make_int(0);

            auto arr = std::make_shared<std::vector<Value>>(size, default_val);
            push(Value::make_array(arr));
            break;
        }
        case OpCode::ARRAY_LOAD: {
            Value idx_val = pop();
            Value arr_val = pop();
            if (arr_val.is_string()) {
                int64_t index = idx_val.as_int();
                const std::string& s = arr_val.as_string();
                if (index < 0 || index >= static_cast<int64_t>(s.size())) return 0;
                push(Value::make_int(static_cast<int64_t>(static_cast<unsigned char>(s[index]))));
                break;
            }
            if (!arr_val.is_array()) return 0;
            int64_t index = idx_val.as_int();
            auto arr = arr_val.as_array();
            if (index < 0 || index >= static_cast<int64_t>(arr->size())) return 0;
            push((*arr)[index]);
            break;
        }
        case OpCode::ARRAY_STORE: {
            Value val = pop();
            Value idx_val = pop();
            Value arr_val = pop();
            if (!arr_val.is_array()) return 0;
            int64_t index = idx_val.as_int();
            auto arr = arr_val.as_array();
            if (index < 0 || index >= static_cast<int64_t>(arr->size())) return 0;
            (*arr)[index] = val;
            push(val);
            break;
        }
        case OpCode::ARRAY_LENGTH: {
            Value arr_val = pop();
            if (arr_val.is_string()) {
                push(Value::make_int(static_cast<int64_t>(arr_val.as_string().size())));
                break;
            }
            if (!arr_val.is_array()) return 0;
            push(Value::make_int(static_cast<int64_t>(arr_val.as_array()->size())));
            break;
        }
        case OpCode::INT_TO_FLOAT: {
            Value val = pop();
            push(Value::make_float(static_cast<double>(val.as_int())));
            break;
        }
        case OpCode::FLOAT_TO_INT: {
            Value val = pop();
            push(Value::make_int(static_cast<int64_t>(val.as_float())));
            break;
        }
        case OpCode::BIT_AND: {
            Value b = pop();
            Value a = pop();
            push(Value::make_int(a.as_int() & b.as_int()));
            break;
        }
        case OpCode::BIT_OR: {
            Value b = pop();
            Value a = pop();
            push(Value::make_int(a.as_int() | b.as_int()));
            break;
        }
        case OpCode::BIT_XOR: {
            Value b = pop();
            Value a = pop();
            push(Value::make_int(a.as_int() ^ b.as_int()));
            break;
        }
        case OpCode::BIT_NOT: {
            Value val = pop();
            push(Value::make_int(~val.as_int()));
            break;
        }
        case OpCode::BIT_SHL: {
            Value b = pop();
            Value a = pop();
            push(Value::make_int(a.as_int() << b.as_int()));
            break;
        }
        case OpCode::BIT_SHR: {
            Value b = pop();
            Value a = pop();
            push(Value::make_int(a.as_int() >> b.as_int()));
            break;
        }
        case OpCode::LOAD_GLOBAL: {
            uint16_t index = read_short();
            push(debug_globals[index]);
            break;
        }
        case OpCode::STORE_GLOBAL: {
            uint16_t index = read_short();
            debug_globals[index] = peek();
            break;
        }
        case OpCode::CALL_BUILTIN: {
            uint8_t id = read_byte();
            if (id == 0) { // sqrt
                Value val = pop();
                push(Value::make_float(std::sqrt(val.is_float() ? val.as_float() : static_cast<double>(val.as_int()))));
            } else if (id == 1) { // pow
                Value b = pop();
                Value a = pop();
                double base = a.is_float() ? a.as_float() : static_cast<double>(a.as_int());
                double exponent = b.is_float() ? b.as_float() : static_cast<double>(b.as_int());
                push(Value::make_float(std::pow(base, exponent)));
            } else if (id == 2) { // abs
                Value val = pop();
                push(Value::make_int(std::abs(val.as_int())));
            } else if (id == 3) { // min
                Value b = pop();
                Value a = pop();
                if (a.is_float() || b.is_float()) {
                    double da = a.is_float() ? a.as_float() : static_cast<double>(a.as_int());
                    double db = b.is_float() ? b.as_float() : static_cast<double>(b.as_int());
                    push(Value::make_float(std::min(da, db)));
                } else {
                    push(Value::make_int(std::min(a.as_int(), b.as_int())));
                }
            } else if (id == 4) { // max
                Value b = pop();
                Value a = pop();
                if (a.is_float() || b.is_float()) {
                    double da = a.is_float() ? a.as_float() : static_cast<double>(a.as_int());
                    double db = b.is_float() ? b.as_float() : static_cast<double>(b.as_int());
                    push(Value::make_float(std::max(da, db)));
                } else {
                    push(Value::make_int(std::max(a.as_int(), b.as_int())));
                }
            }
            break;
        }
        case OpCode::WRITE: {
            Value val = pop();
            debug_output += val.to_string();
            break;
        }
        case OpCode::READ: {
            uint8_t type_code = read_byte();
            if (type_code == 1) push(Value::make_float(0.0));
            else if (type_code == 2) push(Value::make_bool(false));
            else if (type_code == 3) push(Value::make_string(""));
            else push(Value::make_int(0));
            break;
        }
        case OpCode::FILL_ARRAY: {
            Value fill_val = pop();
            Value arr_val = pop();
            auto arr = arr_val.as_array();
            for (size_t i = 0; i < arr->size(); ++i) {
                if (fill_val.is_array()) {
                    auto copied_vec = std::make_shared<std::vector<Value>>(*fill_val.as_array());
                    (*arr)[i] = Value::make_array(copied_vec);
                } else {
                    (*arr)[i] = fill_val;
                }
            }
            push(arr_val);
            break;
        }
        case OpCode::NEW_PAIR: {
            Value second = pop();
            Value first = pop();
            auto arr = std::make_shared<std::vector<Value>>();
            arr->push_back(first);
            arr->push_back(second);
            push(Value::make_array(arr));
            break;
        }
        case OpCode::VECTOR_PUSH_BACK: {
            Value val = pop();
            Value arr_val = pop();
            arr_val.as_array()->push_back(val);
            push(Value::make_int(0));
            break;
        }
        case OpCode::VECTOR_POP_BACK: {
            Value arr_val = pop();
            auto arr = arr_val.as_array();
            if (!arr->empty()) arr->pop_back();
            push(Value::make_int(0));
            break;
        }
        case OpCode::CONTAINER_EMPTY: {
            Value arr_val = pop();
            push(Value::make_bool(arr_val.as_array()->empty()));
            break;
        }
        case OpCode::CONTAINER_CLEAR: {
            Value arr_val = pop();
            arr_val.as_array()->clear();
            push(Value::make_int(0));
            break;
        }
        case OpCode::QUEUE_POP: {
            Value arr_val = pop();
            auto arr = arr_val.as_array();
            if (!arr->empty()) arr->erase(arr->begin());
            push(Value::make_int(0));
            break;
        }
        case OpCode::QUEUE_FRONT: {
            Value arr_val = pop();
            auto arr = arr_val.as_array();
            push(arr->front());
            break;
        }
        case OpCode::STACK_TOP: {
            Value arr_val = pop();
            auto arr = arr_val.as_array();
            push(arr->back());
            break;
        }
        case OpCode::PUSH_NULL: {
            push(Value::make_int(0));
            break;
        }
        case OpCode::DUP: {
            push(peek());
            break;
        }
        case OpCode::JUMP: {
            uint16_t target = read_short();
            frame.ip = target;
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            uint16_t target = read_short();
            Value condition = pop();
            if (!condition.as_bool()) {
                frame.ip = target;
            }
            break;
        }
        case OpCode::CALL: {
            uint16_t fn_index = read_short();
            uint8_t argc = read_byte();
            CallFrame next_frame;
            next_frame.fn_index = fn_index;
            next_frame.ip = 0;
            next_frame.base_pointer = static_cast<int>(debug_stack.size()) - argc;
            debug_frames.push_back(next_frame);
            break;
        }
        case OpCode::RETURN: {
            Value result = pop();
            int base = frame.base_pointer;
            debug_frames.pop_back();
            debug_stack.resize(base);
            if (debug_frames.empty()) return 0;
            debug_stack.push_back(result);
            break;
        }
        case OpCode::PRINT: {
            Value val = pop();
            debug_output += val.to_string() + "\n";
            break;
        }
        case OpCode::HALT:
            return 0;
    }
    return debug_frames.empty() ? 0 : 1;
}

EMSCRIPTEN_KEEPALIVE
int wasm_debug_current_line() {
    if (debug_frames.empty()) return 0;
    CallFrame& frame = debug_frames.back();
    Chunk& chunk = last_program.functions[frame.fn_index].chunk;
    if (frame.ip < 0 || frame.ip >= static_cast<int>(chunk.lines.size())) return 0;
    return chunk.lines[frame.ip];
}

EMSCRIPTEN_KEEPALIVE
int wasm_debug_current_ip() {
    if (debug_frames.empty()) return 0;
    return debug_frames.back().ip;
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_stack() {
    std::ostringstream ss;
    int size = static_cast<int>(debug_stack.size());
    for (int i = 0; i < size; ++i) {
        ss << "[" << i << "] " << debug_stack[i].to_string() << "\n";
    }
    string_buffer = ss.str();
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_variables() {
    std::ostringstream ss;
    if (debug_frames.empty()) {
        string_buffer = "";
        return string_buffer.c_str();
    }
    int base = debug_frames.back().base_pointer;
    const auto& locals = last_program.functions[debug_frames.back().fn_index].debug_locals;
    for (const auto& loc : locals) {
        int idx = base + loc.slot;
        if (idx >= 0 && idx < static_cast<int>(debug_stack.size())) {
            ss << loc.name << " = " << debug_stack[idx].to_string() << "\n";
        }
    }
    string_buffer = ss.str();
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_output() {
    string_buffer = debug_output;
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_bytecode() {
    if (debug_frames.empty()) {
        string_buffer = "";
        return string_buffer.c_str();
    }
    CallFrame& frame = debug_frames.back();
    Chunk& chunk = last_program.functions[frame.fn_index].chunk;
    std::ostringstream ss;

    int offset = 0;
    while (offset < static_cast<int>(chunk.code.size())) {
        int line = chunk.lines[offset];
        bool is_current = (offset == frame.ip);

        ss << (is_current ? "> " : "  ") 
           << std::setw(4) << std::setfill('0') << offset << " ";

        uint8_t op = chunk.code[offset];
        OpCode opcode = static_cast<OpCode>(op);
        ss << opcode_to_string(opcode);

        if (opcode == OpCode::PUSH_INT || opcode == OpCode::PUSH_STRING) {
            uint16_t arg = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
            ss << " " << arg << " (" << chunk.constants[arg].to_string() << ")";
            offset += 3;
        } else if (opcode == OpCode::LOAD_LOCAL || opcode == OpCode::STORE_LOCAL) {
            uint16_t arg = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
            ss << " " << arg;
            const auto& locals = last_program.functions[frame.fn_index].debug_locals;
            for (const auto& loc : locals) {
                if (loc.slot == arg) {
                    ss << " (" << loc.name << ")";
                    break;
                }
            }
            offset += 3;
        } else if (opcode == OpCode::JUMP || opcode == OpCode::JUMP_IF_FALSE) {
            uint16_t arg = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
            ss << " " << std::setw(4) << std::setfill('0') << arg;
            offset += 3;
        } else if (opcode == OpCode::CALL) {
            uint16_t fn_idx = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
            uint8_t argc = chunk.code[offset + 3];
            ss << " fn:" << fn_idx << " argc:" << static_cast<int>(argc);
            offset += 4;
        } else if (opcode == OpCode::LOAD_GLOBAL || opcode == OpCode::STORE_GLOBAL) {
            uint16_t arg = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
            ss << " " << arg;
            offset += 3;
        } else if (opcode == OpCode::CALL_BUILTIN || opcode == OpCode::READ) {
            uint8_t arg = chunk.code[offset + 1];
            ss << " " << static_cast<int>(arg);
            offset += 2;
        } else {
            offset += 1;
        }
        ss << "\n";
    }

    string_buffer = ss.str();
    return string_buffer.c_str();
}

}
