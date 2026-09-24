#ifndef PSEUDOC_PARSER_H
#define PSEUDOC_PARSER_H

#include "ast.h"
#include "diagnostics.h"
#include "token.h"

#include <vector>

struct ParseError {};

class Parser {
public:
    Parser(const std::vector<Token>& tokens, Diagnostics& diag);

    Block parseProgram();

private:
    const std::vector<Token>& toks_;
    Diagnostics& diag_;
    size_t cur_ = 0;

    const Token& peek() const;
    const Token& peekNext() const;
    const Token& previous() const;
    bool atEnd() const;
    bool check(Tok type) const;
    const Token& advance();
    bool match(Tok type);
    const Token& expect(Tok type, const std::string& desc);
    void fail(const Token& t, const std::string& msg);
    static std::string describe(const Token& t);
    void skipNewlines();

    StmtPtr parseStatementSafe();
    StmtPtr parseStatement();
    StmtPtr parseDeclare();
    StmtPtr parseConstant();
    TypeInfo parseType();
    StmtPtr parseAssignOrMemberOrArray(const Token& name);
    StmtPtr parseOutput();
    StmtPtr parseInput();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseRepeat();
    StmtPtr parseFor();
    StmtPtr parseCase();
    StmtPtr parseTypeDecl();
    StmtPtr parseProcedureDecl();
    StmtPtr parseFunctionDecl();
    StmtPtr parseCall();
    StmtPtr parseReturn();
    StmtPtr parseOpenFile();
    StmtPtr parseCloseFile();
    StmtPtr parseReadFile();
    StmtPtr parseWriteFile();

    ExprPtr parseExpr();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseComparison();
    ExprPtr parseAddition();
    ExprPtr parseMultiplication();
    ExprPtr parseUnary();
    ExprPtr parsePostfix(ExprPtr expr);
    ExprPtr parseBuiltInCall(Tok funcTok);
    ExprPtr parsePrimary();
};

#endif // PSEUDOC_PARSER_H
