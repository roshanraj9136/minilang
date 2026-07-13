#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include "ast.h"
#include "error.h"

struct Symbol {
    std::string name;
    DataType type;
    int depth;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer(ErrorReporter& errors, const std::string& source);
    void analyze(std::vector<StmtPtr>& statements);

private:
    ErrorReporter& errors_;
    const std::string& source_;
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
    std::unordered_map<std::string, std::vector<Param>> fn_signatures_;
    std::unordered_map<std::string, DataType> fn_return_types_;
    DataType current_return_type_ = DataType::VOID;
    int loop_depth_ = 0;

    void enter_scope();
    void exit_scope();
    void define(const std::string& name, DataType type, int line);
    std::optional<Symbol> lookup(const std::string& name);

    void analyze_stmt(Stmt& stmt);
    void analyze_fn_decl(FnDeclStmt& fn, int line);
    void analyze_block(BlockStmt& block);
    void analyze_var_decl(VarDeclStmt& decl, int line);
    void analyze_if(IfStmt& stmt, int line);
    void analyze_while(WhileStmt& stmt, int line);
    void analyze_for(ForStmt& stmt, int line);
    void analyze_return(ReturnStmt& stmt, int line);
    void analyze_print(PrintStmt& stmt, int line);
    void analyze_expr_stmt(ExprStmt& stmt);
    void analyze_struct_decl(StructDeclStmt& stmt, int line);
    void analyze_enum_decl(EnumDeclStmt& stmt, int line);
    void analyze_switch(SwitchStmt& stmt, int line);
    void analyze_do_while(DoWhileStmt& stmt, int line);
    void analyze_const_decl(ConstDeclStmt& stmt, int line);
    void analyze_cout(CoutStmt& stmt, int line);
    void analyze_cin(CinStmt& stmt, int line);

    DataType analyze_expr(Expr& expr);
    DataType analyze_binary(BinaryExpr& expr, Expr& parent);
    DataType analyze_unary(UnaryExpr& expr, Expr& parent);
    DataType analyze_call(CallExpr& expr, Expr& parent);
    DataType analyze_assign(AssignExpr& expr, Expr& parent);
    DataType analyze_array_index(ArrayIndexExpr& expr, Expr& parent);
    DataType analyze_new_array(NewArrayExpr& expr, Expr& parent);
    DataType analyze_len(LenExpr& expr, Expr& parent);
    DataType analyze_cast(CastExpr& expr, Expr& parent);
    DataType analyze_ternary(TernaryExpr& expr, Expr& parent);
    DataType analyze_compound_assign(CompoundAssignExpr& expr, Expr& parent);
    DataType analyze_pre_unary(PreUnaryExpr& expr, Expr& parent);
    DataType analyze_post_unary(PostUnaryExpr& expr, Expr& parent);
    DataType analyze_sizeof(SizeofExpr& expr, Expr& parent);
    DataType analyze_typeof(TypeofExpr& expr, Expr& parent);
    DataType analyze_constructor(ConstructorExpr& expr, Expr& parent);
    DataType analyze_method_call(MethodCallExpr& expr, Expr& parent);
    DataType analyze_member_access(MemberAccessExpr& expr, Expr& parent);

    std::unordered_map<std::string, std::vector<std::pair<std::string, DataType>>> struct_defs_;
    std::unordered_map<std::string, std::unordered_map<std::string, int64_t>> enum_defs_;
    std::unordered_set<std::string> constants_;
};
