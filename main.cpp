#include "backend/AsmGen.hpp"
#include "codegen/CodeGen.hpp"
#include "codegen/QuadOptimizer.hpp"
#include "errorReporter/ErrorReporter.hpp"
#include "lexer/Lexer.hpp"
#include "optimize/DominatorTree.hpp"
#include "optimize/GlobalConstEval.hpp"
#include "optimize/LICM.hpp"
#include "optimize/LoopAnalysis.hpp"
#include "optimize/LoopUnroll.hpp"
#include "optimize/Mem2Reg.hpp"
#include "optimize/PhiElimination.hpp"
#include "parser/Parser.hpp"
#include "semantic/SemanticAnalyzer.hpp"
#include <fstream>
#include <iostream>

// Optimization switch: set to true to enable IR optimizations
constexpr bool ENABLE_OPTIMIZATION = true;
const int MAX_ROUND = 10;

int main() {
  std::ofstream errorfile("error.txt");
  std::streambuf *original_cerr = std::cerr.rdbuf();
  std::cerr.rdbuf(errorfile.rdbuf());

  std::ofstream parserfile("ir.txt");
  std::streambuf *original_cout = std::cout.rdbuf();
  std::cout.rdbuf(parserfile.rdbuf());

  std::ifstream inputfile("testfile.txt");
  if (!inputfile.is_open()) {
    std::cerr << "Error opening testfile.txt" << std::endl;
    std::cerr.rdbuf(original_cerr);
    std::cout.rdbuf(original_cout);
    return 1;
  }

  std::string fileContent((std::istreambuf_iterator<char>(inputfile)),
                          std::istreambuf_iterator<char>());
  inputfile.close();

  Lexer lexer(fileContent);
  auto firstToken = lexer.nextToken();
  Parser parser(std::move(lexer), firstToken);
  auto compUnit = parser.parseCompUnit();

  SemanticAnalyzer semanticAnalyzer;
  if (compUnit) {
    semanticAnalyzer.visit(compUnit.get());
  }

  if (ErrorReporter::getInstance().hasError()) {
    ErrorReporter::getInstance().printErrors();
    std::cerr.rdbuf(original_cerr);
    std::cout.rdbuf(original_cout);
    exit(1);
  }

  if (compUnit) {
    CodeGen cg(semanticAnalyzer.getSymbolTable());
    cg.generate(compUnit.get());

    // Apply IR optimizations if enabled
    if constexpr (ENABLE_OPTIMIZATION) {
      auto &functions = cg.getFunctions();
      // build SSA
      for (auto &fp : functions) {
        DominatorTree dt;
        dt.run(*fp);
        Mem2RegPass mem2reg;
        mem2reg.run(*fp, dt);
      }

      for (auto &fp : functions) {
        DominatorTree dt;
        dt.run(*fp);
        LoopAnalysis loopAnalysis;
        loopAnalysis.run(*fp, dt);
        auto &loops = loopAnalysis.getLoops();
        if (!loops.empty()) {
          LICMPass licm;
          licm.run(*fp, dt, loops);
          LoopUnrollPass loopUnroll;
          loopUnroll.run(*fp, loops);
        }
      }
      bool changed = true;
      int round = 0;
      while (changed && round < MAX_ROUND) {
        changed = false;
        round++;
        GlobalConstEvalPass globalEval(functions);
        for (auto &fp : functions) {
          if (globalEval.run(*fp)) {
            changed = true;
          }
        }
        for (auto &fp : functions) {
          DominatorTree dt;
          dt.run(*fp);
          if (runDefaultQuadOptimizations(*fp, dt)) {
            changed = true;
          }
        }
      }
      // phi elimination
      for (auto &fp : functions) {
        PhiEliminationPass phiElim;
        phiElim.run(*fp);
      }
    }

    // Output optimized IR to ir.txt
    for (const auto &fp : cg.getFunctions()) {
      for (const auto &blk : fp->getBlocks()) {
        for (const auto &inst : blk->getInstructions()) {
          if (inst) {
            std::cout << inst->toString() << '\n';
          }
        }
      }
    }

    IRModuleView mod;
    for (const auto &fp : cg.getFunctions()) {
      mod.functions.push_back(fp.get());
    }
    const auto &globals = cg.getGlobalsIR();
    for (size_t i = 0; i < globals.size(); ++i) {
      mod.globals.push_back(globals[i].get());
    }
    for (const auto &kv : cg.getStringLiteralSymbols()) {
      auto &literal = kv.first;
      auto &label = kv.second;
      mod.stringLiterals[literal] = label;
    }

    std::ofstream asmout("mips.txt");
    AsmGen asmgen;
    asmgen.generate(mod, asmout);
  }

  std::cerr.rdbuf(original_cerr);
  std::cout.rdbuf(original_cout);

  return 0;
}
