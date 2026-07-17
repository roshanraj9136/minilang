#include "optimizer.h"
#include <variant>
#include <cmath>

void Optimizer::optimize(std::vector<StmtPtr>& statements) {
    for (auto& stmt : statements) {
        if (stmt) {
            optimize_stmt(*stmt);
        }
    }
}

void Optimizer::optimize_stmt(Stmt& stmt) {
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, VarDeclStmt>) {
            arg.initializer = optimize_expr(std::move(arg.initializer));
        } else if constexpr (std::is_same_v<T, ConstDeclStmt>) {
            arg.initializer = optimize_expr(std::move(arg.initializer));
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
            for (auto& s : arg.statements) {
                if (s) optimize_stmt(*s);
            }
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            arg.condition = optimize_expr(std::move(arg.condition));
            if (arg.then_branch) optimize_stmt(*arg.then_branch);
            if (arg.else_branch) optimize_stmt(*arg.else_branch);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            arg.condition = optimize_expr(std::move(arg.condition));
            if (arg.body) optimize_stmt(*arg.body);
        } else if constexpr (std::is_same_v<T, DoWhileStmt>) {
            arg.condition = optimize_expr(std::move(arg.condition));
            if (arg.body) optimize_stmt(*arg.body);
        } else if constexpr (std::is_same_v<T, ForStmt>) {
            if (arg.init) optimize_stmt(*arg.init);
            arg.condition = optimize_expr(std::move(arg.condition));
            arg.update = optimize_expr(std::move(arg.update));
            if (arg.body) optimize_stmt(*arg.body);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            if (arg.value) arg.value = optimize_expr(std::move(arg.value));
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
            arg.value = optimize_expr(std::move(arg.value));
        } else if constexpr (std::is_same_v<T, FnDeclStmt>) {
            if (arg.body) optimize_stmt(*arg.body);
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
            arg.expr = optimize_expr(std::move(arg.expr));
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            arg.value = optimize_expr(std::move(arg.value));
            for (auto& c : arg.cases) {
                c.first = optimize_expr(std::move(c.first));
                if (c.second) optimize_stmt(*c.second);
            }
            if (arg.default_case) optimize_stmt(*arg.default_case);
        } else if constexpr (std::is_same_v<T, CoutStmt>) {
            for (auto& e : arg.exprs) {
                e = optimize_expr(std::move(e));
            }
        } else if constexpr (std::is_same_v<T, CinStmt>) {
            for (auto& e : arg.targets) {
                e = optimize_expr(std::move(e));
            }
        }
    }, stmt.node);
}

ExprPtr Optimizer::optimize_expr(ExprPtr expr) {
    if (!expr) return nullptr;

    // Recursively optimize children first
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, BinaryExpr>) {
            arg.left = optimize_expr(std::move(arg.left));
            arg.right = optimize_expr(std::move(arg.right));
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
            arg.operand = optimize_expr(std::move(arg.operand));
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
            arg.lhs = optimize_expr(std::move(arg.lhs));
            arg.value = optimize_expr(std::move(arg.value));
        } else if constexpr (std::is_same_v<T, CallExpr>) {
            for (auto& a : arg.args) {
                a = optimize_expr(std::move(a));
            }
        } else if constexpr (std::is_same_v<T, ArrayIndexExpr>) {
            arg.array = optimize_expr(std::move(arg.array));
            arg.index = optimize_expr(std::move(arg.index));
        } else if constexpr (std::is_same_v<T, NewArrayExpr>) {
            arg.size = optimize_expr(std::move(arg.size));
        } else if constexpr (std::is_same_v<T, LenExpr>) {
            arg.array = optimize_expr(std::move(arg.array));
        } else if constexpr (std::is_same_v<T, CastExpr>) {
            arg.operand = optimize_expr(std::move(arg.operand));
        } else if constexpr (std::is_same_v<T, TernaryExpr>) {
            arg.condition = optimize_expr(std::move(arg.condition));
            arg.then_expr = optimize_expr(std::move(arg.then_expr));
            arg.else_expr = optimize_expr(std::move(arg.else_expr));
        } else if constexpr (std::is_same_v<T, CompoundAssignExpr>) {
            arg.target = optimize_expr(std::move(arg.target));
            arg.value = optimize_expr(std::move(arg.value));
        } else if constexpr (std::is_same_v<T, PreUnaryExpr>) {
            arg.operand = optimize_expr(std::move(arg.operand));
        } else if constexpr (std::is_same_v<T, PostUnaryExpr>) {
            arg.operand = optimize_expr(std::move(arg.operand));
        } else if constexpr (std::is_same_v<T, TypeofExpr>) {
            arg.operand = optimize_expr(std::move(arg.operand));
        } else if constexpr (std::is_same_v<T, ConstructorExpr>) {
            for (auto& a : arg.args) {
                a = optimize_expr(std::move(a));
            }
        } else if constexpr (std::is_same_v<T, MethodCallExpr>) {
            arg.object = optimize_expr(std::move(arg.object));
            for (auto& a : arg.args) {
                a = optimize_expr(std::move(a));
            }
        } else if constexpr (std::is_same_v<T, MemberAccessExpr>) {
            arg.object = optimize_expr(std::move(arg.object));
        }
    }, expr->node);

    // Try constant folding for this expression
    if (std::holds_alternative<BinaryExpr>(expr->node)) {
        auto& bin = std::get<BinaryExpr>(expr->node);
        Expr* left = bin.left.get();
        Expr* right = bin.right.get();

        if (!left || !right) return expr;

        // 1. Double Int Operations
        if (std::holds_alternative<LiteralIntExpr>(left->node) &&
            std::holds_alternative<LiteralIntExpr>(right->node)) {
            int64_t l_val = std::get<LiteralIntExpr>(left->node).value;
            int64_t r_val = std::get<LiteralIntExpr>(right->node).value;

            switch (bin.op) {
                case TokenType::PLUS: return make_int_literal(expr->line, l_val + r_val);
                case TokenType::MINUS: return make_int_literal(expr->line, l_val - r_val);
                case TokenType::STAR: return make_int_literal(expr->line, l_val * r_val);
                case TokenType::SLASH:
                    if (r_val != 0) return make_int_literal(expr->line, l_val / r_val);
                    break;
                case TokenType::PERCENT:
                    if (r_val != 0) return make_int_literal(expr->line, l_val % r_val);
                    break;
                case TokenType::EQUAL_EQUAL: return make_bool_literal(expr->line, l_val == r_val);
                case TokenType::BANG_EQUAL: return make_bool_literal(expr->line, l_val != r_val);
                case TokenType::LESS: return make_bool_literal(expr->line, l_val < r_val);
                case TokenType::LESS_EQUAL: return make_bool_literal(expr->line, l_val <= r_val);
                case TokenType::GREATER: return make_bool_literal(expr->line, l_val > r_val);
                case TokenType::GREATER_EQUAL: return make_bool_literal(expr->line, l_val >= r_val);
                case TokenType::AMP: return make_int_literal(expr->line, l_val & r_val);
                case TokenType::PIPE: return make_int_literal(expr->line, l_val | r_val);
                case TokenType::CARET: return make_int_literal(expr->line, l_val ^ r_val);
                case TokenType::LESS_LESS: return make_int_literal(expr->line, l_val << r_val);
                case TokenType::GREATER_GREATER: return make_int_literal(expr->line, l_val >> r_val);
                default: break;
            }
        }
        // 2. Double Float Operations
        else if (std::holds_alternative<LiteralFloatExpr>(left->node) &&
                 std::holds_alternative<LiteralFloatExpr>(right->node)) {
            double l_val = std::get<LiteralFloatExpr>(left->node).value;
            double r_val = std::get<LiteralFloatExpr>(right->node).value;

            switch (bin.op) {
                case TokenType::PLUS: return make_float_literal(expr->line, l_val + r_val);
                case TokenType::MINUS: return make_float_literal(expr->line, l_val - r_val);
                case TokenType::STAR: return make_float_literal(expr->line, l_val * r_val);
                case TokenType::SLASH:
                    if (r_val != 0.0) return make_float_literal(expr->line, l_val / r_val);
                    break;
                case TokenType::EQUAL_EQUAL: return make_bool_literal(expr->line, l_val == r_val);
                case TokenType::BANG_EQUAL: return make_bool_literal(expr->line, l_val != r_val);
                case TokenType::LESS: return make_bool_literal(expr->line, l_val < r_val);
                case TokenType::LESS_EQUAL: return make_bool_literal(expr->line, l_val <= r_val);
                case TokenType::GREATER: return make_bool_literal(expr->line, l_val > r_val);
                case TokenType::GREATER_EQUAL: return make_bool_literal(expr->line, l_val >= r_val);
                default: break;
            }
        }
        // 3. Double Bool Operations
        else if (std::holds_alternative<LiteralBoolExpr>(left->node) &&
                 std::holds_alternative<LiteralBoolExpr>(right->node)) {
            bool l_val = std::get<LiteralBoolExpr>(left->node).value;
            bool r_val = std::get<LiteralBoolExpr>(right->node).value;

            switch (bin.op) {
                case TokenType::AMP_AMP: return make_bool_literal(expr->line, l_val && r_val);
                case TokenType::PIPE_PIPE: return make_bool_literal(expr->line, l_val || r_val);
                case TokenType::EQUAL_EQUAL: return make_bool_literal(expr->line, l_val == r_val);
                case TokenType::BANG_EQUAL: return make_bool_literal(expr->line, l_val != r_val);
                default: break;
            }
        }
        // 4. Double String Operations (Concatenation)
        else if (std::holds_alternative<LiteralStringExpr>(left->node) &&
                 std::holds_alternative<LiteralStringExpr>(right->node)) {
            const std::string& l_val = std::get<LiteralStringExpr>(left->node).value;
            const std::string& r_val = std::get<LiteralStringExpr>(right->node).value;

            if (bin.op == TokenType::PLUS) {
                return make_string_literal(expr->line, l_val + r_val);
            }
        }
    }
    else if (std::holds_alternative<UnaryExpr>(expr->node)) {
        auto& un = std::get<UnaryExpr>(expr->node);
        Expr* operand = un.operand.get();

        if (!operand) return expr;

        if (std::holds_alternative<LiteralIntExpr>(operand->node)) {
            int64_t val = std::get<LiteralIntExpr>(operand->node).value;
            if (un.op == TokenType::MINUS) return make_int_literal(expr->line, -val);
            if (un.op == TokenType::TILDE) return make_int_literal(expr->line, ~val);
        }
        else if (std::holds_alternative<LiteralFloatExpr>(operand->node)) {
            double val = std::get<LiteralFloatExpr>(operand->node).value;
            if (un.op == TokenType::MINUS) return make_float_literal(expr->line, -val);
        }
        else if (std::holds_alternative<LiteralBoolExpr>(operand->node)) {
            bool val = std::get<LiteralBoolExpr>(operand->node).value;
            if (un.op == TokenType::BANG) return make_bool_literal(expr->line, !val);
        }
    }
    else if (std::holds_alternative<TernaryExpr>(expr->node)) {
        auto& tern = std::get<TernaryExpr>(expr->node);
        Expr* cond = tern.condition.get();

        if (cond && std::holds_alternative<LiteralBoolExpr>(cond->node)) {
            bool val = std::get<LiteralBoolExpr>(cond->node).value;
            return val ? std::move(tern.then_expr) : std::move(tern.else_expr);
        }
    }
    else if (std::holds_alternative<CastExpr>(expr->node)) {
        auto& cast = std::get<CastExpr>(expr->node);
        Expr* op = cast.operand.get();

        if (op) {
            if (cast.target_type == DataType::INT) {
                if (std::holds_alternative<LiteralFloatExpr>(op->node)) {
                    return make_int_literal(expr->line, static_cast<int64_t>(std::get<LiteralFloatExpr>(op->node).value));
                }
                if (std::holds_alternative<LiteralBoolExpr>(op->node)) {
                    return make_int_literal(expr->line, std::get<LiteralBoolExpr>(op->node).value ? 1 : 0);
                }
            }
            else if (cast.target_type == DataType::FLOAT) {
                if (std::holds_alternative<LiteralIntExpr>(op->node)) {
                    return make_float_literal(expr->line, static_cast<double>(std::get<LiteralIntExpr>(op->node).value));
                }
            }
            else if (cast.target_type == DataType::BOOL) {
                if (std::holds_alternative<LiteralIntExpr>(op->node)) {
                    return make_bool_literal(expr->line, std::get<LiteralIntExpr>(op->node).value != 0);
                }
            }
        }
    }

    return expr;
}
