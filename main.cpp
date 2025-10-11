#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include <fstream>
#include <iostream>

int main() {
  // 重定向错误输出到error.txt
  std::ofstream errorfile("error.txt");
  std::streambuf *original_cerr = std::cerr.rdbuf();
  std::cerr.rdbuf(errorfile.rdbuf());

  // 重定向标准输出到parser.txt
  std::ofstream parserfile("parser.txt");
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

  // 恢复原始输出流
  std::cerr.rdbuf(original_cerr);
  std::cout.rdbuf(original_cout);

  return 0;
}
