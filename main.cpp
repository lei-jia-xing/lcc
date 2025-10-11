#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include <fstream>
#include <iostream>

int main() {
  std::ofstream errorfile("error.txt");
  std::streambuf *original_cerr = std::cerr.rdbuf();
  std::cerr.rdbuf(errorfile.rdbuf());

  std::ifstream inputfile("testfile.txt");
  if (!inputfile.is_open()) {
    std::cerr << "Error opening testfile.txt" << std::endl;
    std::cerr.rdbuf(original_cerr);
    return 1;
  }

  std::string fileContent((std::istreambuf_iterator<char>(inputfile)),
                          std::istreambuf_iterator<char>());
  inputfile.close();
  try {
    Lexer lexer(fileContent);
    Parser parser(std::move(lexer));
    auto compUnit = parser.parseCompUnit();
  } catch (const std::exception &e) {
    std::cerr << "Parse error: " << e.what() << std::endl;
  }
  std::cerr.rdbuf(original_cerr);

  return 0;
}
