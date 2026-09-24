#include "repl.h"
#include "bytecode.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "vm.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

void runRepl() {
    std::cout << "Pseudoc REPL v3.0 (VM-powered, type ':q' or 'EXIT' to quit)\n";
    std::unordered_map<std::string, Sema::Symbol> replSymbols;
    std::vector<std::pair<std::string, TypeInfo>> replVars;
    VM vm({});

    std::string accumulated;
    int blockDepth = 0;

    auto updateBlockDepth = [](const std::string& line, int depth) -> int {
        Diagnostics diag("<check>", line, true);
        std::vector<Token> toks = Lexer(line, diag).tokenize();
        for (const auto& t : toks) {
            if (t.type == Tok::If || t.type == Tok::While || t.type == Tok::For) depth++;
            else if (t.type == Tok::EndIf || t.type == Tok::EndWhile || t.type == Tok::Next) depth = std::max(0, depth - 1);
        }
        return depth;
    };

    while (true) {
        std::cout << (blockDepth > 0 ? "... " : ">>> ");
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (blockDepth == 0 && (line == ":q" || line == "EXIT" || line == "exit")) break;
        if (line.empty() && blockDepth == 0) continue;

        if (!accumulated.empty()) accumulated += "\n";
        accumulated += line;
        blockDepth = updateBlockDepth(line, blockDepth);
        if (blockDepth > 0) continue;

        std::string inputToRun = accumulated;
        accumulated.clear();

        // Check if expression should be auto-wrapped as OUTPUT
        std::string trialLine = inputToRun;
        {
            Diagnostics testDiag("<repl>", inputToRun, true);
            std::vector<Token> toks = Lexer(inputToRun, testDiag).tokenize();
            Parser parser(toks, testDiag);
            (void)parser.parseProgram();
            if (testDiag.errorCount() > 0) {
                std::string wrapped = "OUTPUT " + inputToRun;
                Diagnostics wrapDiag("<repl>", wrapped, true);
                std::vector<Token> wrapToks = Lexer(wrapped, wrapDiag).tokenize();
                Parser wrapParser(wrapToks, wrapDiag);
                (void)wrapParser.parseProgram();
                if (wrapDiag.errorCount() == 0) trialLine = wrapped;
            }
        }

        Diagnostics trialDiag("<repl>", trialLine);
        std::vector<Token> trialToks = Lexer(trialLine, trialDiag).tokenize();
        Block trialProg = Parser(trialToks, trialDiag).parseProgram();
        if (trialDiag.errorCount() > 0) continue;

        Sema trialSema(trialDiag);
        trialSema.setExistingSymbols(replSymbols, replVars);
        trialSema.run(trialProg);
        if (trialDiag.errorCount() > 0) continue;

        // Commit symbols and update VM variables
        replSymbols = trialSema.symbols();
        replVars = trialSema.variables();
        vm.syncGlobals(replVars);

        // Compile statement(s) to bytecode and run on VM
        Chunk chunk = BytecodeCompiler(replVars).compile(trialProg);
        vm.run(chunk, true);
    }
}
