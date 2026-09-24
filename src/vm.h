#ifndef PSEUDOC_VM_H
#define PSEUDOC_VM_H

#include "ast.h"
#include "bytecode.h"

#include <string>
#include <utility>
#include <vector>

class VM {
public:
    explicit VM(const std::vector<std::pair<std::string, TypeInfo>>& vars);

    void initGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars);
    void syncGlobals(const std::vector<std::pair<std::string, TypeInfo>>& vars);

    int run(const Chunk& chunk, bool isRepl = false);

private:
    std::vector<Value> globals_;
    std::vector<Value> stack_;

    void push(Value v);
    Value pop();
    const Value& peek() const;

    static Value defaultValue(const TypeInfo& t);
    static std::string readLine();
};

#endif // PSEUDOC_VM_H
