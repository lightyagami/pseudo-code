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
    };

    explicit Sema(Diagnostics& diag);

    void run(Block& program);

    const std::vector<std::pair<std::string, TypeInfo>>& variables() const { return order_; }
    const std::unordered_map<std::string, Symbol>& symbols() const { return symbols_; }

    void setExistingSymbols(const std::unordered_map<std::string, Symbol>& syms,
                            const std::vector<std::pair<std::string, TypeInfo>>& order);

private:
    Diagnostics& diag_;
    std::unordered_map<std::string, Symbol> symbols_;
    std::vector<std::pair<std::string, TypeInfo>> order_;

    void err(int line, int col, const std::string& msg);
    Symbol* lookup(const std::string& name);
    static bool assignable(const TypeInfo& to, const TypeInfo& from);

    void checkBlock(Block& block);
    void requireType(const Expr& e, const TypeInfo& t, BaseType want, const std::string& what);
    void checkStmt(Stmt& s);

    TypeInfo typeOf(Expr& e);
    TypeInfo computeType(Expr& e);
    TypeInfo computeCall(CallExpr& c);
    TypeInfo computeBinary(BinaryExpr& b);
};

#endif // PSEUDOC_SEMA_H
