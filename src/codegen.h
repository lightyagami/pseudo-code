#ifndef PSEUDOC_CODEGEN_H
#define PSEUDOC_CODEGEN_H

#include "ast.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class CodeGen {
public:
    explicit CodeGen(const std::vector<std::pair<std::string, TypeInfo>>& vars);

    std::string generate(const Block& program);

private:
    const std::vector<std::pair<std::string, TypeInfo>>& vars_;
    std::unordered_map<std::string, TypeInfo> varMap_;
    std::ostringstream out_;
    int indent_ = 0;
    int tempCounter_ = 0;

    void line(const std::string& text);
    static std::string cBaseType(BaseType t);
    static std::string zeroValue(BaseType t);
    static std::string cName(const std::string& name);
    static std::string escapeCStr(const std::string& s);

    void emitRuntimeHeaders();
    void emitBlock(const Block& block);
    void emitIndented(const Block& block);
    std::string arrayOffset(const std::string& name, const std::vector<ExprPtr>& indices);
    void emitStmt(const Stmt& s);
    void emitOutput(const OutputStmt& o);
    void emitInput(const InputStmt& in);
    void emitFor(const ForStmt& f);
    std::string expr(const Expr& e);
    std::string call(const CallExpr& c);
    std::string binary(const BinaryExpr& b);
};

#endif // PSEUDOC_CODEGEN_H
