#include "codegen.h"
#include <functional>

CodeGen::CodeGen(const std::vector<std::pair<std::string, TypeInfo>>& vars,
                 const std::unordered_map<std::string, RecordDef>& records,
                 const std::unordered_map<std::string, Sema::FunctionSig>& funcs,
                 const std::unordered_map<std::string, Sema::ClassInfo>& classes)
    : vars_(vars), recordTypes_(records), functions_(funcs), classTypes_(classes) {
    for (const auto& v : vars_) varMap_[v.first] = v.second;
}

std::string CodeGen::cTypeName(const std::string& name) {
    return "pc_type_" + name;
}

std::string CodeGen::cBaseType(const TypeInfo& t) {
    if (t.base == BaseType::Record) return cTypeName(t.recordName);
    if (t.base == BaseType::Object) return cClassTypeName(t.recordName);
    switch (t.base) {
        case BaseType::Integer: return "long long";
        case BaseType::Real:    return "double";
        case BaseType::Boolean: return "bool";
        case BaseType::Void:    return "void";
        default:                return "const char*";
    }
}

std::string CodeGen::zeroValue(const TypeInfo& t) {
    if (t.base == BaseType::Record || t.base == BaseType::Object) return "{0}";
    switch (t.base) {
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

void CodeGen::line(const std::string& text) {
    out_ << std::string(indent_ * 4, ' ') << text << "\n";
}

void CodeGen::emitRuntimeHeaders() {
    out_ << "#include <stdio.h>\n"
         << "#include <stdlib.h>\n"
         << "#include <stdbool.h>\n"
         << "#include <string.h>\n"
         << "#include <ctype.h>\n"
         << "#include <strings.h>\n"
         << "#include <math.h>\n\n"
         << "// --- Pseudoc Runtime Core & Dynamic Memory Tracker ---\n"
         << "typedef struct PC_Node { void* ptr; struct PC_Node* next; } PC_Node;\n"
         << "static PC_Node* pc_gc_head = NULL;\n"
         << "static void* pc_track(void* p) {\n"
         << "    if (!p) return NULL;\n"
         << "    PC_Node* n = (PC_Node*)malloc(sizeof(PC_Node));\n"
         << "    n->ptr = p; n->next = pc_gc_head; pc_gc_head = n;\n"
         << "    return p;\n"
         << "}\n"
         << "// --- File I/O Runtime Tracker ---\n"
         << "typedef struct PC_File { char name[256]; FILE* fp; struct PC_File* next; } PC_File;\n"
         << "static PC_File* pc_files_head = NULL;\n"
         << "static void pc_open_file(const char* name, const char* mode) {\n"
         << "    const char* m = \"r\";\n"
         << "    if (strcmp(mode, \"WRITE\") == 0) m = \"w\";\n"
         << "    else if (strcmp(mode, \"APPEND\") == 0) m = \"a\";\n"
         << "    FILE* fp = fopen(name, m);\n"
         << "    if (!fp) { fprintf(stderr, \"Runtime Error: Cannot open file '%s' for %s\\n\", name, mode); exit(1); }\n"
         << "    PC_File* f = (PC_File*)malloc(sizeof(PC_File));\n"
         << "    strncpy(f->name, name, sizeof(f->name) - 1); f->name[sizeof(f->name) - 1] = '\\0';\n"
         << "    f->fp = fp; f->next = pc_files_head; pc_files_head = f;\n"
         << "}\n"
         << "static FILE* pc_get_file(const char* name) {\n"
         << "    PC_File* cur = pc_files_head;\n"
         << "    while (cur) {\n"
         << "        if (strcmp(cur->name, name) == 0) return cur->fp;\n"
         << "        cur = cur->next;\n"
         << "    }\n"
         << "    fprintf(stderr, \"Runtime Error: File '%s' is not open\\n\", name); exit(1);\n"
         << "    return NULL;\n"
         << "}\n"
         << "static void pc_close_file(const char* name) {\n"
         << "    PC_File** cur = &pc_files_head;\n"
         << "    while (*cur) {\n"
         << "        if (strcmp((*cur)->name, name) == 0) {\n"
         << "            PC_File* to_del = *cur;\n"
         << "            fclose(to_del->fp);\n"
         << "            *cur = to_del->next;\n"
         << "            free(to_del);\n"
         << "            return;\n"
         << "        }\n"
         << "        cur = &((*cur)->next);\n"
         << "    }\n"
         << "}\n"
         << "static char* pc_read_file_line(const char* name) {\n"
         << "    FILE* fp = pc_get_file(name);\n"
         << "    static char fbuf[4096];\n"
         << "    if (!fgets(fbuf, sizeof(fbuf), fp)) fbuf[0] = '\\0';\n"
         << "    size_t len = strlen(fbuf);\n"
         << "    while (len > 0 && (fbuf[len - 1] == '\\n' || fbuf[len - 1] == '\\r')) fbuf[--len] = '\\0';\n"
         << "    return (char*)pc_track(strdup(fbuf));\n"
         << "}\n"
         << "static void pc_write_file_line(const char* name, const char* str) {\n"
         << "    FILE* fp = pc_get_file(name);\n"
         << "    fprintf(fp, \"%s\\n\", str);\n"
         << "}\n"
         << "static bool pc_eof(const char* name) {\n"
         << "    FILE* fp = pc_get_file(name);\n"
         << "    int c = fgetc(fp);\n"
         << "    if (c == EOF) return true;\n"
         << "    ungetc(c, fp);\n"
         << "    return false;\n"
         << "}\n"
         << "static void pc_cleanup(void) {\n"
         << "    while (pc_files_head) {\n"
         << "        PC_File* next = pc_files_head->next;\n"
         << "        fclose(pc_files_head->fp);\n"
         << "        free(pc_files_head);\n"
         << "        pc_files_head = next;\n"
         << "    }\n"
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
         << "static double pc_str_to_num(const char* s) { return atof(s); }\n"
         << "static char* pc_left(const char* s, long long len) {\n"
         << "    if (len <= 0) return (char*)pc_track(strdup(\"\"));\n"
         << "    long long slen = (long long)strlen(s);\n"
         << "    if (len > slen) len = slen;\n"
         << "    char* sub = (char*)malloc(len + 1);\n"
         << "    memcpy(sub, s, len);\n"
         << "    sub[len] = '\\0';\n"
         << "    return (char*)pc_track(sub);\n"
         << "}\n"
         << "static char* pc_right(const char* s, long long len) {\n"
         << "    if (len <= 0) return (char*)pc_track(strdup(\"\"));\n"
         << "    long long slen = (long long)strlen(s);\n"
         << "    if (len > slen) len = slen;\n"
         << "    char* sub = (char*)malloc(len + 1);\n"
         << "    memcpy(sub, s + (slen - len), len);\n"
         << "    sub[len] = '\\0';\n"
         << "    return (char*)pc_track(sub);\n"
         << "}\n"
         << "static char* pc_chr(long long code) {\n"
         << "    char* res = (char*)malloc(2);\n"
         << "    res[0] = (char)(code & 0xFF);\n"
         << "    res[1] = '\\0';\n"
         << "    return (char*)pc_track(res);\n"
         << "}\n"
         << "static long long pc_asc(const char* s) {\n"
         << "    if (!s || s[0] == '\\0') return 0;\n"
         << "    return (long long)(unsigned char)s[0];\n"
         << "}\n"
         << "static long long pc_int(double v) {\n"
         << "    return (long long)floor(v);\n"
         << "}\n"
         << "static double pc_round(double v, long long places) {\n"
         << "    double factor = pow(10.0, (double)places);\n"
         << "    return round(v * factor) / factor;\n"
         << "}\n"
         << "static inline double pc_rnd(void) {\n"
         << "    return (double)rand() / ((double)RAND_MAX + 1.0);\n"
         << "}\n\n";
}

void CodeGen::emitRecordDefinitions() {
    for (const auto& kv : recordTypes_) {
        const auto& rdef = kv.second;
        out_ << "typedef struct " << cTypeName(rdef.name) << " {\n";
        for (const auto& f : rdef.fields) {
            out_ << "    " << cBaseType(f.type) << " " << cName(f.name) << ";\n";
        }
        out_ << "} " << cTypeName(rdef.name) << ";\n\n";
    }
}

std::string CodeGen::cClassTypeName(const std::string& name) {
    return "pc_class_" + name;
}

void CodeGen::emitClassDefinitions() {
    if (classTypes_.empty()) return;
    // Forward declarations
    for (const auto& kv : classTypes_) {
        out_ << "typedef struct " << cClassTypeName(kv.first) << " " << cClassTypeName(kv.first) << ";\n";
    }
    out_ << "\n";
    // Emit structs. Walk class hierarchy so superclass properties appear too.
    // We use a topological approach: emit a class only after its superclass.
    std::unordered_set<std::string> emitted;
    std::function<void(const std::string&)> emitClass = [&](const std::string& name) {
        if (emitted.count(name)) return;
        auto cit = classTypes_.find(name);
        if (cit == classTypes_.end()) return;
        const auto& ci = cit->second;
        if (!ci.superClass.empty()) emitClass(ci.superClass); // ensure parent emitted first
        out_ << "struct " << cClassTypeName(name) << " {\n";
        // Inherited fields from superclass chain
        std::vector<std::string> chain;
        std::string cur = ci.superClass;
        while (!cur.empty()) {
            chain.push_back(cur);
            auto pcit = classTypes_.find(cur);
            cur = (pcit != classTypes_.end()) ? pcit->second.superClass : "";
        }
        // Emit inherited fields (from root to parent)
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            auto pcit = classTypes_.find(*it);
            if (pcit == classTypes_.end()) continue;
            for (const auto& prop : pcit->second.properties) {
                out_ << "    " << cBaseType(prop.type) << " " << cName(prop.name) << ";\n";
            }
        }
        // Own fields
        for (const auto& prop : ci.properties) {
            out_ << "    " << cBaseType(prop.type) << " " << cName(prop.name) << ";\n";
        }
        out_ << "};\n\n";
        emitted.insert(name);
    };
    for (const auto& kv : classTypes_) emitClass(kv.first);
}

void CodeGen::emitFunctionPrototypes() {
    for (const auto& kv : functions_) {
        const auto& fn = kv.second;
        std::string retType = fn.isFunction ? cBaseType(fn.returnType) : "void";
        out_ << retType << " " << cName(fn.name) << "(";
        if (fn.params.empty()) {
            out_ << "void";
        } else {
            for (size_t i = 0; i < fn.params.size(); ++i) {
                if (i > 0) out_ << ", ";
                out_ << cBaseType(fn.params[i].type);
                if (fn.params[i].isByRef) out_ << "*";
                out_ << " " << cName(fn.params[i].name);
            }
        }
        out_ << ");\n";
    }
    if (!functions_.empty()) out_ << "\n";
}

void CodeGen::emitFunctionDefinitions(const Block& program) {
    insideFunction_ = true;
    for (const auto& s : program) {
        if (s->kind == Stmt::Kind::ProcedureDecl) {
            auto& p = static_cast<const ProcedureDeclStmt&>(*s);
            currentByRefParams_.clear();
            for (const auto& param : p.params) {
                if (param.isByRef) currentByRefParams_.insert(param.name);
            }
            out_ << "void " << cName(p.name) << "(";
            if (p.params.empty()) out_ << "void";
            else {
                for (size_t i = 0; i < p.params.size(); ++i) {
                    if (i > 0) out_ << ", ";
                    out_ << cBaseType(p.params[i].type);
                    if (p.params[i].isByRef) out_ << "*";
                    out_ << " " << cName(p.params[i].name);
                }
            }
            out_ << ") {\n";
            emitIndented(p.body);
            out_ << "}\n\n";
            currentByRefParams_.clear();
        } else if (s->kind == Stmt::Kind::FunctionDecl) {
            auto& f = static_cast<const FunctionDeclStmt&>(*s);
            currentByRefParams_.clear();
            for (const auto& param : f.params) {
                if (param.isByRef) currentByRefParams_.insert(param.name);
            }
            out_ << cBaseType(f.returnType) << " " << cName(f.name) << "(";
            if (f.params.empty()) out_ << "void";
            else {
                for (size_t i = 0; i < f.params.size(); ++i) {
                    if (i > 0) out_ << ", ";
                    out_ << cBaseType(f.params[i].type);
                    if (f.params[i].isByRef) out_ << "*";
                    out_ << " " << cName(f.params[i].name);
                }
            }
            out_ << ") {\n";
            emitIndented(f.body);
            out_ << "}\n\n";
            currentByRefParams_.clear();
        }
    }
    insideFunction_ = false;
}

void CodeGen::emitClassMethods(const Block& program) {
    insideFunction_ = true;
    for (const auto& s : program) {
        if (s->kind != Stmt::Kind::ClassDecl) continue;
        auto& cls = static_cast<const ClassDeclStmt&>(*s);
        currentClassName_ = cls.name;
        std::string structType = cClassTypeName(cls.name);

        // Find constructor (NEW method)
        const ClassMethod* ctorMethod = nullptr;
        for (const auto& methodPtr : cls.methods) {
            if (methodPtr->isConstructor) { ctorMethod = methodPtr.get(); break; }
        }

        for (const auto& methodPtr : cls.methods) {
            const ClassMethod& method = *methodPtr;
            currentByRefParams_.clear();
            for (const auto& param : method.params) {
                if (param.isByRef) currentByRefParams_.insert(param.name);
            }
            // Return type
            std::string retType = method.isFunction ? cBaseType(method.returnType) : "void";
            // Function signature: rettype pc_ClassName_MethodName(StructType* pc_THIS, params...)
            std::string funcName = "pc_" + cls.name + "_" + method.name;
            out_ << retType << " " << funcName << "(" << structType << "* pc_THIS";
            for (const auto& param : method.params) {
                out_ << ", " << cBaseType(param.type);
                if (param.isByRef) out_ << "*";
                out_ << " " << cName(param.name);
            }
            out_ << ") {\n";
            emitIndented(method.body);
            out_ << "}\n\n";
            currentByRefParams_.clear();
        }

        // Emit factory function: StructType pc_new_ClassName(ctor_params...)
        {
            out_ << structType << " pc_new_" << cls.name << "(";
            if (ctorMethod && !ctorMethod->params.empty()) {
                for (size_t i = 0; i < ctorMethod->params.size(); ++i) {
                    if (i > 0) out_ << ", ";
                    out_ << cBaseType(ctorMethod->params[i].type) << " " << cName(ctorMethod->params[i].name);
                }
            } else {
                out_ << "void";
            }
            out_ << ") {\n";
            out_ << "    " << structType << " pc__obj = {0};\n";
            if (ctorMethod) {
                out_ << "    pc_" << cls.name << "_NEW(&pc__obj";
                for (const auto& param : ctorMethod->params) {
                    out_ << ", " << cName(param.name);
                }
                out_ << ");\n";
            }
            out_ << "    return pc__obj;\n";
            out_ << "}\n\n";
        }

        currentClassName_.clear();
    }
    insideFunction_ = false;
}

std::string CodeGen::generate(const Block& program) {
    emitRuntimeHeaders();
    emitRecordDefinitions();
    emitClassDefinitions();
    emitFunctionPrototypes();
    emitFunctionDefinitions(program);
    emitClassMethods(program);

    // Global arrays are emitted at file scope
    bool hasArrays = false;
    for (const auto& v : vars_) {
        if (v.second.isArray) {
            hasArrays = true;
            long long n1 = (v.second.upper1 - v.second.lower1 + 1);
            if (v.second.dims == 1) {
                out_ << "static " << cBaseType(v.second) << " " << cName(v.first)
                     << "[" << n1 << "];\n";
            } else {
                long long n2 = (v.second.upper2 - v.second.lower2 + 1);
                out_ << "static " << cBaseType(v.second) << " " << cName(v.first)
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
            line(cBaseType(v.second) + " " + cName(v.first) + " = " + zeroValue(v.second) + ";");
        }
    }
    if (!vars_.empty()) out_ << "\n";

    emitBlock(program);
    line("pc_cleanup();");
    line("return 0;");
    out_ << "}\n";
    return out_.str();
}

void CodeGen::emitBlock(const Block& block) {
    for (const auto& s : block) {
        if (s->kind != Stmt::Kind::ProcedureDecl && s->kind != Stmt::Kind::FunctionDecl &&
            s->kind != Stmt::Kind::TypeDecl && s->kind != Stmt::Kind::ClassDecl) {
            emitStmt(*s);
        }
    }
}

void CodeGen::emitIndented(const Block& block) {
    ++indent_;
    for (const auto& s : block) emitStmt(*s);
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

std::string CodeGen::lvalueExpr(const Expr& e) {
    if (e.kind == Expr::Kind::Var) {
        std::string name = static_cast<const VarExpr&>(e).name;
        if (currentByRefParams_.find(name) != currentByRefParams_.end()) {
            return "(*" + cName(name) + ")";
        }
        return cName(name);
    }
    if (e.kind == Expr::Kind::MemberAccess) {
        auto& m = static_cast<const MemberAccessExpr&>(e);
        return lvalueExpr(*m.target) + "." + cName(m.field);
    }
    if (e.kind == Expr::Kind::ArrayAccess) {
        auto& a = static_cast<const ArrayAccessExpr&>(e);
        std::string arrName = a.name;
        if (arrName.empty() && a.target && a.target->kind == Expr::Kind::Var) {
            arrName = static_cast<const VarExpr&>(*a.target).name;
        }
        if (!arrName.empty() && varMap_.find(arrName) != varMap_.end()) {
            return cName(arrName) + arrayOffset(arrName, a.indices);
        }
        std::string s = a.target ? lvalueExpr(*a.target) : cName(a.name);
        for (auto& idx : a.indices) {
            s += "[" + expr(*idx) + "]";
        }
        return s;
    }
    return expr(e);
}

void CodeGen::emitStmt(const Stmt& s) {
    switch (s.kind) {
        case Stmt::Kind::TypeDecl:
        case Stmt::Kind::ProcedureDecl:
        case Stmt::Kind::FunctionDecl:
        case Stmt::Kind::ClassDecl:
            break;

        case Stmt::Kind::Declare: {
            auto& d = static_cast<const DeclareStmt&>(s);
            // Local declarations inside procedures/functions
            if (insideFunction_) {
                if (d.declaredType.isArray) {
                    long long n1 = (d.declaredType.upper1 - d.declaredType.lower1 + 1);
                    if (d.declaredType.dims == 1) {
                        line(cBaseType(d.declaredType) + " " + cName(d.name) + "[" + std::to_string(n1) + "];");
                    } else {
                        long long n2 = (d.declaredType.upper2 - d.declaredType.lower2 + 1);
                        line(cBaseType(d.declaredType) + " " + cName(d.name) + "[" + std::to_string(n1) + "][" + std::to_string(n2) + "];");
                    }
                } else {
                    line(cBaseType(d.declaredType) + " " + cName(d.name) + " = " + zeroValue(d.declaredType) + ";");
                }
            }
            break;
        }

        case Stmt::Kind::Constant: {
            auto& c = static_cast<const ConstantStmt&>(s);
            if (insideFunction_) {
                line(cBaseType(c.value->type) + " " + cName(c.name) + " = " + expr(*c.value) + ";");
            } else {
                line(cName(c.name) + " = " + expr(*c.value) + ";");
            }
            break;
        }

        case Stmt::Kind::Assign: {
            auto& a = static_cast<const AssignStmt&>(s);
            std::string lhs = cName(a.name);
            if (!currentClassName_.empty()) {
                bool isProp = false;
                std::string cur = currentClassName_;
                while (!cur.empty()) {
                    auto cit = classTypes_.find(cur);
                    if (cit == classTypes_.end()) break;
                    for (const auto& p : cit->second.properties) {
                        if (p.name == a.name) { isProp = true; break; }
                    }
                    if (isProp) break;
                    cur = cit->second.superClass;
                }
                if (isProp && currentByRefParams_.find(a.name) == currentByRefParams_.end() &&
                    varMap_.find(a.name) == varMap_.end()) {
                    lhs = "pc_THIS->" + cName(a.name);
                }
            }
            if (currentByRefParams_.find(a.name) != currentByRefParams_.end()) {
                lhs = "(*" + lhs + ")";
            }
            line(lhs + " = " + expr(*a.value) + ";");
            break;
        }

        case Stmt::Kind::ArrayAssign: {
            auto& a = static_cast<const ArrayAssignStmt&>(s);
            if (a.target) {
                std::string s = lvalueExpr(*a.target);
                for (auto& idx : a.indices) s += "[" + expr(*idx) + "]";
                line(s + " = " + expr(*a.value) + ";");
            } else {
                line(cName(a.name) + arrayOffset(a.name, a.indices) + " = " + expr(*a.value) + ";");
            }
            break;
        }

        case Stmt::Kind::MemberAssign: {
            auto& m = static_cast<const MemberAssignStmt&>(s);
            line(lvalueExpr(*m.target) + "." + cName(m.field) + " = " + expr(*m.value) + ";");
            break;
        }

        case Stmt::Kind::Call: {
            auto& c = static_cast<const CallStmt&>(s);
            if (c.isSuper && !currentClassName_.empty()) {
                auto cit = classTypes_.find(currentClassName_);
                std::string superClass = (cit != classTypes_.end()) ? cit->second.superClass : "";
                std::string cur = superClass;
                std::string defClass;
                while (!cur.empty()) {
                    auto it = classTypes_.find(cur);
                    if (it == classTypes_.end()) break;
                    if (it->second.methods.find(c.name) != it->second.methods.end()) {
                        defClass = cur;
                        break;
                    }
                    cur = it->second.superClass;
                }
                if (defClass.empty()) defClass = superClass;
                std::string callStr = "pc_" + defClass + "_" + c.name + "((struct pc_class_" + defClass + "*)pc_THIS";
                for (auto& arg : c.args) callStr += ", " + expr(*arg);
                callStr += ");";
                line(callStr);
            } else if (c.target) {
                std::string objExpr = expr(*c.target);
                std::string cls;
                if (c.target->type.base == BaseType::Object) cls = c.target->type.recordName;
                else if (c.target->kind == Expr::Kind::Var &&
                    (static_cast<const VarExpr&>(*c.target).name == "THIS" || static_cast<const VarExpr&>(*c.target).name == "this")) {
                    cls = currentClassName_;
                }
                std::string cur = cls;
                std::string defClass;
                while (!cur.empty()) {
                    auto it = classTypes_.find(cur);
                    if (it == classTypes_.end()) break;
                    if (it->second.methods.find(c.name) != it->second.methods.end()) {
                        defClass = cur;
                        break;
                    }
                    cur = it->second.superClass;
                }
                if (defClass.empty()) defClass = cls;
                std::string targetPtr;
                if (c.target->kind == Expr::Kind::Var &&
                    (static_cast<const VarExpr&>(*c.target).name == "THIS" || static_cast<const VarExpr&>(*c.target).name == "this")) {
                    targetPtr = "pc_THIS";
                } else {
                    targetPtr = "&(" + objExpr + ")";
                }
                std::string callStr = "pc_" + defClass + "_" + c.name + "((struct pc_class_" + defClass + "*)" + targetPtr;
                for (auto& arg : c.args) callStr += ", " + expr(*arg);
                callStr += ");";
                line(callStr);
            } else {
                std::string callStr = cName(c.name) + "(";
                auto it = functions_.find(c.name);
                for (size_t i = 0; i < c.args.size(); ++i) {
                    if (i > 0) callStr += ", ";
                    if (it != functions_.end() && i < it->second.params.size() && it->second.params[i].isByRef) {
                        callStr += "&" + lvalueExpr(*c.args[i]);
                    } else {
                        callStr += expr(*c.args[i]);
                    }
                }
                callStr += ");";
                line(callStr);
            }
            break;
        }

        case Stmt::Kind::Return: {
            auto& r = static_cast<const ReturnStmt&>(s);
            if (r.value) line("return " + expr(*r.value) + ";");
            else line("return;");
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

        case Stmt::Kind::Repeat: {
            auto& r = static_cast<const RepeatStmt&>(s);
            line("do {");
            emitIndented(r.body);
            line("} while (!(" + expr(*r.cond) + "));");
            break;
        }

        case Stmt::Kind::For:
            emitFor(static_cast<const ForStmt&>(s));
            break;

        case Stmt::Kind::Case:
            emitCase(static_cast<const CaseStmt&>(s));
            break;

        case Stmt::Kind::OpenFile: {
            auto& o = static_cast<const OpenFileStmt&>(s);
            line("pc_open_file(" + expr(*o.filename) + ", \"" + o.mode + "\");");
            break;
        }

        case Stmt::Kind::CloseFile: {
            auto& cf = static_cast<const CloseFileStmt&>(s);
            line("pc_close_file(" + expr(*cf.filename) + ");");
            break;
        }

        case Stmt::Kind::ReadFile: {
            auto& rf = static_cast<const ReadFileStmt&>(s);
            line(lvalueExpr(*rf.target) + " = pc_read_file_line(" + expr(*rf.filename) + ");");
            break;
        }

        case Stmt::Kind::WriteFile: {
            auto& wf = static_cast<const WriteFileStmt&>(s);
            line("pc_write_file_line(" + expr(*wf.filename) + ", " + expr(*wf.value) + ");");
            break;
        }
    }
}

void CodeGen::emitCase(const CaseStmt& c) {
    std::string sel = expr(*c.selector);
    if (c.selector->type.base == BaseType::Integer) {
        line("switch (" + sel + ") {");
        for (const auto& b : c.branches) {
            for (const auto& v : b.values) {
                line("case " + expr(*v) + ":");
            }
            ++indent_;
            for (const auto& s : b.body) emitStmt(*s);
            line("break;");
            --indent_;
        }
        if (!c.otherwiseBlock.empty()) {
            line("default:");
            ++indent_;
            for (const auto& s : c.otherwiseBlock) emitStmt(*s);
            line("break;");
            --indent_;
        }
        line("}");
    } else {
        bool first = true;
        for (const auto& b : c.branches) {
            std::string cond;
            for (size_t i = 0; i < b.values.size(); ++i) {
                if (i > 0) cond += " || ";
                cond += "(strcmp(" + sel + ", " + expr(*b.values[i]) + ") == 0)";
            }
            if (first) {
                line("if (" + cond + ") {");
                first = false;
            } else {
                line("} else if (" + cond + ") {");
            }
            emitIndented(b.body);
        }
        if (!c.otherwiseBlock.empty()) {
            line("} else {");
            emitIndented(c.otherwiseBlock);
        }
        line("}");
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
            case BaseType::Char:
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
    std::string target;
    BaseType b = BaseType::Integer;
    if (in.target) {
        target = lvalueExpr(*in.target);
        b = in.target->type.base;
    } else {
        target = cName(in.name);
        b = varMap_[in.name].base;
        if (!in.indices.empty()) target += arrayOffset(in.name, in.indices);
    }

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
        case BaseType::Char:
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
    if (currentByRefParams_.find(f.var) != currentByRefParams_.end()) {
        var = "(*" + var + ")";
    }

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
                case Tok::CharLit:
                case Tok::StrLit:  return escapeCStr(l.text);
                case Tok::True:    return "true";
                default:           return "false";
            }
        }
        case Expr::Kind::Var: {
            std::string name = static_cast<const VarExpr&>(e).name;
            // Inside a class method, THIS maps to the THIS* pointer parameter
            if ((name == "THIS" || name == "this") && !currentClassName_.empty()) {
                return "(*pc_THIS)";
            }
            if (currentByRefParams_.find(name) != currentByRefParams_.end()) {
                return "(*" + cName(name) + ")";
            }
            if (!currentClassName_.empty() && varMap_.find(name) == varMap_.end()) {
                bool isProp = false;
                std::string cur = currentClassName_;
                while (!cur.empty()) {
                    auto cit = classTypes_.find(cur);
                    if (cit == classTypes_.end()) break;
                    for (const auto& p : cit->second.properties) {
                        if (p.name == name) { isProp = true; break; }
                    }
                    if (isProp) break;
                    cur = cit->second.superClass;
                }
                if (isProp) {
                    return "pc_THIS->" + cName(name);
                }
            }
            return cName(name);
        }
        case Expr::Kind::ArrayAccess: {
            auto& a = static_cast<const ArrayAccessExpr&>(e);
            std::string arrName = a.name;
            if (arrName.empty() && a.target && a.target->kind == Expr::Kind::Var) {
                arrName = static_cast<const VarExpr&>(*a.target).name;
            }
            if (!arrName.empty() && varMap_.find(arrName) != varMap_.end()) {
                return cName(arrName) + arrayOffset(arrName, a.indices);
            }
            std::string s = a.target ? lvalueExpr(*a.target) : cName(a.name);
            for (auto& idx : a.indices) s += "[" + expr(*idx) + "]";
            return s;
        }
        case Expr::Kind::MemberAccess: {
            auto& m = static_cast<const MemberAccessExpr&>(e);
            // If target is an Object type, use -> syntax; otherwise use .
            if (m.target->type.base == BaseType::Object) {
                return expr(*m.target) + "." + cName(m.field);
            }
            return lvalueExpr(*m.target) + "." + cName(m.field);
        }
        case Expr::Kind::Unary: {
            auto& u = static_cast<const UnaryExpr&>(e);
            return std::string(u.op == Tok::Not ? "(!" : "(-") + expr(*u.operand) + ")";
        }
        case Expr::Kind::Binary:
            return binary(static_cast<const BinaryExpr&>(e));
        case Expr::Kind::Call:
            return call(static_cast<const CallExpr&>(e));
        case Expr::Kind::UserCall:
            return userCall(static_cast<const UserCallExpr&>(e));
        case Expr::Kind::New: {
            auto& n = static_cast<const NewExpr&>(e);
            // Call the factory function that allocates the struct and calls the constructor
            std::string funcName = "pc_new_" + n.className;
            std::string s = funcName + "(";
            for (size_t i = 0; i < n.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += expr(*n.args[i]);
            }
            s += ")";
            return s;
        }
        case Expr::Kind::MethodCall: {
            auto& m = static_cast<const MethodCallExpr&>(e);
            std::string cls;
            std::string targetExpr;
            if (m.isSuper) {
                auto cit = classTypes_.find(currentClassName_);
                cls = (cit != classTypes_.end()) ? cit->second.superClass : "";
                targetExpr = "pc_THIS";
            } else {
                if (m.target->type.base == BaseType::Object) cls = m.target->type.recordName;
                else if (m.target->kind == Expr::Kind::Var &&
                    (static_cast<const VarExpr&>(*m.target).name == "THIS" || static_cast<const VarExpr&>(*m.target).name == "this")) {
                    cls = currentClassName_;
                }
                if (m.target->kind == Expr::Kind::Var &&
                    (static_cast<const VarExpr&>(*m.target).name == "THIS" || static_cast<const VarExpr&>(*m.target).name == "this")) {
                    targetExpr = "pc_THIS";
                } else {
                    targetExpr = "&(" + expr(*m.target) + ")";
                }
            }
            std::string cur = cls;
            std::string defClass;
            while (!cur.empty()) {
                auto it = classTypes_.find(cur);
                if (it == classTypes_.end()) break;
                if (it->second.methods.find(m.method) != it->second.methods.end()) {
                    defClass = cur;
                    break;
                }
                cur = it->second.superClass;
            }
            if (defClass.empty()) defClass = cls;
            std::string s = "pc_" + defClass + "_" + m.method + "((struct pc_class_" + defClass + "*)" + targetExpr;
            for (auto& arg : m.args) s += ", " + expr(*arg);
            s += ")";
            return s;
        }
    }
    return "";
}

std::string CodeGen::userCall(const UserCallExpr& c) {
    std::string s = cName(c.callee) + "(";
    auto it = functions_.find(c.callee);
    for (size_t i = 0; i < c.args.size(); ++i) {
        if (i > 0) s += ", ";
        if (it != functions_.end() && i < it->second.params.size() && it->second.params[i].isByRef) {
            s += "&" + lvalueExpr(*c.args[i]);
        } else {
            s += expr(*c.args[i]);
        }
    }
    s += ")";
    return s;
}

std::string CodeGen::call(const CallExpr& c) {
    switch (c.func) {
        case Tok::Length:
            return "((long long)strlen(" + expr(*c.args[0]) + "))";
        case Tok::Substring:
        case Tok::Mid:
            return "pc_substring(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ", " + expr(*c.args[2]) + ")";
        case Tok::Left:
            return "pc_left(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ")";
        case Tok::Right:
            return "pc_right(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ")";
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
        case Tok::Chr:
            return "pc_chr(" + expr(*c.args[0]) + ")";
        case Tok::Asc:
            return "pc_asc(" + expr(*c.args[0]) + ")";
        case Tok::IntFunc:
            return "pc_int(" + expr(*c.args[0]) + ")";
        case Tok::Round:
            return "pc_round(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ")";
        case Tok::Rnd:
            return "pc_rnd()";
        case Tok::Mod:
            return "pc_mod(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ")";
        case Tok::Div:
            return "pc_div(" + expr(*c.args[0]) + ", " + expr(*c.args[1]) + ")";
        case Tok::EofFunc:
            return "pc_eof(" + expr(*c.args[0]) + ")";
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
