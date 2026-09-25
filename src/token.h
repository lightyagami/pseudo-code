#ifndef PSEUDOC_TOKEN_H
#define PSEUDOC_TOKEN_H

#include <string>
#include <unordered_map>

enum class Tok {
    IntLit, RealLit, StrLit, CharLit, Ident,
    // Keywords - Types & Declarations
    Declare, Constant, Integer, Real, Boolean, String, Char, Array, Of,
    Type, EndType,
    // OOP Keywords
    Class, EndClass, Inherits, Extends, Super, New, Public, Private, Protected,
    // Procedures & Functions
    Procedure, EndProcedure, Function, EndFunction, Returns, Return, Call, ByVal, ByRef,
    // Control Flow
    If, Then, Else, EndIf,
    While, Do, EndWhile,
    Repeat, Until,
    For, To, Step, Next,
    Case, Otherwise, EndCase,
    // I/O & Files
    Output, Input,
    OpenFile, CloseFile, ReadFile, WriteFile, Read, Write, Append,
    Random, Seek, GetRecord, PutRecord,
    // Logic & Math Operators
    And, Or, Not, Div, Mod, True, False,
    // Built-in functions
    Length, Substring, Mid, Left, Right, UCase, LCase, NumToStr, StrToNum,
    Chr, Asc, IntFunc, Round, Rnd, EofFunc,
    // Symbols
    Arrow, Colon, Comma, Dot, LParen, RParen, LBracket, RBracket,
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
    {"DECLARE", Tok::Declare}, {"CONSTANT", Tok::Constant},
    {"INTEGER", Tok::Integer}, {"REAL", Tok::Real},
    {"BOOLEAN", Tok::Boolean}, {"STRING", Tok::String}, {"CHAR", Tok::Char},
    {"ARRAY", Tok::Array}, {"OF", Tok::Of},
    {"TYPE", Tok::Type}, {"ENDTYPE", Tok::EndType},
    {"CLASS", Tok::Class}, {"ENDCLASS", Tok::EndClass},
    {"INHERITS", Tok::Inherits}, {"EXTENDS", Tok::Extends}, {"SUPER", Tok::Super},
    {"NEW", Tok::New}, {"PUBLIC", Tok::Public}, {"PRIVATE", Tok::Private}, {"PROTECTED", Tok::Protected},
    {"PROCEDURE", Tok::Procedure}, {"ENDPROCEDURE", Tok::EndProcedure},
    {"FUNCTION", Tok::Function}, {"ENDFUNCTION", Tok::EndFunction},
    {"RETURNS", Tok::Returns}, {"RETURN", Tok::Return}, {"CALL", Tok::Call},
    {"BYVAL", Tok::ByVal}, {"BYREF", Tok::ByRef},
    {"IF", Tok::If}, {"THEN", Tok::Then}, {"ELSE", Tok::Else}, {"ENDIF", Tok::EndIf},
    {"WHILE", Tok::While}, {"DO", Tok::Do}, {"ENDWHILE", Tok::EndWhile},
    {"REPEAT", Tok::Repeat}, {"UNTIL", Tok::Until},
    {"FOR", Tok::For}, {"TO", Tok::To}, {"STEP", Tok::Step}, {"NEXT", Tok::Next},
    {"CASE", Tok::Case}, {"OTHERWISE", Tok::Otherwise}, {"ENDCASE", Tok::EndCase},
    {"OUTPUT", Tok::Output}, {"INPUT", Tok::Input},
    {"OPENFILE", Tok::OpenFile}, {"CLOSEFILE", Tok::CloseFile},
    {"READFILE", Tok::ReadFile}, {"WRITEFILE", Tok::WriteFile},
    {"READ", Tok::Read}, {"WRITE", Tok::Write}, {"APPEND", Tok::Append},
    {"RANDOM", Tok::Random}, {"SEEK", Tok::Seek},
    {"GETRECORD", Tok::GetRecord}, {"PUTRECORD", Tok::PutRecord},
    {"AND", Tok::And}, {"OR", Tok::Or}, {"NOT", Tok::Not},
    {"DIV", Tok::Div}, {"MOD", Tok::Mod},
    {"TRUE", Tok::True}, {"FALSE", Tok::False}
};

static const std::unordered_map<std::string, Tok> kBuiltinFunctions = {
    {"LENGTH", Tok::Length}, {"SUBSTRING", Tok::Substring},
    {"MID", Tok::Mid}, {"LEFT", Tok::Left}, {"RIGHT", Tok::Right},
    {"UCASE", Tok::UCase}, {"LCASE", Tok::LCase},
    {"NUM_TO_STR", Tok::NumToStr}, {"STR_TO_NUM", Tok::StrToNum},
    {"CHR", Tok::Chr}, {"ASC", Tok::Asc},
    {"INT", Tok::IntFunc}, {"ROUND", Tok::Round},
    {"RND", Tok::Rnd}, {"RANDOM", Tok::Rnd},
    {"EOF", Tok::EofFunc}
};

#endif // PSEUDOC_TOKEN_H
