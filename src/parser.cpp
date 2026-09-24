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

const Token& Parser::peekNext() const {
    if (cur_ + 1 < toks_.size()) return toks_[cur_ + 1];
    return toks_.back();
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
        case Tok::Declare:     advance(); return parseDeclare();
        case Tok::Constant:    advance(); return parseConstant();
        case Tok::Output:      advance(); return parseOutput();
        case Tok::Input:       advance(); return parseInput();
        case Tok::If:          advance(); return parseIf();
        case Tok::While:       advance(); return parseWhile();
        case Tok::Repeat:      advance(); return parseRepeat();
        case Tok::For:         advance(); return parseFor();
        case Tok::Case:        advance(); return parseCase();
        case Tok::Type:        advance(); return parseTypeDecl();
        case Tok::Procedure:   advance(); return parseProcedureDecl();
        case Tok::Function:    advance(); return parseFunctionDecl();
        case Tok::Call:        advance(); return parseCall();
        case Tok::Return:      advance(); return parseReturn();
        case Tok::OpenFile:    advance(); return parseOpenFile();
        case Tok::CloseFile:   advance(); return parseCloseFile();
        case Tok::ReadFile:    advance(); return parseReadFile();
        case Tok::WriteFile:   advance(); return parseWriteFile();
        case Tok::Ident: {
            Token name = advance();
            return parseAssignOrMemberOrArray(name);
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

StmtPtr Parser::parseConstant() {
    const Token& name = expect(Tok::Ident, "constant name");
    TypeInfo type{BaseType::Error, "", false, 0, 0, 0, 0, 0};
    if (match(Tok::Colon)) {
        type = parseType();
    }
    expect(Tok::Eq, "'='");
    ExprPtr val = parseExpr();
    expect(Tok::Newline, "end of line");
    auto s = std::make_unique<ConstantStmt>(name.line, name.col);
    s->name = name.lexeme;
    s->value = std::move(val);
    s->explicitType = type;
    return s;
}

TypeInfo Parser::parseType() {
    const Token& t = peek();
    if (match(Tok::Integer)) return TypeInfo{BaseType::Integer, "", false, 0, 0, 0, 0, 0};
    if (match(Tok::Real))    return TypeInfo{BaseType::Real, "", false, 0, 0, 0, 0, 0};
    if (match(Tok::Boolean)) return TypeInfo{BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
    if (match(Tok::String))  return TypeInfo{BaseType::String, "", false, 0, 0, 0, 0, 0};
    if (match(Tok::Char))    return TypeInfo{BaseType::Char, "", false, 0, 0, 0, 0, 0};

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
        if (match(Tok::Integer))      info.base = BaseType::Integer;
        else if (match(Tok::Real))    info.base = BaseType::Real;
        else if (match(Tok::Boolean)) info.base = BaseType::Boolean;
        else if (match(Tok::String))  info.base = BaseType::String;
        else if (match(Tok::Char))    info.base = BaseType::Char;
        else if (match(Tok::Ident)) {
            info.base = BaseType::Record;
            info.recordName = elemTok.lexeme;
        } else {
            fail(elemTok, "expected element type (INTEGER, REAL, BOOLEAN, STRING, CHAR, or record type name)");
        }

        return info;
    }

    if (match(Tok::Ident)) {
        return TypeInfo{BaseType::Record, t.lexeme, false, 0, 0, 0, 0, 0};
    }

    fail(t, "expected a type (INTEGER, REAL, BOOLEAN, STRING, CHAR, ARRAY, or record name)");
    return TypeInfo{BaseType::Error, "", false, 0, 0, 0, 0, 0};
}

StmtPtr Parser::parseTypeDecl() {
    int line = previous().line, col = previous().col;
    const Token& name = expect(Tok::Ident, "record type name");
    expect(Tok::Newline, "end of line");
    skipNewlines();

    RecordDef rdef;
    rdef.name = name.lexeme;
    rdef.line = line;
    rdef.col = col;

    while (!check(Tok::EndType) && !check(Tok::Eof)) {
        expect(Tok::Declare, "'DECLARE'");
        const Token& fname = expect(Tok::Ident, "field name");
        expect(Tok::Colon, "':'");
        TypeInfo ftype = parseType();
        expect(Tok::Newline, "end of line");
        skipNewlines();
        rdef.fields.push_back({fname.lexeme, ftype, fname.line, fname.col});
    }
    expect(Tok::EndType, "'ENDTYPE'");
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<TypeDeclStmt>(line, col);
    s->recordDef = std::move(rdef);
    return s;
}

StmtPtr Parser::parseProcedureDecl() {
    int line = previous().line, col = previous().col;
    const Token& name = expect(Tok::Ident, "procedure name");
    expect(Tok::LParen, "'('");

    std::vector<ParamDef> params;
    if (!check(Tok::RParen)) {
        do {
            bool isByRef = false;
            if (match(Tok::ByRef)) isByRef = true;
            else match(Tok::ByVal);

            const Token& pname = expect(Tok::Ident, "parameter name");
            expect(Tok::Colon, "':'");
            TypeInfo ptype = parseType();
            params.push_back({pname.lexeme, ptype, isByRef, pname.line, pname.col});
        } while (match(Tok::Comma));
    }
    expect(Tok::RParen, "')'");
    expect(Tok::Newline, "end of line");
    skipNewlines();

    Block body;
    while (!check(Tok::EndProcedure) && !check(Tok::Eof)) {
        if (StmtPtr st = parseStatementSafe()) body.push_back(std::move(st));
        skipNewlines();
    }
    expect(Tok::EndProcedure, "'ENDPROCEDURE'");
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<ProcedureDeclStmt>(line, col);
    s->name = name.lexeme;
    s->params = std::move(params);
    s->body = std::move(body);
    return s;
}

StmtPtr Parser::parseFunctionDecl() {
    int line = previous().line, col = previous().col;
    const Token& name = expect(Tok::Ident, "function name");
    expect(Tok::LParen, "'('");

    std::vector<ParamDef> params;
    if (!check(Tok::RParen)) {
        do {
            bool isByRef = false;
            if (match(Tok::ByRef)) isByRef = true;
            else match(Tok::ByVal);

            const Token& pname = expect(Tok::Ident, "parameter name");
            expect(Tok::Colon, "':'");
            TypeInfo ptype = parseType();
            params.push_back({pname.lexeme, ptype, isByRef, pname.line, pname.col});
        } while (match(Tok::Comma));
    }
    expect(Tok::RParen, "')'");
    expect(Tok::Returns, "'RETURNS'");
    TypeInfo retType = parseType();
    expect(Tok::Newline, "end of line");
    skipNewlines();

    Block body;
    while (!check(Tok::EndFunction) && !check(Tok::Eof)) {
        if (StmtPtr st = parseStatementSafe()) body.push_back(std::move(st));
        skipNewlines();
    }
    expect(Tok::EndFunction, "'ENDFUNCTION'");
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<FunctionDeclStmt>(line, col);
    s->name = name.lexeme;
    s->params = std::move(params);
    s->returnType = retType;
    s->body = std::move(body);
    return s;
}

StmtPtr Parser::parseCall() {
    int line = previous().line, col = previous().col;
    const Token& name = expect(Tok::Ident, "procedure name");
    auto s = std::make_unique<CallStmt>(line, col);
    s->name = name.lexeme;
    if (match(Tok::LParen)) {
        if (!check(Tok::RParen)) {
            s->args.push_back(parseExpr());
            while (match(Tok::Comma)) s->args.push_back(parseExpr());
        }
        expect(Tok::RParen, "')'");
    }
    expect(Tok::Newline, "end of line");
    return s;
}

StmtPtr Parser::parseReturn() {
    int line = previous().line, col = previous().col;
    auto s = std::make_unique<ReturnStmt>(line, col);
    if (!check(Tok::Newline) && !check(Tok::Eof)) {
        s->value = parseExpr();
    }
    expect(Tok::Newline, "end of line");
    return s;
}

StmtPtr Parser::parseOpenFile() {
    int line = previous().line, col = previous().col;
    ExprPtr filename = parseExpr();
    expect(Tok::For, "'FOR'");
    std::string mode;
    if (match(Tok::Read)) mode = "READ";
    else if (match(Tok::Write)) mode = "WRITE";
    else if (match(Tok::Append)) mode = "APPEND";
    else fail(peek(), "expected file mode ('READ', 'WRITE', or 'APPEND')");
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<OpenFileStmt>(line, col);
    s->filename = std::move(filename);
    s->mode = mode;
    return s;
}

StmtPtr Parser::parseCloseFile() {
    int line = previous().line, col = previous().col;
    ExprPtr filename = parseExpr();
    expect(Tok::Newline, "end of line");
    auto s = std::make_unique<CloseFileStmt>(line, col);
    s->filename = std::move(filename);
    return s;
}

StmtPtr Parser::parseReadFile() {
    int line = previous().line, col = previous().col;
    ExprPtr filename = parseExpr();
    expect(Tok::Comma, "','");
    ExprPtr target = parseExpr();
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<ReadFileStmt>(line, col);
    s->filename = std::move(filename);
    s->target = std::move(target);
    return s;
}

StmtPtr Parser::parseWriteFile() {
    int line = previous().line, col = previous().col;
    ExprPtr filename = parseExpr();
    expect(Tok::Comma, "','");
    ExprPtr val = parseExpr();
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<WriteFileStmt>(line, col);
    s->filename = std::move(filename);
    s->value = std::move(val);
    return s;
}

StmtPtr Parser::parseCase() {
    int line = previous().line, col = previous().col;
    expect(Tok::Of, "'OF'");
    ExprPtr selector = parseExpr();
    expect(Tok::Newline, "end of line");
    skipNewlines();

    std::vector<CaseBranch> branches;
    Block otherwiseBlock;

    while (!check(Tok::EndCase) && !check(Tok::Otherwise) && !check(Tok::Eof)) {
        CaseBranch branch;
        branch.line = peek().line;
        branch.col = peek().col;
        branch.values.push_back(parseExpr());
        while (match(Tok::Comma)) branch.values.push_back(parseExpr());
        expect(Tok::Colon, "':'");

        // Single statement or multiple until next case / otherwise / endcase
        skipNewlines();
        while (!check(Tok::Otherwise) && !check(Tok::EndCase) && !check(Tok::Eof)) {
            // Check if looking at another branch: literal followed by colon or comma
            if ((check(Tok::IntLit) || check(Tok::StrLit)) &&
                (cur_ + 1 < toks_.size() && (toks_[cur_ + 1].type == Tok::Colon || toks_[cur_ + 1].type == Tok::Comma))) {
                break;
            }
            if (StmtPtr st = parseStatementSafe()) branch.body.push_back(std::move(st));
            skipNewlines();
        }
        branches.push_back(std::move(branch));
    }

    if (match(Tok::Otherwise)) {
        match(Tok::Colon);
        skipNewlines();
        while (!check(Tok::EndCase) && !check(Tok::Eof)) {
            if (StmtPtr st = parseStatementSafe()) otherwiseBlock.push_back(std::move(st));
            skipNewlines();
        }
    }

    expect(Tok::EndCase, "'ENDCASE'");
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<CaseStmt>(line, col);
    s->selector = std::move(selector);
    s->branches = std::move(branches);
    s->otherwiseBlock = std::move(otherwiseBlock);
    return s;
}

StmtPtr Parser::parseAssignOrMemberOrArray(const Token& name) {
    ExprPtr target = std::make_unique<VarExpr>(name.line, name.col);
    static_cast<VarExpr&>(*target).name = name.lexeme;

    // Parse chaining of array indexing and member accesses on LHS
    while (check(Tok::LBracket) || check(Tok::Dot)) {
        if (match(Tok::LBracket)) {
            auto arr = std::make_unique<ArrayAccessExpr>(name.line, name.col);
            arr->target = std::move(target);
            arr->indices.push_back(parseExpr());
            if (match(Tok::Comma)) arr->indices.push_back(parseExpr());
            expect(Tok::RBracket, "']'");
            target = std::move(arr);
        } else if (match(Tok::Dot)) {
            const Token& field = expect(Tok::Ident, "field name");
            auto mem = std::make_unique<MemberAccessExpr>(field.line, field.col);
            mem->target = std::move(target);
            mem->field = field.lexeme;
            target = std::move(mem);
        }
    }

    expect(Tok::Arrow, "'<-'");
    ExprPtr val = parseExpr();
    expect(Tok::Newline, "end of line");

    if (target->kind == Expr::Kind::Var) {
        auto s = std::make_unique<AssignStmt>(name.line, name.col);
        s->name = static_cast<VarExpr&>(*target).name;
        s->value = std::move(val);
        return s;
    }
    if (target->kind == Expr::Kind::ArrayAccess && static_cast<ArrayAccessExpr&>(*target).target->kind == Expr::Kind::Var) {
        auto& arr = static_cast<ArrayAccessExpr&>(*target);
        auto s = std::make_unique<ArrayAssignStmt>(name.line, name.col);
        s->name = static_cast<VarExpr&>(*arr.target).name;
        s->indices = std::move(arr.indices);
        s->value = std::move(val);
        return s;
    }
    if (target->kind == Expr::Kind::MemberAccess) {
        auto& mem = static_cast<MemberAccessExpr&>(*target);
        auto s = std::make_unique<MemberAssignStmt>(mem.line, mem.col);
        s->field = mem.field;
        s->target = std::move(mem.target);
        s->value = std::move(val);
        return s;
    }

    // General array element assignment (e.g. member array element)
    auto& arr = static_cast<ArrayAccessExpr&>(*target);
    auto s = std::make_unique<ArrayAssignStmt>(name.line, name.col);
    s->target = std::move(arr.target);
    s->indices = std::move(arr.indices);
    s->value = std::move(val);
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
    ExprPtr target = parseExpr();
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<InputStmt>(line, col);
    if (target->kind == Expr::Kind::Var) {
        s->name = static_cast<VarExpr&>(*target).name;
    } else if (target->kind == Expr::Kind::ArrayAccess && static_cast<ArrayAccessExpr&>(*target).target &&
               static_cast<ArrayAccessExpr&>(*target).target->kind == Expr::Kind::Var) {
        s->name = static_cast<VarExpr&>(*static_cast<ArrayAccessExpr&>(*target).target).name;
        s->indices = std::move(static_cast<ArrayAccessExpr&>(*target).indices);
    } else {
        s->target = std::move(target);
    }
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

StmtPtr Parser::parseRepeat() {
    int line = previous().line, col = previous().col;
    auto s = std::make_unique<RepeatStmt>(line, col);
    expect(Tok::Newline, "end of line");
    skipNewlines();

    while (!check(Tok::Until) && !check(Tok::Eof)) {
        if (StmtPtr st = parseStatementSafe()) s->body.push_back(std::move(st));
        skipNewlines();
    }
    expect(Tok::Until, "'UNTIL'");
    s->cond = parseExpr();
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
    return parsePostfix(parsePrimary());
}

ExprPtr Parser::parsePostfix(ExprPtr expr) {
    while (check(Tok::LBracket) || check(Tok::Dot)) {
        if (match(Tok::LBracket)) {
            auto arr = std::make_unique<ArrayAccessExpr>(previous().line, previous().col);
            if (expr->kind == Expr::Kind::Var) {
                arr->name = static_cast<VarExpr&>(*expr).name;
            }
            arr->target = std::move(expr);
            arr->indices.push_back(parseExpr());
            if (match(Tok::Comma)) arr->indices.push_back(parseExpr());
            expect(Tok::RBracket, "']'");
            expr = std::move(arr);
        } else if (match(Tok::Dot)) {
            const Token& field = expect(Tok::Ident, "field name");
            auto mem = std::make_unique<MemberAccessExpr>(field.line, field.col);
            mem->target = std::move(expr);
            mem->field = field.lexeme;
            expr = std::move(mem);
        }
    }
    return expr;
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
        case Tok::CharLit:
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
        case Tok::Mid:
        case Tok::Left:
        case Tok::Right:
        case Tok::UCase:
        case Tok::LCase:
        case Tok::NumToStr:
        case Tok::StrToNum:
        case Tok::Chr:
        case Tok::Asc:
        case Tok::IntFunc:
        case Tok::Round:
        case Tok::Rnd:
        case Tok::EofFunc:
            return parseBuiltInCall(t.type);

        case Tok::Div:
        case Tok::Mod:
            if (peekNext().type == Tok::LParen) {
                return parseBuiltInCall(t.type);
            }
            fail(t, "expected an expression, found " + describe(t));
            return nullptr;

        case Tok::Ident: {
            Token id = advance();
            if (match(Tok::LParen)) {
                auto call = std::make_unique<UserCallExpr>(id.line, id.col);
                call->callee = id.lexeme;
                if (!check(Tok::RParen)) {
                    call->args.push_back(parseExpr());
                    while (match(Tok::Comma)) call->args.push_back(parseExpr());
                }
                expect(Tok::RParen, "')'");
                return call;
            }
            auto e = std::make_unique<VarExpr>(id.line, id.col);
            e->name = id.lexeme;
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
