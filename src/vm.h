#pragma once

#include <vector>
#include <string>
#include <iostream>
#include "opcode.h"

struct CallFrame {
    int fn_index;
    int ip;
    int base_pointer;
};

enum class VMResult { OK, RUNTIME_ERROR };

class VM {
public:
    explicit VM(const CompiledProgram& program);
    VMResult run();

    const std::vector<Value>& stack() const;
    const std::vector<CallFrame>& frames() const;
    const CompiledProgram& program() const;
    const std::string& output() const;

private:
    CompiledProgram program_;
    std::vector<Value> stack_;
    std::vector<Value> globals_;
    std::vector<CallFrame> call_frames_;
    std::string output_;

    uint8_t read_byte();
    uint16_t read_short();
    Value pop();
    const Value& peek() const;
    void push(const Value& value);
    Chunk& current_chunk();
    CallFrame& current_frame();
    void runtime_error(const std::string& message);
};
