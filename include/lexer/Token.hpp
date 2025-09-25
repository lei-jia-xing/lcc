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
  X(END_OF_FILE)                                                               \
  X(UNKNOWN)

#define X(name) name,
enum class TokenType { TOKEN_LIST };
#undef X

class Token {
public:
  TokenType type;
  std::string lexeme;
  int line;
  std::variant<std::monostate, int, std::string> value;

  Token(TokenType type, std::string lexeme, int line,
        std::variant<std::monostate, int, std::string> value = {})
      : type(type), lexeme(lexeme), line(line), value(value) {}
  std::variant<std::monostate, int, std::string> getToken() const {
    return value;
  }
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
