#include "lexer.h"

#include <cctype>
#include <utility>

Lexer::Lexer(const std::string& src, Diagnostics& diag)
    : src_(src), diag_(diag) {}

std::vector<Token> Lexer::tokenize() {
    while (!atEnd()) scanToken();
    add(Tok::Newline, "", line_, col_);
    add(Tok::Eof, "", line_, col_);
    return std::move(tokens_);
}

bool Lexer::atEnd() const {
    return pos_ >= src_.size();
}

char Lexer::peek(size_t offset) const {
    return (pos_ + offset < src_.size()) ? src_[pos_ + offset] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') {
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

void Lexer::add(Tok type, std::string lexeme, int line, int col) {
    tokens_.push_back({type, std::move(lexeme), line, col});
}

bool Lexer::isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::isIdentPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

void Lexer::scanToken() {
    int line = line_, col = col_;
    char c = advance();
    switch (c) {
        case ' ': case '\t': case '\r': return;
        case '\n': add(Tok::Newline, "\\n", line, col); return;
        case '(': add(Tok::LParen, "(", line, col); return;
        case ')': add(Tok::RParen, ")", line, col); return;
        case '[': add(Tok::LBracket, "[", line, col); return;
        case ']': add(Tok::RBracket, "]", line, col); return;
        case ',': add(Tok::Comma, ",", line, col); return;
        case '.': add(Tok::Dot, ".", line, col); return;
        case ':': add(Tok::Colon, ":", line, col); return;
        case '&': add(Tok::Ampersand, "&", line, col); return;
        case '+': add(Tok::Plus, "+", line, col); return;
        case '-': add(Tok::Minus, "-", line, col); return;
        case '*': add(Tok::Star, "*", line, col); return;
        case '=': add(Tok::Eq, "=", line, col); return;
        case '/':
            if (peek() == '/') {
                while (!atEnd() && peek() != '\n') advance();
                return;
            }
            add(Tok::Slash, "/", line, col);
            return;
        case '<':
            if (peek() == '-') { advance(); add(Tok::Arrow, "<-", line, col); }
            else if (peek() == '=') { advance(); add(Tok::Le, "<=", line, col); }
            else if (peek() == '>') { advance(); add(Tok::Neq, "<>", line, col); }
            else add(Tok::Lt, "<", line, col);
            return;
        case '>':
            if (peek() == '=') { advance(); add(Tok::Ge, ">=", line, col); }
            else add(Tok::Gt, ">", line, col);
            return;
        case '"':
            scanString(line, col);
            return;
        case '\'':
            scanChar(line, col);
            return;
        default:
            // UTF-8 Left Arrow: ← (0xE2 0x86 0x90)
            if (static_cast<unsigned char>(c) == 0xE2 &&
                static_cast<unsigned char>(peek()) == 0x86 &&
                static_cast<unsigned char>(peek(1)) == 0x90) {
                advance(); advance();
                add(Tok::Arrow, "<-", line, col);
                return;
            }
            if (isDigit(c)) { scanNumber(c, line, col); return; }
            if (isIdentStart(c)) { scanIdent(c, line, col); return; }
            diag_.error(line, col, std::string("unexpected character '") + c + "'");
            return;
    }
}

void Lexer::scanNumber(char first, int line, int col) {
    std::string text(1, first);
    while (isDigit(peek())) text += advance();
    bool isReal = false;
    if (peek() == '.' && isDigit(peek(1))) {
        isReal = true;
        text += advance();
        while (isDigit(peek())) text += advance();
    }
    add(isReal ? Tok::RealLit : Tok::IntLit, text, line, col);
}

void Lexer::scanIdent(char first, int line, int col) {
    std::string text(1, first);
    while (isIdentPart(peek())) text += advance();

    std::string upper = text;
    for (char& ch : upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    // Built-in functions require an immediate following '(' to avoid collision with variable names
    auto bIt = kBuiltinFunctions.find(upper);
    if (bIt != kBuiltinFunctions.end()) {
        size_t look = pos_;
        while (look < src_.size() && (src_[look] == ' ' || src_[look] == '\t')) look++;
        if (look < src_.size() && src_[look] == '(') {
            add(bIt->second, text, line, col);
            return;
        }
    }

    auto it = kKeywords.find(upper);
    add(it == kKeywords.end() ? Tok::Ident : it->second, text, line, col);
}

void Lexer::scanString(int line, int col) {
    std::string text;
    while (!atEnd() && peek() != '"' && peek() != '\n') {
        if (peek() == '\\') {
            advance();
            if (atEnd() || peek() == '\n') break;
            char esc = advance();
            switch (esc) {
                case 'n':  text += '\n'; break;
                case 't':  text += '\t'; break;
                case 'r':  text += '\r'; break;
                case '"':  text += '"';  break;
                case '\\': text += '\\'; break;
                default:   text += esc;  break;
            }
        } else {
            text += advance();
        }
    }
    if (atEnd() || peek() == '\n') {
        diag_.error(line, col, "unterminated string literal");
        return;
    }
    advance();
    add(Tok::StrLit, text, line, col);
}

void Lexer::scanChar(int line, int col) {
    std::string text;
    if (atEnd() || peek() == '\n' || peek() == '\'') {
        diag_.error(line, col, "empty character literal");
        if (!atEnd() && peek() == '\'') advance();
        return;
    }
    if (peek() == '\\') {
        advance();
        if (atEnd() || peek() == '\n') {
            diag_.error(line, col, "unterminated character literal");
            return;
        }
        char esc = advance();
        switch (esc) {
            case 'n':  text += '\n'; break;
            case 't':  text += '\t'; break;
            case 'r':  text += '\r'; break;
            case '\'': text += '\''; break;
            case '\\': text += '\\'; break;
            default:   text += esc;  break;
        }
    } else {
        text += advance();
    }
    if (atEnd() || peek() != '\'') {
        diag_.error(line, col, "unterminated character literal");
        return;
    }
    advance();
    add(Tok::CharLit, text, line, col);
}

