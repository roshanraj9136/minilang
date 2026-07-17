#include "debugger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

Debugger::Debugger(const std::string& source, const CompiledProgram& program)
    : source_(source), vm_(program), stepping_(true) {
    // Debugger suppresses stdout output so TUI doesn't get messed up!
    vm_.on_write = [](const std::string&) {};
    vm_.on_print = [](const std::string&) {};
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
    if (vm_.frames().empty()) return 0;
    int ip = vm_.frames().back().ip;
    const Chunk& chunk = vm_.program().functions[vm_.frames().back().fn_index].chunk;
    if (ip < 0 || ip >= static_cast<int>(chunk.lines.size())) return 0;
    return chunk.lines[ip];
}

std::string Debugger::current_fn_name() {
    if (vm_.frames().empty()) return "";
    return vm_.program().functions[vm_.frames().back().fn_index].name;
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
    if (vm_.frames().empty()) return;

    const Chunk& chk = vm_.program().functions[vm_.frames().back().fn_index].chunk;
    int current_ip = vm_.frames().back().ip;

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
            const auto& debug_locals = vm_.program().functions[vm_.frames().back().fn_index].debug_locals;
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
    int size = static_cast<int>(vm_.stack().size());
    int visible = std::min(size, height - 2);

    for (int i = 0; i < visible; ++i) {
        int idx = size - 1 - i;
        move_cursor(row + 1 + i, col + 2);
        std::string val_str = vm_.stack()[idx].to_string();
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
    if (vm_.frames().empty()) return;

    int base = vm_.frames().back().base_pointer;
    const auto& debug_locals = vm_.program().functions[vm_.frames().back().fn_index].debug_locals;

    int count = 0;
    for (const auto& loc : debug_locals) {
        int stack_idx = base + loc.slot;
        if (stack_idx >= 0 && stack_idx < static_cast<int>(vm_.stack().size())) {
            if (count >= height - 2) break;
            move_cursor(row + 1 + count, col + 2);
            std::string val_str = vm_.stack()[stack_idx].to_string();
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
    std::cout << bold << "IP: " << cyan << std::setw(4) << std::setfill('0') << (vm_.frames().empty() ? 0 : vm_.frames().back().ip) << reset;
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
    VMStepResult res = vm_.step();
    return res == VMStepResult::OK;
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
