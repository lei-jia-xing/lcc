/**
 * @file
 * @brief header file for lexer
 */

#pragma once
#include "Token.hpp"
#include <unordered_map>
class Lexer {
private:
  /**
   * @brief source programe to be analyzed
   */
  std::string source;
  /**
   * @brief current position in source
   */
  size_t pos;
  /**
   * @brief current line number in source
   */
  int line;

  /**
   * @brief reserve keyword in this EBNF
   */
  inline static std::unordered_map<std::string, TokenType> reserveWords = {
      {"const", TokenType::CONSTTK},       {"int", TokenType::INTTK},
      {"static", TokenType::STATICTK},     {"break", TokenType::BREAKTK},
      {"continue", TokenType::CONTINUETK}, {"if", TokenType::IFTK},
      {"main", TokenType::MAINTK},         {"else", TokenType::ELSETK},
      {"for", TokenType::FORTK},           {"return", TokenType::RETURNTK},
      {"void", TokenType::VOIDTK},         {"printf", TokenType::PRINTFTK}};
  /**
   * @brief skip whitespace
   */
  void skipwhitespace();

public:
  /**
   * @brief silent depth for error and output
   */
  inline static int silentDepth = 0;
  /**
   * @brief error logging
   *
   * @param line current line number
   * @param errorType errorType to show
   */
  void error(const int &line, const std::string errorType);
  /**
   * @brief constructor for Lexer
   *
   * @param source source programe to be analyzed
   * @param pos current position in source
   * @param line current line number in source
   */
  Lexer(std::string source, size_t pos = 0, int line = 1);
  /**
   * @brief a function to get next token and output, error logging
   *
   * @return next Token
   */
  Token nextToken();
  /**
   * @brief to enable or disable silent for error and output
   *
   * @param silent whether to change silent depth
   */
  void silentPV(bool silent);
  /**
   * @brief a function to output
   *
   * @param type TokenType
   * @param value token lexeme
   */
  void output(const std::string &type, const std::string &value);

  /**
   * @brief look ahead n tokens without consuming them
   *
   * @param n look ahead n tokens
   * @return the n-th token
   */
  Token peekToken(int n);
};
