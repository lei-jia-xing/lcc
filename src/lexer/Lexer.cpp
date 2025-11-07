#include "lexer/Lexer.hpp"
#include "errorReporter/ErrorReporter.hpp"
#include "lexer/Token.hpp"
#include <iostream>
#include <string>

Lexer::Lexer(std::string source, size_t pos, int line)
    : source(source), pos(pos), line(line) {}
void Lexer::skipwhitespace() {
  while (pos < source.length() && (source[pos] == '\t' || source[pos] == '\r' ||
                                   source[pos] == '\n' || source[pos] == ' ')) {
    if (source[pos] == '\n') {
      line++;
    }
    pos++;
  }
}
void Lexer::error(const int &line, const std::string errorType) {
  if (silentDepth == 0) {
    ErrorReporter::getInstance().addError(line, errorType);
  }
}

void Lexer::silentPV(bool silent) {
  if (silent) {
    silentDepth++;
  } else {
    silentDepth > 0 ? silentDepth-- : silentDepth = 0;
  }
}

void Lexer::output(const std::string &type, const std::string &value) {
  if (silentDepth == 0 && outputEnabled) {
    std::cout << type << " " << value << std::endl;
  }
}

Token Lexer::peekToken(int n) {
  size_t savedPos = pos;
  int savedLine = line;

  silentDepth++;
  Token token;
  for (int i = 0; i < n; i++) {
    token = nextToken();
  }

  silentDepth--;

  pos = savedPos;
  line = savedLine;

  return token;
}

Token Lexer::nextToken() {
  int index;
  skipwhitespace();
  if (pos >= source.length()) {
    return Token(TokenType::EOFTK, "", line);
  }
  if (isdigit(source[pos])) {
    std::string digit;
    for (index = pos; isdigit(source[index]) && index < source.length();
         index++) {
      digit.push_back(source[index]);
    }
    pos = index;
    output("INTCON", digit);
    return Token(TokenType::INTCON, digit, line, std::stoi(digit));

  } else if (isalpha(source[pos]) || source[pos] == '_') {
    std::string word;
    for (index = pos; (isalnum(source[index]) || source[index] == '_') &&
                      index < source.length();
         index++) {
      word.push_back(source[index]);
    }
    pos = index;
    if (reserveWords.find(word) != reserveWords.end()) {
      Token token(reserveWords[word], word, line);
      output(token.getTokenType(), word);
      return token;
    } else {
      output("IDENFR", word);
      return Token(TokenType::IDENFR, word, line, word);
    }
  }
  switch (source[pos]) {
  case '!': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      output("NEQ", "!=");
      return Token(TokenType::NEQ, "!=", line);
    }
    pos++;
    output("NOT", "!");
    return Token(TokenType::NOT, "!", line);
  }
  case '&': {
    if (pos + 1 < source.length() && source[pos + 1] == '&') {
      pos += 2;
      output("AND", "&&");
      return Token(TokenType::AND, "&&", line);
    }
    pos++;
    error(line, "a");
    return Token(TokenType::UNKNOWN, "&", line);
  }
  case '|': {
    if (pos + 1 < source.length() && source[pos + 1] == '|') {
      pos += 2;
      output("OR", "||");
      return Token(TokenType::OR, "||", line);
    }
    pos++;
    error(line, "a");
    return Token(TokenType::UNKNOWN, "|", line);
  }
  case '+': {
    pos++;
    output("PLUS", "+");
    return Token(TokenType::PLUS, "+", line);
  }
  case '-': {
    pos++;
    output("MINU", "-");
    return Token(TokenType::MINU, "-", line);
  }
  case '*': {
    pos++;
    output("MULT", "*");
    return Token(TokenType::MULT, "*", line);
  }
  case '/': {
    if (pos + 1 < source.length() && source[pos + 1] == '*') {
      index = pos + 2;
      while (index + 1 < source.length() &&
             (source[index] != '*' || source[index + 1] != '/')) {
        if (source[index] == '\n') {
          line++;
        }
        index++;
      }
      pos = index + 2;
      return nextToken();
    } else if (pos + 1 < source.length() && source[pos + 1] == '/') {
      pos += 2;
      while (pos < source.length() && source[pos] != '\n') {
        pos++;
      }
      return nextToken();
    }
    pos++;
    output("DIV", "/");
    return Token(TokenType::DIV, "/", line);
  }
  case '%': {
    pos++;
    output("MOD", "%");
    return Token(TokenType::MOD, "%", line);
  }
  case '<': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      output("LEQ", "<=");
      return Token(TokenType::LEQ, "<=", line);
    }
    pos++;
    output("LSS", "<");
    return Token(TokenType::LSS, "<", line);
  }
  case '>': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      output("GEQ", ">=");
      return Token(TokenType::GEQ, ">=", line);
    }
    pos++;
    output("GRE", ">");
    return Token(TokenType::GRE, ">", line);
  }
  case '=': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      output("EQL", "==");
      return Token(TokenType::EQL, "==", line);
    }
    pos++;
    output("ASSIGN", "=");
    return Token(TokenType::ASSIGN, "=", line);
  }
  case ';': {
    pos++;
    output("SEMICN", ";");
    return Token(TokenType::SEMICN, ";", line);
  }
  case ',': {
    pos++;
    output("COMMA", ",");
    return Token(TokenType::COMMA, ",", line);
  }
  case '(': {
    pos++;
    output("LPARENT", "(");
    return Token(TokenType::LPARENT, "(", line);
  }
  case ')': {
    pos++;
    output("RPARENT", ")");
    return Token(TokenType::RPARENT, ")", line);
  }
  case '[': {
    pos++;
    output("LBRACK", "[");
    return Token(TokenType::LBRACK, "[", line);
  }
  case ']': {
    pos++;
    output("RBRACK", "]");
    return Token(TokenType::RBRACK, "]", line);
  }
  case '{': {
    pos++;
    output("LBRACE", "{");
    return Token(TokenType::LBRACE, "{", line);
  }
  case '}': {
    pos++;
    output("RBRACE", "}");
    return Token(TokenType::RBRACE, "}", line);
  }
  case '"': {
    index = pos + 1;
    std::string strcon;
    strcon.push_back('"');
    while (index < source.length() && source[index] != '"') {
      strcon.push_back(source[index]);
      index++;
    }
    strcon.push_back('"');
    pos = index + 1;
    output("STRCON", strcon);
    return Token(TokenType::STRCON, strcon, line, strcon);
  }

  default:
    index = pos;
    std::string unknown;
    while (index < source.length() && source[index] != ' ' &&
           source[index] != '\t' && source[index] != '\n' &&
           source[index] != '\r') {
      unknown.push_back(source[index]);
      index++;
    }
    pos = index;
    error(line, "a");
    return Token(TokenType::UNKNOWN, unknown, line);
  }
}
