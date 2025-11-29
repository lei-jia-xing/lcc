#include "backend/AsmGen.hpp"
#include "codegen/CodeGen.hpp"
#include "errorReporter/ErrorReporter.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "semantic/SemanticAnalyzer.hpp"
#include <fstream>
#include <iostream>

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

    IRModuleView mod;
    for (const auto &fp : cg.getFunctions()) {
      mod.functions.push_back(fp.get());
    }
    const auto &globals = cg.getGlobalsIR();
    for (size_t i = 0; i < globals.size(); ++i) {
      mod.globals.push_back(&globals[i]);
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
