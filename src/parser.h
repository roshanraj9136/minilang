#pragma once

#include <vector>
#include "ast.h"
#include "error.h"

class Parser {
public:
    Parser(const std::vector<Token>& tokens, ErrorReporter& errors, const std::string& source);
    std::vector<StmtPtr> parse();

private:
    const std::vector<Token>& tokens_;
    int current_;
    ErrorReporter& errors_;
    const std::string& source_;
    bool allow_shift_operators_ = true;
    bool split_greater_greater_ = false;
    bool allow_split_greater_greater_ = false;

    const Token& peek() const;
    const Token& previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool check_type_start() const;
    bool is_function_decl() const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    bool is_at_end() const;
    void synchronize();
    void error(const Token& token, const std::string& message);

    StmtPtr parse_fn_decl();
    StmtPtr parse_block();
    StmtPtr parse_statement();
    StmtPtr parse_var_decl();
    StmtPtr parse_if_stmt();
    StmtPtr parse_while_stmt();
    StmtPtr parse_for_stmt();
    StmtPtr parse_return_stmt();
    StmtPtr parse_print_stmt();
    StmtPtr parse_struct_decl();
    StmtPtr parse_enum_decl();
    StmtPtr parse_switch_stmt();
    StmtPtr parse_do_while_stmt();
    StmtPtr parse_const_decl();
    StmtPtr parse_cout_stmt();
    StmtPtr parse_cin_stmt();
    DataType parse_type();

    ExprPtr parse_expression();
    ExprPtr parse_ternary();
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_bitwise_or();
    ExprPtr parse_bitwise_xor();
    ExprPtr parse_bitwise_and();
    ExprPtr parse_equality();
    ExprPtr parse_comparison();
    ExprPtr parse_shift();
    ExprPtr parse_term();
    ExprPtr parse_factor();
    ExprPtr parse_unary();
    ExprPtr parse_call();
    ExprPtr parse_primary();
};
