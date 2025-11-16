#include "errorReporter/ErrorReporter.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "semantic/SemanticAnalyzer.hpp"
#include <fstream>
#include <iostream>
#include "codegen/CodeGen.hpp"

int main() {
  std::ofstream errorfile("error.txt");
  std::streambuf *original_cerr = std::cerr.rdbuf();
  std::cerr.rdbuf(errorfile.rdbuf());

  std::ofstream parserfile("mips.txt");
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

  ErrorReporter::getInstance().printErrors();

  // Generate IR; output is internalized to std::cout which currently
  // points to mips.txt
  if (compUnit) {
    lcc::codegen::CodeGen cg;
    cg.generate(compUnit.get());
  }

  // 恢复原始输出流
  std::cerr.rdbuf(original_cerr);
  std::cout.rdbuf(original_cout);

  return 0;
}
