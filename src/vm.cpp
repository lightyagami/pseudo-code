#include "vm.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

VM::VM(const std::vector<std::pair<std::string, TypeInfo>>& vars,
       const std::unordered_map<std::string, RecordDef>& recordTypes)
    : recordTypes_(recordTypes) {
    initGlobals(vars);
}

VM::~VM() {
    closeAllFiles();
}

void VM::closeAllFiles() {
    for (auto& pair : openFiles_) {
        if (pair.second) {
            fclose(pair.second);
        }
    }
    openFiles_.clear();
    fileModes_.clear();
}

void VM::initGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars) {
    globals_.clear();
    globals_.resize(vars.size());
    for (size_t i = 0; i < vars.size(); ++i) {
        globals_[i] = defaultValue(vars[i].second);
    }
}

void VM::syncGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars) {
    if (vars.size() > globals_.size()) {
        size_t oldSize = globals_.size();
        globals_.resize(vars.size());
        for (size_t i = oldSize; i < vars.size(); ++i) {
            globals_[i] = defaultValue(vars[i].second);
        }
    }
}

void VM::push(Value v) {
    stack_.push_back(std::move(v));
}

Value VM::pop() {
    Value v = std::move(stack_.back());
    stack_.pop_back();
    return v;
}

const Value& VM::peek() const {
    return stack_.back();
}

Value VM::defaultValue(const TypeInfo& t) {
    if (t.isArray) {
        auto arr = std::make_shared<ArrayData>();
        arr->dims = t.dims;
        arr->lower1 = t.lower1;
        arr->upper1 = t.upper1;
        arr->lower2 = t.lower2;
        arr->upper2 = t.upper2;
        arr->elemType = t.base;
        int64_t span1 = t.upper1 - t.lower1 + 1;
        int64_t span2 = (t.dims == 2) ? (t.upper2 - t.lower2 + 1) : 1;
        size_t total = static_cast<size_t>(span1 * span2);

        arr->data.reserve(total);
        for (size_t i = 0; i < total; ++i) {
            TypeInfo elemType{t.base, t.recordName, false, 0, 0, 0, 0, 0};
            arr->data.push_back(defaultValue(elemType));
        }
        return Value::makeArray(arr);
    }

    if (t.base == BaseType::Record) {
        auto rec = std::make_shared<RecordData>();
        rec->typeName = t.recordName;
        auto it = recordTypes_.find(t.recordName);
        if (it != recordTypes_.end()) {
            for (const auto& field : it->second.fields) {
                rec->fields[field.name] = defaultValue(field.type);
            }
        }
        return Value::makeRecord(rec);
    }

    switch (t.base) {
        case BaseType::Integer: return Value::makeInt(0);
        case BaseType::Real:    return Value::makeReal(0.0);
        case BaseType::Boolean: return Value::makeBool(false);
        case BaseType::String:
        case BaseType::Char:    return Value::makeString("");
        default:                return Value::makeInt(0);
    }
}

std::string VM::readLine() {
    std::string s;
    if (!std::getline(std::cin, s)) return "";
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

Value VM::deref(const Value& v) {
    if (!v.isRef() || !v.refVal) return v;
    const RefTarget& r = *v.refVal;
    switch (r.kind) {
        case RefTarget::Kind::Global:
            return globals_[r.globalSlot];
        case RefTarget::Kind::StackSlot: {
            const Value& target = stack_[r.stackIndex];
            return target.isRef() ? deref(target) : target;
        }
        case RefTarget::Kind::Array1D:
        case RefTarget::Kind::Array2D:
            if (r.array && r.arrayOffset < r.array->data.size())
                return r.array->data[r.arrayOffset];
            return Value::makeNil();
        case RefTarget::Kind::RecordField:
            if (r.record) {
                auto it = r.record->fields.find(r.fieldName);
                if (it != r.record->fields.end()) return it->second;
            }
            return Value::makeNil();
    }
    return Value::makeNil();
}

void VM::writeThrough(const Value& ref, Value newVal) {
    if (!ref.isRef() || !ref.refVal) return;
    const RefTarget& r = *ref.refVal;
    switch (r.kind) {
        case RefTarget::Kind::Global:
            globals_[r.globalSlot] = copyValue(newVal);
            break;
        case RefTarget::Kind::StackSlot: {
            if (stack_[r.stackIndex].isRef()) {
                writeThrough(stack_[r.stackIndex], newVal);
            } else {
                stack_[r.stackIndex] = copyValue(newVal);
            }
            break;
        }
        case RefTarget::Kind::Array1D:
        case RefTarget::Kind::Array2D:
            if (r.array && r.arrayOffset < r.array->data.size()) {
                if (r.array->elemType == BaseType::Real && newVal.isInt()) {
                    newVal = Value::makeReal(newVal.asReal());
                }
                r.array->data[r.arrayOffset] = copyValue(newVal);
            }
            break;
        case RefTarget::Kind::RecordField:
            if (r.record) {
                r.record->fields[r.fieldName] = copyValue(newVal);
            }
            break;
    }
}

Value& VM::getVarRef(int enc) {
    if (enc < 0) {
        int slot = -enc - 1;
        size_t idx = callFrames_.back().stackBase + slot;
        if (stack_[idx].isRef()) {
            const RefTarget& r = *stack_[idx].refVal;
            if (r.kind == RefTarget::Kind::Global) return globals_[r.globalSlot];
            if (r.kind == RefTarget::Kind::StackSlot) return stack_[r.stackIndex];
        }
        return stack_[idx];
    }
    return globals_[enc];
}

Value VM::getVarVal(int enc) {
    if (enc < 0) {
        int slot = -enc - 1;
        size_t idx = callFrames_.back().stackBase + slot;
        const Value& v = stack_[idx];
        return v.isRef() ? deref(v) : v;
    }
    return globals_[enc];
}

void VM::setVarVal(int enc, Value val) {
    if (enc < 0) {
        int slot = -enc - 1;
        size_t idx = callFrames_.back().stackBase + slot;
        Value& v = stack_[idx];
        if (v.isRef()) {
            writeThrough(v, val);
        } else {
            v = copyValue(val);
        }
    } else {
        globals_[enc] = copyValue(val);
    }
}

int VM::run(const Chunk& chunk, bool isRepl) {
    (void)isRepl;
    recordTypes_ = chunk.recordTypes;
    syncGlobals(chunk.varDescs);
    size_t ip = 0;
    stack_.clear();
    callFrames_.clear();

    auto runtimeErr = [&](const std::string& msg, int line) {
        (void)line;
        std::cerr << "Runtime Error: " << msg << "\n";
    };

    while (ip < chunk.code.size()) {
        const Instruction& inst = chunk.code[ip++];
        switch (inst.op) {
            case OpCode::OpHalt:
                closeAllFiles();
                return 0;

            case OpCode::OpConstant:
                push(chunk.constants[inst.a]);
                break;

            case OpCode::OpPop:
                pop();
                break;

            case OpCode::OpDup:
                push(peek());
                break;

            case OpCode::OpWidenReal: {
                Value v = pop();
                push(Value::makeReal(v.asReal()));
                break;
            }

            case OpCode::OpGetVar:
                push(getVarVal(inst.a));
                break;

            case OpCode::OpSetVar: {
                Value v = pop();
                setVarVal(inst.a, v);
                break;
            }

            case OpCode::OpPushRefVar: {
                int enc = inst.a;
                if (enc < 0) {
                    int slot = -enc - 1;
                    size_t idx = callFrames_.back().stackBase + slot;
                    if (stack_[idx].isRef()) {
                        push(stack_[idx]);
                    } else {
                        push(Value::makeRef(RefTarget::makeStackSlot(idx)));
                    }
                } else {
                    push(Value::makeRef(RefTarget::makeGlobal(enc)));
                }
                break;
            }

            case OpCode::OpPushRefArray1D: {
                int64_t idx = pop().asInt();
                int enc = inst.a;
                const std::string& name = chunk.constants[inst.b].asString();
                Value& var = getVarRef(enc);
                auto& arr = var.arrVal;
                if (!arr || idx < arr->lower1 || idx > arr->upper1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx, arr ? (long long)arr->lower1 : 0LL, arr ? (long long)arr->upper1 : 0LL);
                    runtimeErr(buf, inst.line);
                    return 1;
                }
                push(Value::makeRef(RefTarget::makeArray(arr, arr->offset1D(idx))));
                break;
            }

            case OpCode::OpPushRefArray2D: {
                int64_t idx2 = pop().asInt();
                int64_t idx1 = pop().asInt();
                int enc = inst.a;
                const std::string& name = chunk.constants[inst.b].asString();
                Value& var = getVarRef(enc);
                auto& arr = var.arrVal;
                if (!arr || idx1 < arr->lower1 || idx1 > arr->upper1 || idx2 < arr->lower2 || idx2 > arr->upper2) {
                    runtimeErr("Array index out of bounds on '" + name + "'", inst.line);
                    return 1;
                }
                push(Value::makeRef(RefTarget::makeArray(arr, arr->offset2D(idx1, idx2))));
                break;
            }

            case OpCode::OpPushRefField: {
                Value target = pop();
                if (!target.isRecord() || !target.recVal) {
                    runtimeErr("Attempt to access field of non-record", inst.line);
                    return 1;
                }
                const std::string& field = chunk.constants[inst.a].asString();
                push(Value::makeRef(RefTarget::makeField(target.recVal, field)));
                break;
            }

            case OpCode::OpGetArray1D: {
                int64_t idx = pop().asInt();
                const std::string& name = chunk.constants[inst.b].asString();
                Value& var = getVarRef(inst.a);
                auto& arr = var.arrVal;
                if (!arr || idx < arr->lower1 || idx > arr->upper1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx, arr ? (long long)arr->lower1 : 0LL, arr ? (long long)arr->upper1 : 0LL);
                    runtimeErr(buf, inst.line);
                    return 1;
                }
                push(arr->data[arr->offset1D(idx)]);
                break;
            }

            case OpCode::OpSetArray1D: {
                int64_t idx = pop().asInt();
                Value val = pop();
                const std::string& name = chunk.constants[inst.b].asString();
                Value& var = getVarRef(inst.a);
                auto& arr = var.arrVal;
                if (!arr || idx < arr->lower1 || idx > arr->upper1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx, arr ? (long long)arr->lower1 : 0LL, arr ? (long long)arr->upper1 : 0LL);
                    runtimeErr(buf, inst.line);
                    return 1;
                }
                if (arr->elemType == BaseType::Real && val.isInt()) {
                    val = Value::makeReal(val.asReal());
                }
                arr->data[arr->offset1D(idx)] = copyValue(val);
                break;
            }

            case OpCode::OpGetArray2D: {
                int64_t idx2 = pop().asInt();
                int64_t idx1 = pop().asInt();
                const std::string& name = chunk.constants[inst.b].asString();
                Value& var = getVarRef(inst.a);
                auto& arr = var.arrVal;
                if (!arr || idx1 < arr->lower1 || idx1 > arr->upper1 || idx2 < arr->lower2 || idx2 > arr->upper2) {
                    runtimeErr("Array index out of bounds on '" + name + "'", inst.line);
                    return 1;
                }
                push(arr->data[arr->offset2D(idx1, idx2)]);
                break;
            }

            case OpCode::OpSetArray2D: {
                int64_t idx2 = pop().asInt();
                int64_t idx1 = pop().asInt();
                Value val = pop();
                const std::string& name = chunk.constants[inst.b].asString();
                Value& var = getVarRef(inst.a);
                auto& arr = var.arrVal;
                if (!arr || idx1 < arr->lower1 || idx1 > arr->upper1 || idx2 < arr->lower2 || idx2 > arr->upper2) {
                    runtimeErr("Array index out of bounds on '" + name + "'", inst.line);
                    return 1;
                }
                if (arr->elemType == BaseType::Real && val.isInt()) {
                    val = Value::makeReal(val.asReal());
                }
                arr->data[arr->offset2D(idx1, idx2)] = copyValue(val);
                break;
            }

            case OpCode::OpGetField: {
                Value target = pop();
                if (!target.isRecord() || !target.recVal) {
                    runtimeErr("Attempt to access field of non-record", inst.line);
                    return 1;
                }
                const std::string& field = chunk.constants[inst.a].asString();
                auto it = target.recVal->fields.find(field);
                if (it != target.recVal->fields.end()) {
                    push(it->second);
                } else {
                    push(Value::makeNil());
                }
                break;
            }

            case OpCode::OpSetField: {
                Value target = pop();
                Value val = pop();
                if (!target.isRecord() || !target.recVal) {
                    runtimeErr("Attempt to set field of non-record", inst.line);
                    return 1;
                }
                const std::string& field = chunk.constants[inst.a].asString();
                target.recVal->fields[field] = copyValue(val);
                break;
            }

            case OpCode::OpCall: {
                const std::string& name = chunk.constants[inst.a].asString();
                auto fit = chunk.functions.find(name);
                if (fit == chunk.functions.end()) {
                    runtimeErr("Call to unknown function or procedure '" + name + "'", inst.line);
                    return 1;
                }
                const FunctionInfo& fi = fit->second;
                size_t stackBase = stack_.size() - inst.b;
                for (size_t i = inst.b; i < fi.localVars.size(); ++i) {
                    push(defaultValue(fi.localVars[i].second));
                }
                CallFrame frame;
                frame.funcName = name;
                frame.returnIp = ip;
                frame.stackBase = stackBase;
                frame.numLocals = static_cast<int>(fi.localVars.size());
                frame.isFunction = (inst.c != 0);
                callFrames_.push_back(frame);
                ip = fi.entryIp;
                break;
            }

            case OpCode::OpReturn: {
                if (callFrames_.empty()) {
                    closeAllFiles();
                    return 0;
                }
                CallFrame frame = callFrames_.back();
                callFrames_.pop_back();
                stack_.resize(frame.stackBase);
                ip = frame.returnIp;
                break;
            }

            case OpCode::OpReturnVal: {
                Value retVal = pop();
                if (callFrames_.empty()) {
                    closeAllFiles();
                    return 0;
                }
                CallFrame frame = callFrames_.back();
                callFrames_.pop_back();
                stack_.resize(frame.stackBase);
                ip = frame.returnIp;
                push(retVal);
                break;
            }

            case OpCode::OpOpenFile: {
                const std::string& mode = chunk.constants[inst.a].asString();
                std::string fname = pop().asString();
                const char* fmode = "r";
                if (mode == "WRITE") fmode = "w";
                else if (mode == "APPEND") fmode = "a";

                if (openFiles_.find(fname) != openFiles_.end()) {
                    fclose(openFiles_[fname]);
                }
                FILE* fp = fopen(fname.c_str(), fmode);
                if (!fp) {
                    runtimeErr("Cannot open file '" + fname + "' for " + mode, inst.line);
                    return 1;
                }
                openFiles_[fname] = fp;
                fileModes_[fname] = mode;
                break;
            }

            case OpCode::OpCloseFile: {
                std::string fname = pop().asString();
                auto it = openFiles_.find(fname);
                if (it != openFiles_.end()) {
                    fclose(it->second);
                    openFiles_.erase(it);
                    fileModes_.erase(fname);
                }
                break;
            }

            case OpCode::OpReadFile: {
                std::string fname = pop().asString();
                auto it = openFiles_.find(fname);
                if (it == openFiles_.end()) {
                    runtimeErr("File '" + fname + "' is not open", inst.line);
                    return 1;
                }
                char buf[4096];
                if (!fgets(buf, sizeof(buf), it->second)) buf[0] = '\0';
                std::string lineStr(buf);
                while (!lineStr.empty() && (lineStr.back() == '\r' || lineStr.back() == '\n')) {
                    lineStr.pop_back();
                }
                BaseType targetBase = static_cast<BaseType>(inst.a);
                switch (targetBase) {
                    case BaseType::Integer: {
                        char* end = nullptr;
                        long long val = std::strtoll(lineStr.c_str(), &end, 10);
                        push(Value::makeInt(end == lineStr.c_str() ? 0LL : val));
                        break;
                    }
                    case BaseType::Real: {
                        char* end = nullptr;
                        double val = std::strtod(lineStr.c_str(), &end);
                        push(Value::makeReal(end == lineStr.c_str() ? 0.0 : val));
                        break;
                    }
                    case BaseType::Boolean: {
                        std::string up = lineStr;
                        for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                        push(Value::makeBool(up == "TRUE" || up == "1"));
                        break;
                    }
                    case BaseType::String:
                    default:
                        push(Value::makeString(lineStr));
                        break;
                }
                break;
            }

            case OpCode::OpWriteFile: {
                Value val = pop();
                std::string fname = pop().asString();
                auto it = openFiles_.find(fname);
                if (it == openFiles_.end()) {
                    runtimeErr("File '" + fname + "' is not open", inst.line);
                    return 1;
                }
                std::ostringstream ss;
                val.print(ss);
                fprintf(it->second, "%s\n", ss.str().c_str());
                fflush(it->second);
                break;
            }

            case OpCode::OpEof: {
                std::string fname = pop().asString();
                auto it = openFiles_.find(fname);
                if (it == openFiles_.end()) {
                    runtimeErr("File '" + fname + "' is not open", inst.line);
                    return 1;
                }
                int ch = fgetc(it->second);
                if (ch == EOF) {
                    push(Value::makeBool(true));
                } else {
                    ungetc(ch, it->second);
                    push(Value::makeBool(false));
                }
                break;
            }

            case OpCode::OpAdd: {
                Value b = pop();
                Value a = pop();
                if (a.isInt() && b.isInt()) {
                    int64_t res;
                    if (__builtin_add_overflow(a.asInt(), b.asInt(), &res)) {
                        runtimeErr("64-bit integer addition overflow", inst.line);
                        return 1;
                    }
                    push(Value::makeInt(res));
                } else {
                    push(Value::makeReal(a.asReal() + b.asReal()));
                }
                break;
            }

            case OpCode::OpSub: {
                Value b = pop();
                Value a = pop();
                if (a.isInt() && b.isInt()) {
                    int64_t res;
                    if (__builtin_sub_overflow(a.asInt(), b.asInt(), &res)) {
                        runtimeErr("64-bit integer subtraction overflow", inst.line);
                        return 1;
                    }
                    push(Value::makeInt(res));
                } else {
                    push(Value::makeReal(a.asReal() - b.asReal()));
                }
                break;
            }

            case OpCode::OpMul: {
                Value b = pop();
                Value a = pop();
                if (a.isInt() && b.isInt()) {
                    int64_t res;
                    if (__builtin_mul_overflow(a.asInt(), b.asInt(), &res)) {
                        runtimeErr("64-bit integer multiplication overflow", inst.line);
                        return 1;
                    }
                    push(Value::makeInt(res));
                } else {
                    push(Value::makeReal(a.asReal() * b.asReal()));
                }
                break;
            }

            case OpCode::OpDivReal: {
                Value b = pop();
                Value a = pop();
                if (b.asReal() == 0.0) {
                    runtimeErr("Division by zero", inst.line);
                    return 1;
                }
                push(Value::makeReal(a.asReal() / b.asReal()));
                break;
            }

            case OpCode::OpDivInt: {
                Value b = pop();
                Value a = pop();
                if (b.asInt() == 0) {
                    runtimeErr("Integer division (DIV) by zero", inst.line);
                    return 1;
                }
                if (a.asInt() == INT64_MIN && b.asInt() == -1) {
                    runtimeErr("64-bit integer division overflow", inst.line);
                    return 1;
                }
                push(Value::makeInt(a.asInt() / b.asInt()));
                break;
            }

            case OpCode::OpMod: {
                Value b = pop();
                Value a = pop();
                if (b.asInt() == 0) {
                    runtimeErr("Modulo (MOD) by zero", inst.line);
                    return 1;
                }
                push(Value::makeInt(a.asInt() % b.asInt()));
                break;
            }

            case OpCode::OpNeg: {
                Value v = pop();
                if (v.isInt()) {
                    int64_t res;
                    if (__builtin_sub_overflow(0, v.asInt(), &res)) {
                        runtimeErr("64-bit integer negation overflow", inst.line);
                        return 1;
                    }
                    push(Value::makeInt(res));
                } else {
                    push(Value::makeReal(-v.asReal()));
                }
                break;
            }

            case OpCode::OpConcat: {
                Value b = pop();
                Value a = pop();
                push(Value::makeString(a.asString() + b.asString()));
                break;
            }

            case OpCode::OpLength: {
                Value v = pop();
                push(Value::makeInt(static_cast<int64_t>(v.asString().size())));
                break;
            }

            case OpCode::OpSubstring: {
                Value lenVal = pop();
                Value startVal = pop();
                Value strVal = pop();
                int64_t start = startVal.asInt();
                int64_t len = lenVal.asInt();
                const std::string& s = strVal.asString();
                if (start < 1 || start > static_cast<int64_t>(s.size())) {
                    push(Value::makeString(""));
                } else {
                    size_t st = static_cast<size_t>(start - 1);
                    size_t count = (len < 0) ? 0 : static_cast<size_t>(len);
                    push(Value::makeString(s.substr(st, count)));
                }
                break;
            }

            case OpCode::OpUCase: {
                Value v = pop();
                std::string s = v.asString();
                for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                push(Value::makeString(s));
                break;
            }

            case OpCode::OpLCase: {
                Value v = pop();
                std::string s = v.asString();
                for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                push(Value::makeString(s));
                break;
            }

            case OpCode::OpNumToStr: {
                Value v = pop();
                if (v.isInt()) {
                    push(Value::makeString(std::to_string(v.asInt())));
                } else {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.10g", v.asReal());
                    push(Value::makeString(buf));
                }
                break;
            }

            case OpCode::OpStrToNum: {
                Value v = pop();
                const std::string& s = v.asString();
                if (s.find('.') != std::string::npos) {
                    char* end = nullptr;
                    double val = std::strtod(s.c_str(), &end);
                    push(Value::makeReal(end == s.c_str() ? 0.0 : val));
                } else {
                    char* end = nullptr;
                    long long val = std::strtoll(s.c_str(), &end, 10);
                    push(Value::makeInt(end == s.c_str() ? 0LL : val));
                }
                break;
            }

            case OpCode::OpLeft: {
                Value lenVal = pop();
                Value strVal = pop();
                int64_t len = lenVal.asInt();
                const std::string& s = strVal.asString();
                if (len <= 0) {
                    push(Value::makeString(""));
                } else {
                    size_t count = (static_cast<size_t>(len) > s.size()) ? s.size() : static_cast<size_t>(len);
                    push(Value::makeString(s.substr(0, count)));
                }
                break;
            }

            case OpCode::OpRight: {
                Value lenVal = pop();
                Value strVal = pop();
                int64_t len = lenVal.asInt();
                const std::string& s = strVal.asString();
                if (len <= 0) {
                    push(Value::makeString(""));
                } else {
                    size_t count = (static_cast<size_t>(len) > s.size()) ? s.size() : static_cast<size_t>(len);
                    push(Value::makeString(s.substr(s.size() - count, count)));
                }
                break;
            }

            case OpCode::OpChr: {
                Value codeVal = pop();
                int64_t code = codeVal.asInt();
                char c = static_cast<char>(code & 0xFF);
                push(Value::makeString(std::string(1, c)));
                break;
            }

            case OpCode::OpAsc: {
                Value strVal = pop();
                const std::string& s = strVal.asString();
                int64_t code = s.empty() ? 0 : static_cast<int64_t>(static_cast<unsigned char>(s[0]));
                push(Value::makeInt(code));
                break;
            }

            case OpCode::OpInt: {
                Value v = pop();
                double realVal = v.asReal();
                push(Value::makeInt(static_cast<int64_t>(std::floor(realVal))));
                break;
            }

            case OpCode::OpRound: {
                Value placesVal = pop();
                Value vVal = pop();
                double v = vVal.asReal();
                int64_t places = placesVal.asInt();
                double factor = std::pow(10.0, static_cast<double>(places));
                push(Value::makeReal(std::round(v * factor) / factor));
                break;
            }

            case OpCode::OpEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isInt() && b.isInt()) push(Value::makeBool(a.asInt() == b.asInt()));
                else if (a.isReal() || b.isReal()) push(Value::makeBool(a.asReal() == b.asReal()));
                else if (a.isString() && b.isString()) push(Value::makeBool(a.asString() == b.asString()));
                else if (a.isBool() && b.isBool()) push(Value::makeBool(a.asBool() == b.asBool()));
                else push(Value::makeBool(false));
                break;
            }

            case OpCode::OpNotEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isInt() && b.isInt()) push(Value::makeBool(a.asInt() != b.asInt()));
                else if (a.isReal() || b.isReal()) push(Value::makeBool(a.asReal() != b.asReal()));
                else if (a.isString() && b.isString()) push(Value::makeBool(a.asString() != b.asString()));
                else if (a.isBool() && b.isBool()) push(Value::makeBool(a.asBool() != b.asBool()));
                else push(Value::makeBool(true));
                break;
            }

            case OpCode::OpLess: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) push(Value::makeBool(a.asString() < b.asString()));
                else push(Value::makeBool(a.asReal() < b.asReal()));
                break;
            }

            case OpCode::OpLessEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) push(Value::makeBool(a.asString() <= b.asString()));
                else push(Value::makeBool(a.asReal() <= b.asReal()));
                break;
            }

            case OpCode::OpGreater: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) push(Value::makeBool(a.asString() > b.asString()));
                else push(Value::makeBool(a.asReal() > b.asReal()));
                break;
            }

            case OpCode::OpGreaterEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) push(Value::makeBool(a.asString() >= b.asString()));
                else push(Value::makeBool(a.asReal() >= b.asReal()));
                break;
            }

            case OpCode::OpNot: {
                Value v = pop();
                push(Value::makeBool(!v.asBool()));
                break;
            }

            case OpCode::OpJump:
                ip = static_cast<size_t>(inst.a);
                break;

            case OpCode::OpJumpIfFalse: {
                Value c = pop();
                if (!c.asBool()) ip = static_cast<size_t>(inst.a);
                break;
            }

            case OpCode::OpJumpIfFalseOrPop: {
                if (!peek().asBool()) {
                    ip = static_cast<size_t>(inst.a);
                } else {
                    pop();
                }
                break;
            }

            case OpCode::OpJumpIfTrueOrPop: {
                if (peek().asBool()) {
                    ip = static_cast<size_t>(inst.a);
                } else {
                    pop();
                }
                break;
            }

            case OpCode::OpCheckStep: {
                int64_t step = getVarVal(inst.a).asInt();
                if (step == 0) {
                    runtimeErr("FOR step cannot be zero", inst.line);
                    return 1;
                }
                break;
            }

            case OpCode::OpForCheck: {
                int64_t varVal = getVarVal(inst.a).asInt();
                int64_t endVal = getVarVal(inst.b).asInt();
                int64_t stepVal = getVarVal(inst.c).asInt();
                if (stepVal > 0 ? (varVal > endVal) : (varVal < endVal)) {
                    ip = static_cast<size_t>(inst.d);
                }
                break;
            }

            case OpCode::OpForStep: {
                int64_t varVal = getVarVal(inst.a).asInt();
                int64_t stepVal = getVarVal(inst.b).asInt();
                int64_t res;
                if (__builtin_add_overflow(varVal, stepVal, &res)) {
                    runtimeErr("64-bit integer addition overflow", inst.line);
                    return 1;
                }
                setVarVal(inst.a, Value::makeInt(res));
                break;
            }

            case OpCode::OpPrint: {
                Value v = pop();
                v.print(std::cout);
                break;
            }

            case OpCode::OpPrintLn:
                std::cout << "\n";
                break;

            case OpCode::OpReadInt: {
                std::string line = readLine();
                char* end = nullptr;
                long long val = std::strtoll(line.c_str(), &end, 10);
                push(Value::makeInt(end == line.c_str() ? 0LL : val));
                break;
            }

            case OpCode::OpReadReal: {
                std::string line = readLine();
                char* end = nullptr;
                double val = std::strtod(line.c_str(), &end);
                push(Value::makeReal(end == line.c_str() ? 0.0 : val));
                break;
            }

            case OpCode::OpReadBool: {
                std::string line = readLine();
                size_t idx = 0;
                while (idx < line.size() && std::isspace(static_cast<unsigned char>(line[idx]))) idx++;
                std::string sub = line.substr(idx);
                for (char& ch : sub) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                push(Value::makeBool(sub == "TRUE" || sub == "1"));
                break;
            }

            case OpCode::OpReadStr: {
                push(Value::makeString(readLine()));
                break;
            }
        }
    }
    closeAllFiles();
    return 0;
}
