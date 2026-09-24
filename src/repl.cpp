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
    VM vm({}, {});

    std::string accumulated;
    int blockDepth = 0;

    auto updateBlockDepth = [](const std::string& line, int depth) -> int {
        Diagnostics diag("<check>", line, true);
        std::vector<Token> toks = Lexer(line, diag).tokenize();
        for (const auto& t : toks) {
            if (t.type == Tok::If || t.type == Tok::While || t.type == Tok::For ||
                t.type == Tok::Case || t.type == Tok::Function || t.type == Tok::Procedure ||
                t.type == Tok::Type || t.type == Tok::Repeat) {
                depth++;
            } else if (t.type == Tok::EndIf || t.type == Tok::EndWhile || t.type == Tok::Next ||
                       t.type == Tok::EndCase || t.type == Tok::EndFunction || t.type == Tok::EndProcedure ||
                       t.type == Tok::EndType || t.type == Tok::Until) {
                depth = std::max(0, depth - 1);
            }
        }
        return depth;
    };

    while (true) {
        std::cout << (blockDepth > 0 ? "... " : ">>> ");
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (blockDepth == 0 && (line == ":q" || line == "EXIT" || line == "exit")) break;
        if (blockDepth == 0 && (line == ":help" || line == "help")) {
            std::cout << "Pseudoc REPL Commands:\n"
                      << "  :help           Show this help message\n"
                      << "  :vars / :env    Display all active variables and values\n"
                      << "  :reset          Reset all variables and REPL state\n"
                      << "  :q / EXIT       Exit the REPL\n\n"
                      << "Cambridge Pseudocode Quick Reference:\n"
                      << "  DECLARE <id> : <type>             (INTEGER, REAL, BOOLEAN, STRING, CHAR)\n"
                      << "  CONSTANT <id> = <value>\n"
                      << "  <var> <- <expr>\n"
                      << "  OUTPUT <expr>, ...\n"
                      << "  INPUT <var>\n"
                      << "  IF ... THEN ... ELSE ... ENDIF\n"
                      << "  WHILE ... DO ... ENDWHILE\n"
                      << "  REPEAT ... UNTIL <cond>\n"
                      << "  FOR <var> <- <start> TO <end> [STEP <s>] ... NEXT <var>\n";
            continue;
        }
        if (blockDepth == 0 && (line == ":vars" || line == ":env")) {
            if (replVars.empty()) {
                std::cout << "(no variables declared)\n";
            } else {
                const auto& vals = vm.globals();
                for (size_t i = 0; i < replVars.size(); ++i) {
                    std::cout << "  " << replVars[i].first << " : "
                              << typeString(replVars[i].second) << " = ";
                    if (i < vals.size()) vals[i].print(std::cout);
                    std::cout << "\n";
                }
            }
            continue;
        }
        if (blockDepth == 0 && line == ":reset") {
            replSymbols.clear();
            replVars.clear();
            vm.initGlobals({});
            std::cout << "REPL state reset.\n";
            continue;
        }
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
        vm.setRecordTypes(trialSema.recordTypes());

        // Compile statement(s) to bytecode and run on VM
        Chunk chunk = BytecodeCompiler(replVars, trialSema.recordTypes(), trialSema.functions()).compile(trialProg);
        vm.run(chunk, true);
    }
}
