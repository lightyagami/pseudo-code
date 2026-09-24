#ifndef PSEUDOC_PY_CODEGEN_H
#define PSEUDOC_PY_CODEGEN_H

#include "ast.h"
#include "sema.h"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class PyCodeGen {
public:
    PyCodeGen(const std::vector<std::pair<std::string, TypeInfo>>& vars,
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
    std::string currentClassName_;
    bool insideClass_ = false;

    std::ostringstream out_;
    int indent_ = 0;

    void line(const std::string& text);
    static std::string pyTypeName(const TypeInfo& t);
    static std::string escapePyStr(const std::string& s);
    bool isClassProperty(const std::string& name) const;

    void emitImports();
    void emitRecordClasses();
    void emitClassDefinitions(const Block& program);
    void emitFunctions(const Block& program);
    void emitBlock(const Block& block);
    void emitStmt(const Stmt& s);

    std::string expr(const Expr& e);
    std::string binary(const BinaryExpr& b);
    std::string call(const CallExpr& c);
};

#endif // PSEUDOC_PY_CODEGEN_H
