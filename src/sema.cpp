#include "sema.h"

Sema::Sema(Diagnostics& diag) : diag_(diag) {}

void Sema::run(Block& program) {
    checkBlock(program);
}

void Sema::setExistingSymbols(const std::unordered_map<std::string, Symbol>& syms,
                            const std::vector<std::pair<std::string, TypeInfo>>& order) {
    symbols_ = syms;
    order_ = order;
}

void Sema::err(int line, int col, const std::string& msg) {
    diag_.error(line, col, msg);
}

Sema::Symbol* Sema::lookup(const std::string& name) {
    auto it = symbols_.find(name);
    return it == symbols_.end() ? nullptr : &it->second;
}

bool Sema::assignable(const TypeInfo& to, const TypeInfo& from) {
    if (to.isArray || from.isArray) return to == from;
    if (to.base == from.base) return true;
    return to.base == BaseType::Real && from.base == BaseType::Integer;
}

void Sema::checkBlock(Block& block) {
    for (auto& s : block) checkStmt(*s);
}

void Sema::requireType(const Expr& e, const TypeInfo& t, BaseType want, const std::string& what) {
    if (t.isArray || (t.base != want && t.base != BaseType::Error)) {
        err(e.line, e.col, what + " must be a scalar " + baseTypeName(want) +
            ", found " + typeString(t));
    }
}

void Sema::checkStmt(Stmt& s) {
    switch (s.kind) {
        case Stmt::Kind::Declare: {
            auto& d = static_cast<DeclareStmt&>(s);
            if (Symbol* prev = lookup(d.name)) {
                err(d.line, d.col, "'" + d.name + "' is already declared (line " +
                    std::to_string(prev->line) + ")");
            } else {
                if (d.declaredType.isArray) {
                    if (d.declaredType.lower1 > d.declaredType.upper1)
                        err(d.line, d.col, "invalid array bounds: lower bound > upper bound");
                    if (d.declaredType.dims == 2 && d.declaredType.lower2 > d.declaredType.upper2)
                        err(d.line, d.col, "invalid second-dimension array bounds: lower bound > upper bound");
                }
                symbols_[d.name] = Symbol{d.declaredType, d.line};
                order_.emplace_back(d.name, d.declaredType);
            }
            break;
        }
        case Stmt::Kind::Assign: {
            auto& a = static_cast<AssignStmt&>(s);
            Symbol* sym = lookup(a.name);
            if (!sym) err(a.line, a.col, "'" + a.name + "' is not declared");
            TypeInfo value = typeOf(*a.value);
            if (sym && sym->type.isArray) {
                err(a.line, a.col, "cannot assign directly to array '" + a.name + "'; index required");
            } else if (sym && value.base != BaseType::Error && !assignable(sym->type, value)) {
                err(a.line, a.col, "cannot assign " + typeString(value) + " to " +
                    typeString(sym->type) + " variable '" + a.name + "'");
            }
            break;
        }
        case Stmt::Kind::ArrayAssign: {
            auto& a = static_cast<ArrayAssignStmt&>(s);
            Symbol* sym = lookup(a.name);
            if (!sym) {
                err(a.line, a.col, "'" + a.name + "' is not declared");
            } else if (!sym->type.isArray) {
                err(a.line, a.col, "'" + a.name + "' is not an array");
            } else {
                if (static_cast<int>(a.indices.size()) != sym->type.dims) {
                    err(a.line, a.col, "array '" + a.name + "' expects " +
                        std::to_string(sym->type.dims) + " indices, found " +
                        std::to_string(a.indices.size()));
                }
                for (auto& idx : a.indices) {
                    requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
                }
            }
            TypeInfo val = typeOf(*a.value);
            if (sym && sym->type.isArray && val.base != BaseType::Error) {
                TypeInfo elemType{sym->type.base, false, 0, 0, 0, 0, 0};
                if (!assignable(elemType, val)) {
                    err(a.line, a.col, "cannot assign " + typeString(val) + " to " +
                        baseTypeName(sym->type.base) + " array element");
                }
            }
            break;
        }
        case Stmt::Kind::Output: {
            for (auto& arg : static_cast<OutputStmt&>(s).args) {
                TypeInfo t = typeOf(*arg);
                if (t.isArray) err(arg->line, arg->col, "cannot output an entire array");
            }
            break;
        }
        case Stmt::Kind::Input: {
            auto& in = static_cast<InputStmt&>(s);
            Symbol* sym = lookup(in.name);
            if (!sym) {
                err(in.line, in.col, "'" + in.name + "' is not declared");
            } else if (sym->type.isArray) {
                if (in.indices.empty()) {
                    err(in.line, in.col, "cannot INPUT into whole array '" + in.name + "'");
                } else {
                    if (static_cast<int>(in.indices.size()) != sym->type.dims) {
                        err(in.line, in.col, "array '" + in.name + "' expects " +
                            std::to_string(sym->type.dims) + " indices, found " +
                            std::to_string(in.indices.size()));
                    }
                    for (auto& idx : in.indices) {
                        requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
                    }
                }
            } else if (!in.indices.empty()) {
                err(in.line, in.col, "'" + in.name + "' is not an array");
            }
            break;
        }
        case Stmt::Kind::If: {
            auto& i = static_cast<IfStmt&>(s);
            requireType(*i.cond, typeOf(*i.cond), BaseType::Boolean, "IF condition");
            checkBlock(i.thenBlock);
            checkBlock(i.elseBlock);
            break;
        }
        case Stmt::Kind::While: {
            auto& w = static_cast<WhileStmt&>(s);
            requireType(*w.cond, typeOf(*w.cond), BaseType::Boolean, "WHILE condition");
            checkBlock(w.body);
            break;
        }
        case Stmt::Kind::For: {
            auto& f = static_cast<ForStmt&>(s);
            Symbol* sym = lookup(f.var);
            if (!sym)
                err(f.line, f.col, "'" + f.var + "' is not declared");
            else if (sym->type.isArray || sym->type.base != BaseType::Integer)
                err(f.line, f.col, "FOR variable '" + f.var + "' must be INTEGER");

            requireType(*f.start, typeOf(*f.start), BaseType::Integer, "FOR start value");
            requireType(*f.end, typeOf(*f.end), BaseType::Integer, "FOR end value");
            if (f.step) {
                requireType(*f.step, typeOf(*f.step), BaseType::Integer, "STEP value");
                if (f.step->kind == Expr::Kind::Literal) {
                    auto& lit = static_cast<LiteralExpr&>(*f.step);
                    if (lit.litType == Tok::IntLit && std::stoll(lit.text) == 0) {
                        err(f.step->line, f.step->col, "STEP value cannot be 0");
                    }
                }
            }
            checkBlock(f.body);
            break;
        }
    }
}

TypeInfo Sema::typeOf(Expr& e) {
    e.type = computeType(e);
    return e.type;
}

TypeInfo Sema::computeType(Expr& e) {
    TypeInfo errType{BaseType::Error, false, 0, 0, 0, 0, 0};
    switch (e.kind) {
        case Expr::Kind::Literal: {
            switch (static_cast<LiteralExpr&>(e).litType) {
                case Tok::IntLit:  return {BaseType::Integer, false, 0, 0, 0, 0, 0};
                case Tok::RealLit: return {BaseType::Real, false, 0, 0, 0, 0, 0};
                case Tok::StrLit:  return {BaseType::String, false, 0, 0, 0, 0, 0};
                default:           return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            }
        }
        case Expr::Kind::Var: {
            auto& v = static_cast<VarExpr&>(e);
            Symbol* sym = lookup(v.name);
            if (!sym) {
                err(v.line, v.col, "'" + v.name + "' is not declared");
                return errType;
            }
            return sym->type;
        }
        case Expr::Kind::ArrayAccess: {
            auto& a = static_cast<ArrayAccessExpr&>(e);
            Symbol* sym = lookup(a.name);
            if (!sym) {
                err(a.line, a.col, "'" + a.name + "' is not declared");
                return errType;
            }
            if (!sym->type.isArray) {
                err(a.line, a.col, "'" + a.name + "' is not an array");
                return errType;
            }
            if (static_cast<int>(a.indices.size()) != sym->type.dims) {
                err(a.line, a.col, "array '" + a.name + "' expects " +
                    std::to_string(sym->type.dims) + " indices, found " +
                    std::to_string(a.indices.size()));
            }
            for (auto& idx : a.indices) {
                requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
            }
            return {sym->type.base, false, 0, 0, 0, 0, 0};
        }
        case Expr::Kind::Unary: {
            auto& u = static_cast<UnaryExpr&>(e);
            TypeInfo t = typeOf(*u.operand);
            if (t.base == BaseType::Error) return u.op == Tok::Not ? TypeInfo{BaseType::Boolean, false, 0, 0, 0, 0, 0} : errType;
            if (u.op == Tok::Minus) {
                if (isNumeric(t)) return t;
                err(u.line, u.col, std::string("unary '-' needs a numeric operand, found ") + typeString(t));
                return errType;
            }
            if (t.base == BaseType::Boolean && !t.isArray) return t;
            err(u.line, u.col, std::string("'NOT' needs a BOOLEAN operand, found ") + typeString(t));
            return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
        }
        case Expr::Kind::Binary:
            return computeBinary(static_cast<BinaryExpr&>(e));
        case Expr::Kind::Call:
            return computeCall(static_cast<CallExpr&>(e));
    }
    return errType;
}

TypeInfo Sema::computeCall(CallExpr& c) {
    TypeInfo errType{BaseType::Error, false, 0, 0, 0, 0, 0};
    auto checkArgCount = [&](size_t expected) -> bool {
        if (c.args.size() != expected) {
            err(c.line, c.col, "function expects " + std::to_string(expected) +
                " arguments, found " + std::to_string(c.args.size()));
            return false;
        }
        return true;
    };

    switch (c.func) {
        case Tok::Length:
            if (checkArgCount(1)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "LENGTH argument");
            }
            return {BaseType::Integer, false, 0, 0, 0, 0, 0};
        case Tok::Substring:
            if (checkArgCount(3)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "SUBSTRING argument 1");
                requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "SUBSTRING argument 2 (start)");
                requireType(*c.args[2], typeOf(*c.args[2]), BaseType::Integer, "SUBSTRING argument 3 (length)");
            }
            return {BaseType::String, false, 0, 0, 0, 0, 0};
        case Tok::UCase:
        case Tok::LCase:
            if (checkArgCount(1)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "string function argument");
            }
            return {BaseType::String, false, 0, 0, 0, 0, 0};
        case Tok::NumToStr:
            if (checkArgCount(1)) {
                TypeInfo t = typeOf(*c.args[0]);
                if (!isNumeric(t)) {
                    err(c.line, c.col, "NUM_TO_STR requires a numeric argument, found " + typeString(t));
                }
            }
            return {BaseType::String, false, 0, 0, 0, 0, 0};
        case Tok::StrToNum:
            if (checkArgCount(1)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "STR_TO_NUM argument");
            }
            return {BaseType::Real, false, 0, 0, 0, 0, 0};
        default:
            return errType;
    }
}

TypeInfo Sema::computeBinary(BinaryExpr& b) {
    TypeInfo l = typeOf(*b.lhs);
    TypeInfo r = typeOf(*b.rhs);
    bool boolResult = isComparison(b.op) || b.op == Tok::And || b.op == Tok::Or;
    TypeInfo errResult = boolResult ? TypeInfo{BaseType::Boolean, false, 0, 0, 0, 0, 0}
                                    : TypeInfo{BaseType::Error, false, 0, 0, 0, 0, 0};

    if (l.base == BaseType::Error || r.base == BaseType::Error) return errResult;
    if (l.isArray || r.isArray) {
        err(b.line, b.col, "cannot apply operators to whole array types");
        return errResult;
    }

    auto bad = [&](const char* need) {
        err(b.line, b.col, "operator '" + opName(b.op) + "' needs " + need +
            " operands, found " + typeString(l) + " and " + typeString(r));
        return errResult;
    };

    switch (b.op) {
        case Tok::Plus:
        case Tok::Minus:
        case Tok::Star:
            if (isNumeric(l) && isNumeric(r)) {
                if (l.base == BaseType::Real || r.base == BaseType::Real)
                    return {BaseType::Real, false, 0, 0, 0, 0, 0};
                return {BaseType::Integer, false, 0, 0, 0, 0, 0};
            }
            return bad("numeric");

        case Tok::Slash:
            if (isNumeric(l) && isNumeric(r))
                return {BaseType::Real, false, 0, 0, 0, 0, 0};
            return bad("numeric");

        case Tok::Div:
        case Tok::Mod:
            if (l.base == BaseType::Integer && r.base == BaseType::Integer)
                return {BaseType::Integer, false, 0, 0, 0, 0, 0};
            return bad("INTEGER");

        case Tok::Ampersand:
            if (l.base == BaseType::String && r.base == BaseType::String)
                return {BaseType::String, false, 0, 0, 0, 0, 0};
            return bad("STRING");

        case Tok::And:
        case Tok::Or:
            if (l.base == BaseType::Boolean && r.base == BaseType::Boolean)
                return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            return bad("BOOLEAN");

        case Tok::Eq:
        case Tok::Neq:
            if (isNumeric(l) && isNumeric(r)) return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            if (l.base == r.base) return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            return bad("matching type");

        case Tok::Lt:
        case Tok::Le:
        case Tok::Gt:
        case Tok::Ge:
            if (isNumeric(l) && isNumeric(r)) return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            if (l.base == BaseType::String && r.base == BaseType::String)
                return {BaseType::Boolean, false, 0, 0, 0, 0, 0};
            return bad("numeric or STRING");

        default:
            return errResult;
    }
}
