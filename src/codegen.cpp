#include "codegen.h"

CodeGen::CodeGen(const std::vector<std::pair<std::string, TypeInfo>>& vars)
    : vars_(vars) {
    for (const auto& v : vars_) varMap_[v.first] = v.second;
}

std::string CodeGen::generate(const Block& program) {
    emitRuntimeHeaders();

    // Large arrays are emitted as file-scope static variables to avoid stack overflow
    bool hasArrays = false;
    for (const auto& v : vars_) {
        if (v.second.isArray) {
            hasArrays = true;
            long long n1 = (v.second.upper1 - v.second.lower1 + 1);
            if (v.second.dims == 1) {
                out_ << "static " << cBaseType(v.second.base) << " " << cName(v.first)
                     << "[" << n1 << "];\n";
            } else {
                long long n2 = (v.second.upper2 - v.second.lower2 + 1);
                out_ << "static " << cBaseType(v.second.base) << " " << cName(v.first)
                     << "[" << n1 << "][" << n2 << "];\n";
            }
        }
    }
    if (hasArrays) out_ << "\n";

    out_ << "int main(void) {\n";
    indent_ = 1;

    // Scalar declarations inside main()
    for (const auto& v : vars_) {
        if (!v.second.isArray) {
            line(cBaseType(v.second.base) + " " + cName(v.first) + " = " + zeroValue(v.second.base) + ";");
        }
    }
    if (!vars_.empty()) out_ << "\n";

    emitBlock(program);
    line("pc_cleanup();");
    line("return 0;");
    out_ << "}\n";
    return out_.str();
}

void CodeGen::line(const std::string& text) {
    out_ << std::string(indent_ * 4, ' ') << text << "\n";
}

std::string CodeGen::cBaseType(BaseType t) {
    switch (t) {
        case BaseType::Integer: return "long long";
        case BaseType::Real:    return "double";
        case BaseType::Boolean: return "bool";
        default:                return "const char*";
    }
}

std::string CodeGen::zeroValue(BaseType t) {
    switch (t) {
        case BaseType::Integer: return "0";
        case BaseType::Real:    return "0.0";
        case BaseType::Boolean: return "false";
        default:                return "\"\"";
    }
}

std::string CodeGen::cName(const std::string& name) {
    return "pc_" + name;
}

std::string CodeGen::escapeCStr(const std::string& s) {
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

void CodeGen::emitRuntimeHeaders() {
    out_ << "#include <stdio.h>\n"
         << "#include <stdlib.h>\n"
         << "#include <stdbool.h>\n"
         << "#include <string.h>\n"
         << "#include <ctype.h>\n"
         << "#include <strings.h>\n\n"
         << "// --- Pseudoc Runtime Core & Dynamic Memory Tracker ---\n"
         << "typedef struct PC_Node { void* ptr; struct PC_Node* next; } PC_Node;\n"
         << "static PC_Node* pc_gc_head = NULL;\n"
         << "static void* pc_track(void* p) {\n"
         << "    if (!p) return NULL;\n"
         << "    PC_Node* n = (PC_Node*)malloc(sizeof(PC_Node));\n"
         << "    n->ptr = p; n->next = pc_gc_head; pc_gc_head = n;\n"
         << "    return p;\n"
         << "}\n"
         << "static void pc_cleanup(void) {\n"
         << "    while (pc_gc_head) {\n"
         << "        PC_Node* next = pc_gc_head->next;\n"
         << "        free(pc_gc_head->ptr);\n"
         << "        free(pc_gc_head);\n"
         << "        pc_gc_head = next;\n"
         << "    }\n"
         << "}\n"
         << "// --- Line-based Unified Input Runtime ---\n"
         << "static char pc_input_buf[4096];\n"
         << "static char* pc_read_line(void) {\n"
         << "    if (!fgets(pc_input_buf, sizeof(pc_input_buf), stdin)) {\n"
         << "        pc_input_buf[0] = '\\0';\n"
         << "    } else {\n"
         << "        size_t len = strlen(pc_input_buf);\n"
         << "        while (len > 0 && (pc_input_buf[len - 1] == '\\n' || pc_input_buf[len - 1] == '\\r')) {\n"
         << "            pc_input_buf[--len] = '\\0';\n"
         << "        }\n"
         << "    }\n"
         << "    return pc_input_buf;\n"
         << "}\n"
         << "static long long pc_read_int(void) {\n"
         << "    char* line = pc_read_line();\n"
         << "    char* end;\n"
         << "    long long val = strtoll(line, &end, 10);\n"
         << "    return (end == line) ? 0LL : val;\n"
         << "}\n"
         << "static double pc_read_real(void) {\n"
         << "    char* line = pc_read_line();\n"
         << "    char* end;\n"
         << "    double val = strtod(line, &end);\n"
         << "    return (end == line) ? 0.0 : val;\n"
         << "}\n"
         << "static bool pc_read_bool(void) {\n"
         << "    char* line = pc_read_line();\n"
         << "    while (*line && isspace((unsigned char)*line)) line++;\n"
         << "    if (strcasecmp(line, \"TRUE\") == 0 || strcmp(line, \"1\") == 0) return true;\n"
         << "    return false;\n"
         << "}\n"
         << "// --- Overflow-checked Integer Arithmetic ---\n"
         << "static inline long long pc_add(long long a, long long b) {\n"
         << "    long long res;\n"
         << "    if (__builtin_add_overflow(a, b, &res)) {\n"
         << "        fprintf(stderr, \"Runtime Error: 64-bit integer addition overflow\\n\");\n"
         << "        pc_cleanup(); exit(1);\n"
         << "    }\n"
         << "    return res;\n"
         << "}\n"
         << "static inline long long pc_sub(long long a, long long b) {\n"
         << "    long long res;\n"
         << "    if (__builtin_sub_overflow(a, b, &res)) {\n"
         << "        fprintf(stderr, \"Runtime Error: 64-bit integer subtraction overflow\\n\");\n"
         << "        pc_cleanup(); exit(1);\n"
         << "    }\n"
         << "    return res;\n"
         << "}\n"
         << "static inline long long pc_mul(long long a, long long b) {\n"
         << "    long long res;\n"
         << "    if (__builtin_mul_overflow(a, b, &res)) {\n"
         << "        fprintf(stderr, \"Runtime Error: 64-bit integer multiplication overflow\\n\");\n"
         << "        pc_cleanup(); exit(1);\n"
         << "    }\n"
         << "    return res;\n"
         << "}\n"
         << "// --- Array Indexing, Bounds Checking & Mathematical Helpers ---\n"
         << "static inline void pc_bounds_check(long long val, long long low, long long high, const char* name) {\n"
         << "    if (val < low || val > high) {\n"
         << "        fprintf(stderr, \"Runtime Error: Array index out of bounds on '%s': index %lld not in [%lld:%lld]\\n\", name, val, low, high);\n"
         << "        pc_cleanup(); exit(1);\n"
         << "    }\n"
         << "}\n"
         << "static inline long long pc_div(long long a, long long b) {\n"
         << "    if (b == 0) { fprintf(stderr, \"Runtime Error: Division by zero\\n\"); pc_cleanup(); exit(1); }\n"
         << "    long long q = a / b, r = a % b;\n"
         << "    if ((r != 0) && ((r < 0) ^ (b < 0))) q--;\n"
         << "    return q;\n"
         << "}\n"
         << "static inline long long pc_mod(long long a, long long b) {\n"
         << "    if (b == 0) { fprintf(stderr, \"Runtime Error: Modulo by zero\\n\"); pc_cleanup(); exit(1); }\n"
         << "    long long r = a % b;\n"
         << "    if ((r != 0) && ((r < 0) ^ (b < 0))) r += b;\n"
         << "    return r;\n"
         << "}\n"
         << "static char* pc_concat(const char* s1, const char* s2) {\n"
         << "    size_t l1 = strlen(s1), l2 = strlen(s2);\n"
         << "    char* res = (char*)malloc(l1 + l2 + 1);\n"
         << "    memcpy(res, s1, l1); memcpy(res + l1, s2, l2); res[l1 + l2] = '\\0';\n"
         << "    return (char*)pc_track(res);\n"
         << "}\n"
         << "static char* pc_substring(const char* s, long long start, long long len) {\n"
         << "    long long slen = (long long)strlen(s);\n"
         << "    if (start < 1) start = 1;\n"
         << "    if (len < 0) len = 0;\n"
         << "    if (start > slen) return (char*)pc_track(strdup(\"\"));\n"
         << "    if (start - 1 + len > slen) len = slen - (start - 1);\n"
         << "    char* sub = (char*)malloc(len + 1);\n"
         << "    memcpy(sub, s + (start - 1), len);\n"
         << "    sub[len] = '\\0';\n"
         << "    return (char*)pc_track(sub);\n"
         << "}\n"
         << "static char* pc_ucase(const char* s) {\n"
         << "    size_t len = strlen(s);\n"
         << "    char* r = (char*)malloc(len + 1);\n"
         << "    for (size_t i = 0; i < len; ++i) r[i] = toupper((unsigned char)s[i]);\n"
         << "    r[len] = '\\0';\n"
         << "    return (char*)pc_track(r);\n"
         << "}\n"
         << "static char* pc_lcase(const char* s) {\n"
         << "    size_t len = strlen(s);\n"
         << "    char* r = (char*)malloc(len + 1);\n"
         << "    for (size_t i = 0; i < len; ++i) r[i] = tolower((unsigned char)s[i]);\n"
         << "    r[len] = '\\0';\n"
         << "    return (char*)pc_track(r);\n"
         << "}\n"
         << "static char* pc_num_to_str_int(long long n) {\n"
         << "    char buf[64]; snprintf(buf, sizeof(buf), \"%lld\", n);\n"
         << "    return (char*)pc_track(strdup(buf));\n"
         << "}\n"
         << "static char* pc_num_to_str_real(double d) {\n"
         << "    char buf[64]; snprintf(buf, sizeof(buf), \"%.10g\", d);\n"
         << "    return (char*)pc_track(strdup(buf));\n"
         << "}\n"
         << "static double pc_str_to_num(const char* s) { return atof(s); }\n\n";
}

void CodeGen::emitBlock(const Block& block) {
    for (const auto& s : block) emitStmt(*s);
}

void CodeGen::emitIndented(const Block& block) {
    ++indent_;
    emitBlock(block);
    --indent_;
}

std::string CodeGen::arrayOffset(const std::string& name, const std::vector<ExprPtr>& indices) {
    const TypeInfo& info = varMap_[name];
    std::string s;
    if (info.dims == 1) {
        std::string i1 = expr(*indices[0]);
        s += "[(pc_bounds_check(" + i1 + ", " + std::to_string(info.lower1) + ", " +
             std::to_string(info.upper1) + ", \"" + name + "\"), (" + i1 + " - " +
             std::to_string(info.lower1) + "))]";
    } else {
        std::string i1 = expr(*indices[0]);
        std::string i2 = expr(*indices[1]);
        s += "[(pc_bounds_check(" + i1 + ", " + std::to_string(info.lower1) + ", " +
             std::to_string(info.upper1) + ", \"" + name + "\"), (" + i1 + " - " +
             std::to_string(info.lower1) + "))]";
        s += "[(pc_bounds_check(" + i2 + ", " + std::to_string(info.lower2) + ", " +
             std::to_string(info.upper2) + ", \"" + name + "\"), (" + i2 + " - " +
             std::to_string(info.lower2) + "))]";
    }
    return s;
}

void CodeGen::emitStmt(const Stmt& s) {
    switch (s.kind) {
        case Stmt::Kind::Declare:
            break;
        case Stmt::Kind::Assign: {
            auto& a = static_cast<const AssignStmt&>(s);
            line(cName(a.name) + " = " + expr(*a.value) + ";");
            break;
        }
        case Stmt::Kind::ArrayAssign: {
            auto& a = static_cast<const ArrayAssignStmt&>(s);
            line(cName(a.name) + arrayOffset(a.name, a.indices) + " = " + expr(*a.value) + ";");
            break;
        }
        case Stmt::Kind::Output:
            emitOutput(static_cast<const OutputStmt&>(s));
            break;
        case Stmt::Kind::Input:
            emitInput(static_cast<const InputStmt&>(s));
            break;
        case Stmt::Kind::If: {
            auto& i = static_cast<const IfStmt&>(s);
            line("if (" + expr(*i.cond) + ") {");
            emitIndented(i.thenBlock);
            if (!i.elseBlock.empty()) {
                line("} else {");
                emitIndented(i.elseBlock);
            }
            line("}");
            break;
        }
        case Stmt::Kind::While: {
            auto& w = static_cast<const WhileStmt&>(s);
            line("while (" + expr(*w.cond) + ") {");
            emitIndented(w.body);
            line("}");
            break;
        }
        case Stmt::Kind::For:
            emitFor(static_cast<const ForStmt&>(s));
            break;
    }
}

void CodeGen::emitOutput(const OutputStmt& o) {
    std::string fmt, args;
    for (const auto& arg : o.args) {
        switch (arg->type.base) {
            case BaseType::Integer:
                fmt += "%lld";
                args += ", " + expr(*arg);
                break;
            case BaseType::Real:
                fmt += "%.10g";
                args += ", " + expr(*arg);
                break;
            case BaseType::Boolean:
                fmt += "%s";
                args += ", (" + expr(*arg) + " ? \"TRUE\" : \"FALSE\")";
                break;
            case BaseType::String:
                fmt += "%s";
                args += ", " + expr(*arg);
                break;
            default:
                break;
        }
    }
    line("printf(\"" + fmt + "\\n\"" + args + ");");
}

void CodeGen::emitInput(const InputStmt& in) {
    std::string target = cName(in.name);
    BaseType b = varMap_[in.name].base;
    if (!in.indices.empty()) target += arrayOffset(in.name, in.indices);

    switch (b) {
        case BaseType::Integer:
            line(target + " = pc_read_int();");
            break;
        case BaseType::Real:
            line(target + " = pc_read_real();");
            break;
        case BaseType::Boolean:
            line(target + " = pc_read_bool();");
            break;
        case BaseType::String:
            line(target + " = (char*)pc_track(strdup(pc_read_line()));");
            break;
        default:
            break;
    }
}

void CodeGen::emitFor(const ForStmt& f) {
    int id = ++tempCounter_;
    std::string endVar = "tmp_end_" + std::to_string(id);
    std::string var = cName(f.var);

    line("{");
    ++indent_;
    line("long long " + endVar + " = " + expr(*f.end) + ";");
    if (f.step) {
        std::string stepVar = "tmp_step_" + std::to_string(id);
        line("long long " + stepVar + " = " + expr(*f.step) + ";");
        line("if (" + stepVar + " == 0) { "
             "fprintf(stderr, \"Runtime Error: FOR step cannot be zero\\n\"); "
             "pc_cleanup(); exit(1); }");
        line("for (" + var + " = " + expr(*f.start) + "; " + stepVar + " > 0 ? " + var +
             " <= " + endVar + " : " + var + " >= " + endVar + "; " + var + " += " +
             stepVar + ") {");
    } else {
        line("for (" + var + " = " + expr(*f.start) + "; " + var + " <= " + endVar + "; " +
             var + " += 1) {");
    }
    emitIndented(f.body);
    line("}");
    --indent_;
    line("}");
}

std::string CodeGen::expr(const Expr& e) {
    switch (e.kind) {
        case Expr::Kind::Literal: {
            auto& l = static_cast<const LiteralExpr&>(e);
            switch (l.litType) {
                case Tok::IntLit:  return l.text + "LL";
                case Tok::RealLit: return l.text;
                case Tok::StrLit:  return escapeCStr(l.text);
                case Tok::True:    return "true";
                default:           return "false";
            }
        }
        case Expr::Kind::Var:
            return cName(static_cast<const VarExpr&>(e).name);
        case Expr::Kind::ArrayAccess: {
            auto& a = static_cast<const ArrayAccessExpr&>(e);
            return cName(a.name) + arrayOffset(a.name, a.indices);
        }
        case Expr::Kind::Unary: {
            auto& u = static_cast<const UnaryExpr&>(e);
            return std::string(u.op == Tok::Not ? "(!" : "(-") + expr(*u.operand) + ")";
        }
        case Expr::Kind::Binary:
            return binary(static_cast<const BinaryExpr&>(e));
        case Expr::Kind::Call:
            return call(static_cast<const CallExpr&>(e));
    }
    return "";
}

std::string CodeGen::call(const CallExpr& c) {
    switch (c.func) {
        case Tok::Length:
            return "((long long)strlen(" + expr(*c.args[0]) + "))";
        case Tok::Substring:
            return "pc_substring(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ", " + expr(*c.args[2]) + ")";
        case Tok::UCase:
            return "pc_ucase(" + expr(*c.args[0]) + ")";
        case Tok::LCase:
            return "pc_lcase(" + expr(*c.args[0]) + ")";
        case Tok::NumToStr:
            if (c.args[0]->type.base == BaseType::Integer)
                return "pc_num_to_str_int(" + expr(*c.args[0]) + ")";
            return "pc_num_to_str_real(" + expr(*c.args[0]) + ")";
        case Tok::StrToNum:
            return "pc_str_to_num(" + expr(*c.args[0]) + ")";
        default:
            return "";
    }
}

std::string CodeGen::binary(const BinaryExpr& b) {
    std::string l = expr(*b.lhs), r = expr(*b.rhs);
    if (b.op == Tok::Ampersand) return "pc_concat(" + l + ", " + r + ")";

    if (b.lhs->type.base == BaseType::Integer && b.rhs->type.base == BaseType::Integer) {
        if (b.op == Tok::Plus)  return "pc_add(" + l + ", " + r + ")";
        if (b.op == Tok::Minus) return "pc_sub(" + l + ", " + r + ")";
        if (b.op == Tok::Star)  return "pc_mul(" + l + ", " + r + ")";
    }

    switch (b.op) {
        case Tok::Plus:  return "(" + l + " + " + r + ")";
        case Tok::Minus: return "(" + l + " - " + r + ")";
        case Tok::Star:  return "(" + l + " * " + r + ")";
        case Tok::Slash: return "((double)" + l + " / (double)" + r + ")";
        case Tok::Div:   return "pc_div(" + l + ", " + r + ")";
        case Tok::Mod:   return "pc_mod(" + l + ", " + r + ")";
        case Tok::And:   return "(" + l + " && " + r + ")";
        case Tok::Or:    return "(" + l + " || " + r + ")";
        default: break;
    }

    const char* cop = "==";
    switch (b.op) {
        case Tok::Eq:  cop = "=="; break;
        case Tok::Neq: cop = "!="; break;
        case Tok::Lt:  cop = "<";  break;
        case Tok::Le:  cop = "<="; break;
        case Tok::Gt:  cop = ">";  break;
        case Tok::Ge:  cop = ">="; break;
        default: break;
    }
    if (b.lhs->type.base == BaseType::String) return "(strcmp(" + l + ", " + r + ") " + cop + " 0)";
    return "(" + l + " " + cop + " " + r + ")";
}
