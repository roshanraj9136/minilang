#include "vm.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

VM::VM(const CompiledProgram& program) : program_(program) {
    // Check if we actually have a main function to start running from.
    if (program_.main_index < 0) {
        throw std::runtime_error("Main function not found in compiled program");
    }
    // Push our entry frame to get the ball rolling
    call_frames_.push_back(CallFrame{program_.main_index, 0, 0});
    
    // Initialize our global variables with default integers
    globals_.resize(program_.global_count, Value::make_int(0));
}

// Getters and setters for our VM state so the debugger can peek inside
const std::vector<Value>& VM::stack() const { return stack_; }
std::vector<Value>& VM::stack() { return stack_; }
const std::vector<CallFrame>& VM::frames() const { return call_frames_; }
std::vector<CallFrame>& VM::frames() { return call_frames_; }
const std::vector<Value>& VM::globals() const { return globals_; }
std::vector<Value>& VM::globals() { return globals_; }
const CompiledProgram& VM::program() const { return program_; }
const std::string& VM::output() const { return output_; }
void VM::set_output(const std::string& val) { output_ = val; }

uint8_t VM::read_byte() {
    // Grab the next byte of bytecode and increment instruction pointer
    return current_chunk().code[current_frame().ip++];
}

uint16_t VM::read_short() {
    // Read two bytes and combine them into a 16-bit short
    uint8_t high = read_byte();
    uint8_t low = read_byte();
    return (high << 8) | low;
}

Value VM::pop() {
    Value v = stack_.back();
    stack_.pop_back();
    return v;
}

const Value& VM::peek() const {
    return stack_.back();
}

void VM::push(const Value& value) {
    stack_.push_back(value);
}

Chunk& VM::current_chunk() {
    return program_.functions[current_frame().fn_index].chunk;
}

CallFrame& VM::current_frame() {
    return call_frames_.back();
}

void VM::runtime_error(const std::string& message) {
    // Use the error callback if registered, otherwise print to stderr
    if (on_error) {
        on_error(message);
    } else {
        std::cerr << "Runtime error: " << message << " in function '"
                  << program_.functions[current_frame().fn_index].name << "'\n";
    }
}

VMResult VM::run() {
    // Standard run loop: just keep stepping until we either finish or hit an error
    while (true) {
        VMStepResult res = step();
        if (res == VMStepResult::HALTED) {
            return VMResult::OK;
        }
        if (res == VMStepResult::RUNTIME_ERROR) {
            return VMResult::RUNTIME_ERROR;
        }
    }
}

VMStepResult VM::step() {
    // Safety check: make sure IP isn't running past the end of the bytecode chunk
    if (current_frame().ip >= static_cast<int>(current_chunk().code.size())) {
        return VMStepResult::RUNTIME_ERROR;
    }

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
                // String concatenation!
                push(Value::make_string(a.as_string() + b.as_string()));
            } else {
                runtime_error("invalid operands for ADD");
                return VMStepResult::RUNTIME_ERROR;
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
                runtime_error("invalid operands for SUB");
                return VMStepResult::RUNTIME_ERROR;
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
                runtime_error("invalid operands for MUL");
                return VMStepResult::RUNTIME_ERROR;
            }
            break;
        }
        case OpCode::DIV: {
            Value b = pop();
            Value a = pop();
            if (a.is_int() && b.is_int()) {
                if (b.as_int() == 0) {
                    runtime_error("division by zero");
                    return VMStepResult::RUNTIME_ERROR;
                }
                push(Value::make_int(a.as_int() / b.as_int()));
            } else if (a.is_float() && b.is_float()) {
                if (b.as_float() == 0.0) {
                    runtime_error("division by zero");
                    return VMStepResult::RUNTIME_ERROR;
                }
                push(Value::make_float(a.as_float() / b.as_float()));
            } else {
                runtime_error("invalid operands for DIV");
                return VMStepResult::RUNTIME_ERROR;
            }
            break;
        }
        case OpCode::MOD: {
            Value b = pop();
            Value a = pop();
            if (!a.is_int() || !b.is_int()) {
                runtime_error("invalid operands for MOD (must be integers)");
                return VMStepResult::RUNTIME_ERROR;
            }
            if (b.as_int() == 0) {
                runtime_error("division by zero");
                return VMStepResult::RUNTIME_ERROR;
            }
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
                runtime_error("invalid operand for NEGATE");
                return VMStepResult::RUNTIME_ERROR;
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
                runtime_error("invalid operands for LT");
                return VMStepResult::RUNTIME_ERROR;
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
                runtime_error("invalid operands for GT");
                return VMStepResult::RUNTIME_ERROR;
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
                runtime_error("invalid operands for LTE");
                return VMStepResult::RUNTIME_ERROR;
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
                runtime_error("invalid operands for GTE");
                return VMStepResult::RUNTIME_ERROR;
            }
            break;
        }
        case OpCode::NOT: {
            Value a = pop();
            if (!a.is_bool()) {
                runtime_error("invalid operand for NOT (must be boolean)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_bool(!a.as_bool()));
            break;
        }
        case OpCode::AND: {
            Value b = pop();
            Value a = pop();
            if (!a.is_bool() || !b.is_bool()) {
                runtime_error("invalid operands for AND (must be booleans)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_bool(a.as_bool() && b.as_bool()));
            break;
        }
        case OpCode::OR: {
            Value b = pop();
            Value a = pop();
            if (!a.is_bool() || !b.is_bool()) {
                runtime_error("invalid operands for OR (must be booleans)");
                return VMStepResult::RUNTIME_ERROR;
            }
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
            stack_[current_frame().base_pointer + slot] = peek();
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
            if (!ptr.is_pointer()) {
                runtime_error("Expected pointer for dereference load");
                return VMStepResult::RUNTIME_ERROR;
            }
            uint32_t addr = ptr.as_pointer();
            if (addr >= stack_.size()) {
                runtime_error("Dangling or out-of-bounds pointer read");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(stack_[addr]);
            break;
        }
        case OpCode::STORE_DEREF: {
            Value ptr = pop();
            Value val = peek(); // store returns stored value
            if (!ptr.is_pointer()) {
                runtime_error("Expected pointer for dereference store");
                return VMStepResult::RUNTIME_ERROR;
            }
            uint32_t addr = ptr.as_pointer();
            if (addr >= stack_.size()) {
                runtime_error("Dangling or out-of-bounds pointer write");
                return VMStepResult::RUNTIME_ERROR;
            }
            stack_[addr] = val;
            break;
        }
        case OpCode::NEW_ARRAY: {
            uint8_t type_code = read_byte();
            Value size_val = pop();
            if (!size_val.is_int()) {
                runtime_error("Array size must be an integer");
                return VMStepResult::RUNTIME_ERROR;
            }
            int64_t size = size_val.as_int();
            if (size < 0) {
                runtime_error("Array size cannot be negative");
                return VMStepResult::RUNTIME_ERROR;
            }
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
            if (!idx_val.is_int()) {
                runtime_error("Array index must be an integer");
                return VMStepResult::RUNTIME_ERROR;
            }
            if (arr_val.is_string()) {
                int64_t index = idx_val.as_int();
                const std::string& s = arr_val.as_string();
                if (index < 0 || index >= static_cast<int64_t>(s.size())) {
                    runtime_error("String index out of bounds: index " + std::to_string(index) + ", size " + std::to_string(s.size()));
                    return VMStepResult::RUNTIME_ERROR;
                }
                push(Value::make_int(static_cast<int64_t>(static_cast<unsigned char>(s[index]))));
                break;
            }
            if (!arr_val.is_array()) {
                runtime_error("Expected array for element load");
                return VMStepResult::RUNTIME_ERROR;
            }
            int64_t index = idx_val.as_int();
            auto arr = arr_val.as_array();
            if (index < 0 || index >= static_cast<int64_t>(arr->size())) {
                runtime_error("Array index out of bounds: index " + std::to_string(index) + ", size " + std::to_string(arr->size()));
                return VMStepResult::RUNTIME_ERROR;
            }
            push((*arr)[index]);
            break;
        }
        case OpCode::ARRAY_STORE: {
            Value val = pop();
            Value idx_val = pop();
            Value arr_val = pop();
            if (!idx_val.is_int()) {
                runtime_error("Array index must be an integer");
                return VMStepResult::RUNTIME_ERROR;
            }
            if (!arr_val.is_array()) {
                runtime_error("Expected array for element store");
                return VMStepResult::RUNTIME_ERROR;
            }
            int64_t index = idx_val.as_int();
            auto arr = arr_val.as_array();
            if (index < 0 || index >= static_cast<int64_t>(arr->size())) {
                runtime_error("Array index out of bounds: index " + std::to_string(index) + ", size " + std::to_string(arr->size()));
                return VMStepResult::RUNTIME_ERROR;
            }
            (*arr)[index] = val;
            push(val); // Assignment returns assigned value
            break;
        }
        case OpCode::ARRAY_LENGTH: {
            Value arr_val = pop();
            if (arr_val.is_string()) {
                push(Value::make_int(static_cast<int64_t>(arr_val.as_string().size())));
                break;
            }
            if (!arr_val.is_array()) {
                runtime_error("Expected array for length query");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_int(static_cast<int64_t>(arr_val.as_array()->size())));
            break;
        }
        case OpCode::INT_TO_FLOAT: {
            Value val = pop();
            if (!val.is_int()) {
                runtime_error("invalid operand for INT_TO_FLOAT (must be integer)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_float(static_cast<double>(val.as_int())));
            break;
        }
        case OpCode::FLOAT_TO_INT: {
            Value val = pop();
            if (!val.is_float()) {
                runtime_error("invalid operand for FLOAT_TO_INT (must be float)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_int(static_cast<int64_t>(val.as_float())));
            break;
        }
        case OpCode::BIT_AND: {
            Value b = pop();
            Value a = pop();
            if (!a.is_int() || !b.is_int()) {
                runtime_error("invalid operands for BIT_AND (must be integers)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_int(a.as_int() & b.as_int()));
            break;
        }
        case OpCode::BIT_OR: {
            Value b = pop();
            Value a = pop();
            if (!a.is_int() || !b.is_int()) {
                runtime_error("invalid operands for BIT_OR (must be integers)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_int(a.as_int() | b.as_int()));
            break;
        }
        case OpCode::BIT_XOR: {
            Value b = pop();
            Value a = pop();
            if (!a.is_int() || !b.is_int()) {
                runtime_error("invalid operands for BIT_XOR (must be integers)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_int(a.as_int() ^ b.as_int()));
            break;
        }
        case OpCode::BIT_NOT: {
            Value val = pop();
            if (!val.is_int()) {
                runtime_error("invalid operand for BIT_NOT (must be integer)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_int(~val.as_int()));
            break;
        }
        case OpCode::BIT_SHL: {
            Value b = pop();
            Value a = pop();
            if (!a.is_int() || !b.is_int()) {
                runtime_error("invalid operands for BIT_SHL (must be integers)");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_int(a.as_int() << b.as_int()));
            break;
        }
        case OpCode::BIT_SHR: {
            Value b = pop();
            Value a = pop();
            if (!a.is_int() || !b.is_int()) {
                runtime_error("invalid operands for BIT_SHR (must be integers)");
                return VMStepResult::RUNTIME_ERROR;
            }
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
            globals_[index] = peek();
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
            std::string text = val.to_string();
            output_ += text;
            if (on_write) {
                on_write(text);
            } else {
                std::cout << text << std::flush;
            }
            break;
        }
        case OpCode::READ: {
            uint8_t type_code = read_byte();
            if (on_read) {
                push(on_read(type_code));
            } else {
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
            }
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
            current_frame().ip = target;
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            uint16_t target = read_short();
            Value condition = pop();
            if (!condition.is_bool()) {
                runtime_error("Condition for jump must be a boolean");
                return VMStepResult::RUNTIME_ERROR;
            }
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
            if (call_frames_.empty()) return VMStepResult::HALTED;
            push(result);
            break;
        }
        case OpCode::FILL_ARRAY: {
            Value fill_val = pop();
            Value arr_val = pop();
            if (!arr_val.is_array()) {
                runtime_error("Expected array for FILL_ARRAY");
                return VMStepResult::RUNTIME_ERROR;
            }
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
            if (!arr_val.is_array()) {
                runtime_error("Expected array for push_back");
                return VMStepResult::RUNTIME_ERROR;
            }
            arr_val.as_array()->push_back(val);
            push(Value::make_int(0));
            break;
        }
        case OpCode::VECTOR_POP_BACK: {
            Value arr_val = pop();
            if (!arr_val.is_array()) {
                runtime_error("Expected array for pop_back");
                return VMStepResult::RUNTIME_ERROR;
            }
            auto arr = arr_val.as_array();
            if (!arr->empty()) arr->pop_back();
            push(Value::make_int(0));
            break;
        }
        case OpCode::CONTAINER_EMPTY: {
            Value arr_val = pop();
            if (!arr_val.is_array()) {
                runtime_error("Expected array/container for empty check");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(Value::make_bool(arr_val.as_array()->empty()));
            break;
        }
        case OpCode::CONTAINER_CLEAR: {
            Value arr_val = pop();
            if (!arr_val.is_array()) {
                runtime_error("Expected array/container for clear");
                return VMStepResult::RUNTIME_ERROR;
            }
            arr_val.as_array()->clear();
            push(Value::make_int(0));
            break;
        }
        case OpCode::QUEUE_POP: {
            Value arr_val = pop();
            if (!arr_val.is_array()) {
                runtime_error("Expected queue for pop");
                return VMStepResult::RUNTIME_ERROR;
            }
            auto arr = arr_val.as_array();
            if (!arr->empty()) arr->erase(arr->begin());
            push(Value::make_int(0));
            break;
        }
        case OpCode::QUEUE_FRONT: {
            Value arr_val = pop();
            if (!arr_val.is_array()) {
                runtime_error("Expected queue for front");
                return VMStepResult::RUNTIME_ERROR;
            }
            auto arr = arr_val.as_array();
            if (arr->empty()) {
                runtime_error("queue front called on empty queue");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(arr->front());
            break;
        }
        case OpCode::STACK_TOP: {
            Value arr_val = pop();
            if (!arr_val.is_array()) {
                runtime_error("Expected stack for top");
                return VMStepResult::RUNTIME_ERROR;
            }
            auto arr = arr_val.as_array();
            if (arr->empty()) {
                runtime_error("stack top called on empty stack");
                return VMStepResult::RUNTIME_ERROR;
            }
            push(arr->back());
            break;
        }
        case OpCode::PRINT: {
            Value val = pop();
            std::string text = val.to_string();
            output_ += text + "\n";
            if (on_print) {
                on_print(text + "\n");
            } else {
                std::cout << text << std::endl;
            }
            break;
        }
        case OpCode::HALT:
            return VMStepResult::HALTED;
    }

    return VMStepResult::OK;
}
