#include "Token.hpp"
#include <unordered_map>
class lexer {
private:
  std::string source;
  size_t pos = 0;
  int line = 1;

  inline static std::unordered_map<std::string, TokenType> reserveWords = {
      {"const", TokenType::CONSTTK},       {"int", TokenType::INTTK},
      {"static", TokenType::STATICTK},     {"break", TokenType::BREAKTK},
      {"continue", TokenType::CONTINUETK}, {"if", TokenType::IFTK},
      {"main", TokenType::MAINTK},         {"else", TokenType::ELSETK},
      {"for", TokenType::FORTK},           {"return", TokenType::RETURNTK},
      {"void", TokenType::VOIDTK},         {"printf", TokenType::PRINTFTK}};

public:
  lexer(std::string source) : source(source) {}
  Token nextToken();
};
