#include "bytecode.h"

#include <cstdio>
#include <iomanip>

void Value::print(std::ostream& os) const {
    switch (kind) {
        case ValueKind::Int:
            os << intVal;
            break;
        case ValueKind::Real: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.10g", realVal);
            os << buf;
            break;
        }
        case ValueKind::Bool:
            os << (boolVal ? "TRUE" : "FALSE");
            break;
        case ValueKind::String:
            os << strVal;
            break;
        case ValueKind::Array:
            os << "<array>";
            break;
        case ValueKind::Nil:
            os << "<nil>";
            break;
    }
}

const char* opCodeName(OpCode op) {
    switch (op) {
        case OpCode::OpConstant:           return "OP_CONSTANT";
        case OpCode::OpPop:                return "OP_POP";
        case OpCode::OpDup:                return "OP_DUP";
        case OpCode::OpWidenReal:          return "OP_WIDEN_REAL";
        case OpCode::OpGetGlobal:          return "OP_GET_GLOBAL";
        case OpCode::OpSetGlobal:          return "OP_SET_GLOBAL";
        case OpCode::OpGetArray1D:         return "OP_GET_ARRAY_1D";
        case OpCode::OpSetArray1D:         return "OP_SET_ARRAY_1D";
        case OpCode::OpGetArray2D:         return "OP_GET_ARRAY_2D";
        case OpCode::OpSetArray2D:         return "OP_SET_ARRAY_2D";
        case OpCode::OpAdd:                return "OP_ADD";
        case OpCode::OpSub:                return "OP_SUB";
        case OpCode::OpMul:                return "OP_MUL";
        case OpCode::OpDivReal:            return "OP_DIV_REAL";
        case OpCode::OpDivInt:             return "OP_DIV_INT";
        case OpCode::OpMod:                return "OP_MOD";
        case OpCode::OpNeg:                return "OP_NEG";
        case OpCode::OpConcat:             return "OP_CONCAT";
        case OpCode::OpLength:             return "OP_LENGTH";
        case OpCode::OpSubstring:          return "OP_SUBSTRING";
        case OpCode::OpUCase:              return "OP_UCASE";
        case OpCode::OpLCase:              return "OP_LCASE";
        case OpCode::OpNumToStr:           return "OP_NUM_TO_STR";
        case OpCode::OpStrToNum:           return "OP_STR_TO_NUM";
        case OpCode::OpEqual:              return "OP_EQUAL";
        case OpCode::OpNotEqual:           return "OP_NOT_EQUAL";
        case OpCode::OpLess:               return "OP_LESS";
        case OpCode::OpLessEqual:          return "OP_LESS_EQUAL";
        case OpCode::OpGreater:            return "OP_GREATER";
        case OpCode::OpGreaterEqual:       return "OP_GREATER_EQUAL";
        case OpCode::OpNot:                return "OP_NOT";
        case OpCode::OpJump:               return "OP_JUMP";
        case OpCode::OpJumpIfFalse:        return "OP_JUMP_IF_FALSE";
        case OpCode::OpJumpIfFalseOrPop:   return "OP_JUMP_IF_FALSE_OR_POP";
        case OpCode::OpJumpIfTrueOrPop:    return "OP_JUMP_IF_TRUE_OR_POP";
        case OpCode::OpCheckStep:          return "OP_CHECK_STEP";
        case OpCode::OpForCheck:           return "OP_FOR_CHECK";
        case OpCode::OpForStep:            return "OP_FOR_STEP";
        case OpCode::OpPrint:              return "OP_PRINT";
        case OpCode::OpPrintLn:            return "OP_PRINT_LN";
        case OpCode::OpReadInt:            return "OP_READ_INT";
        case OpCode::OpReadReal:           return "OP_READ_REAL";
        case OpCode::OpReadBool:           return "OP_READ_BOOL";
        case OpCode::OpReadStr:            return "OP_READ_STR";
        case OpCode::OpHalt:               return "OP_HALT";
    }
    return "OP_UNKNOWN";
}

void printInstruction(std::ostream& os, const Chunk& chunk, size_t ip) {
    const Instruction& inst = chunk.code[ip];
    char buf[128];
    snprintf(buf, sizeof(buf), "%04zu   %4d  ", ip, inst.line);
    os << buf;
    os << std::left << std::setw(25) << opCodeName(inst.op);
    switch (inst.op) {
        case OpCode::OpConstant:
            os << " #" << inst.a << " (";
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.constants.size())) {
                if (chunk.constants[inst.a].isString()) os << "\"" << chunk.constants[inst.a].asString() << "\"";
                else chunk.constants[inst.a].print(os);
            }
            os << ")";
            break;
        case OpCode::OpGetGlobal:
        case OpCode::OpSetGlobal:
            os << " slot:" << inst.a;
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.varDescs.size()))
                os << " (" << chunk.varDescs[inst.a].first << ")";
            break;
        case OpCode::OpGetArray1D:
        case OpCode::OpSetArray1D:
        case OpCode::OpGetArray2D:
        case OpCode::OpSetArray2D:
            os << " slot:" << inst.a;
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.varDescs.size()))
                os << " (" << chunk.varDescs[inst.a].first << ")";
            break;
        case OpCode::OpJump:
        case OpCode::OpJumpIfFalse:
        case OpCode::OpJumpIfFalseOrPop:
        case OpCode::OpJumpIfTrueOrPop:
            os << " -> " << inst.a;
            break;
        case OpCode::OpCheckStep:
            os << " slot:" << inst.a;
            break;
        case OpCode::OpForCheck:
            os << " var:" << inst.a << " end:" << inst.b << " step:" << inst.c << " exit->" << inst.d;
            break;
        case OpCode::OpForStep:
            os << " var:" << inst.a << " step:" << inst.b;
            break;
        default:
            break;
    }
    os << "\n";
}

void dumpBytecode(const Chunk& chunk, const std::string& name, std::ostream& os) {
    os << "== Disassembly: " << name << " ==\n";
    os << "Constants (" << chunk.constants.size() << "):\n";
    for (size_t i = 0; i < chunk.constants.size(); ++i) {
        os << "  #" << i << ": ";
        if (chunk.constants[i].isString()) os << "\"" << chunk.constants[i].asString() << "\"\n";
        else { chunk.constants[i].print(os); os << "\n"; }
    }
    os << "Variables (" << chunk.varDescs.size() << "):\n";
    for (size_t i = 0; i < chunk.varDescs.size(); ++i) {
        os << "  slot " << i << ": " << chunk.varDescs[i].first << " (" << typeString(chunk.varDescs[i].second) << ")\n";
    }
    os << "Instructions (" << chunk.code.size() << "):\n";
    os << "IP     LINE  OPCODE                    OPERANDS\n";
    os << "--------------------------------------------------------\n";
    for (size_t ip = 0; ip < chunk.code.size(); ++ip) {
        printInstruction(os, chunk, ip);
    }
    os << "========================================================\n";
}

BytecodeCompiler::BytecodeCompiler(const std::vector<std::pair<std::string, TypeInfo>>& vars)
    : allVars_(vars) {
    for (size_t i = 0; i < vars.size(); ++i) {
        slotMap_[vars[i].first] = static_cast<int>(i);
        typeMap_[vars[i].first] = vars[i].second;
    }
}

Chunk BytecodeCompiler::compile(const Block& program) {
    for (const auto& stmt : program) {
        compileStmt(*stmt);
    }
    emit(OpCode::OpHalt, 0, 0, 0, 0, 0);
    chunk_.varDescs = allVars_;
    return std::move(chunk_);
}

int BytecodeCompiler::addConstant(Value v) {
    for (size_t i = 0; i < chunk_.constants.size(); ++i) {
        const auto& c = chunk_.constants[i];
        if (c.kind == v.kind) {
            if (c.isInt() && c.asInt() == v.asInt()) return static_cast<int>(i);
            if (c.isReal() && c.asReal() == v.asReal()) return static_cast<int>(i);
            if (c.isBool() && c.asBool() == v.asBool()) return static_cast<int>(i);
            if (c.isString() && c.asString() == v.asString()) return static_cast<int>(i);
        }
    }
    chunk_.constants.push_back(std::move(v));
    return static_cast<int>(chunk_.constants.size() - 1);
}

int BytecodeCompiler::emit(OpCode op, int32_t a, int32_t b, int32_t c, int32_t d, int line) {
    chunk_.code.push_back({op, a, b, c, d, line});
    return static_cast<int>(chunk_.code.size() - 1);
}

int BytecodeCompiler::emitJump(OpCode op, int line) {
    return emit(op, -1, 0, 0, 0, line);
}

void BytecodeCompiler::patchJump(int jumpInst) {
    chunk_.code[jumpInst].a = static_cast<int32_t>(chunk_.code.size());
}

int BytecodeCompiler::allocateTempVar(BaseType base) {
    int slot = static_cast<int>(allVars_.size());
    std::string name = "$tmp_" + std::to_string(slot);
    TypeInfo t{base, false, 0, 0, 0, 0, 0};
    allVars_.emplace_back(name, t);
    slotMap_[name] = slot;
    typeMap_[name] = t;
    return slot;
}

void BytecodeCompiler::compileBlock(const Block& block) {
    for (const auto& s : block) compileStmt(*s);
}

void BytecodeCompiler::compileStmt(const Stmt& s) {
    switch (s.kind) {
        case Stmt::Kind::Declare:
            break;
        case Stmt::Kind::Assign: {
            auto& a = static_cast<const AssignStmt&>(s);
            compileExpr(*a.value);
            if (typeMap_.at(a.name).base == BaseType::Real && a.value->type.base == BaseType::Integer) {
                emit(OpCode::OpWidenReal, 0, 0, 0, 0, a.line);
            }
            emit(OpCode::OpSetGlobal, slotMap_.at(a.name), 0, 0, 0, a.line);
            break;
        }
        case Stmt::Kind::ArrayAssign: {
            auto& a = static_cast<const ArrayAssignStmt&>(s);
            compileExpr(*a.value);
            if (typeMap_.at(a.name).base == BaseType::Real && a.value->type.base == BaseType::Integer) {
                emit(OpCode::OpWidenReal, 0, 0, 0, 0, a.line);
            }
            for (auto& idx : a.indices) {
                compileExpr(*idx);
            }
            int slot = slotMap_.at(a.name);
            int nameConst = addConstant(Value::makeString(a.name));
            if (a.indices.size() == 1) {
                emit(OpCode::OpSetArray1D, slot, nameConst, 0, 0, a.line);
            } else {
                emit(OpCode::OpSetArray2D, slot, nameConst, 0, 0, a.line);
            }
            break;
        }
        case Stmt::Kind::Output: {
            auto& o = static_cast<const OutputStmt&>(s);
            for (const auto& arg : o.args) {
                compileExpr(*arg);
                emit(OpCode::OpPrint, 0, 0, 0, 0, arg->line);
            }
            emit(OpCode::OpPrintLn, 0, 0, 0, 0, o.line);
            break;
        }
        case Stmt::Kind::Input: {
            auto& in = static_cast<const InputStmt&>(s);
            int slot = slotMap_.at(in.name);
            BaseType b = typeMap_.at(in.name).base;
            OpCode readOp = OpCode::OpReadInt;
            switch (b) {
                case BaseType::Integer: readOp = OpCode::OpReadInt; break;
                case BaseType::Real:    readOp = OpCode::OpReadReal; break;
                case BaseType::Boolean: readOp = OpCode::OpReadBool; break;
                case BaseType::String:  readOp = OpCode::OpReadStr; break;
                default: break;
            }
            if (in.indices.empty()) {
                emit(readOp, 0, 0, 0, 0, in.line);
                emit(OpCode::OpSetGlobal, slot, 0, 0, 0, in.line);
            } else {
                emit(readOp, 0, 0, 0, 0, in.line);
                for (auto& idx : in.indices) {
                    compileExpr(*idx);
                }
                int nameConst = addConstant(Value::makeString(in.name));
                if (in.indices.size() == 1) {
                    emit(OpCode::OpSetArray1D, slot, nameConst, 0, 0, in.line);
                } else {
                    emit(OpCode::OpSetArray2D, slot, nameConst, 0, 0, in.line);
                }
            }
            break;
        }
        case Stmt::Kind::If: {
            auto& i = static_cast<const IfStmt&>(s);
            compileExpr(*i.cond);
            int elseJump = emitJump(OpCode::OpJumpIfFalse, i.line);
            compileBlock(i.thenBlock);
            if (!i.elseBlock.empty()) {
                int exitJump = emitJump(OpCode::OpJump, i.line);
                patchJump(elseJump);
                compileBlock(i.elseBlock);
                patchJump(exitJump);
            } else {
                patchJump(elseJump);
            }
            break;
        }
        case Stmt::Kind::While: {
            auto& w = static_cast<const WhileStmt&>(s);
            int loopStart = static_cast<int>(chunk_.code.size());
            compileExpr(*w.cond);
            int exitJump = emitJump(OpCode::OpJumpIfFalse, w.line);
            compileBlock(w.body);
            emit(OpCode::OpJump, loopStart, 0, 0, 0, w.line);
            patchJump(exitJump);
            break;
        }
        case Stmt::Kind::For: {
            auto& f = static_cast<const ForStmt&>(s);
            int varSlot = slotMap_.at(f.var);
            int endSlot = allocateTempVar(BaseType::Integer);
            int stepSlot = allocateTempVar(BaseType::Integer);

            compileExpr(*f.start);
            emit(OpCode::OpSetGlobal, varSlot, 0, 0, 0, f.line);

            compileExpr(*f.end);
            emit(OpCode::OpSetGlobal, endSlot, 0, 0, 0, f.line);

            if (f.step) {
                compileExpr(*f.step);
                emit(OpCode::OpSetGlobal, stepSlot, 0, 0, 0, f.step->line);
            } else {
                int oneConst = addConstant(Value::makeInt(1));
                emit(OpCode::OpConstant, oneConst, 0, 0, 0, f.line);
                emit(OpCode::OpSetGlobal, stepSlot, 0, 0, 0, f.line);
            }

            emit(OpCode::OpCheckStep, stepSlot, 0, 0, 0, f.line);

            int loopStart = static_cast<int>(chunk_.code.size());
            int forCheckInst = emit(OpCode::OpForCheck, varSlot, endSlot, stepSlot, -1, f.line);

            compileBlock(f.body);

            emit(OpCode::OpForStep, varSlot, stepSlot, 0, 0, f.line);
            emit(OpCode::OpJump, loopStart, 0, 0, 0, f.line);

            chunk_.code[forCheckInst].d = static_cast<int32_t>(chunk_.code.size());
            break;
        }
    }
}

void BytecodeCompiler::compileExpr(const Expr& e) {
    switch (e.kind) {
        case Expr::Kind::Literal: {
            auto& lit = static_cast<const LiteralExpr&>(e);
            int cIdx = 0;
            switch (lit.litType) {
                case Tok::IntLit:
                    cIdx = addConstant(Value::makeInt(std::stoll(lit.text)));
                    break;
                case Tok::RealLit:
                    cIdx = addConstant(Value::makeReal(std::stod(lit.text)));
                    break;
                case Tok::StrLit:
                    cIdx = addConstant(Value::makeString(lit.text));
                    break;
                case Tok::True:
                    cIdx = addConstant(Value::makeBool(true));
                    break;
                case Tok::False:
                    cIdx = addConstant(Value::makeBool(false));
                    break;
                default:
                    break;
            }
            emit(OpCode::OpConstant, cIdx, 0, 0, 0, lit.line);
            break;
        }
        case Expr::Kind::Var: {
            auto& v = static_cast<const VarExpr&>(e);
            emit(OpCode::OpGetGlobal, slotMap_.at(v.name), 0, 0, 0, v.line);
            break;
        }
        case Expr::Kind::ArrayAccess: {
            auto& a = static_cast<const ArrayAccessExpr&>(e);
            for (auto& idx : a.indices) {
                compileExpr(*idx);
            }
            int slot = slotMap_.at(a.name);
            int nameConst = addConstant(Value::makeString(a.name));
            if (a.indices.size() == 1) {
                emit(OpCode::OpGetArray1D, slot, nameConst, 0, 0, a.line);
            } else {
                emit(OpCode::OpGetArray2D, slot, nameConst, 0, 0, a.line);
            }
            break;
        }
        case Expr::Kind::Unary: {
            auto& u = static_cast<const UnaryExpr&>(e);
            compileExpr(*u.operand);
            if (u.op == Tok::Minus) {
                emit(OpCode::OpNeg, 0, 0, 0, 0, u.line);
            } else if (u.op == Tok::Not) {
                emit(OpCode::OpNot, 0, 0, 0, 0, u.line);
            }
            break;
        }
        case Expr::Kind::Binary: {
            auto& b = static_cast<const BinaryExpr&>(e);
            if (b.op == Tok::And) {
                compileExpr(*b.lhs);
                int jump = emitJump(OpCode::OpJumpIfFalseOrPop, b.line);
                compileExpr(*b.rhs);
                patchJump(jump);
                return;
            }
            if (b.op == Tok::Or) {
                compileExpr(*b.lhs);
                int jump = emitJump(OpCode::OpJumpIfTrueOrPop, b.line);
                compileExpr(*b.rhs);
                patchJump(jump);
                return;
            }
            compileExpr(*b.lhs);
            compileExpr(*b.rhs);
            switch (b.op) {
                case Tok::Plus:      emit(OpCode::OpAdd, 0, 0, 0, 0, b.line); break;
                case Tok::Minus:     emit(OpCode::OpSub, 0, 0, 0, 0, b.line); break;
                case Tok::Star:      emit(OpCode::OpMul, 0, 0, 0, 0, b.line); break;
                case Tok::Slash:     emit(OpCode::OpDivReal, 0, 0, 0, 0, b.line); break;
                case Tok::Div:       emit(OpCode::OpDivInt, 0, 0, 0, 0, b.line); break;
                case Tok::Mod:       emit(OpCode::OpMod, 0, 0, 0, 0, b.line); break;
                case Tok::Ampersand: emit(OpCode::OpConcat, 0, 0, 0, 0, b.line); break;
                case Tok::Eq:        emit(OpCode::OpEqual, 0, 0, 0, 0, b.line); break;
                case Tok::Neq:       emit(OpCode::OpNotEqual, 0, 0, 0, 0, b.line); break;
                case Tok::Lt:        emit(OpCode::OpLess, 0, 0, 0, 0, b.line); break;
                case Tok::Le:        emit(OpCode::OpLessEqual, 0, 0, 0, 0, b.line); break;
                case Tok::Gt:        emit(OpCode::OpGreater, 0, 0, 0, 0, b.line); break;
                case Tok::Ge:        emit(OpCode::OpGreaterEqual, 0, 0, 0, 0, b.line); break;
                default: break;
            }
            break;
        }
        case Expr::Kind::Call: {
            auto& c = static_cast<const CallExpr&>(e);
            for (auto& arg : c.args) {
                compileExpr(*arg);
            }
            switch (c.func) {
                case Tok::Length:    emit(OpCode::OpLength, 0, 0, 0, 0, c.line); break;
                case Tok::Substring: emit(OpCode::OpSubstring, 0, 0, 0, 0, c.line); break;
                case Tok::UCase:     emit(OpCode::OpUCase, 0, 0, 0, 0, c.line); break;
                case Tok::LCase:     emit(OpCode::OpLCase, 0, 0, 0, 0, c.line); break;
                case Tok::NumToStr:  emit(OpCode::OpNumToStr, 0, 0, 0, 0, c.line); break;
                case Tok::StrToNum:  emit(OpCode::OpStrToNum, 0, 0, 0, 0, c.line); break;
                default: break;
            }
            break;
        }
    }
}
