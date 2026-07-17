#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include "opcode.h"

// Keeps track of where we are in a function call
struct CallFrame {
    int fn_index;      // index into program.functions
    int ip;            // instruction pointer (bytecode offset)
    int base_pointer;  // stack frame base pointer
};

// Result of running the VM to completion
enum class VMResult { OK, RUNTIME_ERROR };

// Result of executing a single instruction
enum class VMStepResult {
    OK,             // Executed one instruction, keep going
    HALTED,         // Program hit HALT or returned from main
    RUNTIME_ERROR   // Something went wrong (div by zero, bad index, etc.)
};

class VM {
public:
    explicit VM(const CompiledProgram& program);
    
    // Runs the VM until it halts or hits an error
    VMResult run();
    
    // Executes exactly one instruction. Handy for debugging.
    VMStepResult step();

    // Callbacks to hook up I/O and errors (super useful for debuggers and web playgrounds)
    std::function<void(const std::string&)> on_write;
    std::function<void(const std::string&)> on_print;
    std::function<Value(uint8_t type_code)> on_read;
    std::function<void(const std::string&)> on_error;

    // Getters so we can inspect the VM state from the outside
    const std::vector<Value>& stack() const;
    std::vector<Value>& stack();
    
    const std::vector<CallFrame>& frames() const;
    std::vector<CallFrame>& frames();
    
    const std::vector<Value>& globals() const;
    std::vector<Value>& globals();
    
    const CompiledProgram& program() const;
    const std::string& output() const;
    void set_output(const std::string& val);

private:
    CompiledProgram program_;
    std::vector<Value> stack_;
    std::vector<Value> globals_;
    std::vector<CallFrame> call_frames_;
    std::string output_;

    // Internal helpers
    uint8_t read_byte();
    uint16_t read_short();
    Value pop();
    const Value& peek() const;
    void push(const Value& value);
    Chunk& current_chunk();
    CallFrame& current_frame();
    void runtime_error(const std::string& message);
};
