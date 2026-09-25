#include "bytecode.h"
#include "c_to_pseudo.h"
#include "codegen.h"
#include "py_codegen.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "vm.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

static std::string g_result;

namespace {

struct StreamRedirect {
    std::istringstream inStream;
    std::ostringstream outStream;
    std::ostringstream errStream;
    std::streambuf* oldIn;
    std::streambuf* oldOut;
    std::streambuf* oldErr;

    explicit StreamRedirect(const std::string& input)
        : inStream(input),
          oldIn(std::cin.rdbuf(inStream.rdbuf())),
          oldOut(std::cout.rdbuf(outStream.rdbuf())),
          oldErr(std::cerr.rdbuf(errStream.rdbuf())) {}

    ~StreamRedirect() {
        std::cin.rdbuf(oldIn);
        std::cout.rdbuf(oldOut);
        std::cerr.rdbuf(oldErr);
    }
};

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* wasm_run_vm(const char* source, const char* stdin_input) {
    if (!source) {
        g_result = "";
        return g_result.c_str();
    }
    std::string inputStr = stdin_input ? stdin_input : "";
    StreamRedirect redir(inputStr);

    Diagnostics diag("<playground>", source);
    std::vector<Token> tokens = Lexer(source, diag).tokenize();
    Block program = Parser(tokens, diag).parseProgram();

    Sema sema(diag);
    if (diag.errorCount() == 0) {
        sema.run(program);
    }

    if (diag.errorCount() > 0) {
        g_result = redir.errStream.str();
        return g_result.c_str();
    }

    try {
        Chunk chunk = BytecodeCompiler(sema.variables(), sema.recordTypes(), sema.functions(), sema.classTypes()).compile(program);
        VM vm(chunk.varDescs, sema.recordTypes());
        vm.run(chunk);
    } catch (const std::exception& ex) {
        std::cerr << "Internal error: " << ex.what() << "\n";
    }

    g_result = redir.outStream.str() + redir.errStream.str();
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_emit_c(const char* source) {
    if (!source) {
        g_result = "";
        return g_result.c_str();
    }
    StreamRedirect redir("");

    Diagnostics diag("<playground>", source);
    std::vector<Token> tokens = Lexer(source, diag).tokenize();
    Block program = Parser(tokens, diag).parseProgram();

    Sema sema(diag);
    if (diag.errorCount() == 0) {
        sema.run(program);
    }

    if (diag.errorCount() > 0) {
        g_result = redir.errStream.str();
        return g_result.c_str();
    }

    try {
        g_result = CodeGen(sema.variables(), sema.recordTypes(), sema.functions(), sema.classTypes()).generate(program);
    } catch (const std::exception& ex) {
        g_result = std::string("Error generating C: ") + ex.what();
    }
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_emit_py(const char* source) {
    if (!source) {
        g_result = "";
        return g_result.c_str();
    }
    StreamRedirect redir("");

    Diagnostics diag("<playground>", source);
    std::vector<Token> tokens = Lexer(source, diag).tokenize();
    Block program = Parser(tokens, diag).parseProgram();

    Sema sema(diag);
    if (diag.errorCount() == 0) {
        sema.run(program);
    }

    if (diag.errorCount() > 0) {
        g_result = redir.errStream.str();
        return g_result.c_str();
    }

    try {
        g_result = PyCodeGen(sema.variables(), sema.recordTypes(), sema.functions(), sema.classTypes()).generate(program);
    } catch (const std::exception& ex) {
        g_result = std::string("Error generating Python: ") + ex.what();
    }
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_dump_bytecode(const char* source) {
    if (!source) {
        g_result = "";
        return g_result.c_str();
    }
    StreamRedirect redir("");

    Diagnostics diag("<playground>", source);
    std::vector<Token> tokens = Lexer(source, diag).tokenize();
    Block program = Parser(tokens, diag).parseProgram();

    Sema sema(diag);
    if (diag.errorCount() == 0) {
        sema.run(program);
    }

    if (diag.errorCount() > 0) {
        g_result = redir.errStream.str();
        return g_result.c_str();
    }

    try {
        Chunk chunk = BytecodeCompiler(sema.variables(), sema.recordTypes(), sema.functions(), sema.classTypes()).compile(program);
        dumpBytecode(chunk, "<playground>");
        g_result = redir.outStream.str();
    } catch (const std::exception& ex) {
        g_result = std::string("Error compiling bytecode: ") + ex.what();
    }
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_check(const char* source) {
    if (!source) {
        g_result = "";
        return g_result.c_str();
    }
    StreamRedirect redir("");

    Diagnostics diag("<playground>", source);
    std::vector<Token> tokens = Lexer(source, diag).tokenize();
    Block program = Parser(tokens, diag).parseProgram();

    Sema sema(diag);
    if (diag.errorCount() == 0) {
        sema.run(program);
    }

    if (diag.errorCount() > 0) {
        g_result = redir.errStream.str();
    } else {
        g_result = "OK: Syntax and semantic analysis passed with 0 errors.";
    }
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_c_to_pseudo(const char* c_source) {
    if (!c_source) {
        g_result = "";
        return g_result.c_str();
    }
    try {
        g_result = translateCToPseudocode(c_source);
    } catch (const std::exception& ex) {
        g_result = std::string("Error decompiling C: ") + ex.what();
    }
    return g_result.c_str();
}

} // extern "C"
