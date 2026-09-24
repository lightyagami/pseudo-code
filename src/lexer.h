#ifndef PSEUDOC_LEXER_H
#define PSEUDOC_LEXER_H

#include "diagnostics.h"
#include "token.h"

#include <string>
#include <vector>

class Lexer {
public:
    Lexer(const std::string& src, Diagnostics& diag);

    std::vector<Token> tokenize();

private:
    const std::string& src_;
    Diagnostics& diag_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    std::vector<Token> tokens_;

    bool atEnd() const;
    char peek(size_t offset = 0) const;
    char advance();
    void add(Tok type, std::string lexeme, int line, int col);

    static bool isIdentStart(char c);
    static bool isIdentPart(char c);
    static bool isDigit(char c);

    void scanToken();
    void scanNumber(char first, int line, int col);
    void scanIdent(char first, int line, int col);
    void scanString(int line, int col);
};

#endif // PSEUDOC_LEXER_H
