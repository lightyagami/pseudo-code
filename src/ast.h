#ifndef PSEUDOC_AST_H
#define PSEUDOC_AST_H

#include "token.h"

#include <memory>
#include <string>
#include <vector>

enum class BaseType {
    Integer,
    Real,
    Boolean,
    String,
    Error
};

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

inline const char* baseTypeName(BaseType t) {
    switch (t) {
        case BaseType::Integer: return "INTEGER";
        case BaseType::Real:    return "REAL";
        case BaseType::Boolean: return "BOOLEAN";
        case BaseType::String:  return "STRING";
        default:                return "<error>";
    }
}

inline std::string typeString(const TypeInfo& t) {
    if (!t.isArray) return baseTypeName(t.base);
    std::string s = "ARRAY[";
    s += std::to_string(t.lower1) + ":" + std::to_string(t.upper1);
    if (t.dims == 2) s += ", " + std::to_string(t.lower2) + ":" + std::to_string(t.upper2);
    s += "] OF " + std::string(baseTypeName(t.base));
    return s;
}

inline bool isNumeric(const TypeInfo& t) {
    return !t.isArray && (t.base == BaseType::Integer || t.base == BaseType::Real);
}

inline bool isComparison(Tok op) {
    return op == Tok::Eq || op == Tok::Neq || op == Tok::Lt ||
           op == Tok::Le || op == Tok::Gt || op == Tok::Ge;
}

inline std::string opName(Tok op) {
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

// AST Expressions

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

// AST Statements

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
    std::vector<ExprPtr> indices;
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

#endif // PSEUDOC_AST_H
