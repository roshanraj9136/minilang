#include "ast.h"
#include <sstream>

std::string data_type_to_string(DataType type) {
    switch (type) {
        case DataType::INT: return "int";
        case DataType::FLOAT: return "float";
        case DataType::BOOL: return "bool";
        case DataType::STRING: return "string";
        case DataType::VOID: return "void";
        case DataType::INT_PTR: return "*int";
        case DataType::FLOAT_PTR: return "*float";
        case DataType::BOOL_PTR: return "*bool";
        case DataType::STRING_PTR: return "*string";
        case DataType::INT_ARRAY: return "[int]";
        case DataType::FLOAT_ARRAY: return "[float]";
        case DataType::BOOL_ARRAY: return "[bool]";
        case DataType::STRING_ARRAY: return "[string]";
        case DataType::NULL_TYPE: return "null";
        case DataType::VECTOR_INT: return "vector<int>";
        case DataType::VECTOR_FLOAT: return "vector<float>";
        case DataType::VECTOR_BOOL: return "vector<bool>";
        case DataType::VECTOR_STRING: return "vector<string>";
        case DataType::VECTOR_PAIR_INT_INT: return "vector<pair<int, int>>";
        case DataType::VECTOR_VECTOR_INT: return "vector<vector<int>>";
        case DataType::VECTOR_VECTOR_PAIR_INT_INT: return "vector<vector<pair<int, int>>>";
        case DataType::PAIR_INT_INT: return "pair<int, int>";
        case DataType::QUEUE_INT: return "queue<int>";
        case DataType::QUEUE_PAIR_INT_INT: return "queue<pair<int, int>>";
        case DataType::STACK_INT: return "stack<int>";
        default: return "unknown";
    }
}

ExprPtr make_int_literal(int line, int64_t value) {
    return std::make_unique<Expr>(Expr{LiteralIntExpr{value}, line, DataType::INT});
}

ExprPtr make_float_literal(int line, double value) {
    return std::make_unique<Expr>(Expr{LiteralFloatExpr{value}, line, DataType::FLOAT});
}

ExprPtr make_bool_literal(int line, bool value) {
    return std::make_unique<Expr>(Expr{LiteralBoolExpr{value}, line, DataType::BOOL});
}

ExprPtr make_string_literal(int line, const std::string& value) {
    return std::make_unique<Expr>(Expr{LiteralStringExpr{value}, line, DataType::STRING});
}

ExprPtr make_binary(int line, TokenType op, ExprPtr left, ExprPtr right) {
    return std::make_unique<Expr>(Expr{BinaryExpr{op, std::move(left), std::move(right)}, line, DataType::UNKNOWN});
}

ExprPtr make_unary(int line, TokenType op, ExprPtr operand) {
    return std::make_unique<Expr>(Expr{UnaryExpr{op, std::move(operand)}, line, DataType::UNKNOWN});
}

ExprPtr make_var_ref(int line, const std::string& name) {
    return std::make_unique<Expr>(Expr{VarRefExpr{name}, line, DataType::UNKNOWN});
}

ExprPtr make_assign(int line, ExprPtr lhs, ExprPtr value) {
    return std::make_unique<Expr>(Expr{AssignExpr{std::move(lhs), std::move(value)}, line, DataType::UNKNOWN});
}

ExprPtr make_call(int line, const std::string& name, std::vector<ExprPtr> args) {
    return std::make_unique<Expr>(Expr{CallExpr{name, std::move(args)}, line, DataType::UNKNOWN});
}

ExprPtr make_array_index(int line, ExprPtr array, ExprPtr index) {
    return std::make_unique<Expr>(Expr{ArrayIndexExpr{std::move(array), std::move(index)}, line, DataType::UNKNOWN});
}

ExprPtr make_new_array(int line, DataType element_type, ExprPtr size) {
    DataType array_type = DataType::UNKNOWN;
    if (element_type == DataType::INT) array_type = DataType::INT_ARRAY;
    else if (element_type == DataType::FLOAT) array_type = DataType::FLOAT_ARRAY;
    else if (element_type == DataType::BOOL) array_type = DataType::BOOL_ARRAY;
    else if (element_type == DataType::STRING) array_type = DataType::STRING_ARRAY;
    return std::make_unique<Expr>(Expr{NewArrayExpr{element_type, std::move(size)}, line, array_type});
}

ExprPtr make_len(int line, ExprPtr array) {
    return std::make_unique<Expr>(Expr{LenExpr{std::move(array)}, line, DataType::INT});
}

ExprPtr make_cast(int line, DataType target_type, ExprPtr operand) {
    return std::make_unique<Expr>(Expr{CastExpr{target_type, std::move(operand)}, line, target_type});
}

ExprPtr make_ternary(int line, ExprPtr condition, ExprPtr then_expr, ExprPtr else_expr) {
    return std::make_unique<Expr>(Expr{TernaryExpr{std::move(condition), std::move(then_expr), std::move(else_expr)}, line, DataType::UNKNOWN});
}

ExprPtr make_compound_assign(int line, TokenType op, ExprPtr target, ExprPtr value) {
    return std::make_unique<Expr>(Expr{CompoundAssignExpr{op, std::move(target), std::move(value)}, line, DataType::UNKNOWN});
}

ExprPtr make_pre_unary(int line, TokenType op, ExprPtr operand) {
    return std::make_unique<Expr>(Expr{PreUnaryExpr{op, std::move(operand)}, line, DataType::UNKNOWN});
}

ExprPtr make_post_unary(int line, TokenType op, ExprPtr operand) {
    return std::make_unique<Expr>(Expr{PostUnaryExpr{op, std::move(operand)}, line, DataType::UNKNOWN});
}

ExprPtr make_null(int line) {
    return std::make_unique<Expr>(Expr{NullExpr{}, line, DataType::NULL_TYPE});
}

ExprPtr make_sizeof(int line, DataType target_type) {
    return std::make_unique<Expr>(Expr{SizeofExpr{target_type}, line, DataType::INT});
}

ExprPtr make_typeof(int line, ExprPtr operand) {
    return std::make_unique<Expr>(Expr{TypeofExpr{std::move(operand)}, line, DataType::STRING});
}

ExprPtr make_constructor(int line, DataType type, std::vector<ExprPtr> args) {
    return std::make_unique<Expr>(Expr{ConstructorExpr{type, std::move(args)}, line, type});
}

ExprPtr make_method_call(int line, ExprPtr object, const std::string& method, std::vector<ExprPtr> args) {
    return std::make_unique<Expr>(Expr{MethodCallExpr{std::move(object), method, std::move(args)}, line, DataType::UNKNOWN});
}

ExprPtr make_member_access(int line, ExprPtr object, const std::string& member) {
    return std::make_unique<Expr>(Expr{MemberAccessExpr{std::move(object), member}, line, DataType::UNKNOWN});
}

StmtPtr make_var_decl(int line, const std::string& name, DataType type, ExprPtr init) {
    return std::make_unique<Stmt>(Stmt{VarDeclStmt{name, type, std::move(init)}, line});
}

StmtPtr make_block(int line, std::vector<StmtPtr> stmts) {
    return std::make_unique<Stmt>(Stmt{BlockStmt{std::move(stmts)}, line});
}

StmtPtr make_if(int line, ExprPtr cond, StmtPtr then_br, StmtPtr else_br) {
    return std::make_unique<Stmt>(Stmt{IfStmt{std::move(cond), std::move(then_br), std::move(else_br)}, line});
}

StmtPtr make_while(int line, ExprPtr cond, StmtPtr body) {
    return std::make_unique<Stmt>(Stmt{WhileStmt{std::move(cond), std::move(body)}, line});
}

StmtPtr make_for(int line, StmtPtr init, ExprPtr cond, ExprPtr update, StmtPtr body) {
    return std::make_unique<Stmt>(Stmt{ForStmt{std::move(init), std::move(cond), std::move(update), std::move(body)}, line});
}

StmtPtr make_break(int line) {
    return std::make_unique<Stmt>(Stmt{BreakStmt{}, line});
}

StmtPtr make_continue(int line) {
    return std::make_unique<Stmt>(Stmt{ContinueStmt{}, line});
}

StmtPtr make_return(int line, ExprPtr value) {
    return std::make_unique<Stmt>(Stmt{ReturnStmt{std::move(value)}, line});
}

StmtPtr make_print(int line, ExprPtr value) {
    return std::make_unique<Stmt>(Stmt{PrintStmt{std::move(value)}, line});
}

StmtPtr make_fn_decl(int line, const std::string& name, std::vector<Param> params, DataType ret_type, StmtPtr body) {
    return std::make_unique<Stmt>(Stmt{FnDeclStmt{name, std::move(params), ret_type, std::move(body)}, line});
}

StmtPtr make_expr_stmt(int line, ExprPtr expr) {
    return std::make_unique<Stmt>(Stmt{ExprStmt{std::move(expr)}, line});
}

StmtPtr make_struct_decl(int line, const std::string& name, std::vector<std::pair<std::string, DataType>> fields) {
    return std::make_unique<Stmt>(Stmt{StructDeclStmt{name, std::move(fields)}, line});
}

StmtPtr make_enum_decl(int line, const std::string& name, std::vector<std::pair<std::string, int64_t>> values) {
    return std::make_unique<Stmt>(Stmt{EnumDeclStmt{name, std::move(values)}, line});
}

StmtPtr make_switch(int line, ExprPtr value, std::vector<std::pair<ExprPtr, StmtPtr>> cases, StmtPtr default_case) {
    return std::make_unique<Stmt>(Stmt{SwitchStmt{std::move(value), std::move(cases), std::move(default_case)}, line});
}

StmtPtr make_do_while(int line, StmtPtr body, ExprPtr condition) {
    return std::make_unique<Stmt>(Stmt{DoWhileStmt{std::move(body), std::move(condition)}, line});
}

StmtPtr make_const_decl(int line, const std::string& name, DataType type, ExprPtr init) {
    return std::make_unique<Stmt>(Stmt{ConstDeclStmt{name, type, std::move(init)}, line});
}

StmtPtr make_cout(int line, std::vector<ExprPtr> exprs) {
    return std::make_unique<Stmt>(Stmt{CoutStmt{std::move(exprs)}, line});
}

StmtPtr make_cin(int line, std::vector<ExprPtr> targets) {
    return std::make_unique<Stmt>(Stmt{CinStmt{std::move(targets)}, line});
}

std::string expr_to_string(const Expr& expr, int indent) {
    std::string ind(indent, ' ');
    return std::visit([&](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, LiteralIntExpr>) {
            return ind + "LiteralInt(" + std::to_string(arg.value) + ")";
        } else if constexpr (std::is_same_v<T, LiteralFloatExpr>) {
            return ind + "LiteralFloat(" + std::to_string(arg.value) + ")";
        } else if constexpr (std::is_same_v<T, LiteralBoolExpr>) {
            return ind + "LiteralBool(" + (arg.value ? "true" : "false") + ")";
        } else if constexpr (std::is_same_v<T, LiteralStringExpr>) {
            return ind + "LiteralString(\"" + arg.value + "\")";
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
            return ind + "BinaryExpr(\n" +
                   ind + "  op: " + token_type_to_string(arg.op) + "\n" +
                   expr_to_string(*arg.left, indent + 2) + "\n" +
                   expr_to_string(*arg.right, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
            return ind + "UnaryExpr(\n" +
                   ind + "  op: " + token_type_to_string(arg.op) + "\n" +
                   expr_to_string(*arg.operand, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, VarRefExpr>) {
            return ind + "VarRef(" + arg.name + ")";
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
            return ind + "AssignExpr(\n" +
                   expr_to_string(*arg.lhs, indent + 2) + "\n" +
                   expr_to_string(*arg.value, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, CallExpr>) {
            std::string res = ind + "CallExpr(\n" + ind + "  callee: " + arg.callee + "\n";
            for (auto& a : arg.args) {
                res += expr_to_string(*a, indent + 2) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, ArrayIndexExpr>) {
            return ind + "ArrayIndex(\n" +
                   expr_to_string(*arg.array, indent + 2) + "\n" +
                   expr_to_string(*arg.index, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, NewArrayExpr>) {
            return ind + "NewArray(\n" +
                   ind + "  type: " + data_type_to_string(arg.element_type) + "\n" +
                   expr_to_string(*arg.size, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, LenExpr>) {
            return ind + "Len(\n" +
                   expr_to_string(*arg.array, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, CastExpr>) {
            return ind + "CastExpr(\n" +
                   ind + "  target_type: " + data_type_to_string(arg.target_type) + "\n" +
                   expr_to_string(*arg.operand, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, TernaryExpr>) {
            return ind + "TernaryExpr(\n" +
                   expr_to_string(*arg.condition, indent + 2) + "\n" +
                   expr_to_string(*arg.then_expr, indent + 2) + "\n" +
                   expr_to_string(*arg.else_expr, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, CompoundAssignExpr>) {
            return ind + "CompoundAssignExpr(\n" +
                   ind + "  op: " + token_type_to_string(arg.op) + "\n" +
                   expr_to_string(*arg.target, indent + 2) + "\n" +
                   expr_to_string(*arg.value, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, PreUnaryExpr>) {
            return ind + "PreUnaryExpr(\n" +
                   ind + "  op: " + token_type_to_string(arg.op) + "\n" +
                   expr_to_string(*arg.operand, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, PostUnaryExpr>) {
            return ind + "PostUnaryExpr(\n" +
                   ind + "  op: " + token_type_to_string(arg.op) + "\n" +
                   expr_to_string(*arg.operand, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, NullExpr>) {
            return ind + "NullExpr";
        } else if constexpr (std::is_same_v<T, SizeofExpr>) {
            return ind + "SizeofExpr(" + data_type_to_string(arg.target_type) + ")";
        } else if constexpr (std::is_same_v<T, TypeofExpr>) {
            return ind + "TypeofExpr(\n" +
                   expr_to_string(*arg.operand, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, ConstructorExpr>) {
            std::string res = ind + "ConstructorExpr(\n" + ind + "  type: " + data_type_to_string(arg.type) + "\n";
            for (auto& a : arg.args) {
                res += expr_to_string(*a, indent + 2) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, MethodCallExpr>) {
            std::string res = ind + "MethodCallExpr(\n" +
                              expr_to_string(*arg.object, indent + 2) + "\n" +
                              ind + "  method: " + arg.method + "\n";
            for (auto& a : arg.args) {
                res += expr_to_string(*a, indent + 2) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, MemberAccessExpr>) {
            return ind + "MemberAccessExpr(\n" +
                   expr_to_string(*arg.object, indent + 2) + "\n" +
                   ind + "  member: " + arg.member + "\n" +
                   ind + ")";
        }
        return ind + "UnknownExpr";
    }, expr.node);
}

std::string stmt_to_string(const Stmt& stmt, int indent) {
    std::string ind(indent, ' ');
    return std::visit([&](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, VarDeclStmt>) {
            return ind + "VarDecl(\n" +
                   ind + "  name: " + arg.name + "\n" +
                   ind + "  type: " + data_type_to_string(arg.type) + "\n" +
                   expr_to_string(*arg.initializer, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
            std::string res = ind + "Block(\n";
            for (auto& s : arg.statements) {
                res += stmt_to_string(*s, indent + 2) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            std::string res = ind + "If(\n" +
                              expr_to_string(*arg.condition, indent + 2) + "\n" +
                              stmt_to_string(*arg.then_branch, indent + 2) + "\n";
            if (arg.else_branch) {
                res += stmt_to_string(*arg.else_branch, indent + 2) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            return ind + "While(\n" +
                   expr_to_string(*arg.condition, indent + 2) + "\n" +
                   stmt_to_string(*arg.body, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, ForStmt>) {
            return ind + "For(\n" +
                   (arg.init ? stmt_to_string(*arg.init, indent + 2) + "\n" : ind + "  init: nullptr\n") +
                   (arg.condition ? expr_to_string(*arg.condition, indent + 2) + "\n" : ind + "  cond: nullptr\n") +
                   (arg.update ? expr_to_string(*arg.update, indent + 2) + "\n" : ind + "  update: nullptr\n") +
                   stmt_to_string(*arg.body, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
            return ind + "Break";
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
            return ind + "Continue";
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            return ind + "Return(\n" +
                   (arg.value ? expr_to_string(*arg.value, indent + 2) + "\n" : "") +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
            return ind + "Print(\n" +
                   expr_to_string(*arg.value, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, FnDeclStmt>) {
            std::string res = ind + "FnDecl(\n" +
                              ind + "  name: " + arg.name + "\n" +
                              ind + "  ret_type: " + data_type_to_string(arg.return_type) + "\n" +
                              ind + "  params:\n";
            for (auto& p : arg.params) {
                res += ind + "    " + p.name + ": " + data_type_to_string(p.type) + "\n";
            }
            res += stmt_to_string(*arg.body, indent + 2) + "\n" +
                   ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
            return ind + "ExprStmt(\n" +
                   expr_to_string(*arg.expr, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, StructDeclStmt>) {
            std::string res = ind + "StructDecl(\n" + ind + "  name: " + arg.name + "\n";
            for (auto& f : arg.fields) {
                res += ind + "  field: " + f.first + " : " + data_type_to_string(f.second) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, EnumDeclStmt>) {
            std::string res = ind + "EnumDecl(\n" + ind + "  name: " + arg.name + "\n";
            for (auto& v : arg.values) {
                res += ind + "  val: " + v.first + " = " + std::to_string(v.second) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            std::string res = ind + "Switch(\n" + expr_to_string(*arg.value, indent + 2) + "\n";
            for (auto& c : arg.cases) {
                res += ind + "  Case:\n" + expr_to_string(*c.first, indent + 4) + "\n" + stmt_to_string(*c.second, indent + 4) + "\n";
            }
            if (arg.default_case) {
                res += ind + "  Default:\n" + stmt_to_string(*arg.default_case, indent + 4) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, DoWhileStmt>) {
            return ind + "DoWhile(\n" +
                   stmt_to_string(*arg.body, indent + 2) + "\n" +
                   expr_to_string(*arg.condition, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, ConstDeclStmt>) {
            return ind + "ConstDecl(\n" +
                   ind + "  name: " + arg.name + "\n" +
                   ind + "  type: " + data_type_to_string(arg.type) + "\n" +
                   expr_to_string(*arg.initializer, indent + 2) + "\n" +
                   ind + ")";
        } else if constexpr (std::is_same_v<T, CoutStmt>) {
            std::string res = ind + "CoutStmt(\n";
            for (auto& e : arg.exprs) {
                res += expr_to_string(*e, indent + 2) + "\n";
            }
            res += ind + ")";
            return res;
        } else if constexpr (std::is_same_v<T, CinStmt>) {
            std::string res = ind + "CinStmt(\n";
            for (auto& t : arg.targets) {
                res += expr_to_string(*t, indent + 2) + "\n";
            }
            res += ind + ")";
            return res;
        }
        return ind + "UnknownStmt";
    }, stmt.node);
}
