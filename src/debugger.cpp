#include "debugger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

Debugger::Debugger(const std::string& source, const CompiledProgram& program)
    : source_(source), program_(program), stepping_(true) {
    if (program_.main_index >= 0) {
        call_frames_.push_back(CallFrame{program_.main_index, 0, 0});
    }
    globals_.resize(program_.global_count, Value::make_int(0));
}

void Debugger::clear_screen() {
    std::cout << "\033[2J\033[H";
}

void Debugger::move_cursor(int row, int col) {
    std::cout << "\033[" << row << ";" << col << "H";
}

void Debugger::render_box(int row, int col, int height, int width, const std::string& title) {
    std::string reset = "\033[0m";
    std::string dim = "\033[2m";
    std::string bold = "\033[1m";

    move_cursor(row, col);
    std::cout << dim << "┌─ " << bold << title << reset << dim;
    for (int i = 0; i < width - 5 - static_cast<int>(title.size()); ++i) {
        std::cout << "─";
    }
    std::cout << "┐" << reset;

    for (int r = 1; r < height - 1; ++r) {
        move_cursor(row + r, col);
        std::cout << dim << "│" << reset;
        move_cursor(row + r, col + width - 1);
        std::cout << dim << "│" << reset;
    }

    move_cursor(row + height - 1, col);
    std::cout << dim << "└";
    for (int i = 0; i < width - 2; ++i) {
        std::cout << "─";
    }
    std::cout << "┘" << reset;
}

int Debugger::current_source_line() {
    if (call_frames_.empty()) return 0;
    int ip = current_frame().ip;
    if (ip < 0 || ip >= static_cast<int>(current_chunk().lines.size())) return 0;
    return current_chunk().lines[ip];
}

std::string Debugger::current_fn_name() {
    if (call_frames_.empty()) return "";
    return program_.functions[current_frame().fn_index].name;
}

Chunk& Debugger::current_chunk() {
    return program_.functions[current_frame().fn_index].chunk;
}

CallFrame& Debugger::current_frame() {
    return call_frames_.back();
}

uint8_t Debugger::read_byte() {
    return current_chunk().code[current_frame().ip++];
}

uint16_t Debugger::read_short() {
    uint8_t high = read_byte();
    uint8_t low = read_byte();
    return (high << 8) | low;
}

Value Debugger::pop() {
    Value v = stack_.back();
    stack_.pop_back();
    return v;
}

const Value& Debugger::peek_stack() const {
    return stack_.back();
}

void Debugger::push(const Value& value) {
    stack_.push_back(value);
}

void Debugger::render_source_panel(int row, int col, int height, int width) {
    render_box(row, col, height, width, "Source Code");
    std::vector<std::string> lines;
    std::string current_line;
    std::istringstream stream(source_);
    while (std::getline(stream, current_line)) {
        lines.push_back(current_line);
    }

    int cur_line = current_source_line();
    int start_idx = std::max(0, cur_line - height / 2);
    int end_idx = std::min(static_cast<int>(lines.size()), start_idx + height - 2);

    for (int i = start_idx; i < end_idx; ++i) {
        move_cursor(row + 1 + (i - start_idx), col + 2);
        std::string mark = (i + 1 == cur_line) ? "\033[1;36m>\033[0m" : " ";
        std::string prefix = (breakpoints_.count(i + 1)) ? "\033[1;31m•\033[0m" : " ";
        std::string line_content = lines[i];
        if (line_content.size() > static_cast<size_t>(width - 8)) {
            line_content = line_content.substr(0, width - 11) + "...";
        }
        std::cout << prefix << mark << " " << std::setw(3) << (i + 1) << " "
                  << ((i + 1 == cur_line) ? "\033[1;36m" : "") << line_content << "\033[0m";
    }
}

void Debugger::render_bytecode_panel(int row, int col, int height, int width) {
    render_box(row, col, height, width, "Bytecode: " + current_fn_name());
    if (call_frames_.empty()) return;

    Chunk& chk = current_chunk();
    int current_ip = current_frame().ip;

    std::vector<std::pair<int, std::string>> insts;
    int offset = 0;
    while (offset < static_cast<int>(chk.code.size())) {
        int temp_offset = offset;
        uint8_t op = chk.code[temp_offset];
        OpCode opcode = static_cast<OpCode>(op);
        std::string name = opcode_to_string(opcode);
        std::string details = "";

        if (opcode == OpCode::PUSH_INT || opcode == OpCode::PUSH_STRING) {
            uint16_t arg = (chk.code[temp_offset + 1] << 8) | chk.code[temp_offset + 2];
            details = " " + std::to_string(arg) + " (" + chk.constants[arg].to_string() + ")";
            temp_offset += 3;
        } else if (opcode == OpCode::LOAD_LOCAL || opcode == OpCode::STORE_LOCAL) {
            uint16_t arg = (chk.code[temp_offset + 1] << 8) | chk.code[temp_offset + 2];
            details = " " + std::to_string(arg);
            const auto& debug_locals = program_.functions[current_frame().fn_index].debug_locals;
            for (const auto& loc : debug_locals) {
                if (loc.slot == arg) {
                    details += " (" + loc.name + ")";
                    break;
                }
            }
            temp_offset += 3;
        } else if (opcode == OpCode::JUMP || opcode == OpCode::JUMP_IF_FALSE) {
            uint16_t arg = (chk.code[temp_offset + 1] << 8) | chk.code[temp_offset + 2];
            details = " " + std::to_string(arg);
            temp_offset += 3;
        } else if (opcode == OpCode::CALL) {
            uint16_t fn_idx = (chk.code[temp_offset + 1] << 8) | chk.code[temp_offset + 2];
            uint8_t argc = chk.code[temp_offset + 3];
            details = " fn:" + std::to_string(fn_idx) + " argc:" + std::to_string(argc);
            temp_offset += 4;
        } else {
            temp_offset += 1;
        }

        insts.push_back({offset, name + details});
        offset = temp_offset;
    }

    int highlight_idx = 0;
    for (size_t i = 0; i < insts.size(); ++i) {
        if (insts[i].first == current_ip) {
            highlight_idx = i;
            break;
        }
    }

    int start_idx = std::max(0, highlight_idx - height / 2);
    int end_idx = std::min(static_cast<int>(insts.size()), start_idx + height - 2);

    for (int i = start_idx; i < end_idx; ++i) {
        move_cursor(row + 1 + (i - start_idx), col + 2);
        bool is_current = (insts[i].first == current_ip);
        std::string mark = is_current ? "\033[1;32m>\033[0m" : " ";
        std::string line_content = insts[i].second;
        if (line_content.size() > static_cast<size_t>(width - 10)) {
            line_content = line_content.substr(0, width - 13) + "...";
        }
        std::cout << mark << " " << std::setw(4) << std::setfill('0') << insts[i].first << " "
                  << (is_current ? "\033[1;32m" : "") << line_content << "\033[0m";
    }
}

void Debugger::render_stack_panel(int row, int col, int height, int width) {
    render_box(row, col, height, width, "Stack");
    int size = static_cast<int>(stack_.size());
    int visible = std::min(size, height - 2);

    for (int i = 0; i < visible; ++i) {
        int idx = size - 1 - i;
        move_cursor(row + 1 + i, col + 2);
        std::string val_str = stack_[idx].to_string();
        if (val_str.size() > static_cast<size_t>(width - 8)) {
            val_str = val_str.substr(0, width - 11) + "...";
        }
        std::cout << "[" << std::setw(2) << idx << "] \033[1;33m" << val_str << "\033[0m";
        if (idx == size - 1) {
            std::cout << " \033[1;33m←\033[0m";
        }
    }
}

void Debugger::render_variables_panel(int row, int col, int height, int width) {
    render_box(row, col, height, width, "Variables");
    if (call_frames_.empty()) return;

    int base = current_frame().base_pointer;
    const auto& debug_locals = program_.functions[current_frame().fn_index].debug_locals;

    int count = 0;
    for (const auto& loc : debug_locals) {
        int stack_idx = base + loc.slot;
        if (stack_idx >= 0 && stack_idx < static_cast<int>(stack_.size())) {
            if (count >= height - 2) break;
            move_cursor(row + 1 + count, col + 2);
            std::string val_str = stack_[stack_idx].to_string();
            if (val_str.size() > static_cast<size_t>(width - 25)) {
                val_str = val_str.substr(0, width - 28) + "...";
            }
            std::cout << std::setw(12) << loc.name << " = \033[1;35m" << val_str << "\033[0m";
            count++;
        }
    }
}

void Debugger::render_controls(int row) {
    std::string reset = "\033[0m";
    std::string bold = "\033[1m";
    std::string cyan = "\033[36m";

    move_cursor(row, 2);
    std::cout << bold << "[S]tep  [N]ext  [C]ontinue  [B]reakpoint <line>  [Q]uit" << reset;
    move_cursor(row, 60);
    std::cout << bold << "IP: " << cyan << std::setw(4) << std::setfill('0') << (call_frames_.empty() ? 0 : current_frame().ip) << reset;
}

void Debugger::render() {
    clear_screen();
    render_source_panel(1, 1, 12, 45);
    render_bytecode_panel(1, 47, 12, 34);
    render_stack_panel(1, 82, 22, 20);
    render_variables_panel(14, 1, 9, 79);
    render_controls(24);
    move_cursor(25, 1);
    std::cout << std::flush;
}

bool Debugger::execute_one_instruction() {
    if (call_frames_.empty()) return false;
    if (current_frame().ip >= static_cast<int>(current_chunk().code.size())) return false;

    uint8_t instruction = read_byte();
    switch (static_cast<OpCode>(instruction)) {
        case OpCode::PUSH_INT: {
            uint16_t idx = read_short();
            push(current_chunk().constants[idx]);
            break;
        }
        case OpCode::PUSH_FLOAT: {
            uint16_t idx = read_short();
            push(current_chunk().constants[idx]);
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
            push(current_chunk().constants[idx]);
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
                return false;
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
                return false;
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
                return false;
            }
            break;
        }
        case OpCode::DIV: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                if (b.as_int() == 0) return false;
                push(Value::make_int(a.as_int() / b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                if (b.as_float() == 0.0) return false;
                push(Value::make_float(a.as_float() / b.as_float()));
            } else {
                return false;
            }
            break;
        }
        case OpCode::MOD: {
            Value b = pop();
            Value a = pop();
            if (b.as_int() == 0) return false;
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
                return false;
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
                return false;
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
                return false;
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
                return false;
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
                return false;
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
            push(stack_[current_frame().base_pointer + slot]);
            break;
        }
        case OpCode::STORE_LOCAL: {
            uint16_t slot = read_short();
            stack_[current_frame().base_pointer + slot] = peek_stack();
            break;
        }
        case OpCode::LOAD_LOCAL_ADDR: {
            uint16_t slot = read_short();
            uint32_t addr = current_frame().base_pointer + slot;
            push(Value::make_pointer(addr));
            break;
        }
        case OpCode::LOAD_DEREF: {
            Value ptr = pop();
            if (!ptr.is_pointer()) return false;
            uint32_t addr = ptr.as_pointer();
            if (addr >= stack_.size()) return false;
            push(stack_[addr]);
            break;
        }
        case OpCode::STORE_DEREF: {
            Value ptr = pop();
            Value val = peek_stack();
            if (!ptr.is_pointer()) return false;
            uint32_t addr = ptr.as_pointer();
            if (addr >= stack_.size()) return false;
            stack_[addr] = val;
            break;
        }
        case OpCode::NEW_ARRAY: {
            uint8_t type_code = read_byte();
            Value size_val = pop();
            int64_t size = size_val.as_int();
            if (size < 0) return false;
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
            if (!arr_val.is_array()) return false;
            int64_t index = idx_val.as_int();
            auto arr = arr_val.as_array();
            if (index < 0 || index >= static_cast<int64_t>(arr->size())) return false;
            push((*arr)[index]);
            break;
        }
        case OpCode::ARRAY_STORE: {
            Value val = pop();
            Value idx_val = pop();
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
            int64_t index = idx_val.as_int();
            auto arr = arr_val.as_array();
            if (index < 0 || index >= static_cast<int64_t>(arr->size())) return false;
            (*arr)[index] = val;
            push(val);
            break;
        }
        case OpCode::ARRAY_LENGTH: {
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
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
            push(globals_[index]);
            break;
        }
        case OpCode::STORE_GLOBAL: {
            uint16_t index = read_short();
            globals_[index] = peek_stack();
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
        case OpCode::PUSH_NULL: {
            push(Value::make_int(0));
            break;
        }
        case OpCode::DUP: {
            push(peek_stack());
            break;
        }
        case OpCode::JUMP: {
            uint16_t target = read_short();
            current_frame().ip = target;
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            uint16_t target = read_short();
            Value condition = pop();
            if (!condition.as_bool()) {
                current_frame().ip = target;
            }
            break;
        }
        case OpCode::CALL: {
            uint16_t fn_index = read_short();
            uint8_t argc = read_byte();
            CallFrame frame;
            frame.fn_index = fn_index;
            frame.ip = 0;
            frame.base_pointer = static_cast<int>(stack_.size()) - argc;
            call_frames_.push_back(frame);
            break;
        }
        case OpCode::RETURN: {
            Value result = pop();
            int base = current_frame().base_pointer;
            call_frames_.pop_back();
            stack_.resize(base);
            if (call_frames_.empty()) return false;
            push(result);
            break;
        }
        case OpCode::WRITE: {
            Value val = pop();
            output_log_ += val.to_string();
            break;
        }
        case OpCode::READ: {
            uint8_t type_code = read_byte();
            if (type_code == 0) { // INT
                int64_t v = 0;
                if (std::cin >> v) {
                    push(Value::make_int(v));
                } else {
                    push(Value::make_int(0));
                }
            } else if (type_code == 1) { // FLOAT
                double v = 0.0;
                if (std::cin >> v) {
                    push(Value::make_float(v));
                } else {
                    push(Value::make_float(0.0));
                }
            } else if (type_code == 2) { // BOOL
                bool v = false;
                if (std::cin >> std::boolalpha >> v) {
                    push(Value::make_bool(v));
                } else {
                    push(Value::make_bool(false));
                }
            } else if (type_code == 3) { // STRING
                std::string v;
                if (std::cin >> v) {
                    push(Value::make_string(v));
                } else {
                    push(Value::make_string(""));
                }
            }
            break;
        }
        case OpCode::FILL_ARRAY: {
            Value fill_val = pop();
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
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
            if (!arr_val.is_array()) return false;
            arr_val.as_array()->push_back(val);
            push(Value::make_int(0));
            break;
        }
        case OpCode::VECTOR_POP_BACK: {
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
            auto arr = arr_val.as_array();
            if (!arr->empty()) arr->pop_back();
            push(Value::make_int(0));
            break;
        }
        case OpCode::CONTAINER_EMPTY: {
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
            push(Value::make_bool(arr_val.as_array()->empty()));
            break;
        }
        case OpCode::CONTAINER_CLEAR: {
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
            arr_val.as_array()->clear();
            push(Value::make_int(0));
            break;
        }
        case OpCode::QUEUE_POP: {
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
            auto arr = arr_val.as_array();
            if (!arr->empty()) arr->erase(arr->begin());
            push(Value::make_int(0));
            break;
        }
        case OpCode::QUEUE_FRONT: {
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
            auto arr = arr_val.as_array();
            if (arr->empty()) return false;
            push(arr->front());
            break;
        }
        case OpCode::STACK_TOP: {
            Value arr_val = pop();
            if (!arr_val.is_array()) return false;
            auto arr = arr_val.as_array();
            if (arr->empty()) return false;
            push(arr->back());
            break;
        }
        case OpCode::PRINT: {
            Value val = pop();
            output_log_ += val.to_string() + "\n";
            break;
        }
        case OpCode::HALT:
            return false;
    }
    return true;
}

void Debugger::run() {
    while (true) {
        render();
        int cur_line = current_source_line();
        if (stepping_ || breakpoints_.count(cur_line)) {
            stepping_ = true;
            move_cursor(25, 1);
            std::cout << "\033[1;36mdebug>\033[0m ";
            std::string cmd;
            std::getline(std::cin, cmd);

            if (cmd.empty() || cmd == "s" || cmd == "step") {
                if (!execute_one_instruction()) {
                    render();
                    move_cursor(25, 1);
                    std::cout << "Program finished execution.\n";
                    return;
                }
            } else if (cmd == "n" || cmd == "next") {
                int start_line = current_source_line();
                while (current_source_line() == start_line) {
                    if (!execute_one_instruction()) {
                        render();
                        move_cursor(25, 1);
                        std::cout << "Program finished execution.\n";
                        return;
                    }
                }
            } else if (cmd == "c" || cmd == "continue") {
                stepping_ = false;
            } else if (cmd.rfind("b ", 0) == 0) {
                try {
                    int bp_line = std::stoi(cmd.substr(2));
                    if (breakpoints_.count(bp_line)) {
                        breakpoints_.erase(bp_line);
                    } else {
                        breakpoints_.insert(bp_line);
                    }
                } catch (...) {}
            } else if (cmd == "q" || cmd == "quit") {
                return;
            }
        } else {
            if (!execute_one_instruction()) {
                render();
                move_cursor(25, 1);
                std::cout << "Program finished execution.\n";
                return;
            }
        }
    }
}
