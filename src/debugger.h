#pragma once

#include <string>
#include <vector>
#include <set>
#include "opcode.h"
#include "vm.h"

class Debugger {
public:
    Debugger(const std::string& source, const CompiledProgram& program);
    void run();

private:
    std::string source_;
    CompiledProgram program_;
    std::vector<Value> stack_;
    std::vector<Value> globals_;
    std::vector<CallFrame> call_frames_;
    std::set<int> breakpoints_;
    bool stepping_;
    std::string output_log_;

    void render();
    void render_box(int row, int col, int height, int width, const std::string& title);
    void render_source_panel(int row, int col, int height, int width);
    void render_bytecode_panel(int row, int col, int height, int width);
    void render_stack_panel(int row, int col, int height, int width);
    void render_variables_panel(int row, int col, int height, int width);
    void render_controls(int row);
    void move_cursor(int row, int col);
    void clear_screen();

    bool execute_one_instruction();
    uint8_t read_byte();
    uint16_t read_short();
    Value pop();
    const Value& peek_stack() const;
    void push(const Value& value);
    Chunk& current_chunk();
    CallFrame& current_frame();
    int current_source_line();
    std::string current_fn_name();
};
