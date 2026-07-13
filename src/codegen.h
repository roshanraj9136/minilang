#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "ast.h"
#include "opcode.h"
#include "error.h"

class CodeGenerator {
public:
    CodeGenerator(ErrorReporter& errors, const std::string& source);
    CompiledProgram generate(const std::vector<StmtPtr>& statements);

private:
    ErrorReporter& errors_;
    const std::string& source_;
    CompiledProgram program_;
    Chunk* current_chunk_ = nullptr;
    std::unordered_map<std::string, int> fn_indices_;
    std::unordered_map<std::string, std::vector<DataType>> fn_param_types_;
    DataType current_fn_return_type_ = DataType::VOID;

    struct Local {
        std::string name;
        int slot;
    };
    std::vector<Local> locals_;
    int slot_count_ = 0;

    struct LoopContext {
        std::vector<int> break_jumps;
        std::vector<int> continue_jumps;
    };
    std::vector<LoopContext> loop_stack_;

    void generate_stmt(const Stmt& stmt);
    void generate_fn_decl(const FnDeclStmt& fn, int line);
    void generate_block(const BlockStmt& block);
    void generate_var_decl(const VarDeclStmt& decl, int line);
    void generate_if(const IfStmt& stmt, int line);
    void generate_while(const WhileStmt& stmt, int line);
    void generate_for(const ForStmt& stmt, int line);
    void generate_return(const ReturnStmt& stmt, int line);
    void generate_print(const PrintStmt& stmt, int line);
    void generate_expr_stmt(const ExprStmt& stmt);
    void generate_break(int line);
    void generate_continue(int line);
    void generate_switch(const SwitchStmt& stmt, int line);
    void generate_do_while(const DoWhileStmt& stmt, int line);
    void generate_const_decl(const ConstDeclStmt& decl, int line);
    void generate_cout(const CoutStmt& stmt, int line);
    void generate_cin(const CinStmt& stmt, int line);

    void generate_expr(const Expr& expr);
    void generate_binary(const BinaryExpr& expr, int line);
    void generate_unary(const UnaryExpr& expr, int line);
    void generate_call(const CallExpr& expr, int line);

    int resolve_local(const std::string& name);
    int resolve_global(const std::string& name);
    std::unordered_map<std::string, int> global_indices_;
};
