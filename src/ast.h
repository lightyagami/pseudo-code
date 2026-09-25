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
    Char,
    Record,
    Object,
    Void,
    Error
};

struct TypeInfo {
    BaseType base = BaseType::Integer;
    std::string recordName;
    bool isArray = false;
    int dims = 0;
    long long lower1 = 0, upper1 = 0;
    long long lower2 = 0, upper2 = 0;

    bool operator==(const TypeInfo& o) const {
        if (base != o.base || isArray != o.isArray || dims != o.dims) return false;
        if ((base == BaseType::Record || base == BaseType::Object) && recordName != o.recordName) return false;
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
        case BaseType::Char:    return "CHAR";
        case BaseType::Record:  return "RECORD";
        case BaseType::Object:  return "OBJECT";
        case BaseType::Void:    return "VOID";
        default:                return "<error>";
    }
}

inline std::string typeString(const TypeInfo& t) {
    std::string baseStr;
    if (t.base == BaseType::Record || t.base == BaseType::Object) baseStr = t.recordName;
    else baseStr = baseTypeName(t.base);

    if (!t.isArray) return baseStr;
    std::string s = "ARRAY[";
    s += std::to_string(t.lower1) + ":" + std::to_string(t.upper1);
    if (t.dims == 2) s += ", " + std::to_string(t.lower2) + ":" + std::to_string(t.upper2);
    s += "] OF " + baseStr;
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

// User-Defined Types (Records)
struct FieldDef {
    std::string name;
    TypeInfo type;
    int line = 0, col = 0;
};

struct RecordDef {
    std::string name;
    std::vector<FieldDef> fields;
    int line = 0, col = 0;
};

// Procedure / Function Parameter
struct ParamDef {
    std::string name;
    TypeInfo type;
    bool isByRef = false;
    int line = 0, col = 0;
};

// AST Expressions

struct Expr {
    enum class Kind {
        Literal, Var, ArrayAccess, MemberAccess, Unary, Binary, Call, UserCall,
        New, MethodCall
    };
    Kind kind;
    int line, col;
    TypeInfo type{BaseType::Error, "", false, 0, 0, 0, 0, 0};

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
    ExprPtr target; // If non-null, accesses target[indices...]
    std::vector<ExprPtr> indices;
    ArrayAccessExpr(int l, int c) : Expr(Kind::ArrayAccess, l, c) {}
};

struct MemberAccessExpr : Expr {
    ExprPtr target;
    std::string field;
    MemberAccessExpr(int l, int c) : Expr(Kind::MemberAccess, l, c) {}
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

struct UserCallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    UserCallExpr(int l, int c) : Expr(Kind::UserCall, l, c) {}
};

struct NewExpr : Expr {
    std::string className;
    std::vector<ExprPtr> args;
    NewExpr(int l, int c) : Expr(Kind::New, l, c) {}
};

struct MethodCallExpr : Expr {
    ExprPtr target; // null if SUPER
    bool isSuper = false;
    std::string method;
    std::vector<ExprPtr> args;
    MethodCallExpr(int l, int c) : Expr(Kind::MethodCall, l, c) {}
};

// AST Statements

struct Stmt {
    enum class Kind {
        Declare, Constant, Assign, ArrayAssign, MemberAssign, Output, Input,
        If, While, Repeat, For, Case,
        TypeDecl, ClassDecl, ProcedureDecl, FunctionDecl, Call, Return,
        OpenFile, CloseFile, ReadFile, WriteFile,
        Seek, GetRecord, PutRecord
    };
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

struct ConstantStmt : Stmt {
    std::string name;
    ExprPtr value;
    TypeInfo explicitType{BaseType::Error, "", false, 0, 0, 0, 0, 0};
    ConstantStmt(int l, int c) : Stmt(Kind::Constant, l, c) {}
};

struct AssignStmt : Stmt {
    std::string name;
    ExprPtr value;
    AssignStmt(int l, int c) : Stmt(Kind::Assign, l, c) {}
};

struct ArrayAssignStmt : Stmt {
    std::string name;
    ExprPtr target; // If non-null, target[indices...] = value
    std::vector<ExprPtr> indices;
    ExprPtr value;
    ArrayAssignStmt(int l, int c) : Stmt(Kind::ArrayAssign, l, c) {}
};

struct MemberAssignStmt : Stmt {
    ExprPtr target; // e.g. VarExpr or ArrayAccessExpr
    std::string field;
    ExprPtr value;
    MemberAssignStmt(int l, int c) : Stmt(Kind::MemberAssign, l, c) {}
};

struct OutputStmt : Stmt {
    std::vector<ExprPtr> args;
    OutputStmt(int l, int c) : Stmt(Kind::Output, l, c) {}
};

struct InputStmt : Stmt {
    std::string name;
    ExprPtr target; // If non-null, input into target
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

struct RepeatStmt : Stmt {
    Block body;
    ExprPtr cond;
    RepeatStmt(int l, int c) : Stmt(Kind::Repeat, l, c) {}
};

struct ForStmt : Stmt {
    std::string var;
    ExprPtr start, end, step;
    Block body;
    ForStmt(int l, int c) : Stmt(Kind::For, l, c) {}
};

struct CaseBranch {
    std::vector<ExprPtr> values;
    Block body;
    int line = 0, col = 0;
};

struct CaseStmt : Stmt {
    ExprPtr selector;
    std::vector<CaseBranch> branches;
    Block otherwiseBlock;
    CaseStmt(int l, int c) : Stmt(Kind::Case, l, c) {}
};

struct TypeDeclStmt : Stmt {
    RecordDef recordDef;
    TypeDeclStmt(int l, int c) : Stmt(Kind::TypeDecl, l, c) {}
};

struct ClassProperty {
    std::string name;
    TypeInfo type;
    bool isPrivate = false;
    int line = 0, col = 0;
};

struct ClassMethod {
    bool isFunction = false;
    bool isPrivate = false;
    bool isConstructor = false;
    std::string name;
    std::vector<ParamDef> params;
    TypeInfo returnType{BaseType::Void, "", false, 0, 0, 0, 0, 0};
    Block body;
    int line = 0, col = 0;
};

struct ClassDeclStmt : Stmt {
    std::string name;
    std::string superClass;
    std::vector<ClassProperty> properties;
    std::vector<std::unique_ptr<ClassMethod>> methods;
    ClassDeclStmt(int l, int c) : Stmt(Kind::ClassDecl, l, c) {}
};

struct ProcedureDeclStmt : Stmt {
    std::string name;
    std::vector<ParamDef> params;
    Block body;
    ProcedureDeclStmt(int l, int c) : Stmt(Kind::ProcedureDecl, l, c) {}
};

struct FunctionDeclStmt : Stmt {
    std::string name;
    std::vector<ParamDef> params;
    TypeInfo returnType;
    Block body;
    FunctionDeclStmt(int l, int c) : Stmt(Kind::FunctionDecl, l, c) {}
};

struct CallStmt : Stmt {
    ExprPtr target; // null if standalone procedure call
    bool isSuper = false;
    std::string name;
    std::vector<ExprPtr> args;
    CallStmt(int l, int c) : Stmt(Kind::Call, l, c) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value; // null if void/procedure return
    ReturnStmt(int l, int c) : Stmt(Kind::Return, l, c) {}
};

struct OpenFileStmt : Stmt {
    ExprPtr filename;
    std::string mode; // "READ", "WRITE", "APPEND"
    OpenFileStmt(int l, int c) : Stmt(Kind::OpenFile, l, c) {}
};

struct CloseFileStmt : Stmt {
    ExprPtr filename;
    CloseFileStmt(int l, int c) : Stmt(Kind::CloseFile, l, c) {}
};

struct ReadFileStmt : Stmt {
    ExprPtr filename;
    ExprPtr target; // lvalue target
    ReadFileStmt(int l, int c) : Stmt(Kind::ReadFile, l, c) {}
};

struct WriteFileStmt : Stmt {
    ExprPtr filename;
    ExprPtr value;
    WriteFileStmt(int l, int c) : Stmt(Kind::WriteFile, l, c) {}
};

struct SeekStmt : Stmt {
    ExprPtr filename;
    ExprPtr address;
    SeekStmt(int l, int c) : Stmt(Kind::Seek, l, c) {}
};

struct GetRecordStmt : Stmt {
    ExprPtr filename;
    ExprPtr target;
    GetRecordStmt(int l, int c) : Stmt(Kind::GetRecord, l, c) {}
};

struct PutRecordStmt : Stmt {
    ExprPtr filename;
    ExprPtr value;
    PutRecordStmt(int l, int c) : Stmt(Kind::PutRecord, l, c) {}
};

#endif // PSEUDOC_AST_H
