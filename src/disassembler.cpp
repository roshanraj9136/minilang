#include "disassembler.h"
#include <sstream>
#include <iomanip>

std::string Disassembler::disassemble(const CompiledProgram& program) {
    std::ostringstream ss;
    for (const auto& func : program.functions) {
        ss << disassemble_function(func) << "\n";
    }
    return ss.str();
}

std::string Disassembler::disassemble_function(const CompiledFunction& func) {
    std::ostringstream ss;
    ss << "=== Function: " << func.name << " (arity " << func.arity << ") ===\n";
    int offset = 0;
    while (offset < static_cast<int>(func.chunk.code.size())) {
        ss << disassemble_instruction(func.chunk, offset, func.debug_locals) << "\n";
    }
    return ss.str();
}

std::string Disassembler::disassemble_instruction(const Chunk& chunk, int& offset, const std::vector<DebugLocal>& debug_locals) {
    std::ostringstream ss;
    ss << std::setw(4) << std::setfill('0') << offset << "  ";
    uint8_t op = chunk.code[offset];
    OpCode opcode = static_cast<OpCode>(op);

    switch (opcode) {
        case OpCode::PUSH_INT: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t idx = (hi << 8) | lo;
            ss << "PUSH_INT      " << idx << " (" << chunk.constants[idx].to_string() << ")";
            offset += 3;
            break;
        }
        case OpCode::PUSH_FLOAT: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t idx = (hi << 8) | lo;
            ss << "PUSH_FLOAT    " << idx << " (" << chunk.constants[idx].to_string() << ")";
            offset += 3;
            break;
        }
        case OpCode::PUSH_STRING: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t idx = (hi << 8) | lo;
            ss << "PUSH_STRING   " << idx << " (\"" << chunk.constants[idx].to_string() << "\")";
            offset += 3;
            break;
        }
        case OpCode::LOAD_LOCAL: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t slot = (hi << 8) | lo;
            ss << "LOAD_LOCAL    " << slot;
            for (const auto& loc : debug_locals) {
                if (loc.slot == slot) {
                    ss << " (" << loc.name << ")";
                    break;
                }
            }
            offset += 3;
            break;
        }
        case OpCode::STORE_LOCAL: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t slot = (hi << 8) | lo;
            ss << "STORE_LOCAL   " << slot;
            for (const auto& loc : debug_locals) {
                if (loc.slot == slot) {
                    ss << " (" << loc.name << ")";
                    break;
                }
            }
            offset += 3;
            break;
        }
        case OpCode::LOAD_LOCAL_ADDR: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t slot = (hi << 8) | lo;
            ss << "LOAD_LOCAL_ADDR " << slot;
            for (const auto& loc : debug_locals) {
                if (loc.slot == slot) {
                    ss << " (" << loc.name << ")";
                    break;
                }
            }
            offset += 3;
            break;
        }
        case OpCode::NEW_ARRAY: {
            uint8_t type_code = chunk.code[offset + 1];
            ss << "NEW_ARRAY     type_code:" << static_cast<int>(type_code);
            offset += 2;
            break;
        }
        case OpCode::JUMP: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t target = (hi << 8) | lo;
            ss << "JUMP          " << std::setw(4) << std::setfill('0') << target;
            offset += 3;
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t target = (hi << 8) | lo;
            ss << "JUMP_IF_FALSE " << std::setw(4) << std::setfill('0') << target;
            offset += 3;
            break;
        }
        case OpCode::CALL: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t fn_idx = (hi << 8) | lo;
            uint8_t argc = chunk.code[offset + 3];
            ss << "CALL          fn_idx:" << fn_idx << " argc:" << static_cast<int>(argc);
            offset += 4;
            break;
        }
        case OpCode::LOAD_GLOBAL: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t idx = (hi << 8) | lo;
            ss << "LOAD_GLOBAL   " << idx;
            offset += 3;
            break;
        }
        case OpCode::STORE_GLOBAL: {
            uint8_t hi = chunk.code[offset + 1];
            uint8_t lo = chunk.code[offset + 2];
            uint16_t idx = (hi << 8) | lo;
            ss << "STORE_GLOBAL  " << idx;
            offset += 3;
            break;
        }
        case OpCode::CALL_BUILTIN: {
            uint8_t id = chunk.code[offset + 1];
            ss << "CALL_BUILTIN  " << static_cast<int>(id);
            offset += 2;
            break;
        }
        case OpCode::READ: {
            uint8_t type_code = chunk.code[offset + 1];
            ss << "READ          " << static_cast<int>(type_code);
            offset += 2;
            break;
        }
        case OpCode::WRITE:
            ss << "WRITE";
            offset += 1;
            break;
        case OpCode::ADD:
        case OpCode::SUB:
        case OpCode::MUL:
        case OpCode::DIV:
        case OpCode::MOD:
        case OpCode::NEGATE:
        case OpCode::EQ:
        case OpCode::NEQ:
        case OpCode::LT:
        case OpCode::GT:
        case OpCode::LTE:
        case OpCode::GTE:
        case OpCode::NOT:
        case OpCode::AND:
        case OpCode::OR:
        case OpCode::LOAD_DEREF:
        case OpCode::STORE_DEREF:
        case OpCode::ARRAY_LOAD:
        case OpCode::ARRAY_STORE:
        case OpCode::ARRAY_LENGTH:
        case OpCode::INT_TO_FLOAT:
        case OpCode::FLOAT_TO_INT:
        case OpCode::BIT_AND:
        case OpCode::BIT_OR:
        case OpCode::BIT_XOR:
        case OpCode::BIT_NOT:
        case OpCode::PUSH_TRUE:
        case OpCode::PUSH_FALSE:
        case OpCode::POP:
        case OpCode::BIT_SHL:
        case OpCode::BIT_SHR:
        case OpCode::PUSH_NULL:
        case OpCode::DUP:
        case OpCode::RETURN:
        case OpCode::PRINT:
        case OpCode::HALT:
        case OpCode::FILL_ARRAY:
        case OpCode::NEW_PAIR:
        case OpCode::VECTOR_PUSH_BACK:
        case OpCode::VECTOR_POP_BACK:
        case OpCode::CONTAINER_EMPTY:
        case OpCode::CONTAINER_CLEAR:
        case OpCode::QUEUE_POP:
        case OpCode::QUEUE_FRONT:
        case OpCode::STACK_TOP:
            ss << opcode_to_string(opcode);
            offset += 1;
            break;
        default:
            ss << "UNKNOWN_OPCODE";
            offset += 1;
            break;
    }
    return ss.str();
}
