#pragma once

#include <string>

enum class TokenType {
    PLUS, MINUS, STAR, SLASH, PERCENT,
    LPAREN, RPAREN, LBRACE, RBRACE,
    SEMICOLON, COLON, COMMA,

    EQUAL, EQUAL_EQUAL,
    BANG, BANG_EQUAL,
    LESS, LESS_EQUAL,
    GREATER, GREATER_EQUAL,
    ARROW,
    AMP_AMP,
    PIPE_PIPE,
    AMP,
    LBRACKET, RBRACKET,

    // Compound assignment
    PLUS_EQUAL,    // +=
    MINUS_EQUAL,   // -=
    STAR_EQUAL,    // *=
    SLASH_EQUAL,   // /=
    PERCENT_EQUAL, // %=

    // Increment/Decrement
    PLUS_PLUS,     // ++
    MINUS_MINUS,   // --

    // Bitwise operators
    PIPE,          // |
    CARET,         // ^
    TILDE,         // ~
    LESS_LESS,     // <<
    GREATER_GREATER, // >>

    // Ternary
    QUESTION,      // ?

    // Dot operator
    DOT,           // .

    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    IDENTIFIER,

    KW_LET, KW_FN, KW_RETURN,
    KW_IF, KW_ELSE, KW_WHILE, KW_FOR,
    KW_BREAK, KW_CONTINUE,
    KW_TRUE, KW_FALSE,
    KW_INT, KW_FLOAT, KW_BOOL, KW_STRING,
    KW_PRINT, KW_NEW, KW_LEN,
    KW_STRUCT, KW_ENUM, KW_SWITCH, KW_CASE, KW_DEFAULT,
    KW_DO, KW_CONST, KW_NULL, KW_AS, KW_VOID,
    KW_SIZEOF, KW_TYPEOF,

    TOKEN_EOF, TOKEN_ERROR
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

std::string token_type_to_string(TokenType type);
