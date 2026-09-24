// pseudoc.cpp - Pseudocode-to-C Compiler, Bytecode VM & Interactive REPL (v3.0)
//
// Build:  g++ -std=c++17 -O2 -Wall -Wextra pseudoc.cpp -o pseudoc
// Use:    ./pseudoc program.pseudo               (execute directly on VM)
//         ./pseudoc program.pseudo -o program.c  (compile to C)
//         ./pseudoc -i                           (interactive REPL on VM)
//         ./pseudoc -d program.pseudo            (disassemble bytecode)
//
// Pipeline: source -> Lexer -> Parser -> Sema -> [Bytecode VM | CodeGen -> C source]

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Diagnostics

class Diagnostics {
public:
    Diagnostics(std::string file, const std::string& source, bool quiet = false)
        : file_(std::move(file)), quiet_(quiet) {
        std::istringstream in(source);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines_.push_back(line);
        }
    }

    void error(int line, int col, const std::string& msg) {
        ++errors_;
        if (quiet_) return;
        std::cerr << file_ << ":" << line << ":" << col << ": error: " << msg << "\n";
        if (line >= 1 && line <= static_cast<int>(lines_.size())) {
            const std::string& text = lines_[line - 1];
            std::string pad;
            for (int i = 0; i < col - 1; ++i)
                pad += (i < static_cast<int>(text.size()) && text[i] == '\t') ? '\t' : ' ';
            std::cerr << "  " << text << "\n  " << pad << "^\n";
        }
    }

    int errorCount() const { return errors_; }
    void reset() { errors_ = 0; lines_.clear(); }

private:
    std::string file_;
    std::vector<std::string> lines_;
    int errors_ = 0;
    bool quiet_ = false;
};

// Tokens and Lexer

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

class Lexer {
public:
    Lexer(const std::string& src, Diagnostics& diag) : src_(src), diag_(diag) {}

    std::vector<Token> tokenize() {
        while (!atEnd()) scanToken();
        add(Tok::Newline, "", line_, col_);
        add(Tok::Eof, "", line_, col_);
        return std::move(tokens_);
    }

private:
    const std::string& src_;
    Diagnostics& diag_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    std::vector<Token> tokens_;

    bool atEnd() const { return pos_ >= src_.size(); }

    char peek(size_t offset = 0) const {
        return pos_ + offset < src_.size() ? src_[pos_ + offset] : '\0';
    }

    char advance() {
        char c = src_[pos_++];
        if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
        return c;
    }

    void add(Tok type, std::string lexeme, int line, int col) {
        tokens_.push_back({type, std::move(lexeme), line, col});
    }

    static bool isIdentStart(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }
    static bool isIdentPart(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }
    static bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

    void scanToken() {
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
            case '"': scanString(line, col); return;
            default:
                // UTF-8 left arrow: "\xE2\x86\x90"
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

    void scanNumber(char first, int line, int col) {
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

    void scanIdent(char first, int line, int col) {
        std::string text(1, first);
        while (isIdentPart(peek())) text += advance();

        std::string upper = text;
        for (char& ch : upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

        // Built-ins require an immediate following '(' to avoid reserving identifier names
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

    // Unescape sequences directly into character byte values
    void scanString(int line, int col) {
        std::string text;
        while (!atEnd() && peek() != '"' && peek() != '\n') {
            if (peek() == '\\') {
                advance(); // consume '\\'
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
        advance(); // closing '"'
        add(Tok::StrLit, text, line, col);
    }
};

// AST Data Structures

enum class BaseType { Integer, Real, Boolean, String, Error };

struct TypeInfo {
    BaseType base = BaseType::Integer;
    bool isArray = false;
    int dims = 0;
    long long lower1 = 0, upper1 = 0;
    long long lower2 = 0, upper2 = 0;

    bool operator==(const TypeInfo& o) const {
        if (base != o.base || isArray != o.isArray || dims != o.dims) return false;
        if (!isArray) return true;
        if (dims == 1) return lower1 == o.lower1 && upper1 == o.upper1;
        return lower1 == o.lower1 && upper1 == o.upper1 && lower2 == o.lower2 && upper2 == o.upper2;
    }
    bool operator!=(const TypeInfo& o) const { return !(*this == o); }
};

static const char* baseTypeName(BaseType t) {
    switch (t) {
        case BaseType::Integer: return "INTEGER";
        case BaseType::Real:    return "REAL";
        case BaseType::Boolean: return "BOOLEAN";
        case BaseType::String:  return "STRING";
        default:                return "<error>";
    }
}

static std::string typeString(const TypeInfo& t) {
    if (!t.isArray) return baseTypeName(t.base);
    std::string s = "ARRAY[";
    s += std::to_string(t.lower1) + ":" + std::to_string(t.upper1);
    if (t.dims == 2) s += ", " + std::to_string(t.lower2) + ":" + std::to_string(t.upper2);
    s += "] OF " + std::string(baseTypeName(t.base));
    return s;
}

static bool isNumeric(const TypeInfo& t) {
    return !t.isArray && (t.base == BaseType::Integer || t.base == BaseType::Real);
}

static bool isComparison(Tok op) {
    return op == Tok::Eq || op == Tok::Neq || op == Tok::Lt ||
    op == Tok::Le || op == Tok::Gt || op == Tok::Ge;
}

static std::string opName(Tok op) {
    switch (op) {
        case Tok::Plus:      return "+";
        case Tok::Minus:     return "-";
        case Tok::Star:      return "*";
        case Tok::Slash:     return "/";
        case Tok::Ampersand: return "&";
        case Tok::Div:       return "DIV";
        case Tok::Mod:       return "MOD";
        case Tok::And:       return "AND";
        case Tok::Or:        return "OR";
        case Tok::Not:       return "NOT";
        case Tok::Eq:        return "=";
        case Tok::Neq:       return "<>";
        case Tok::Lt:        return "<";
        case Tok::Le:        return "<=";
        case Tok::Gt:        return ">";
        case Tok::Ge:        return ">=";
        default:             return "?";
    }
}

struct Expr {
    enum class Kind { Literal, Var, ArrayAccess, Unary, Binary, Call };
    Kind kind;
    int line, col;
    TypeInfo type{BaseType::Error, false, 0, 0, 0, 0, 0};
    Expr(Kind k, int l, int c) : kind(k), line(l), col(c) {}
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct LiteralExpr : Expr {
    Tok litType = Tok::IntLit;
    std::string text;
    LiteralExpr(int l, int c) : Expr(Kind::Literal, l, c) {}
};

struct VarExpr : Expr {
    std::string name;
    VarExpr(int l, int c) : Expr(Kind::Var, l, c) {}
};

struct ArrayAccessExpr : Expr {
    std::string name;
    std::vector<ExprPtr> indices;
    ArrayAccessExpr(int l, int c) : Expr(Kind::ArrayAccess, l, c) {}
};

struct UnaryExpr : Expr {
    Tok op = Tok::Minus;
    ExprPtr operand;
    UnaryExpr(int l, int c) : Expr(Kind::Unary, l, c) {}
};

struct BinaryExpr : Expr {
    Tok op = Tok::Plus;
    ExprPtr lhs, rhs;
    BinaryExpr(int l, int c) : Expr(Kind::Binary, l, c) {}
};

struct CallExpr : Expr {
    Tok func;
    std::vector<ExprPtr> args;
    CallExpr(int l, int c) : Expr(Kind::Call, l, c) {}
};

struct Stmt {
    enum class Kind { Declare, Assign, ArrayAssign, Output, Input, If, While, For };
    Kind kind;
    int line, col;
    Stmt(Kind k, int l, int c) : kind(k), line(l), col(c) {}
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;

struct DeclareStmt : Stmt {
    std::string name;
    TypeInfo declaredType;
    DeclareStmt(int l, int c) : Stmt(Kind::Declare, l, c) {}
};

struct AssignStmt : Stmt {
    std::string name;
    ExprPtr value;
    AssignStmt(int l, int c) : Stmt(Kind::Assign, l, c) {}
};

struct ArrayAssignStmt : Stmt {
    std::string name;
    std::vector<ExprPtr> indices;
    ExprPtr value;
    ArrayAssignStmt(int l, int c) : Stmt(Kind::ArrayAssign, l, c) {}
};

struct OutputStmt : Stmt {
    std::vector<ExprPtr> args;
    OutputStmt(int l, int c) : Stmt(Kind::Output, l, c) {}
};

struct InputStmt : Stmt {
    std::string name;
    std::vector<ExprPtr> indices;
    InputStmt(int l, int c) : Stmt(Kind::Input, l, c) {}
};

struct IfStmt : Stmt {
    ExprPtr cond;
    Block thenBlock, elseBlock;
    IfStmt(int l, int c) : Stmt(Kind::If, l, c) {}
};

struct WhileStmt : Stmt {
    ExprPtr cond;
    Block body;
    WhileStmt(int l, int c) : Stmt(Kind::While, l, c) {}
};

struct ForStmt : Stmt {
    std::string var;
    ExprPtr start, end, step;
    Block body;
    ForStmt(int l, int c) : Stmt(Kind::For, l, c) {}
};

// Parser (recursive descent)

struct ParseError {};

class Parser {
public:
    Parser(const std::vector<Token>& tokens, Diagnostics& diag) : toks_(tokens), diag_(diag) {}

    Block parseProgram() {
        Block program;
        skipNewlines();
        while (!check(Tok::Eof)) {
            if (StmtPtr s = parseStatementSafe()) program.push_back(std::move(s));
            skipNewlines();
        }
        return program;
    }

private:
    const std::vector<Token>& toks_;
    Diagnostics& diag_;
    size_t pos_ = 0;

    const Token& peek() const { return toks_[pos_]; }
    bool check(Tok t) const { return peek().type == t; }

    const Token& advance() {
        const Token& t = toks_[pos_];
        if (t.type != Tok::Eof) ++pos_;
        return t;
    }

    bool match(Tok t) {
        if (!check(t)) return false;
        advance();
        return true;
    }

    void skipNewlines() { while (check(Tok::Newline)) advance(); }

    static std::string describe(const Token& t) {
        switch (t.type) {
            case Tok::Newline: return "end of line";
            case Tok::Eof:     return "end of file";
            case Tok::StrLit:  return "string literal";
            default:           return "'" + t.lexeme + "'";
        }
    }

    [[noreturn]] void fail(const Token& at, const std::string& msg) {
        diag_.error(at.line, at.col, msg);
        throw ParseError{};
    }

    const Token& expect(Tok t, const std::string& what) {
        if (!check(t)) fail(peek(), "expected " + what + ", found " + describe(peek()));
        return advance();
    }

    void endStatement() { expect(Tok::Newline, "end of line"); }

    void synchronize() {
        while (!check(Tok::Newline) && !check(Tok::Eof)) advance();
        match(Tok::Newline);
    }

    StmtPtr parseStatementSafe() {
        try {
            return parseStatement();
        } catch (const ParseError&) {
            synchronize();
            return nullptr;
        }
    }

    Block parseBlock(std::initializer_list<Tok> terminators) {
        Block block;
        skipNewlines();
        auto atTerminator = [&]() {
            for (Tok t : terminators) if (check(t)) return true;
            return false;
        };
        while (!atTerminator() && !check(Tok::Eof)) {
            if (StmtPtr s = parseStatementSafe()) block.push_back(std::move(s));
            skipNewlines();
        }
        return block;
    }

    StmtPtr parseStatement() {
        const Token& t = peek();
        switch (t.type) {
            case Tok::Declare: return parseDeclare();
            case Tok::Ident:   return parseAssignOrArrayAssign();
            case Tok::Output:  return parseOutput();
            case Tok::Input:   return parseInput();
            case Tok::If:      return parseIf();
            case Tok::While:   return parseWhile();
            case Tok::For:     return parseFor();
            case Tok::Else:     fail(t, "'ELSE' without a matching 'IF'");
            case Tok::EndIf:    fail(t, "'ENDIF' without a matching 'IF'");
            case Tok::EndWhile: fail(t, "'ENDWHILE' without a matching 'WHILE'");
            case Tok::Next:     fail(t, "'NEXT' without a matching 'FOR'");
            default:            fail(t, "unexpected " + describe(t) + " at start of statement");
        }
    }

    BaseType parseBaseType() {
        const Token& t = peek();
        switch (t.type) {
            case Tok::Integer: advance(); return BaseType::Integer;
            case Tok::Real:    advance(); return BaseType::Real;
            case Tok::Boolean: advance(); return BaseType::Boolean;
            case Tok::String:  advance(); return BaseType::String;
            default: fail(t, "expected INTEGER, REAL, BOOLEAN, or STRING, found " + describe(t));
        }
    }

    long long parseBoundInt() {
        bool neg = match(Tok::Minus);
        const Token& t = expect(Tok::IntLit, "an integer bound");
        long long v = 0;
        try {
            v = std::stoll(t.lexeme);
        } catch (const std::exception&) {
            fail(t, "bound '" + t.lexeme + "' exceeds signed 64-bit range");
        }
        return neg ? -v : v;
    }

    TypeInfo parseType() {
        if (match(Tok::Array)) {
            expect(Tok::LBracket, "'['");
            long long l1 = parseBoundInt();
            expect(Tok::Colon, "':'");
            long long u1 = parseBoundInt();
            long long l2 = 0, u2 = 0;
            int dims = 1;
            if (match(Tok::Comma)) {
                dims = 2;
                l2 = parseBoundInt();
                expect(Tok::Colon, "':'");
                u2 = parseBoundInt();
            }
            expect(Tok::RBracket, "']'");
            expect(Tok::Of, "'OF'");
            BaseType bt = parseBaseType();
            return TypeInfo{bt, true, dims, l1, u1, l2, u2};
        }
        return TypeInfo{parseBaseType(), false, 0, 0, 0, 0, 0};
    }

    StmtPtr parseDeclare() {
        const Token& kw = advance();
        auto s = std::make_unique<DeclareStmt>(kw.line, kw.col);
        s->name = expect(Tok::Ident, "a variable name").lexeme;
        expect(Tok::Colon, "':'");
        s->declaredType = parseType();
        endStatement();
        return s;
    }

    StmtPtr parseAssignOrArrayAssign() {
        const Token& name = advance();
        if (match(Tok::LBracket)) {
            auto s = std::make_unique<ArrayAssignStmt>(name.line, name.col);
            s->name = name.lexeme;
            s->indices.push_back(parseExpr());
            if (match(Tok::Comma)) s->indices.push_back(parseExpr());
            expect(Tok::RBracket, "']'");
            expect(Tok::Arrow, "'<-'");
            s->value = parseExpr();
            endStatement();
            return s;
        }
        auto s = std::make_unique<AssignStmt>(name.line, name.col);
        s->name = name.lexeme;
        expect(Tok::Arrow, "'<-'");
        s->value = parseExpr();
        endStatement();
        return s;
    }

    StmtPtr parseOutput() {
        const Token& kw = advance();
        auto s = std::make_unique<OutputStmt>(kw.line, kw.col);
        s->args.push_back(parseExpr());
        while (match(Tok::Comma)) s->args.push_back(parseExpr());
        endStatement();
        return s;
    }

    StmtPtr parseInput() {
        const Token& kw = advance();
        auto s = std::make_unique<InputStmt>(kw.line, kw.col);
        s->name = expect(Tok::Ident, "a variable name").lexeme;
        if (match(Tok::LBracket)) {
            s->indices.push_back(parseExpr());
            if (match(Tok::Comma)) s->indices.push_back(parseExpr());
            expect(Tok::RBracket, "']'");
        }
        endStatement();
        return s;
    }

    StmtPtr parseIf() {
        const Token& kw = advance();
        auto s = std::make_unique<IfStmt>(kw.line, kw.col);
        s->cond = parseExpr();
        expect(Tok::Then, "'THEN'");
        endStatement();
        s->thenBlock = parseBlock({Tok::Else, Tok::EndIf});
        if (match(Tok::Else)) {
            endStatement();
            s->elseBlock = parseBlock({Tok::EndIf});
        }
        expect(Tok::EndIf, "'ENDIF'");
        endStatement();
        return s;
    }

    StmtPtr parseWhile() {
        const Token& kw = advance();
        auto s = std::make_unique<WhileStmt>(kw.line, kw.col);
        s->cond = parseExpr();
        expect(Tok::Do, "'DO'");
        endStatement();
        s->body = parseBlock({Tok::EndWhile});
        expect(Tok::EndWhile, "'ENDWHILE'");
        endStatement();
        return s;
    }

    StmtPtr parseFor() {
        const Token& kw = advance();
        auto s = std::make_unique<ForStmt>(kw.line, kw.col);
        s->var = expect(Tok::Ident, "a loop variable").lexeme;
        expect(Tok::Arrow, "'<-'");
        s->start = parseExpr();
        expect(Tok::To, "'TO'");
        s->end = parseExpr();
        if (match(Tok::Step)) s->step = parseExpr();
        endStatement();
        s->body = parseBlock({Tok::Next});
        expect(Tok::Next, "'NEXT'");
        const Token& nv = expect(Tok::Ident, "the loop variable name after 'NEXT'");
        if (nv.lexeme != s->var)
            fail(nv, "'NEXT " + nv.lexeme + "' does not match 'FOR " + s->var + "'");
        endStatement();
        return s;
    }

    ExprPtr makeBinary(const Token& op, ExprPtr lhs, ExprPtr rhs) {
        auto b = std::make_unique<BinaryExpr>(op.line, op.col);
        b->op = op.type;
        b->lhs = std::move(lhs);
        b->rhs = std::move(rhs);
        return b;
    }

    ExprPtr parseExpr() { return parseOr(); }

    ExprPtr parseOr() {
        ExprPtr lhs = parseAnd();
        while (check(Tok::Or)) {
            const Token& op = advance();
            lhs = makeBinary(op, std::move(lhs), parseAnd());
        }
        return lhs;
    }

    ExprPtr parseAnd() {
        ExprPtr lhs = parseNot();
        while (check(Tok::And)) {
            const Token& op = advance();
            lhs = makeBinary(op, std::move(lhs), parseNot());
        }
        return lhs;
    }

    ExprPtr parseNot() {
        if (check(Tok::Not)) {
            const Token& op = advance();
            auto u = std::make_unique<UnaryExpr>(op.line, op.col);
            u->op = Tok::Not;
            u->operand = parseNot();
            return u;
        }
        return parseComparison();
    }

    ExprPtr parseComparison() {
        ExprPtr lhs = parseConcat();
        if (isComparison(peek().type)) {
            const Token& op = advance();
            lhs = makeBinary(op, std::move(lhs), parseConcat());
        }
        return lhs;
    }

    ExprPtr parseConcat() {
        ExprPtr lhs = parseAdditive();
        while (check(Tok::Ampersand)) {
            const Token& op = advance();
            lhs = makeBinary(op, std::move(lhs), parseAdditive());
        }
        return lhs;
    }

    ExprPtr parseAdditive() {
        ExprPtr lhs = parseTerm();
        while (check(Tok::Plus) || check(Tok::Minus)) {
            const Token& op = advance();
            lhs = makeBinary(op, std::move(lhs), parseTerm());
        }
        return lhs;
    }

    ExprPtr parseTerm() {
        ExprPtr lhs = parseUnary();
        while (check(Tok::Star) || check(Tok::Slash) || check(Tok::Div) || check(Tok::Mod)) {
            const Token& op = advance();
            lhs = makeBinary(op, std::move(lhs), parseUnary());
        }
        return lhs;
    }

    ExprPtr parseUnary() {
        if (check(Tok::Minus)) {
            const Token& op = advance();
            // Handle negative INT64_MIN bound literal (-9223372036854775808)
            if (check(Tok::IntLit) && peek().lexeme == "9223372036854775808") {
                const Token& lit = advance();
                auto e = std::make_unique<LiteralExpr>(op.line, op.col);
                e->litType = Tok::IntLit;
                e->text = "-" + lit.lexeme;
                return e;
            }
            auto u = std::make_unique<UnaryExpr>(op.line, op.col);
            u->op = Tok::Minus;
            u->operand = parseUnary();
            return u;
        }
        return parsePrimary();
    }

    ExprPtr parseBuiltInCall(Tok funcTok) {
        const Token& fn = advance();
        auto call = std::make_unique<CallExpr>(fn.line, fn.col);
        call->func = funcTok;
        expect(Tok::LParen, "'('");
        if (!check(Tok::RParen)) {
            call->args.push_back(parseExpr());
            while (match(Tok::Comma)) call->args.push_back(parseExpr());
        }
        expect(Tok::RParen, "')'");
        return call;
    }

    ExprPtr parsePrimary() {
        const Token& t = peek();
        switch (t.type) {
            case Tok::IntLit: {
                advance();
                try {
                    (void)std::stoll(t.lexeme);
                } catch (const std::exception&) {
                    fail(t, "integer literal '" + t.lexeme + "' exceeds signed 64-bit integer limit");
                }
                auto e = std::make_unique<LiteralExpr>(t.line, t.col);
                e->litType = Tok::IntLit;
                e->text = t.lexeme;
                return e;
            }
            case Tok::RealLit:
            case Tok::StrLit:
            case Tok::True:
            case Tok::False: {
                advance();
                auto e = std::make_unique<LiteralExpr>(t.line, t.col);
                e->litType = t.type;
                e->text = t.lexeme;
                return e;
            }
            case Tok::Length:
            case Tok::Substring:
            case Tok::UCase:
            case Tok::LCase:
            case Tok::NumToStr:
            case Tok::StrToNum:
                return parseBuiltInCall(t.type);

            case Tok::Ident: {
                advance();
                if (match(Tok::LBracket)) {
                    auto arr = std::make_unique<ArrayAccessExpr>(t.line, t.col);
                    arr->name = t.lexeme;
                    arr->indices.push_back(parseExpr());
                    if (match(Tok::Comma)) arr->indices.push_back(parseExpr());
                    expect(Tok::RBracket, "']'");
                    return arr;
                }
                auto e = std::make_unique<VarExpr>(t.line, t.col);
                e->name = t.lexeme;
                return e;
            }
            case Tok::LParen: {
                advance();
                ExprPtr inner = parseExpr();
                expect(Tok::RParen, "')'");
                return inner;
            }
            default:
                fail(t, "expected an expression, found " + describe(t));
        }
    }
};

// Semantic Analysis

class Sema {
public:
    struct Symbol { TypeInfo type; int line; };

    explicit Sema(Diagnostics& diag) : diag_(diag) {}

    void run(Block& program) { checkBlock(program); }

    const std::vector<std::pair<std::string, TypeInfo>>& variables() const { return order_; }
    const std::unordered_map<std::string, Symbol>& symbols() const { return symbols_; }

    void setExistingSymbols(const std::unordered_map<std::string, Symbol>& syms,
                            const std::vector<std::pair<std::string, TypeInfo>>& order) {
        symbols_ = syms;
        order_ = order;
    }

private:
    Diagnostics& diag_;
    std::unordered_map<std::string, Symbol> symbols_;
    std::vector<std::pair<std::string, TypeInfo>> order_;

    void err(int line, int col, const std::string& msg) { diag_.error(line, col, msg); }

    Symbol* lookup(const std::string& name) {
        auto it = symbols_.find(name);
        return it == symbols_.end() ? nullptr : &it->second;
    }

    static bool assignable(const TypeInfo& to, const TypeInfo& from) {
        if (to.isArray || from.isArray) return to == from;
        if (to.base == from.base) return true;
        return to.base == BaseType::Real && from.base == BaseType::Integer;
    }

    void checkBlock(Block& block) { for (auto& s : block) checkStmt(*s); }

    // Enforce that expected types are strictly SCALARS (Fix #7)
    void requireType(const Expr& e, const TypeInfo& t, BaseType want, const std::string& what) {
        if (t.isArray || (t.base != want && t.base != BaseType::Error)) {
            err(e.line, e.col, what + " must be a scalar " + baseTypeName(want) +
            ", found " + typeString(t));
        }
    }

    void checkStmt(Stmt& s) {
        switch (s.kind) {
            case Stmt::Kind::Declare: {
                auto& d = static_cast<DeclareStmt&>(s);
                if (Symbol* prev = lookup(d.name)) {
                    err(d.line, d.col, "'" + d.name + "' is already declared (line " +
                    std::to_string(prev->line) + ")");
                } else {
                    if (d.declaredType.isArray) {
                        if (d.declaredType.lower1 > d.declaredType.upper1)
                            err(d.line, d.col, "invalid array bounds: lower bound > upper bound");
                        if (d.declaredType.dims == 2 && d.declaredType.lower2 > d.declaredType.upper2)
                            err(d.line, d.col, "invalid second-dimension array bounds: lower bound > upper bound");
                    }
                    symbols_[d.name] = Symbol{d.declaredType, d.line};
                    order_.emplace_back(d.name, d.declaredType);
                }
                break;
            }
            case Stmt::Kind::Assign: {
                auto& a = static_cast<AssignStmt&>(s);
                Symbol* sym = lookup(a.name);
                if (!sym) err(a.line, a.col, "'" + a.name + "' is not declared");
                TypeInfo value = typeOf(*a.value);
                if (sym && sym->type.isArray) {
                    err(a.line, a.col, "cannot assign directly to array '" + a.name + "'; index required");
                } else if (sym && value.base != BaseType::Error && !assignable(sym->type, value)) {
                    err(a.line, a.col, "cannot assign " + typeString(value) + " to " +
                    typeString(sym->type) + " variable '" + a.name + "'");
                }
                break;
            }
            case Stmt::Kind::ArrayAssign: {
                auto& a = static_cast<ArrayAssignStmt&>(s);
                Symbol* sym = lookup(a.name);
                if (!sym) {
                    err(a.line, a.col, "'" + a.name + "' is not declared");
                } else if (!sym->type.isArray) {
                    err(a.line, a.col, "'" + a.name + "' is not an array");
                } else {
                    if (static_cast<int>(a.indices.size()) != sym->type.dims) {
                        err(a.line, a.col, "array '" + a.name + "' expects " +
                        std::to_string(sym->type.dims) + " indices, found " +
                        std::to_string(a.indices.size()));
                    }
                    for (auto& idx : a.indices) {
                        requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
                    }
                }
                TypeInfo val = typeOf(*a.value);
                if (sym && sym->type.isArray && val.base != BaseType::Error) {
                    TypeInfo elemType{sym->type.base, false, 0, 0, 0, 0, 0};
                    if (!assignable(elemType, val)) {
                        err(a.line, a.col, "cannot assign " + typeString(val) + " to " +
                        baseTypeName(sym->type.base) + " array element");
                    }
                }
                break;
            }
            case Stmt::Kind::Output: {
                for (auto& arg : static_cast<OutputStmt&>(s).args) {
                    TypeInfo t = typeOf(*arg);
                    if (t.isArray) err(arg->line, arg->col, "cannot output an entire array");
                }
                break;
            }
            case Stmt::Kind::Input: {
                auto& in = static_cast<InputStmt&>(s);
                Symbol* sym = lookup(in.name);
                if (!sym) {
                    err(in.line, in.col, "'" + in.name + "' is not declared");
                } else if (sym->type.isArray) {
                    if (in.indices.empty()) {
                        err(in.line, in.col, "cannot INPUT into whole array '" + in.name + "'");
                    } else {
                        if (static_cast<int>(in.indices.size()) != sym->type.dims) {
                            err(in.line, in.col, "array '" + in.name + "' expects " +
                            std::to_string(sym->type.dims) + " indices, found " +
                            std::to_string(in.indices.size()));
                        }
                        for (auto& idx : in.indices) {
                            requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
                        }
                    }
                } else if (!in.indices.empty()) {
                    err(in.line, in.col, "'" + in.name + "' is not an array");
                }
                break;
            }
            case Stmt::Kind::If: {
                auto& i = static_cast<IfStmt&>(s);
                requireType(*i.cond, typeOf(*i.cond), BaseType::Boolean, "IF condition");
                checkBlock(i.thenBlock);
                checkBlock(i.elseBlock);
                break;
            }
            case Stmt::Kind::While: {
                auto& w = static_cast<WhileStmt&>(s);
                requireType(*w.cond, typeOf(*w.cond), BaseType::Boolean, "WHILE condition");
                checkBlock(w.body);
                break;
            }
            case Stmt::Kind::For: {
                auto& f = static_cast<ForStmt&>(s);
                Symbol* sym = lookup(f.var);
                if (!sym)
                    err(f.line, f.col, "'" + f.var + "' is not declared");
                else if (sym->type.isArray || sym->type.base != BaseType::Integer)
                    err(f.line, f.col, "FOR variable '" + f.var + "' must be INTEGER");

                requireType(*f.start, typeOf(*f.start), BaseType::Integer, "FOR start value");
                requireType(*f.end, typeOf(*f.end), BaseType::Integer, "FOR end value");
                if (f.step) {
                    requireType(*f.step, typeOf(*f.step), BaseType::Integer, "STEP value");
                    if (f.step->kind == Expr::Kind::Literal) {
                        auto& lit = static_cast<LiteralExpr&>(*f.step);
                        if (lit.litType == Tok::IntLit && std::stoll(lit.text) == 0) {
                            err(f.step->line, f.step->col, "STEP value cannot be 0");
                        }
                    }
                }
                checkBlock(f.body);
                break;
            }
        }
    }

    TypeInfo typeOf(Expr& e) {
        e.type = computeType(e);
        return e.type;
    }

    TypeInfo computeType(Expr& e) {
        TypeInfo errType{BaseType::Error, false, 0, 0, 0, 0, 0};
        switch (e.kind) {
            case Expr::Kind::Literal: {
                switch (static_cast<LiteralExpr&>(e).litType) {
                    case Tok::IntLit:  return {BaseType::Integer, false, 0, 0, 0, 0, 0};
                    case Tok::RealLit: return {BaseType::Real, false, 0, 0, 0, 0, 0};
                    case Tok::StrLit:  return {BaseType::String, false, 0, 0, 0, 0, 0};
                    default:           return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
                }
            }
                    case Expr::Kind::Var: {
                        auto& v = static_cast<VarExpr&>(e);
                        Symbol* sym = lookup(v.name);
                        if (!sym) {
                            err(v.line, v.col, "'" + v.name + "' is not declared");
                            return errType;
                        }
                        return sym->type;
                    }
                    case Expr::Kind::ArrayAccess: {
                        auto& a = static_cast<ArrayAccessExpr&>(e);
                        Symbol* sym = lookup(a.name);
                        if (!sym) {
                            err(a.line, a.col, "'" + a.name + "' is not declared");
                            return errType;
                        }
                        if (!sym->type.isArray) {
                            err(a.line, a.col, "'" + a.name + "' is not an array");
                            return errType;
                        }
                        if (static_cast<int>(a.indices.size()) != sym->type.dims) {
                            err(a.line, a.col, "array '" + a.name + "' expects " +
                            std::to_string(sym->type.dims) + " indices, found " +
                            std::to_string(a.indices.size()));
                        }
                        for (auto& idx : a.indices) {
                            requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
                        }
                        return {sym->type.base, false, 0, 0, 0, 0, 0};
                    }
                    case Expr::Kind::Unary: {
                        auto& u = static_cast<UnaryExpr&>(e);
                        TypeInfo t = typeOf(*u.operand);
                        if (t.base == BaseType::Error) return u.op == Tok::Not ? TypeInfo{BaseType::Boolean, false, 0, 0, 0, 0, 0} : errType;
                        if (u.op == Tok::Minus) {
                            if (isNumeric(t)) return t;
                            err(u.line, u.col, std::string("unary '-' needs a numeric operand, found ") + typeString(t));
                            return errType;
                        }
                        if (t.base == BaseType::Boolean && !t.isArray) return t;
                        err(u.line, u.col, std::string("'NOT' needs a BOOLEAN operand, found ") + typeString(t));
                        return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
                    }
                    case Expr::Kind::Binary:
                        return computeBinary(static_cast<BinaryExpr&>(e));
                    case Expr::Kind::Call:
                        return computeCall(static_cast<CallExpr&>(e));
        }
        return errType;
    }

    TypeInfo computeCall(CallExpr& c) {
        TypeInfo errType{BaseType::Error, false, 0, 0, 0, 0, 0};
        auto checkArgCount = [&](size_t expected) -> bool {
            if (c.args.size() != expected) {
                err(c.line, c.col, "function expects " + std::to_string(expected) +
                " arguments, found " + std::to_string(c.args.size()));
                return false;
            }
            return true;
        };

        switch (c.func) {
            case Tok::Length:
                if (checkArgCount(1)) {
                    requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "LENGTH argument");
                }
                return {BaseType::Integer, false, 0, 0, 0, 0, 0};
            case Tok::Substring:
                if (checkArgCount(3)) {
                    requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "SUBSTRING argument 1");
                    requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "SUBSTRING argument 2 (start)");
                    requireType(*c.args[2], typeOf(*c.args[2]), BaseType::Integer, "SUBSTRING argument 3 (length)");
                }
                return {BaseType::String, false, 0, 0, 0, 0, 0};
            case Tok::UCase:
            case Tok::LCase:
                if (checkArgCount(1)) {
                    requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "string function argument");
                }
                return {BaseType::String, false, 0, 0, 0, 0, 0};
            case Tok::NumToStr:
                if (checkArgCount(1)) {
                    TypeInfo t = typeOf(*c.args[0]);
                    if (!isNumeric(t)) {
                        err(c.line, c.col, "NUM_TO_STR requires a numeric argument, found " + typeString(t));
                    }
                }
                return {BaseType::String, false, 0, 0, 0, 0, 0};
            case Tok::StrToNum:
                if (checkArgCount(1)) {
                    requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "STR_TO_NUM argument");
                }
                return {BaseType::Real, false, 0, 0, 0, 0, 0};
            default:
                return errType;
        }
    }

    TypeInfo computeBinary(BinaryExpr& b) {
        TypeInfo l = typeOf(*b.lhs);
        TypeInfo r = typeOf(*b.rhs);
        bool boolResult = isComparison(b.op) || b.op == Tok::And || b.op == Tok::Or;
        TypeInfo errResult = boolResult ? TypeInfo{BaseType::Boolean, false, 0, 0, 0, 0, 0}
        : TypeInfo{BaseType::Error, false, 0, 0, 0, 0, 0};

        if (l.base == BaseType::Error || r.base == BaseType::Error) return errResult;
        if (l.isArray || r.isArray) {
            err(b.line, b.col, "cannot apply operators to whole array types");
            return errResult;
        }

        auto bad = [&](const char* need) {
            err(b.line, b.col, "operator '" + opName(b.op) + "' needs " + need +
            " operands, found " + typeString(l) + " and " + typeString(r));
            return errResult;
        };

        if (b.op == Tok::Ampersand) {
            if (l.base != BaseType::String || r.base != BaseType::String) return bad("STRING");
            return {BaseType::String, false, 0, 0, 0, 0, 0};
        }

        switch (b.op) {
            case Tok::Plus: case Tok::Minus: case Tok::Star:
                if (!isNumeric(l) || !isNumeric(r)) return bad("numeric");
                return (l.base == BaseType::Real || r.base == BaseType::Real)
                ? TypeInfo{BaseType::Real, false, 0, 0, 0, 0, 0}
                : TypeInfo{BaseType::Integer, false, 0, 0, 0, 0, 0};
            case Tok::Slash:
                if (!isNumeric(l) || !isNumeric(r)) return bad("numeric");
                return {BaseType::Real, false, 0, 0, 0, 0, 0};
            case Tok::Div: case Tok::Mod:
                if (l.base != BaseType::Integer || r.base != BaseType::Integer) return bad("INTEGER");
                return {BaseType::Integer, false, 0, 0, 0, 0, 0};
            case Tok::And: case Tok::Or:
                if (l.base != BaseType::Boolean || r.base != BaseType::Boolean) return bad("BOOLEAN");
                return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            case Tok::Eq: case Tok::Neq:
                if ((isNumeric(l) && isNumeric(r)) || l.base == r.base)
                    return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            err(b.line, b.col, "cannot compare " + typeString(l) + " with " + typeString(r));
            return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            default: // < <= > >=
                if ((isNumeric(l) && isNumeric(r)) || (l.base == BaseType::String && r.base == BaseType::String))
                    return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            return bad("numeric or STRING");
        }
    }
};

// Code Generation

class CodeGen {
public:
    explicit CodeGen(const std::vector<std::pair<std::string, TypeInfo>>& vars) : vars_(vars) {
        for (const auto& v : vars) varMap_[v.first] = v.second;
    }

    std::string generate(const Block& program) {
        emitRuntimeHeaders();

        // Large arrays are emitted as file-scope statics to avoid stack overflow (Fix #3)
        bool hasArrays = false;
        for (const auto& v : vars_) {
            if (v.second.isArray) {
                hasArrays = true;
                long long n1 = (v.second.upper1 - v.second.lower1 + 1);
                if (v.second.dims == 1) {
                    out_ << "static " << cBaseType(v.second.base) << " " << cName(v.first)
                    << "[" << n1 << "];\n";
                } else {
                    long long n2 = (v.second.upper2 - v.second.lower2 + 1);
                    out_ << "static " << cBaseType(v.second.base) << " " << cName(v.first)
                    << "[" << n1 << "][" << n2 << "];\n";
                }
            }
        }
        if (hasArrays) out_ << "\n";

        out_ << "int main(void) {\n";
        indent_ = 1;

        // Scalar declarations inside main()
        for (const auto& v : vars_) {
            if (!v.second.isArray) {
                line(cBaseType(v.second.base) + " " + cName(v.first) + " = " + zeroValue(v.second.base) + ";");
            }
        }
        if (!vars_.empty()) out_ << "\n";

        emitBlock(program);
        line("pc_cleanup();");
        line("return 0;");
        out_ << "}\n";
        return out_.str();
    }

private:
    const std::vector<std::pair<std::string, TypeInfo>>& vars_;
    std::unordered_map<std::string, TypeInfo> varMap_;
    std::ostringstream out_;
    int indent_ = 0;
    int tempCounter_ = 0;

    void line(const std::string& text) { out_ << std::string(indent_ * 4, ' ') << text << "\n"; }

    static std::string cBaseType(BaseType t) {
        switch (t) {
            case BaseType::Integer: return "long long";
            case BaseType::Real:    return "double";
            case BaseType::Boolean: return "bool";
            default:                return "const char*";
        }
    }

    static std::string zeroValue(BaseType t) {
        switch (t) {
            case BaseType::Integer: return "0";
            case BaseType::Real:    return "0.0";
            case BaseType::Boolean: return "false";
            default:                return "\"\"";
        }
    }

    static std::string cName(const std::string& name) { return "pc_" + name; }

    static std::string escapeCStr(const std::string& s) {
        std::string out = "\"";
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                default:   out += c; break;
            }
        }
        out += "\"";
        return out;
    }

    void emitRuntimeHeaders() {
        out_ << "#include <stdio.h>\n"
        << "#include <stdlib.h>\n"
        << "#include <stdbool.h>\n"
        << "#include <string.h>\n"
        << "#include <ctype.h>\n"
        << "#include <strings.h>\n\n"
        << "// --- Pseudoc Runtime Core & GC ---\n"
        << "typedef struct PC_Node { void* ptr; struct PC_Node* next; } PC_Node;\n"
        << "static PC_Node* pc_gc_head = NULL;\n"
        << "static void* pc_track(void* p) {\n"
        << "    if (!p) return NULL;\n"
        << "    PC_Node* n = (PC_Node*)malloc(sizeof(PC_Node));\n"
        << "    n->ptr = p; n->next = pc_gc_head; pc_gc_head = n;\n"
        << "    return p;\n"
        << "}\n"
        << "static void pc_cleanup(void) {\n"
        << "    while (pc_gc_head) {\n"
        << "        PC_Node* next = pc_gc_head->next;\n"
        << "        free(pc_gc_head->ptr);\n"
        << "        free(pc_gc_head);\n"
        << "        pc_gc_head = next;\n"
        << "    }\n"
        << "}\n"
        << "// --- Line-based unified Input Runtime (Fix #1 & #2) ---\n"
        << "static char pc_input_buf[4096];\n"
        << "static char* pc_read_line(void) {\n"
        << "    if (!fgets(pc_input_buf, sizeof(pc_input_buf), stdin)) {\n"
        << "        pc_input_buf[0] = '\\0';\n"
        << "    } else {\n"
        << "        size_t len = strlen(pc_input_buf);\n"
        << "        while (len > 0 && (pc_input_buf[len - 1] == '\\n' || pc_input_buf[len - 1] == '\\r')) {\n"
        << "            pc_input_buf[--len] = '\\0';\n"
        << "        }\n"
        << "    }\n"
        << "    return pc_input_buf;\n"
        << "}\n"
        << "static long long pc_read_int(void) {\n"
        << "    char* line = pc_read_line();\n"
        << "    char* end;\n"
        << "    long long val = strtoll(line, &end, 10);\n"
        << "    return (end == line) ? 0LL : val;\n"
        << "}\n"
        << "static double pc_read_real(void) {\n"
        << "    char* line = pc_read_line();\n"
        << "    char* end;\n"
        << "    double val = strtod(line, &end);\n"
        << "    return (end == line) ? 0.0 : val;\n"
        << "}\n"
        << "static bool pc_read_bool(void) {\n"
        << "    char* line = pc_read_line();\n"
        << "    while (*line && isspace((unsigned char)*line)) line++;\n"
        << "    if (strcasecmp(line, \"TRUE\") == 0 || strcmp(line, \"1\") == 0) return true;\n"
        << "    return false;\n"
        << "}\n"
        << "// --- Overflow-checked integer arithmetic (Fix #8) ---\n"
        << "static inline long long pc_add(long long a, long long b) {\n"
        << "    long long res;\n"
        << "    if (__builtin_add_overflow(a, b, &res)) {\n"
        << "        fprintf(stderr, \"Runtime Error: 64-bit integer addition overflow\\n\");\n"
        << "        pc_cleanup(); exit(1);\n"
        << "    }\n"
        << "    return res;\n"
        << "}\n"
        << "static inline long long pc_sub(long long a, long long b) {\n"
        << "    long long res;\n"
        << "    if (__builtin_sub_overflow(a, b, &res)) {\n"
        << "        fprintf(stderr, \"Runtime Error: 64-bit integer subtraction overflow\\n\");\n"
        << "        pc_cleanup(); exit(1);\n"
        << "    }\n"
        << "    return res;\n"
        << "}\n"
        << "static inline long long pc_mul(long long a, long long b) {\n"
        << "    long long res;\n"
        << "    if (__builtin_mul_overflow(a, b, &res)) {\n"
        << "        fprintf(stderr, \"Runtime Error: 64-bit integer multiplication overflow\\n\");\n"
        << "        pc_cleanup(); exit(1);\n"
        << "    }\n"
        << "    return res;\n"
        << "}\n"
        << "// --- Helpers for array indexing, strings, and division ---\n"
        << "static inline void pc_bounds_check(long long val, long long low, long long high, const char* name) {\n"
        << "    if (val < low || val > high) {\n"
        << "        fprintf(stderr, \"Runtime Error: Array index out of bounds on '%s': index %lld not in [%lld:%lld]\\n\", name, val, low, high);\n"
        << "        pc_cleanup(); exit(1);\n"
        << "    }\n"
        << "}\n"
        << "static inline long long pc_div(long long a, long long b) {\n"
        << "    if (b == 0) { fprintf(stderr, \"Runtime Error: Division by zero\\n\"); pc_cleanup(); exit(1); }\n"
        << "    long long q = a / b, r = a % b;\n"
        << "    if ((r != 0) && ((r < 0) ^ (b < 0))) q--;\n"
        << "    return q;\n"
        << "}\n"
        << "static inline long long pc_mod(long long a, long long b) {\n"
        << "    if (b == 0) { fprintf(stderr, \"Runtime Error: Modulo by zero\\n\"); pc_cleanup(); exit(1); }\n"
        << "    long long r = a % b;\n"
        << "    if ((r != 0) && ((r < 0) ^ (b < 0))) r += b;\n"
        << "    return r;\n"
        << "}\n"
        << "static char* pc_concat(const char* s1, const char* s2) {\n"
        << "    size_t l1 = strlen(s1), l2 = strlen(s2);\n"
        << "    char* res = (char*)malloc(l1 + l2 + 1);\n"
        << "    memcpy(res, s1, l1); memcpy(res + l1, s2, l2); res[l1 + l2] = '\\0';\n"
        << "    return (char*)pc_track(res);\n"
        << "}\n"
        << "static char* pc_substring(const char* s, long long start, long long len) {\n"
        << "    long long slen = (long long)strlen(s);\n"
        << "    if (start < 1) start = 1;\n"
        << "    if (len < 0) len = 0;\n"
        << "    if (start > slen) return (char*)pc_track(strdup(\"\"));\n"
        << "    if (start - 1 + len > slen) len = slen - (start - 1);\n"
        << "    char* sub = (char*)malloc(len + 1);\n"
        << "    memcpy(sub, s + (start - 1), len);\n"
        << "    sub[len] = '\\0';\n"
        << "    return (char*)pc_track(sub);\n"
        << "}\n"
        << "static char* pc_ucase(const char* s) {\n"
        << "    size_t len = strlen(s);\n"
        << "    char* r = (char*)malloc(len + 1);\n"
        << "    for (size_t i = 0; i < len; ++i) r[i] = toupper((unsigned char)s[i]);\n"
        << "    r[len] = '\\0';\n"
        << "    return (char*)pc_track(r);\n"
        << "}\n"
        << "static char* pc_lcase(const char* s) {\n"
        << "    size_t len = strlen(s);\n"
        << "    char* r = (char*)malloc(len + 1);\n"
        << "    for (size_t i = 0; i < len; ++i) r[i] = tolower((unsigned char)s[i]);\n"
        << "    r[len] = '\\0';\n"
        << "    return (char*)pc_track(r);\n"
        << "}\n"
        << "static char* pc_num_to_str_int(long long n) {\n"
        << "    char buf[64]; snprintf(buf, sizeof(buf), \"%lld\", n);\n"
        << "    return (char*)pc_track(strdup(buf));\n"
        << "}\n"
        << "static char* pc_num_to_str_real(double d) {\n"
        << "    char buf[64]; snprintf(buf, sizeof(buf), \"%.10g\", d);\n"
        << "    return (char*)pc_track(strdup(buf));\n"
        << "}\n"
        << "static double pc_str_to_num(const char* s) { return atof(s); }\n\n";
    }

    void emitBlock(const Block& block) { for (const auto& s : block) emitStmt(*s); }

    void emitIndented(const Block& block) {
        ++indent_;
        emitBlock(block);
        --indent_;
    }

    std::string arrayOffset(const std::string& name, const std::vector<ExprPtr>& indices) {
        const TypeInfo& info = varMap_[name];
        std::string s;
        if (info.dims == 1) {
            std::string i1 = expr(*indices[0]);
            s += "[(pc_bounds_check(" + i1 + ", " + std::to_string(info.lower1) + ", " +
            std::to_string(info.upper1) + ", \"" + name + "\"), (" + i1 + " - " +
            std::to_string(info.lower1) + "))]";
        } else {
            std::string i1 = expr(*indices[0]);
            std::string i2 = expr(*indices[1]);
            s += "[(pc_bounds_check(" + i1 + ", " + std::to_string(info.lower1) + ", " +
            std::to_string(info.upper1) + ", \"" + name + "\"), (" + i1 + " - " +
            std::to_string(info.lower1) + "))]";
            s += "[(pc_bounds_check(" + i2 + ", " + std::to_string(info.lower2) + ", " +
            std::to_string(info.upper2) + ", \"" + name + "\"), (" + i2 + " - " +
            std::to_string(info.lower2) + "))]";
        }
        return s;
    }

    void emitStmt(const Stmt& s) {
        switch (s.kind) {
            case Stmt::Kind::Declare:
                break;
            case Stmt::Kind::Assign: {
                auto& a = static_cast<const AssignStmt&>(s);
                line(cName(a.name) + " = " + expr(*a.value) + ";");
                break;
            }
            case Stmt::Kind::ArrayAssign: {
                auto& a = static_cast<const ArrayAssignStmt&>(s);
                line(cName(a.name) + arrayOffset(a.name, a.indices) + " = " + expr(*a.value) + ";");
                break;
            }
            case Stmt::Kind::Output:
                emitOutput(static_cast<const OutputStmt&>(s));
                break;
            case Stmt::Kind::Input:
                emitInput(static_cast<const InputStmt&>(s));
                break;
            case Stmt::Kind::If: {
                auto& i = static_cast<const IfStmt&>(s);
                line("if (" + expr(*i.cond) + ") {");
                emitIndented(i.thenBlock);
                if (!i.elseBlock.empty()) {
                    line("} else {");
                    emitIndented(i.elseBlock);
                }
                line("}");
                break;
            }
            case Stmt::Kind::While: {
                auto& w = static_cast<const WhileStmt&>(s);
                line("while (" + expr(*w.cond) + ") {");
                emitIndented(w.body);
                line("}");
                break;
            }
            case Stmt::Kind::For:
                emitFor(static_cast<const ForStmt&>(s));
                break;
        }
    }

    void emitOutput(const OutputStmt& o) {
        std::string fmt, args;
        for (const auto& arg : o.args) {
            switch (arg->type.base) {
                case BaseType::Integer:
                    fmt += "%lld";
                    args += ", " + expr(*arg);
                    break;
                case BaseType::Real:
                    fmt += "%.10g";
                    args += ", " + expr(*arg);
                    break;
                case BaseType::Boolean:
                    fmt += "%s";
                    args += ", (" + expr(*arg) + " ? \"TRUE\" : \"FALSE\")";
                    break;
                case BaseType::String:
                    fmt += "%s";
                    args += ", " + expr(*arg);
                    break;
                default:
                    break;
            }
        }
        line("printf(\"" + fmt + "\\n\"" + args + ");");
    }

    void emitInput(const InputStmt& in) {
        std::string target = cName(in.name);
        BaseType b = varMap_[in.name].base;
        if (!in.indices.empty()) target += arrayOffset(in.name, in.indices);

        switch (b) {
            case BaseType::Integer:
                line(target + " = pc_read_int();");
                break;
            case BaseType::Real:
                line(target + " = pc_read_real();");
                break;
            case BaseType::Boolean:
                line(target + " = pc_read_bool();");
                break;
            case BaseType::String:
                line(target + " = (char*)pc_track(strdup(pc_read_line()));");
                break;
            default:
                break;
        }
    }

    void emitFor(const ForStmt& f) {
        int id = ++tempCounter_;
        std::string endVar = "tmp_end_" + std::to_string(id);
        std::string var = cName(f.var);

        line("{");
        ++indent_;
        line("long long " + endVar + " = " + expr(*f.end) + ";");
        if (f.step) {
            std::string stepVar = "tmp_step_" + std::to_string(id);
            line("long long " + stepVar + " = " + expr(*f.step) + ";");
            // Runtime zero-step trap (Fix #4)
            line("if (" + stepVar + " == 0) { "
            "fprintf(stderr, \"Runtime Error: FOR step cannot be zero\\n\"); "
            "pc_cleanup(); exit(1); }");
            line("for (" + var + " = " + expr(*f.start) + "; " + stepVar + " > 0 ? " + var +
            " <= " + endVar + " : " + var + " >= " + endVar + "; " + var + " += " +
            stepVar + ") {");
        } else {
            line("for (" + var + " = " + expr(*f.start) + "; " + var + " <= " + endVar + "; " +
            var + " += 1) {");
        }
        emitIndented(f.body);
        line("}");
        --indent_;
        line("}");
    }

    std::string expr(const Expr& e) {
        switch (e.kind) {
            case Expr::Kind::Literal: {
                auto& l = static_cast<const LiteralExpr&>(e);
                switch (l.litType) {
                    case Tok::IntLit:  return l.text + "LL";
                    case Tok::RealLit: return l.text;
                    case Tok::StrLit:  return escapeCStr(l.text);
                    case Tok::True:    return "true";
                    default:           return "false";
                }
            }
                    case Expr::Kind::Var:
                        return cName(static_cast<const VarExpr&>(e).name);
                    case Expr::Kind::ArrayAccess: {
                        auto& a = static_cast<const ArrayAccessExpr&>(e);
                        return cName(a.name) + arrayOffset(a.name, a.indices);
                    }
                    case Expr::Kind::Unary: {
                        auto& u = static_cast<const UnaryExpr&>(e);
                        return std::string(u.op == Tok::Not ? "(!" : "(-") + expr(*u.operand) + ")";
                    }
                    case Expr::Kind::Binary:
                        return binary(static_cast<const BinaryExpr&>(e));
                    case Expr::Kind::Call:
                        return call(static_cast<const CallExpr&>(e));
        }
        return "";
    }

    std::string call(const CallExpr& c) {
        switch (c.func) {
            case Tok::Length:
                return "((long long)strlen(" + expr(*c.args[0]) + "))";
            case Tok::Substring:
                return "pc_substring(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ", " + expr(*c.args[2]) + ")";
            case Tok::UCase:
                return "pc_ucase(" + expr(*c.args[0]) + ")";
            case Tok::LCase:
                return "pc_lcase(" + expr(*c.args[0]) + ")";
            case Tok::NumToStr:
                if (c.args[0]->type.base == BaseType::Integer)
                    return "pc_num_to_str_int(" + expr(*c.args[0]) + ")";
            return "pc_num_to_str_real(" + expr(*c.args[0]) + ")";
            case Tok::StrToNum:
                return "pc_str_to_num(" + expr(*c.args[0]) + ")";
            default:
                return "";
        }
    }

    std::string binary(const BinaryExpr& b) {
        std::string l = expr(*b.lhs), r = expr(*b.rhs);
        if (b.op == Tok::Ampersand) return "pc_concat(" + l + ", " + r + ")";

        // Check integer overflow for arithmetic expressions (Fix #8)
        if (b.lhs->type.base == BaseType::Integer && b.rhs->type.base == BaseType::Integer) {
            if (b.op == Tok::Plus)  return "pc_add(" + l + ", " + r + ")";
            if (b.op == Tok::Minus) return "pc_sub(" + l + ", " + r + ")";
            if (b.op == Tok::Star)  return "pc_mul(" + l + ", " + r + ")";
        }

        switch (b.op) {
            case Tok::Plus:  return "(" + l + " + " + r + ")";
            case Tok::Minus: return "(" + l + " - " + r + ")";
            case Tok::Star:  return "(" + l + " * " + r + ")";
            case Tok::Slash: return "((double)" + l + " / (double)" + r + ")";
            case Tok::Div:   return "pc_div(" + l + ", " + r + ")";
            case Tok::Mod:   return "pc_mod(" + l + ", " + r + ")";
            case Tok::And:   return "(" + l + " && " + r + ")";
            case Tok::Or:    return "(" + l + " || " + r + ")";
            default: break;
        }

        const char* cop = "==";
        switch (b.op) {
            case Tok::Eq:  cop = "=="; break;
            case Tok::Neq: cop = "!="; break;
            case Tok::Lt:  cop = "<";  break;
            case Tok::Le:  cop = "<="; break;
            case Tok::Gt:  cop = ">";  break;
            case Tok::Ge:  cop = ">="; break;
            default: break;
        }
        if (b.lhs->type.base == BaseType::String) return "(strcmp(" + l + ", " + r + ") " + cop + " 0)";
        return "(" + l + " " + cop + " " + r + ")";
    }
};

// Interactive REPL & Driver

// ============================================================================
// Bytecode Instruction Set, Virtual Machine (VM) & Execution Engine
// ============================================================================

struct ArrayData;

enum class ValueKind { Nil, Int, Real, Bool, String, Array };

struct Value {
    ValueKind kind = ValueKind::Nil;
    int64_t intVal = 0;
    double realVal = 0.0;
    bool boolVal = false;
    std::string strVal;
    std::shared_ptr<ArrayData> arrVal;

    static Value makeNil() { return Value(); }
    static Value makeInt(int64_t v) { Value val; val.kind = ValueKind::Int; val.intVal = v; return val; }
    static Value makeReal(double v) { Value val; val.kind = ValueKind::Real; val.realVal = v; return val; }
    static Value makeBool(bool v) { Value val; val.kind = ValueKind::Bool; val.boolVal = v; return val; }
    static Value makeString(std::string v) { Value val; val.kind = ValueKind::String; val.strVal = std::move(v); return val; }
    static Value makeArray(std::shared_ptr<ArrayData> v) { Value val; val.kind = ValueKind::Array; val.arrVal = std::move(v); return val; }

    bool isNil() const { return kind == ValueKind::Nil; }
    bool isInt() const { return kind == ValueKind::Int; }
    bool isReal() const { return kind == ValueKind::Real; }
    bool isBool() const { return kind == ValueKind::Bool; }
    bool isString() const { return kind == ValueKind::String; }
    bool isArray() const { return kind == ValueKind::Array; }

    int64_t asInt() const { return intVal; }
    double asReal() const { return isInt() ? static_cast<double>(intVal) : realVal; }
    bool asBool() const { return boolVal; }
    const std::string& asString() const { return strVal; }

    void print(std::ostream& os) const {
        switch (kind) {
            case ValueKind::Int:
                os << intVal;
                break;
            case ValueKind::Real: {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.10g", realVal);
                os << buf;
                break;
            }
            case ValueKind::Bool:
                os << (boolVal ? "TRUE" : "FALSE");
                break;
            case ValueKind::String:
                os << strVal;
                break;
            case ValueKind::Array:
                os << "<array>";
                break;
            case ValueKind::Nil:
                os << "<nil>";
                break;
        }
    }
};

struct ArrayData {
    int dims = 1;
    int64_t lower1 = 0, upper1 = 0;
    int64_t lower2 = 0, upper2 = 0;
    BaseType elemType = BaseType::Integer;
    std::vector<Value> data;

    size_t offset1D(int64_t i) const {
        return static_cast<size_t>(i - lower1);
    }
    size_t offset2D(int64_t i1, int64_t i2) const {
        int64_t span2 = upper2 - lower2 + 1;
        return static_cast<size_t>((i1 - lower1) * span2 + (i2 - lower2));
    }
};

enum class OpCode : uint8_t {
    OpConstant,
    OpPop,
    OpDup,
    OpWidenReal,

    // Global variable access
    OpGetGlobal,
    OpSetGlobal,

    // Array operations
    OpGetArray1D,
    OpSetArray1D,
    OpGetArray2D,
    OpSetArray2D,

    // Arithmetic
    OpAdd,
    OpSub,
    OpMul,
    OpDivReal,
    OpDivInt,
    OpMod,
    OpNeg,

    // Strings & built-ins
    OpConcat,
    OpLength,
    OpSubstring,
    OpUCase,
    OpLCase,
    OpNumToStr,
    OpStrToNum,

    // Comparisons
    OpEqual,
    OpNotEqual,
    OpLess,
    OpLessEqual,
    OpGreater,
    OpGreaterEqual,

    // Logic
    OpNot,

    // Jumps & Control flow
    OpJump,
    OpJumpIfFalse,
    OpJumpIfFalseOrPop,
    OpJumpIfTrueOrPop,
    OpCheckStep,
    OpForCheck,
    OpForStep,

    // I/O
    OpPrint,
    OpPrintLn,
    OpReadInt,
    OpReadReal,
    OpReadBool,
    OpReadStr,

    // Program termination
    OpHalt
};

struct Instruction {
    OpCode op;
    int32_t a = 0;
    int32_t b = 0;
    int32_t c = 0;
    int32_t d = 0;
    int line = 0;
};

struct Chunk {
    std::vector<Instruction> code;
    std::vector<Value> constants;
    std::vector<std::pair<std::string, TypeInfo>> varDescs;
};

static const char* opCodeName(OpCode op) {
    switch (op) {
        case OpCode::OpConstant:           return "OP_CONSTANT";
        case OpCode::OpPop:                return "OP_POP";
        case OpCode::OpDup:                return "OP_DUP";
        case OpCode::OpWidenReal:          return "OP_WIDEN_REAL";
        case OpCode::OpGetGlobal:          return "OP_GET_GLOBAL";
        case OpCode::OpSetGlobal:          return "OP_SET_GLOBAL";
        case OpCode::OpGetArray1D:         return "OP_GET_ARRAY_1D";
        case OpCode::OpSetArray1D:         return "OP_SET_ARRAY_1D";
        case OpCode::OpGetArray2D:         return "OP_GET_ARRAY_2D";
        case OpCode::OpSetArray2D:         return "OP_SET_ARRAY_2D";
        case OpCode::OpAdd:                return "OP_ADD";
        case OpCode::OpSub:                return "OP_SUB";
        case OpCode::OpMul:                return "OP_MUL";
        case OpCode::OpDivReal:            return "OP_DIV_REAL";
        case OpCode::OpDivInt:             return "OP_DIV_INT";
        case OpCode::OpMod:                return "OP_MOD";
        case OpCode::OpNeg:                return "OP_NEG";
        case OpCode::OpConcat:             return "OP_CONCAT";
        case OpCode::OpLength:             return "OP_LENGTH";
        case OpCode::OpSubstring:          return "OP_SUBSTRING";
        case OpCode::OpUCase:              return "OP_UCASE";
        case OpCode::OpLCase:              return "OP_LCASE";
        case OpCode::OpNumToStr:           return "OP_NUM_TO_STR";
        case OpCode::OpStrToNum:           return "OP_STR_TO_NUM";
        case OpCode::OpEqual:              return "OP_EQUAL";
        case OpCode::OpNotEqual:           return "OP_NOT_EQUAL";
        case OpCode::OpLess:               return "OP_LESS";
        case OpCode::OpLessEqual:          return "OP_LESS_EQUAL";
        case OpCode::OpGreater:            return "OP_GREATER";
        case OpCode::OpGreaterEqual:       return "OP_GREATER_EQUAL";
        case OpCode::OpNot:                return "OP_NOT";
        case OpCode::OpJump:               return "OP_JUMP";
        case OpCode::OpJumpIfFalse:        return "OP_JUMP_IF_FALSE";
        case OpCode::OpJumpIfFalseOrPop:   return "OP_JUMP_IF_FALSE_OR_POP";
        case OpCode::OpJumpIfTrueOrPop:    return "OP_JUMP_IF_TRUE_OR_POP";
        case OpCode::OpCheckStep:          return "OP_CHECK_STEP";
        case OpCode::OpForCheck:           return "OP_FOR_CHECK";
        case OpCode::OpForStep:            return "OP_FOR_STEP";
        case OpCode::OpPrint:              return "OP_PRINT";
        case OpCode::OpPrintLn:            return "OP_PRINT_LN";
        case OpCode::OpReadInt:            return "OP_READ_INT";
        case OpCode::OpReadReal:           return "OP_READ_REAL";
        case OpCode::OpReadBool:           return "OP_READ_BOOL";
        case OpCode::OpReadStr:            return "OP_READ_STR";
        case OpCode::OpHalt:               return "OP_HALT";
    }
    return "OP_UNKNOWN";
}

static void printInstruction(std::ostream& os, const Chunk& chunk, size_t ip) {
    const Instruction& inst = chunk.code[ip];
    char buf[128];
    snprintf(buf, sizeof(buf), "%04zu   %4d  ", ip, inst.line);
    os << buf;
    os << std::left << std::setw(25) << opCodeName(inst.op);
    switch (inst.op) {
        case OpCode::OpConstant:
            os << " #" << inst.a << " (";
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.constants.size())) {
                if (chunk.constants[inst.a].isString()) os << "\"" << chunk.constants[inst.a].asString() << "\"";
                else chunk.constants[inst.a].print(os);
            }
            os << ")";
            break;
        case OpCode::OpGetGlobal:
        case OpCode::OpSetGlobal:
            os << " slot:" << inst.a;
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.varDescs.size()))
                os << " (" << chunk.varDescs[inst.a].first << ")";
            break;
        case OpCode::OpGetArray1D:
        case OpCode::OpSetArray1D:
        case OpCode::OpGetArray2D:
        case OpCode::OpSetArray2D:
            os << " slot:" << inst.a;
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.varDescs.size()))
                os << " (" << chunk.varDescs[inst.a].first << ")";
            break;
        case OpCode::OpJump:
        case OpCode::OpJumpIfFalse:
        case OpCode::OpJumpIfFalseOrPop:
        case OpCode::OpJumpIfTrueOrPop:
            os << " -> " << inst.a;
            break;
        case OpCode::OpCheckStep:
            os << " slot:" << inst.a;
            break;
        case OpCode::OpForCheck:
            os << " var:" << inst.a << " end:" << inst.b << " step:" << inst.c << " exit->" << inst.d;
            break;
        case OpCode::OpForStep:
            os << " var:" << inst.a << " step:" << inst.b;
            break;
        default:
            break;
    }
    os << "\n";
}

static void dumpBytecode(const Chunk& chunk, const std::string& name, std::ostream& os = std::cout) {
    os << "== Disassembly: " << name << " ==\n";
    os << "Constants (" << chunk.constants.size() << "):\n";
    for (size_t i = 0; i < chunk.constants.size(); ++i) {
        os << "  #" << i << ": ";
        if (chunk.constants[i].isString()) os << "\"" << chunk.constants[i].asString() << "\"\n";
        else { chunk.constants[i].print(os); os << "\n"; }
    }
    os << "Variables (" << chunk.varDescs.size() << "):\n";
    for (size_t i = 0; i < chunk.varDescs.size(); ++i) {
        os << "  slot " << i << ": " << chunk.varDescs[i].first << " (" << typeString(chunk.varDescs[i].second) << ")\n";
    }
    os << "Instructions (" << chunk.code.size() << "):\n";
    os << "IP     LINE  OPCODE                    OPERANDS\n";
    os << "--------------------------------------------------------\n";
    for (size_t ip = 0; ip < chunk.code.size(); ++ip) {
        printInstruction(os, chunk, ip);
    }
    os << "========================================================\n";
}

class BytecodeCompiler {
public:
    explicit BytecodeCompiler(const std::vector<std::pair<std::string, TypeInfo>>& vars)
        : allVars_(vars) {
        for (size_t i = 0; i < vars.size(); ++i) {
            slotMap_[vars[i].first] = static_cast<int>(i);
            typeMap_[vars[i].first] = vars[i].second;
        }
    }

    Chunk compile(const Block& program) {
        for (const auto& stmt : program) {
            compileStmt(*stmt);
        }
        emit(OpCode::OpHalt, 0, 0, 0, 0, 0);
        chunk_.varDescs = allVars_;
        return std::move(chunk_);
    }

private:
    std::vector<std::pair<std::string, TypeInfo>> allVars_;
    std::unordered_map<std::string, int> slotMap_;
    std::unordered_map<std::string, TypeInfo> typeMap_;
    Chunk chunk_;

    int addConstant(Value v) {
        for (size_t i = 0; i < chunk_.constants.size(); ++i) {
            const auto& c = chunk_.constants[i];
            if (c.kind == v.kind) {
                if (c.isInt() && c.asInt() == v.asInt()) return static_cast<int>(i);
                if (c.isReal() && c.asReal() == v.asReal()) return static_cast<int>(i);
                if (c.isBool() && c.asBool() == v.asBool()) return static_cast<int>(i);
                if (c.isString() && c.asString() == v.asString()) return static_cast<int>(i);
            }
        }
        chunk_.constants.push_back(std::move(v));
        return static_cast<int>(chunk_.constants.size() - 1);
    }

    int emit(OpCode op, int32_t a = 0, int32_t b = 0, int32_t c = 0, int32_t d = 0, int line = 0) {
        chunk_.code.push_back({op, a, b, c, d, line});
        return static_cast<int>(chunk_.code.size() - 1);
    }

    int emitJump(OpCode op, int line) {
        return emit(op, -1, 0, 0, 0, line);
    }

    void patchJump(int jumpInst) {
        chunk_.code[jumpInst].a = static_cast<int32_t>(chunk_.code.size());
    }

    int allocateTempVar(BaseType base) {
        int slot = static_cast<int>(allVars_.size());
        std::string name = "$tmp_" + std::to_string(slot);
        TypeInfo t{base, false, 0, 0, 0, 0, 0};
        allVars_.emplace_back(name, t);
        slotMap_[name] = slot;
        typeMap_[name] = t;
        return slot;
    }

    void compileBlock(const Block& block) {
        for (const auto& s : block) compileStmt(*s);
    }

    void compileStmt(const Stmt& s) {
        switch (s.kind) {
            case Stmt::Kind::Declare:
                break;
            case Stmt::Kind::Assign: {
                auto& a = static_cast<const AssignStmt&>(s);
                compileExpr(*a.value);
                if (typeMap_.at(a.name).base == BaseType::Real && a.value->type.base == BaseType::Integer) {
                    emit(OpCode::OpWidenReal, 0, 0, 0, 0, a.line);
                }
                emit(OpCode::OpSetGlobal, slotMap_.at(a.name), 0, 0, 0, a.line);
                break;
            }
            case Stmt::Kind::ArrayAssign: {
                auto& a = static_cast<const ArrayAssignStmt&>(s);
                compileExpr(*a.value);
                if (typeMap_.at(a.name).base == BaseType::Real && a.value->type.base == BaseType::Integer) {
                    emit(OpCode::OpWidenReal, 0, 0, 0, 0, a.line);
                }
                for (auto& idx : a.indices) {
                    compileExpr(*idx);
                }
                int slot = slotMap_.at(a.name);
                int nameConst = addConstant(Value::makeString(a.name));
                if (a.indices.size() == 1) {
                    emit(OpCode::OpSetArray1D, slot, nameConst, 0, 0, a.line);
                } else {
                    emit(OpCode::OpSetArray2D, slot, nameConst, 0, 0, a.line);
                }
                break;
            }
            case Stmt::Kind::Output: {
                auto& o = static_cast<const OutputStmt&>(s);
                for (const auto& arg : o.args) {
                    compileExpr(*arg);
                    emit(OpCode::OpPrint, 0, 0, 0, 0, arg->line);
                }
                emit(OpCode::OpPrintLn, 0, 0, 0, 0, o.line);
                break;
            }
            case Stmt::Kind::Input: {
                auto& in = static_cast<const InputStmt&>(s);
                int slot = slotMap_.at(in.name);
                BaseType b = typeMap_.at(in.name).base;
                OpCode readOp = OpCode::OpReadInt;
                switch (b) {
                    case BaseType::Integer: readOp = OpCode::OpReadInt; break;
                    case BaseType::Real:    readOp = OpCode::OpReadReal; break;
                    case BaseType::Boolean: readOp = OpCode::OpReadBool; break;
                    case BaseType::String:  readOp = OpCode::OpReadStr; break;
                    default: break;
                }
                if (in.indices.empty()) {
                    emit(readOp, 0, 0, 0, 0, in.line);
                    emit(OpCode::OpSetGlobal, slot, 0, 0, 0, in.line);
                } else {
                    emit(readOp, 0, 0, 0, 0, in.line);
                    for (auto& idx : in.indices) {
                        compileExpr(*idx);
                    }
                    int nameConst = addConstant(Value::makeString(in.name));
                    if (in.indices.size() == 1) {
                        emit(OpCode::OpSetArray1D, slot, nameConst, 0, 0, in.line);
                    } else {
                        emit(OpCode::OpSetArray2D, slot, nameConst, 0, 0, in.line);
                    }
                }
                break;
            }
            case Stmt::Kind::If: {
                auto& i = static_cast<const IfStmt&>(s);
                compileExpr(*i.cond);
                int elseJump = emitJump(OpCode::OpJumpIfFalse, i.line);
                compileBlock(i.thenBlock);
                if (!i.elseBlock.empty()) {
                    int exitJump = emitJump(OpCode::OpJump, i.line);
                    patchJump(elseJump);
                    compileBlock(i.elseBlock);
                    patchJump(exitJump);
                } else {
                    patchJump(elseJump);
                }
                break;
            }
            case Stmt::Kind::While: {
                auto& w = static_cast<const WhileStmt&>(s);
                int loopStart = static_cast<int>(chunk_.code.size());
                compileExpr(*w.cond);
                int exitJump = emitJump(OpCode::OpJumpIfFalse, w.line);
                compileBlock(w.body);
                emit(OpCode::OpJump, loopStart, 0, 0, 0, w.line);
                patchJump(exitJump);
                break;
            }
            case Stmt::Kind::For: {
                auto& f = static_cast<const ForStmt&>(s);
                int varSlot = slotMap_.at(f.var);
                int endSlot = allocateTempVar(BaseType::Integer);
                int stepSlot = allocateTempVar(BaseType::Integer);

                compileExpr(*f.start);
                emit(OpCode::OpSetGlobal, varSlot, 0, 0, 0, f.line);

                compileExpr(*f.end);
                emit(OpCode::OpSetGlobal, endSlot, 0, 0, 0, f.line);

                if (f.step) {
                    compileExpr(*f.step);
                    emit(OpCode::OpSetGlobal, stepSlot, 0, 0, 0, f.step->line);
                } else {
                    int oneConst = addConstant(Value::makeInt(1));
                    emit(OpCode::OpConstant, oneConst, 0, 0, 0, f.line);
                    emit(OpCode::OpSetGlobal, stepSlot, 0, 0, 0, f.line);
                }

                emit(OpCode::OpCheckStep, stepSlot, 0, 0, 0, f.line);

                int loopStart = static_cast<int>(chunk_.code.size());
                int forCheckInst = emit(OpCode::OpForCheck, varSlot, endSlot, stepSlot, -1, f.line);

                compileBlock(f.body);

                emit(OpCode::OpForStep, varSlot, stepSlot, 0, 0, f.line);
                emit(OpCode::OpJump, loopStart, 0, 0, 0, f.line);

                chunk_.code[forCheckInst].d = static_cast<int32_t>(chunk_.code.size());
                break;
            }
        }
    }

    void compileExpr(const Expr& e) {
        switch (e.kind) {
            case Expr::Kind::Literal: {
                auto& lit = static_cast<const LiteralExpr&>(e);
                int cIdx = 0;
                switch (lit.litType) {
                    case Tok::IntLit:
                        cIdx = addConstant(Value::makeInt(std::stoll(lit.text)));
                        break;
                    case Tok::RealLit:
                        cIdx = addConstant(Value::makeReal(std::stod(lit.text)));
                        break;
                    case Tok::StrLit:
                        cIdx = addConstant(Value::makeString(lit.text));
                        break;
                    case Tok::True:
                        cIdx = addConstant(Value::makeBool(true));
                        break;
                    case Tok::False:
                        cIdx = addConstant(Value::makeBool(false));
                        break;
                    default:
                        break;
                }
                emit(OpCode::OpConstant, cIdx, 0, 0, 0, lit.line);
                break;
            }
            case Expr::Kind::Var: {
                auto& v = static_cast<const VarExpr&>(e);
                emit(OpCode::OpGetGlobal, slotMap_.at(v.name), 0, 0, 0, v.line);
                break;
            }
            case Expr::Kind::ArrayAccess: {
                auto& a = static_cast<const ArrayAccessExpr&>(e);
                for (auto& idx : a.indices) {
                    compileExpr(*idx);
                }
                int slot = slotMap_.at(a.name);
                int nameConst = addConstant(Value::makeString(a.name));
                if (a.indices.size() == 1) {
                    emit(OpCode::OpGetArray1D, slot, nameConst, 0, 0, a.line);
                } else {
                    emit(OpCode::OpGetArray2D, slot, nameConst, 0, 0, a.line);
                }
                break;
            }
            case Expr::Kind::Unary: {
                auto& u = static_cast<const UnaryExpr&>(e);
                compileExpr(*u.operand);
                if (u.op == Tok::Minus) {
                    emit(OpCode::OpNeg, 0, 0, 0, 0, u.line);
                } else if (u.op == Tok::Not) {
                    emit(OpCode::OpNot, 0, 0, 0, 0, u.line);
                }
                break;
            }
            case Expr::Kind::Binary: {
                auto& b = static_cast<const BinaryExpr&>(e);
                if (b.op == Tok::And) {
                    compileExpr(*b.lhs);
                    int jump = emitJump(OpCode::OpJumpIfFalseOrPop, b.line);
                    compileExpr(*b.rhs);
                    patchJump(jump);
                    return;
                }
                if (b.op == Tok::Or) {
                    compileExpr(*b.lhs);
                    int jump = emitJump(OpCode::OpJumpIfTrueOrPop, b.line);
                    compileExpr(*b.rhs);
                    patchJump(jump);
                    return;
                }
                compileExpr(*b.lhs);
                compileExpr(*b.rhs);
                switch (b.op) {
                    case Tok::Plus:      emit(OpCode::OpAdd, 0, 0, 0, 0, b.line); break;
                    case Tok::Minus:     emit(OpCode::OpSub, 0, 0, 0, 0, b.line); break;
                    case Tok::Star:      emit(OpCode::OpMul, 0, 0, 0, 0, b.line); break;
                    case Tok::Slash:     emit(OpCode::OpDivReal, 0, 0, 0, 0, b.line); break;
                    case Tok::Div:       emit(OpCode::OpDivInt, 0, 0, 0, 0, b.line); break;
                    case Tok::Mod:       emit(OpCode::OpMod, 0, 0, 0, 0, b.line); break;
                    case Tok::Ampersand: emit(OpCode::OpConcat, 0, 0, 0, 0, b.line); break;
                    case Tok::Eq:        emit(OpCode::OpEqual, 0, 0, 0, 0, b.line); break;
                    case Tok::Neq:       emit(OpCode::OpNotEqual, 0, 0, 0, 0, b.line); break;
                    case Tok::Lt:        emit(OpCode::OpLess, 0, 0, 0, 0, b.line); break;
                    case Tok::Le:        emit(OpCode::OpLessEqual, 0, 0, 0, 0, b.line); break;
                    case Tok::Gt:        emit(OpCode::OpGreater, 0, 0, 0, 0, b.line); break;
                    case Tok::Ge:        emit(OpCode::OpGreaterEqual, 0, 0, 0, 0, b.line); break;
                    default: break;
                }
                break;
            }
            case Expr::Kind::Call: {
                auto& c = static_cast<const CallExpr&>(e);
                for (auto& arg : c.args) {
                    compileExpr(*arg);
                }
                switch (c.func) {
                    case Tok::Length:    emit(OpCode::OpLength, 0, 0, 0, 0, c.line); break;
                    case Tok::Substring: emit(OpCode::OpSubstring, 0, 0, 0, 0, c.line); break;
                    case Tok::UCase:     emit(OpCode::OpUCase, 0, 0, 0, 0, c.line); break;
                    case Tok::LCase:     emit(OpCode::OpLCase, 0, 0, 0, 0, c.line); break;
                    case Tok::NumToStr:  emit(OpCode::OpNumToStr, 0, 0, 0, 0, c.line); break;
                    case Tok::StrToNum:  emit(OpCode::OpStrToNum, 0, 0, 0, 0, c.line); break;
                    default: break;
                }
                break;
            }
        }
    }
};

class VM {
public:
    explicit VM(const std::vector<std::pair<std::string, TypeInfo>>& vars) {
        initGlobals(vars);
    }

    void initGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars) {
        globals_.clear();
        globals_.resize(vars.size());
        for (size_t i = 0; i < vars.size(); ++i) {
            globals_[i] = defaultValue(vars[i].second);
        }
    }

    void syncGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars) {
        if (vars.size() > globals_.size()) {
            size_t oldSize = globals_.size();
            globals_.resize(vars.size());
            for (size_t i = oldSize; i < vars.size(); ++i) {
                globals_[i] = defaultValue(vars[i].second);
            }
        }
    }

    int run(const Chunk& chunk, bool isRepl = false) {
        (void)isRepl;
        syncGlobals(chunk.varDescs);
        size_t ip = 0;
        stack_.clear();

        auto runtimeErr = [&](const std::string& msg, int line) {
            (void)line;
            std::cerr << "Runtime Error: " << msg << "\n";
        };

        while (ip < chunk.code.size()) {
            const Instruction& inst = chunk.code[ip++];
            switch (inst.op) {
                case OpCode::OpHalt:
                    return 0;

                case OpCode::OpConstant:
                    push(chunk.constants[inst.a]);
                    break;

                case OpCode::OpPop:
                    pop();
                    break;

                case OpCode::OpDup:
                    push(peek());
                    break;

                case OpCode::OpWidenReal: {
                    Value v = pop();
                    push(Value::makeReal(v.asReal()));
                    break;
                }

                case OpCode::OpGetGlobal:
                    push(globals_[inst.a]);
                    break;

                case OpCode::OpSetGlobal: {
                    Value v = pop();
                    globals_[inst.a] = v;
                    break;
                }

                case OpCode::OpGetArray1D: {
                    int64_t idx = pop().asInt();
                    const std::string& name = chunk.constants[inst.b].asString();
                    auto& arr = globals_[inst.a].arrVal;
                    if (idx < arr->lower1 || idx > arr->upper1) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                                 name.c_str(), (long long)idx, (long long)arr->lower1, (long long)arr->upper1);
                        runtimeErr(buf, inst.line);
                        return 1;
                    }
                    push(arr->data[arr->offset1D(idx)]);
                    break;
                }

                case OpCode::OpSetArray1D: {
                    int64_t idx = pop().asInt();
                    Value val = pop();
                    const std::string& name = chunk.constants[inst.b].asString();
                    auto& arr = globals_[inst.a].arrVal;
                    if (idx < arr->lower1 || idx > arr->upper1) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                                 name.c_str(), (long long)idx, (long long)arr->lower1, (long long)arr->upper1);
                        runtimeErr(buf, inst.line);
                        return 1;
                    }
                    if (arr->elemType == BaseType::Real && val.isInt()) {
                        val = Value::makeReal(val.asReal());
                    }
                    arr->data[arr->offset1D(idx)] = val;
                    break;
                }

                case OpCode::OpGetArray2D: {
                    int64_t idx2 = pop().asInt();
                    int64_t idx1 = pop().asInt();
                    const std::string& name = chunk.constants[inst.b].asString();
                    auto& arr = globals_[inst.a].arrVal;
                    if (idx1 < arr->lower1 || idx1 > arr->upper1) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                                 name.c_str(), (long long)idx1, (long long)arr->lower1, (long long)arr->upper1);
                        runtimeErr(buf, inst.line);
                        return 1;
                    }
                    if (idx2 < arr->lower2 || idx2 > arr->upper2) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                                 name.c_str(), (long long)idx2, (long long)arr->lower2, (long long)arr->upper2);
                        runtimeErr(buf, inst.line);
                        return 1;
                    }
                    push(arr->data[arr->offset2D(idx1, idx2)]);
                    break;
                }

                case OpCode::OpSetArray2D: {
                    int64_t idx2 = pop().asInt();
                    int64_t idx1 = pop().asInt();
                    Value val = pop();
                    const std::string& name = chunk.constants[inst.b].asString();
                    auto& arr = globals_[inst.a].arrVal;
                    if (idx1 < arr->lower1 || idx1 > arr->upper1) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                                 name.c_str(), (long long)idx1, (long long)arr->lower1, (long long)arr->upper1);
                        runtimeErr(buf, inst.line);
                        return 1;
                    }
                    if (idx2 < arr->lower2 || idx2 > arr->upper2) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                                 name.c_str(), (long long)idx2, (long long)arr->lower2, (long long)arr->upper2);
                        runtimeErr(buf, inst.line);
                        return 1;
                    }
                    if (arr->elemType == BaseType::Real && val.isInt()) {
                        val = Value::makeReal(val.asReal());
                    }
                    arr->data[arr->offset2D(idx1, idx2)] = val;
                    break;
                }

                case OpCode::OpAdd: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isInt() && b.isInt()) {
                        int64_t res;
                        if (__builtin_add_overflow(a.asInt(), b.asInt(), &res)) {
                            runtimeErr("64-bit integer addition overflow", inst.line);
                            return 1;
                        }
                        push(Value::makeInt(res));
                    } else {
                        push(Value::makeReal(a.asReal() + b.asReal()));
                    }
                    break;
                }

                case OpCode::OpSub: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isInt() && b.isInt()) {
                        int64_t res;
                        if (__builtin_sub_overflow(a.asInt(), b.asInt(), &res)) {
                            runtimeErr("64-bit integer subtraction overflow", inst.line);
                            return 1;
                        }
                        push(Value::makeInt(res));
                    } else {
                        push(Value::makeReal(a.asReal() - b.asReal()));
                    }
                    break;
                }

                case OpCode::OpMul: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isInt() && b.isInt()) {
                        int64_t res;
                        if (__builtin_mul_overflow(a.asInt(), b.asInt(), &res)) {
                            runtimeErr("64-bit integer multiplication overflow", inst.line);
                            return 1;
                        }
                        push(Value::makeInt(res));
                    } else {
                        push(Value::makeReal(a.asReal() * b.asReal()));
                    }
                    break;
                }

                case OpCode::OpDivReal: {
                    Value b = pop();
                    Value a = pop();
                    double denom = b.asReal();
                    if (denom == 0.0) {
                        runtimeErr("Division by zero", inst.line);
                        return 1;
                    }
                    push(Value::makeReal(a.asReal() / denom));
                    break;
                }

                case OpCode::OpDivInt: {
                    int64_t b = pop().asInt();
                    int64_t a = pop().asInt();
                    if (b == 0) {
                        runtimeErr("Division by zero", inst.line);
                        return 1;
                    }
                    int64_t q = a / b, r = a % b;
                    if ((r != 0) && ((r < 0) ^ (b < 0))) q--;
                    push(Value::makeInt(q));
                    break;
                }

                case OpCode::OpMod: {
                    int64_t b = pop().asInt();
                    int64_t a = pop().asInt();
                    if (b == 0) {
                        runtimeErr("Modulo by zero", inst.line);
                        return 1;
                    }
                    int64_t r = a % b;
                    if ((r != 0) && ((r < 0) ^ (b < 0))) r += b;
                    push(Value::makeInt(r));
                    break;
                }

                case OpCode::OpNeg: {
                    Value v = pop();
                    if (v.isInt()) {
                        if (v.asInt() == INT64_MIN) {
                            runtimeErr("64-bit integer negation overflow", inst.line);
                            return 1;
                        }
                        push(Value::makeInt(-v.asInt()));
                    } else {
                        push(Value::makeReal(-v.asReal()));
                    }
                    break;
                }

                case OpCode::OpConcat: {
                    Value b = pop();
                    Value a = pop();
                    push(Value::makeString(a.asString() + b.asString()));
                    break;
                }

                case OpCode::OpLength: {
                    Value s = pop();
                    push(Value::makeInt(static_cast<int64_t>(s.asString().size())));
                    break;
                }

                case OpCode::OpSubstring: {
                    int64_t len = pop().asInt();
                    int64_t start = pop().asInt();
                    std::string s = pop().asString();
                    int64_t slen = static_cast<int64_t>(s.size());
                    if (start < 1) start = 1;
                    if (len < 0) len = 0;
                    if (start > slen) {
                        push(Value::makeString(""));
                    } else {
                        int64_t st = start - 1;
                        if (st + len > slen) len = slen - st;
                        push(Value::makeString(s.substr(st, len)));
                    }
                    break;
                }

                case OpCode::OpUCase: {
                    std::string s = pop().asString();
                    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    push(Value::makeString(std::move(s)));
                    break;
                }

                case OpCode::OpLCase: {
                    std::string s = pop().asString();
                    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    push(Value::makeString(std::move(s)));
                    break;
                }

                case OpCode::OpNumToStr: {
                    Value v = pop();
                    if (v.isInt()) {
                        push(Value::makeString(std::to_string(v.asInt())));
                    } else {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%.10g", v.asReal());
                        push(Value::makeString(buf));
                    }
                    break;
                }

                case OpCode::OpStrToNum: {
                    std::string s = pop().asString();
                    push(Value::makeReal(std::atof(s.c_str())));
                    break;
                }

                case OpCode::OpEqual: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isString() && b.isString()) {
                        push(Value::makeBool(a.asString() == b.asString()));
                    } else if (a.isBool() && b.isBool()) {
                        push(Value::makeBool(a.asBool() == b.asBool()));
                    } else {
                        push(Value::makeBool(a.asReal() == b.asReal()));
                    }
                    break;
                }

                case OpCode::OpNotEqual: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isString() && b.isString()) {
                        push(Value::makeBool(a.asString() != b.asString()));
                    } else if (a.isBool() && b.isBool()) {
                        push(Value::makeBool(a.asBool() != b.asBool()));
                    } else {
                        push(Value::makeBool(a.asReal() != b.asReal()));
                    }
                    break;
                }

                case OpCode::OpLess: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isString() && b.isString()) {
                        push(Value::makeBool(a.asString() < b.asString()));
                    } else {
                        push(Value::makeBool(a.asReal() < b.asReal()));
                    }
                    break;
                }

                case OpCode::OpLessEqual: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isString() && b.isString()) {
                        push(Value::makeBool(a.asString() <= b.asString()));
                    } else {
                        push(Value::makeBool(a.asReal() <= b.asReal()));
                    }
                    break;
                }

                case OpCode::OpGreater: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isString() && b.isString()) {
                        push(Value::makeBool(a.asString() > b.asString()));
                    } else {
                        push(Value::makeBool(a.asReal() > b.asReal()));
                    }
                    break;
                }

                case OpCode::OpGreaterEqual: {
                    Value b = pop();
                    Value a = pop();
                    if (a.isString() && b.isString()) {
                        push(Value::makeBool(a.asString() >= b.asString()));
                    } else {
                        push(Value::makeBool(a.asReal() >= b.asReal()));
                    }
                    break;
                }

                case OpCode::OpNot: {
                    Value v = pop();
                    push(Value::makeBool(!v.asBool()));
                    break;
                }

                case OpCode::OpJump:
                    ip = static_cast<size_t>(inst.a);
                    break;

                case OpCode::OpJumpIfFalse: {
                    Value c = pop();
                    if (!c.asBool()) ip = static_cast<size_t>(inst.a);
                    break;
                }

                case OpCode::OpJumpIfFalseOrPop: {
                    if (!peek().asBool()) {
                        ip = static_cast<size_t>(inst.a);
                    } else {
                        pop();
                    }
                    break;
                }

                case OpCode::OpJumpIfTrueOrPop: {
                    if (peek().asBool()) {
                        ip = static_cast<size_t>(inst.a);
                    } else {
                        pop();
                    }
                    break;
                }

                case OpCode::OpCheckStep: {
                    int64_t step = globals_[inst.a].asInt();
                    if (step == 0) {
                        runtimeErr("FOR step cannot be zero", inst.line);
                        return 1;
                    }
                    break;
                }

                case OpCode::OpForCheck: {
                    int64_t varVal = globals_[inst.a].asInt();
                    int64_t endVal = globals_[inst.b].asInt();
                    int64_t stepVal = globals_[inst.c].asInt();
                    if (stepVal > 0 ? (varVal > endVal) : (varVal < endVal)) {
                        ip = static_cast<size_t>(inst.d);
                    }
                    break;
                }

                case OpCode::OpForStep: {
                    int64_t varVal = globals_[inst.a].asInt();
                    int64_t stepVal = globals_[inst.b].asInt();
                    int64_t res;
                    if (__builtin_add_overflow(varVal, stepVal, &res)) {
                        runtimeErr("64-bit integer addition overflow", inst.line);
                        return 1;
                    }
                    globals_[inst.a].intVal = res;
                    break;
                }

                case OpCode::OpPrint: {
                    Value v = pop();
                    v.print(std::cout);
                    break;
                }

                case OpCode::OpPrintLn:
                    std::cout << "\n";
                    break;

                case OpCode::OpReadInt: {
                    std::string line = readLine();
                    char* end = nullptr;
                    long long val = std::strtoll(line.c_str(), &end, 10);
                    push(Value::makeInt(end == line.c_str() ? 0LL : val));
                    break;
                }

                case OpCode::OpReadReal: {
                    std::string line = readLine();
                    char* end = nullptr;
                    double val = std::strtod(line.c_str(), &end);
                    push(Value::makeReal(end == line.c_str() ? 0.0 : val));
                    break;
                }

                case OpCode::OpReadBool: {
                    std::string line = readLine();
                    size_t idx = 0;
                    while (idx < line.size() && std::isspace(static_cast<unsigned char>(line[idx]))) idx++;
                    std::string sub = line.substr(idx);
                    for (char& ch : sub) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    push(Value::makeBool(sub == "TRUE" || sub == "1"));
                    break;
                }

                case OpCode::OpReadStr: {
                    push(Value::makeString(readLine()));
                    break;
                }
            }
        }
        return 0;
    }

private:
    std::vector<Value> globals_;
    std::vector<Value> stack_;

    void push(Value v) { stack_.push_back(std::move(v)); }
    Value pop() {
        Value v = std::move(stack_.back());
        stack_.pop_back();
        return v;
    }
    const Value& peek() const { return stack_.back(); }

    static Value defaultValue(const TypeInfo& t) {
        if (t.isArray) {
            auto arr = std::make_shared<ArrayData>();
            arr->dims = t.dims;
            arr->lower1 = t.lower1;
            arr->upper1 = t.upper1;
            arr->lower2 = t.lower2;
            arr->upper2 = t.upper2;
            arr->elemType = t.base;
            int64_t span1 = t.upper1 - t.lower1 + 1;
            int64_t span2 = (t.dims == 2) ? (t.upper2 - t.lower2 + 1) : 1;
            size_t total = static_cast<size_t>(span1 * span2);
            Value elemDef;
            switch (t.base) {
                case BaseType::Integer: elemDef = Value::makeInt(0); break;
                case BaseType::Real:    elemDef = Value::makeReal(0.0); break;
                case BaseType::Boolean: elemDef = Value::makeBool(false); break;
                case BaseType::String:  elemDef = Value::makeString(""); break;
                default:                elemDef = Value::makeInt(0); break;
            }
            arr->data.assign(total, elemDef);
            return Value::makeArray(arr);
        }
        switch (t.base) {
            case BaseType::Integer: return Value::makeInt(0);
            case BaseType::Real:    return Value::makeReal(0.0);
            case BaseType::Boolean: return Value::makeBool(false);
            case BaseType::String:  return Value::makeString("");
            default:                return Value::makeInt(0);
        }
    }

    static std::string readLine() {
        std::string s;
        if (!std::getline(std::cin, s)) return "";
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
        return s;
    }
};

// ============================================================================
// Interactive REPL & CLI Driver
// ============================================================================

static void usage() {
    std::cerr << "usage: pseudoc [<input.pseudo>] [options]\n\n"
              << "Options:\n"
              << "  -o <output.c>         Compile pseudocode to C and write to <output.c>\n"
              << "  -c, --emit-c          Compile pseudocode and emit C source code to stdout\n"
              << "  -d, --dump-bc         Disassemble and print bytecode without executing\n"
              << "  -i, --repl            Run interactive REPL (powered by VM)\n"
              << "  -h, --help            Show this help message\n\n"
              << "Default behavior:\n"
              << "  pseudoc script.pseudo Execute script directly on the Pseudoc VM\n"
              << "  pseudoc               Start interactive REPL\n";
}

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

static void runRepl() {
    std::cout << "Pseudoc REPL v3.0 (VM-powered, type ':q' or 'EXIT' to quit)\n";
    std::unordered_map<std::string, Sema::Symbol> replSymbols;
    std::vector<std::pair<std::string, TypeInfo>> replVars;
    VM vm({});

    std::string accumulated;
    int blockDepth = 0;

    auto updateBlockDepth = [](const std::string& line, int depth) -> int {
        Diagnostics diag("<check>", line, true);
        std::vector<Token> toks = Lexer(line, diag).tokenize();
        for (const auto& t : toks) {
            if (t.type == Tok::If || t.type == Tok::While || t.type == Tok::For) depth++;
            else if (t.type == Tok::EndIf || t.type == Tok::EndWhile || t.type == Tok::Next) depth = std::max(0, depth - 1);
        }
        return depth;
    };

    while (true) {
        std::cout << (blockDepth > 0 ? "... " : ">>> ");
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (blockDepth == 0 && (line == ":q" || line == "EXIT" || line == "exit")) break;
        if (line.empty() && blockDepth == 0) continue;

        if (!accumulated.empty()) accumulated += "\n";
        accumulated += line;
        blockDepth = updateBlockDepth(line, blockDepth);
        if (blockDepth > 0) continue;

        std::string inputToRun = accumulated;
        accumulated.clear();

        // Check if expression that should be auto-wrapped as OUTPUT
        std::string trialLine = inputToRun;
        {
            Diagnostics testDiag("<repl>", inputToRun, true);
            std::vector<Token> toks = Lexer(inputToRun, testDiag).tokenize();
            Parser parser(toks, testDiag);
            (void)parser.parseProgram();
            if (testDiag.errorCount() > 0) {
                std::string wrapped = "OUTPUT " + inputToRun;
                Diagnostics wrapDiag("<repl>", wrapped, true);
                std::vector<Token> wrapToks = Lexer(wrapped, wrapDiag).tokenize();
                Parser wrapParser(wrapToks, wrapDiag);
                (void)wrapParser.parseProgram();
                if (wrapDiag.errorCount() == 0) trialLine = wrapped;
            }
        }

        Diagnostics trialDiag("<repl>", trialLine);
        std::vector<Token> trialToks = Lexer(trialLine, trialDiag).tokenize();
        Block trialProg = Parser(trialToks, trialDiag).parseProgram();
        if (trialDiag.errorCount() > 0) continue;

        Sema trialSema(trialDiag);
        trialSema.setExistingSymbols(replSymbols, replVars);
        trialSema.run(trialProg);
        if (trialDiag.errorCount() > 0) continue;

        // Commit symbols and update VM variables
        replSymbols = trialSema.symbols();
        replVars = trialSema.variables();
        vm.syncGlobals(replVars);

        // Compile statement(s) to bytecode and run on VM
        Chunk chunk = BytecodeCompiler(replVars).compile(trialProg);
        vm.run(chunk, true);
    }
}

int main(int argc, char** argv) {
    std::string inPath, outPath;
    bool interactive = false;
    bool emitC = false;
    bool dumpBc = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { usage(); return 0; }
        if (arg == "-i" || arg == "--repl") { interactive = true; }
        else if (arg == "-c" || arg == "--emit-c") { emitC = true; }
        else if (arg == "-d" || arg == "--dump-bc" || arg == "--dump-bytecode") { dumpBc = true; }
        else if (arg == "-o" && i + 1 < argc) { outPath = argv[++i]; }
        else if (inPath.empty()) { inPath = arg; }
        else { usage(); return 2; }
    }

    if (interactive || (inPath.empty() && outPath.empty() && !emitC && !dumpBc)) {
        runRepl();
        return 0;
    }

    if (inPath.empty()) {
        usage();
        return 2;
    }

    std::string source;
    if (!readFile(inPath, source)) {
        std::cerr << "pseudoc: cannot open '" << inPath << "'\n";
        return 2;
    }

    Diagnostics diag(inPath, source);
    std::vector<Token> tokens = Lexer(source, diag).tokenize();
    Block program = Parser(tokens, diag).parseProgram();

    Sema sema(diag);
    if (diag.errorCount() == 0) sema.run(program);

    if (diag.errorCount() > 0) {
        std::cerr << diag.errorCount() << (diag.errorCount() == 1 ? " error" : " errors")
                  << " found; no execution or output written.\n";
        return 1;
    }

    // C Code Generation Mode (-o <file.c> or -c / --emit-c)
    if (!outPath.empty() || emitC) {
        std::string c = CodeGen(sema.variables()).generate(program);
        if (outPath.empty()) {
            std::cout << c;
        } else {
            std::ofstream out(outPath);
            if (!out) {
                std::cerr << "pseudoc: cannot write '" << outPath << "'\n";
                return 2;
            }
            out << c;
        }
        return 0;
    }

    // Bytecode Compilation
    Chunk chunk = BytecodeCompiler(sema.variables()).compile(program);

    // Disassembly Mode (-d / --dump-bc)
    if (dumpBc) {
        dumpBytecode(chunk, inPath);
        return 0;
    }

    // Default: Direct execution on the Pseudoc VM
    VM vm(chunk.varDescs);
    return vm.run(chunk);
}
