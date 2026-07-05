#include "codegen.h"
#include <stdexcept>

CodeGenerator::CodeGenerator(ErrorReporter& errors, const std::string& source)
    : errors_(errors), source_(source) {}

CompiledProgram CodeGenerator::generate(const std::vector<StmtPtr>& statements) {
    global_indices_.clear();
    program_.global_count = 0;
    for (auto& stmt : statements) {
        if (auto var = std::get_if<VarDeclStmt>(&stmt->node)) {
            global_indices_[var->name] = program_.global_count++;
        } else if (auto con = std::get_if<ConstDeclStmt>(&stmt->node)) {
            global_indices_[con->name] = program_.global_count++;
        }
    }

    for (size_t i = 0; i < statements.size(); ++i) {
        if (auto fn = std::get_if<FnDeclStmt>(&statements[i]->node)) {
            fn_indices_[fn->name] = static_cast<int>(program_.functions.size());
            std::vector<DataType> p_types;
            for (const auto& p : fn->params) {
                p_types.push_back(p.type);
            }
            fn_param_types_[fn->name] = p_types;

            CompiledFunction placeholder_func;
            placeholder_func.name = fn->name;
            program_.functions.push_back(std::move(placeholder_func));
        }
    }

    int fn_counter = 0;
    for (auto& stmt : statements) {
        if (auto fn = std::get_if<FnDeclStmt>(&stmt->node)) {
            CompiledFunction func;
            func.name = fn->name;
            func.arity = static_cast<int>(fn->params.size());
            current_chunk_ = &func.chunk;
            current_fn_return_type_ = fn->return_type;

            locals_.clear();
            slot_count_ = 0;

            for (auto& param : fn->params) {
                locals_.push_back({param.name, slot_count_++});
            }

            if (fn->name == "main") {
                for (auto& st : statements) {
                    if (auto var = std::get_if<VarDeclStmt>(&st->node)) {
                        generate_expr(*var->initializer);
                        if (var->type == DataType::FLOAT && var->initializer->resolved_type == DataType::INT) {
                            current_chunk_->emit_op(OpCode::INT_TO_FLOAT, st->line);
                        }
                        int idx = global_indices_[var->name];
                        current_chunk_->emit_op_with_arg(OpCode::STORE_GLOBAL, idx, st->line);
                        current_chunk_->emit_op(OpCode::POP, st->line);
                    } else if (auto con = std::get_if<ConstDeclStmt>(&st->node)) {
                        generate_expr(*con->initializer);
                        if (con->type == DataType::FLOAT && con->initializer->resolved_type == DataType::INT) {
                            current_chunk_->emit_op(OpCode::INT_TO_FLOAT, st->line);
                        }
                        int idx = global_indices_[con->name];
                        current_chunk_->emit_op_with_arg(OpCode::STORE_GLOBAL, idx, st->line);
                        current_chunk_->emit_op(OpCode::POP, st->line);
                    }
                }
            }

            if (auto block = std::get_if<BlockStmt>(&fn->body->node)) {
                for (auto& block_stmt : block->statements) {
                    generate_stmt(*block_stmt);
                }
            }

            if (func.chunk.code.empty() || static_cast<OpCode>(func.chunk.code.back()) != OpCode::RETURN) {
                int zero_idx = current_chunk_->add_constant(Value::make_int(0));
                current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, zero_idx, stmt->line);
                current_chunk_->emit_op(OpCode::RETURN, stmt->line);
            }

            for (auto& loc : locals_) {
                func.debug_locals.push_back({loc.name, loc.slot});
            }

            program_.functions[fn_counter] = std::move(func);
            fn_counter++;
        }
    }

    for (size_t i = 0; i < program_.functions.size(); ++i) {
        if (program_.functions[i].name == "main") {
            program_.main_index = static_cast<int>(i);
            break;
        }
    }

    return program_;
}

void CodeGenerator::generate_stmt(const Stmt& stmt) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, FnDeclStmt>) {
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
            generate_block(arg);
        } else if constexpr (std::is_same_v<T, VarDeclStmt>) {
            generate_var_decl(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            generate_if(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            generate_while(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ForStmt>) {
            generate_for(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            generate_return(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
            generate_print(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
            generate_expr_stmt(arg);
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
            generate_break(stmt.line);
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
            generate_continue(stmt.line);
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            generate_switch(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, DoWhileStmt>) {
            generate_do_while(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ConstDeclStmt>) {
            generate_const_decl(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, CoutStmt>) {
            generate_cout(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, CinStmt>) {
            generate_cin(arg, stmt.line);
        }
    }, stmt.node);
}

void CodeGenerator::generate_block(const BlockStmt& block) {
    int saved_locals = locals_.size();
    int saved_slots = slot_count_;

    for (auto& stmt : block.statements) {
        generate_stmt(*stmt);
    }

    int pops = slot_count_ - saved_slots;
    for (int i = 0; i < pops; ++i) {
        current_chunk_->emit_op(OpCode::POP, 0);
    }

    locals_.resize(saved_locals);
    slot_count_ = saved_slots;
}

void CodeGenerator::generate_var_decl(const VarDeclStmt& decl, int line) {
    generate_expr(*decl.initializer);
    if (decl.type == DataType::FLOAT && decl.initializer->resolved_type == DataType::INT) {
        current_chunk_->emit_op(OpCode::INT_TO_FLOAT, line);
    }
    locals_.push_back({decl.name, slot_count_++});
}

void CodeGenerator::generate_if(const IfStmt& stmt, int line) {
    generate_expr(*stmt.condition);
    int else_jump = current_chunk_->emit_jump(OpCode::JUMP_IF_FALSE, line);
    generate_stmt(*stmt.then_branch);

    if (stmt.else_branch) {
        int end_jump = current_chunk_->emit_jump(OpCode::JUMP, line);
        current_chunk_->patch_jump(else_jump);
        generate_stmt(*stmt.else_branch);
        current_chunk_->patch_jump(end_jump);
    } else {
        current_chunk_->patch_jump(else_jump);
    }
}

void CodeGenerator::generate_while(const WhileStmt& stmt, int line) {
    int loop_start = static_cast<int>(current_chunk_->code.size());
    loop_stack_.push_back({{}, {}});

    generate_expr(*stmt.condition);
    int exit_jump = current_chunk_->emit_jump(OpCode::JUMP_IF_FALSE, line);

    generate_stmt(*stmt.body);

    current_chunk_->emit_op_with_arg(OpCode::JUMP, loop_start, line);
    current_chunk_->patch_jump(exit_jump);

    for (int j : loop_stack_.back().break_jumps) {
        current_chunk_->patch_jump(j);
    }
    for (int j : loop_stack_.back().continue_jumps) {
        current_chunk_->patch_jump_to(j, loop_start);
    }

    loop_stack_.pop_back();
}

void CodeGenerator::generate_for(const ForStmt& stmt, int line) {
    int saved_locals = locals_.size();
    int saved_slots = slot_count_;

    generate_stmt(*stmt.init);

    int loop_start = static_cast<int>(current_chunk_->code.size());
    loop_stack_.push_back({{}, {}});

    generate_expr(*stmt.condition);
    int exit_jump = current_chunk_->emit_jump(OpCode::JUMP_IF_FALSE, line);

    generate_stmt(*stmt.body);

    int update_start = static_cast<int>(current_chunk_->code.size());
    generate_expr(*stmt.update);
    current_chunk_->emit_op(OpCode::POP, line);

    current_chunk_->emit_op_with_arg(OpCode::JUMP, loop_start, line);
    current_chunk_->patch_jump(exit_jump);

    for (int j : loop_stack_.back().break_jumps) {
        current_chunk_->patch_jump(j);
    }
    for (int j : loop_stack_.back().continue_jumps) {
        current_chunk_->patch_jump_to(j, update_start);
    }

    loop_stack_.pop_back();

    int pops = slot_count_ - saved_slots;
    for (int i = 0; i < pops; ++i) {
        current_chunk_->emit_op(OpCode::POP, line);
    }

    locals_.resize(saved_locals);
    slot_count_ = saved_slots;
}

void CodeGenerator::generate_return(const ReturnStmt& stmt, int line) {
    if (stmt.value) {
        generate_expr(*stmt.value);
        if (current_fn_return_type_ == DataType::FLOAT && stmt.value->resolved_type == DataType::INT) {
            current_chunk_->emit_op(OpCode::INT_TO_FLOAT, line);
        }
    } else {
        int zero_idx = current_chunk_->add_constant(Value::make_int(0));
        current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, zero_idx, line);
    }
    current_chunk_->emit_op(OpCode::RETURN, line);
}

void CodeGenerator::generate_print(const PrintStmt& stmt, int line) {
    generate_expr(*stmt.value);
    current_chunk_->emit_op(OpCode::PRINT, line);
}

void CodeGenerator::generate_expr_stmt(const ExprStmt& stmt) {
    generate_expr(*stmt.expr);
    current_chunk_->emit_op(OpCode::POP, stmt.expr->line);
}

void CodeGenerator::generate_break(int line) {
    int j = current_chunk_->emit_jump(OpCode::JUMP, line);
    loop_stack_.back().break_jumps.push_back(j);
}

void CodeGenerator::generate_continue(int line) {
    int j = current_chunk_->emit_jump(OpCode::JUMP, line);
    loop_stack_.back().continue_jumps.push_back(j);
}

void CodeGenerator::generate_switch(const SwitchStmt& stmt, int line) {
    generate_expr(*stmt.value);

    std::vector<int> end_jumps;
    int next_case_jump = -1;

    for (size_t i = 0; i < stmt.cases.size(); ++i) {
        if (next_case_jump != -1) {
            current_chunk_->patch_jump(next_case_jump);
        }

        current_chunk_->emit_op(OpCode::DUP, line);
        generate_expr(*stmt.cases[i].first);
        current_chunk_->emit_op(OpCode::EQ, line);
        next_case_jump = current_chunk_->emit_jump(OpCode::JUMP_IF_FALSE, line);

        current_chunk_->emit_op(OpCode::POP, line);
        generate_stmt(*stmt.cases[i].second);
        int end_jmp = current_chunk_->emit_jump(OpCode::JUMP, line);
        end_jumps.push_back(end_jmp);
    }

    if (next_case_jump != -1) {
        current_chunk_->patch_jump(next_case_jump);
    }

    current_chunk_->emit_op(OpCode::POP, line);
    if (stmt.default_case) {
        generate_stmt(*stmt.default_case);
    }

    for (int j : end_jumps) {
        current_chunk_->patch_jump(j);
    }
}

void CodeGenerator::generate_do_while(const DoWhileStmt& stmt, int line) {
    int loop_start = static_cast<int>(current_chunk_->code.size());
    loop_stack_.push_back({{}, {}});

    generate_stmt(*stmt.body);

    int cond_start = static_cast<int>(current_chunk_->code.size());
    generate_expr(*stmt.condition);

    int exit_jump = current_chunk_->emit_jump(OpCode::JUMP_IF_FALSE, line);
    current_chunk_->emit_op_with_arg(OpCode::JUMP, loop_start, line);
    current_chunk_->patch_jump(exit_jump);

    for (int j : loop_stack_.back().break_jumps) {
        current_chunk_->patch_jump(j);
    }
    for (int j : loop_stack_.back().continue_jumps) {
        current_chunk_->patch_jump_to(j, cond_start);
    }

    loop_stack_.pop_back();
}

void CodeGenerator::generate_const_decl(const ConstDeclStmt& decl, int line) {
    generate_expr(*decl.initializer);
    if (decl.type == DataType::FLOAT && decl.initializer->resolved_type == DataType::INT) {
        current_chunk_->emit_op(OpCode::INT_TO_FLOAT, line);
    }
    locals_.push_back({decl.name, slot_count_++});
}

void CodeGenerator::generate_cout(const CoutStmt& stmt, int line) {
    for (auto& expr : stmt.exprs) {
        generate_expr(*expr);
        current_chunk_->emit_op(OpCode::WRITE, line);
    }
}

void CodeGenerator::generate_cin(const CinStmt& stmt, int line) {
    for (auto& target : stmt.targets) {
        DataType target_type = target->resolved_type;
        uint8_t type_code = 0;
        if (target_type == DataType::FLOAT) type_code = 1;
        else if (target_type == DataType::BOOL) type_code = 2;
        else if (target_type == DataType::STRING) type_code = 3;

        if (auto var_ref = std::get_if<VarRefExpr>(&target->node)) {
            current_chunk_->emit_op(OpCode::READ, line);
            current_chunk_->emit_byte(type_code, line);
            int slot = resolve_local(var_ref->name);
            if (slot != -1) {
                current_chunk_->emit_op_with_arg(OpCode::STORE_LOCAL, slot, line);
            } else {
                int idx = resolve_global(var_ref->name);
                current_chunk_->emit_op_with_arg(OpCode::STORE_GLOBAL, idx, line);
            }
            current_chunk_->emit_op(OpCode::POP, line);
        } else if (auto unary = std::get_if<UnaryExpr>(&target->node)) {
            if (unary->op == TokenType::STAR) {
                current_chunk_->emit_op(OpCode::READ, line);
                current_chunk_->emit_byte(type_code, line);
                generate_expr(*unary->operand);
                current_chunk_->emit_op(OpCode::STORE_DEREF, line);
                current_chunk_->emit_op(OpCode::POP, line);
            }
        } else if (auto array_idx = std::get_if<ArrayIndexExpr>(&target->node)) {
            generate_expr(*array_idx->array);
            generate_expr(*array_idx->index);
            current_chunk_->emit_op(OpCode::READ, line);
            current_chunk_->emit_byte(type_code, line);
            current_chunk_->emit_op(OpCode::ARRAY_STORE, line);
            current_chunk_->emit_op(OpCode::POP, line);
        }
    }
}

int CodeGenerator::resolve_global(const std::string& name) {
    auto found = global_indices_.find(name);
    if (found != global_indices_.end()) {
        return found->second;
    }
    return -1;
}

int CodeGenerator::resolve_local(const std::string& name) {
    for (int i = static_cast<int>(locals_.size()) - 1; i >= 0; --i) {
        if (locals_[i].name == name) {
            return locals_[i].slot;
        }
    }
    return -1;
}

static DataType get_element_type(DataType t) {
    if (t == DataType::VECTOR_INT || t == DataType::QUEUE_INT || t == DataType::STACK_INT) return DataType::INT;
    if (t == DataType::VECTOR_FLOAT) return DataType::FLOAT;
    if (t == DataType::VECTOR_BOOL) return DataType::BOOL;
    if (t == DataType::VECTOR_STRING) return DataType::STRING;
    if (t == DataType::VECTOR_PAIR_INT_INT || t == DataType::QUEUE_PAIR_INT_INT) return DataType::PAIR_INT_INT;
    if (t == DataType::VECTOR_VECTOR_INT) return DataType::VECTOR_INT;
    if (t == DataType::VECTOR_VECTOR_PAIR_INT_INT) return DataType::PAIR_INT_INT;
    return DataType::UNKNOWN;
}

void CodeGenerator::generate_expr(const Expr& expr) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, LiteralIntExpr>) {
            int idx = current_chunk_->add_constant(Value::make_int(arg.value));
            current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, idx, expr.line);
        } else if constexpr (std::is_same_v<T, LiteralFloatExpr>) {
            int idx = current_chunk_->add_constant(Value::make_float(arg.value));
            current_chunk_->emit_op_with_arg(OpCode::PUSH_FLOAT, idx, expr.line);
        } else if constexpr (std::is_same_v<T, LiteralBoolExpr>) {
            current_chunk_->emit_op(arg.value ? OpCode::PUSH_TRUE : OpCode::PUSH_FALSE, expr.line);
        } else if constexpr (std::is_same_v<T, LiteralStringExpr>) {
            int idx = current_chunk_->add_constant(Value::make_string(arg.value));
            current_chunk_->emit_op_with_arg(OpCode::PUSH_STRING, idx, expr.line);
        } else if constexpr (std::is_same_v<T, VarRefExpr>) {
            int slot = resolve_local(arg.name);
            if (slot != -1) {
                current_chunk_->emit_op_with_arg(OpCode::LOAD_LOCAL, slot, expr.line);
            } else {
                int idx = resolve_global(arg.name);
                current_chunk_->emit_op_with_arg(OpCode::LOAD_GLOBAL, idx, expr.line);
            }
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
            if (auto var_ref = std::get_if<VarRefExpr>(&arg.lhs->node)) {
                generate_expr(*arg.value);
                if (arg.lhs->resolved_type == DataType::FLOAT && arg.value->resolved_type == DataType::INT) {
                    current_chunk_->emit_op(OpCode::INT_TO_FLOAT, expr.line);
                }
                int slot = resolve_local(var_ref->name);
                if (slot != -1) {
                    current_chunk_->emit_op_with_arg(OpCode::STORE_LOCAL, slot, expr.line);
                } else {
                    int idx = resolve_global(var_ref->name);
                    current_chunk_->emit_op_with_arg(OpCode::STORE_GLOBAL, idx, expr.line);
                }
            } else if (auto unary = std::get_if<UnaryExpr>(&arg.lhs->node)) {
                if (unary->op == TokenType::STAR) {
                    generate_expr(*arg.value);
                    if (arg.lhs->resolved_type == DataType::FLOAT && arg.value->resolved_type == DataType::INT) {
                        current_chunk_->emit_op(OpCode::INT_TO_FLOAT, expr.line);
                    }
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::STORE_DEREF, expr.line);
                }
            } else if (auto array_idx = std::get_if<ArrayIndexExpr>(&arg.lhs->node)) {
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                generate_expr(*arg.value);
                if (arg.lhs->resolved_type == DataType::FLOAT && arg.value->resolved_type == DataType::INT) {
                    current_chunk_->emit_op(OpCode::INT_TO_FLOAT, expr.line);
                }
                current_chunk_->emit_op(OpCode::ARRAY_STORE, expr.line);
            }
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
            generate_binary(arg, expr.line);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
            generate_unary(arg, expr.line);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
            generate_call(arg, expr.line);
        } else if constexpr (std::is_same_v<T, ArrayIndexExpr>) {
            generate_expr(*arg.array);
            generate_expr(*arg.index);
            current_chunk_->emit_op(OpCode::ARRAY_LOAD, expr.line);
        } else if constexpr (std::is_same_v<T, NewArrayExpr>) {
            generate_expr(*arg.size);
            uint8_t type_code = 0;
            if (arg.element_type == DataType::FLOAT) type_code = 1;
            else if (arg.element_type == DataType::BOOL) type_code = 2;
            else if (arg.element_type == DataType::STRING) type_code = 3;
            current_chunk_->emit_op(OpCode::NEW_ARRAY, expr.line);
            current_chunk_->emit_byte(type_code, expr.line);
        } else if constexpr (std::is_same_v<T, LenExpr>) {
            generate_expr(*arg.array);
            current_chunk_->emit_op(OpCode::ARRAY_LENGTH, expr.line);
        } else if constexpr (std::is_same_v<T, CastExpr>) {
            generate_expr(*arg.operand);
            if (arg.target_type == DataType::FLOAT && arg.operand->resolved_type == DataType::INT) {
                current_chunk_->emit_op(OpCode::INT_TO_FLOAT, expr.line);
            } else if (arg.target_type == DataType::INT && arg.operand->resolved_type == DataType::FLOAT) {
                current_chunk_->emit_op(OpCode::FLOAT_TO_INT, expr.line);
            }
        } else if constexpr (std::is_same_v<T, TernaryExpr>) {
            generate_expr(*arg.condition);
            int else_jump = current_chunk_->emit_jump(OpCode::JUMP_IF_FALSE, expr.line);
            generate_expr(*arg.then_expr);
            int end_jump = current_chunk_->emit_jump(OpCode::JUMP, expr.line);
            current_chunk_->patch_jump(else_jump);
            generate_expr(*arg.else_expr);
            current_chunk_->patch_jump(end_jump);
        } else if constexpr (std::is_same_v<T, CompoundAssignExpr>) {
            if (auto var_ref = std::get_if<VarRefExpr>(&arg.target->node)) {
                int slot = resolve_local(var_ref->name);
                current_chunk_->emit_op_with_arg(OpCode::LOAD_LOCAL, slot, expr.line);
                generate_expr(*arg.value);
                if (arg.target->resolved_type == DataType::FLOAT && arg.value->resolved_type == DataType::INT) {
                    current_chunk_->emit_op(OpCode::INT_TO_FLOAT, expr.line);
                }
                OpCode bin_op;
                switch (arg.op) {
                    case TokenType::PLUS_EQUAL: bin_op = OpCode::ADD; break;
                    case TokenType::MINUS_EQUAL: bin_op = OpCode::SUB; break;
                    case TokenType::STAR_EQUAL: bin_op = OpCode::MUL; break;
                    case TokenType::SLASH_EQUAL: bin_op = OpCode::DIV; break;
                    case TokenType::PERCENT_EQUAL: bin_op = OpCode::MOD; break;
                    default: bin_op = OpCode::ADD; break;
                }
                current_chunk_->emit_op(bin_op, expr.line);
                current_chunk_->emit_op_with_arg(OpCode::STORE_LOCAL, slot, expr.line);
            } else if (auto unary = std::get_if<UnaryExpr>(&arg.target->node)) {
                if (unary->op == TokenType::STAR) {
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::LOAD_DEREF, expr.line);
                    generate_expr(*arg.value);
                    if (arg.target->resolved_type == DataType::FLOAT && arg.value->resolved_type == DataType::INT) {
                        current_chunk_->emit_op(OpCode::INT_TO_FLOAT, expr.line);
                    }
                    OpCode bin_op;
                    switch (arg.op) {
                        case TokenType::PLUS_EQUAL: bin_op = OpCode::ADD; break;
                        case TokenType::MINUS_EQUAL: bin_op = OpCode::SUB; break;
                        case TokenType::STAR_EQUAL: bin_op = OpCode::MUL; break;
                        case TokenType::SLASH_EQUAL: bin_op = OpCode::DIV; break;
                        case TokenType::PERCENT_EQUAL: bin_op = OpCode::MOD; break;
                        default: bin_op = OpCode::ADD; break;
                    }
                    current_chunk_->emit_op(bin_op, expr.line);
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::STORE_DEREF, expr.line);
                }
            } else if (auto array_idx = std::get_if<ArrayIndexExpr>(&arg.target->node)) {
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                current_chunk_->emit_op(OpCode::ARRAY_LOAD, expr.line);
                generate_expr(*arg.value);
                if (arg.target->resolved_type == DataType::FLOAT && arg.value->resolved_type == DataType::INT) {
                    current_chunk_->emit_op(OpCode::INT_TO_FLOAT, expr.line);
                }
                OpCode bin_op;
                switch (arg.op) {
                    case TokenType::PLUS_EQUAL: bin_op = OpCode::ADD; break;
                    case TokenType::MINUS_EQUAL: bin_op = OpCode::SUB; break;
                    case TokenType::STAR_EQUAL: bin_op = OpCode::MUL; break;
                    case TokenType::SLASH_EQUAL: bin_op = OpCode::DIV; break;
                    case TokenType::PERCENT_EQUAL: bin_op = OpCode::MOD; break;
                    default: bin_op = OpCode::ADD; break;
                }
                current_chunk_->emit_op(bin_op, expr.line);
                current_chunk_->emit_op(OpCode::ARRAY_STORE, expr.line);
            }
        } else if constexpr (std::is_same_v<T, PreUnaryExpr>) {
            if (auto var_ref = std::get_if<VarRefExpr>(&arg.operand->node)) {
                int slot = resolve_local(var_ref->name);
                current_chunk_->emit_op_with_arg(OpCode::LOAD_LOCAL, slot, expr.line);
                int one_idx = current_chunk_->add_constant(Value::make_int(1));
                if (arg.operand->resolved_type == DataType::FLOAT) {
                    int one_f_idx = current_chunk_->add_constant(Value::make_float(1.0));
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_FLOAT, one_f_idx, expr.line);
                } else {
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, one_idx, expr.line);
                }
                current_chunk_->emit_op(arg.op == TokenType::PLUS_PLUS ? OpCode::ADD : OpCode::SUB, expr.line);
                current_chunk_->emit_op_with_arg(OpCode::STORE_LOCAL, slot, expr.line);
            } else if (auto unary = std::get_if<UnaryExpr>(&arg.operand->node)) {
                if (unary->op == TokenType::STAR) {
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::LOAD_DEREF, expr.line);
                    int one_idx = current_chunk_->add_constant(Value::make_int(1));
                    if (arg.operand->resolved_type == DataType::FLOAT) {
                        int one_f_idx = current_chunk_->add_constant(Value::make_float(1.0));
                        current_chunk_->emit_op_with_arg(OpCode::PUSH_FLOAT, one_f_idx, expr.line);
                    } else {
                        current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, one_idx, expr.line);
                    }
                    current_chunk_->emit_op(arg.op == TokenType::PLUS_PLUS ? OpCode::ADD : OpCode::SUB, expr.line);
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::STORE_DEREF, expr.line);
                }
            } else if (auto array_idx = std::get_if<ArrayIndexExpr>(&arg.operand->node)) {
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                current_chunk_->emit_op(OpCode::ARRAY_LOAD, expr.line);
                int one_idx = current_chunk_->add_constant(Value::make_int(1));
                if (arg.operand->resolved_type == DataType::FLOAT) {
                    int one_f_idx = current_chunk_->add_constant(Value::make_float(1.0));
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_FLOAT, one_f_idx, expr.line);
                } else {
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, one_idx, expr.line);
                }
                current_chunk_->emit_op(arg.op == TokenType::PLUS_PLUS ? OpCode::ADD : OpCode::SUB, expr.line);
                current_chunk_->emit_op(OpCode::ARRAY_STORE, expr.line);
            }
        } else if constexpr (std::is_same_v<T, PostUnaryExpr>) {
            if (auto var_ref = std::get_if<VarRefExpr>(&arg.operand->node)) {
                int slot = resolve_local(var_ref->name);
                current_chunk_->emit_op_with_arg(OpCode::LOAD_LOCAL, slot, expr.line);
                current_chunk_->emit_op_with_arg(OpCode::LOAD_LOCAL, slot, expr.line);
                int one_idx = current_chunk_->add_constant(Value::make_int(1));
                if (arg.operand->resolved_type == DataType::FLOAT) {
                    int one_f_idx = current_chunk_->add_constant(Value::make_float(1.0));
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_FLOAT, one_f_idx, expr.line);
                } else {
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, one_idx, expr.line);
                }
                current_chunk_->emit_op(arg.op == TokenType::PLUS_PLUS ? OpCode::ADD : OpCode::SUB, expr.line);
                current_chunk_->emit_op_with_arg(OpCode::STORE_LOCAL, slot, expr.line);
                current_chunk_->emit_op(OpCode::POP, expr.line);
            } else if (auto unary = std::get_if<UnaryExpr>(&arg.operand->node)) {
                if (unary->op == TokenType::STAR) {
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::LOAD_DEREF, expr.line);
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::LOAD_DEREF, expr.line);
                    int one_idx = current_chunk_->add_constant(Value::make_int(1));
                    if (arg.operand->resolved_type == DataType::FLOAT) {
                        int one_f_idx = current_chunk_->add_constant(Value::make_float(1.0));
                        current_chunk_->emit_op_with_arg(OpCode::PUSH_FLOAT, one_f_idx, expr.line);
                    } else {
                        current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, one_idx, expr.line);
                    }
                    current_chunk_->emit_op(arg.op == TokenType::PLUS_PLUS ? OpCode::ADD : OpCode::SUB, expr.line);
                    generate_expr(*unary->operand);
                    current_chunk_->emit_op(OpCode::STORE_DEREF, expr.line);
                    current_chunk_->emit_op(OpCode::POP, expr.line);
                }
            } else if (auto array_idx = std::get_if<ArrayIndexExpr>(&arg.operand->node)) {
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                current_chunk_->emit_op(OpCode::ARRAY_LOAD, expr.line);
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                generate_expr(*array_idx->array);
                generate_expr(*array_idx->index);
                current_chunk_->emit_op(OpCode::ARRAY_LOAD, expr.line);
                int one_idx = current_chunk_->add_constant(Value::make_int(1));
                if (arg.operand->resolved_type == DataType::FLOAT) {
                    int one_f_idx = current_chunk_->add_constant(Value::make_float(1.0));
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_FLOAT, one_f_idx, expr.line);
                } else {
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, one_idx, expr.line);
                }
                current_chunk_->emit_op(arg.op == TokenType::PLUS_PLUS ? OpCode::ADD : OpCode::SUB, expr.line);
                current_chunk_->emit_op(OpCode::ARRAY_STORE, expr.line);
                current_chunk_->emit_op(OpCode::POP, expr.line);
            }
        } else if constexpr (std::is_same_v<T, NullExpr>) {
            current_chunk_->emit_op(OpCode::PUSH_NULL, expr.line);
        } else if constexpr (std::is_same_v<T, SizeofExpr>) {
            int size = 8;
            if (arg.target_type == DataType::BOOL) size = 1;
            int idx = current_chunk_->add_constant(Value::make_int(size));
            current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, idx, expr.line);
        } else if constexpr (std::is_same_v<T, TypeofExpr>) {
            generate_expr(*arg.operand);
            current_chunk_->emit_op(OpCode::POP, expr.line);
            std::string t_name = data_type_to_string(arg.operand->resolved_type);
            int idx = current_chunk_->add_constant(Value::make_string(t_name));
            current_chunk_->emit_op_with_arg(OpCode::PUSH_STRING, idx, expr.line);
        } else if constexpr (std::is_same_v<T, ConstructorExpr>) {
            bool is_vector = (arg.type == DataType::VECTOR_INT || arg.type == DataType::VECTOR_FLOAT ||
                              arg.type == DataType::VECTOR_BOOL || arg.type == DataType::VECTOR_STRING ||
                              arg.type == DataType::VECTOR_PAIR_INT_INT || arg.type == DataType::VECTOR_VECTOR_INT ||
                              arg.type == DataType::VECTOR_VECTOR_PAIR_INT_INT);
            bool is_pair = (arg.type == DataType::PAIR_INT_INT);
            bool is_queue = (arg.type == DataType::QUEUE_INT || arg.type == DataType::QUEUE_PAIR_INT_INT);
            bool is_stack = (arg.type == DataType::STACK_INT);

            if (is_vector) {
                uint8_t type_code = 0;
                DataType elem = get_element_type(arg.type);
                if (elem == DataType::FLOAT) type_code = 1;
                else if (elem == DataType::BOOL) type_code = 2;
                else if (elem == DataType::STRING) type_code = 3;

                if (arg.args.empty()) {
                    int idx = current_chunk_->add_constant(Value::make_int(0));
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, idx, expr.line);
                    current_chunk_->emit_op(OpCode::NEW_ARRAY, expr.line);
                    current_chunk_->emit_byte(type_code, expr.line);
                } else {
                    generate_expr(*arg.args[0]);
                    current_chunk_->emit_op(OpCode::NEW_ARRAY, expr.line);
                    current_chunk_->emit_byte(type_code, expr.line);
                    if (arg.args.size() > 1) {
                        generate_expr(*arg.args[1]);
                        current_chunk_->emit_op(OpCode::FILL_ARRAY, expr.line);
                    }
                }
            } else if (is_pair) {
                if (arg.args.empty()) {
                    int idx = current_chunk_->add_constant(Value::make_int(0));
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, idx, expr.line);
                    current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, idx, expr.line);
                    current_chunk_->emit_op(OpCode::NEW_PAIR, expr.line);
                } else {
                    generate_expr(*arg.args[0]);
                    generate_expr(*arg.args[1]);
                    current_chunk_->emit_op(OpCode::NEW_PAIR, expr.line);
                }
            } else if (is_queue || is_stack) {
                int idx = current_chunk_->add_constant(Value::make_int(0));
                current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, idx, expr.line);
                current_chunk_->emit_op(OpCode::NEW_ARRAY, expr.line);
                current_chunk_->emit_byte(0, expr.line);
            }
        } else if constexpr (std::is_same_v<T, MethodCallExpr>) {
            generate_expr(*arg.object);
            DataType obj_type = arg.object->resolved_type;
            bool is_vector = (obj_type == DataType::VECTOR_INT || obj_type == DataType::VECTOR_FLOAT ||
                              obj_type == DataType::VECTOR_BOOL || obj_type == DataType::VECTOR_STRING ||
                              obj_type == DataType::VECTOR_PAIR_INT_INT || obj_type == DataType::VECTOR_VECTOR_INT ||
                              obj_type == DataType::VECTOR_VECTOR_PAIR_INT_INT);
            bool is_queue = (obj_type == DataType::QUEUE_INT || obj_type == DataType::QUEUE_PAIR_INT_INT);
            bool is_stack = (obj_type == DataType::STACK_INT);

            if (is_vector) {
                if (arg.method == "push_back") {
                    generate_expr(*arg.args[0]);
                    current_chunk_->emit_op(OpCode::VECTOR_PUSH_BACK, expr.line);
                } else if (arg.method == "pop_back") {
                    current_chunk_->emit_op(OpCode::VECTOR_POP_BACK, expr.line);
                } else if (arg.method == "size") {
                    current_chunk_->emit_op(OpCode::ARRAY_LENGTH, expr.line);
                } else if (arg.method == "empty") {
                    current_chunk_->emit_op(OpCode::CONTAINER_EMPTY, expr.line);
                } else if (arg.method == "clear") {
                    current_chunk_->emit_op(OpCode::CONTAINER_CLEAR, expr.line);
                }
            } else if (is_queue) {
                if (arg.method == "push") {
                    generate_expr(*arg.args[0]);
                    current_chunk_->emit_op(OpCode::VECTOR_PUSH_BACK, expr.line);
                } else if (arg.method == "pop") {
                    current_chunk_->emit_op(OpCode::QUEUE_POP, expr.line);
                } else if (arg.method == "front") {
                    current_chunk_->emit_op(OpCode::QUEUE_FRONT, expr.line);
                } else if (arg.method == "size") {
                    current_chunk_->emit_op(OpCode::ARRAY_LENGTH, expr.line);
                } else if (arg.method == "empty") {
                    current_chunk_->emit_op(OpCode::CONTAINER_EMPTY, expr.line);
                }
            } else if (is_stack) {
                if (arg.method == "push") {
                    generate_expr(*arg.args[0]);
                    current_chunk_->emit_op(OpCode::VECTOR_PUSH_BACK, expr.line);
                } else if (arg.method == "pop") {
                    current_chunk_->emit_op(OpCode::VECTOR_POP_BACK, expr.line);
                } else if (arg.method == "top") {
                    current_chunk_->emit_op(OpCode::STACK_TOP, expr.line);
                } else if (arg.method == "size") {
                    current_chunk_->emit_op(OpCode::ARRAY_LENGTH, expr.line);
                } else if (arg.method == "empty") {
                    current_chunk_->emit_op(OpCode::CONTAINER_EMPTY, expr.line);
                }
            }
        } else if constexpr (std::is_same_v<T, MemberAccessExpr>) {
            generate_expr(*arg.object);
            int idx_val = (arg.member == "first") ? 0 : 1;
            int idx = current_chunk_->add_constant(Value::make_int(idx_val));
            current_chunk_->emit_op_with_arg(OpCode::PUSH_INT, idx, expr.line);
            current_chunk_->emit_op(OpCode::ARRAY_LOAD, expr.line);
        }
    }, expr.node);
}

void CodeGenerator::generate_binary(const BinaryExpr& expr, int line) {
    generate_expr(*expr.left);
    if (expr.left->resolved_type == DataType::INT && expr.right->resolved_type == DataType::FLOAT) {
        current_chunk_->emit_op(OpCode::INT_TO_FLOAT, line);
    }
    generate_expr(*expr.right);
    if (expr.right->resolved_type == DataType::INT && expr.left->resolved_type == DataType::FLOAT) {
        current_chunk_->emit_op(OpCode::INT_TO_FLOAT, line);
    }

    switch (expr.op) {
        case TokenType::PLUS: current_chunk_->emit_op(OpCode::ADD, line); break;
        case TokenType::MINUS: current_chunk_->emit_op(OpCode::SUB, line); break;
        case TokenType::STAR: current_chunk_->emit_op(OpCode::MUL, line); break;
        case TokenType::SLASH: current_chunk_->emit_op(OpCode::DIV, line); break;
        case TokenType::PERCENT: current_chunk_->emit_op(OpCode::MOD, line); break;
        case TokenType::EQUAL_EQUAL: current_chunk_->emit_op(OpCode::EQ, line); break;
        case TokenType::BANG_EQUAL: current_chunk_->emit_op(OpCode::NEQ, line); break;
        case TokenType::LESS: current_chunk_->emit_op(OpCode::LT, line); break;
        case TokenType::LESS_EQUAL: current_chunk_->emit_op(OpCode::LTE, line); break;
        case TokenType::GREATER: current_chunk_->emit_op(OpCode::GT, line); break;
        case TokenType::GREATER_EQUAL: current_chunk_->emit_op(OpCode::GTE, line); break;
        case TokenType::AMP_AMP: current_chunk_->emit_op(OpCode::AND, line); break;
        case TokenType::PIPE_PIPE: current_chunk_->emit_op(OpCode::OR, line); break;
        case TokenType::PIPE: current_chunk_->emit_op(OpCode::BIT_OR, line); break;
        case TokenType::CARET: current_chunk_->emit_op(OpCode::BIT_XOR, line); break;
        case TokenType::AMP: current_chunk_->emit_op(OpCode::BIT_AND, line); break;
        case TokenType::LESS_LESS: current_chunk_->emit_op(OpCode::BIT_SHL, line); break;
        case TokenType::GREATER_GREATER: current_chunk_->emit_op(OpCode::BIT_SHR, line); break;
        default: break;
    }
}

void CodeGenerator::generate_unary(const UnaryExpr& expr, int line) {
    if (expr.op == TokenType::AMP) {
        if (auto var_ref = std::get_if<VarRefExpr>(&expr.operand->node)) {
            int slot = resolve_local(var_ref->name);
            current_chunk_->emit_op_with_arg(OpCode::LOAD_LOCAL_ADDR, slot, line);
        }
    } else if (expr.op == TokenType::STAR) {
        generate_expr(*expr.operand);
        current_chunk_->emit_op(OpCode::LOAD_DEREF, line);
    } else {
        generate_expr(*expr.operand);
        switch (expr.op) {
            case TokenType::MINUS: current_chunk_->emit_op(OpCode::NEGATE, line); break;
            case TokenType::BANG: current_chunk_->emit_op(OpCode::NOT, line); break;
            case TokenType::TILDE: current_chunk_->emit_op(OpCode::BIT_NOT, line); break;
            default: break;
        }
    }
}

void CodeGenerator::generate_call(const CallExpr& expr, int line) {
    if (expr.callee == "sqrt" || expr.callee == "pow" || expr.callee == "abs" ||
        expr.callee == "min" || expr.callee == "max") {
        for (size_t i = 0; i < expr.args.size(); ++i) {
            generate_expr(*expr.args[i]);
        }
        uint8_t id = 0;
        if (expr.callee == "sqrt") id = 0;
        else if (expr.callee == "pow") id = 1;
        else if (expr.callee == "abs") id = 2;
        else if (expr.callee == "min") id = 3;
        else if (expr.callee == "max") id = 4;

        current_chunk_->emit_op(OpCode::CALL_BUILTIN, line);
        current_chunk_->emit_byte(id, line);
        return;
    }

    const auto& expected_types = fn_param_types_[expr.callee];
    for (size_t i = 0; i < expr.args.size(); ++i) {
        generate_expr(*expr.args[i]);
        if (i < expected_types.size() && expected_types[i] == DataType::FLOAT && expr.args[i]->resolved_type == DataType::INT) {
            current_chunk_->emit_op(OpCode::INT_TO_FLOAT, line);
        }
    }
    int fn_idx = fn_indices_[expr.callee];
    int argc = static_cast<int>(expr.args.size());

    current_chunk_->emit_op(OpCode::CALL, line);
    current_chunk_->emit_byte((fn_idx >> 8) & 0xFF, line);
    current_chunk_->emit_byte(fn_idx & 0xFF, line);
    current_chunk_->emit_byte(argc & 0xFF, line);
}
