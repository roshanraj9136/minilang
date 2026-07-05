#include "opcode.h"
#include <sstream>
#include <memory>

std::string opcode_to_string(OpCode op) {
    switch (op) {
        case OpCode::PUSH_INT: return "PUSH_INT";
        case OpCode::PUSH_FLOAT: return "PUSH_FLOAT";
        case OpCode::PUSH_TRUE: return "PUSH_TRUE";
        case OpCode::PUSH_FALSE: return "PUSH_FALSE";
        case OpCode::PUSH_STRING: return "PUSH_STRING";
        case OpCode::POP: return "POP";
        case OpCode::ADD: return "ADD";
        case OpCode::SUB: return "SUB";
        case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV";
        case OpCode::MOD: return "MOD";
        case OpCode::NEGATE: return "NEGATE";
        case OpCode::EQ: return "EQ";
        case OpCode::NEQ: return "NEQ";
        case OpCode::LT: return "LT";
        case OpCode::GT: return "GT";
        case OpCode::LTE: return "LTE";
        case OpCode::GTE: return "GTE";
        case OpCode::NOT: return "NOT";
        case OpCode::AND: return "AND";
        case OpCode::OR: return "OR";
        case OpCode::LOAD_LOCAL: return "LOAD_LOCAL";
        case OpCode::STORE_LOCAL: return "STORE_LOCAL";
        case OpCode::LOAD_LOCAL_ADDR: return "LOAD_LOCAL_ADDR";
        case OpCode::LOAD_DEREF: return "LOAD_DEREF";
        case OpCode::STORE_DEREF: return "STORE_DEREF";
        case OpCode::NEW_ARRAY: return "NEW_ARRAY";
        case OpCode::ARRAY_LOAD: return "ARRAY_LOAD";
        case OpCode::ARRAY_STORE: return "ARRAY_STORE";
        case OpCode::ARRAY_LENGTH: return "ARRAY_LENGTH";
        case OpCode::INT_TO_FLOAT: return "INT_TO_FLOAT";
        case OpCode::FLOAT_TO_INT: return "FLOAT_TO_INT";
        case OpCode::JUMP: return "JUMP";
        case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::CALL: return "CALL";
        case OpCode::RETURN: return "RETURN";
        case OpCode::PRINT: return "PRINT";
        case OpCode::HALT: return "HALT";
        case OpCode::BIT_AND: return "BIT_AND";
        case OpCode::BIT_OR: return "BIT_OR";
        case OpCode::BIT_XOR: return "BIT_XOR";
        case OpCode::BIT_NOT: return "BIT_NOT";
        case OpCode::BIT_SHL: return "BIT_SHL";
        case OpCode::BIT_SHR: return "BIT_SHR";
        case OpCode::PUSH_NULL: return "PUSH_NULL";
        case OpCode::DUP: return "DUP";
        case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
        case OpCode::CALL_BUILTIN: return "CALL_BUILTIN";
        case OpCode::WRITE: return "WRITE";
        case OpCode::READ: return "READ";
        case OpCode::FILL_ARRAY: return "FILL_ARRAY";
        case OpCode::NEW_PAIR: return "NEW_PAIR";
        case OpCode::VECTOR_PUSH_BACK: return "VECTOR_PUSH_BACK";
        case OpCode::VECTOR_POP_BACK: return "VECTOR_POP_BACK";
        case OpCode::CONTAINER_EMPTY: return "CONTAINER_EMPTY";
        case OpCode::CONTAINER_CLEAR: return "CONTAINER_CLEAR";
        case OpCode::QUEUE_POP: return "QUEUE_POP";
        case OpCode::QUEUE_FRONT: return "QUEUE_FRONT";
        case OpCode::STACK_TOP: return "STACK_TOP";
        default: return "UNKNOWN";
    }
}

Value Value::make_int(int64_t v) {
    return Value{v};
}

Value Value::make_float(double v) {
    return Value{v};
}

Value Value::make_bool(bool v) {
    return Value{v};
}

Value Value::make_string(const std::string& v) {
    return Value{v};
}

Value Value::make_pointer(uint32_t idx) {
    return Value{PointerValue{idx}};
}

Value Value::make_array(const ArrayValue& v) {
    return Value{v};
}

bool Value::is_int() const {
    return std::holds_alternative<int64_t>(this->data);
}

bool Value::is_float() const {
    return std::holds_alternative<double>(this->data);
}

bool Value::is_bool() const {
    return std::holds_alternative<bool>(this->data);
}

bool Value::is_string() const {
    return std::holds_alternative<std::string>(this->data);
}

bool Value::is_pointer() const {
    return std::holds_alternative<PointerValue>(this->data);
}

bool Value::is_array() const {
    return std::holds_alternative<ArrayValue>(this->data);
}

int64_t Value::as_int() const {
    return std::get<int64_t>(this->data);
}

double Value::as_float() const {
    return std::get<double>(this->data);
}

bool Value::as_bool() const {
    return std::get<bool>(this->data);
}

const std::string& Value::as_string() const {
    return std::get<std::string>(this->data);
}

uint32_t Value::as_pointer() const {
    return std::get<PointerValue>(this->data).index;
}

std::shared_ptr<std::vector<Value>> Value::as_array() const {
    return std::get<ArrayValue>(this->data);
}

std::string Value::to_string() const {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            std::string s = std::to_string(arg);
            if (s.find('.') != std::string::npos) {
                while (s.back() == '0') s.pop_back();
                if (s.back() == '.') s.pop_back();
            }
            return s;
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, PointerValue>) {
            return "&stack[" + std::to_string(arg.index) + "]";
        } else if constexpr (std::is_same_v<T, ArrayValue>) {
            std::string res = "[";
            for (size_t i = 0; i < arg->size(); ++i) {
                if (i > 0) res += ", ";
                res += (*arg)[i].to_string();
            }
            res += "]";
            return res;
        }
        return "";
    }, this->data);
}

void Chunk::emit_byte(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
}

void Chunk::emit_op(OpCode op, int line) {
    emit_byte(static_cast<uint8_t>(op), line);
}

void Chunk::emit_op_with_arg(OpCode op, uint16_t arg, int line) {
    emit_op(op, line);
    emit_byte((arg >> 8) & 0xFF, line);
    emit_byte(arg & 0xFF, line);
}

int Chunk::add_constant(const Value& value) {
    for (int i = 0; i < static_cast<int>(constants.size()); ++i) {
        if (constants[i].data == value.data) {
            return i;
        }
    }
    constants.push_back(value);
    return static_cast<int>(constants.size()) - 1;
}

int Chunk::emit_jump(OpCode op, int line) {
    emit_op(op, line);
    emit_byte(0xFF, line);
    emit_byte(0xFF, line);
    return static_cast<int>(code.size()) - 2;
}

void Chunk::patch_jump_to(int offset, int target) {
    code[offset] = (target >> 8) & 0xFF;
    code[offset + 1] = target & 0xFF;
}

void Chunk::patch_jump(int offset) {
    patch_jump_to(offset, static_cast<int>(code.size()));
}
