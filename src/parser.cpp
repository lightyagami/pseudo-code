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
        case Tok::Class:       advance(); return parseClassDecl();
        case Tok::Super:       advance(); return parseSuperStmt();
        case Tok::Procedure:   advance(); return parseProcedureDecl();
        case Tok::Function:    advance(); return parseFunctionDecl();
        case Tok::Call:        advance(); return parseCall();
        case Tok::Return:      advance(); return parseReturn();
        case Tok::OpenFile:    advance(); return parseOpenFile();
        case Tok::CloseFile:   advance(); return parseCloseFile();
        case Tok::ReadFile:    advance(); return parseReadFile();
        case Tok::WriteFile:   advance(); return parseWriteFile();
        case Tok::Seek:        advance(); return parseSeek();
        case Tok::GetRecord:   advance(); return parseGetRecord();
        case Tok::PutRecord:   advance(); return parsePutRecord();
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

StmtPtr Parser::parseClassDecl() {
    int line = previous().line, col = previous().col;
    const Token& name = expect(Tok::Ident, "class name");
    std::string superClass;
    if (match(Tok::Inherits)) {
        const Token& sname = expect(Tok::Ident, "super class name");
        superClass = sname.lexeme;
    }
    expect(Tok::Newline, "end of line");
    skipNewlines();

    auto cstmt = std::make_unique<ClassDeclStmt>(line, col);
    cstmt->name = name.lexeme;
    cstmt->superClass = superClass;

    while (!check(Tok::EndClass) && !check(Tok::Eof)) {
        bool isPrivate = false;
        if (match(Tok::Private)) {
            isPrivate = true;
        } else {
            match(Tok::Public);
        }

        if (match(Tok::Procedure)) {
            int mline = previous().line, mcol = previous().col;
            std::string mname;
            bool isCtor = false;
            if (match(Tok::New)) {
                mname = "NEW";
                isCtor = true;
            } else {
                const Token& ptok = expect(Tok::Ident, "procedure name");
                mname = ptok.lexeme;
            }
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

            auto m = std::make_unique<ClassMethod>();
            m->isFunction = false;
            m->isPrivate = isPrivate;
            m->isConstructor = isCtor;
            m->name = mname;
            m->params = std::move(params);
            m->returnType = TypeInfo{BaseType::Void, "", false, 0, 0, 0, 0, 0};
            m->body = std::move(body);
            m->line = mline;
            m->col = mcol;
            cstmt->methods.push_back(std::move(m));
        } else if (match(Tok::Function)) {
            int mline = previous().line, mcol = previous().col;
            const Token& ftok = expect(Tok::Ident, "function name");
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

            auto m = std::make_unique<ClassMethod>();
            m->isFunction = true;
            m->isPrivate = isPrivate;
            m->isConstructor = false;
            m->name = ftok.lexeme;
            m->params = std::move(params);
            m->returnType = retType;
            m->body = std::move(body);
            m->line = mline;
            m->col = mcol;
            cstmt->methods.push_back(std::move(m));
        } else {
            match(Tok::Declare);
            const Token& propName = expect(Tok::Ident, "property or method name");
            expect(Tok::Colon, "':'");
            TypeInfo propType = parseType();
            expect(Tok::Newline, "end of line");
            cstmt->properties.push_back({propName.lexeme, propType, isPrivate, propName.line, propName.col});
        }
        skipNewlines();
    }

    expect(Tok::EndClass, "'ENDCLASS'");
    expect(Tok::Newline, "end of line");
    return cstmt;
}

StmtPtr Parser::parseSuperStmt() {
    int line = previous().line, col = previous().col;
    expect(Tok::Dot, "'.' after SUPER");
    std::string methodName;
    if (match(Tok::New)) {
        methodName = "NEW";
    } else {
        methodName = expect(Tok::Ident, "method name").lexeme;
    }
    auto s = std::make_unique<CallStmt>(line, col);
    s->isSuper = true;
    s->name = methodName;
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
    if (match(Tok::Super)) {
        return parseSuperStmt();
    }
    const Token& firstTok = expect(Tok::Ident, "procedure or object name");
    ExprPtr target = std::make_unique<VarExpr>(firstTok.line, firstTok.col);
    static_cast<VarExpr&>(*target).name = firstTok.lexeme;

    if (check(Tok::Dot) || check(Tok::LBracket)) {
        while (check(Tok::Dot) || check(Tok::LBracket)) {
            if (match(Tok::LBracket)) {
                auto arr = std::make_unique<ArrayAccessExpr>(firstTok.line, firstTok.col);
                arr->target = std::move(target);
                arr->indices.push_back(parseExpr());
                if (match(Tok::Comma)) arr->indices.push_back(parseExpr());
                expect(Tok::RBracket, "']'");
                target = std::move(arr);
            } else if (match(Tok::Dot)) {
                std::string memberName;
                int mline = peek().line, mcol = peek().col;
                if (match(Tok::New)) memberName = "NEW";
                else memberName = expect(Tok::Ident, "field or method name").lexeme;

                if (check(Tok::Dot) || check(Tok::LBracket)) {
                    auto mem = std::make_unique<MemberAccessExpr>(mline, mcol);
                    mem->target = std::move(target);
                    mem->field = memberName;
                    target = std::move(mem);
                } else {
                    auto s = std::make_unique<CallStmt>(line, col);
                    s->target = std::move(target);
                    s->name = memberName;
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
            }
        }
    }

    auto s = std::make_unique<CallStmt>(line, col);
    s->name = firstTok.lexeme;
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
    else if (match(Tok::Random)) mode = "RANDOM";
    else fail(peek(), "expected file mode ('READ', 'WRITE', 'APPEND', or 'RANDOM')");
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

StmtPtr Parser::parseSeek() {
    int line = previous().line, col = previous().col;
    ExprPtr filename = parseExpr();
    expect(Tok::Comma, "','");
    ExprPtr address = parseExpr();
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<SeekStmt>(line, col);
    s->filename = std::move(filename);
    s->address = std::move(address);
    return s;
}

StmtPtr Parser::parseGetRecord() {
    int line = previous().line, col = previous().col;
    ExprPtr filename = parseExpr();
    expect(Tok::Comma, "','");
    ExprPtr target = parseExpr();
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<GetRecordStmt>(line, col);
    s->filename = std::move(filename);
    s->target = std::move(target);
    return s;
}

StmtPtr Parser::parsePutRecord() {
    int line = previous().line, col = previous().col;
    ExprPtr filename = parseExpr();
    expect(Tok::Comma, "','");
    ExprPtr val = parseExpr();
    expect(Tok::Newline, "end of line");

    auto s = std::make_unique<PutRecordStmt>(line, col);
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
            std::string fieldName;
            int fline = peek().line, fcol = peek().col;
            if (match(Tok::New)) fieldName = "NEW";
            else fieldName = expect(Tok::Ident, "field or method name").lexeme;

            if (check(Tok::LParen)) {
                advance(); // consume '('
                auto s = std::make_unique<CallStmt>(name.line, name.col);
                s->target = std::move(target);
                s->name = fieldName;
                if (!check(Tok::RParen)) {
                    s->args.push_back(parseExpr());
                    while (match(Tok::Comma)) s->args.push_back(parseExpr());
                }
                expect(Tok::RParen, "')'");
                expect(Tok::Newline, "end of line");
                return s;
            }

            auto mem = std::make_unique<MemberAccessExpr>(fline, fcol);
            mem->target = std::move(target);
            mem->field = fieldName;
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
            std::string fieldName;
            int fline = peek().line, fcol = peek().col;
            if (match(Tok::New)) fieldName = "NEW";
            else fieldName = expect(Tok::Ident, "field or method name").lexeme;

            if (match(Tok::LParen)) {
                auto mc = std::make_unique<MethodCallExpr>(fline, fcol);
                mc->target = std::move(expr);
                mc->method = fieldName;
                if (!check(Tok::RParen)) {
                    mc->args.push_back(parseExpr());
                    while (match(Tok::Comma)) mc->args.push_back(parseExpr());
                }
                expect(Tok::RParen, "')'");
                expr = std::move(mc);
            } else {
                auto mem = std::make_unique<MemberAccessExpr>(fline, fcol);
                mem->target = std::move(expr);
                mem->field = fieldName;
                expr = std::move(mem);
            }
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

        case Tok::New: {
            advance();
            const Token& cname = expect(Tok::Ident, "class name after NEW");
            auto e = std::make_unique<NewExpr>(t.line, t.col);
            e->className = cname.lexeme;
            expect(Tok::LParen, "'('");
            if (!check(Tok::RParen)) {
                e->args.push_back(parseExpr());
                while (match(Tok::Comma)) e->args.push_back(parseExpr());
            }
            expect(Tok::RParen, "')'");
            return e;
        }

        case Tok::Super: {
            advance();
            expect(Tok::Dot, "'.' after SUPER");
            std::string mname;
            if (match(Tok::New)) mname = "NEW";
            else mname = expect(Tok::Ident, "method name after SUPER.").lexeme;
            auto e = std::make_unique<MethodCallExpr>(t.line, t.col);
            e->isSuper = true;
            e->method = mname;
            expect(Tok::LParen, "'('");
            if (!check(Tok::RParen)) {
                e->args.push_back(parseExpr());
                while (match(Tok::Comma)) e->args.push_back(parseExpr());
            }
            expect(Tok::RParen, "')'");
            return e;
        }

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
