#include "optimizer.h"
#include <variant>
#include <cmath>
#include <unordered_map>
#include <cstdint>

namespace {

class OptimizerInstance {
public:
    std::vector<std::unordered_map<std::string, ExprPtr>> scopes;

    void push_scope() {
        scopes.emplace_back();
    }

    void pop_scope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    void add_constant(const std::string& name, const Expr* literal_expr) {
        if (scopes.empty()) return;
        if (!literal_expr) return;
        scopes.back()[name] = clone_literal(literal_expr);
    }

    ExprPtr lookup_constant(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end() && found->second) {
                return clone_literal(found->second.get());
            }
        }
        return nullptr;
    }

    ExprPtr clone_literal(const Expr* expr) {
        if (!expr) return nullptr;
        ExprPtr new_expr = std::visit([&](auto&& arg) -> ExprPtr {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, LiteralIntExpr>) {
                return make_int_literal(expr->line, arg.value);
            } else if constexpr (std::is_same_v<T, LiteralFloatExpr>) {
                return make_float_literal(expr->line, arg.value);
            } else if constexpr (std::is_same_v<T, LiteralBoolExpr>) {
                return make_bool_literal(expr->line, arg.value);
            } else if constexpr (std::is_same_v<T, LiteralStringExpr>) {
                return make_string_literal(expr->line, arg.value);
            } else {
                return ExprPtr(nullptr);
            }
        }, expr->node);
        if (new_expr) {
            new_expr->resolved_type = expr->resolved_type;
        }
        return new_expr;
    }

    void optimize_stmt(Stmt& stmt) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, VarDeclStmt>) {
                arg.initializer = optimize_expr(std::move(arg.initializer));
            } else if constexpr (std::is_same_v<T, ConstDeclStmt>) {
                arg.initializer = optimize_expr(std::move(arg.initializer));
                if (arg.initializer) {
                    ExprNode& node = arg.initializer->node;
                    if (std::holds_alternative<LiteralIntExpr>(node) ||
                        std::holds_alternative<LiteralFloatExpr>(node) ||
                        std::holds_alternative<LiteralBoolExpr>(node) ||
                        std::holds_alternative<LiteralStringExpr>(node)) {
                        add_constant(arg.name, arg.initializer.get());
                    }
                }
            } else if constexpr (std::is_same_v<T, BlockStmt>) {
                push_scope();
                std::vector<StmtPtr> optimized_stmts;
                for (auto& s : arg.statements) {
                    if (!s) continue;
                    optimize_stmt(*s);
                    
                    // Skip empty block statements
                    if (auto block = std::get_if<BlockStmt>(&s->node)) {
                        if (block->statements.empty()) {
                            continue;
                        }
                    }
                    
                    optimized_stmts.push_back(std::move(s));
                    
                    // If we see a terminating statement (Return, Break, Continue),
                    // subsequent statements in this block are unreachable!
                    if (std::holds_alternative<ReturnStmt>(optimized_stmts.back()->node) ||
                        std::holds_alternative<BreakStmt>(optimized_stmts.back()->node) ||
                        std::holds_alternative<ContinueStmt>(optimized_stmts.back()->node)) {
                        break;
                    }
                }
                arg.statements = std::move(optimized_stmts);
                pop_scope();
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
                push_scope();
                if (arg.init) optimize_stmt(*arg.init);
                arg.condition = optimize_expr(std::move(arg.condition));
                arg.update = optimize_expr(std::move(arg.update));
                if (arg.body) optimize_stmt(*arg.body);
                pop_scope();
            } else if constexpr (std::is_same_v<T, ReturnStmt>) {
                if (arg.value) arg.value = optimize_expr(std::move(arg.value));
            } else if constexpr (std::is_same_v<T, PrintStmt>) {
                arg.value = optimize_expr(std::move(arg.value));
            } else if constexpr (std::is_same_v<T, FnDeclStmt>) {
                auto old_scopes = std::move(scopes);
                scopes.clear();
                push_scope();
                if (arg.body) optimize_stmt(*arg.body);
                pop_scope();
                scopes = std::move(old_scopes);
            } else if constexpr (std::is_same_v<T, ExprStmt>) {
                arg.expr = optimize_expr(std::move(arg.expr));
            } else if constexpr (std::is_same_v<T, SwitchStmt>) {
                arg.value = optimize_expr(std::move(arg.value));
                push_scope();
                for (auto& c : arg.cases) {
                    c.first = optimize_expr(std::move(c.first));
                    if (c.second) optimize_stmt(*c.second);
                }
                if (arg.default_case) optimize_stmt(*arg.default_case);
                pop_scope();
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

        // Dead Code Elimination post-passes (safe from variant self-mutation bugs)
        if (auto if_stmt = std::get_if<IfStmt>(&stmt.node)) {
            if (if_stmt->condition && std::holds_alternative<LiteralBoolExpr>(if_stmt->condition->node)) {
                bool cond_val = std::get<LiteralBoolExpr>(if_stmt->condition->node).value;
                StmtPtr branch = std::move(cond_val ? if_stmt->then_branch : if_stmt->else_branch);
                if (branch) {
                    stmt = std::move(*branch);
                } else {
                    stmt.node = BlockStmt{std::vector<StmtPtr>{}};
                }
            }
        } else if (auto while_stmt = std::get_if<WhileStmt>(&stmt.node)) {
            if (while_stmt->condition && std::holds_alternative<LiteralBoolExpr>(while_stmt->condition->node)) {
                bool cond_val = std::get<LiteralBoolExpr>(while_stmt->condition->node).value;
                if (!cond_val) {
                    stmt.node = BlockStmt{std::vector<StmtPtr>{}};
                }
            }
        }
    }

    ExprPtr optimize_expr(ExprPtr expr) {
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

        // Constant propagation: replace VarRefExpr with stored constant if available
        if (std::holds_alternative<VarRefExpr>(expr->node)) {
            auto& ref = std::get<VarRefExpr>(expr->node);
            ExprPtr constant = lookup_constant(ref.name);
            if (constant) {
                return constant;
            }
        }

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
                    case TokenType::PLUS:
                        if ((r_val > 0 && l_val > INT64_MAX - r_val) ||
                            (r_val < 0 && l_val < INT64_MIN - r_val)) {
                            break;
                        }
                        return make_int_literal(expr->line, l_val + r_val);
                    case TokenType::MINUS:
                        if ((r_val < 0 && l_val > INT64_MAX + r_val) ||
                            (r_val > 0 && l_val < INT64_MIN + r_val)) {
                            break;
                        }
                        return make_int_literal(expr->line, l_val - r_val);
                    case TokenType::STAR:
                        if (l_val > 0) {
                            if (r_val > 0) {
                                if (l_val > INT64_MAX / r_val) break;
                            } else {
                                if (r_val < INT64_MIN / l_val) break;
                            }
                        } else {
                            if (r_val > 0) {
                                if (l_val < INT64_MIN / r_val) break;
                            } else {
                                if (l_val != 0 && r_val < INT64_MAX / l_val) break;
                            }
                        }
                        return make_int_literal(expr->line, l_val * r_val);
                    case TokenType::SLASH:
                        if (r_val != 0) {
                            // Check for overflow case: INT64_MIN / -1
                            if (l_val == INT64_MIN && r_val == -1) break;
                            return make_int_literal(expr->line, l_val / r_val);
                        }
                        break;
                    case TokenType::PERCENT:
                        if (r_val != 0) {
                            if (l_val == INT64_MIN && r_val == -1) break;
                            return make_int_literal(expr->line, l_val % r_val);
                        }
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
                    case TokenType::LESS_LESS:
                        if (r_val >= 0 && r_val < 64) {
                            return make_int_literal(expr->line, l_val << r_val);
                        }
                        break;
                    case TokenType::GREATER_GREATER:
                        if (r_val >= 0 && r_val < 64) {
                            return make_int_literal(expr->line, l_val >> r_val);
                        }
                        break;
                    default: break;
                }
            }
            // 2. Double Float Operations
            else if (std::holds_alternative<LiteralFloatExpr>(left->node) &&
                     std::holds_alternative<LiteralFloatExpr>(right->node)) {
                double l_val = std::get<LiteralFloatExpr>(left->node).value;
                double r_val = std::get<LiteralFloatExpr>(right->node).value;

                double res = 0.0;
                bool do_fold = false;

                switch (bin.op) {
                    case TokenType::PLUS: res = l_val + r_val; do_fold = true; break;
                    case TokenType::MINUS: res = l_val - r_val; do_fold = true; break;
                    case TokenType::STAR: res = l_val * r_val; do_fold = true; break;
                    case TokenType::SLASH:
                        if (r_val != 0.0) {
                            res = l_val / r_val;
                            do_fold = true;
                        }
                        break;
                    case TokenType::EQUAL_EQUAL: return make_bool_literal(expr->line, l_val == r_val);
                    case TokenType::BANG_EQUAL: return make_bool_literal(expr->line, l_val != r_val);
                    case TokenType::LESS: return make_bool_literal(expr->line, l_val < r_val);
                    case TokenType::LESS_EQUAL: return make_bool_literal(expr->line, l_val <= r_val);
                    case TokenType::GREATER: return make_bool_literal(expr->line, l_val > r_val);
                    case TokenType::GREATER_EQUAL: return make_bool_literal(expr->line, l_val >= r_val);
                    default: break;
                }

                if (do_fold && std::isfinite(res)) {
                    return make_float_literal(expr->line, res);
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
                if (un.op == TokenType::MINUS) {
                    // Guard negation overflow: -INT64_MIN
                    if (val == INT64_MIN) return expr;
                    return make_int_literal(expr->line, -val);
                }
                if (un.op == TokenType::TILDE) return make_int_literal(expr->line, ~val);
            }
            else if (std::holds_alternative<LiteralFloatExpr>(operand->node)) {
                double val = std::get<LiteralFloatExpr>(operand->node).value;
                if (un.op == TokenType::MINUS) {
                    double res = -val;
                    if (std::isfinite(res)) {
                        return make_float_literal(expr->line, res);
                    }
                }
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
                        double f_val = std::get<LiteralFloatExpr>(op->node).value;
                        if (std::isfinite(f_val)) {
                            return make_int_literal(expr->line, static_cast<int64_t>(f_val));
                        }
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
};

} // namespace

void Optimizer::optimize(std::vector<StmtPtr>& statements) {
    OptimizerInstance instance;
    instance.push_scope();
    for (auto& stmt : statements) {
        if (stmt) {
            instance.optimize_stmt(*stmt);
        }
    }
    instance.pop_scope();
}
