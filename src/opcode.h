#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <memory>

enum class OpCode : uint8_t {
    PUSH_INT,
    PUSH_FLOAT,
    PUSH_TRUE,
    PUSH_FALSE,
    PUSH_STRING,
    POP,

    ADD, SUB, MUL, DIV, MOD, NEGATE,

    EQ, NEQ, LT, GT, LTE, GTE,

    NOT, AND, OR,

    LOAD_LOCAL,
    STORE_LOCAL,
    LOAD_LOCAL_ADDR,

    LOAD_DEREF,
    STORE_DEREF,

    NEW_ARRAY,
    ARRAY_LOAD,
    ARRAY_STORE,
    ARRAY_LENGTH,
    INT_TO_FLOAT,
    FLOAT_TO_INT,

    JUMP,
    JUMP_IF_FALSE,

    CALL,
    RETURN,

    PRINT,
    HALT,

    // Bitwise
    BIT_AND,
    BIT_OR,
    BIT_XOR,
    BIT_NOT,
    BIT_SHL,
    BIT_SHR,

    // Null
    PUSH_NULL,

    // Stack operations
    DUP,

    // Globals and Builtins
    LOAD_GLOBAL,
    STORE_GLOBAL,
    CALL_BUILTIN,
    WRITE,
    READ,
    FILL_ARRAY,
    NEW_PAIR,
    VECTOR_PUSH_BACK,
    VECTOR_POP_BACK,
    CONTAINER_EMPTY,
    CONTAINER_CLEAR,
    QUEUE_POP,
    QUEUE_FRONT,
    STACK_TOP
};

std::string opcode_to_string(OpCode op);

struct PointerValue {
    uint32_t index;
    bool operator==(const PointerValue& other) const {
        return index == other.index;
    }
};

// Forward declare Value to define ArrayValue as shared_ptr of vector of Value
struct Value;
using ArrayValue = std::shared_ptr<std::vector<Value>>;

struct Value {
    std::variant<int64_t, double, bool, std::string, PointerValue, ArrayValue> data;

    static Value make_int(int64_t v);
    static Value make_float(double v);
    static Value make_bool(bool v);
    static Value make_string(const std::string& v);
    static Value make_pointer(uint32_t idx);
    static Value make_array(const ArrayValue& v);

    bool is_int() const;
    bool is_float() const;
    bool is_bool() const;
    bool is_string() const;
    bool is_pointer() const;
    bool is_array() const;

    int64_t as_int() const;
    double as_float() const;
    bool as_bool() const;
    const std::string& as_string() const;
    uint32_t as_pointer() const;
    ArrayValue as_array() const;

    std::string to_string() const;
};

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<Value> constants;
    std::vector<int> lines;

    void emit_byte(uint8_t byte, int line);
    void emit_op(OpCode op, int line);
    void emit_op_with_arg(OpCode op, uint16_t arg, int line);
    int add_constant(const Value& value);
    int emit_jump(OpCode op, int line);
    void patch_jump(int offset);
    void patch_jump_to(int offset, int target);
};

struct DebugLocal {
    std::string name;
    int slot;
};

struct CompiledFunction {
    std::string name;
    int arity;
    Chunk chunk;
    std::vector<DebugLocal> debug_locals;
};

struct CompiledProgram {
    std::vector<CompiledFunction> functions;
    int main_index = -1;
    int global_count = 0;
};
