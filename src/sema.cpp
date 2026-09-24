#include "sema.h"
#include <unordered_set>

Sema::Sema(Diagnostics& diag) : diag_(diag) {}

void Sema::resolveType(TypeInfo& t) {
    if (t.base == BaseType::Record) {
        if (classTypes_.find(t.recordName) != classTypes_.end()) {
            t.base = BaseType::Object;
        }
    }
}

bool Sema::isSubclass(const std::string& sub, const std::string& super) const {
    std::string cur = sub;
    std::unordered_set<std::string> visited;
    while (!cur.empty() && visited.insert(cur).second) {
        auto it = classTypes_.find(cur);
        if (it == classTypes_.end()) break;
        if (it->second.superClass == super) return true;
        cur = it->second.superClass;
    }
    return false;
}

const ClassProperty* Sema::lookupClassProperty(const std::string& className, const std::string& fieldName, std::string* outDeclaringClass) const {
    std::string cur = className;
    std::unordered_set<std::string> visited;
    while (!cur.empty() && visited.insert(cur).second) {
        auto it = classTypes_.find(cur);
        if (it == classTypes_.end()) break;
        for (const auto& p : it->second.properties) {
            if (p.name == fieldName) {
                if (outDeclaringClass) *outDeclaringClass = cur;
                return &p;
            }
        }
        cur = it->second.superClass;
    }
    return nullptr;
}

const Sema::FunctionSig* Sema::lookupClassMethod(const std::string& className, const std::string& methodName, bool* outIsPrivate, std::string* outDeclaringClass) const {
    std::string cur = className;
    std::unordered_set<std::string> visited;
    while (!cur.empty() && visited.insert(cur).second) {
        auto it = classTypes_.find(cur);
        if (it == classTypes_.end()) break;
        auto mit = it->second.methods.find(methodName);
        if (mit != it->second.methods.end()) {
            if (outIsPrivate) {
                auto vit = it->second.methodVisibility.find(methodName);
                *outIsPrivate = (vit != it->second.methodVisibility.end()) ? vit->second : false;
            }
            if (outDeclaringClass) *outDeclaringClass = cur;
            return &mit->second;
        }
        cur = it->second.superClass;
    }
    return nullptr;
}

void Sema::run(Block& program) {
    // Pass 1: Collect class declarations
    for (auto& s : program) {
        if (s->kind == Stmt::Kind::ClassDecl) {
            auto& cd = static_cast<ClassDeclStmt&>(*s);
            if (classTypes_.find(cd.name) != classTypes_.end()) {
                err(cd.line, cd.col, "class '" + cd.name + "' is already defined");
                continue;
            }
            ClassInfo cinfo;
            cinfo.name = cd.name;
            cinfo.superClass = cd.superClass;
            cinfo.line = cd.line;
            cinfo.col = cd.col;
            for (auto& p : cd.properties) {
                resolveType(p.type);
                cinfo.properties.push_back(p);
            }
            for (auto& m : cd.methods) {
                resolveType(m->returnType);
                for (auto& param : m->params) {
                    resolveType(param.type);
                }
                FunctionSig sig{m->name, m->isFunction, m->returnType, m->params, m->line};
                cinfo.methods[m->name] = sig;
                cinfo.methodVisibility[m->name] = m->isPrivate;
            }
            classTypes_[cd.name] = std::move(cinfo);
        }
    }

    // Pass 2: Verify superclasses and inheritance cycles
    for (const auto& kv : classTypes_) {
        const auto& c = kv.second;
        if (!c.superClass.empty()) {
            if (classTypes_.find(c.superClass) == classTypes_.end()) {
                err(c.line, c.col, "class '" + c.name + "' inherits from unknown class '" + c.superClass + "'");
            } else if (isSubclass(c.superClass, c.name)) {
                err(c.line, c.col, "cyclical inheritance detected between '" + c.name + "' and '" + c.superClass + "'");
            }
        }
    }

    // Pass 3: Check statements and method bodies
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

bool Sema::assignable(const TypeInfo& to, const TypeInfo& from) const {
    if (to.isArray || from.isArray) return to == from;
    if (to.base == from.base) {
        if (to.base == BaseType::Record) return to.recordName == from.recordName;
        if (to.base == BaseType::Object) {
            if (to.recordName == from.recordName) return true;
            return isSubclass(from.recordName, to.recordName);
        }
        return true;
    }
    if (to.base == BaseType::Real && from.base == BaseType::Integer) return true;
    if (to.base == BaseType::String && from.base == BaseType::Char) return true;
    if (to.base == BaseType::Char && from.base == BaseType::String) return true;
    return false;
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
                localSymbols_[param.name] = Symbol{param.type, param.line, param.isByRef, false, static_cast<int>(i)};
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
                localSymbols_[param.name] = Symbol{param.type, param.line, param.isByRef, false, static_cast<int>(i)};
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

        case Stmt::Kind::ClassDecl: {
            auto& cd = static_cast<ClassDeclStmt&>(s);
            for (auto& m : cd.methods) {
                bool prevInProc = insideProcedure_;
                bool prevInFunc = insideFunction_;
                TypeInfo prevRetType = currentReturnType_;
                std::string prevClass = currentClassName_;
                auto prevLocalSyms = std::move(localSymbols_);
                auto prevLocalOrder = std::move(localOrder_);

                currentClassName_ = cd.name;
                insideProcedure_ = !m->isFunction;
                insideFunction_ = m->isFunction;
                currentReturnType_ = m->returnType;
                localSymbols_.clear();
                localOrder_.clear();

                // Slot 0: THIS / this
                TypeInfo thisType{BaseType::Object, cd.name, false, 0, 0, 0, 0, 0};
                localSymbols_["THIS"] = Symbol{thisType, m->line, false, false, 0};
                localSymbols_["this"] = Symbol{thisType, m->line, false, false, 0};
                localOrder_.emplace_back("THIS", thisType);

                for (size_t i = 0; i < m->params.size(); ++i) {
                    const auto& param = m->params[i];
                    localSymbols_[param.name] = Symbol{param.type, param.line, param.isByRef, false, static_cast<int>(i + 1)};
                    localOrder_.emplace_back(param.name, param.type);
                }

                checkBlock(m->body);

                insideProcedure_ = prevInProc;
                insideFunction_ = prevInFunc;
                currentReturnType_ = prevRetType;
                currentClassName_ = prevClass;
                localSymbols_ = std::move(prevLocalSyms);
                localOrder_ = std::move(prevLocalOrder);
            }
            break;
        }

        case Stmt::Kind::Call: {
            auto& c = static_cast<CallStmt&>(s);
            if (c.target || c.isSuper) {
                std::string className;
                if (c.isSuper) {
                    if (currentClassName_.empty()) {
                        err(c.line, c.col, "SUPER call used outside of a class method");
                        break;
                    }
                    auto cit = classTypes_.find(currentClassName_);
                    if (cit == classTypes_.end() || cit->second.superClass.empty()) {
                        err(c.line, c.col, "class '" + currentClassName_ + "' does not inherit from any superclass");
                        break;
                    }
                    className = cit->second.superClass;
                } else {
                    TypeInfo tgt = typeOf(*c.target);
                    if (tgt.base != BaseType::Object || tgt.isArray) {
                        err(c.line, c.col, "method call requires an object instance, found " + typeString(tgt));
                        break;
                    }
                    className = tgt.recordName;
                }

                bool isPrivate = false;
                std::string decClass;
                const FunctionSig* sig = lookupClassMethod(className, c.name, &isPrivate, &decClass);
                if (!sig) {
                    err(c.line, c.col, "class '" + className + "' has no method named '" + c.name + "'");
                    break;
                }
                if (isPrivate && currentClassName_ != decClass) {
                    err(c.line, c.col, "cannot call private method '" + c.name + "' of class '" + decClass + "'");
                }
                if (c.args.size() != sig->params.size()) {
                    err(c.line, c.col, "method '" + c.name + "' expects " +
                        std::to_string(sig->params.size()) + " arguments, found " +
                        std::to_string(c.args.size()));
                    break;
                }
                for (size_t i = 0; i < c.args.size(); ++i) {
                    TypeInfo argType = typeOf(*c.args[i]);
                    const auto& param = sig->params[i];
                    if (param.isByRef) {
                        if (c.args[i]->kind != Expr::Kind::Var &&
                            c.args[i]->kind != Expr::Kind::ArrayAccess &&
                            c.args[i]->kind != Expr::Kind::MemberAccess) {
                            err(c.args[i]->line, c.args[i]->col, "BYREF parameter '" + param.name +
                                "' requires an lvalue");
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
                    } else if (c.args[i]->kind == Expr::Kind::Var) {
                        auto& v = static_cast<VarExpr&>(*c.args[i]);
                        Symbol* sym = lookup(v.name);
                        if (sym && sym->isConstant) {
                            err(c.args[i]->line, c.args[i]->col, "cannot pass constant '" + v.name + "' by reference");
                        }
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
                resolveType(d.declaredType);
                if (d.declaredType.base == BaseType::Record) {
                    if (recordTypes_.find(d.declaredType.recordName) == recordTypes_.end()) {
                        err(d.line, d.col, "unknown record or class type '" + d.declaredType.recordName + "'");
                    }
                } else if (d.declaredType.base == BaseType::Object) {
                    if (classTypes_.find(d.declaredType.recordName) == classTypes_.end()) {
                        err(d.line, d.col, "unknown class '" + d.declaredType.recordName + "'");
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
                    localSymbols_[d.name] = Symbol{d.declaredType, d.line, false, false, slot};
                    localOrder_.emplace_back(d.name, d.declaredType);
                } else {
                    symbols_[d.name] = Symbol{d.declaredType, d.line, false, false, -1};
                    order_.emplace_back(d.name, d.declaredType);
                }
            }
            break;
        }

        case Stmt::Kind::Constant: {
            auto& c = static_cast<ConstantStmt&>(s);
            if (Symbol* prev = lookup(c.name)) {
                err(c.line, c.col, "'" + c.name + "' is already declared (line " +
                    std::to_string(prev->line) + ")");
            } else {
                TypeInfo valType = typeOf(*c.value);
                TypeInfo constType = valType;
                if (c.explicitType.base != BaseType::Error) {
                    if (!assignable(c.explicitType, valType)) {
                        err(c.line, c.col, "cannot initialize constant of type " +
                            typeString(c.explicitType) + " with value of type " + typeString(valType));
                    }
                    constType = c.explicitType;
                }
                if (insideFunction_ || insideProcedure_) {
                    int slot = static_cast<int>(localOrder_.size());
                    localSymbols_[c.name] = Symbol{constType, c.line, false, true, slot};
                    localOrder_.emplace_back(c.name, constType);
                } else {
                    symbols_[c.name] = Symbol{constType, c.line, false, true, -1};
                    order_.emplace_back(c.name, constType);
                }
            }
            break;
        }

        case Stmt::Kind::Assign: {
            auto& a = static_cast<AssignStmt&>(s);
            Symbol* sym = lookup(a.name);
            if (!sym) {
                if (!currentClassName_.empty()) {
                    std::string decClass;
                    const ClassProperty* cp = lookupClassProperty(currentClassName_, a.name, &decClass);
                    if (cp) {
                        if (cp->isPrivate && currentClassName_ != decClass) {
                            err(a.line, a.col, "cannot access private property '" + a.name + "' of class '" + decClass + "'");
                        }
                        TypeInfo value = typeOf(*a.value);
                        if (value.base != BaseType::Error && !assignable(cp->type, value)) {
                            err(a.line, a.col, "cannot assign " + typeString(value) + " to property '" +
                                a.name + "' of type " + typeString(cp->type));
                        }
                        break;
                    }
                }
                err(a.line, a.col, "'" + a.name + "' is not declared");
            }
            if (sym && sym->isConstant) {
                err(a.line, a.col, "cannot assign to constant '" + a.name + "'");
            }
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
            if (targetType.base == BaseType::Record && !targetType.isArray) {
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
            } else if (targetType.base == BaseType::Object && !targetType.isArray) {
                std::string decClass;
                const ClassProperty* p = lookupClassProperty(targetType.recordName, m.field, &decClass);
                if (!p) {
                    err(m.line, m.col, "class '" + targetType.recordName + "' has no property named '" + m.field + "'");
                } else {
                    if (p->isPrivate && currentClassName_ != decClass) {
                        err(m.line, m.col, "cannot access private property '" + m.field + "' of class '" + decClass + "'");
                    }
                    TypeInfo valType = typeOf(*m.value);
                    if (!assignable(p->type, valType)) {
                        err(m.line, m.col, "cannot assign " + typeString(valType) + " to property '" +
                            m.field + "' of type " + typeString(p->type));
                    }
                }
            } else {
                err(m.line, m.col, "member assignment requires a record or class object, found " + typeString(targetType));
            }
            break;
        }

        case Stmt::Kind::Output: {
            for (auto& arg : static_cast<OutputStmt&>(s).args) {
                TypeInfo t = typeOf(*arg);
                if (t.isArray) err(arg->line, arg->col, "cannot output an entire array");
                if (t.base == BaseType::Record) err(arg->line, arg->col, "cannot output an entire record");
                if (t.base == BaseType::Object) err(arg->line, arg->col, "cannot output an entire object");
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
                } else if (sym->isConstant) {
                    err(in.line, in.col, "cannot INPUT into constant '" + in.name + "'");
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

        case Stmt::Kind::Repeat: {
            auto& r = static_cast<RepeatStmt&>(s);
            checkBlock(r.body);
            requireType(*r.cond, typeOf(*r.cond), BaseType::Boolean, "UNTIL condition");
            break;
        }

        case Stmt::Kind::For: {
            auto& f = static_cast<ForStmt&>(s);
            Symbol* sym = lookup(f.var);
            if (!sym)
                err(f.line, f.col, "'" + f.var + "' is not declared");
            else if (sym->isConstant)
                err(f.line, f.col, "FOR loop variable '" + f.var + "' cannot be a constant");
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
                case Tok::CharLit: return {BaseType::Char, "", false, 0, 0, 0, 0, 0};
                default:           return {BaseType::Boolean, "", false, 0, 0, 0, 0, 0};
            }
        }
        case Expr::Kind::Var: {
            auto& v = static_cast<VarExpr&>(e);
            Symbol* sym = lookup(v.name);
            if (!sym) {
                if (!currentClassName_.empty()) {
                    std::string decClass;
                    const ClassProperty* cp = lookupClassProperty(currentClassName_, v.name, &decClass);
                    if (cp) {
                        if (cp->isPrivate && currentClassName_ != decClass) {
                            err(v.line, v.col, "cannot access private property '" + v.name + "' of class '" + decClass + "'");
                            return errType;
                        }
                        return cp->type;
                    }
                }
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
            if (tgt.base == BaseType::Record && !tgt.isArray) {
                const FieldDef* f = lookupField(tgt.recordName, m.field);
                if (!f) {
                    err(m.line, m.col, "record '" + tgt.recordName + "' has no field named '" + m.field + "'");
                    return errType;
                }
                return f->type;
            } else if (tgt.base == BaseType::Object && !tgt.isArray) {
                std::string decClass;
                const ClassProperty* p = lookupClassProperty(tgt.recordName, m.field, &decClass);
                if (!p) {
                    err(m.line, m.col, "class '" + tgt.recordName + "' has no property named '" + m.field + "'");
                    return errType;
                }
                if (p->isPrivate && currentClassName_ != decClass) {
                    err(m.line, m.col, "cannot access private property '" + m.field + "' of class '" + decClass + "'");
                    return errType;
                }
                return p->type;
            }
            err(m.line, m.col, "member access '.' requires a record or class object, found " + typeString(tgt));
            return errType;
        }
        case Expr::Kind::New: {
            auto& n = static_cast<NewExpr&>(e);
            auto it = classTypes_.find(n.className);
            if (it == classTypes_.end()) {
                err(n.line, n.col, "unknown class '" + n.className + "'");
                return errType;
            }
            const FunctionSig* ctor = lookupClassMethod(n.className, "NEW");
            if (ctor) {
                if (n.args.size() != ctor->params.size()) {
                    err(n.line, n.col, "constructor for class '" + n.className + "' expects " +
                        std::to_string(ctor->params.size()) + " arguments, found " + std::to_string(n.args.size()));
                } else {
                    for (size_t i = 0; i < n.args.size(); ++i) {
                        TypeInfo argType = typeOf(*n.args[i]);
                        if (!assignable(ctor->params[i].type, argType)) {
                            err(n.args[i]->line, n.args[i]->col, "type mismatch in argument " + std::to_string(i + 1) +
                                ": cannot pass " + typeString(argType) + " to parameter of type " + typeString(ctor->params[i].type));
                        }
                    }
                }
            } else {
                if (!n.args.empty()) {
                    err(n.line, n.col, "class '" + n.className + "' has no constructor taking " +
                        std::to_string(n.args.size()) + " arguments");
                }
            }
            return TypeInfo{BaseType::Object, n.className, false, 0, 0, 0, 0, 0};
        }
        case Expr::Kind::MethodCall: {
            auto& m = static_cast<MethodCallExpr&>(e);
            std::string className;
            if (m.isSuper) {
                if (currentClassName_.empty()) {
                    err(m.line, m.col, "SUPER call used outside of a class method");
                    return errType;
                }
                auto cit = classTypes_.find(currentClassName_);
                if (cit == classTypes_.end() || cit->second.superClass.empty()) {
                    err(m.line, m.col, "class '" + currentClassName_ + "' does not inherit from any superclass");
                    return errType;
                }
                className = cit->second.superClass;
            } else {
                TypeInfo tgt = typeOf(*m.target);
                if (tgt.base != BaseType::Object || tgt.isArray) {
                    err(m.line, m.col, "method call requires an object instance, found " + typeString(tgt));
                    return errType;
                }
                className = tgt.recordName;
            }

            bool isPrivate = false;
            std::string decClass;
            const FunctionSig* sig = lookupClassMethod(className, m.method, &isPrivate, &decClass);
            if (!sig) {
                err(m.line, m.col, "class '" + className + "' has no method named '" + m.method + "'");
                return errType;
            }
            if (isPrivate && currentClassName_ != decClass) {
                err(m.line, m.col, "cannot call private method '" + m.method + "' of class '" + decClass + "'");
            }
            if (!sig->isFunction && m.method != "NEW") {
                err(m.line, m.col, "cannot use procedure method '" + m.method + "' in an expression; use CALL");
                return errType;
            }
            if (m.args.size() != sig->params.size()) {
                err(m.line, m.col, "method '" + m.method + "' expects " +
                    std::to_string(sig->params.size()) + " arguments, found " + std::to_string(m.args.size()));
            } else {
                for (size_t i = 0; i < m.args.size(); ++i) {
                    TypeInfo argType = typeOf(*m.args[i]);
                    if (!assignable(sig->params[i].type, argType)) {
                        err(m.args[i]->line, m.args[i]->col, "type mismatch in argument " + std::to_string(i + 1) +
                            ": cannot pass " + typeString(argType) + " to parameter of type " + typeString(sig->params[i].type));
                    }
                }
            }
            return sig->returnType;
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
            } else if (c.args[i]->kind == Expr::Kind::Var) {
                auto& v = static_cast<VarExpr&>(*c.args[i]);
                Symbol* sym = lookup(v.name);
                if (sym && sym->isConstant) {
                    err(c.args[i]->line, c.args[i]->col, "cannot pass constant '" + v.name + "' by reference");
                }
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
                TypeInfo t = typeOf(*c.args[0]);
                if (t.base != BaseType::String && t.base != BaseType::Char) {
                    requireType(*c.args[0], t, BaseType::String, "LENGTH argument");
                }
            }
            return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
        case Tok::Substring:
        case Tok::Mid:
            if (checkArgCount(3)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "SUBSTRING/MID argument 1");
                requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "SUBSTRING/MID argument 2 (start)");
                requireType(*c.args[2], typeOf(*c.args[2]), BaseType::Integer, "SUBSTRING/MID argument 3 (length)");
            }
            return {BaseType::String, "", false, 0, 0, 0, 0, 0};
        case Tok::Left:
        case Tok::Right:
            if (checkArgCount(2)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::String, "LEFT/RIGHT argument 1");
                requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "LEFT/RIGHT argument 2 (length)");
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
        case Tok::Chr:
            if (checkArgCount(1)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::Integer, "CHR argument");
            }
            return {BaseType::Char, "", false, 0, 0, 0, 0, 0};
        case Tok::Asc:
            if (checkArgCount(1)) {
                TypeInfo t = typeOf(*c.args[0]);
                if (t.base != BaseType::String && t.base != BaseType::Char) {
                    requireType(*c.args[0], t, BaseType::String, "ASC argument");
                }
            }
            return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
        case Tok::IntFunc:
            if (checkArgCount(1)) {
                TypeInfo t = typeOf(*c.args[0]);
                if (!isNumeric(t)) {
                    err(c.line, c.col, "INT requires a numeric argument, found " + typeString(t));
                }
            }
            return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
        case Tok::Round:
            if (checkArgCount(2)) {
                TypeInfo t = typeOf(*c.args[0]);
                if (!isNumeric(t)) {
                    err(c.line, c.col, "ROUND requires a numeric first argument, found " + typeString(t));
                }
                requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "ROUND argument 2 (places)");
            }
            return {BaseType::Real, "", false, 0, 0, 0, 0, 0};
        case Tok::Rnd:
            if (checkArgCount(0)) {}
            return {BaseType::Real, "", false, 0, 0, 0, 0, 0};
        case Tok::Mod:
            if (checkArgCount(2)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::Integer, "MOD argument 1");
                requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "MOD argument 2");
            }
            return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
        case Tok::Div:
            if (checkArgCount(2)) {
                requireType(*c.args[0], typeOf(*c.args[0]), BaseType::Integer, "DIV argument 1");
                requireType(*c.args[1], typeOf(*c.args[1]), BaseType::Integer, "DIV argument 2");
            }
            return {BaseType::Integer, "", false, 0, 0, 0, 0, 0};
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
