#include "bytecode.h"
#include "c_to_pseudo.h"
#include "codegen.h"
#include "py_codegen.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "repl.h"
#include "sema.h"
#include "vm.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void usage() {
    std::cerr << "usage: pseudoc [<input>] [options]\n\n"
              << "Options:\n"
              << "  -o <output>           Output file (C source, Python, or Pseudocode depending on mode)\n"
              << "  -c, --emit-c          Compile pseudocode and emit C source code to stdout\n"
              << "  -p, --emit-py         Compile pseudocode and emit Python 3 source code\n"
              << "  -r, --c-to-pseudo     Decompile/transpile C source code to Cambridge pseudocode\n"
              << "  -d, --dump-bc         Disassemble and print bytecode without executing\n"
              << "  -i, --repl            Run interactive REPL (powered by VM)\n"
              << "  -h, --help            Show this help message\n\n"
              << "Default behavior:\n"
              << "  pseudoc script.pseudo Execute script directly on the Pseudoc VM\n"
              << "  pseudoc               Start interactive REPL\n";
}

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

int main(int argc, char** argv) {
    std::string inPath, outPath;
    bool interactive = false;
    bool emitC = false;
    bool emitPy = false;
    bool dumpBc = false;
    bool cToPseudo = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { usage(); return 0; }
        if (arg == "-i" || arg == "--repl") { interactive = true; }
        else if (arg == "-c" || arg == "--emit-c") { emitC = true; }
        else if (arg == "-p" || arg == "--emit-py" || arg == "--emit-python") { emitPy = true; }
        else if (arg == "-r" || arg == "--reverse" || arg == "--c-to-pseudo") { cToPseudo = true; }
        else if (arg == "-d" || arg == "--dump-bc" || arg == "--dump-bytecode") { dumpBc = true; }
        else if (arg == "-o" && i + 1 < argc) { outPath = argv[++i]; }
        else if (inPath.empty()) { inPath = arg; }
        else { usage(); return 2; }
    }

    if (interactive || (inPath.empty() && outPath.empty() && !emitC && !emitPy && !dumpBc && !cToPseudo)) {
        runRepl();
        return 0;
    }

    if (inPath.empty()) {
        usage();
        return 2;
    }

    std::string source;
    if (!readFile(inPath, source)) {
        std::cerr << "pseudoc: cannot open '" << inPath << "'\n";
        return 2;
    }

    // Auto-detect C file if extension is .c and neither emitC nor dumpBc is set
    if (!cToPseudo && inPath.size() >= 2 && inPath.substr(inPath.size() - 2) == ".c" && !emitC && !dumpBc) {
        cToPseudo = true;
    }

    if (cToPseudo) {
        std::string pseudo = translateCToPseudocode(source);
        if (outPath.empty()) {
            std::cout << pseudo;
        } else {
            std::ofstream out(outPath);
            if (!out) {
                std::cerr << "pseudoc: cannot write '" << outPath << "'\n";
                return 2;
            }
            out << pseudo;
        }
        return 0;
    }

    Diagnostics diag(inPath, source);
    std::vector<Token> tokens = Lexer(source, diag).tokenize();
    Block program = Parser(tokens, diag).parseProgram();

    Sema sema(diag);
    if (diag.errorCount() == 0) sema.run(program);

    if (diag.errorCount() > 0) {
        std::cerr << diag.errorCount() << (diag.errorCount() == 1 ? " error" : " errors")
                  << " found; no execution or output written.\n";
        return 1;
    }

    // Python Code Generation Mode (-o <file.py> or -p / --emit-py)
    if (emitPy || (!outPath.empty() && outPath.size() >= 3 && outPath.substr(outPath.size() - 3) == ".py")) {
        std::string py = PyCodeGen(sema.variables(), sema.recordTypes(), sema.functions(), sema.classTypes()).generate(program);
        if (outPath.empty()) {
            std::cout << py;
        } else {
            std::ofstream out(outPath);
            if (!out) {
                std::cerr << "pseudoc: cannot write '" << outPath << "'\n";
                return 2;
            }
            out << py;
        }
        return 0;
    }

    // C Code Generation Mode (-o <file.c> or -c / --emit-c)
    if (!outPath.empty() || emitC) {
        std::string c = CodeGen(sema.variables(), sema.recordTypes(), sema.functions(), sema.classTypes()).generate(program);
        if (outPath.empty()) {
            std::cout << c;
        } else {
            std::ofstream out(outPath);
            if (!out) {
                std::cerr << "pseudoc: cannot write '" << outPath << "'\n";
                return 2;
            }
            out << c;
        }
        return 0;
    }

    // Bytecode Compilation
    Chunk chunk = BytecodeCompiler(sema.variables(), sema.recordTypes(), sema.functions(), sema.classTypes()).compile(program);

    // Disassembly Mode (-d / --dump-bc)
    if (dumpBc) {
        dumpBytecode(chunk, inPath);
        return 0;
    }

    // Default: Direct execution on the Pseudoc VM
    VM vm(chunk.varDescs, sema.recordTypes());
    return vm.run(chunk);
}
