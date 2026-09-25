#include "py_codegen.h"

PyCodeGen::PyCodeGen(const std::vector<std::pair<std::string, TypeInfo>>& vars,
                     const std::unordered_map<std::string, RecordDef>& records,
                     const std::unordered_map<std::string, Sema::FunctionSig>& funcs,
                     const std::unordered_map<std::string, Sema::ClassInfo>& classes)
    : vars_(vars), recordTypes_(records), functions_(funcs), classTypes_(classes) {
    for (const auto& v : vars_) varMap_[v.first] = v.second;
    for (const auto& kv : functions_) {
        const auto& fn = kv.second;
        if (!fn.isFunction) {
            std::vector<size_t> indices;
            for (size_t i = 0; i < fn.params.size(); ++i) {
                const auto& p = fn.params[i];
                if (p.isByRef && !p.type.isArray &&
                    p.type.base != BaseType::Record &&
                    p.type.base != BaseType::Object) {
                    indices.push_back(i);
                }
            }
            if (!indices.empty()) {
                byrefProcs_[fn.name] = indices;
            }
        }
    }
}

void PyCodeGen::line(const std::string& text) {
    if (text.empty()) {
        out_ << "\n";
    } else {
        out_ << std::string(indent_ * 4, ' ') << text << "\n";
    }
}

std::string PyCodeGen::escapePyStr(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:   out += c; break;
        }
    }
    out += "\"";
    return out;
}

bool PyCodeGen::isClassProperty(const std::string& name) const {
    if (currentClassName_.empty()) return false;
    std::string cur = currentClassName_;
    while (!cur.empty()) {
        auto it = classTypes_.find(cur);
        if (it == classTypes_.end()) break;
        for (const auto& p : it->second.properties) {
            if (p.name == name) return true;
        }
        cur = it->second.superClass;
    }
    return false;
}

void PyCodeGen::emitImports() {
    line("import sys");
    line("import math");
    line("import random");
    line("import copy");
    line("");
    line("# --- Pseudoc Runtime Helpers ---");
    line("_pc_files = {}");
    line("_pc_random_files = {}");
    line("def _pc_open_random(name):");
    line("    lines = []");
    line("    try:");
    line("        with open(name, 'r') as f:");
    line("            lines = [l.rstrip('\\r\\n') for l in f]");
    line("    except FileNotFoundError:");
    line("        pass");
    line("    _pc_random_files[name] = {'lines': lines, 'current': 1}");
    line("def _pc_seek(name, addr):");
    line("    if name not in _pc_random_files:");
    line("        raise RuntimeError(f\"File '{name}' not open for RANDOM\")");
    line("    _pc_random_files[name]['current'] = max(1, int(addr))");
    line("def _pc_put_record(name, val):");
    line("    if name not in _pc_random_files:");
    line("        raise RuntimeError(f\"File '{name}' not open for RANDOM\")");
    line("    rf = _pc_random_files[name]");
    line("    idx = rf['current'] - 1");
    line("    while len(rf['lines']) <= idx:");
    line("        rf['lines'].append('')");
    line("    rf['lines'][idx] = str(val)");
    line("    rf['current'] += 1");
    line("def _pc_get_record(name):");
    line("    if name not in _pc_random_files:");
    line("        raise RuntimeError(f\"File '{name}' not open for RANDOM\")");
    line("    rf = _pc_random_files[name]");
    line("    idx = rf['current'] - 1");
    line("    res = rf['lines'][idx] if idx < len(rf['lines']) else ''");
    line("    rf['current'] += 1");
    line("    return res");
    line("def _pc_close_random(name):");
    line("    if name in _pc_random_files:");
    line("        with open(name, 'w') as f:");
    line("            for l in _pc_random_files[name]['lines']:");
    line("                f.write(l + '\\n')");
    line("        del _pc_random_files[name]");
    line("def _pc_open(name, mode):");
    line("    if mode == 'RANDOM':");
    line("        _pc_open_random(name)");
    line("        return");
    line("    m = 'r'");
    line("    if mode == 'WRITE': m = 'w'");
    line("    elif mode == 'APPEND': m = 'a'");
    line("    _pc_files[name] = open(name, m)");
    line("def _pc_read(name):");
    line("    return _pc_files[name].readline().rstrip('\\r\\n')");
    line("def _pc_write(name, val):");
    line("    _pc_files[name].write(str(val) + '\\n')");
    line("def _pc_eof(name):");
    line("    if name in _pc_random_files:");
    line("        return _pc_random_files[name]['current'] > len(_pc_random_files[name]['lines'])");
    line("    f = _pc_files[name]");
    line("    pos = f.tell()");
    line("    line = f.readline()");
    line("    f.seek(pos)");
    line("    return len(line) == 0");
    line("def _pc_close(name):");
    line("    if name in _pc_random_files:");
    line("        _pc_close_random(name)");
    line("        return");
    line("    if name in _pc_files:");
    line("        _pc_files[name].close()");
    line("        del _pc_files[name]");
    line("def _pc_str(v):");
    line("    if v is True: return 'TRUE'");
    line("    if v is False: return 'FALSE'");
    line("    if isinstance(v, float): return f'{v:.10g}'");
    line("    return str(v)");
    line("_PC_INT_MAX = 9223372036854775807");
    line("_PC_INT_MIN = -9223372036854775808");
    line("def _pc_bounds_check(val, low, high, name):");
    line("    if val < low or val > high:");
    line("        print(f\"Runtime Error: Array index out of bounds on '{name}': index {val} not in [{low}:{high}]\", file=sys.stderr)");
    line("        sys.exit(1)");
    line("    return val");
    line("def _pc_add(a, b):");
    line("    res = a + b");
    line("    if res > _PC_INT_MAX or res < _PC_INT_MIN:");
    line("        print(\"Runtime Error: 64-bit integer addition overflow\", file=sys.stderr)");
    line("        sys.exit(1)");
    line("    return res");
    line("def _pc_sub(a, b):");
    line("    res = a - b");
    line("    if res > _PC_INT_MAX or res < _PC_INT_MIN:");
    line("        print(\"Runtime Error: 64-bit integer subtraction overflow\", file=sys.stderr)");
    line("        sys.exit(1)");
    line("    return res");
    line("def _pc_mul(a, b):");
    line("    res = a * b");
    line("    if res > _PC_INT_MAX or res < _PC_INT_MIN:");
    line("        print(\"Runtime Error: 64-bit integer multiplication overflow\", file=sys.stderr)");
    line("        sys.exit(1)");
    line("    return res");
    line("def _pc_div(a, b):");
    line("    if b == 0:");
    line("        print(\"Runtime Error: Integer division (DIV) by zero\", file=sys.stderr)");
    line("        sys.exit(1)");
    line("    return a // b");
    line("def _pc_mod(a, b):");
    line("    if b == 0:");
    line("        print(\"Runtime Error: Modulo (MOD) by zero\", file=sys.stderr)");
    line("        sys.exit(1)");
    line("    return a % b");
    line("");
}

void PyCodeGen::emitRecordClasses() {
    for (const auto& kv : recordTypes_) {
        const auto& rdef = kv.second;
        line("class " + rdef.name + ":");
        ++indent_;
        line("def __init__(self):");
        ++indent_;
        for (const auto& f : rdef.fields) {
            std::string defVal = "0";
            if (f.type.isArray) {
                int64_t n1 = f.type.upper1 + 1;
                std::string elemInit = "0";
                if (f.type.base == BaseType::Record) elemInit = f.type.recordName + "()";
                else if (f.type.base == BaseType::Real) elemInit = "0.0";
                else if (f.type.base == BaseType::Boolean) elemInit = "False";
                else if (f.type.base == BaseType::String || f.type.base == BaseType::Char) elemInit = "''";
                if (f.type.dims == 1) {
                    defVal = "[" + elemInit + " for _ in range(" + std::to_string(n1) + ")]";
                } else {
                    int64_t n2 = f.type.upper2 + 1;
                    defVal = "[[" + elemInit + " for _ in range(" + std::to_string(n2) + ")] for _ in range(" + std::to_string(n1) + ")]";
                }
            } else if (f.type.base == BaseType::Real) defVal = "0.0";
            else if (f.type.base == BaseType::Boolean) defVal = "False";
            else if (f.type.base == BaseType::String || f.type.base == BaseType::Char) defVal = "''";
            else if (f.type.base == BaseType::Record) defVal = f.type.recordName + "()";
            line("self." + f.name + " = " + defVal);
        }
        if (rdef.fields.empty()) line("pass");
        --indent_;
        --indent_;
        line("");
    }
}

void PyCodeGen::emitClassDefinitions(const Block& program) {
    for (const auto& s : program) {
        if (s->kind != Stmt::Kind::ClassDecl) continue;
        auto& cls = static_cast<const ClassDeclStmt&>(*s);
        currentClassName_ = cls.name;
        insideClass_ = true;

        std::string baseClass = cls.superClass.empty() ? "" : "(" + cls.superClass + ")";
        line("class " + cls.name + baseClass + ":");
        ++indent_;

        // Find constructor
        const ClassMethod* ctor = nullptr;
        for (const auto& m : cls.methods) {
            if (m->isConstructor) { ctor = m.get(); break; }
        }

        // Emit __init__
        std::string initSig = "def __init__(self";
        if (ctor) {
            for (const auto& p : ctor->params) {
                initSig += ", " + p.name;
            }
        }
        initSig += "):";
        line(initSig);
        ++indent_;

        // Initialize properties
        for (const auto& p : cls.properties) {
            std::string defVal = "0";
            if (p.type.base == BaseType::Real) defVal = "0.0";
            else if (p.type.base == BaseType::Boolean) defVal = "False";
            else if (p.type.base == BaseType::String || p.type.base == BaseType::Char) defVal = "''";
            else if (p.type.base == BaseType::Record || p.type.base == BaseType::Object) defVal = "None";
            line("self." + p.name + " = " + defVal);
        }

        if (ctor) {
            for (const auto& p : ctor->params) {
                if (!p.isByRef && (p.type.isArray || p.type.base == BaseType::Record)) {
                    line(p.name + " = copy.deepcopy(" + p.name + ")");
                }
            }
            for (const auto& stmt : ctor->body) emitStmt(*stmt);
        } else if (!cls.superClass.empty()) {
            line("super().__init__()");
        } else if (cls.properties.empty()) {
            line("pass");
        }
        --indent_;
        line("");

        // Emit other methods
        for (const auto& m : cls.methods) {
            if (m->isConstructor) continue;
            std::string sig = "def " + m->name + "(self";
            for (const auto& p : m->params) {
                sig += ", " + p.name;
            }
            sig += "):";
            line(sig);
            ++indent_;
            for (const auto& p : m->params) {
                if (!p.isByRef && (p.type.isArray || p.type.base == BaseType::Record)) {
                    line(p.name + " = copy.deepcopy(" + p.name + ")");
                }
            }
            if (m->body.empty()) {
                line("pass");
            } else {
                for (const auto& stmt : m->body) emitStmt(*stmt);
            }
            --indent_;
            line("");
        }

        --indent_;
        insideClass_ = false;
        currentClassName_.clear();
    }
}

void PyCodeGen::emitFunctions(const Block& program) {
    for (const auto& s : program) {
        if (s->kind == Stmt::Kind::ProcedureDecl) {
            auto& p = static_cast<const ProcedureDeclStmt&>(*s);

            std::string sig = "def " + p.name + "(";
            for (size_t i = 0; i < p.params.size(); ++i) {
                if (i > 0) sig += ", ";
                sig += p.params[i].name;
            }
            sig += "):";
            line(sig);
            ++indent_;
            for (const auto& param : p.params) {
                if (!param.isByRef && (param.type.isArray || param.type.base == BaseType::Record)) {
                    line(param.name + " = copy.deepcopy(" + param.name + ")");
                }
            }

            auto it = byrefProcs_.find(p.name);
            const std::vector<size_t>* byrefIndices = (it != byrefProcs_.end()) ? &it->second : nullptr;
            currentProcByrefIndices_ = byrefIndices ? *byrefIndices : std::vector<size_t>{};
            currentProcParamNames_.clear();
            for (const auto& param : p.params) currentProcParamNames_.push_back(param.name);

            if (p.body.empty()) {
                if (byrefIndices && !byrefIndices->empty()) {
                    std::string ret = "return (";
                    for (size_t idx : *byrefIndices) {
                        ret += p.params[idx].name + ", ";
                    }
                    ret += ")";
                    line(ret);
                } else {
                    line("pass");
                }
            } else {
                for (const auto& stmt : p.body) emitStmt(*stmt);
                // At end of procedure, return BYREF scalar values
                if (byrefIndices && !byrefIndices->empty()) {
                    std::string ret = "return (";
                    for (size_t idx : *byrefIndices) {
                        ret += p.params[idx].name + ", ";
                    }
                    ret += ")";
                    line(ret);
                }
            }
            currentProcByrefIndices_.clear();
            currentProcParamNames_.clear();
            --indent_;
            line("");
        } else if (s->kind == Stmt::Kind::FunctionDecl) {
            auto& f = static_cast<const FunctionDeclStmt&>(*s);
            std::string sig = "def " + f.name + "(";
            for (size_t i = 0; i < f.params.size(); ++i) {
                if (i > 0) sig += ", ";
                sig += f.params[i].name;
            }
            sig += "):";
            line(sig);
            ++indent_;
            for (const auto& param : f.params) {
                if (!param.isByRef && (param.type.isArray || param.type.base == BaseType::Record)) {
                    line(param.name + " = copy.deepcopy(" + param.name + ")");
                }
            }
            if (f.body.empty()) {
                line("pass");
            } else {
                for (const auto& stmt : f.body) emitStmt(*stmt);
            }
            --indent_;
            line("");
        }
    }
}

void PyCodeGen::emitBlock(const Block& block) {
    for (const auto& s : block) emitStmt(*s);
}

void PyCodeGen::emitStmt(const Stmt& s) {
    switch (s.kind) {
        case Stmt::Kind::TypeDecl:
        case Stmt::Kind::ClassDecl:
        case Stmt::Kind::ProcedureDecl:
        case Stmt::Kind::FunctionDecl:
            break;

        case Stmt::Kind::Declare: {
            auto& d = static_cast<const DeclareStmt&>(s);
            varMap_[d.name] = d.declaredType;
            if (d.declaredType.isArray) {
                int64_t n1 = d.declaredType.upper1 + 1;
                std::string elemInit = "0";
                if (d.declaredType.base == BaseType::Record) elemInit = d.declaredType.recordName + "()";
                else if (d.declaredType.base == BaseType::Real) elemInit = "0.0";
                else if (d.declaredType.base == BaseType::Boolean) elemInit = "False";
                else if (d.declaredType.base == BaseType::String || d.declaredType.base == BaseType::Char) elemInit = "''";
                if (d.declaredType.dims == 1) {
                    line(d.name + " = [" + elemInit + " for _ in range(" + std::to_string(n1) + ")]");
                } else {
                    int64_t n2 = d.declaredType.upper2 + 1;
                    line(d.name + " = [[" + elemInit + " for _ in range(" + std::to_string(n2) + ")] for _ in range(" + std::to_string(n1) + ")]");
                }
            } else if (d.declaredType.base == BaseType::Record) {
                line(d.name + " = " + d.declaredType.recordName + "()");
            } else {
                std::string defVal = "0";
                if (d.declaredType.base == BaseType::Real) defVal = "0.0";
                else if (d.declaredType.base == BaseType::Boolean) defVal = "False";
                else if (d.declaredType.base == BaseType::String || d.declaredType.base == BaseType::Char) defVal = "''";
                else if (d.declaredType.base == BaseType::Object) defVal = "None";
                line(d.name + " = " + defVal);
            }
            break;
        }

        case Stmt::Kind::Constant: {
            auto& c = static_cast<const ConstantStmt&>(s);
            line(c.name + " = " + expr(*c.value));
            break;
        }

        case Stmt::Kind::Assign: {
            auto& a = static_cast<const AssignStmt&>(s);
            std::string lhs = a.name;
            if (insideClass_ && isClassProperty(a.name)) {
                lhs = "self." + a.name;
            }
            if (a.value->type.isArray) {
                line(lhs + " = copy.deepcopy(" + expr(*a.value) + ")");
            } else if (a.value->type.base == BaseType::Record) {
                line(lhs + ".__dict__.update(copy.deepcopy(" + expr(*a.value) + ").__dict__)");
            } else {
                line(lhs + " = " + expr(*a.value));
            }
            break;
        }

        case Stmt::Kind::ArrayAssign: {
            auto& a = static_cast<const ArrayAssignStmt&>(s);
            std::string arrName = a.name;
            if (arrName.empty() && a.target && a.target->kind == Expr::Kind::Var) {
                arrName = static_cast<const VarExpr&>(*a.target).name;
            }
            std::string target = a.target ? expr(*a.target) : a.name;
            auto it = varMap_.find(arrName);
            if (it != varMap_.end() && it->second.isArray) {
                const auto& info = it->second;
                for (size_t i = 0; i < a.indices.size(); ++i) {
                    int64_t low = (i == 0) ? info.lower1 : info.lower2;
                    int64_t high = (i == 0) ? info.upper1 : info.upper2;
                    if (low != 0 || high != 0) {
                        target += "[_pc_bounds_check(" + expr(*a.indices[i]) + ", " +
                                  std::to_string(low) + ", " + std::to_string(high) + ", \"" + arrName + "\")]";
                    } else {
                        target += "[" + expr(*a.indices[i]) + "]";
                    }
                }
            } else {
                for (size_t i = 0; i < a.indices.size(); ++i) {
                    if (a.target && a.target->type.isArray) {
                        int64_t low = (i == 0) ? a.target->type.lower1 : a.target->type.lower2;
                        int64_t high = (i == 0) ? a.target->type.upper1 : a.target->type.upper2;
                        if (low != 0 || high != 0) {
                            target += "[_pc_bounds_check(" + expr(*a.indices[i]) + ", " +
                                      std::to_string(low) + ", " + std::to_string(high) + ", \"array\")]";
                            continue;
                        }
                    }
                    target += "[" + expr(*a.indices[i]) + "]";
                }
            }
            line(target + " = " + expr(*a.value));
            break;
        }

        case Stmt::Kind::MemberAssign: {
            auto& m = static_cast<const MemberAssignStmt&>(s);
            line(expr(*m.target) + "." + m.field + " = " + expr(*m.value));
            break;
        }

        case Stmt::Kind::Call: {
            auto& c = static_cast<const CallStmt&>(s);
            if (c.isSuper) {
                if (c.name == "NEW") {
                    std::string s = "super().__init__(";
                    for (size_t i = 0; i < c.args.size(); ++i) {
                        if (i > 0) s += ", ";
                        s += expr(*c.args[i]);
                    }
                    s += ")";
                    line(s);
                } else {
                    std::string s = "super()." + c.name + "(";
                    for (size_t i = 0; i < c.args.size(); ++i) {
                        if (i > 0) s += ", ";
                        s += expr(*c.args[i]);
                    }
                    s += ")";
                    line(s);
                }
            } else if (c.target) {
                std::string s = expr(*c.target) + "." + c.name + "(";
                for (size_t i = 0; i < c.args.size(); ++i) {
                    if (i > 0) s += ", ";
                    s += expr(*c.args[i]);
                }
                s += ")";
                line(s);
            } else {
                std::string s;
                if (insideClass_ && classTypes_.count(currentClassName_)) {
                    // Check if method exists on self
                    std::string cur = currentClassName_;
                    bool onSelf = false;
                    while (!cur.empty()) {
                        auto it = classTypes_.find(cur);
                        if (it != classTypes_.end() && it->second.methods.count(c.name)) {
                            onSelf = true; break;
                        }
                        cur = (it != classTypes_.end()) ? it->second.superClass : "";
                    }
                    if (onSelf) s = "self." + c.name + "(";
                    else s = c.name + "(";
                } else {
                    s = c.name + "(";
                }
                for (size_t i = 0; i < c.args.size(); ++i) {
                    if (i > 0) s += ", ";
                    s += expr(*c.args[i]);
                }
                s += ")";

                auto it = byrefProcs_.find(c.name);
                std::string lhsUnpack;
                if (it != byrefProcs_.end() && !it->second.empty()) {
                    for (size_t idx : it->second) {
                        if (idx < c.args.size()) {
                            if (!lhsUnpack.empty()) lhsUnpack += ", ";
                            lhsUnpack += expr(*c.args[idx]);
                        }
                    }
                    if (it->second.size() == 1) lhsUnpack += ",";
                    lhsUnpack += " = ";
                }
                line(lhsUnpack + s);
            }
            break;
        }

        case Stmt::Kind::Return: {
            auto& r = static_cast<const ReturnStmt&>(s);
            if (r.value) {
                line("return " + expr(*r.value));
            } else if (!currentProcByrefIndices_.empty()) {
                std::string ret = "return (";
                for (size_t idx : currentProcByrefIndices_) {
                    ret += currentProcParamNames_[idx] + ", ";
                }
                ret += ")";
                line(ret);
            } else {
                line("return");
            }
            break;
        }

        case Stmt::Kind::Output: {
            auto& o = static_cast<const OutputStmt&>(s);
            std::string s = "print(";
            for (size_t i = 0; i < o.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += "_pc_str(" + expr(*o.args[i]) + ")";
            }
            s += ", sep=\"\")";
            line(s);
            break;
        }

        case Stmt::Kind::Input: {
            auto& in = static_cast<const InputStmt&>(s);
            std::string target = in.name;
            if (in.target) target = expr(*in.target);
            else if (!in.indices.empty()) {
                for (auto& idx : in.indices) target += "[" + expr(*idx) + "]";
            }
            TypeInfo ti = in.target ? in.target->type : (varMap_.count(in.name) ? varMap_[in.name] : TypeInfo{BaseType::String, "", false, 0, 0, 0, 0, 0});
            if (ti.base == BaseType::Integer) {
                line(target + " = int(input())");
            } else if (ti.base == BaseType::Real) {
                line(target + " = float(input())");
            } else if (ti.base == BaseType::Boolean) {
                line(target + " = input().strip().upper() in ('TRUE', '1')");
            } else {
                line(target + " = input()");
            }
            break;
        }

        case Stmt::Kind::If: {
            auto& i = static_cast<const IfStmt&>(s);
            line("if " + expr(*i.cond) + ":");
            ++indent_;
            if (i.thenBlock.empty()) line("pass");
            else emitBlock(i.thenBlock);
            --indent_;
            if (!i.elseBlock.empty()) {
                line("else:");
                ++indent_;
                emitBlock(i.elseBlock);
                --indent_;
            }
            break;
        }

        case Stmt::Kind::While: {
            auto& w = static_cast<const WhileStmt&>(s);
            line("while " + expr(*w.cond) + ":");
            ++indent_;
            if (w.body.empty()) line("pass");
            else emitBlock(w.body);
            --indent_;
            break;
        }

        case Stmt::Kind::Repeat: {
            auto& r = static_cast<const RepeatStmt&>(s);
            line("while True:");
            ++indent_;
            emitBlock(r.body);
            line("if " + expr(*r.cond) + ":");
            ++indent_;
            line("break");
            --indent_;
            --indent_;
            break;
        }

        case Stmt::Kind::For: {
            auto& f = static_cast<const ForStmt&>(s);
            std::string startS = expr(*f.start);
            std::string endS = expr(*f.end);
            if (f.step) {
                std::string stepS = expr(*f.step);
                line("for " + f.var + " in range(" + startS + ", (" + endS + ") + (1 if (" + stepS + ") > 0 else -1), " + stepS + "):");
            } else {
                line("for " + f.var + " in range(" + startS + ", (" + endS + ") + 1):");
            }
            ++indent_;
            if (f.body.empty()) line("pass");
            else emitBlock(f.body);
            --indent_;
            break;
        }

        case Stmt::Kind::Case: {
            auto& c = static_cast<const CaseStmt&>(s);
            std::string sel = expr(*c.selector);
            bool first = true;
            for (const auto& b : c.branches) {
                std::string cond;
                if (b.values.size() == 1) {
                    cond = sel + " == " + expr(*b.values[0]);
                } else {
                    cond = sel + " in (";
                    for (size_t vi = 0; vi < b.values.size(); ++vi) {
                        if (vi > 0) cond += ", ";
                        cond += expr(*b.values[vi]);
                    }
                    cond += ")";
                }
                line(std::string(first ? "if " : "elif ") + cond + ":");
                first = false;
                ++indent_;
                if (b.body.empty()) line("pass");
                else emitBlock(b.body);
                --indent_;
            }
            if (!c.otherwiseBlock.empty()) {
                line("else:");
                ++indent_;
                emitBlock(c.otherwiseBlock);
                --indent_;
            }
            break;
        }

        case Stmt::Kind::OpenFile: {
            auto& o = static_cast<const OpenFileStmt&>(s);
            line("_pc_open(" + expr(*o.filename) + ", \"" + o.mode + "\")");
            break;
        }

        case Stmt::Kind::CloseFile: {
            auto& cf = static_cast<const CloseFileStmt&>(s);
            line("_pc_close(" + expr(*cf.filename) + ")");
            break;
        }

        case Stmt::Kind::ReadFile: {
            auto& rf = static_cast<const ReadFileStmt&>(s);
            std::string target = (rf.target->kind == Expr::Kind::Var) ? static_cast<const VarExpr&>(*rf.target).name : expr(*rf.target);
            line(target + " = _pc_read(" + expr(*rf.filename) + ")");
            break;
        }

        case Stmt::Kind::WriteFile: {
            auto& wf = static_cast<const WriteFileStmt&>(s);
            line("_pc_write(" + expr(*wf.filename) + ", " + expr(*wf.value) + ")");
            break;
        }

        case Stmt::Kind::Seek: {
            auto& sk = static_cast<const SeekStmt&>(s);
            line("_pc_seek(" + expr(*sk.filename) + ", " + expr(*sk.address) + ")");
            break;
        }

        case Stmt::Kind::PutRecord: {
            auto& pr = static_cast<const PutRecordStmt&>(s);
            if (pr.value->type.base == BaseType::Record) {
                const auto& rdef = recordTypes_.at(pr.value->type.recordName);
                std::string valStr = expr(*pr.value);
                std::string parts = "'|'.join([";
                for (size_t i = 0; i < rdef.fields.size(); ++i) {
                    if (i > 0) parts += ", ";
                    parts += "_pc_str(" + valStr + "." + rdef.fields[i].name + ")";
                }
                parts += "])";
                line("_pc_put_record(" + expr(*pr.filename) + ", " + parts + ")");
            } else {
                line("_pc_put_record(" + expr(*pr.filename) + ", _pc_str(" + expr(*pr.value) + "))");
            }
            break;
        }

        case Stmt::Kind::GetRecord: {
            auto& gr = static_cast<const GetRecordStmt&>(s);
            std::string target = (gr.target->kind == Expr::Kind::Var) ? static_cast<const VarExpr&>(*gr.target).name : expr(*gr.target);
            if (insideClass_ && gr.target->kind == Expr::Kind::Var && isClassProperty(target)) {
                target = "self." + target;
            }
            if (gr.target->type.base == BaseType::Record) {
                const auto& rdef = recordTypes_.at(gr.target->type.recordName);
                line("_pc_rec_line = _pc_get_record(" + expr(*gr.filename) + ")");
                line("_pc_rec_parts = _pc_rec_line.split('|')");
                for (size_t i = 0; i < rdef.fields.size(); ++i) {
                    const auto& fld = rdef.fields[i];
                    std::string fldTarget = target + "." + fld.name;
                    std::string partIdx = "(_pc_rec_parts[" + std::to_string(i) + "] if " + std::to_string(i) + " < len(_pc_rec_parts) else '')";
                    if (fld.type.base == BaseType::Integer) {
                        line(fldTarget + " = int(" + partIdx + ") if " + partIdx + " else 0");
                    } else if (fld.type.base == BaseType::Real) {
                        line(fldTarget + " = float(" + partIdx + ") if " + partIdx + " else 0.0");
                    } else if (fld.type.base == BaseType::Boolean) {
                        line(fldTarget + " = (" + partIdx + ".upper() in ('TRUE', '1')) if " + partIdx + " else False");
                    } else {
                        line(fldTarget + " = " + partIdx);
                    }
                }
            } else {
                line("_pc_rec_val = _pc_get_record(" + expr(*gr.filename) + ")");
                if (gr.target->type.base == BaseType::Integer) {
                    line(target + " = int(_pc_rec_val) if _pc_rec_val else 0");
                } else if (gr.target->type.base == BaseType::Real) {
                    line(target + " = float(_pc_rec_val) if _pc_rec_val else 0.0");
                } else if (gr.target->type.base == BaseType::Boolean) {
                    line(target + " = (_pc_rec_val.upper() in ('TRUE', '1')) if _pc_rec_val else False");
                } else {
                    line(target + " = _pc_rec_val");
                }
            }
            break;
        }
    }
}

std::string PyCodeGen::expr(const Expr& e) {
    switch (e.kind) {
        case Expr::Kind::Literal: {
            auto& l = static_cast<const LiteralExpr&>(e);
            switch (l.litType) {
                case Tok::IntLit:  return l.text;
                case Tok::RealLit: return l.text;
                case Tok::CharLit:
                case Tok::StrLit:  return escapePyStr(l.text);
                case Tok::True:    return "True";
                case Tok::False:   return "False";
                default:           return "None";
            }
        }
        case Expr::Kind::Var: {
            std::string name = static_cast<const VarExpr&>(e).name;
            if (insideClass_) {
                if (name == "THIS" || name == "this") return "self";
                if (isClassProperty(name)) return "self." + name;
            }
            return name;
        }
        case Expr::Kind::ArrayAccess: {
            auto& a = static_cast<const ArrayAccessExpr&>(e);
            std::string arrName = a.name;
            if (arrName.empty() && a.target && a.target->kind == Expr::Kind::Var) {
                arrName = static_cast<const VarExpr&>(*a.target).name;
            }
            std::string s = a.target ? expr(*a.target) : a.name;
            auto it = varMap_.find(arrName);
            if (it != varMap_.end() && it->second.isArray) {
                const auto& info = it->second;
                for (size_t i = 0; i < a.indices.size(); ++i) {
                    int64_t low = (i == 0) ? info.lower1 : info.lower2;
                    int64_t high = (i == 0) ? info.upper1 : info.upper2;
                    if (low != 0 || high != 0) {
                        s += "[_pc_bounds_check(" + expr(*a.indices[i]) + ", " +
                             std::to_string(low) + ", " + std::to_string(high) + ", \"" + arrName + "\")]";
                    } else {
                        s += "[" + expr(*a.indices[i]) + "]";
                    }
                }
            } else {
                for (size_t i = 0; i < a.indices.size(); ++i) {
                    if (a.target && a.target->type.isArray) {
                        int64_t low = (i == 0) ? a.target->type.lower1 : a.target->type.lower2;
                        int64_t high = (i == 0) ? a.target->type.upper1 : a.target->type.upper2;
                        if (low != 0 || high != 0) {
                            s += "[_pc_bounds_check(" + expr(*a.indices[i]) + ", " +
                                 std::to_string(low) + ", " + std::to_string(high) + ", \"array\")]";
                            continue;
                        }
                    }
                    s += "[" + expr(*a.indices[i]) + "]";
                }
            }
            return s;
        }
        case Expr::Kind::MemberAccess: {
            auto& m = static_cast<const MemberAccessExpr&>(e);
            return expr(*m.target) + "." + m.field;
        }
        case Expr::Kind::Unary: {
            auto& u = static_cast<const UnaryExpr&>(e);
            if (u.op == Tok::Not) return "(not (" + expr(*u.operand) + "))";
            return "(-(" + expr(*u.operand) + "))";
        }
        case Expr::Kind::Binary:
            return binary(static_cast<const BinaryExpr&>(e));
        case Expr::Kind::Call:
            return call(static_cast<const CallExpr&>(e));
        case Expr::Kind::UserCall: {
            auto& uc = static_cast<const UserCallExpr&>(e);
            std::string s;
            if (insideClass_ && classTypes_.count(currentClassName_)) {
                std::string cur = currentClassName_;
                bool onSelf = false;
                while (!cur.empty()) {
                    auto it = classTypes_.find(cur);
                    if (it != classTypes_.end() && it->second.methods.count(uc.callee)) {
                        onSelf = true; break;
                    }
                    cur = (it != classTypes_.end()) ? it->second.superClass : "";
                }
                s = onSelf ? "self." + uc.callee + "(" : uc.callee + "(";
            } else {
                s = uc.callee + "(";
            }
            for (size_t i = 0; i < uc.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += expr(*uc.args[i]);
            }
            s += ")";
            return s;
        }
        case Expr::Kind::New: {
            auto& n = static_cast<const NewExpr&>(e);
            std::string s = n.className + "(";
            for (size_t i = 0; i < n.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += expr(*n.args[i]);
            }
            s += ")";
            return s;
        }
        case Expr::Kind::MethodCall: {
            auto& m = static_cast<const MethodCallExpr&>(e);
            std::string s;
            if (m.isSuper) {
                s = "super()." + m.method + "(";
            } else {
                s = expr(*m.target) + "." + m.method + "(";
            }
            for (size_t i = 0; i < m.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += expr(*m.args[i]);
            }
            s += ")";
            return s;
        }
    }
    return "None";
}

std::string PyCodeGen::call(const CallExpr& c) {
    switch (c.func) {
        case Tok::Length:
            return "len(" + expr(*c.args[0]) + ")";
        case Tok::Substring:
        case Tok::Mid:
            return "(" + expr(*c.args[0]) + ")[" + expr(*c.args[1]) + " - 1 : " + expr(*c.args[1]) + " - 1 + " + expr(*c.args[2]) + "]";
        case Tok::Left:
            return "(" + expr(*c.args[0]) + ")[:" + expr(*c.args[1]) + "]";
        case Tok::Right:
            return "((" + expr(*c.args[0]) + ")[-" + expr(*c.args[1]) + ":] if (" + expr(*c.args[1]) + ") > 0 else '')";
        case Tok::UCase:
            return "(" + expr(*c.args[0]) + ").upper()";
        case Tok::LCase:
            return "(" + expr(*c.args[0]) + ").lower()";
        case Tok::NumToStr:
            return "str(" + expr(*c.args[0]) + ")";
        case Tok::StrToNum:
            return "(float(" + expr(*c.args[0]) + ") if '.' in str(" + expr(*c.args[0]) + ") else int(" + expr(*c.args[0]) + "))";
        case Tok::Chr:
            return "chr(" + expr(*c.args[0]) + ")";
        case Tok::Asc:
            return "ord((" + expr(*c.args[0]) + ")[0])";
        case Tok::IntFunc:
            return "math.floor(" + expr(*c.args[0]) + ")";
        case Tok::Round:
            return "(int(round(" + expr(*c.args[0]) + ")) if (" + expr(*c.args[1]) + ") == 0 else round(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + "))";
        case Tok::Rnd:
            return "random.random()";
        case Tok::Mod:
            return "_pc_mod(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ")";
        case Tok::Div:
            return "_pc_div(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ")";
        case Tok::EofFunc:
            return "_pc_eof(" + expr(*c.args[0]) + ")";
        case Tok::ReadFile:
            return "_pc_read(" + expr(*c.args[0]) + ")";
        case Tok::GetRecord:
            return "_pc_get_record(" + expr(*c.args[0]) + ")";
        default:
            return "None";
    }
}

std::string PyCodeGen::binary(const BinaryExpr& b) {
    std::string l = expr(*b.lhs);
    std::string r = expr(*b.rhs);

    if (b.op == Tok::Ampersand) {
        return "(str(" + l + ") + str(" + r + "))";
    }
    if (b.op == Tok::Div) {
        return "_pc_div(" + l + ", " + r + ")";
    }
    if (b.op == Tok::Mod) {
        return "_pc_mod(" + l + ", " + r + ")";
    }
    if (b.op == Tok::Slash) {
        return "(" + l + " / " + r + ")";
    }
    if (b.op == Tok::And) {
        return "(" + l + " and " + r + ")";
    }
    if (b.op == Tok::Or) {
        return "(" + l + " or " + r + ")";
    }

    if (b.lhs->type.base == BaseType::Integer && b.rhs->type.base == BaseType::Integer) {
        if (b.op == Tok::Plus)  return "_pc_add(" + l + ", " + r + ")";
        if (b.op == Tok::Minus) return "_pc_sub(" + l + ", " + r + ")";
        if (b.op == Tok::Star)  return "_pc_mul(" + l + ", " + r + ")";
    }

    const char* op = "+";
    switch (b.op) {
        case Tok::Plus:  op = "+"; break;
        case Tok::Minus: op = "-"; break;
        case Tok::Star:  op = "*"; break;
        case Tok::Eq:    op = "=="; break;
        case Tok::Neq:   op = "!="; break;
        case Tok::Lt:    op = "<"; break;
        case Tok::Le:    op = "<="; break;
        case Tok::Gt:    op = ">"; break;
        case Tok::Ge:    op = ">="; break;
        default: break;
    }
    return "(" + l + " " + op + " " + r + ")";
}

std::string PyCodeGen::generate(const Block& program) {
    emitImports();
    emitRecordClasses();
    emitClassDefinitions(program);
    emitFunctions(program);

    line("# --- Main Program ---");
    for (const auto& s : program) {
        if (s->kind != Stmt::Kind::TypeDecl &&
            s->kind != Stmt::Kind::ClassDecl &&
            s->kind != Stmt::Kind::ProcedureDecl &&
            s->kind != Stmt::Kind::FunctionDecl) {
            emitStmt(*s);
        }
    }

    return out_.str();
}
