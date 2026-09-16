#pragma once

#include <string>
#include "opcode.h"

class Disassembler {
public:
    static std::string disassemble(const CompiledProgram& program);
    static std::string disassemble_function(const CompiledFunction& func);
    // Formats the instruction at offset and advances offset past its operands.
    static std::string disassemble_instruction(const Chunk& chunk, int& offset, const std::vector<DebugLocal>& debug_locals);
};
