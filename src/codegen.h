#ifndef PSEUDOC_CODEGEN_H
#define PSEUDOC_CODEGEN_H

#include "ast.h"
#include "sema.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class CodeGen {
public:
    CodeGen(const std::vector<std::pair<std::string, TypeInfo>>& vars,
            const std::unordered_map<std::string, RecordDef>& records,
            const std::unordered_map<std::string, Sema::FunctionSig>& funcs,
            const std::unordered_map<std::string, Sema::ClassInfo>& classes = {});

    std::string generate(const Block& program);

private:
    const std::vector<std::pair<std::string, TypeInfo>>& vars_;
    std::unordered_map<std::string, TypeInfo> varMap_;
    const std::unordered_map<std::string, RecordDef>& recordTypes_;
    const std::unordered_map<std::string, Sema::FunctionSig>& functions_;
    const std::unordered_map<std::string, Sema::ClassInfo>& classTypes_;
    std::unordered_set<std::string> currentByRefParams_;
    std::string currentClassName_;
    bool insideFunction_ = false;

    std::ostringstream out_;
    int indent_ = 0;
    int tempCounter_ = 0;

    void line(const std::string& text);
    std::string cBaseType(const TypeInfo& t);
    std::string zeroValue(const TypeInfo& t);
    static std::string cName(const std::string& name);
    std::string cParamDecl(const ParamDef& p);
    static std::string cTypeName(const std::string& name);
    static std::string cClassTypeName(const std::string& name);
    static std::string escapeCStr(const std::string& s);

    void emitRuntimeHeaders();
    void emitRecordDefinitions();
    void emitClassDefinitions();
    void emitFunctionPrototypes();
    void emitFunctionDefinitions(const Block& program);
    void emitClassMethods(const Block& program);

    void emitBlock(const Block& block);
    void emitIndented(const Block& block);
    std::string arrayOffset(const std::string& name, const std::vector<ExprPtr>& indices);
    void emitStmt(const Stmt& s);
    void emitOutput(const OutputStmt& o);
    void emitInput(const InputStmt& in);
    void emitFor(const ForStmt& f);
    void emitCase(const CaseStmt& c);
    std::string expr(const Expr& e);
    std::string lvalueExpr(const Expr& e);
    std::string call(const CallExpr& c);
    std::string userCall(const UserCallExpr& c);
    std::string binary(const BinaryExpr& b);
};

#endif // PSEUDOC_CODEGEN_H
