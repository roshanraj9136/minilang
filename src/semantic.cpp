#include "semantic.h"
#include <stdexcept>

SemanticAnalyzer::SemanticAnalyzer(ErrorReporter& errors, const std::string& source)
    : errors_(errors), source_(source) {
    enter_scope();
    
    fn_signatures_["sqrt"] = { Param{"x", DataType::FLOAT} };
    fn_return_types_["sqrt"] = DataType::FLOAT;

    fn_signatures_["pow"] = { Param{"base", DataType::FLOAT}, Param{"exponent", DataType::FLOAT} };
    fn_return_types_["pow"] = DataType::FLOAT;

    fn_signatures_["abs"] = { Param{"x", DataType::INT} };
    fn_return_types_["abs"] = DataType::INT;
}

void SemanticAnalyzer::enter_scope() {
    scopes_.push_back(std::unordered_map<std::string, Symbol>());
}

void SemanticAnalyzer::exit_scope() {
    scopes_.pop_back();
}

void SemanticAnalyzer::define(const std::string& name, DataType type, int line) {
    auto& current_scope = scopes_.back();
    if (current_scope.find(name) != current_scope.end()) {
        errors_.report_error(source_, line, 1, "Redefinition of variable '" + name + "' in same scope");
        return;
    }
    current_scope[name] = Symbol{name, type, static_cast<int>(scopes_.size()) - 1};
}

std::optional<Symbol> SemanticAnalyzer::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

void SemanticAnalyzer::analyze(std::vector<StmtPtr>& statements) {
    for (auto& stmt : statements) {
        if (auto fn = std::get_if<FnDeclStmt>(&stmt->node)) {
            if (fn_signatures_.find(fn->name) != fn_signatures_.end()) {
                errors_.report_error(source_, stmt->line, 1, "Redefinition of function '" + fn->name + "'");
                continue;
            }
            fn_signatures_[fn->name] = fn->params;
            fn_return_types_[fn->name] = fn->return_type;
            define(fn->name, fn->return_type, stmt->line);
        }
    }

    for (auto& stmt : statements) {
        analyze_stmt(*stmt);
    }
}

void SemanticAnalyzer::analyze_stmt(Stmt& stmt) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, FnDeclStmt>) {
            analyze_fn_decl(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
            analyze_block(arg);
        } else if constexpr (std::is_same_v<T, VarDeclStmt>) {
            analyze_var_decl(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            analyze_if(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            analyze_while(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ForStmt>) {
            analyze_for(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            analyze_return(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
            analyze_print(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
            analyze_expr_stmt(arg);
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
            if (loop_depth_ <= 0) {
                errors_.report_error(source_, stmt.line, 1, "Cannot use 'break' outside of a loop");
            }
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
            if (loop_depth_ <= 0) {
                errors_.report_error(source_, stmt.line, 1, "Cannot use 'continue' outside of a loop");
            }
        } else if constexpr (std::is_same_v<T, StructDeclStmt>) {
            analyze_struct_decl(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, EnumDeclStmt>) {
            analyze_enum_decl(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            analyze_switch(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, DoWhileStmt>) {
            analyze_do_while(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, ConstDeclStmt>) {
            analyze_const_decl(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, CoutStmt>) {
            analyze_cout(arg, stmt.line);
        } else if constexpr (std::is_same_v<T, CinStmt>) {
            analyze_cin(arg, stmt.line);
        }
    }, stmt.node);
}

void SemanticAnalyzer::analyze_fn_decl(FnDeclStmt& fn, int line) {
    DataType old_return = current_return_type_;
    current_return_type_ = fn.return_type;

    enter_scope();
    for (auto& param : fn.params) {
        define(param.name, param.type, line);
    }

    if (auto block = std::get_if<BlockStmt>(&fn.body->node)) {
        for (auto& stmt : block->statements) {
            analyze_stmt(*stmt);
        }
    }

    exit_scope();
    current_return_type_ = old_return;
}

void SemanticAnalyzer::analyze_block(BlockStmt& block) {
    enter_scope();
    for (auto& stmt : block.statements) {
        analyze_stmt(*stmt);
    }
    exit_scope();
}

void SemanticAnalyzer::analyze_var_decl(VarDeclStmt& decl, int line) {
    DataType init_type = analyze_expr(*decl.initializer);
    if (decl.type != init_type) {
        if (decl.type == DataType::FLOAT && init_type == DataType::INT) {
            // Implicit promotion is allowed.
        } else {
            errors_.report_error(source_, line, 1, "Variable '" + decl.name + "' type mismatch: declared " +
                                                       data_type_to_string(decl.type) + " but initialized with " +
                                                       data_type_to_string(init_type));
        }
    }
    define(decl.name, decl.type, line);
}

void SemanticAnalyzer::analyze_if(IfStmt& stmt, int line) {
    DataType cond_type = analyze_expr(*stmt.condition);
    if (cond_type != DataType::BOOL) {
        errors_.report_error(source_, line, 1, "If condition must be a boolean expression");
    }
    analyze_stmt(*stmt.then_branch);
    if (stmt.else_branch) {
        analyze_stmt(*stmt.else_branch);
    }
}

void SemanticAnalyzer::analyze_while(WhileStmt& stmt, int line) {
    DataType cond_type = analyze_expr(*stmt.condition);
    if (cond_type != DataType::BOOL) {
        errors_.report_error(source_, line, 1, "While condition must be a boolean expression");
    }
    loop_depth_++;
    analyze_stmt(*stmt.body);
    loop_depth_--;
}

void SemanticAnalyzer::analyze_for(ForStmt& stmt, int line) {
    enter_scope();
    analyze_stmt(*stmt.init);
    DataType cond_type = analyze_expr(*stmt.condition);
    if (cond_type != DataType::BOOL) {
        errors_.report_error(source_, line, 1, "For loop condition must be a boolean expression");
    }
    analyze_expr(*stmt.update);
    loop_depth_++;
    analyze_stmt(*stmt.body);
    loop_depth_--;
    exit_scope();
}

void SemanticAnalyzer::analyze_return(ReturnStmt& stmt, int line) {
    DataType ret_type = DataType::VOID;
    if (stmt.value) {
        ret_type = analyze_expr(*stmt.value);
    }
    if (ret_type != current_return_type_) {
        if (current_return_type_ == DataType::FLOAT && ret_type == DataType::INT) {
            // Implicit promotion allowed
        } else {
            errors_.report_error(source_, line, 1, "Return type mismatch: function returns " +
                                                       data_type_to_string(current_return_type_) + " but returned " +
                                                       data_type_to_string(ret_type));
        }
    }
}

void SemanticAnalyzer::analyze_print(PrintStmt& stmt, int) {
    analyze_expr(*stmt.value);
}

void SemanticAnalyzer::analyze_expr_stmt(ExprStmt& stmt) {
    analyze_expr(*stmt.expr);
}

DataType SemanticAnalyzer::analyze_expr(Expr& expr) {
    expr.resolved_type = std::visit([&](auto&& arg) -> DataType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, LiteralIntExpr>) {
            return DataType::INT;
        } else if constexpr (std::is_same_v<T, LiteralFloatExpr>) {
            return DataType::FLOAT;
        } else if constexpr (std::is_same_v<T, LiteralBoolExpr>) {
            return DataType::BOOL;
        } else if constexpr (std::is_same_v<T, LiteralStringExpr>) {
            return DataType::STRING;
        } else if constexpr (std::is_same_v<T, VarRefExpr>) {
            auto sym = lookup(arg.name);
            if (!sym) {
                errors_.report_error(source_, expr.line, 1, "Undefined variable '" + arg.name + "'");
                return DataType::UNKNOWN;
            }
            return sym->type;
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
            return analyze_binary(arg, expr);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
            return analyze_unary(arg, expr);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
            return analyze_call(arg, expr);
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
            return analyze_assign(arg, expr);
        } else if constexpr (std::is_same_v<T, ArrayIndexExpr>) {
            return analyze_array_index(arg, expr);
        } else if constexpr (std::is_same_v<T, NewArrayExpr>) {
            return analyze_new_array(arg, expr);
        } else if constexpr (std::is_same_v<T, LenExpr>) {
            return analyze_len(arg, expr);
        } else if constexpr (std::is_same_v<T, CastExpr>) {
            return analyze_cast(arg, expr);
        } else if constexpr (std::is_same_v<T, TernaryExpr>) {
            return analyze_ternary(arg, expr);
        } else if constexpr (std::is_same_v<T, CompoundAssignExpr>) {
            return analyze_compound_assign(arg, expr);
        } else if constexpr (std::is_same_v<T, PreUnaryExpr>) {
            return analyze_pre_unary(arg, expr);
        } else if constexpr (std::is_same_v<T, PostUnaryExpr>) {
            return analyze_post_unary(arg, expr);
        } else if constexpr (std::is_same_v<T, NullExpr>) {
            return DataType::NULL_TYPE;
        } else if constexpr (std::is_same_v<T, SizeofExpr>) {
            return analyze_sizeof(arg, expr);
        } else if constexpr (std::is_same_v<T, TypeofExpr>) {
            return analyze_typeof(arg, expr);
        } else if constexpr (std::is_same_v<T, ConstructorExpr>) {
            return analyze_constructor(arg, expr);
        } else if constexpr (std::is_same_v<T, MethodCallExpr>) {
            return analyze_method_call(arg, expr);
        } else if constexpr (std::is_same_v<T, MemberAccessExpr>) {
            return analyze_member_access(arg, expr);
        }
        return DataType::UNKNOWN;
    }, expr.node);
    return expr.resolved_type;
}

DataType SemanticAnalyzer::analyze_binary(BinaryExpr& expr, Expr& parent) {
    DataType left = analyze_expr(*expr.left);
    DataType right = analyze_expr(*expr.right);

    switch (expr.op) {
        case TokenType::PLUS:
            if (left == DataType::INT && right == DataType::INT) return DataType::INT;
            if (left == DataType::FLOAT && right == DataType::FLOAT) return DataType::FLOAT;
            if ((left == DataType::INT && right == DataType::FLOAT) || (left == DataType::FLOAT && right == DataType::INT)) {
                return DataType::FLOAT;
            }
            if (left == DataType::STRING && right == DataType::STRING) return DataType::STRING;
            errors_.report_error(source_, parent.line, 1, "Operands for '+' must be both integers, both floats, or both strings");
            return DataType::UNKNOWN;
        case TokenType::MINUS:
        case TokenType::STAR:
        case TokenType::SLASH:
            if (left == DataType::INT && right == DataType::INT) return DataType::INT;
            if (left == DataType::FLOAT && right == DataType::FLOAT) return DataType::FLOAT;
            if ((left == DataType::INT && right == DataType::FLOAT) || (left == DataType::FLOAT && right == DataType::INT)) {
                return DataType::FLOAT;
            }
            errors_.report_error(source_, parent.line, 1, "Operands for arithmetic operator must be numeric");
            return DataType::UNKNOWN;
        case TokenType::PERCENT:
            if (left == DataType::INT && right == DataType::INT) return DataType::INT;
            errors_.report_error(source_, parent.line, 1, "Operands for modulo operator '%' must be integers");
            return DataType::UNKNOWN;
        case TokenType::PIPE:
        case TokenType::CARET:
        case TokenType::AMP:
        case TokenType::LESS_LESS:
        case TokenType::GREATER_GREATER:
            if (left == DataType::INT && right == DataType::INT) return DataType::INT;
            errors_.report_error(source_, parent.line, 1, "Operands for bitwise operators must be integers");
            return DataType::UNKNOWN;
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            if (left == DataType::INT && right == DataType::INT) return DataType::BOOL;
            if (left == DataType::FLOAT && right == DataType::FLOAT) return DataType::BOOL;
            if ((left == DataType::INT && right == DataType::FLOAT) || (left == DataType::FLOAT && right == DataType::INT)) {
                return DataType::BOOL;
            }
            errors_.report_error(source_, parent.line, 1, "Operands for comparison operator must be numeric");
            return DataType::UNKNOWN;
        case TokenType::EQUAL_EQUAL:
        case TokenType::BANG_EQUAL:
            if (left == right && left != DataType::UNKNOWN) return DataType::BOOL;
            if ((left == DataType::INT && right == DataType::FLOAT) || (left == DataType::FLOAT && right == DataType::INT)) {
                return DataType::BOOL;
            }
            errors_.report_error(source_, parent.line, 1, "Operands for equality check must be compatible types");
            return DataType::UNKNOWN;
        case TokenType::AMP_AMP:
        case TokenType::PIPE_PIPE:
            if (left == DataType::BOOL && right == DataType::BOOL) return DataType::BOOL;
            errors_.report_error(source_, parent.line, 1, "Operands for logical operators must be boolean");
            return DataType::UNKNOWN;
        default:
            return DataType::UNKNOWN;
    }
}

DataType SemanticAnalyzer::analyze_unary(UnaryExpr& expr, Expr& parent) {
    DataType operand = analyze_expr(*expr.operand);

    switch (expr.op) {
        case TokenType::MINUS:
            if (operand == DataType::INT) return DataType::INT;
            if (operand == DataType::FLOAT) return DataType::FLOAT;
            errors_.report_error(source_, parent.line, 1, "Operand for unary '-' must be numeric");
            return DataType::UNKNOWN;
        case TokenType::BANG:
            if (operand == DataType::BOOL) return DataType::BOOL;
            errors_.report_error(source_, parent.line, 1, "Operand for '!' must be a boolean");
            return DataType::UNKNOWN;
        case TokenType::TILDE:
            if (operand == DataType::INT) return DataType::INT;
            errors_.report_error(source_, parent.line, 1, "Operand for '~' must be an integer");
            return DataType::UNKNOWN;
        case TokenType::AMP: {
            bool is_lvalue = false;
            if (std::holds_alternative<VarRefExpr>(expr.operand->node)) is_lvalue = true;
            else if (std::holds_alternative<ArrayIndexExpr>(expr.operand->node)) is_lvalue = true;
            else if (auto unary = std::get_if<UnaryExpr>(&expr.operand->node)) {
                if (unary->op == TokenType::STAR) is_lvalue = true;
            }
            if (!is_lvalue) {
                errors_.report_error(source_, parent.line, 1, "Cannot take address of non-lvalue expression");
                return DataType::UNKNOWN;
            }
            if (operand == DataType::INT) return DataType::INT_PTR;
            if (operand == DataType::FLOAT) return DataType::FLOAT_PTR;
            if (operand == DataType::BOOL) return DataType::BOOL_PTR;
            if (operand == DataType::STRING) return DataType::STRING_PTR;
            errors_.report_error(source_, parent.line, 1, "Cannot take address of this expression type");
            return DataType::UNKNOWN;
        }
        case TokenType::STAR:
            if (operand == DataType::INT_PTR) return DataType::INT;
            if (operand == DataType::FLOAT_PTR) return DataType::FLOAT;
            if (operand == DataType::BOOL_PTR) return DataType::BOOL;
            if (operand == DataType::STRING_PTR) return DataType::STRING;
            errors_.report_error(source_, parent.line, 1, "Cannot dereference non-pointer type " + data_type_to_string(operand));
            return DataType::UNKNOWN;
        default:
            return DataType::UNKNOWN;
    }
}

DataType SemanticAnalyzer::analyze_call(CallExpr& expr, Expr& parent) {
    if (expr.callee == "min" || expr.callee == "max") {
        if (expr.args.size() != 2) {
            errors_.report_error(source_, parent.line, 1, "Function '" + expr.callee + "' expects 2 arguments, but got " + std::to_string(expr.args.size()));
            return DataType::UNKNOWN;
        }
        DataType t1 = analyze_expr(*expr.args[0]);
        DataType t2 = analyze_expr(*expr.args[1]);
        if (t1 == DataType::INT && t2 == DataType::INT) {
            return DataType::INT;
        }
        if ((t1 == DataType::FLOAT || t1 == DataType::INT) && (t2 == DataType::FLOAT || t2 == DataType::INT)) {
            return DataType::FLOAT;
        }
        errors_.report_error(source_, parent.line, 1, "Invalid arguments to '" + expr.callee + "': expected numeric types");
        return DataType::UNKNOWN;
    }

    auto signature = fn_signatures_.find(expr.callee);
    if (signature == fn_signatures_.end()) {
        errors_.report_error(source_, parent.line, 1, "Call to undefined function '" + expr.callee + "'");
        return DataType::UNKNOWN;
    }

    auto& expected_params = signature->second;
    if (expected_params.size() != expr.args.size()) {
        errors_.report_error(source_, parent.line, 1, "Function '" + expr.callee + "' expects " +
                                                   std::to_string(expected_params.size()) + " arguments, but got " +
                                                   std::to_string(expr.args.size()));
        return fn_return_types_[expr.callee];
    }

    for (size_t i = 0; i < expected_params.size(); ++i) {
        DataType arg_type = analyze_expr(*expr.args[i]);
        if (arg_type != expected_params[i].type) {
            if (expected_params[i].type == DataType::FLOAT && arg_type == DataType::INT) {
                // Implicit promotion allowed
            } else {
                errors_.report_error(source_, parent.line, 1, "Argument " + std::to_string(i + 1) + " of function '" +
                                                           expr.callee + "' type mismatch: expected " +
                                                           data_type_to_string(expected_params[i].type) + " but got " +
                                                           data_type_to_string(arg_type));
            }
        }
    }

    return fn_return_types_[expr.callee];
}

DataType SemanticAnalyzer::analyze_assign(AssignExpr& expr, Expr& parent) {
    bool is_lvalue = false;
    if (std::holds_alternative<VarRefExpr>(expr.lhs->node)) is_lvalue = true;
    else if (std::holds_alternative<ArrayIndexExpr>(expr.lhs->node)) is_lvalue = true;
    else if (auto unary = std::get_if<UnaryExpr>(&expr.lhs->node)) {
        if (unary->op == TokenType::STAR) is_lvalue = true;
    }
    if (!is_lvalue) {
        errors_.report_error(source_, parent.line, 1, "Left-hand side of assignment must be an lvalue");
        return DataType::UNKNOWN;
    }

    // Check if the variable is a constant
    if (auto var_ref = std::get_if<VarRefExpr>(&expr.lhs->node)) {
        if (constants_.find(var_ref->name) != constants_.end()) {
            errors_.report_error(source_, parent.line, 1, "Cannot assign to constant variable '" + var_ref->name + "'");
            return DataType::UNKNOWN;
        }
    }

    DataType lhs_type = analyze_expr(*expr.lhs);
    DataType val_type = analyze_expr(*expr.value);

    if (lhs_type != val_type) {
        if (lhs_type == DataType::FLOAT && val_type == DataType::INT) {
            // Implicit promotion is allowed.
        } else {
            errors_.report_error(source_, parent.line, 1, "Type mismatch in assignment: expected " +
                                                       data_type_to_string(lhs_type) + " but got " +
                                                       data_type_to_string(val_type));
        }
    }

    return lhs_type;
}

DataType SemanticAnalyzer::analyze_array_index(ArrayIndexExpr& expr, Expr& parent) {
    DataType array_type = analyze_expr(*expr.array);
    DataType index_type = analyze_expr(*expr.index);

    if (index_type != DataType::INT) {
        errors_.report_error(source_, parent.line, 1, "Array index must be an integer");
    }

    if (array_type == DataType::INT_ARRAY) return DataType::INT;
    if (array_type == DataType::FLOAT_ARRAY) return DataType::FLOAT;
    if (array_type == DataType::BOOL_ARRAY) return DataType::BOOL;
    if (array_type == DataType::STRING_ARRAY) return DataType::STRING;
    if (array_type == DataType::STRING) return DataType::INT;

    if (array_type == DataType::VECTOR_INT) return DataType::INT;
    if (array_type == DataType::VECTOR_FLOAT) return DataType::FLOAT;
    if (array_type == DataType::VECTOR_BOOL) return DataType::BOOL;
    if (array_type == DataType::VECTOR_STRING) return DataType::STRING;
    if (array_type == DataType::VECTOR_PAIR_INT_INT) return DataType::PAIR_INT_INT;
    if (array_type == DataType::VECTOR_VECTOR_INT) return DataType::VECTOR_INT;
    if (array_type == DataType::VECTOR_VECTOR_PAIR_INT_INT) return DataType::VECTOR_PAIR_INT_INT;

    errors_.report_error(source_, parent.line, 1, "Cannot index non-array type " + data_type_to_string(array_type));
    return DataType::UNKNOWN;
}

DataType SemanticAnalyzer::analyze_new_array(NewArrayExpr& expr, Expr& parent) {
    DataType size_type = analyze_expr(*expr.size);
    if (size_type != DataType::INT) {
        errors_.report_error(source_, parent.line, 1, "Array size must be an integer");
    }

    if (expr.element_type == DataType::INT) return DataType::INT_ARRAY;
    if (expr.element_type == DataType::FLOAT) return DataType::FLOAT_ARRAY;
    if (expr.element_type == DataType::BOOL) return DataType::BOOL_ARRAY;
    if (expr.element_type == DataType::STRING) return DataType::STRING_ARRAY;

    errors_.report_error(source_, parent.line, 1, "Invalid array element type");
    return DataType::UNKNOWN;
}

DataType SemanticAnalyzer::analyze_len(LenExpr& expr, Expr& parent) {
    DataType array_type = analyze_expr(*expr.array);
    if (array_type != DataType::INT_ARRAY && array_type != DataType::FLOAT_ARRAY &&
        array_type != DataType::BOOL_ARRAY && array_type != DataType::STRING_ARRAY &&
        array_type != DataType::STRING) {
        errors_.report_error(source_, parent.line, 1, "Argument to 'len' must be an array or string");
    }
    return DataType::INT;
}

void SemanticAnalyzer::analyze_struct_decl(StructDeclStmt& stmt, int line) {
    if (struct_defs_.find(stmt.name) != struct_defs_.end()) {
        errors_.report_error(source_, line, 1, "Redefinition of struct '" + stmt.name + "'");
        return;
    }
    struct_defs_[stmt.name] = stmt.fields;
}

void SemanticAnalyzer::analyze_enum_decl(EnumDeclStmt& stmt, int line) {
    if (enum_defs_.find(stmt.name) != enum_defs_.end()) {
        errors_.report_error(source_, line, 1, "Redefinition of enum '" + stmt.name + "'");
        return;
    }
    std::unordered_map<std::string, int64_t> vals;
    for (auto& member : stmt.values) {
        vals[member.first] = member.second;
        define(member.first, DataType::INT, line);
        constants_.insert(member.first);
    }
    enum_defs_[stmt.name] = vals;
}

void SemanticAnalyzer::analyze_switch(SwitchStmt& stmt, int line) {
    DataType val_type = analyze_expr(*stmt.value);
    for (auto& c : stmt.cases) {
        DataType case_type = analyze_expr(*c.first);
        if (case_type != val_type) {
            errors_.report_error(source_, line, 1, "Case expression type mismatch: expected " +
                                                       data_type_to_string(val_type) + " but got " +
                                                       data_type_to_string(case_type));
        }
        analyze_stmt(*c.second);
    }
    if (stmt.default_case) {
        analyze_stmt(*stmt.default_case);
    }
}

void SemanticAnalyzer::analyze_do_while(DoWhileStmt& stmt, int line) {
    loop_depth_++;
    analyze_stmt(*stmt.body);
    loop_depth_--;
    DataType cond_type = analyze_expr(*stmt.condition);
    if (cond_type != DataType::BOOL) {
        errors_.report_error(source_, line, 1, "Do-while condition must be a boolean expression");
    }
}

void SemanticAnalyzer::analyze_const_decl(ConstDeclStmt& stmt, int line) {
    DataType init_type = analyze_expr(*stmt.initializer);
    if (stmt.type != init_type) {
        if (stmt.type == DataType::FLOAT && init_type == DataType::INT) {
            // Implicit promotion allowed
        } else {
            errors_.report_error(source_, line, 1, "Constant '" + stmt.name + "' type mismatch: declared " +
                                                       data_type_to_string(stmt.type) + " but initialized with " +
                                                       data_type_to_string(init_type));
        }
    }
    define(stmt.name, stmt.type, line);
    constants_.insert(stmt.name);
}

DataType SemanticAnalyzer::analyze_cast(CastExpr& expr, Expr& parent) {
    DataType op_type = analyze_expr(*expr.operand);
    if (op_type == DataType::UNKNOWN || expr.target_type == DataType::UNKNOWN) return DataType::UNKNOWN;

    bool is_op_ok = (op_type == DataType::INT || op_type == DataType::FLOAT || op_type == DataType::BOOL);
    bool is_target_ok = (expr.target_type == DataType::INT || expr.target_type == DataType::FLOAT || expr.target_type == DataType::BOOL);

    if (!is_op_ok || !is_target_ok) {
        errors_.report_error(source_, parent.line, 1, "Invalid cast from " + data_type_to_string(op_type) + " to " + data_type_to_string(expr.target_type));
        return DataType::UNKNOWN;
    }
    return expr.target_type;
}

DataType SemanticAnalyzer::analyze_ternary(TernaryExpr& expr, Expr& parent) {
    DataType cond_type = analyze_expr(*expr.condition);
    if (cond_type != DataType::BOOL) {
        errors_.report_error(source_, parent.line, 1, "Ternary condition must be a boolean expression");
    }
    DataType then_type = analyze_expr(*expr.then_expr);
    DataType else_type = analyze_expr(*expr.else_expr);

    if (then_type != else_type) {
        if (then_type == DataType::FLOAT && else_type == DataType::INT) return DataType::FLOAT;
        if (then_type == DataType::INT && else_type == DataType::FLOAT) return DataType::FLOAT;
        errors_.report_error(source_, parent.line, 1, "Ternary branch types mismatch: then branch is " +
                                                   data_type_to_string(then_type) + " but else branch is " +
                                                   data_type_to_string(else_type));
        return DataType::UNKNOWN;
    }
    return then_type;
}

DataType SemanticAnalyzer::analyze_compound_assign(CompoundAssignExpr& expr, Expr& parent) {
    bool is_lvalue = false;
    if (std::holds_alternative<VarRefExpr>(expr.target->node)) is_lvalue = true;
    else if (std::holds_alternative<ArrayIndexExpr>(expr.target->node)) is_lvalue = true;
    else if (auto unary = std::get_if<UnaryExpr>(&expr.target->node)) {
        if (unary->op == TokenType::STAR) is_lvalue = true;
    }
    if (!is_lvalue) {
        errors_.report_error(source_, parent.line, 1, "Left-hand side of compound assignment must be an lvalue");
        return DataType::UNKNOWN;
    }

    if (auto var_ref = std::get_if<VarRefExpr>(&expr.target->node)) {
        if (constants_.find(var_ref->name) != constants_.end()) {
            errors_.report_error(source_, parent.line, 1, "Cannot assign to constant variable '" + var_ref->name + "'");
            return DataType::UNKNOWN;
        }
    }

    DataType target_type = analyze_expr(*expr.target);
    DataType value_type = analyze_expr(*expr.value);

    if (target_type != value_type) {
        if (target_type == DataType::FLOAT && value_type == DataType::INT) {
            // Implicit promotion allowed
        } else {
            errors_.report_error(source_, parent.line, 1, "Type mismatch in compound assignment: expected " +
                                                       data_type_to_string(target_type) + " but got " +
                                                       data_type_to_string(value_type));
        }
    }
    return target_type;
}

DataType SemanticAnalyzer::analyze_pre_unary(PreUnaryExpr& expr, Expr& parent) {
    DataType op_type = analyze_expr(*expr.operand);
    bool is_lvalue = false;
    if (std::holds_alternative<VarRefExpr>(expr.operand->node)) is_lvalue = true;
    else if (std::holds_alternative<ArrayIndexExpr>(expr.operand->node)) is_lvalue = true;
    else if (auto unary = std::get_if<UnaryExpr>(&expr.operand->node)) {
        if (unary->op == TokenType::STAR) is_lvalue = true;
    }
    if (!is_lvalue) {
        errors_.report_error(source_, parent.line, 1, "Operand of increment/decrement must be an lvalue");
        return DataType::UNKNOWN;
    }

    if (auto var_ref = std::get_if<VarRefExpr>(&expr.operand->node)) {
        if (constants_.find(var_ref->name) != constants_.end()) {
            errors_.report_error(source_, parent.line, 1, "Cannot modify constant variable '" + var_ref->name + "'");
            return DataType::UNKNOWN;
        }
    }

    if (op_type != DataType::INT && op_type != DataType::FLOAT) {
        errors_.report_error(source_, parent.line, 1, "Increment/decrement operand must be numeric");
        return DataType::UNKNOWN;
    }
    return op_type;
}

DataType SemanticAnalyzer::analyze_post_unary(PostUnaryExpr& expr, Expr& parent) {
    DataType op_type = analyze_expr(*expr.operand);
    bool is_lvalue = false;
    if (std::holds_alternative<VarRefExpr>(expr.operand->node)) is_lvalue = true;
    else if (std::holds_alternative<ArrayIndexExpr>(expr.operand->node)) is_lvalue = true;
    else if (auto unary = std::get_if<UnaryExpr>(&expr.operand->node)) {
        if (unary->op == TokenType::STAR) is_lvalue = true;
    }
    if (!is_lvalue) {
        errors_.report_error(source_, parent.line, 1, "Operand of increment/decrement must be an lvalue");
        return DataType::UNKNOWN;
    }

    if (auto var_ref = std::get_if<VarRefExpr>(&expr.operand->node)) {
        if (constants_.find(var_ref->name) != constants_.end()) {
            errors_.report_error(source_, parent.line, 1, "Cannot modify constant variable '" + var_ref->name + "'");
            return DataType::UNKNOWN;
        }
    }

    if (op_type != DataType::INT && op_type != DataType::FLOAT) {
        errors_.report_error(source_, parent.line, 1, "Increment/decrement operand must be numeric");
        return DataType::UNKNOWN;
    }
    return op_type;
}

DataType SemanticAnalyzer::analyze_sizeof(SizeofExpr&, Expr&) {
    return DataType::INT;
}

DataType SemanticAnalyzer::analyze_typeof(TypeofExpr& expr, Expr&) {
    analyze_expr(*expr.operand);
    return DataType::STRING;
}

void SemanticAnalyzer::analyze_cout(CoutStmt& stmt, int line) {
    for (auto& expr : stmt.exprs) {
        DataType t = analyze_expr(*expr);
        if (t == DataType::UNKNOWN) {
            errors_.report_error(source_, line, 1, "Invalid expression in cout statement");
        }
    }
}

void SemanticAnalyzer::analyze_cin(CinStmt& stmt, int line) {
    for (auto& target : stmt.targets) {
        DataType t = analyze_expr(*target);
        if (t == DataType::UNKNOWN) {
            errors_.report_error(source_, line, 1, "Invalid target in cin statement");
            continue;
        }
        
        bool is_lvalue = false;
        if (auto var_ref = std::get_if<VarRefExpr>(&target->node)) {
            is_lvalue = true;
            if (constants_.find(var_ref->name) != constants_.end()) {
                errors_.report_error(source_, line, 1, "Cannot input to constant variable '" + var_ref->name + "'");
            }
        } else if (auto unary = std::get_if<UnaryExpr>(&target->node)) {
            if (unary->op == TokenType::STAR) {
                is_lvalue = true;
            }
        } else if (std::holds_alternative<ArrayIndexExpr>(target->node)) {
            is_lvalue = true;
        }
        
        if (!is_lvalue) {
            errors_.report_error(source_, line, 1, "Target of cin statement must be a modifiable lvalue");
        }
    }
}

static DataType get_element_type(DataType t) {
    if (t == DataType::VECTOR_INT || t == DataType::QUEUE_INT || t == DataType::STACK_INT) return DataType::INT;
    if (t == DataType::VECTOR_FLOAT) return DataType::FLOAT;
    if (t == DataType::VECTOR_BOOL) return DataType::BOOL;
    if (t == DataType::VECTOR_STRING) return DataType::STRING;
    if (t == DataType::VECTOR_PAIR_INT_INT || t == DataType::QUEUE_PAIR_INT_INT) return DataType::PAIR_INT_INT;
    if (t == DataType::VECTOR_VECTOR_INT) return DataType::VECTOR_INT;
    if (t == DataType::VECTOR_VECTOR_PAIR_INT_INT) return DataType::VECTOR_PAIR_INT_INT;
    return DataType::UNKNOWN;
}

DataType SemanticAnalyzer::analyze_constructor(ConstructorExpr& expr, Expr& parent) {
    for (auto& arg : expr.args) {
        analyze_expr(*arg);
    }
    
    // Typecheck vector constructor args
    if (expr.type == DataType::VECTOR_INT || expr.type == DataType::VECTOR_FLOAT ||
        expr.type == DataType::VECTOR_BOOL || expr.type == DataType::VECTOR_STRING ||
        expr.type == DataType::VECTOR_PAIR_INT_INT || expr.type == DataType::VECTOR_VECTOR_INT ||
        expr.type == DataType::VECTOR_VECTOR_PAIR_INT_INT) {
        if (!expr.args.empty()) {
            if (expr.args[0]->resolved_type != DataType::INT) {
                errors_.report_error(source_, parent.line, 1, "Vector size argument must be an integer");
            }
            if (expr.args.size() > 1) {
                DataType elem_type = get_element_type(expr.type);
                if (expr.args[1]->resolved_type != elem_type) {
                    errors_.report_error(source_, parent.line, 1, "Vector initializer type mismatch");
                }
            }
        }
    }
    return expr.type;
}

DataType SemanticAnalyzer::analyze_method_call(MethodCallExpr& expr, Expr& parent) {
    DataType obj_type = analyze_expr(*expr.object);
    for (auto& arg : expr.args) {
        analyze_expr(*arg);
    }
    
    bool is_vector = (obj_type == DataType::VECTOR_INT || obj_type == DataType::VECTOR_FLOAT ||
                      obj_type == DataType::VECTOR_BOOL || obj_type == DataType::VECTOR_STRING ||
                      obj_type == DataType::VECTOR_PAIR_INT_INT || obj_type == DataType::VECTOR_VECTOR_INT ||
                      obj_type == DataType::VECTOR_VECTOR_PAIR_INT_INT);
    bool is_queue = (obj_type == DataType::QUEUE_INT || obj_type == DataType::QUEUE_PAIR_INT_INT);
    bool is_stack = (obj_type == DataType::STACK_INT);
    
    if (is_vector) {
        if (expr.method == "push_back") {
            if (expr.args.size() != 1) {
                errors_.report_error(source_, parent.line, 1, "push_back expects 1 argument");
                return DataType::VOID;
            }
            DataType elem = get_element_type(obj_type);
            if (expr.args[0]->resolved_type != elem) {
                errors_.report_error(source_, parent.line, 1, "push_back argument type mismatch");
            }
            return DataType::VOID;
        } else if (expr.method == "pop_back") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "pop_back expects 0 arguments");
            return DataType::VOID;
        } else if (expr.method == "size") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "size expects 0 arguments");
            return DataType::INT;
        } else if (expr.method == "empty") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "empty expects 0 arguments");
            return DataType::BOOL;
        } else if (expr.method == "clear") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "clear expects 0 arguments");
            return DataType::VOID;
        }
    } else if (is_queue) {
        if (expr.method == "push") {
            if (expr.args.size() != 1) {
                errors_.report_error(source_, parent.line, 1, "push expects 1 argument");
                return DataType::VOID;
            }
            DataType elem = get_element_type(obj_type);
            if (expr.args[0]->resolved_type != elem) {
                errors_.report_error(source_, parent.line, 1, "push argument type mismatch");
            }
            return DataType::VOID;
        } else if (expr.method == "pop") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "pop expects 0 arguments");
            return DataType::VOID;
        } else if (expr.method == "front") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "front expects 0 arguments");
            return get_element_type(obj_type);
        } else if (expr.method == "size") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "size expects 0 arguments");
            return DataType::INT;
        } else if (expr.method == "empty") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "empty expects 0 arguments");
            return DataType::BOOL;
        }
    } else if (is_stack) {
        if (expr.method == "push") {
            if (expr.args.size() != 1) {
                errors_.report_error(source_, parent.line, 1, "push expects 1 argument");
                return DataType::VOID;
            }
            DataType elem = get_element_type(obj_type);
            if (expr.args[0]->resolved_type != elem) {
                errors_.report_error(source_, parent.line, 1, "push argument type mismatch");
            }
            return DataType::VOID;
        } else if (expr.method == "pop") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "pop expects 0 arguments");
            return DataType::VOID;
        } else if (expr.method == "top") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "top expects 0 arguments");
            return get_element_type(obj_type);
        } else if (expr.method == "size") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "size expects 0 arguments");
            return DataType::INT;
        } else if (expr.method == "empty") {
            if (!expr.args.empty()) errors_.report_error(source_, parent.line, 1, "empty expects 0 arguments");
            return DataType::BOOL;
        }
    }
    
    errors_.report_error(source_, parent.line, 1, "Method '" + expr.method + "' not found on type " + data_type_to_string(obj_type));
    return DataType::UNKNOWN;
}

DataType SemanticAnalyzer::analyze_member_access(MemberAccessExpr& expr, Expr& parent) {
    DataType obj_type = analyze_expr(*expr.object);
    if (obj_type == DataType::PAIR_INT_INT) {
        if (expr.member == "first" || expr.member == "second") {
            return DataType::INT;
        }
    }
    errors_.report_error(source_, parent.line, 1, "Member '" + expr.member + "' not found on type " + data_type_to_string(obj_type));
    return DataType::UNKNOWN;
}
