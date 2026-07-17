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
    VM vm_; // The actual VM we are stepping through!
    std::set<int> breakpoints_;
    bool stepping_;

    // Screen/TUI rendering logic
    void render();
    void render_box(int row, int col, int height, int width, const std::string& title);
    void render_source_panel(int row, int col, int height, int width);
    void render_bytecode_panel(int row, int col, int height, int width);
    void render_stack_panel(int row, int col, int height, int width);
    void render_variables_panel(int row, int col, int height, int width);
    void render_controls(int row);
    void move_cursor(int row, int col);
    void clear_screen();

    // Debugger control helpers
    bool execute_one_instruction();
    int current_source_line();
    std::string current_fn_name();
};
