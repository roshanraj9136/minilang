#pragma once

#include <string>
#include <unordered_map>
#include "token.h"

class Lexer {
public:
    explicit Lexer(const std::string& source);
    Token next_token();
    const std::string& source() const;

private:
    std::string source_;
    int current_;
    int start_;
    int line_;
    int column_;
    int start_column_;
    static const std::unordered_map<std::string, TokenType> keywords_;

    char advance();
    char peek() const;
    char peek_next() const;
    bool match(char expected);
    bool is_at_end() const;
    Token make_token(TokenType type) const;
    Token error_token(const std::string& message) const;
    void skip_whitespace();
    Token scan_number();
    Token scan_string();
    Token scan_identifier();
};
