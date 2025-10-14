/**
 * @file
 * @brief header file for token definition
 */

#pragma once
#include <string>
#include <variant>

#define TOKEN_LIST                                                             \
  X(IDENFR)                                                                    \
  X(INTCON)                                                                    \
  X(STRCON)                                                                    \
  X(CONSTTK)                                                                   \
  X(INTTK)                                                                     \
  X(STATICTK)                                                                  \
  X(BREAKTK)                                                                   \
  X(CONTINUETK)                                                                \
  X(IFTK)                                                                      \
  X(MAINTK)                                                                    \
  X(ELSETK)                                                                    \
  X(NOT)                                                                       \
  X(AND)                                                                       \
  X(OR)                                                                        \
  X(FORTK)                                                                     \
  X(RETURNTK)                                                                  \
  X(VOIDTK)                                                                    \
  X(PLUS)                                                                      \
  X(MINU)                                                                      \
  X(PRINTFTK)                                                                  \
  X(MULT)                                                                      \
  X(DIV)                                                                       \
  X(MOD)                                                                       \
  X(LSS)                                                                       \
  X(LEQ)                                                                       \
  X(GRE)                                                                       \
  X(GEQ)                                                                       \
  X(EQL)                                                                       \
  X(NEQ)                                                                       \
  X(SEMICN)                                                                    \
  X(COMMA)                                                                     \
  X(LPARENT)                                                                   \
  X(RPARENT)                                                                   \
  X(LBRACK)                                                                    \
  X(RBRACK)                                                                    \
  X(LBRACE)                                                                    \
  X(RBRACE)                                                                    \
  X(ASSIGN)                                                                    \
  X(EOFTK)                                                                     \
  X(UNKNOWN)

#define X(name) name,
enum class TokenType { TOKEN_LIST };
#undef X

class Token {
public:
  /**
   * @brief the type of token
   */
  TokenType type;
  /**
   * @brief the lexeme of token
   */
  std::string lexeme;
  /**
   * @brief the line number of token in source code
   */
  int line;
  std::variant<std::monostate, int, std::string> value;

  /**
   * @brief constructor for Token
   *
   * @param type initial type of token. default to UNKNOWN
   * @param lexeme initial lexeme of token, default to empty string
   * @param line initial line number of token in source code, default to 1
   * @param value value of token(int for INTCON, string for STRCON), default to
   * monostate
   */
  Token(TokenType type = TokenType::UNKNOWN, std::string lexeme = "",
        int line = 1, std::variant<std::monostate, int, std::string> value = {})
      : type(type), lexeme(lexeme), line(line), value(value) {}
  std::variant<std::monostate, int, std::string> getToken() const {
    return value;
  }
  /**
   * @brief a function to get string representation of token type
   */
  std::string getTokenType() {
    switch (type) {
#define X(name)                                                                \
  case TokenType::name:                                                        \
    return #name;
      TOKEN_LIST
#undef X
    default:
      return "UNKNOWN";
    }
  }
};
