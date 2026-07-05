#include "transpiler.h"
#include <variant>
#include <sstream>
#include <type_traits>

static std::string transpile_type(DataType type) {
    switch (type) {
        case DataType::INT: return "int64_t";
        case DataType::FLOAT: return "double";
        case DataType::BOOL: return "bool";
        case DataType::STRING: return "std::string";
        case DataType::VOID: return "void";
        case DataType::INT_PTR: return "int64_t*";
        case DataType::FLOAT_PTR: return "double*";
        case DataType::BOOL_PTR: return "bool*";
        case DataType::STRING_PTR: return "std::string*";
        case DataType::INT_ARRAY: return "std::vector<int64_t>";
        case DataType::FLOAT_ARRAY: return "std::vector<double>";
        case DataType::BOOL_ARRAY: return "std::vector<bool>";
        case DataType::STRING_ARRAY: return "std::vector<std::string>";
        case DataType::VECTOR_INT: return "std::vector<int64_t>";
        case DataType::VECTOR_FLOAT: return "std::vector<double>";
        case DataType::VECTOR_BOOL: return "std::vector<bool>";
        case DataType::VECTOR_STRING: return "std::vector<std::string>";
        case DataType::VECTOR_PAIR_INT_INT: return "std::vector<std::pair<int64_t, int64_t>>";
        case DataType::VECTOR_VECTOR_INT: return "std::vector<std::vector<int64_t>>";
        case DataType::VECTOR_VECTOR_PAIR_INT_INT: return "std::vector<std::vector<std::pair<int64_t, int64_t>>>";
        case DataType::PAIR_INT_INT: return "std::pair<int64_t, int64_t>";
        case DataType::QUEUE_INT: return "std::queue<int64_t>";
        case DataType::QUEUE_PAIR_INT_INT: return "std::queue<std::pair<int64_t, int64_t>>";
        case DataType::STACK_INT: return "std::stack<int64_t>";
        default: return "void";
    }
}

static std::string escape_string(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            default:   result += c; break;
        }
    }
    return result;
}

static std::string transpile_expr(const Expr& expr) {
    return std::visit([&](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, LiteralIntExpr>) {
            return std::to_string(arg.value);
        } else if constexpr (std::is_same_v<T, LiteralFloatExpr>) {
            return std::to_string(arg.value);
        } else if constexpr (std::is_same_v<T, LiteralBoolExpr>) {
            return arg.value ? "true" : "false";
        } else if constexpr (std::is_same_v<T, LiteralStringExpr>) {
            return "\"" + escape_string(arg.value) + "\"";
        } else if constexpr (std::is_same_v<T, VarRefExpr>) {
            return arg.name;
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
            return transpile_expr(*arg.lhs) + " = " + transpile_expr(*arg.value);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
            std::string op;
            switch (arg.op) {
                case TokenType::PLUS: op = "+"; break;
                case TokenType::MINUS: op = "-"; break;
                case TokenType::STAR: op = "*"; break;
                case TokenType::SLASH: op = "/"; break;
                case TokenType::PERCENT: op = "%"; break;
                case TokenType::EQUAL_EQUAL: op = "=="; break;
                case TokenType::BANG_EQUAL: op = "!="; break;
                case TokenType::LESS: op = "<"; break;
                case TokenType::LESS_EQUAL: op = "<="; break;
                case TokenType::GREATER: op = ">"; break;
                case TokenType::GREATER_EQUAL: op = ">="; break;
                case TokenType::AMP_AMP: op = "&&"; break;
                case TokenType::PIPE_PIPE: op = "||"; break;
                case TokenType::PIPE: op = "|"; break;
                case TokenType::CARET: op = "^"; break;
                case TokenType::AMP: op = "&"; break;
                case TokenType::LESS_LESS: op = "<<"; break;
                case TokenType::GREATER_GREATER: op = ">>"; break;
                default: op = "+"; break;
            }
            return "(" + transpile_expr(*arg.left) + " " + op + " " + transpile_expr(*arg.right) + ")";
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
            std::string op;
            switch (arg.op) {
                case TokenType::MINUS: op = "-"; break;
                case TokenType::BANG: op = "!"; break;
                case TokenType::AMP: op = "&"; break;
                case TokenType::STAR: op = "*"; break;
                case TokenType::TILDE: op = "~"; break;
                default: op = "-"; break;
            }
            return "(" + op + transpile_expr(*arg.operand) + ")";
        } else if constexpr (std::is_same_v<T, CallExpr>) {
            std::string res = arg.callee + "(";
            for (size_t i = 0; i < arg.args.size(); ++i) {
                if (i > 0) res += ", ";
                res += transpile_expr(*arg.args[i]);
            }
            res += ")";
            return res;
        } else if constexpr (std::is_same_v<T, ArrayIndexExpr>) {
            return transpile_expr(*arg.array) + "[" + transpile_expr(*arg.index) + "]";
        } else if constexpr (std::is_same_v<T, NewArrayExpr>) {
            std::string cpp_elem;
            switch (arg.element_type) {
                case DataType::INT: cpp_elem = "int64_t"; break;
                case DataType::FLOAT: cpp_elem = "double"; break;
                case DataType::BOOL: cpp_elem = "bool"; break;
                case DataType::STRING: cpp_elem = "std::string"; break;
                default: cpp_elem = "int64_t"; break;
            }
            return "std::vector<" + cpp_elem + ">(" + transpile_expr(*arg.size) + ")";
        } else if constexpr (std::is_same_v<T, LenExpr>) {
            return "static_cast<int64_t>(" + transpile_expr(*arg.array) + ".size())";
        } else if constexpr (std::is_same_v<T, CastExpr>) {
            return "static_cast<" + transpile_type(arg.target_type) + ">(" + transpile_expr(*arg.operand) + ")";
        } else if constexpr (std::is_same_v<T, TernaryExpr>) {
            return "(" + transpile_expr(*arg.condition) + " ? " + transpile_expr(*arg.then_expr) + " : " + transpile_expr(*arg.else_expr) + ")";
        } else if constexpr (std::is_same_v<T, CompoundAssignExpr>) {
            std::string op;
            switch (arg.op) {
                case TokenType::PLUS_EQUAL: op = "+="; break;
                case TokenType::MINUS_EQUAL: op = "-="; break;
                case TokenType::STAR_EQUAL: op = "*="; break;
                case TokenType::SLASH_EQUAL: op = "/="; break;
                case TokenType::PERCENT_EQUAL: op = "%="; break;
                default: op = "+="; break;
            }
            return transpile_expr(*arg.target) + " " + op + " " + transpile_expr(*arg.value);
        } else if constexpr (std::is_same_v<T, PreUnaryExpr>) {
            std::string op = (arg.op == TokenType::PLUS_PLUS) ? "++" : "--";
            return "(" + op + transpile_expr(*arg.operand) + ")";
        } else if constexpr (std::is_same_v<T, PostUnaryExpr>) {
            std::string op = (arg.op == TokenType::PLUS_PLUS) ? "++" : "--";
            return "(" + transpile_expr(*arg.operand) + op + ")";
        } else if constexpr (std::is_same_v<T, NullExpr>) {
            return "nullptr";
        } else if constexpr (std::is_same_v<T, SizeofExpr>) {
            return "static_cast<int64_t>(sizeof(" + transpile_type(arg.target_type) + "))";
        } else if constexpr (std::is_same_v<T, TypeofExpr>) {
            return "\"" + data_type_to_string(arg.operand->resolved_type) + "\"";
        } else if constexpr (std::is_same_v<T, ConstructorExpr>) {
            std::string res = transpile_type(arg.type) + "(";
            for (size_t i = 0; i < arg.args.size(); ++i) {
                if (i > 0) res += ", ";
                res += transpile_expr(*arg.args[i]);
            }
            res += ")";
            return res;
        } else if constexpr (std::is_same_v<T, MethodCallExpr>) {
            std::string res = transpile_expr(*arg.object) + "." + arg.method + "(";
            for (size_t i = 0; i < arg.args.size(); ++i) {
                if (i > 0) res += ", ";
                res += transpile_expr(*arg.args[i]);
            }
            res += ")";
            return res;
        } else if constexpr (std::is_same_v<T, MemberAccessExpr>) {
            return transpile_expr(*arg.object) + "." + arg.member;
        }
        return "";
    }, expr.node);
}

static std::string transpile_stmt(const Stmt& stmt, int indent = 4) {
    std::string ind(indent, ' ');
    return std::visit([&](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, BlockStmt>) {
            std::string res = ind + "{\n";
            for (const auto& s : arg.statements) {
                res += transpile_stmt(*s, indent + 4);
            }
            res += ind + "}\n";
            return res;
        } else if constexpr (std::is_same_v<T, VarDeclStmt>) {
            return ind + transpile_type(arg.type) + " " + arg.name + " = " + transpile_expr(*arg.initializer) + ";\n";
        } else if constexpr (std::is_same_v<T, ConstDeclStmt>) {
            return ind + "const " + transpile_type(arg.type) + " " + arg.name + " = " + transpile_expr(*arg.initializer) + ";\n";
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            std::string res;
            if (std::holds_alternative<BlockStmt>(arg.then_branch->node)) {
                res = ind + "if (" + transpile_expr(*arg.condition) + ")\n" + transpile_stmt(*arg.then_branch, indent);
            } else {
                res = ind + "if (" + transpile_expr(*arg.condition) + ") {\n" + transpile_stmt(*arg.then_branch, indent + 4) + ind + "}\n";
            }
            if (arg.else_branch) {
                if (std::holds_alternative<BlockStmt>(arg.else_branch->node)) {
                    res += ind + "else\n" + transpile_stmt(*arg.else_branch, indent);
                } else {
                    res += ind + "else {\n" + transpile_stmt(*arg.else_branch, indent + 4) + ind + "}\n";
                }
            }
            return res;
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            if (std::holds_alternative<BlockStmt>(arg.body->node)) {
                return ind + "while (" + transpile_expr(*arg.condition) + ")\n" + transpile_stmt(*arg.body, indent);
            } else {
                return ind + "while (" + transpile_expr(*arg.condition) + ") {\n" + transpile_stmt(*arg.body, indent + 4) + ind + "}\n";
            }
        } else if constexpr (std::is_same_v<T, DoWhileStmt>) {
            std::string res = ind + "do {\n";
            res += transpile_stmt(*arg.body, indent + 4);
            res += ind + "} while (" + transpile_expr(*arg.condition) + ");\n";
            return res;
        } else if constexpr (std::is_same_v<T, ForStmt>) {
            std::string init_str = transpile_stmt(*arg.init, 0);
            if (!init_str.empty() && init_str.back() == '\n') init_str.pop_back();
            if (!init_str.empty() && init_str.back() == ';') init_str.pop_back();

            std::string cond_str = transpile_expr(*arg.condition);
            std::string update_str = transpile_expr(*arg.update);

            if (std::holds_alternative<BlockStmt>(arg.body->node)) {
                return ind + "for (" + init_str + "; " + cond_str + "; " + update_str + ")\n" + transpile_stmt(*arg.body, indent);
            } else {
                return ind + "for (" + init_str + "; " + cond_str + "; " + update_str + ") {\n" + transpile_stmt(*arg.body, indent + 4) + ind + "}\n";
            }
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            std::string res = ind + "switch (" + transpile_expr(*arg.value) + ") {\n";
            for (const auto& c : arg.cases) {
                res += ind + "    case " + transpile_expr(*c.first) + ":\n";
                res += transpile_stmt(*c.second, indent + 8);
                res += ind + "        break;\n";
            }
            if (arg.default_case) {
                res += ind + "    default:\n";
                res += transpile_stmt(*arg.default_case, indent + 8);
                res += ind + "        break;\n";
            }
            res += ind + "}\n";
            return res;
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            if (arg.value) {
                return ind + "return " + transpile_expr(*arg.value) + ";\n";
            } else {
                return ind + "return;\n";
            }
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
            return ind + "std::cout << " + transpile_expr(*arg.value) + " << std::endl;\n";
        } else if constexpr (std::is_same_v<T, CoutStmt>) {
            std::string res = ind + "std::cout";
            for (const auto& e : arg.exprs) {
                res += " << " + transpile_expr(*e);
            }
            res += ";\n";
            return res;
        } else if constexpr (std::is_same_v<T, CinStmt>) {
            std::string res = ind + "std::cin";
            for (const auto& t : arg.targets) {
                res += " >> " + transpile_expr(*t);
            }
            res += ";\n";
            return res;
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
            return ind + transpile_expr(*arg.expr) + ";\n";
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
            return ind + "break;\n";
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
            return ind + "continue;\n";
        } else if constexpr (std::is_same_v<T, StructDeclStmt>) {
            std::string res = ind + "struct " + arg.name + " {\n";
            for (const auto& field : arg.fields) {
                res += ind + "    " + transpile_type(field.second) + " " + field.first + ";\n";
            }
            res += ind + "};\n";
            return res;
        } else if constexpr (std::is_same_v<T, EnumDeclStmt>) {
            std::string res = ind + "enum class " + arg.name + " {\n";
            for (const auto& val : arg.values) {
                res += ind + "    " + val.first + " = " + std::to_string(val.second) + ",\n";
            }
            res += ind + "};\n";
            return res;
        } else if constexpr (std::is_same_v<T, FnDeclStmt>) {
            std::string res = ind + transpile_type(arg.return_type) + " ";
            // Special handling to rename 'main' to C++ standard main signature if needed,
            // but void main() is technically supported in C++ (or int main() is better).
            // Let's keep the name as is, but if it is main, we can ensure it returns int64_t/int
            std::string fn_name = arg.name;
            if (fn_name == "main") {
                res = ind + "int main(";
            } else {
                res += fn_name + "(";
            }

            for (size_t i = 0; i < arg.params.size(); ++i) {
                if (i > 0) res += ", ";
                res += transpile_type(arg.params[i].type) + " " + arg.params[i].name;
            }
            res += ") ";
            std::string body_str = transpile_stmt(*arg.body, indent);
            if (body_str.find_first_not_of(' ') != std::string::npos) {
                body_str = body_str.substr(body_str.find_first_not_of(' '));
            }

            // For main function, insert std::boolalpha so true/false print as strings, matching VM
            if (fn_name == "main") {
                std::string prefix = "{\n" + std::string(indent + 4, ' ') + "std::cout << std::boolalpha;\n";
                // remove the opening brace "{\n" of body_str and prepend our prefix
                if (body_str.front() == '{') {
                    body_str = body_str.substr(1);
                    // remove leading spaces/newlines if any
                    while (!body_str.empty() && (body_str.front() == '\n' || body_str.front() == '\r')) {
                        body_str = body_str.substr(1);
                    }
                    body_str = prefix + body_str;
                }
            }

            res += body_str;
            return res;
        }
        return "";
    }, stmt.node);
}

std::string CPlusPlusTranspiler::transpile(const std::vector<StmtPtr>& statements) {
    std::stringstream ss;
    ss << "#include <iostream>\n";
    ss << "#include <vector>\n";
    ss << "#include <string>\n";
    ss << "#include <cstdint>\n";
    ss << "#include <utility>\n";
    ss << "#include <queue>\n";
    ss << "#include <stack>\n";
    ss << "#include <cmath>\n";
    ss << "#include <algorithm>\n\n";

    ss << "// Helper operator for printing std::vector (MiniLang Dynamic Arrays)\n";
    ss << "template <typename T>\n";
    ss << "std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {\n";
    ss << "    os << \"[\";\n";
    ss << "    for (size_t i = 0; i < vec.size(); ++i) {\n";
    ss << "        if (i > 0) os << \", \";\n";
    ss << "        os << vec[i];\n";
    ss << "    }\n";
    ss << "    os << \"]\";\n";
    ss << "    return os;\n";
    ss << "}\n\n";

    ss << "// Helper operator for printing std::pair\n";
    ss << "template <typename T1, typename T2>\n";
    ss << "std::ostream& operator<<(std::ostream& os, const std::pair<T1, T2>& p) {\n";
    ss << "    os << \"(\" << p.first << \", \" << p.second << \")\";\n";
    ss << "    return os;\n";
    ss << "}\n\n";

    // Forward declare functions
    for (const auto& stmt : statements) {
        if (auto fn = std::get_if<FnDeclStmt>(&stmt->node)) {
            if (fn->name != "main") {
                ss << transpile_type(fn->return_type) << " " << fn->name << "(";
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << transpile_type(fn->params[i].type) << " " << fn->params[i].name;
                }
                ss << ");\n";
            }
        }
    }
    ss << "\n";

    // Transpile all statements
    for (const auto& stmt : statements) {
        ss << transpile_stmt(*stmt, 0) << "\n";
    }

    return ss.str();
}
