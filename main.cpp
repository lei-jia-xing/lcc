#include "lexer/Lexer.hpp"
#include "lexer/Token.hpp"
#include <fstream>
#include <iostream>
int main() {
  std::ifstream inputfile;
  std::ofstream errorfile("error.txt");
  std::ofstream lexerfile("lexer.txt");
  inputfile.open("testfile.txt");
  if (!inputfile.is_open()) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }
  std::string fileContent((std::istreambuf_iterator<char>(inputfile)),
                          std::istreambuf_iterator<char>());
  class Lexer lexer = Lexer(fileContent, 0, 1);
  class Token token = lexer.nextToken();
  while (token.type != TokenType::END_OF_FILE) {
    if (token.type == TokenType::UNKNOWN) {
      errorfile << token.line << " " << "a" << std::endl;
    } else {
      lexerfile << token.getTokenType() << " " << token.lexeme << std::endl;
    }
    token = lexer.nextToken();
  }
  return 0;
}
