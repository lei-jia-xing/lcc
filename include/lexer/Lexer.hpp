#pragma once
#include "Token.hpp"
#include <unordered_map>
class Lexer {
private:
  std::string source;
  size_t pos;
  int line;

  inline static std::unordered_map<std::string, TokenType> reserveWords = {
      {"const", TokenType::CONSTTK},       {"int", TokenType::INTTK},
      {"static", TokenType::STATICTK},     {"break", TokenType::BREAKTK},
      {"continue", TokenType::CONTINUETK}, {"if", TokenType::IFTK},
      {"main", TokenType::MAINTK},         {"else", TokenType::ELSETK},
      {"for", TokenType::FORTK},           {"return", TokenType::RETURNTK},
      {"void", TokenType::VOIDTK},         {"printf", TokenType::PRINTFTK}};
  void skipwhitespace();

public:
  inline static int silentDepth = 0;
  void error(const int &line, const std::string errorType);
  Lexer(std::string source, size_t pos = 0, int line = 1);
  Token nextToken();
  void silentPV(bool silent);
  void output(const std::string &type, const std::string &value);
};
