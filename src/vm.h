#ifndef PSEUDOC_VM_H
#define PSEUDOC_VM_H

#include "ast.h"
#include "bytecode.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct CallFrame {
    std::string funcName;
    size_t returnIp = 0;
    size_t stackBase = 0;
    int numLocals = 0;
    bool isFunction = false;
};

class VM {
public:
    VM(const std::vector<std::pair<std::string, TypeInfo>>& vars,
       const std::unordered_map<std::string, RecordDef>& recordTypes);
    ~VM();

    void initGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars);
    void syncGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars);
    void setRecordTypes(const std::unordered_map<std::string, RecordDef>& recordTypes) {
        recordTypes_ = recordTypes;
    }

    int run(const Chunk& chunk, bool isRepl = false);

private:
    std::vector<Value> globals_;
    std::vector<Value> stack_;
    std::vector<CallFrame> callFrames_;
    std::unordered_map<std::string, RecordDef> recordTypes_;
    std::unordered_map<std::string, FILE*> openFiles_;
    std::unordered_map<std::string, std::string> fileModes_;

    void push(Value v);
    Value pop();
    const Value& peek() const;

    Value defaultValue(const TypeInfo& t);
    static std::string readLine();

    Value deref(const Value& v);
    void writeThrough(const Value& ref, Value newVal);
    Value& getVarRef(int enc);
    Value getVarVal(int enc);
    void setVarVal(int enc, Value val);

    void closeAllFiles();
};

#endif // PSEUDOC_VM_H
