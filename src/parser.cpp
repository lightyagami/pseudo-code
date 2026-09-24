#include "parser.h"

#include <exception>
#include <utility>

Parser::Parser(const std::vector<Token>& tokens, Diagnostics& diag)
    : toks_(tokens), diag_(diag) {}

Block Parser::parseProgram() {
    Block program;
    skipNewlines();
    while (!check(Tok::Eof)) {
        if (StmtPtr s = parseStatementSafe()) program.push_back(std::move(s));
        skipNewlines();
    }
    return program;
}

const Token& Parser::peek() const {
    return toks_[cur_];
}

const Token& Parser::previous() const {
    return toks_[cur_ - 1];
}

bool Parser::atEnd() const {
    return check(Tok::Eof);
}

bool Parser::check(Tok type) const {
    return peek().type == type;
}

const Token& Parser::advance() {
    if (!atEnd()) ++cur_;
    return previous();
}

bool Parser::match(Tok type) {
    if (check(type)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(Tok type, const std::string& desc) {
    if (check(type)) return advance();
    fail(peek(), "expected " + desc + ", found " + describe(peek()));
    return peek();
}

void Parser::fail(const Token& t, const std::string& msg) {
    diag_.error(t.line, t.col, msg);
    throw ParseError{};
}

std::string Parser::describe(const Token& t) {
    switch (t.type) {
        case Tok::Newline: return "end of line";
        case Tok::Eof:     return "end of file";
        case Tok::Ident:   return "'" + t.lexeme + "'";
        case Tok::IntLit:  return "integer " + t.lexeme;
        case Tok::RealLit: return "real " + t.lexeme;
        case Tok::StrLit:  return "string literal";
        default:           return "'" + t.lexeme + "'";
    }
}

void Parser::skipNewlines() {
    while (match(Tok::Newline)) {}
}

StmtPtr Parser::parseStatementSafe() {
    try {
        return parseStatement();
    } catch (const ParseError&) {
        while (!check(Tok::Newline) && !check(Tok::Eof)) advance();
        skipNewlines();
        return nullptr;
    }
}

StmtPtr Parser::parseStatement() {
    const Token& t = peek();
    switch (t.type) {
        case Tok::Declare: advance(); return parseDeclare();
        case Tok::Output:  advance(); return parseOutput();
        case Tok::Input:   advance(); return parseInput();
        case Tok::If:      advance(); return parseIf();
        case Tok::While:   advance(); return parseWhile();
        case Tok::For:     advance(); return parseFor();
        case Tok::Ident: {
            Token name = advance();
            return parseAssignOrArrayAssign(name);
        }
        default:
            fail(t, "unexpected " + describe(t) + " at start of statement");
            return nullptr;
    }
}

StmtPtr Parser::parseDeclare() {
    const Token& name = expect(Tok::Ident, "variable name");
    expect(Tok::Colon, "':'");
    TypeInfo type = parseType();
    expect(Tok::Newline, "end of line");
    auto s = std::make_unique<DeclareStmt>(name.line, name.col);
    s->name = name.lexeme;
    s->declaredType = type;
    return s;
}

TypeInfo Parser::parseType() {
    const Token& t = peek();
    if (match(Tok::Integer)) return TypeInfo{BaseType::Integer, false, 0, 0, 0, 0, 0};
    if (match(Tok::Real))    return TypeInfo{BaseType::Real, false, 0, 0, 0, 0, 0};
    if (match(Tok::Boolean)) return TypeInfo{BaseType::Boolean, false, 0, 0, 0, 0, 0};
    if (match(Tok::String))  return TypeInfo{BaseType::String, false, 0, 0, 0, 0, 0};

    if (match(Tok::Array)) {
        expect(Tok::LBracket, "'['");
        const Token& l1 = expect(Tok::IntLit, "array lower bound");
        expect(Tok::Colon, "':'");
        const Token& u1 = expect(Tok::IntLit, "array upper bound");

        TypeInfo info;
        info.isArray = true;
        info.dims = 1;
        info.lower1 = std::stoll(l1.lexeme);
        info.upper1 = std::stoll(u1.lexeme);

        if (match(Tok::Comma)) {
            info.dims = 2;
            const Token& l2 = expect(Tok::IntLit, "second-dimension lower bound");
            expect(Tok::Colon, "':'");
            const Token& u2 = expect(Tok::IntLit, "second-dimension upper bound");
            info.lower2 = std::stoll(l2.lexeme);
            info.upper2 = std::stoll(u2.lexeme);
        }
        expect(Tok::RBracket, "']'");
        expect(Tok::Of, "'OF'");

        const Token& elemTok = peek();
        if (match(Tok::Integer)) info.base = BaseType::Integer;
        else if (match(Tok::Real)) info.base = BaseType::Real;
        else if (match(Tok::Boolean)) info.base = BaseType::Boolean;
        else if (match(Tok::String)) info.base = BaseType::String;
        else fail(elemTok, "expected element type (INTEGER, REAL, BOOLEAN, STRING)");

        return info;
    }

    fail(t, "expected a type (INTEGER, REAL, BOOLEAN, STRING, ARRAY)");
    return TypeInfo{BaseType::Error, false, 0, 0, 0, 0, 0};
}

StmtPtr Parser::parseAssignOrArrayAssign(const Token& name) {
    if (match(Tok::LBracket)) {
        auto s = std::make_unique<ArrayAssignStmt>(name.line, name.col);
        s->name = name.lexeme;
        s->indices.push_back(parseExpr());
        if (match(Tok::Comma)) s->indices.push_back(parseExpr());
        expect(Tok::RBracket, "']'");
        expect(Tok::Arrow, "'<-'");
        s->value = parseExpr();
        expect(Tok::Newline, "end of line");
        return s;
    }
    expect(Tok::Arrow, "'<-'");
    auto s = std::make_unique<AssignStmt>(name.line, name.col);
    s->name = name.lexeme;
    s->value = parseExpr();
    expect(Tok::Newline, "end of line");
    return s;
}

StmtPtr Parser::parseOutput() {
    int line = previous().line, col = previous().col;
    auto s = std::make_unique<OutputStmt>(line, col);
    s->args.push_back(parseExpr());
    while (match(Tok::Comma)) s->args.push_back(parseExpr());
    expect(Tok::Newline, "end of line");
    return s;
}

StmtPtr Parser::parseInput() {
    int line = previous().line, col = previous().col;
    const Token& name = expect(Tok::Ident, "variable name");
    auto s = std::make_unique<InputStmt>(line, col);
    s->name = name.lexeme;
    if (match(Tok::LBracket)) {
        s->indices.push_back(parseExpr());
        if (match(Tok::Comma)) s->indices.push_back(parseExpr());
        expect(Tok::RBracket, "']'");
    }
    expect(Tok::Newline, "end of line");
    return s;
}

StmtPtr Parser::parseIf() {
    int line = previous().line, col = previous().col;
    auto s = std::make_unique<IfStmt>(line, col);
    s->cond = parseExpr();
    expect(Tok::Then, "'THEN'");
    expect(Tok::Newline, "end of line");
    skipNewlines();

    while (!check(Tok::Else) && !check(Tok::EndIf) && !check(Tok::Eof)) {
        if (StmtPtr st = parseStatementSafe()) s->thenBlock.push_back(std::move(st));
        skipNewlines();
    }
    if (match(Tok::Else)) {
        expect(Tok::Newline, "end of line");
        skipNewlines();
        while (!check(Tok::EndIf) && !check(Tok::Eof)) {
            if (StmtPtr st = parseStatementSafe()) s->elseBlock.push_back(std::move(st));
            skipNewlines();
        }
    }
    expect(Tok::EndIf, "'ENDIF'");
    expect(Tok::Newline, "end of line");
    return s;
}

StmtPtr Parser::parseWhile() {
    int line = previous().line, col = previous().col;
    auto s = std::make_unique<WhileStmt>(line, col);
    s->cond = parseExpr();
    expect(Tok::Do, "'DO'");
    expect(Tok::Newline, "end of line");
    skipNewlines();

    while (!check(Tok::EndWhile) && !check(Tok::Eof)) {
        if (StmtPtr st = parseStatementSafe()) s->body.push_back(std::move(st));
        skipNewlines();
    }
    expect(Tok::EndWhile, "'ENDWHILE'");
    expect(Tok::Newline, "end of line");
    return s;
}

StmtPtr Parser::parseFor() {
    int line = previous().line, col = previous().col;
    auto s = std::make_unique<ForStmt>(line, col);
    const Token& var = expect(Tok::Ident, "loop variable");
    s->var = var.lexeme;
    expect(Tok::Arrow, "'<-'");
    s->start = parseExpr();
    expect(Tok::To, "'TO'");
    s->end = parseExpr();
    if (match(Tok::Step)) {
        s->step = parseExpr();
    }
    expect(Tok::Newline, "end of line");
    skipNewlines();

    while (!check(Tok::Next) && !check(Tok::Eof)) {
        if (StmtPtr st = parseStatementSafe()) s->body.push_back(std::move(st));
        skipNewlines();
    }
    expect(Tok::Next, "'NEXT'");
    const Token& nextVar = expect(Tok::Ident, "loop variable after 'NEXT'");
    if (nextVar.lexeme != s->var) {
        diag_.error(nextVar.line, nextVar.col,
                    "expected 'NEXT " + s->var + "', found 'NEXT " + nextVar.lexeme + "'");
    }
    expect(Tok::Newline, "end of line");
    return s;
}

ExprPtr Parser::parseExpr() {
    return parseOr();
}

ExprPtr Parser::parseOr() {
    ExprPtr expr = parseAnd();
    while (match(Tok::Or)) {
        Token op = previous();
        auto bin = std::make_unique<BinaryExpr>(op.line, op.col);
        bin->op = op.type;
        bin->lhs = std::move(expr);
        bin->rhs = parseAnd();
        expr = std::move(bin);
    }
    return expr;
}

ExprPtr Parser::parseAnd() {
    ExprPtr expr = parseComparison();
    while (match(Tok::And)) {
        Token op = previous();
        auto bin = std::make_unique<BinaryExpr>(op.line, op.col);
        bin->op = op.type;
        bin->lhs = std::move(expr);
        bin->rhs = parseComparison();
        expr = std::move(bin);
    }
    return expr;
}

ExprPtr Parser::parseComparison() {
    ExprPtr expr = parseAddition();
    while (check(Tok::Eq) || check(Tok::Neq) || check(Tok::Lt) ||
           check(Tok::Le) || check(Tok::Gt) || check(Tok::Ge)) {
        Token op = advance();
        auto bin = std::make_unique<BinaryExpr>(op.line, op.col);
        bin->op = op.type;
        bin->lhs = std::move(expr);
        bin->rhs = parseAddition();
        expr = std::move(bin);
    }
    return expr;
}

ExprPtr Parser::parseAddition() {
    ExprPtr expr = parseMultiplication();
    while (check(Tok::Plus) || check(Tok::Minus) || check(Tok::Ampersand)) {
        Token op = advance();
        auto bin = std::make_unique<BinaryExpr>(op.line, op.col);
        bin->op = op.type;
        bin->lhs = std::move(expr);
        bin->rhs = parseMultiplication();
        expr = std::move(bin);
    }
    return expr;
}

ExprPtr Parser::parseMultiplication() {
    ExprPtr expr = parseUnary();
    while (check(Tok::Star) || check(Tok::Slash) || check(Tok::Div) || check(Tok::Mod)) {
        Token op = advance();
        auto bin = std::make_unique<BinaryExpr>(op.line, op.col);
        bin->op = op.type;
        bin->lhs = std::move(expr);
        bin->rhs = parseUnary();
        expr = std::move(bin);
    }
    return expr;
}

ExprPtr Parser::parseUnary() {
    if (check(Tok::Minus) || check(Tok::Not)) {
        Token op = advance();
        auto u = std::make_unique<UnaryExpr>(op.line, op.col);
        u->op = op.type;
        u->operand = parseUnary();
        return u;
    }
    return parsePrimary();
}

ExprPtr Parser::parseBuiltInCall(Tok funcTok) {
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

ExprPtr Parser::parsePrimary() {
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
            return nullptr;
    }
}
