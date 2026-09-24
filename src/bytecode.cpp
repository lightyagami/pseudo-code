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
        case ValueKind::Record:
            os << "<record " << (recVal ? recVal->typeName : "") << ">";
            break;
        case ValueKind::Ref:
            os << "<ref>";
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
        case OpCode::OpGetVar:             return "OP_GET_VAR";
        case OpCode::OpSetVar:             return "OP_SET_VAR";
        case OpCode::OpPushRefVar:         return "OP_PUSH_REF_VAR";
        case OpCode::OpPushRefArray1D:     return "OP_PUSH_REF_ARRAY_1D";
        case OpCode::OpPushRefArray2D:     return "OP_PUSH_REF_ARRAY_2D";
        case OpCode::OpPushRefField:       return "OP_PUSH_REF_FIELD";
        case OpCode::OpGetArray1D:         return "OP_GET_ARRAY_1D";
        case OpCode::OpSetArray1D:         return "OP_SET_ARRAY_1D";
        case OpCode::OpGetArray2D:         return "OP_GET_ARRAY_2D";
        case OpCode::OpSetArray2D:         return "OP_SET_ARRAY_2D";
        case OpCode::OpGetField:           return "OP_GET_FIELD";
        case OpCode::OpSetField:           return "OP_SET_FIELD";
        case OpCode::OpCall:               return "OP_CALL";
        case OpCode::OpReturn:             return "OP_RETURN";
        case OpCode::OpReturnVal:          return "OP_RETURN_VAL";
        case OpCode::OpOpenFile:           return "OP_OPEN_FILE";
        case OpCode::OpCloseFile:          return "OP_CLOSE_FILE";
        case OpCode::OpReadFile:           return "OP_READ_FILE";
        case OpCode::OpWriteFile:          return "OP_WRITE_FILE";
        case OpCode::OpEof:                return "OP_EOF";
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
        case OpCode::OpLeft:               return "OP_LEFT";
        case OpCode::OpRight:              return "OP_RIGHT";
        case OpCode::OpChr:                return "OP_CHR";
        case OpCode::OpAsc:                return "OP_ASC";
        case OpCode::OpInt:                return "OP_INT";
        case OpCode::OpRound:              return "OP_ROUND";
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
        case OpCode::OpGetVar:
        case OpCode::OpSetVar:
        case OpCode::OpPushRefVar:
            if (inst.a < 0) os << " local:" << (-inst.a - 1);
            else os << " global:" << inst.a;
            break;
        case OpCode::OpGetArray1D:
        case OpCode::OpSetArray1D:
        case OpCode::OpGetArray2D:
        case OpCode::OpSetArray2D:
        case OpCode::OpPushRefArray1D:
        case OpCode::OpPushRefArray2D:
            if (inst.a < 0) os << " local:" << (-inst.a - 1);
            else os << " global:" << inst.a;
            if (inst.b >= 0 && inst.b < static_cast<int>(chunk.constants.size()))
                os << " (" << chunk.constants[inst.b].asString() << ")";
            break;
        case OpCode::OpGetField:
        case OpCode::OpSetField:
        case OpCode::OpPushRefField:
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.constants.size()))
                os << " ." << chunk.constants[inst.a].asString();
            break;
        case OpCode::OpCall:
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.constants.size()))
                os << " " << chunk.constants[inst.a].asString();
            os << " args:" << inst.b << (inst.c ? " [returns val]" : " [void]");
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
        case OpCode::OpOpenFile:
            if (inst.a >= 0 && inst.a < static_cast<int>(chunk.constants.size()))
                os << " mode:" << chunk.constants[inst.a].asString();
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
    os << "Globals (" << chunk.varDescs.size() << "):\n";
    for (size_t i = 0; i < chunk.varDescs.size(); ++i) {
        os << "  slot " << i << ": " << chunk.varDescs[i].first << " (" << typeString(chunk.varDescs[i].second) << ")\n";
    }
    if (!chunk.functions.empty()) {
        os << "Functions (" << chunk.functions.size() << "):\n";
        for (const auto& [fname, fi] : chunk.functions) {
            os << "  " << (fi.isFunction ? "FUNCTION " : "PROCEDURE ") << fname
               << " @ IP " << fi.entryIp << " (params: " << fi.numParams
               << ", locals: " << fi.numLocals << ")\n";
        }
    }
    os << "Instructions (" << chunk.code.size() << "):\n";
    os << "IP     LINE  OPCODE                    OPERANDS\n";
    os << "--------------------------------------------------------\n";
    for (size_t ip = 0; ip < chunk.code.size(); ++ip) {
        printInstruction(os, chunk, ip);
    }
    os << "========================================================\n";
}

BytecodeCompiler::BytecodeCompiler(const std::vector<std::pair<std::string, TypeInfo>>& vars,
                                   const std::unordered_map<std::string, RecordDef>& records,
                                   const std::unordered_map<std::string, Sema::FunctionSig>& funcs)
    : allVars_(vars), recordTypes_(records), functions_(funcs) {
    for (size_t i = 0; i < vars.size(); ++i) {
        slotMap_[vars[i].first] = static_cast<int>(i);
        typeMap_[vars[i].first] = vars[i].second;
    }
}

int BytecodeCompiler::getVarSlot(const std::string& name) const {
    if (insideFunction_) {
        auto it = localSlotMap_.find(name);
        if (it != localSlotMap_.end()) return encodeSlot(it->second, true);
    }
    return encodeSlot(slotMap_.at(name), false);
}

TypeInfo BytecodeCompiler::getVarType(const std::string& name) const {
    if (insideFunction_) {
        auto it = localTypeMap_.find(name);
        if (it != localTypeMap_.end()) return it->second;
    }
    return typeMap_.at(name);
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
    TypeInfo t{base, "", false, 0, 0, 0, 0, 0};
    if (insideFunction_) {
        int slot = static_cast<int>(localVars_.size());
        std::string name = "$tmp_" + std::to_string(slot);
        localVars_.emplace_back(name, t);
        localSlotMap_[name] = slot;
        localTypeMap_[name] = t;
        localIsByRef_[name] = false;
        return encodeSlot(slot, true);
    } else {
        int slot = static_cast<int>(allVars_.size());
        std::string name = "$tmp_" + std::to_string(slot);
        allVars_.emplace_back(name, t);
        slotMap_[name] = slot;
        typeMap_[name] = t;
        return encodeSlot(slot, false);
    }
}

Chunk BytecodeCompiler::compile(const Block& program) {
    chunk_.recordTypes = recordTypes_;

    // Emit initial jump to bypass procedures and functions
    int jumpToMain = emitJump(OpCode::OpJump, 0);

    // Compile procedure and function declarations first
    for (const auto& stmt : program) {
        if (stmt->kind == Stmt::Kind::ProcedureDecl) {
            auto& p = static_cast<const ProcedureDeclStmt&>(*stmt);
            FunctionInfo fi;
            fi.name = p.name;
            fi.isFunction = false;
            fi.entryIp = chunk_.code.size();
            fi.numParams = static_cast<int>(p.params.size());

            insideFunction_ = true;
            localVars_.clear();
            localSlotMap_.clear();
            localTypeMap_.clear();
            localIsByRef_.clear();

            for (const auto& param : p.params) {
                int slot = static_cast<int>(localVars_.size());
                localVars_.emplace_back(param.name, param.type);
                localSlotMap_[param.name] = slot;
                localTypeMap_[param.name] = param.type;
                localIsByRef_[param.name] = param.isByRef;
                fi.paramIsByRef.push_back(param.isByRef);
            }

            compileBlock(p.body);
            emit(OpCode::OpReturn, 0, 0, 0, 0, p.line);

            fi.numLocals = static_cast<int>(localVars_.size());
            fi.localVars = localVars_;
            chunk_.functions[p.name] = fi;

            insideFunction_ = false;
        } else if (stmt->kind == Stmt::Kind::FunctionDecl) {
            auto& f = static_cast<const FunctionDeclStmt&>(*stmt);
            FunctionInfo fi;
            fi.name = f.name;
            fi.isFunction = true;
            fi.entryIp = chunk_.code.size();
            fi.numParams = static_cast<int>(f.params.size());
            fi.returnType = f.returnType;

            insideFunction_ = true;
            localVars_.clear();
            localSlotMap_.clear();
            localTypeMap_.clear();
            localIsByRef_.clear();

            for (const auto& param : f.params) {
                int slot = static_cast<int>(localVars_.size());
                localVars_.emplace_back(param.name, param.type);
                localSlotMap_[param.name] = slot;
                localTypeMap_[param.name] = param.type;
                localIsByRef_[param.name] = param.isByRef;
                fi.paramIsByRef.push_back(param.isByRef);
            }

            compileBlock(f.body);
            // Default return value in case control falls through
            int defConst = 0;
            switch (f.returnType.base) {
                case BaseType::Integer: defConst = addConstant(Value::makeInt(0)); break;
                case BaseType::Real:    defConst = addConstant(Value::makeReal(0.0)); break;
                case BaseType::Boolean: defConst = addConstant(Value::makeBool(false)); break;
                case BaseType::String:
                case BaseType::Char:    defConst = addConstant(Value::makeString("")); break;
                default: break;
            }
            emit(OpCode::OpConstant, defConst, 0, 0, 0, f.line);
            emit(OpCode::OpReturnVal, 0, 0, 0, 0, f.line);

            fi.numLocals = static_cast<int>(localVars_.size());
            fi.localVars = localVars_;
            chunk_.functions[f.name] = fi;

            insideFunction_ = false;
        }
    }

    patchJump(jumpToMain);

    // Compile main script statements
    for (const auto& stmt : program) {
        if (stmt->kind != Stmt::Kind::TypeDecl &&
            stmt->kind != Stmt::Kind::ProcedureDecl &&
            stmt->kind != Stmt::Kind::FunctionDecl) {
            compileStmt(*stmt);
        }
    }

    emit(OpCode::OpHalt, 0, 0, 0, 0, 0);
    chunk_.varDescs = allVars_;
    return std::move(chunk_);
}

void BytecodeCompiler::compileBlock(const Block& block) {
    for (const auto& s : block) compileStmt(*s);
}

void BytecodeCompiler::compileLValueRef(const Expr& e) {
    switch (e.kind) {
        case Expr::Kind::Var: {
            auto& v = static_cast<const VarExpr&>(e);
            int enc = getVarSlot(v.name);
            emit(OpCode::OpPushRefVar, enc, 0, 0, 0, v.line);
            break;
        }
        case Expr::Kind::ArrayAccess: {
            auto& a = static_cast<const ArrayAccessExpr&>(e);
            std::string arrName = a.name;
            if (arrName.empty() && a.target && a.target->kind == Expr::Kind::Var) {
                arrName = static_cast<const VarExpr&>(*a.target).name;
            }
            for (auto& idx : a.indices) {
                compileExpr(*idx);
            }
            int enc = getVarSlot(arrName);
            int nameConst = addConstant(Value::makeString(arrName));
            if (a.indices.size() == 1) {
                emit(OpCode::OpPushRefArray1D, enc, nameConst, 0, 0, a.line);
            } else {
                emit(OpCode::OpPushRefArray2D, enc, nameConst, 0, 0, a.line);
            }
            break;
        }
        case Expr::Kind::MemberAccess: {
            auto& m = static_cast<const MemberAccessExpr&>(e);
            compileExpr(*m.target);
            int fieldConst = addConstant(Value::makeString(m.field));
            emit(OpCode::OpPushRefField, fieldConst, 0, 0, 0, m.line);
            break;
        }
        default:
            break;
    }
}

void BytecodeCompiler::compileCase(const CaseStmt& c) {
    compileExpr(*c.selector);
    int encSel = allocateTempVar(c.selector->type.base);
    emit(OpCode::OpSetVar, encSel, 0, 0, 0, c.line);

    std::vector<int> exitJumps;

    for (const auto& branch : c.branches) {
        if (branch.values.empty()) continue;

        // sel == v0 || sel == v1 || ...
        emit(OpCode::OpGetVar, encSel, 0, 0, 0, branch.line);
        compileExpr(*branch.values[0]);
        emit(OpCode::OpEqual, 0, 0, 0, 0, branch.line);

        std::vector<int> orJumps;
        for (size_t i = 1; i < branch.values.size(); ++i) {
            orJumps.push_back(emitJump(OpCode::OpJumpIfTrueOrPop, branch.values[i]->line));
            emit(OpCode::OpGetVar, encSel, 0, 0, 0, branch.values[i]->line);
            compileExpr(*branch.values[i]);
            emit(OpCode::OpEqual, 0, 0, 0, 0, branch.values[i]->line);
        }

        for (int oj : orJumps) {
            patchJump(oj);
        }

        int skipJump = emitJump(OpCode::OpJumpIfFalse, branch.line);
        compileBlock(branch.body);
        exitJumps.push_back(emitJump(OpCode::OpJump, branch.line));
        patchJump(skipJump);
    }

    if (!c.otherwiseBlock.empty()) {
        compileBlock(c.otherwiseBlock);
    }

    for (int ej : exitJumps) {
        patchJump(ej);
    }
}

void BytecodeCompiler::compileStmt(const Stmt& s) {
    switch (s.kind) {
        case Stmt::Kind::Declare: {
            auto& d = static_cast<const DeclareStmt&>(s);
            if (insideFunction_) {
                int slot = static_cast<int>(localVars_.size());
                localVars_.emplace_back(d.name, d.declaredType);
                localSlotMap_[d.name] = slot;
                localTypeMap_[d.name] = d.declaredType;
                localIsByRef_[d.name] = false;
            }
            break;
        }
        case Stmt::Kind::Constant: {
            auto& c = static_cast<const ConstantStmt&>(s);
            TypeInfo cType = (c.explicitType.base != BaseType::Error) ? c.explicitType : c.value->type;
            if (insideFunction_) {
                int slot = static_cast<int>(localVars_.size());
                localVars_.emplace_back(c.name, cType);
                localSlotMap_[c.name] = slot;
                localTypeMap_[c.name] = cType;
                localIsByRef_[c.name] = false;
            }
            compileExpr(*c.value);
            TypeInfo varType = getVarType(c.name);
            if (varType.base == BaseType::Real && c.value->type.base == BaseType::Integer) {
                emit(OpCode::OpWidenReal, 0, 0, 0, 0, c.line);
            }
            emit(OpCode::OpSetVar, getVarSlot(c.name), 0, 0, 0, c.line);
            break;
        }
        case Stmt::Kind::Assign: {
            auto& a = static_cast<const AssignStmt&>(s);
            compileExpr(*a.value);
            TypeInfo varType = getVarType(a.name);
            if (varType.base == BaseType::Real && a.value->type.base == BaseType::Integer) {
                emit(OpCode::OpWidenReal, 0, 0, 0, 0, a.line);
            }
            emit(OpCode::OpSetVar, getVarSlot(a.name), 0, 0, 0, a.line);
            break;
        }
        case Stmt::Kind::ArrayAssign: {
            auto& a = static_cast<const ArrayAssignStmt&>(s);
            std::string arrName = a.name;
            if (arrName.empty() && a.target && a.target->kind == Expr::Kind::Var) {
                arrName = static_cast<const VarExpr&>(*a.target).name;
            }
            compileExpr(*a.value);
            TypeInfo varType = getVarType(arrName);
            if (varType.base == BaseType::Real && a.value->type.base == BaseType::Integer) {
                emit(OpCode::OpWidenReal, 0, 0, 0, 0, a.line);
            }
            for (auto& idx : a.indices) {
                compileExpr(*idx);
            }
            int enc = getVarSlot(arrName);
            int nameConst = addConstant(Value::makeString(arrName));
            if (a.indices.size() == 1) {
                emit(OpCode::OpSetArray1D, enc, nameConst, 0, 0, a.line);
            } else {
                emit(OpCode::OpSetArray2D, enc, nameConst, 0, 0, a.line);
            }
            break;
        }
        case Stmt::Kind::MemberAssign: {
            auto& m = static_cast<const MemberAssignStmt&>(s);
            compileExpr(*m.value);
            compileExpr(*m.target);
            int fieldConst = addConstant(Value::makeString(m.field));
            emit(OpCode::OpSetField, fieldConst, 0, 0, 0, m.line);
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
            TypeInfo t = in.target ? in.target->type : getVarType(in.name);
            OpCode readOp = OpCode::OpReadInt;
            switch (t.base) {
                case BaseType::Integer: readOp = OpCode::OpReadInt; break;
                case BaseType::Real:    readOp = OpCode::OpReadReal; break;
                case BaseType::Boolean: readOp = OpCode::OpReadBool; break;
                case BaseType::String:
                case BaseType::Char:    readOp = OpCode::OpReadStr; break;
                default: break;
            }
            emit(readOp, 0, 0, 0, 0, in.line);
            if (in.target) {
                if (in.target->kind == Expr::Kind::Var) {
                    auto& v = static_cast<const VarExpr&>(*in.target);
                    emit(OpCode::OpSetVar, getVarSlot(v.name), 0, 0, 0, in.line);
                } else if (in.target->kind == Expr::Kind::ArrayAccess) {
                    auto& a = static_cast<const ArrayAccessExpr&>(*in.target);
                    for (auto& idx : a.indices) compileExpr(*idx);
                    int enc = getVarSlot(a.name);
                    int nameConst = addConstant(Value::makeString(a.name));
                    if (a.indices.size() == 1) emit(OpCode::OpSetArray1D, enc, nameConst, 0, 0, in.line);
                    else emit(OpCode::OpSetArray2D, enc, nameConst, 0, 0, in.line);
                } else if (in.target->kind == Expr::Kind::MemberAccess) {
                    auto& m = static_cast<const MemberAccessExpr&>(*in.target);
                    compileExpr(*m.target);
                    int fieldConst = addConstant(Value::makeString(m.field));
                    emit(OpCode::OpSetField, fieldConst, 0, 0, 0, in.line);
                }
            } else if (in.indices.empty()) {
                emit(OpCode::OpSetVar, getVarSlot(in.name), 0, 0, 0, in.line);
            } else {
                for (auto& idx : in.indices) compileExpr(*idx);
                int enc = getVarSlot(in.name);
                int nameConst = addConstant(Value::makeString(in.name));
                if (in.indices.size() == 1) emit(OpCode::OpSetArray1D, enc, nameConst, 0, 0, in.line);
                else emit(OpCode::OpSetArray2D, enc, nameConst, 0, 0, in.line);
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
        case Stmt::Kind::Repeat: {
            auto& r = static_cast<const RepeatStmt&>(s);
            int loopStart = static_cast<int>(chunk_.code.size());
            compileBlock(r.body);
            compileExpr(*r.cond);
            emit(OpCode::OpJumpIfFalse, loopStart, 0, 0, 0, r.line);
            break;
        }
        case Stmt::Kind::For: {
            auto& f = static_cast<const ForStmt&>(s);
            int varSlot = getVarSlot(f.var);
            int endSlot = allocateTempVar(BaseType::Integer);
            int stepSlot = allocateTempVar(BaseType::Integer);

            compileExpr(*f.start);
            emit(OpCode::OpSetVar, varSlot, 0, 0, 0, f.line);

            compileExpr(*f.end);
            emit(OpCode::OpSetVar, endSlot, 0, 0, 0, f.line);

            if (f.step) {
                compileExpr(*f.step);
                emit(OpCode::OpSetVar, stepSlot, 0, 0, 0, f.step->line);
            } else {
                int oneConst = addConstant(Value::makeInt(1));
                emit(OpCode::OpConstant, oneConst, 0, 0, 0, f.line);
                emit(OpCode::OpSetVar, stepSlot, 0, 0, 0, f.line);
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
        case Stmt::Kind::Case:
            compileCase(static_cast<const CaseStmt&>(s));
            break;
        case Stmt::Kind::Call: {
            auto& c = static_cast<const CallStmt&>(s);
            auto it = functions_.find(c.name);
            int nameConst = addConstant(Value::makeString(c.name));
            for (size_t i = 0; i < c.args.size(); ++i) {
                bool isRef = (it != functions_.end() && i < it->second.params.size() && it->second.params[i].isByRef);
                if (isRef) {
                    compileLValueRef(*c.args[i]);
                } else {
                    compileExpr(*c.args[i]);
                }
            }
            emit(OpCode::OpCall, nameConst, static_cast<int32_t>(c.args.size()), 0, 0, c.line);
            break;
        }
        case Stmt::Kind::Return: {
            auto& r = static_cast<const ReturnStmt&>(s);
            if (r.value) {
                compileExpr(*r.value);
                emit(OpCode::OpReturnVal, 0, 0, 0, 0, r.line);
            } else {
                emit(OpCode::OpReturn, 0, 0, 0, 0, r.line);
            }
            break;
        }
        case Stmt::Kind::OpenFile: {
            auto& o = static_cast<const OpenFileStmt&>(s);
            compileExpr(*o.filename);
            int modeConst = addConstant(Value::makeString(o.mode));
            emit(OpCode::OpOpenFile, modeConst, 0, 0, 0, o.line);
            break;
        }
        case Stmt::Kind::CloseFile: {
            auto& cf = static_cast<const CloseFileStmt&>(s);
            compileExpr(*cf.filename);
            emit(OpCode::OpCloseFile, 0, 0, 0, 0, cf.line);
            break;
        }
        case Stmt::Kind::ReadFile: {
            auto& rf = static_cast<const ReadFileStmt&>(s);
            compileExpr(*rf.filename);
            emit(OpCode::OpReadFile, static_cast<int>(rf.target->type.base), 0, 0, 0, rf.line);
            if (rf.target->kind == Expr::Kind::Var) {
                auto& v = static_cast<const VarExpr&>(*rf.target);
                emit(OpCode::OpSetVar, getVarSlot(v.name), 0, 0, 0, rf.line);
            } else if (rf.target->kind == Expr::Kind::ArrayAccess) {
                auto& a = static_cast<const ArrayAccessExpr&>(*rf.target);
                for (auto& idx : a.indices) compileExpr(*idx);
                int enc = getVarSlot(a.name);
                int nameConst = addConstant(Value::makeString(a.name));
                if (a.indices.size() == 1) emit(OpCode::OpSetArray1D, enc, nameConst, 0, 0, rf.line);
                else emit(OpCode::OpSetArray2D, enc, nameConst, 0, 0, rf.line);
            } else if (rf.target->kind == Expr::Kind::MemberAccess) {
                auto& m = static_cast<const MemberAccessExpr&>(*rf.target);
                compileExpr(*m.target);
                int fieldConst = addConstant(Value::makeString(m.field));
                emit(OpCode::OpSetField, fieldConst, 0, 0, 0, rf.line);
            }
            break;
        }
        case Stmt::Kind::WriteFile: {
            auto& wf = static_cast<const WriteFileStmt&>(s);
            compileExpr(*wf.filename);
            compileExpr(*wf.value);
            emit(OpCode::OpWriteFile, 0, 0, 0, 0, wf.line);
            break;
        }
        case Stmt::Kind::TypeDecl:
        case Stmt::Kind::ProcedureDecl:
        case Stmt::Kind::FunctionDecl:
            break;
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
                case Tok::CharLit:
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
            emit(OpCode::OpGetVar, getVarSlot(v.name), 0, 0, 0, v.line);
            break;
        }
        case Expr::Kind::ArrayAccess: {
            auto& a = static_cast<const ArrayAccessExpr&>(e);
            std::string arrName = a.name;
            if (arrName.empty() && a.target && a.target->kind == Expr::Kind::Var) {
                arrName = static_cast<const VarExpr&>(*a.target).name;
            }
            for (auto& idx : a.indices) {
                compileExpr(*idx);
            }
            int enc = getVarSlot(arrName);
            int nameConst = addConstant(Value::makeString(arrName));
            if (a.indices.size() == 1) {
                emit(OpCode::OpGetArray1D, enc, nameConst, 0, 0, a.line);
            } else {
                emit(OpCode::OpGetArray2D, enc, nameConst, 0, 0, a.line);
            }
            break;
        }
        case Expr::Kind::MemberAccess: {
            auto& m = static_cast<const MemberAccessExpr&>(e);
            compileExpr(*m.target);
            int fieldConst = addConstant(Value::makeString(m.field));
            emit(OpCode::OpGetField, fieldConst, 0, 0, 0, m.line);
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
                case Tok::Substring:
                case Tok::Mid:       emit(OpCode::OpSubstring, 0, 0, 0, 0, c.line); break;
                case Tok::Left:      emit(OpCode::OpLeft, 0, 0, 0, 0, c.line); break;
                case Tok::Right:     emit(OpCode::OpRight, 0, 0, 0, 0, c.line); break;
                case Tok::UCase:     emit(OpCode::OpUCase, 0, 0, 0, 0, c.line); break;
                case Tok::LCase:     emit(OpCode::OpLCase, 0, 0, 0, 0, c.line); break;
                case Tok::NumToStr:  emit(OpCode::OpNumToStr, 0, 0, 0, 0, c.line); break;
                case Tok::StrToNum:  emit(OpCode::OpStrToNum, 0, 0, 0, 0, c.line); break;
                case Tok::Chr:       emit(OpCode::OpChr, 0, 0, 0, 0, c.line); break;
                case Tok::Asc:       emit(OpCode::OpAsc, 0, 0, 0, 0, c.line); break;
                case Tok::IntFunc:   emit(OpCode::OpInt, 0, 0, 0, 0, c.line); break;
                case Tok::Round:     emit(OpCode::OpRound, 0, 0, 0, 0, c.line); break;
                case Tok::EofFunc:   emit(OpCode::OpEof, 0, 0, 0, 0, c.line); break;
                default: break;
            }
            break;
        }
        case Expr::Kind::UserCall: {
            auto& uc = static_cast<const UserCallExpr&>(e);
            auto it = functions_.find(uc.callee);
            int nameConst = addConstant(Value::makeString(uc.callee));
            for (size_t i = 0; i < uc.args.size(); ++i) {
                bool isRef = (it != functions_.end() && i < it->second.params.size() && it->second.params[i].isByRef);
                if (isRef) {
                    compileLValueRef(*uc.args[i]);
                } else {
                    compileExpr(*uc.args[i]);
                }
            }
            emit(OpCode::OpCall, nameConst, static_cast<int32_t>(uc.args.size()), 1, 0, uc.line);
            break;
        }
    }
}
