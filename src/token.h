#ifndef PSEUDOC_TOKEN_H
#define PSEUDOC_TOKEN_H

#include <string>
#include <unordered_map>

enum class Tok {
    IntLit, RealLit, StrLit, Ident,
    // Keywords
    Declare, Integer, Real, Boolean, String, Array, Of,
    If, Then, Else, EndIf,
    While, Do, EndWhile,
    For, To, Step, Next,
    Output, Input,
    And, Or, Not, Div, Mod, True, False,
    // Built-in functions
    Length, Substring, UCase, LCase, NumToStr, StrToNum,
    // Symbols
    Arrow, Colon, Comma, LParen, RParen, LBracket, RBracket,
    Plus, Minus, Star, Slash, Ampersand,
    Eq, Neq, Lt, Le, Gt, Ge,
    Newline, Eof
};

struct Token {
    Tok type;
    std::string lexeme;
    int line;
    int col;
};

static const std::unordered_map<std::string, Tok> kKeywords = {
    {"DECLARE", Tok::Declare}, {"INTEGER", Tok::Integer}, {"REAL", Tok::Real},
    {"BOOLEAN", Tok::Boolean}, {"STRING", Tok::String},
    {"ARRAY", Tok::Array}, {"OF", Tok::Of},
    {"IF", Tok::If}, {"THEN", Tok::Then}, {"ELSE", Tok::Else}, {"ENDIF", Tok::EndIf},
    {"WHILE", Tok::While}, {"DO", Tok::Do}, {"ENDWHILE", Tok::EndWhile},
    {"FOR", Tok::For}, {"TO", Tok::To}, {"STEP", Tok::Step}, {"NEXT", Tok::Next},
    {"OUTPUT", Tok::Output}, {"INPUT", Tok::Input},
    {"AND", Tok::And}, {"OR", Tok::Or}, {"NOT", Tok::Not},
    {"DIV", Tok::Div}, {"MOD", Tok::Mod},
    {"TRUE", Tok::True}, {"FALSE", Tok::False}
};

static const std::unordered_map<std::string, Tok> kBuiltinFunctions = {
    {"LENGTH", Tok::Length}, {"SUBSTRING", Tok::Substring},
    {"UCASE", Tok::UCase}, {"LCASE", Tok::LCase},
    {"NUM_TO_STR", Tok::NumToStr}, {"STR_TO_NUM", Tok::StrToNum}
};

#endif // PSEUDOC_TOKEN_H
