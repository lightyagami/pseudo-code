#ifndef PSEUDOC_SEMA_H
#define PSEUDOC_SEMA_H

#include "ast.h"
#include "diagnostics.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Sema {
public:
    struct Symbol {
        TypeInfo type;
        int line;
        bool isByRef = false;
        bool isConstant = false;
        int localSlot = -1; // -1 if global
    };

    struct FunctionSig {
        std::string name;
        bool isFunction = false;
        TypeInfo returnType;
        std::vector<ParamDef> params;
        int line = 0;
    };

    explicit Sema(Diagnostics& diag);

    void run(Block& program);

    const std::vector<std::pair<std::string, TypeInfo>>& variables() const { return order_; }
    const std::unordered_map<std::string, Symbol>& symbols() const { return symbols_; }
    const std::unordered_map<std::string, RecordDef>& recordTypes() const { return recordTypes_; }
    const std::unordered_map<std::string, FunctionSig>& functions() const { return functions_; }

    void setExistingSymbols(const std::unordered_map<std::string, Symbol>& syms,
                            const std::vector<std::pair<std::string, TypeInfo>>& order);

private:
    Diagnostics& diag_;
    std::unordered_map<std::string, Symbol> symbols_;
    std::vector<std::pair<std::string, TypeInfo>> order_;
    std::unordered_map<std::string, RecordDef> recordTypes_;
    std::unordered_map<std::string, FunctionSig> functions_;

    // Local function/procedure checking state
    std::unordered_map<std::string, Symbol> localSymbols_;
    std::vector<std::pair<std::string, TypeInfo>> localOrder_;
    bool insideFunction_ = false;
    bool insideProcedure_ = false;
    TypeInfo currentReturnType_{BaseType::Void, "", false, 0, 0, 0, 0, 0};

    void err(int line, int col, const std::string& msg);
    Symbol* lookup(const std::string& name);
    static bool assignable(const TypeInfo& to, const TypeInfo& from);
    const FieldDef* lookupField(const std::string& recordName, const std::string& fieldName);

    void checkBlock(Block& block);
    void requireType(const Expr& e, const TypeInfo& t, BaseType want, const std::string& what);
    void checkStmt(Stmt& s);

    TypeInfo typeOf(Expr& e);
    TypeInfo computeType(Expr& e);
    TypeInfo computeCall(CallExpr& c);
    TypeInfo computeUserCall(UserCallExpr& c);
    TypeInfo computeBinary(BinaryExpr& b);
};

#endif // PSEUDOC_SEMA_H
