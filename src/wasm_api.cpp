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
#include "optimizer.h"
#include <cmath>
#include <algorithm>

static std::string last_source;
static CompiledProgram last_program;
static std::unique_ptr<VM> g_debug_vm;
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
    Optimizer::optimize(ast);

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
    g_debug_vm = std::make_unique<VM>(last_program);
    // Suppress standard console output so the web frontend gets clean output
    g_debug_vm->on_write = [](const std::string&) {};
    g_debug_vm->on_print = [](const std::string&) {};
    // Pushes default values instead of blocking on console input
    g_debug_vm->on_read = [](uint8_t type_code) -> Value {
        if (type_code == 1) return Value::make_float(0.0);
        else if (type_code == 2) return Value::make_bool(false);
        else if (type_code == 3) return Value::make_string("");
        else return Value::make_int(0);
    };
    return 1;
}

EMSCRIPTEN_KEEPALIVE
int wasm_debug_step() {
    if (!g_debug_vm) return 0;
    VMStepResult res = g_debug_vm->step();
    return (res == VMStepResult::OK) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_debug_current_line() {
    if (g_debug_vm->frames().empty()) return 0;
    CallFrame& frame = g_debug_vm->frames().back();
    const Chunk& chunk = g_debug_vm->program().functions[frame.fn_index].chunk;
    if (frame.ip < 0 || frame.ip >= static_cast<int>(chunk.lines.size())) return 0;
    return chunk.lines[frame.ip];
}

EMSCRIPTEN_KEEPALIVE
int wasm_debug_current_ip() {
    if (g_debug_vm->frames().empty()) return 0;
    return g_debug_vm->frames().back().ip;
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_stack() {
    std::ostringstream ss;
    int size = static_cast<int>(g_debug_vm->stack().size());
    for (int i = 0; i < size; ++i) {
        ss << "[" << i << "] " << g_debug_vm->stack()[i].to_string() << "\n";
    }
    string_buffer = ss.str();
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_variables() {
    std::ostringstream ss;
    if (g_debug_vm->frames().empty()) {
        string_buffer = "";
        return string_buffer.c_str();
    }
    int base = g_debug_vm->frames().back().base_pointer;
    const auto& locals = last_program.functions[g_debug_vm->frames().back().fn_index].debug_locals;
    for (const auto& loc : locals) {
        int idx = base + loc.slot;
        if (idx >= 0 && idx < static_cast<int>(g_debug_vm->stack().size())) {
            ss << loc.name << " = " << g_debug_vm->stack()[idx].to_string() << "\n";
        }
    }
    string_buffer = ss.str();
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_output() {
    string_buffer = g_debug_vm->output();
    return string_buffer.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_debug_bytecode() {
    if (g_debug_vm->frames().empty()) {
        string_buffer = "";
        return string_buffer.c_str();
    }
    CallFrame& frame = g_debug_vm->frames().back();
    const Chunk& chunk = g_debug_vm->program().functions[frame.fn_index].chunk;
    std::ostringstream ss;

    int offset = 0;
    while (offset < static_cast<int>(chunk.code.size())) {
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
