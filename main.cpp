#include "lexer/Lexer.hpp"
#include "lexer/Token.hpp"
#include <fstream>
#include <iostream>
int main() {
  std::ifstream inputfile;
  std::ofstream outputfile("error.txt");
  inputfile.open("lexer.txt");
  if (!inputfile.is_open()) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }
  std::string fileContent((std::istreambuf_iterator<char>(inputfile)),
                          std::istreambuf_iterator<char>());
  class Lexer lexer = Lexer(fileContent);
  class Token token = lexer.nextToken();
  while (token.type != TokenType::END_OF_FILE) {
    if (token.type == TokenType::UNKNOWN) {
      outputfile << token.line << " " << "a" << std::endl;
    } else {
      std::cout << token.getTokenType() << " " << token.lexeme << std::endl;
    }
    token = lexer.nextToken();
  }
  return 0;
}
