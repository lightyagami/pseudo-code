// pseudoc.cpp - Full Pseudocode-to-C Compiler & Interactive REPL (v2)
//
// Build:  g++ -std=c++17 -O2 -Wall -Wextra pseudoc.cpp -o pseudoc
// Use:    ./pseudoc program.pseudo -o program.c
//         ./pseudoc -i                           (interactive REPL)
//         gcc program.c -o program && ./program
//
// Pipeline: source -> Lexer -> Parser -> Sema -> CodeGen -> C source

#include <algorithm>
#include <cctype>
#include <fstream>
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
    Diagnostics(std::string file, const std::string& source) : file_(std::move(file)) {
        std::istringstream in(source);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines_.push_back(line);
        }
    }

    void error(int line, int col, const std::string& msg) {
        ++errors_;
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
    {"TRUE", Tok::True}, {"FALSE", Tok::False},
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
        auto it = kKeywords.find(upper);
        add(it == kKeywords.end() ? Tok::Ident : it->second, text, line, col);
    }

    void scanString(int line, int col) {
        std::string text;
        while (!atEnd() && peek() != '"' && peek() != '\n') {
            if (peek() == '\\' && peek(1) != '\0' && peek(1) != '\n') text += advance();
            text += advance();
        }
        if (atEnd() || peek() == '\n') {
            diag_.error(line, col, "unterminated string literal");
            return;
        }
        advance();
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
    std::vector<ExprPtr> indices; // empty if scalar
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
        long long v = std::stoll(t.lexeme);
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
            try { (void)std::stoull(t.lexeme); }
            catch (const std::exception&) { fail(t, "integer literal '" + t.lexeme + "' is too large"); }
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
    explicit Sema(Diagnostics& diag) : diag_(diag) {}

    void run(Block& program) { checkBlock(program); }

    const std::vector<std::pair<std::string, TypeInfo>>& variables() const { return order_; }

private:
    struct Symbol { TypeInfo type; int line; };

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

    void requireType(const Expr& e, const TypeInfo& t, BaseType want, const std::string& what) {
        if (t.base != want && t.base != BaseType::Error)
            err(e.line, e.col, what + " must be " + baseTypeName(want) + ", found " + typeString(t));
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
        out_ << "int main(void) {\n";
        indent_ = 1;

        // Hoist declarations
        for (const auto& v : vars_) {
            if (!v.second.isArray) {
                line(cBaseType(v.second.base) + " " + cName(v.first) + " = " + zeroValue(v.second.base) + ";");
            } else {
                long long n1 = (v.second.upper1 - v.second.lower1 + 1);
                if (v.second.dims == 1) {
                    line(cBaseType(v.second.base) + " " + cName(v.first) + "[" + std::to_string(n1) + "] = {0};");
                } else {
                    long long n2 = (v.second.upper2 - v.second.lower2 + 1);
                    line(cBaseType(v.second.base) + " " + cName(v.first) + "[" + std::to_string(n1) + "][" + std::to_string(n2) + "] = {{0}};");
                }
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
             << "#include <ctype.h>\n\n"
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
             << "static double pc_str_to_num(const char* s) { return atof(s); }\n"
             << "static char* pc_input_string(void) {\n"
             << "    char buf[4096];\n"
             << "    if (!fgets(buf, sizeof(buf), stdin)) return (char*)pc_track(strdup(\"\"));\n"
             << "    size_t len = strlen(buf);\n"
             << "    while (len > 0 && (buf[len-1] == '\\n' || buf[len-1] == '\\r')) buf[--len] = '\\0';\n"
             << "    return (char*)pc_track(strdup(buf));\n"
             << "}\n\n";
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

        if (b == BaseType::String) {
            line(target + " = pc_input_string();");
            return;
        }
        const char* spec = b == BaseType::Integer ? "%lld" : (b == BaseType::Real ? "%lf" : "%d");
        line("if (scanf(\"" + std::string(spec) + "\", &" + target + ") != 1) { " + target + " = 0; }");
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

//Interactive REPL & Driver

static void usage() {
    std::cerr << "usage: pseudoc [<input.pseudo>] [-o output.c] [-i]\n"
                 "  Compiles pseudocode to C. Writes to stdout if -o is omitted.\n"
                 "  Run with -i or no arguments for interactive REPL.\n";
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
    std::cout << "Pseudoc REPL v2 (type ':q' or 'EXIT' to quit)\n";
    std::string sessionCode;
    std::string line;

    while (true) {
        std::cout << ">>> ";
        if (!std::getline(std::cin, line)) break;
        if (line == ":q" || line == "EXIT" || line == "exit") break;
        if (line.empty()) continue;

        // Auto-wrap bare expressions as OUTPUT statements
        std::string testCode = sessionCode + "\n" + line;
        Diagnostics testDiag("<repl>", testCode);
        std::vector<Token> toks = Lexer(testCode, testDiag).tokenize();
        Parser parser(toks, testDiag);
        Block prog = parser.parseProgram();

        if (testDiag.errorCount() > 0) {
            std::string wrapped = "OUTPUT " + line;
            std::string wrapCode = sessionCode + "\n" + wrapped;
            Diagnostics wrapDiag("<repl>", wrapCode);
            std::vector<Token> wrapToks = Lexer(wrapCode, wrapDiag).tokenize();
            Block wrapProg = Parser(wrapToks, wrapDiag).parseProgram();
            if (wrapDiag.errorCount() == 0) {
                line = wrapped;
            }
        }

        std::string trial = sessionCode + "\n" + line;
        Diagnostics trialDiag("<repl>", trial);
        std::vector<Token> trialToks = Lexer(trial, trialDiag).tokenize();
        Block trialProg = Parser(trialToks, trialDiag).parseProgram();
        if (trialDiag.errorCount() > 0) continue;

        Sema trialSema(trialDiag);
        trialSema.run(trialProg);
        if (trialDiag.errorCount() > 0) continue;

        // Trial succeeded: commit line to current session
        sessionCode = trial;

        // Compile and execute immediately
        std::string cSrc = CodeGen(trialSema.variables()).generate(trialProg);
        const char* cPath = "/tmp/pseudoc_repl.c";
        const char* binPath = "/tmp/pseudoc_repl.out";

        std::ofstream outC(cPath);
        outC << cSrc;
        outC.close();

        std::string compileCmd = "gcc " + std::string(cPath) + " -o " + std::string(binPath);
        if (system(compileCmd.c_str()) == 0) {
            (void)system(binPath);
        }
    }
}

int main(int argc, char** argv) {
    std::string inPath, outPath;
    bool interactive = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { usage(); return 0; }
        if (arg == "-i" || arg == "--repl") { interactive = true; }
        else if (arg == "-o" && i + 1 < argc) { outPath = argv[++i]; }
        else if (inPath.empty()) { inPath = arg; }
        else { usage(); return 2; }
    }

    if (interactive || (inPath.empty() && outPath.empty())) {
        runRepl();
        return 0;
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
                  << " found; no output written.\n";
        return 1;
    }

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