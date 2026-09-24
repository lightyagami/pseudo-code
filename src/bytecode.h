#ifndef PSEUDOC_BYTECODE_H
#define PSEUDOC_BYTECODE_H

#include "ast.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct ArrayData;

enum class ValueKind { Nil, Int, Real, Bool, String, Array };

struct Value {
    ValueKind kind = ValueKind::Nil;
    int64_t intVal = 0;
    double realVal = 0.0;
    bool boolVal = false;
    std::string strVal;
    std::shared_ptr<ArrayData> arrVal;

    static Value makeNil() { return Value(); }
    static Value makeInt(int64_t v) { Value val; val.kind = ValueKind::Int; val.intVal = v; return val; }
    static Value makeReal(double v) { Value val; val.kind = ValueKind::Real; val.realVal = v; return val; }
    static Value makeBool(bool v) { Value val; val.kind = ValueKind::Bool; val.boolVal = v; return val; }
    static Value makeString(std::string v) { Value val; val.kind = ValueKind::String; val.strVal = std::move(v); return val; }
    static Value makeArray(std::shared_ptr<ArrayData> v) { Value val; val.kind = ValueKind::Array; val.arrVal = std::move(v); return val; }

    bool isNil() const { return kind == ValueKind::Nil; }
    bool isInt() const { return kind == ValueKind::Int; }
    bool isReal() const { return kind == ValueKind::Real; }
    bool isBool() const { return kind == ValueKind::Bool; }
    bool isString() const { return kind == ValueKind::String; }
    bool isArray() const { return kind == ValueKind::Array; }

    int64_t asInt() const { return intVal; }
    double asReal() const { return isInt() ? static_cast<double>(intVal) : realVal; }
    bool asBool() const { return boolVal; }
    const std::string& asString() const { return strVal; }

    void print(std::ostream& os) const;
};

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

enum class OpCode : uint8_t {
    OpConstant,
    OpPop,
    OpDup,
    OpWidenReal,

    // Global variable access
    OpGetGlobal,
    OpSetGlobal,

    // Array operations
    OpGetArray1D,
    OpSetArray1D,
    OpGetArray2D,
    OpSetArray2D,

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

struct Chunk {
    std::vector<Instruction> code;
    std::vector<Value> constants;
    std::vector<std::pair<std::string, TypeInfo>> varDescs;
};

const char* opCodeName(OpCode op);
void printInstruction(std::ostream& os, const Chunk& chunk, size_t ip);
void dumpBytecode(const Chunk& chunk, const std::string& name, std::ostream& os = std::cout);

class BytecodeCompiler {
public:
    explicit BytecodeCompiler(const std::vector<std::pair<std::string, TypeInfo>>& vars);

    Chunk compile(const Block& program);

private:
    std::vector<std::pair<std::string, TypeInfo>> allVars_;
    std::unordered_map<std::string, int> slotMap_;
    std::unordered_map<std::string, TypeInfo> typeMap_;
    Chunk chunk_;

    int addConstant(Value v);
    int emit(OpCode op, int32_t a = 0, int32_t b = 0, int32_t c = 0, int32_t d = 0, int line = 0);
    int emitJump(OpCode op, int line);
    void patchJump(int jumpInst);
    int allocateTempVar(BaseType base);

    void compileBlock(const Block& block);
    void compileStmt(const Stmt& s);
    void compileExpr(const Expr& e);
};

#endif // PSEUDOC_BYTECODE_H
