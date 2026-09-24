#ifndef PSEUDOC_BYTECODE_H
#define PSEUDOC_BYTECODE_H

#include "ast.h"
#include "sema.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct ArrayData;
struct RecordData;
struct RefTarget;
struct ObjectData;

enum class ValueKind { Nil, Int, Real, Bool, String, Array, Record, Ref, Object };

struct Value {
    ValueKind kind = ValueKind::Nil;
    int64_t intVal = 0;
    double realVal = 0.0;
    bool boolVal = false;
    std::string strVal;
    std::shared_ptr<ArrayData> arrVal;
    std::shared_ptr<RecordData> recVal;
    std::shared_ptr<RefTarget> refVal;
    std::shared_ptr<ObjectData> objVal;

    static Value makeNil() { return Value(); }
    static Value makeInt(int64_t v) { Value val; val.kind = ValueKind::Int; val.intVal = v; return val; }
    static Value makeReal(double v) { Value val; val.kind = ValueKind::Real; val.realVal = v; return val; }
    static Value makeBool(bool v) { Value val; val.kind = ValueKind::Bool; val.boolVal = v; return val; }
    static Value makeString(std::string v) { Value val; val.kind = ValueKind::String; val.strVal = std::move(v); return val; }
    static Value makeArray(std::shared_ptr<ArrayData> v) { Value val; val.kind = ValueKind::Array; val.arrVal = std::move(v); return val; }
    static Value makeRecord(std::shared_ptr<RecordData> v) { Value val; val.kind = ValueKind::Record; val.recVal = std::move(v); return val; }
    static Value makeRef(std::shared_ptr<RefTarget> v) { Value val; val.kind = ValueKind::Ref; val.refVal = std::move(v); return val; }
    static Value makeObject(std::shared_ptr<ObjectData> v) { Value val; val.kind = ValueKind::Object; val.objVal = std::move(v); return val; }

    bool isNil() const { return kind == ValueKind::Nil; }
    bool isInt() const { return kind == ValueKind::Int; }
    bool isReal() const { return kind == ValueKind::Real; }
    bool isBool() const { return kind == ValueKind::Bool; }
    bool isString() const { return kind == ValueKind::String; }
    bool isArray() const { return kind == ValueKind::Array; }
    bool isRecord() const { return kind == ValueKind::Record; }
    bool isRef() const { return kind == ValueKind::Ref; }
    bool isObject() const { return kind == ValueKind::Object; }

    int64_t asInt() const { return intVal; }
    double asReal() const { return isInt() ? static_cast<double>(intVal) : realVal; }
    bool asBool() const { return boolVal; }
    const std::string& asString() const { return strVal; }

    void print(std::ostream& os) const;
};

struct ObjectData {
    std::string className;
    std::unordered_map<std::string, Value> fields;
};

struct RecordData {
    std::string typeName;
    std::unordered_map<std::string, Value> fields;
};

inline Value copyValue(const Value& v) {
    if (v.isRecord() && v.recVal) {
        auto newRec = std::make_shared<RecordData>();
        newRec->typeName = v.recVal->typeName;
        for (const auto& kv : v.recVal->fields) {
            newRec->fields[kv.first] = copyValue(kv.second);
        }
        return Value::makeRecord(newRec);
    }
    return v;
}

struct ArrayData {
    int dims = 1;
    int64_t lower1 = 0, upper1 = 0;
    int64_t lower2 = 0, upper2 = 0;
    BaseType elemType = BaseType::Integer;
    std::vector<Value> data;

    size_t offset1D(int64_t i) const {
        return static_cast<size_t>(i - lower1);
    }
    size_t offset2D(int64_t i1, int64_t i2) const {
        int64_t span2 = upper2 - lower2 + 1;
        return static_cast<size_t>((i1 - lower1) * span2 + (i2 - lower2));
    }
};

struct RefTarget {
    enum class Kind { Global, StackSlot, Array1D, Array2D, RecordField, ObjectField };
    Kind kind = Kind::Global;
    int globalSlot = -1;
    size_t stackIndex = 0;
    std::shared_ptr<ArrayData> array;
    size_t arrayOffset = 0;
    std::shared_ptr<RecordData> record;
    std::shared_ptr<ObjectData> object;
    std::string fieldName;

    static std::shared_ptr<RefTarget> makeGlobal(int slot) {
        auto r = std::make_shared<RefTarget>();
        r->kind = Kind::Global;
        r->globalSlot = slot;
        return r;
    }
    static std::shared_ptr<RefTarget> makeStackSlot(size_t idx) {
        auto r = std::make_shared<RefTarget>();
        r->kind = Kind::StackSlot;
        r->stackIndex = idx;
        return r;
    }
    static std::shared_ptr<RefTarget> makeArray(std::shared_ptr<ArrayData> arr, size_t off) {
        auto r = std::make_shared<RefTarget>();
        r->kind = Kind::Array1D;
        r->array = std::move(arr);
        r->arrayOffset = off;
        return r;
    }
    static std::shared_ptr<RefTarget> makeField(std::shared_ptr<RecordData> rec, std::string field) {
        auto r = std::make_shared<RefTarget>();
        r->kind = Kind::RecordField;
        r->record = std::move(rec);
        r->fieldName = std::move(field);
        return r;
    }
    static std::shared_ptr<RefTarget> makeObjectField(std::shared_ptr<ObjectData> obj, std::string field) {
        auto r = std::make_shared<RefTarget>();
        r->kind = Kind::ObjectField;
        r->object = std::move(obj);
        r->fieldName = std::move(field);
        return r;
    }
};

enum class OpCode : uint8_t {
    OpConstant,
    OpPop,
    OpDup,
    OpWidenReal,

    // Variable access (inst.a < 0 -> local -inst.a-1, inst.a >= 0 -> global inst.a)
    OpGetVar,
    OpSetVar,

    // Lvalue reference pushing for BYREF calls
    OpPushRefVar,
    OpPushRefArray1D,
    OpPushRefArray2D,
    OpPushRefField,

    // Array operations
    OpGetArray1D,
    OpSetArray1D,
    OpGetArray2D,
    OpSetArray2D,

    // Record operations
    OpGetField,
    OpSetField,

    // Procedure and Function calls
    OpCall,
    OpReturn,
    OpReturnVal,

    // OOP operations
    OpNewObject,
    OpInvokeMethod,
    OpSuperCall,

    // File I/O
    OpOpenFile,
    OpCloseFile,
    OpReadFile,
    OpWriteFile,
    OpEof,

    // Arithmetic
    OpAdd,
    OpSub,
    OpMul,
    OpDivReal,
    OpDivInt,
    OpMod,
    OpNeg,

    // Strings & built-ins
    OpConcat,
    OpLength,
    OpSubstring,
    OpUCase,
    OpLCase,
    OpNumToStr,
    OpStrToNum,
    OpLeft,
    OpRight,
    OpChr,
    OpAsc,
    OpInt,
    OpRound,
    OpRnd,

    // Comparisons
    OpEqual,
    OpNotEqual,
    OpLess,
    OpLessEqual,
    OpGreater,
    OpGreaterEqual,

    // Logic
    OpNot,

    // Jumps & Control flow
    OpJump,
    OpJumpIfFalse,
    OpJumpIfFalseOrPop,
    OpJumpIfTrueOrPop,
    OpCheckStep,
    OpForCheck,
    OpForStep,

    // I/O
    OpPrint,
    OpPrintLn,
    OpReadInt,
    OpReadReal,
    OpReadBool,
    OpReadStr,

    // Program termination
    OpHalt
};

struct Instruction {
    OpCode op;
    int32_t a = 0;
    int32_t b = 0;
    int32_t c = 0;
    int32_t d = 0;
    int line = 0;
};

struct FunctionInfo {
    std::string name;
    bool isFunction = false;
    size_t entryIp = 0;
    int numParams = 0;
    int numLocals = 0;
    std::vector<bool> paramIsByRef;
    TypeInfo returnType;
    std::vector<std::pair<std::string, TypeInfo>> localVars;
};

struct Chunk {
    std::vector<Instruction> code;
    std::vector<Value> constants;
    std::vector<std::pair<std::string, TypeInfo>> varDescs;
    std::unordered_map<std::string, FunctionInfo> functions;
    std::unordered_map<std::string, RecordDef> recordTypes;
    std::unordered_map<std::string, Sema::ClassInfo> classTypes;
};

const char* opCodeName(OpCode op);
void printInstruction(std::ostream& os, const Chunk& chunk, size_t ip);
void dumpBytecode(const Chunk& chunk, const std::string& name, std::ostream& os = std::cout);

static const inline std::unordered_map<std::string, Sema::ClassInfo> kEmptyClasses{};

class BytecodeCompiler {
public:
    BytecodeCompiler(const std::vector<std::pair<std::string, TypeInfo>>& vars,
                     const std::unordered_map<std::string, RecordDef>& records,
                     const std::unordered_map<std::string, Sema::FunctionSig>& funcs,
                     const std::unordered_map<std::string, Sema::ClassInfo>& classes = kEmptyClasses);

    Chunk compile(const Block& program);

private:
    std::vector<std::pair<std::string, TypeInfo>> allVars_;
    std::unordered_map<std::string, int> slotMap_;
    std::unordered_map<std::string, TypeInfo> typeMap_;
    const std::unordered_map<std::string, RecordDef>& recordTypes_;
    const std::unordered_map<std::string, Sema::FunctionSig>& functions_;
    const std::unordered_map<std::string, Sema::ClassInfo>& classTypes_;
    std::string currentClassName_ = "";
    Chunk chunk_;

    // Function/Procedure compilation state
    bool insideFunction_ = false;
    std::vector<std::pair<std::string, TypeInfo>> localVars_;
    std::unordered_map<std::string, int> localSlotMap_;
    std::unordered_map<std::string, TypeInfo> localTypeMap_;
    std::unordered_map<std::string, bool> localIsByRef_;

    static int encodeSlot(int slot, bool isLocal) {
        return isLocal ? -(slot + 1) : slot;
    }
    int getVarSlot(const std::string& name) const;
    TypeInfo getVarType(const std::string& name) const;
    bool isClassProperty(const std::string& name) const;

    int addConstant(Value v);
    int emit(OpCode op, int32_t a = 0, int32_t b = 0, int32_t c = 0, int32_t d = 0, int line = 0);
    int emitJump(OpCode op, int line);
    void patchJump(int jumpInst);
    int allocateTempVar(BaseType base);

    void compileBlock(const Block& block);
    void compileStmt(const Stmt& s);
    void compileExpr(const Expr& e);
    void compileLValueRef(const Expr& e);
    void compileCase(const CaseStmt& c);
};

#endif // PSEUDOC_BYTECODE_H
