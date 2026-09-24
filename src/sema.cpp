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
    if (insideFunction_ || insideProcedure_) {
        auto it = localSymbols_.find(name);
        if (it != localSymbols_.end()) return &it->second;
    }
    auto it = symbols_.find(name);
    return it == symbols_.end() ? nullptr : &it->second;
}

bool Sema::assignable(const TypeInfo& to, const TypeInfo& from) {
    if (to.isArray || from.isArray) return to == from;
    if (to.base == from.base) {
        if (to.base == BaseType::Record) return to.recordName == from.recordName;
        return true;
    }
    return to.base == BaseType::Real && from.base == BaseType::Integer;
}

const FieldDef* Sema::lookupField(const std::string& recordName, const std::string& fieldName) {
    auto it = recordTypes_.find(recordName);
    if (it == recordTypes_.end()) return nullptr;
    for (const auto& f : it->second.fields) {
        if (f.name == fieldName) return &f;
    }
    return nullptr;
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
        case Stmt::Kind::TypeDecl: {
            auto& td = static_cast<TypeDeclStmt&>(s);
            if (recordTypes_.find(td.recordDef.name) != recordTypes_.end()) {
                err(td.line, td.col, "type '" + td.recordDef.name + "' is already defined");
                break;
            }
            std::unordered_map<std::string, int> fieldSet;
            for (const auto& f : td.recordDef.fields) {
                if (fieldSet.find(f.name) != fieldSet.end()) {
                    err(f.line, f.col, "duplicate field name '" + f.name + "' in type '" + td.recordDef.name + "'");
                }
                fieldSet[f.name] = f.line;
                if (f.type.base == BaseType::Record && recordTypes_.find(f.type.recordName) == recordTypes_.end()) {
                    err(f.line, f.col, "unknown record type '" + f.type.recordName + "' in field definition");
                }
            }
            recordTypes_[td.recordDef.name] = td.recordDef;
            break;
        }

        case Stmt::Kind::ProcedureDecl: {
            auto& p = static_cast<ProcedureDeclStmt&>(s);
            if (functions_.find(p.name) != functions_.end()) {
                err(p.line, p.col, "procedure/function '" + p.name + "' is already declared");
                break;
            }
            FunctionSig sig{p.name, false, {BaseType::Void, "", false, 0, 0, 0, 0, 0}, p.params, p.line};
            functions_[p.name] = sig;

            bool prevInProc = insideProcedure_;
            bool prevInFunc = insideFunction_;
            auto prevLocalSyms = std::move(localSymbols_);
            auto prevLocalOrder = std::move(localOrder_);

            insideProcedure_ = true;
            insideFunction_ = false;
            localSymbols_.clear();
            localOrder_.clear();

            for (size_t i = 0; i < p.params.size(); ++i) {
                const auto& param = p.params[i];
                localSymbols_[param.name] = Symbol{param.type, param.line, param.isByRef, static_cast<int>(i)};
                localOrder_.emplace_back(param.name, param.type);
            }

            checkBlock(p.body);

            insideProcedure_ = prevInProc;
            insideFunction_ = prevInFunc;
            localSymbols_ = std::move(prevLocalSyms);
            localOrder_ = std::move(prevLocalOrder);
            break;
        }

        case Stmt::Kind::FunctionDecl: {
            auto& f = static_cast<FunctionDeclStmt&>(s);
            if (functions_.find(f.name) != functions_.end()) {
                err(f.line, f.col, "procedure/function '" + f.name + "' is already declared");
                break;
            }
            FunctionSig sig{f.name, true, f.returnType, f.params, f.line};
            functions_[f.name] = sig;

            bool prevInProc = insideProcedure_;
            bool prevInFunc = insideFunction_;
            TypeInfo prevRetType = currentReturnType_;
            auto prevLocalSyms = std::move(localSymbols_);
            auto prevLocalOrder = std::move(localOrder_);

            insideProcedure_ = false;
            insideFunction_ = true;
            currentReturnType_ = f.returnType;
            localSymbols_.clear();
            localOrder_.clear();

            for (size_t i = 0; i < f.params.size(); ++i) {
                const auto& param = f.params[i];
                localSymbols_[param.name] = Symbol{param.type, param.line, param.isByRef, static_cast<int>(i)};
                localOrder_.emplace_back(param.name, param.type);
            }

            checkBlock(f.body);

            insideProcedure_ = prevInProc;
            insideFunction_ = prevInFunc;
            currentReturnType_ = prevRetType;
            localSymbols_ = std::move(prevLocalSyms);
            localOrder_ = std::move(prevLocalOrder);
            break;
        }

        case Stmt::Kind::Call: {
            auto& c = static_cast<CallStmt&>(s);
            auto it = functions_.find(c.name);
            if (it == functions_.end()) {
                err(c.line, c.col, "procedure '" + c.name + "' is not declared");
                break;
            }
            if (it->second.isFunction) {
                err(c.line, c.col, "'" + c.name + "' is a FUNCTION, cannot be called as a statement (use expression)");
            }
            if (c.args.size() != it->second.params.size()) {
                err(c.line, c.col, "procedure '" + c.name + "' expects " +
                    std::to_string(it->second.params.size()) + " arguments, found " +
                    std::to_string(c.args.size()));
                break;
            }
            for (size_t i = 0; i < c.args.size(); ++i) {
                TypeInfo argType = typeOf(*c.args[i]);
                const auto& param = it->second.params[i];
                if (param.isByRef) {
                    if (c.args[i]->kind != Expr::Kind::Var &&
                        c.args[i]->kind != Expr::Kind::ArrayAccess &&
                        c.args[i]->kind != Expr::Kind::MemberAccess) {
                        err(c.args[i]->line, c.args[i]->col, "BYREF parameter '" + param.name +
                            "' requires an lvalue (variable, array element, or record field)");
                    }
                    if (argType != param.type) {
                        err(c.args[i]->line, c.args[i]->col, "BYREF argument type mismatch: expected " +
                            typeString(param.type) + ", found " + typeString(argType));
                    }
                } else {
                    if (!assignable(param.type, argType)) {
                        err(c.args[i]->line, c.args[i]->col, "argument " + std::to_string(i + 1) +
                            " cannot pass " + typeString(argType) + " to " + typeString(param.type));
                    }
                }
            }
            break;
        }

        case Stmt::Kind::Return: {
            auto& r = static_cast<ReturnStmt&>(s);
            if (insideFunction_) {
                if (!r.value) {
                    err(r.line, r.col, "FUNCTION must return a value matching " + typeString(currentReturnType_));
                } else {
                    TypeInfo valType = typeOf(*r.value);
                    if (!assignable(currentReturnType_, valType)) {
                        err(r.line, r.col, "cannot return " + typeString(valType) +
                            " from function returning " + typeString(currentReturnType_));
                    }
                }
            } else if (insideProcedure_) {
                if (r.value) {
                    err(r.line, r.col, "PROCEDURE cannot return a value");
                }
            } else {
                err(r.line, r.col, "RETURN cannot be used outside a FUNCTION or PROCEDURE");
            }
            break;
        }

        case Stmt::Kind::Declare: {
            auto& d = static_cast<DeclareStmt&>(s);
            if (Symbol* prev = lookup(d.name)) {
                err(d.line, d.col, "'" + d.name + "' is already declared (line " +
                    std::to_string(prev->line) + ")");
            } else {
                if (d.declaredType.base == BaseType::Record) {
                    if (recordTypes_.find(d.declaredType.recordName) == recordTypes_.end()) {
                        err(d.line, d.col, "unknown record type '" + d.declaredType.recordName + "'");
                    }
                }
                if (d.declaredType.isArray) {
                    if (d.declaredType.lower1 > d.declaredType.upper1)
                        err(d.line, d.col, "invalid array bounds: lower bound > upper bound");
                    if (d.declaredType.dims == 2 && d.declaredType.lower2 > d.declaredType.upper2)
                        err(d.line, d.col, "invalid second-dimension array bounds: lower bound > upper bound");
                }
                if (insideFunction_ || insideProcedure_) {
                    int slot = static_cast<int>(localOrder_.size());
                    localSymbols_[d.name] = Symbol{d.declaredType, d.line, false, slot};
                    localOrder_.emplace_back(d.name, d.declaredType);
                } else {
                    symbols_[d.name] = Symbol{d.declaredType, d.line, false, -1};
                    order_.emplace_back(d.name, d.declaredType);
                }
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
            TypeInfo arrType{BaseType::Error, "", false, 0, 0, 0, 0, 0};
            if (a.target) {
                arrType = typeOf(*a.target);
            } else {
                Symbol* sym = lookup(a.name);
                if (!sym) err(a.line, a.col, "'" + a.name + "' is not declared");
                else arrType = sym->type;
            }

            if (!arrType.isArray) {
                err(a.line, a.col, "cannot index non-array type " + typeString(arrType));
            } else {
                if (static_cast<int>(a.indices.size()) != arrType.dims) {
                    err(a.line, a.col, "array expects " + std::to_string(arrType.dims) +
                        " indices, found " + std::to_string(a.indices.size()));
                }
                for (auto& idx : a.indices) {
                    requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
                }
            }

            TypeInfo val = typeOf(*a.value);
            if (arrType.isArray && val.base != BaseType::Error) {
                TypeInfo elemType{arrType.base, arrType.recordName, false, 0, 0, 0, 0, 0};
                if (!assignable(elemType, val)) {
                    err(a.line, a.col, "cannot assign " + typeString(val) + " to array of " + typeString(elemType));
                }
            }
            break;
        }

        case Stmt::Kind::MemberAssign: {
            auto& m = static_cast<MemberAssignStmt&>(s);
            TypeInfo targetType = typeOf(*m.target);
            if (targetType.base != BaseType::Record || targetType.isArray) {
                err(m.line, m.col, "member assignment requires a record object, found " + typeString(targetType));
            } else {
                const FieldDef* f = lookupField(targetType.recordName, m.field);
                if (!f) {
                    err(m.line, m.col, "record '" + targetType.recordName + "' has no field named '" + m.field + "'");
                } else {
                    TypeInfo valType = typeOf(*m.value);
                    if (!assignable(f->type, valType)) {
                        err(m.line, m.col, "cannot assign " + typeString(valType) + " to field '" +
                            m.field + "' of type " + typeString(f->type));
                    }
                }
            }
            break;
        }

        case Stmt::Kind::Output: {
            for (auto& arg : static_cast<OutputStmt&>(s).args) {
                TypeInfo t = typeOf(*arg);
                if (t.isArray) err(arg->line, arg->col, "cannot output an entire array");
                if (t.base == BaseType::Record) err(arg->line, arg->col, "cannot output an entire record");
            }
            break;
        }

        case Stmt::Kind::Input: {
            auto& in = static_cast<InputStmt&>(s);
            TypeInfo targetType{BaseType::Error, "", false, 0, 0, 0, 0, 0};
            if (in.target) {
                targetType = typeOf(*in.target);
            } else {
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
                        targetType = TypeInfo{sym->type.base, sym->type.recordName, false, 0, 0, 0, 0, 0};
                    }
                } else if (!in.indices.empty()) {
                    err(in.line, in.col, "'" + in.name + "' is not an array");
                } else {
                    targetType = sym->type;
                }
            }
            if (targetType.base == BaseType::Record) {
                err(in.line, in.col, "cannot INPUT into a record directly (read into a scalar field)");
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

        case Stmt::Kind::Case: {
            auto& c = static_cast<CaseStmt&>(s);
            TypeInfo selType = typeOf(*c.selector);
            if (selType.isArray || (selType.base != BaseType::Integer && selType.base != BaseType::String && selType.base != BaseType::Boolean)) {
                err(c.line, c.col, "CASE OF selector must be INTEGER, STRING, or BOOLEAN");
            }
            for (auto& b : c.branches) {
                for (auto& v : b.values) {
                    TypeInfo vType = typeOf(*v);
                    if (vType != selType) {
                        err(v->line, v->col, "case label type " + typeString(vType) + " does not match selector " + typeString(selType));
                    }
                }
                checkBlock(b.body);
            }
            checkBlock(c.otherwiseBlock);
            break;
        }

        case Stmt::Kind::OpenFile: {
            auto& o = static_cast<OpenFileStmt&>(s);
            requireType(*o.filename, typeOf(*o.filename), BaseType::String, "OPENFILE filename");
            break;
        }

        case Stmt::Kind::CloseFile: {
            auto& cf = static_cast<CloseFileStmt&>(s);
            requireType(*cf.filename, typeOf(*cf.filename), BaseType::String, "CLOSEFILE filename");
            break;
        }

        case Stmt::Kind::ReadFile: {
            auto& rf = static_cast<ReadFileStmt&>(s);
            requireType(*rf.filename, typeOf(*rf.filename), BaseType::String, "READFILE filename");
            TypeInfo tgt = typeOf(*rf.target);
            if (tgt.isArray || tgt.base == BaseType::Record) {
                err(rf.target->line, rf.target->col, "READFILE target must be a scalar variable, array element, or record field");
            }
            break;
        }

        case Stmt::Kind::WriteFile: {
            auto& wf = static_cast<WriteFileStmt&>(s);
            requireType(*wf.filename, typeOf(*wf.filename), BaseType::String, "WRITEFILE filename");
            TypeInfo val = typeOf(*wf.value);
            if (val.isArray || val.base == BaseType::Record) {
                err(wf.value->line, wf.value->col, "WRITEFILE value must be a scalar expression");
            }
            break;
        }
    }
}

TypeInfo Sema::typeOf(Expr& e) {
    e.type = computeType(e);
    return e.type;
}

TypeInfo Sema::computeType(Expr& e) {
    TypeInfo errType{BaseType::Error, "", false, 0, 0, 0, 0, 0};
    switch (e.kind) {
        case Expr::Kind::Literal: {
            switch (static_cast<LiteralExpr&>(e).litType) {
                case Tok::IntLit:  return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
                case Tok::RealLit: return {BaseType::Real, "", false, 0, 0, 0, 0, 0};
                case Tok::StrLit:  return {BaseType::String, "", false, 0, 0, 0, 0, 0};
                default:           return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
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
            TypeInfo arrType = errType;
            if (a.target) {
                arrType = typeOf(*a.target);
            } else {
                Symbol* sym = lookup(a.name);
                if (!sym) {
                    err(a.line, a.col, "'" + a.name + "' is not declared");
                    return errType;
                }
                arrType = sym->type;
            }

            if (!arrType.isArray) {
                err(a.line, a.col, "cannot index non-array type " + typeString(arrType));
                return errType;
            }
            if (static_cast<int>(a.indices.size()) != arrType.dims) {
                err(a.line, a.col, "array expects " + std::to_string(arrType.dims) +
                    " indices, found " + std::to_string(a.indices.size()));
            }
            for (auto& idx : a.indices) {
                requireType(*idx, typeOf(*idx), BaseType::Integer, "array index");
            }
            return {arrType.base, arrType.recordName, false, 0, 0, 0, 0, 0};
        }
        case Expr::Kind::MemberAccess: {
            auto& m = static_cast<MemberAccessExpr&>(e);
            TypeInfo tgt = typeOf(*m.target);
            if (tgt.base != BaseType::Record || tgt.isArray) {
                err(m.line, m.col, "member access '.' requires a record object, found " + typeString(tgt));
                return errType;
            }
            const FieldDef* f = lookupField(tgt.recordName, m.field);
            if (!f) {
                err(m.line, m.col, "record '" + tgt.recordName + "' has no field named '" + m.field + "'");
                return errType;
            }
            return f->type;
        }
        case Expr::Kind::Unary: {
            auto& u = static_cast<UnaryExpr&>(e);
            TypeInfo t = typeOf(*u.operand);
            if (t.base == BaseType::Error) return u.op == Tok::Not ? TypeInfo{BaseType::Boolean, "", false, 0, 0, 0, 0, 0} : errType;
            if (u.op == Tok::Minus) {
                if (isNumeric(t)) return t;
                err(u.line, u.col, std::string("unary '-' needs a numeric operand, found ") + typeString(t));
                return errType;
            }
            if (t.base == BaseType::Boolean && !t.isArray) return t;
            err(u.line, u.col, std::string("'NOT' needs a BOOLEAN operand, found ") + typeString(t));
            return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
        }
        case Expr::Kind::Binary:
            return computeBinary(static_cast<BinaryExpr&>(e));
        case Expr::Kind::Call:
            return computeCall(static_cast<CallExpr&>(e));
        case Expr::Kind::UserCall:
            return computeUserCall(static_cast<UserCallExpr&>(e));
    }
    return errType;
}

TypeInfo Sema::computeUserCall(UserCallExpr& c) {
    TypeInfo errType{BaseType::Error, "", false, 0, 0, 0, 0, 0};
    auto it = functions_.find(c.callee);
    if (it == functions_.end()) {
        err(c.line, c.col, "function '" + c.callee + "' is not declared");
        return errType;
    }
    if (!it->second.isFunction) {
        err(c.line, c.col, "'" + c.callee + "' is a PROCEDURE, cannot be called in an expression");
        return errType;
    }
    if (c.args.size() != it->second.params.size()) {
        err(c.line, c.col, "function '" + c.callee + "' expects " +
            std::to_string(it->second.params.size()) + " arguments, found " +
            std::to_string(c.args.size()));
        return it->second.returnType;
    }
    for (size_t i = 0; i < c.args.size(); ++i) {
        TypeInfo argType = typeOf(*c.args[i]);
        const auto& param = it->second.params[i];
        if (param.isByRef) {
            if (c.args[i]->kind != Expr::Kind::Var &&
                c.args[i]->kind != Expr::Kind::ArrayAccess &&
                c.args[i]->kind != Expr::Kind::MemberAccess) {
                err(c.args[i]->line, c.args[i]->col, "BYREF parameter requires an lvalue");
            }
            if (argType != param.type) {
                err(c.args[i]->line, c.args[i]->col, "BYREF argument type mismatch");
            }
        } else {
            if (!assignable(param.type, argType)) {
                err(c.args[i]->line, c.args[i]->col, "cannot pass " + typeString(argType) +
                    " to parameter of type " + typeString(param.type));
            }
        }
    }
    return it->second.returnType;
}

TypeInfo Sema::computeCall(CallExpr& c) {
    TypeInfo errType{BaseType::Error, "", false, 0, 0, 0, 0, 0};
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
            return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
        case Tok::Substring:
            if (checkArgCount(3)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "SUBSTRING argument 1");
                requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "SUBSTRING argument 2 (start)");
                requireType(*c.args[2], typeOf(*c.args[2]), BaseType::Integer, "SUBSTRING argument 3 (length)");
            }
            return {BaseType::String, "", false, 0, 0, 0, 0, 0};
        case Tok::UCase:
        case Tok::LCase:
            if (checkArgCount(1)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "string function argument");
            }
            return {BaseType::String, "", false, 0, 0, 0, 0, 0};
        case Tok::NumToStr:
            if (checkArgCount(1)) {
                TypeInfo t = typeOf(*c.args[0]);
                if (!isNumeric(t)) {
                    err(c.line, c.col, "NUM_TO_STR requires a numeric argument, found " + typeString(t));
                }
            }
            return {BaseType::String, "", false, 0, 0, 0, 0, 0};
        case Tok::StrToNum:
            if (checkArgCount(1)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "STR_TO_NUM argument");
            }
            return {BaseType::Real, "", false, 0, 0, 0, 0, 0};
        case Tok::EofFunc:
            if (checkArgCount(1)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "EOF argument (filename)");
            }
            return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
        default:
            return errType;
    }
}

TypeInfo Sema::computeBinary(BinaryExpr& b) {
    TypeInfo l = typeOf(*b.lhs);
    TypeInfo r = typeOf(*b.rhs);
    bool boolResult = isComparison(b.op) || b.op == Tok::And || b.op == Tok::Or;
    TypeInfo errResult = boolResult ? TypeInfo{BaseType::Boolean, "", false, 0, 0, 0, 0, 0}
                                    : TypeInfo{BaseType::Error, "", false, 0, 0, 0, 0, 0};

    if (l.base == BaseType::Error || r.base == BaseType::Error) return errResult;
    if (l.isArray || r.isArray) {
        err(b.line, b.col, "cannot apply operators to whole array types");
        return errResult;
    }
    if (l.base == BaseType::Record || r.base == BaseType::Record) {
        err(b.line, b.col, "cannot apply operators to whole record types");
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
                    return {BaseType::Real, "", false, 0, 0, 0, 0, 0};
                return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
            }
            return bad("numeric");

        case Tok::Slash:
            if (isNumeric(l) && isNumeric(r))
                return {BaseType::Real, "", false, 0, 0, 0, 0, 0};
            return bad("numeric");

        case Tok::Div:
        case Tok::Mod:
            if (l.base == BaseType::Integer && r.base == BaseType::Integer)
                return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
            return bad("INTEGER");

        case Tok::Ampersand:
            if (l.base == BaseType::String && r.base == BaseType::String)
                return {BaseType::String, "", false, 0, 0, 0, 0, 0};
            return bad("STRING");

        case Tok::And:
        case Tok::Or:
            if (l.base == BaseType::Boolean && r.base == BaseType::Boolean)
                return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
            return bad("BOOLEAN");

        case Tok::Eq:
        case Tok::Neq:
            if (isNumeric(l) && isNumeric(r)) return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
            if (l.base == r.base) return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
            return bad("matching type");

        case Tok::Lt:
        case Tok::Le:
        case Tok::Gt:
        case Tok::Ge:
            if (isNumeric(l) && isNumeric(r)) return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
            if (l.base == BaseType::String && r.base == BaseType::String)
                return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
            return bad("numeric or STRING");

        default:
            return errResult;
    }
}
