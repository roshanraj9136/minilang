#include "lexer.h"

const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"let", TokenType::KW_LET},
    {"fn", TokenType::KW_FN},
    {"return", TokenType::KW_RETURN},
    {"if", TokenType::KW_IF},
    {"else", TokenType::KW_ELSE},
    {"while", TokenType::KW_WHILE},
    {"for", TokenType::KW_FOR},
    {"break", TokenType::KW_BREAK},
    {"continue", TokenType::KW_CONTINUE},
    {"true", TokenType::KW_TRUE},
    {"false", TokenType::KW_FALSE},
    {"int", TokenType::KW_INT},
    {"float", TokenType::KW_FLOAT},
    {"bool", TokenType::KW_BOOL},
    {"string", TokenType::KW_STRING},
    {"print", TokenType::KW_PRINT},
    {"new", TokenType::KW_NEW},
    {"len", TokenType::KW_LEN},
    {"struct", TokenType::KW_STRUCT},
    {"enum", TokenType::KW_ENUM},
    {"switch", TokenType::KW_SWITCH},
    {"case", TokenType::KW_CASE},
    {"default", TokenType::KW_DEFAULT},
    {"do", TokenType::KW_DO},
    {"const", TokenType::KW_CONST},
    {"null", TokenType::KW_NULL},
    {"as", TokenType::KW_AS},
    {"void", TokenType::KW_VOID},
    {"sizeof", TokenType::KW_SIZEOF},
    {"typeof", TokenType::KW_TYPEOF},
    {"std::string", TokenType::KW_STRING}
};

Lexer::Lexer(const std::string& source)
    : source_(source), current_(0), start_(0), line_(1), column_(1), start_column_(1) {}

const std::string& Lexer::source() const {
    return source_;
}

bool Lexer::is_at_end() const {
    return current_ >= static_cast<int>(source_.size());
}

char Lexer::advance() {
    char c = source_[current_++];
    column_++;
    return c;
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= static_cast<int>(source_.size())) return '\0';
    return source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (is_at_end()) return false;
    if (source_[current_] != expected) return false;
    current_++;
    column_++;
    return true;
}

Token Lexer::make_token(TokenType type) const {
    return Token{
        type,
        source_.substr(start_, current_ - start_),
        line_,
        start_column_
    };
}

Token Lexer::error_token(const std::string& message) const {
    return Token{
        TokenType::TOKEN_ERROR,
        message,
        line_,
        start_column_
    };
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                advance();
                line_++;
                column_ = 1;
                break;
            case '#':
                while (!is_at_end() && peek() != '\n') {
                    advance();
                }
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) {
                        advance();
                    }
                } else if (peek_next() == '*') {
                    advance(); // consume '/'
                    advance(); // consume '*'
                    while (!is_at_end()) {
                        if (peek() == '*' && peek_next() == '/') {
                            advance(); // consume '*'
                            advance(); // consume '/'
                            break;
                        }
                        if (peek() == '\n') {
                            line_++;
                            column_ = 1;
                        }
                        advance();
                    }
                } else {
                    return;
                }
                break;
            default:
                if (isalpha(c)) {
                    std::string rem = source_.substr(current_, 6);
                    if (rem == "using ") {
                        while (!is_at_end() && peek() != ';' && peek() != '\n') {
                            advance();
                        }
                        if (peek() == ';') {
                            advance();
                        }
                        break;
                    }
                }
                return;
        }
    }
}

Token Lexer::scan_number() {
    while (isdigit(peek())) {
        advance();
    }
    if (peek() == '.' && isdigit(peek_next())) {
        advance(); // consume '.'
        while (isdigit(peek())) {
            advance();
        }
        return make_token(TokenType::FLOAT_LITERAL);
    }
    return make_token(TokenType::INT_LITERAL);
}

Token Lexer::scan_string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            line_++;
            column_ = 1;
        }
        if (peek() == '\\') {
            advance();
        }
        advance();
    }

    if (is_at_end()) {
        return error_token("Unterminated string literal");
    }

    advance();

    std::string value;
    int i = start_ + 1;
    while (i < current_ - 1) {
        if (source_[i] == '\\' && i + 1 < current_ - 1) {
            i++;
            switch (source_[i]) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                default: value += source_[i]; break;
            }
        } else {
            value += source_[i];
        }
        i++;
    }

    Token tok = make_token(TokenType::STRING_LITERAL);
    tok.lexeme = value;
    return tok;
}

Token Lexer::scan_identifier() {
    while (isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string text = source_.substr(start_, current_ - start_);
    if (text == "std" && peek() == ':' && peek_next() == ':') {
        advance(); // consume ':'
        advance(); // consume ':'
        start_ = current_;
        while (isalnum(peek()) || peek() == '_') {
            advance();
        }
        text = source_.substr(start_, current_ - start_);
    }

    auto it = keywords_.find(text);
    if (it != keywords_.end()) {
        return make_token(it->second);
    }
    return make_token(TokenType::IDENTIFIER);
}

Token Lexer::next_token() {
    skip_whitespace();
    start_ = current_;
    start_column_ = column_;

    if (is_at_end()) {
        return make_token(TokenType::TOKEN_EOF);
    }

    char c = advance();

    if (isdigit(c)) return scan_number();
    if (isalpha(c) || c == '_') return scan_identifier();

    switch (c) {
        case '(': return make_token(TokenType::LPAREN);
        case ')': return make_token(TokenType::RPAREN);
        case '{': return make_token(TokenType::LBRACE);
        case '}': return make_token(TokenType::RBRACE);
        case ';': return make_token(TokenType::SEMICOLON);
        case ':': return make_token(TokenType::COLON);
        case ',': return make_token(TokenType::COMMA);
        case '+':
            if (match('+')) return make_token(TokenType::PLUS_PLUS);
            if (match('=')) return make_token(TokenType::PLUS_EQUAL);
            return make_token(TokenType::PLUS);
        case '*':
            if (match('=')) return make_token(TokenType::STAR_EQUAL);
            return make_token(TokenType::STAR);
        case '/':
            if (match('=')) return make_token(TokenType::SLASH_EQUAL);
            return make_token(TokenType::SLASH);
        case '%':
            if (match('=')) return make_token(TokenType::PERCENT_EQUAL);
            return make_token(TokenType::PERCENT);
        case '-':
            if (match('-')) return make_token(TokenType::MINUS_MINUS);
            if (match('=')) return make_token(TokenType::MINUS_EQUAL);
            if (match('>')) return make_token(TokenType::ARROW);
            return make_token(TokenType::MINUS);
        case '=':
            if (match('=')) return make_token(TokenType::EQUAL_EQUAL);
            return make_token(TokenType::EQUAL);
        case '!':
            if (match('=')) return make_token(TokenType::BANG_EQUAL);
            return make_token(TokenType::BANG);
        case '<':
            if (match('=')) return make_token(TokenType::LESS_EQUAL);
            if (match('<')) return make_token(TokenType::LESS_LESS);
            return make_token(TokenType::LESS);
        case '>':
            if (match('=')) return make_token(TokenType::GREATER_EQUAL);
            if (match('>')) return make_token(TokenType::GREATER_GREATER);
            return make_token(TokenType::GREATER);
        case '&':
            if (match('&')) return make_token(TokenType::AMP_AMP);
            return make_token(TokenType::AMP);
        case '|':
            if (match('|')) return make_token(TokenType::PIPE_PIPE);
            return make_token(TokenType::PIPE);
        case '^': return make_token(TokenType::CARET);
        case '~': return make_token(TokenType::TILDE);
        case '?': return make_token(TokenType::QUESTION);
        case '.': return make_token(TokenType::DOT);
        case '[': return make_token(TokenType::LBRACKET);
        case ']': return make_token(TokenType::RBRACKET);
        case '"': return scan_string();
        case '\'': {
            if (is_at_end()) return error_token("Unterminated character literal");
            char c = advance();
            if (c == '\\') {
                if (is_at_end()) return error_token("Unterminated character literal");
                char esc = advance();
                switch (esc) {
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case '\\': c = '\\'; break;
                    case '\'': c = '\''; break;
                    default: return error_token("Unknown escape sequence");
                }
            }
            if (is_at_end() || peek() != '\'') {
                return error_token("Character literal must contain exactly one character");
            }
            advance();
            return Token{
                TokenType::INT_LITERAL,
                std::to_string(static_cast<int64_t>(c)),
                line_,
                start_column_
            };
        }
    }

    return error_token("Unexpected character");
}

std::string token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COLON: return "COLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::BANG: return "BANG";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::ARROW: return "ARROW";
        case TokenType::AMP_AMP: return "AMP_AMP";
        case TokenType::PIPE_PIPE: return "PIPE_PIPE";
        case TokenType::AMP: return "AMP";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::STAR_EQUAL: return "STAR_EQUAL";
        case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";
        case TokenType::PERCENT_EQUAL: return "PERCENT_EQUAL";
        case TokenType::PLUS_PLUS: return "PLUS_PLUS";
        case TokenType::MINUS_MINUS: return "MINUS_MINUS";
        case TokenType::PIPE: return "PIPE";
        case TokenType::CARET: return "CARET";
        case TokenType::TILDE: return "TILDE";
        case TokenType::LESS_LESS: return "LESS_LESS";
        case TokenType::GREATER_GREATER: return "GREATER_GREATER";
        case TokenType::QUESTION: return "QUESTION";
        case TokenType::DOT: return "DOT";
        case TokenType::INT_LITERAL: return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::KW_LET: return "KW_LET";
        case TokenType::KW_FN: return "KW_FN";
        case TokenType::KW_RETURN: return "KW_RETURN";
        case TokenType::KW_IF: return "KW_IF";
        case TokenType::KW_ELSE: return "KW_ELSE";
        case TokenType::KW_WHILE: return "KW_WHILE";
        case TokenType::KW_FOR: return "KW_FOR";
        case TokenType::KW_BREAK: return "KW_BREAK";
        case TokenType::KW_CONTINUE: return "KW_CONTINUE";
        case TokenType::KW_TRUE: return "KW_TRUE";
        case TokenType::KW_FALSE: return "KW_FALSE";
        case TokenType::KW_INT: return "KW_INT";
        case TokenType::KW_FLOAT: return "KW_FLOAT";
        case TokenType::KW_BOOL: return "KW_BOOL";
        case TokenType::KW_STRING: return "KW_STRING";
        case TokenType::KW_PRINT: return "KW_PRINT";
        case TokenType::KW_NEW: return "KW_NEW";
        case TokenType::KW_LEN: return "KW_LEN";
        case TokenType::KW_STRUCT: return "KW_STRUCT";
        case TokenType::KW_ENUM: return "KW_ENUM";
        case TokenType::KW_SWITCH: return "KW_SWITCH";
        case TokenType::KW_CASE: return "KW_CASE";
        case TokenType::KW_DEFAULT: return "KW_DEFAULT";
        case TokenType::KW_DO: return "KW_DO";
        case TokenType::KW_CONST: return "KW_CONST";
        case TokenType::KW_NULL: return "KW_NULL";
        case TokenType::KW_AS: return "KW_AS";
        case TokenType::KW_VOID: return "KW_VOID";
        case TokenType::KW_SIZEOF: return "KW_SIZEOF";
        case TokenType::KW_TYPEOF: return "KW_TYPEOF";
        case TokenType::TOKEN_EOF: return "TOKEN_EOF";
        case TokenType::TOKEN_ERROR: return "TOKEN_ERROR";
        default: return "UNKNOWN";
    }
}
