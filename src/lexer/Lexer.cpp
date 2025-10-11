#include "lexer/Lexer.hpp"
#include "lexer/Token.hpp"
#include <iostream>
#include <string>

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
  std::cerr << line << " " << errorType << std::endl;
}

Token Lexer::nextToken() {
  int index;
  skipwhitespace();
  if (pos >= source.length()) {
    return Token(TokenType::END_OF_FILE, "", line);
  }
  if (isdigit(source[pos])) {
    if (source[pos] == '0') {
      pos++;
      return Token(TokenType::INTCON, "0", line, 0);
    }
    std::string digit;
    for (index = pos; isdigit(source[index]) && index < source.length();
         index++) {
      digit.push_back(source[index]);
    }
    pos = index;
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
      return Token(reserveWords[word], word, line);
    } else {
      return Token(TokenType::IDENFR, word, line, word);
    }
  }
  switch (source[pos]) {
  case '!': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      return Token(TokenType::NEQ, "!=", line);
    }
    pos++;
    return Token(TokenType::NOT, "!", line);
  }
  case '&': {
    if (pos + 1 < source.length() && source[pos + 1] == '&') {
      pos += 2;
      return Token(TokenType::AND, "&&", line);
    }
    pos++;
    error(line, "a");
    return Token(TokenType::UNKNOWN, "&", line);
  }
  case '|': {
    if (pos + 1 < source.length() && source[pos + 1] == '|') {
      pos += 2;
      return Token(TokenType::OR, "||", line);
    }
    pos++;
    error(line, "a");
    return Token(TokenType::UNKNOWN, "|", line);
  }
  case '+': {
    pos++;
    return Token(TokenType::PLUS, "+", line);
  }
  case '-': {
    pos++;
    return Token(TokenType::MINU, "-", line);
  }
  case '*': {
    pos++;
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
    return Token(TokenType::DIV, "/", line);
  }
  case '%': {
    pos++;
    return Token(TokenType::MOD, "%", line);
  }
  case '<': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      return Token(TokenType::LEQ, "<=", line);
    }
    pos++;
    return Token(TokenType::LSS, "<", line);
  }
  case '>': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      return Token(TokenType::GEQ, ">=", line);
    }
    pos++;
    return Token(TokenType::GRE, ">", line);
  }
  case '=': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      return Token(TokenType::EQL, "==", line);
    }
    pos++;
    return Token(TokenType::ASSIGN, "=", line);
  }
  case ';': {
    pos++;
    return Token(TokenType::SEMICN, ";", line);
  }
  case ',': {
    pos++;
    return Token(TokenType::COMMA, ",", line);
  }
  case '(': {
    pos++;
    return Token(TokenType::LPARENT, "(", line);
  }
  case ')': {
    pos++;
    return Token(TokenType::RPARENT, ")", line);
  }
  case '[': {
    pos++;
    return Token(TokenType::LBRACK, "[", line);
  }
  case ']': {
    pos++;
    return Token(TokenType::RBRACK, "]", line);
  }
  case '{': {
    pos++;
    return Token(TokenType::LBRACE, "{", line);
  }
  case '}': {
    pos++;
    return Token(TokenType::RBRACE, "}", line);
  }
  case '"': {
    index = pos + 1;
    std::string strcon;
    strcon.push_back('"');
    while (source[index] != '"' && index < source.length()) {
      strcon.push_back(source[index]);
      index++;
    }
    strcon.push_back('"');
    pos = index + 1;
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
