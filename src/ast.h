#pragma once

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include "token.h"

enum class DataType {
    INT, FLOAT, BOOL, STRING, VOID, UNKNOWN,
    INT_PTR, FLOAT_PTR, BOOL_PTR, STRING_PTR,
    INT_ARRAY, FLOAT_ARRAY, BOOL_ARRAY, STRING_ARRAY,
    NULL_TYPE,

    // C++ Templates
    VECTOR_INT,
    VECTOR_FLOAT,
    VECTOR_BOOL,
    VECTOR_STRING,
    VECTOR_PAIR_INT_INT,
    VECTOR_VECTOR_INT,
    VECTOR_VECTOR_PAIR_INT_INT,
    PAIR_INT_INT,
    QUEUE_INT,
    QUEUE_PAIR_INT_INT,
    STACK_INT
};

std::string data_type_to_string(DataType type);

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct LiteralIntExpr { int64_t value; };
struct LiteralFloatExpr { double value; };
struct LiteralBoolExpr { bool value; };
struct LiteralStringExpr { std::string value; };
struct BinaryExpr { TokenType op; ExprPtr left; ExprPtr right; };
struct UnaryExpr { TokenType op; ExprPtr operand; };
struct VarRefExpr { std::string name; };
struct AssignExpr { ExprPtr lhs; ExprPtr value; };
struct CallExpr { std::string callee; std::vector<ExprPtr> args; };
struct ArrayIndexExpr { ExprPtr array; ExprPtr index; };
struct NewArrayExpr { DataType element_type; ExprPtr size; };
struct LenExpr { ExprPtr array; };
struct CastExpr { DataType target_type; ExprPtr operand; };
struct TernaryExpr { ExprPtr condition; ExprPtr then_expr; ExprPtr else_expr; };
struct CompoundAssignExpr { TokenType op; ExprPtr target; ExprPtr value; };
struct PreUnaryExpr { TokenType op; ExprPtr operand; };
struct PostUnaryExpr { TokenType op; ExprPtr operand; };
struct NullExpr {};
struct SizeofExpr { DataType target_type; };
struct TypeofExpr { ExprPtr operand; };

struct ConstructorExpr { DataType type; std::vector<ExprPtr> args; };
struct MethodCallExpr { ExprPtr object; std::string method; std::vector<ExprPtr> args; };
struct MemberAccessExpr { ExprPtr object; std::string member; };

using ExprNode = std::variant<LiteralIntExpr, LiteralFloatExpr, LiteralBoolExpr, LiteralStringExpr,
                              BinaryExpr, UnaryExpr, VarRefExpr, AssignExpr, CallExpr,
                              ArrayIndexExpr, NewArrayExpr, LenExpr,
                              CastExpr, TernaryExpr, CompoundAssignExpr,
                              PreUnaryExpr, PostUnaryExpr, NullExpr,
                              SizeofExpr, TypeofExpr,
                              ConstructorExpr, MethodCallExpr, MemberAccessExpr>;

struct Expr {
    ExprNode node;
    int line;
    DataType resolved_type = DataType::UNKNOWN;
};

struct Param {
    std::string name;
    DataType type;
};

struct VarDeclStmt { std::string name; DataType type; ExprPtr initializer; };
struct BlockStmt { std::vector<StmtPtr> statements; };
struct IfStmt { ExprPtr condition; StmtPtr then_branch; StmtPtr else_branch; };
struct WhileStmt { ExprPtr condition; StmtPtr body; };
struct ForStmt { StmtPtr init; ExprPtr condition; ExprPtr update; StmtPtr body; };
struct BreakStmt {};
struct ContinueStmt {};
struct ReturnStmt { ExprPtr value; };
struct PrintStmt { ExprPtr value; };
struct FnDeclStmt { std::string name; std::vector<Param> params; DataType return_type; StmtPtr body; };
struct ExprStmt { ExprPtr expr; };

struct StructDeclStmt {
    std::string name;
    std::vector<std::pair<std::string, DataType>> fields;
};

struct EnumDeclStmt {
    std::string name;
    std::vector<std::pair<std::string, int64_t>> values;
};

struct SwitchStmt {
    ExprPtr value;
    std::vector<std::pair<ExprPtr, StmtPtr>> cases;
    StmtPtr default_case;
};

struct DoWhileStmt { StmtPtr body; ExprPtr condition; };

struct ConstDeclStmt { std::string name; DataType type; ExprPtr initializer; };

struct CoutStmt { std::vector<ExprPtr> exprs; };
struct CinStmt { std::vector<ExprPtr> targets; };

using StmtNode = std::variant<VarDeclStmt, BlockStmt, IfStmt, WhileStmt, ForStmt,
                              BreakStmt, ContinueStmt, ReturnStmt, PrintStmt,
                              FnDeclStmt, ExprStmt,
                              StructDeclStmt, EnumDeclStmt, SwitchStmt,
                              DoWhileStmt, ConstDeclStmt,
                              CoutStmt, CinStmt>;

struct Stmt {
    StmtNode node;
    int line;
};

ExprPtr make_int_literal(int line, int64_t value);
ExprPtr make_float_literal(int line, double value);
ExprPtr make_bool_literal(int line, bool value);
ExprPtr make_string_literal(int line, const std::string& value);
ExprPtr make_binary(int line, TokenType op, ExprPtr left, ExprPtr right);
ExprPtr make_unary(int line, TokenType op, ExprPtr operand);
ExprPtr make_var_ref(int line, const std::string& name);
ExprPtr make_assign(int line, ExprPtr lhs, ExprPtr value);
ExprPtr make_call(int line, const std::string& name, std::vector<ExprPtr> args);
ExprPtr make_array_index(int line, ExprPtr array, ExprPtr index);
ExprPtr make_new_array(int line, DataType element_type, ExprPtr size);
ExprPtr make_len(int line, ExprPtr array);
ExprPtr make_cast(int line, DataType target_type, ExprPtr operand);
ExprPtr make_ternary(int line, ExprPtr condition, ExprPtr then_expr, ExprPtr else_expr);
ExprPtr make_compound_assign(int line, TokenType op, ExprPtr target, ExprPtr value);
ExprPtr make_pre_unary(int line, TokenType op, ExprPtr operand);
ExprPtr make_post_unary(int line, TokenType op, ExprPtr operand);
ExprPtr make_null(int line);
ExprPtr make_sizeof(int line, DataType target_type);
ExprPtr make_typeof(int line, ExprPtr operand);
ExprPtr make_constructor(int line, DataType type, std::vector<ExprPtr> args);
ExprPtr make_method_call(int line, ExprPtr object, const std::string& method, std::vector<ExprPtr> args);
ExprPtr make_member_access(int line, ExprPtr object, const std::string& member);

StmtPtr make_var_decl(int line, const std::string& name, DataType type, ExprPtr init);
StmtPtr make_block(int line, std::vector<StmtPtr> stmts);
StmtPtr make_if(int line, ExprPtr cond, StmtPtr then_br, StmtPtr else_br);
StmtPtr make_while(int line, ExprPtr cond, StmtPtr body);
StmtPtr make_for(int line, StmtPtr init, ExprPtr cond, ExprPtr update, StmtPtr body);
StmtPtr make_break(int line);
StmtPtr make_continue(int line);
StmtPtr make_return(int line, ExprPtr value);
StmtPtr make_print(int line, ExprPtr value);
StmtPtr make_fn_decl(int line, const std::string& name, std::vector<Param> params, DataType ret_type, StmtPtr body);
StmtPtr make_expr_stmt(int line, ExprPtr expr);
StmtPtr make_struct_decl(int line, const std::string& name, std::vector<std::pair<std::string, DataType>> fields);
StmtPtr make_enum_decl(int line, const std::string& name, std::vector<std::pair<std::string, int64_t>> values);
StmtPtr make_switch(int line, ExprPtr value, std::vector<std::pair<ExprPtr, StmtPtr>> cases, StmtPtr default_case);
StmtPtr make_do_while(int line, StmtPtr body, ExprPtr condition);
StmtPtr make_const_decl(int line, const std::string& name, DataType type, ExprPtr init);
StmtPtr make_cout(int line, std::vector<ExprPtr> exprs);
StmtPtr make_cin(int line, std::vector<ExprPtr> targets);

std::string expr_to_string(const Expr& expr, int indent = 0);
std::string stmt_to_string(const Stmt& stmt, int indent = 0);
