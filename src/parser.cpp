#include "parser.h"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens, ErrorReporter& errors, const std::string& source)
    : tokens_(tokens), current_(0), errors_(errors), source_(source) {}

std::vector<StmtPtr> Parser::parse() {
    std::vector<StmtPtr> decls;
    while (!is_at_end()) {
        try {
            if (check(TokenType::KW_STRUCT)) {
                decls.push_back(parse_struct_decl());
            } else if (check(TokenType::KW_ENUM)) {
                decls.push_back(parse_enum_decl());
            } else if (check(TokenType::KW_FN)) {
                decls.push_back(parse_fn_decl());
            } else if (check_type_start()) {
                if (is_function_decl()) {
                    // Parse C++ style function: Type name(params) { body }
                    DataType ret_type = parse_type();
                    Token name = consume(TokenType::IDENTIFIER, "Expected function name");
                    consume(TokenType::LPAREN, "Expected '(' after function name");

                    std::vector<Param> params;
                    if (!check(TokenType::RPAREN)) {
                        do {
                            DataType param_type = parse_type();
                            Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                            params.push_back(Param{param_name.lexeme, param_type});
                        } while (match(TokenType::COMMA));
                    }
                    consume(TokenType::RPAREN, "Expected ')' after parameters");

                    StmtPtr body = parse_block();
                    decls.push_back(make_fn_decl(name.line, name.lexeme, params, ret_type, std::move(body)));
                } else {
                    // Parse global variable declaration: Type name = initializer;
                    int line = peek().line;
                    DataType type = parse_type();
                    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
                    consume(TokenType::EQUAL, "Expected '=' in variable declaration");
                    ExprPtr init = parse_expression();
                    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
                    decls.push_back(make_var_decl(line, name.lexeme, type, std::move(init)));
                }
            } else {
                error(peek(), "Expected function, struct, or enum declaration");
                advance(); // consume to prevent infinite loop
            }
        } catch (const std::runtime_error&) {
            synchronize();
        }
    }
    return decls;
}

const Token& Parser::peek() const {
    if (split_greater_greater_) {
        const Token& real = tokens_[current_];
        mock_greater_.line = real.line;
        mock_greater_.column = real.column;
        return mock_greater_;
    }
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::TOKEN_EOF;
}

Token Parser::advance() {
    if (split_greater_greater_) {
        split_greater_greater_ = false;
        const Token& real = tokens_[current_];
        Token mock_greater{TokenType::GREATER, ">", real.line, real.column};
        return mock_greater;
    }

    if (allow_split_greater_greater_ && !is_at_end() && tokens_[current_].type == TokenType::GREATER_GREATER) {
        split_greater_greater_ = true;
        const Token& real = tokens_[current_];
        current_++;
        Token mock_greater{TokenType::GREATER, ">", real.line, real.column};
        return mock_greater;
    }

    if (!is_at_end()) current_++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    if (allow_split_greater_greater_ && type == TokenType::GREATER && peek().type == TokenType::GREATER_GREATER) {
        return true;
    }
    return peek().type == type;
}

bool Parser::check_type_start() const {
    if (check(TokenType::KW_INT) || check(TokenType::KW_FLOAT) || 
        check(TokenType::KW_BOOL) || check(TokenType::KW_STRING) || 
        check(TokenType::KW_VOID)) {
        return true;
    }
    if (check(TokenType::STAR)) {
        if (current_ + 1 >= static_cast<int>(tokens_.size())) return false;
        TokenType next = tokens_[current_ + 1].type;
        return next == TokenType::KW_INT || next == TokenType::KW_FLOAT ||
               next == TokenType::KW_BOOL || next == TokenType::KW_STRING ||
               next == TokenType::KW_VOID || next == TokenType::STAR ||
               next == TokenType::LBRACKET;
    }
    if (check(TokenType::LBRACKET)) {
        if (current_ + 1 >= static_cast<int>(tokens_.size())) return false;
        TokenType next = tokens_[current_ + 1].type;
        return next == TokenType::KW_INT || next == TokenType::KW_FLOAT ||
               next == TokenType::KW_BOOL || next == TokenType::KW_STRING ||
               next == TokenType::KW_VOID || next == TokenType::STAR ||
               next == TokenType::LBRACKET;
    }
    if (check(TokenType::IDENTIFIER)) {
        std::string lex = peek().lexeme;
        if (lex == "vector" || lex == "std::vector" ||
            lex == "pair" || lex == "std::pair" ||
            lex == "queue" || lex == "std::queue" ||
            lex == "stack" || lex == "std::stack") {
            return true;
        }
    }
    return false;
}

bool Parser::is_function_decl() const {
    int temp = current_;
    while (temp < static_cast<int>(tokens_.size()) && tokens_[temp].type == TokenType::STAR) {
        temp++;
    }
    while (temp < static_cast<int>(tokens_.size()) && tokens_[temp].type == TokenType::LBRACKET) {
        temp++;
        if (temp < static_cast<int>(tokens_.size()) && tokens_[temp].type == TokenType::RBRACKET) {
            temp++;
        } else {
            return false;
        }
    }
    if (temp < static_cast<int>(tokens_.size()) && 
        (tokens_[temp].type == TokenType::KW_INT || tokens_[temp].type == TokenType::KW_FLOAT ||
         tokens_[temp].type == TokenType::KW_BOOL || tokens_[temp].type == TokenType::KW_STRING ||
         tokens_[temp].type == TokenType::KW_VOID || tokens_[temp].type == TokenType::IDENTIFIER)) {
        temp++;
    } else {
        return false;
    }
    if (temp < static_cast<int>(tokens_.size()) && tokens_[temp].type == TokenType::IDENTIFIER) {
        temp++;
    } else {
        return false;
    }
    if (temp < static_cast<int>(tokens_.size()) && tokens_[temp].type == TokenType::LPAREN) {
        return true;
    }
    return false;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    error(peek(), message);
    throw std::runtime_error(message);
}

void Parser::error(const Token& token, const std::string& message) {
    if (token.type == TokenType::TOKEN_EOF) {
        errors_.report_error(source_, token.line, token.column, message + " at end of file");
    } else {
        errors_.report_error(source_, token.line, token.column, message + " at '" + token.lexeme + "'");
    }
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) return;
        switch (peek().type) {
            case TokenType::KW_FN:
            case TokenType::KW_LET:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_FOR:
            case TokenType::KW_RETURN:
            case TokenType::KW_PRINT:
            case TokenType::KW_STRUCT:
            case TokenType::KW_ENUM:
            case TokenType::KW_SWITCH:
            case TokenType::KW_DO:
            case TokenType::KW_CONST:
                return;
            default:
                advance();
        }
    }
}

StmtPtr Parser::parse_fn_decl() {
    consume(TokenType::KW_FN, "Expected 'fn' keyword");
    Token name = consume(TokenType::IDENTIFIER, "Expected function name");
    consume(TokenType::LPAREN, "Expected '(' after function name");

    std::vector<Param> params;
    if (!check(TokenType::RPAREN)) {
        do {
            Token param_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
            consume(TokenType::COLON, "Expected ':' after parameter name");
            DataType param_type = parse_type();
            params.push_back(Param{param_name.lexeme, param_type});
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    DataType return_type = DataType::VOID;
    if (match(TokenType::ARROW)) {
        return_type = parse_type();
    }

    StmtPtr body = parse_block();
    return make_fn_decl(name.line, name.lexeme, params, return_type, std::move(body));
}

StmtPtr Parser::parse_block() {
    Token open = consume(TokenType::LBRACE, "Expected '{' to start block");
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RBRACE) && !is_at_end()) {
        statements.push_back(parse_statement());
    }
    consume(TokenType::RBRACE, "Expected '}' to end block");
    return make_block(open.line, std::move(statements));
}

StmtPtr Parser::parse_statement() {
    if (check(TokenType::LBRACE)) return parse_block();
    if (check(TokenType::KW_LET)) return parse_var_decl();
    if (check(TokenType::KW_CONST)) return parse_const_decl();
    if (check(TokenType::KW_IF)) return parse_if_stmt();
    if (check(TokenType::KW_WHILE)) return parse_while_stmt();
    if (check(TokenType::KW_FOR)) return parse_for_stmt();
    if (check(TokenType::KW_DO)) return parse_do_while_stmt();
    if (check(TokenType::KW_SWITCH)) return parse_switch_stmt();
    if (check(TokenType::KW_RETURN)) return parse_return_stmt();
    if (check(TokenType::KW_PRINT)) return parse_print_stmt();
    if (match(TokenType::KW_BREAK)) {
        int line = previous().line;
        consume(TokenType::SEMICOLON, "Expected ';' after 'break'");
        return make_break(line);
    }
    if (match(TokenType::KW_CONTINUE)) {
        int line = previous().line;
        consume(TokenType::SEMICOLON, "Expected ';' after 'continue'");
        return make_continue(line);
    }

    if (check(TokenType::IDENTIFIER)) {
        std::string lex = peek().lexeme;
        if (lex == "cout" || lex == "std::cout") {
            return parse_cout_stmt();
        }
        if (lex == "cin" || lex == "std::cin") {
            return parse_cin_stmt();
        }
    }

    // Check for C++ style variable declaration: type name = init; or type name;
    if (check_type_start()) {
        int line = peek().line;
        DataType type = parse_type();
        Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
        ExprPtr init = nullptr;
        if (match(TokenType::EQUAL)) {
            init = parse_expression();
        } else if (match(TokenType::LPAREN)) {
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after constructor arguments");
            init = make_constructor(line, type, std::move(args));
        } else {
            // Default initializer
            if (type == DataType::INT) {
                init = make_int_literal(line, 0);
            } else if (type == DataType::FLOAT) {
                init = make_float_literal(line, 0.0);
            } else if (type == DataType::BOOL) {
                init = make_bool_literal(line, false);
            } else if (type == DataType::STRING) {
                init = make_string_literal(line, "");
            } else if (type == DataType::INT_PTR || type == DataType::FLOAT_PTR || type == DataType::BOOL_PTR || type == DataType::STRING_PTR) {
                init = make_null(line);
            } else if (type == DataType::VECTOR_INT || type == DataType::VECTOR_FLOAT || type == DataType::VECTOR_BOOL || type == DataType::VECTOR_STRING ||
                       type == DataType::VECTOR_PAIR_INT_INT || type == DataType::VECTOR_VECTOR_INT || type == DataType::VECTOR_VECTOR_PAIR_INT_INT ||
                       type == DataType::PAIR_INT_INT || type == DataType::QUEUE_INT || type == DataType::QUEUE_PAIR_INT_INT || type == DataType::STACK_INT) {
                init = make_constructor(line, type, {});
            } else {
                init = make_int_literal(line, 0);
            }
        }
        consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
        return make_var_decl(line, name.lexeme, type, std::move(init));
    }

    int line = peek().line;
    ExprPtr expr = parse_expression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression");
    return make_expr_stmt(line, std::move(expr));
}

StmtPtr Parser::parse_var_decl() {
    Token let_tok = consume(TokenType::KW_LET, "Expected 'let'");
    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
    consume(TokenType::COLON, "Expected ':' after variable name");
    DataType type = parse_type();
    consume(TokenType::EQUAL, "Expected '=' in variable declaration");
    ExprPtr init = parse_expression();
    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    return make_var_decl(let_tok.line, name.lexeme, type, std::move(init));
}

StmtPtr Parser::parse_if_stmt() {
    Token if_tok = consume(TokenType::KW_IF, "Expected 'if'");
    consume(TokenType::LPAREN, "Expected '(' after 'if'");
    ExprPtr cond = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after condition");
    StmtPtr then_branch = parse_block();
    StmtPtr else_branch = nullptr;
    if (match(TokenType::KW_ELSE)) {
        if (check(TokenType::KW_IF)) {
            else_branch = parse_if_stmt();
        } else {
            else_branch = parse_block();
        }
    }
    return make_if(if_tok.line, std::move(cond), std::move(then_branch), std::move(else_branch));
}

StmtPtr Parser::parse_while_stmt() {
    Token while_tok = consume(TokenType::KW_WHILE, "Expected 'while'");
    consume(TokenType::LPAREN, "Expected '(' after 'while'");
    ExprPtr cond = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after condition");
    StmtPtr body = parse_block();
    return make_while(while_tok.line, std::move(cond), std::move(body));
}

StmtPtr Parser::parse_for_stmt() {
    Token for_tok = consume(TokenType::KW_FOR, "Expected 'for'");
    consume(TokenType::LPAREN, "Expected '(' after 'for'");

    StmtPtr init = nullptr;
    if (check(TokenType::KW_LET)) {
        init = parse_var_decl();
    } else if (check_type_start()) {
        // Support C++ style variable declaration inside for loop initializer
        int line = peek().line;
        DataType type = parse_type();
        Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
        consume(TokenType::EQUAL, "Expected '=' in variable declaration");
        ExprPtr init_expr = parse_expression();
        consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
        init = make_var_decl(line, name.lexeme, type, std::move(init_expr));
    } else {
        int line = peek().line;
        ExprPtr expr = parse_expression();
        consume(TokenType::SEMICOLON, "Expected ';' after initializer expression");
        init = make_expr_stmt(line, std::move(expr));
    }

    ExprPtr cond = parse_expression();
    consume(TokenType::SEMICOLON, "Expected ';' after loop condition");

    ExprPtr update = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after update expression");

    StmtPtr body = parse_block();
    return make_for(for_tok.line, std::move(init), std::move(cond), std::move(update), std::move(body));
}

StmtPtr Parser::parse_return_stmt() {
    Token ret_tok = consume(TokenType::KW_RETURN, "Expected 'return'");
    ExprPtr val = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        val = parse_expression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after return value");
    return make_return(ret_tok.line, std::move(val));
}

StmtPtr Parser::parse_print_stmt() {
    Token print_tok = consume(TokenType::KW_PRINT, "Expected 'print'");
    consume(TokenType::LPAREN, "Expected '(' after 'print'");
    ExprPtr val = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after print argument");
    consume(TokenType::SEMICOLON, "Expected ';' after print call");
    return make_print(print_tok.line, std::move(val));
}

StmtPtr Parser::parse_struct_decl() {
    Token struct_tok = consume(TokenType::KW_STRUCT, "Expected 'struct'");
    Token name = consume(TokenType::IDENTIFIER, "Expected struct name");
    consume(TokenType::LBRACE, "Expected '{' to start struct body");
    
    std::vector<std::pair<std::string, DataType>> fields;
    while (!check(TokenType::RBRACE) && !is_at_end()) {
        DataType field_type = parse_type();
        Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
        consume(TokenType::SEMICOLON, "Expected ';' after field declaration");
        fields.push_back({field_name.lexeme, field_type});
    }
    consume(TokenType::RBRACE, "Expected '}' to end struct body");
    match(TokenType::SEMICOLON); // optional trailing semicolon for struct
    return make_struct_decl(struct_tok.line, name.lexeme, fields);
}

StmtPtr Parser::parse_enum_decl() {
    Token enum_tok = consume(TokenType::KW_ENUM, "Expected 'enum'");
    Token name = consume(TokenType::IDENTIFIER, "Expected enum name");
    consume(TokenType::LBRACE, "Expected '{' to start enum body");
    
    std::vector<std::pair<std::string, int64_t>> values;
    int64_t current_val = 0;
    if (!check(TokenType::RBRACE)) {
        do {
            Token member_name = consume(TokenType::IDENTIFIER, "Expected enum member identifier");
            if (match(TokenType::EQUAL)) {
                Token val_tok = consume(TokenType::INT_LITERAL, "Expected integer literal for enum value");
                current_val = std::stoll(val_tok.lexeme);
            }
            values.push_back({member_name.lexeme, current_val});
            current_val++;
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RBRACE, "Expected '}' to end enum body");
    match(TokenType::SEMICOLON); // optional trailing semicolon for enum
    return make_enum_decl(enum_tok.line, name.lexeme, values);
}

StmtPtr Parser::parse_switch_stmt() {
    Token switch_tok = consume(TokenType::KW_SWITCH, "Expected 'switch'");
    consume(TokenType::LPAREN, "Expected '(' after 'switch'");
    ExprPtr value = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after switch expression");
    consume(TokenType::LBRACE, "Expected '{' to start switch body");
    
    std::vector<std::pair<ExprPtr, StmtPtr>> cases;
    StmtPtr default_case = nullptr;
    
    while (!check(TokenType::RBRACE) && !is_at_end()) {
        if (match(TokenType::KW_CASE)) {
            ExprPtr case_val = parse_expression();
            consume(TokenType::COLON, "Expected ':' after case value");
            StmtPtr case_body = parse_statement();
            cases.push_back({std::move(case_val), std::move(case_body)});
        } else if (match(TokenType::KW_DEFAULT)) {
            consume(TokenType::COLON, "Expected ':' after default");
            default_case = parse_statement();
        } else {
            error(peek(), "Expected 'case' or 'default' inside switch");
            throw std::runtime_error("Invalid switch body");
        }
    }
    consume(TokenType::RBRACE, "Expected '}' to end switch body");
    return make_switch(switch_tok.line, std::move(value), std::move(cases), std::move(default_case));
}

StmtPtr Parser::parse_do_while_stmt() {
    Token do_tok = consume(TokenType::KW_DO, "Expected 'do'");
    StmtPtr body = parse_block();
    consume(TokenType::KW_WHILE, "Expected 'while' after do body");
    consume(TokenType::LPAREN, "Expected '(' after 'while'");
    ExprPtr cond = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after loop condition");
    consume(TokenType::SEMICOLON, "Expected ';' after do-while condition");
    return make_do_while(do_tok.line, std::move(body), std::move(cond));
}

StmtPtr Parser::parse_const_decl() {
    Token const_tok = consume(TokenType::KW_CONST, "Expected 'const'");
    DataType type = parse_type();
    Token name = consume(TokenType::IDENTIFIER, "Expected variable name");
    consume(TokenType::EQUAL, "Expected '=' in variable declaration");
    ExprPtr init = parse_expression();
    consume(TokenType::SEMICOLON, "Expected ';' after const declaration");
    return make_const_decl(const_tok.line, name.lexeme, type, std::move(init));
}

StmtPtr Parser::parse_cout_stmt() {
    Token cout_tok = advance();
    std::vector<ExprPtr> exprs;
    allow_shift_operators_ = false;
    while (match(TokenType::LESS_LESS)) {
        if (check(TokenType::IDENTIFIER) && (peek().lexeme == "endl" || peek().lexeme == "std::endl")) {
            Token endl_tok = advance();
            exprs.push_back(make_string_literal(endl_tok.line, "\n"));
        } else {
            exprs.push_back(parse_expression());
        }
    }
    allow_shift_operators_ = true;
    consume(TokenType::SEMICOLON, "Expected ';' after cout statement");
    return make_cout(cout_tok.line, std::move(exprs));
}

StmtPtr Parser::parse_cin_stmt() {
    Token cin_tok = advance();
    std::vector<ExprPtr> targets;
    allow_shift_operators_ = false;
    while (match(TokenType::GREATER_GREATER)) {
        targets.push_back(parse_expression());
    }
    allow_shift_operators_ = true;
    consume(TokenType::SEMICOLON, "Expected ';' after cin statement");
    return make_cin(cin_tok.line, std::move(targets));
}


struct SplitGuard {
    bool& flag;
    bool old_val;
    SplitGuard(bool& f, bool v) : flag(f), old_val(f) { flag = v; }
    ~SplitGuard() { flag = old_val; }
};

DataType Parser::parse_type() {
    SplitGuard guard(allow_split_greater_greater_, true);
    DataType type = DataType::UNKNOWN;
    if (match(TokenType::KW_INT)) type = DataType::INT;
    else if (match(TokenType::KW_FLOAT)) type = DataType::FLOAT;
    else if (match(TokenType::KW_BOOL)) type = DataType::BOOL;
    else if (match(TokenType::KW_STRING)) type = DataType::STRING;
    else if (match(TokenType::KW_VOID)) type = DataType::VOID;

    if (type != DataType::UNKNOWN) {
        if (match(TokenType::STAR)) {
            if (type == DataType::INT) type = DataType::INT_PTR;
            else if (type == DataType::FLOAT) type = DataType::FLOAT_PTR;
            else if (type == DataType::BOOL) type = DataType::BOOL_PTR;
            else if (type == DataType::STRING) type = DataType::STRING_PTR;
        }
        return type;
    }
    else if (match(TokenType::STAR)) {
        DataType base = parse_type();
        if (base == DataType::INT) return DataType::INT_PTR;
        if (base == DataType::FLOAT) return DataType::FLOAT_PTR;
        if (base == DataType::BOOL) return DataType::BOOL_PTR;
        if (base == DataType::STRING) return DataType::STRING_PTR;
        error(previous(), "Unsupported pointer base type");
        throw std::runtime_error("Unsupported pointer base type");
    }
    else if (match(TokenType::LBRACKET)) {
        DataType base = parse_type();
        consume(TokenType::RBRACKET, "Expected ']' after array element type");
        if (base == DataType::INT) return DataType::INT_ARRAY;
        if (base == DataType::FLOAT) return DataType::FLOAT_ARRAY;
        if (base == DataType::BOOL) return DataType::BOOL_ARRAY;
        if (base == DataType::STRING) return DataType::STRING_ARRAY;
        error(previous(), "Unsupported array element type");
        throw std::runtime_error("Unsupported array element type");
    }
    else if (check(TokenType::IDENTIFIER)) {
        std::string lex = peek().lexeme;
        if (lex == "vector" || lex == "std::vector") {
            advance();
            consume(TokenType::LESS, "Expected '<' after vector");
            DataType inner = parse_type();
            consume(TokenType::GREATER, "Expected '>' after vector type");
            if (inner == DataType::INT) return DataType::VECTOR_INT;
            if (inner == DataType::FLOAT) return DataType::VECTOR_FLOAT;
            if (inner == DataType::BOOL) return DataType::VECTOR_BOOL;
            if (inner == DataType::STRING) return DataType::VECTOR_STRING;
            if (inner == DataType::PAIR_INT_INT) return DataType::VECTOR_PAIR_INT_INT;
            if (inner == DataType::VECTOR_INT) return DataType::VECTOR_VECTOR_INT;
            if (inner == DataType::VECTOR_PAIR_INT_INT) return DataType::VECTOR_VECTOR_PAIR_INT_INT;
            error(previous(), "Unsupported vector element type");
            throw std::runtime_error("Unsupported vector element type");
        } else if (lex == "pair" || lex == "std::pair") {
            advance();
            consume(TokenType::LESS, "Expected '<' after pair");
            DataType first = parse_type();
            consume(TokenType::COMMA, "Expected ',' in pair");
            DataType second = parse_type();
            consume(TokenType::GREATER, "Expected '>' after pair types");
            if (first == DataType::INT && second == DataType::INT) {
                return DataType::PAIR_INT_INT;
            }
            error(previous(), "Unsupported pair template arguments");
            throw std::runtime_error("Unsupported pair template arguments");
        } else if (lex == "queue" || lex == "std::queue") {
            advance();
            consume(TokenType::LESS, "Expected '<' after queue");
            DataType inner = parse_type();
            consume(TokenType::GREATER, "Expected '>' after queue type");
            if (inner == DataType::INT) return DataType::QUEUE_INT;
            if (inner == DataType::PAIR_INT_INT) return DataType::QUEUE_PAIR_INT_INT;
            error(previous(), "Unsupported queue template arguments");
            throw std::runtime_error("Unsupported queue template arguments");
        } else if (lex == "stack" || lex == "std::stack") {
            advance();
            consume(TokenType::LESS, "Expected '<' after stack");
            DataType inner = parse_type();
            consume(TokenType::GREATER, "Expected '>' after stack type");
            if (inner == DataType::INT) return DataType::STACK_INT;
            error(previous(), "Unsupported stack template arguments");
            throw std::runtime_error("Unsupported stack template arguments");
        } else {
            error(peek(), "Expected a valid type (int, float, bool, string, pointer, or array)");
            throw std::runtime_error("Invalid type specifier");
        }
    }
    else {
        error(peek(), "Expected a valid type (int, float, bool, string, pointer, or array)");
        throw std::runtime_error("Invalid type specifier");
    }
    return type;
}

ExprPtr Parser::parse_expression() {
    ExprPtr expr = parse_ternary();
    if (match(TokenType::EQUAL)) {
        int line = previous().line;
        ExprPtr value = parse_expression();
        return make_assign(line, std::move(expr), std::move(value));
    }
    if (match(TokenType::PLUS_EQUAL) || match(TokenType::MINUS_EQUAL) ||
        match(TokenType::STAR_EQUAL) || match(TokenType::SLASH_EQUAL) ||
        match(TokenType::PERCENT_EQUAL)) {
        TokenType op = previous().type;
        int line = previous().line;
        ExprPtr value = parse_expression();
        return make_compound_assign(line, op, std::move(expr), std::move(value));
    }
    return expr;
}

ExprPtr Parser::parse_ternary() {
    ExprPtr expr = parse_or();
    if (match(TokenType::QUESTION)) {
        int line = previous().line;
        ExprPtr then_expr = parse_expression();
        consume(TokenType::COLON, "Expected ':' in ternary expression");
        ExprPtr else_expr = parse_expression();
        return make_ternary(line, std::move(expr), std::move(then_expr), std::move(else_expr));
    }
    return expr;
}

ExprPtr Parser::parse_or() {
    ExprPtr expr = parse_and();
    while (match(TokenType::PIPE_PIPE)) {
        TokenType op = previous().type;
        ExprPtr right = parse_and();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_and() {
    ExprPtr expr = parse_bitwise_or();
    while (match(TokenType::AMP_AMP)) {
        TokenType op = previous().type;
        ExprPtr right = parse_bitwise_or();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_bitwise_or() {
    ExprPtr expr = parse_bitwise_xor();
    while (match(TokenType::PIPE)) {
        TokenType op = previous().type;
        ExprPtr right = parse_bitwise_xor();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_bitwise_xor() {
    ExprPtr expr = parse_bitwise_and();
    while (match(TokenType::CARET)) {
        TokenType op = previous().type;
        ExprPtr right = parse_bitwise_and();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_bitwise_and() {
    ExprPtr expr = parse_equality();
    while (match(TokenType::AMP)) {
        TokenType op = previous().type;
        ExprPtr right = parse_equality();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_equality() {
    ExprPtr expr = parse_comparison();
    while (match(TokenType::EQUAL_EQUAL) || match(TokenType::BANG_EQUAL)) {
        TokenType op = previous().type;
        ExprPtr right = parse_comparison();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_comparison() {
    ExprPtr expr = parse_shift();
    while (match(TokenType::LESS) || match(TokenType::LESS_EQUAL) ||
           match(TokenType::GREATER) || match(TokenType::GREATER_EQUAL)) {
        TokenType op = previous().type;
        ExprPtr right = parse_shift();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_shift() {
    ExprPtr expr = parse_term();
    if (allow_shift_operators_) {
        while (match(TokenType::LESS_LESS) || match(TokenType::GREATER_GREATER)) {
            TokenType op = previous().type;
            ExprPtr right = parse_term();
            expr = make_binary(previous().line, op, std::move(expr), std::move(right));
        }
    }
    return expr;
}

ExprPtr Parser::parse_term() {
    ExprPtr expr = parse_factor();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op = previous().type;
        ExprPtr right = parse_factor();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_factor() {
    ExprPtr expr = parse_unary();
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT)) {
        TokenType op = previous().type;
        ExprPtr right = parse_unary();
        expr = make_binary(previous().line, op, std::move(expr), std::move(right));
    }
    return expr;
}

ExprPtr Parser::parse_unary() {
    if (match(TokenType::BANG) || match(TokenType::MINUS) || match(TokenType::AMP) || match(TokenType::STAR) || match(TokenType::TILDE)) {
        TokenType op = previous().type;
        ExprPtr operand = parse_unary();
        return make_unary(previous().line, op, std::move(operand));
    }
    if (match(TokenType::PLUS_PLUS) || match(TokenType::MINUS_MINUS)) {
        TokenType op = previous().type;
        ExprPtr operand = parse_unary();
        return make_pre_unary(previous().line, op, std::move(operand));
    }
    return parse_call();
}

ExprPtr Parser::parse_call() {
    ExprPtr expr = parse_primary();
    while (true) {
        if (match(TokenType::LPAREN)) {
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments");

            auto var_ref = std::get_if<VarRefExpr>(&expr->node);
            if (!var_ref) {
                error(previous(), "Can only call named functions");
                throw std::runtime_error("Invalid call expression");
            }
            expr = make_call(expr->line, var_ref->name, std::move(args));
        } else if (match(TokenType::LBRACKET)) {
            int line = previous().line;
            ExprPtr index = parse_expression();
            consume(TokenType::RBRACKET, "Expected ']' after array index");
            expr = make_array_index(line, std::move(expr), std::move(index));
        } else if (match(TokenType::DOT)) {
            int line = previous().line;
            Token member = consume(TokenType::IDENTIFIER, "Expected member or method name after '.'");
            if (match(TokenType::LPAREN)) {
                std::vector<ExprPtr> args;
                if (!check(TokenType::RPAREN)) {
                    do {
                        args.push_back(parse_expression());
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expected ')' after method arguments");
                expr = make_method_call(line, std::move(expr), member.lexeme, std::move(args));
            } else {
                expr = make_member_access(line, std::move(expr), member.lexeme);
            }
        } else if (match(TokenType::PLUS_PLUS) || match(TokenType::MINUS_MINUS)) {
            TokenType op = previous().type;
            int line = expr->line;
            expr = make_post_unary(line, op, std::move(expr));
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::parse_primary() {
    ExprPtr expr = nullptr;

    if (match(TokenType::INT_LITERAL)) {
        expr = make_int_literal(previous().line, std::stoll(previous().lexeme));
    } else if (match(TokenType::FLOAT_LITERAL)) {
        expr = make_float_literal(previous().line, std::stod(previous().lexeme));
    } else if (match(TokenType::STRING_LITERAL)) {
        expr = make_string_literal(previous().line, previous().lexeme);
    } else if (match(TokenType::KW_TRUE)) {
        expr = make_bool_literal(previous().line, true);
    } else if (match(TokenType::KW_FALSE)) {
        expr = make_bool_literal(previous().line, false);
    } else if (match(TokenType::KW_NULL)) {
        expr = make_null(previous().line);
    } else if (match(TokenType::KW_SIZEOF)) {
        int line = previous().line;
        consume(TokenType::LPAREN, "Expected '(' after 'sizeof'");
        DataType type = parse_type();
        consume(TokenType::RPAREN, "Expected ')' after type");
        expr = make_sizeof(line, type);
    } else if (match(TokenType::KW_TYPEOF)) {
        int line = previous().line;
        consume(TokenType::LPAREN, "Expected '(' after 'typeof'");
        ExprPtr operand = parse_expression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        expr = make_typeof(line, std::move(operand));
    } else if (check_type_start()) {
        int line = peek().line;
        DataType type = parse_type();
        consume(TokenType::LPAREN, "Expected '(' for constructor call");
        std::vector<ExprPtr> args;
        if (!check(TokenType::RPAREN)) {
            do {
                args.push_back(parse_expression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')' after constructor arguments");
        expr = make_constructor(line, type, std::move(args));
    } else if (match(TokenType::IDENTIFIER)) {
        expr = make_var_ref(previous().line, previous().lexeme);
    } else if (match(TokenType::LPAREN)) {
        bool old_shift = allow_shift_operators_;
        allow_shift_operators_ = true;
        expr = parse_expression();
        allow_shift_operators_ = old_shift;
        consume(TokenType::RPAREN, "Expected ')' after expression");
    } else if (match(TokenType::KW_NEW)) {
        int line = previous().line;
        DataType elem_type = parse_type();
        consume(TokenType::LBRACKET, "Expected '[' for array size");
        ExprPtr size = parse_expression();
        consume(TokenType::RBRACKET, "Expected ']' after array size");
        expr = make_new_array(line, elem_type, std::move(size));
    } else if (match(TokenType::KW_LEN)) {
        int line = previous().line;
        consume(TokenType::LPAREN, "Expected '(' after 'len'");
        ExprPtr array = parse_expression();
        consume(TokenType::RPAREN, "Expected ')' after array argument");
        expr = make_len(line, std::move(array));
    } else {
        error(peek(), "Expected expression");
        throw std::runtime_error("Expected expression");
    }

    // Postfix type casting using 'as' keyword
    while (match(TokenType::KW_AS)) {
        DataType target_type = parse_type();
        int line = expr->line;
        expr = make_cast(line, target_type, std::move(expr));
    }

    return expr;
}
