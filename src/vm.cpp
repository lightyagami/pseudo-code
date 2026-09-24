#include "vm.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>

VM::VM(const std::vector<std::pair<std::string, TypeInfo>>& vars) {
    initGlobals(vars);
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
        Value elemDef;
        switch (t.base) {
            case BaseType::Integer: elemDef = Value::makeInt(0); break;
            case BaseType::Real:    elemDef = Value::makeReal(0.0); break;
            case BaseType::Boolean: elemDef = Value::makeBool(false); break;
            case BaseType::String:  elemDef = Value::makeString(""); break;
            default:                elemDef = Value::makeInt(0); break;
        }
        arr->data.assign(total, elemDef);
        return Value::makeArray(arr);
    }
    switch (t.base) {
        case BaseType::Integer: return Value::makeInt(0);
        case BaseType::Real:    return Value::makeReal(0.0);
        case BaseType::Boolean: return Value::makeBool(false);
        case BaseType::String:  return Value::makeString("");
        default:                return Value::makeInt(0);
    }
}

std::string VM::readLine() {
    std::string s;
    if (!std::getline(std::cin, s)) return "";
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

int VM::run(const Chunk& chunk, bool isRepl) {
    (void)isRepl;
    syncGlobals(chunk.varDescs);
    size_t ip = 0;
    stack_.clear();

    auto runtimeErr = [&](const std::string& msg, int line) {
        (void)line;
        std::cerr << "Runtime Error: " << msg << "\n";
    };

    while (ip < chunk.code.size()) {
        const Instruction& inst = chunk.code[ip++];
        switch (inst.op) {
            case OpCode::OpHalt:
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

            case OpCode::OpGetGlobal:
                push(globals_[inst.a]);
                break;

            case OpCode::OpSetGlobal: {
                Value v = pop();
                globals_[inst.a] = v;
                break;
            }

            case OpCode::OpGetArray1D: {
                int64_t idx = pop().asInt();
                const std::string& name = chunk.constants[inst.b].asString();
                auto& arr = globals_[inst.a].arrVal;
                if (idx < arr->lower1 || idx > arr->upper1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx, (long long)arr->lower1, (long long)arr->upper1);
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
                auto& arr = globals_[inst.a].arrVal;
                if (idx < arr->lower1 || idx > arr->upper1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx, (long long)arr->lower1, (long long)arr->upper1);
                    runtimeErr(buf, inst.line);
                    return 1;
                }
                if (arr->elemType == BaseType::Real && val.isInt()) {
                    val = Value::makeReal(val.asReal());
                }
                arr->data[arr->offset1D(idx)] = val;
                break;
            }

            case OpCode::OpGetArray2D: {
                int64_t idx2 = pop().asInt();
                int64_t idx1 = pop().asInt();
                const std::string& name = chunk.constants[inst.b].asString();
                auto& arr = globals_[inst.a].arrVal;
                if (idx1 < arr->lower1 || idx1 > arr->upper1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx1, (long long)arr->lower1, (long long)arr->upper1);
                    runtimeErr(buf, inst.line);
                    return 1;
                }
                if (idx2 < arr->lower2 || idx2 > arr->upper2) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx2, (long long)arr->lower2, (long long)arr->upper2);
                    runtimeErr(buf, inst.line);
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
                auto& arr = globals_[inst.a].arrVal;
                if (idx1 < arr->lower1 || idx1 > arr->upper1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx1, (long long)arr->lower1, (long long)arr->upper1);
                    runtimeErr(buf, inst.line);
                    return 1;
                }
                if (idx2 < arr->lower2 || idx2 > arr->upper2) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Array index out of bounds on '%s': index %lld not in [%lld:%lld]",
                             name.c_str(), (long long)idx2, (long long)arr->lower2, (long long)arr->upper2);
                    runtimeErr(buf, inst.line);
                    return 1;
                }
                if (arr->elemType == BaseType::Real && val.isInt()) {
                    val = Value::makeReal(val.asReal());
                }
                arr->data[arr->offset2D(idx1, idx2)] = val;
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
                double denom = b.asReal();
                if (denom == 0.0) {
                    runtimeErr("Division by zero", inst.line);
                    return 1;
                }
                push(Value::makeReal(a.asReal() / denom));
                break;
            }

            case OpCode::OpDivInt: {
                int64_t b = pop().asInt();
                int64_t a = pop().asInt();
                if (b == 0) {
                    runtimeErr("Division by zero", inst.line);
                    return 1;
                }
                int64_t q = a / b, r = a % b;
                if ((r != 0) && ((r < 0) ^ (b < 0))) q--;
                push(Value::makeInt(q));
                break;
            }

            case OpCode::OpMod: {
                int64_t b = pop().asInt();
                int64_t a = pop().asInt();
                if (b == 0) {
                    runtimeErr("Modulo by zero", inst.line);
                    return 1;
                }
                int64_t r = a % b;
                if ((r != 0) && ((r < 0) ^ (b < 0))) r += b;
                push(Value::makeInt(r));
                break;
            }

            case OpCode::OpNeg: {
                Value v = pop();
                if (v.isInt()) {
                    if (v.asInt() == INT64_MIN) {
                        runtimeErr("64-bit integer negation overflow", inst.line);
                        return 1;
                    }
                    push(Value::makeInt(-v.asInt()));
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
                Value s = pop();
                push(Value::makeInt(static_cast<int64_t>(s.asString().size())));
                break;
            }

            case OpCode::OpSubstring: {
                int64_t len = pop().asInt();
                int64_t start = pop().asInt();
                std::string s = pop().asString();
                int64_t slen = static_cast<int64_t>(s.size());
                if (start < 1) start = 1;
                if (len < 0) len = 0;
                if (start > slen) {
                    push(Value::makeString(""));
                } else {
                    int64_t st = start - 1;
                    if (st + len > slen) len = slen - st;
                    push(Value::makeString(s.substr(st, len)));
                }
                break;
            }

            case OpCode::OpUCase: {
                std::string s = pop().asString();
                for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                push(Value::makeString(std::move(s)));
                break;
            }

            case OpCode::OpLCase: {
                std::string s = pop().asString();
                for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                push(Value::makeString(std::move(s)));
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
                std::string s = pop().asString();
                push(Value::makeReal(std::atof(s.c_str())));
                break;
            }

            case OpCode::OpEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) {
                    push(Value::makeBool(a.asString() == b.asString()));
                } else if (a.isBool() && b.isBool()) {
                    push(Value::makeBool(a.asBool() == b.asBool()));
                } else {
                    push(Value::makeBool(a.asReal() == b.asReal()));
                }
                break;
            }

            case OpCode::OpNotEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) {
                    push(Value::makeBool(a.asString() != b.asString()));
                } else if (a.isBool() && b.isBool()) {
                    push(Value::makeBool(a.asBool() != b.asBool()));
                } else {
                    push(Value::makeBool(a.asReal() != b.asReal()));
                }
                break;
            }

            case OpCode::OpLess: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) {
                    push(Value::makeBool(a.asString() < b.asString()));
                } else {
                    push(Value::makeBool(a.asReal() < b.asReal()));
                }
                break;
            }

            case OpCode::OpLessEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) {
                    push(Value::makeBool(a.asString() <= b.asString()));
                } else {
                    push(Value::makeBool(a.asReal() <= b.asReal()));
                }
                break;
            }

            case OpCode::OpGreater: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) {
                    push(Value::makeBool(a.asString() > b.asString()));
                } else {
                    push(Value::makeBool(a.asReal() > b.asReal()));
                }
                break;
            }

            case OpCode::OpGreaterEqual: {
                Value b = pop();
                Value a = pop();
                if (a.isString() && b.isString()) {
                    push(Value::makeBool(a.asString() >= b.asString()));
                } else {
                    push(Value::makeBool(a.asReal() >= b.asReal()));
                }
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
                int64_t step = globals_[inst.a].asInt();
                if (step == 0) {
                    runtimeErr("FOR step cannot be zero", inst.line);
                    return 1;
                }
                break;
            }

            case OpCode::OpForCheck: {
                int64_t varVal = globals_[inst.a].asInt();
                int64_t endVal = globals_[inst.b].asInt();
                int64_t stepVal = globals_[inst.c].asInt();
                if (stepVal > 0 ? (varVal > endVal) : (varVal < endVal)) {
                    ip = static_cast<size_t>(inst.d);
                }
                break;
            }

            case OpCode::OpForStep: {
                int64_t varVal = globals_[inst.a].asInt();
                int64_t stepVal = globals_[inst.b].asInt();
                int64_t res;
                if (__builtin_add_overflow(varVal, stepVal, &res)) {
                    runtimeErr("64-bit integer addition overflow", inst.line);
                    return 1;
                }
                globals_[inst.a].intVal = res;
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
    return 0;
}
