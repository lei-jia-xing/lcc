#include "lexer/Lexer.hpp"
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
  std::cerr << line << " " << errorType << std::endl;
}

Token Lexer::nextToken() {
  int index;
  skipwhitespace();
  if (pos >= source.length()) {
    return Token(TokenType::END_OF_FILE, "", line);
  }
  if (isdigit(source[pos])) {
    std::string digit;
    for (index = pos; isdigit(source[index]) && index < source.length();
         index++) {
      digit.push_back(source[index]);
    }
    pos = index;
    std::cout << "INTCON" << " " << digit << std::endl;
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
      auto token = Token(reserveWords[word], word, line);
      std::cout << token.lexeme << " " << word << std::endl;
      return token;
    } else {
      std::cout << "IDENFR" << " " << word << std::endl;
      return Token(TokenType::IDENFR, word, line, word);
    }
  }
  switch (source[pos]) {
  case '!': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      std::cout << "NEQ" << " " << "!=" << std::endl;
      return Token(TokenType::NEQ, "!=", line);
    }
    pos++;
    std::cout << "NOT" << " " << "!" << std::endl;
    return Token(TokenType::NOT, "!", line);
  }
  case '&': {
    if (pos + 1 < source.length() && source[pos + 1] == '&') {
      pos += 2;
      std::cout << "AND" << " " << "&&" << std::endl;
      return Token(TokenType::AND, "&&", line);
    }
    pos++;
    error(line, "a");
    return Token(TokenType::UNKNOWN, "&", line);
  }
  case '|': {
    if (pos + 1 < source.length() && source[pos + 1] == '|') {
      pos += 2;
      std::cout << "OR" << " " << "||" << std::endl;
      return Token(TokenType::OR, "||", line);
    }
    pos++;
    error(line, "a");
    return Token(TokenType::UNKNOWN, "|", line);
  }
  case '+': {
    pos++;
    std::cout << "PLUS" << " " << "+" << std::endl;
    return Token(TokenType::PLUS, "+", line);
  }
  case '-': {
    pos++;
    std::cout << "MINU" << " " << "-" << std::endl;
    return Token(TokenType::MINU, "-", line);
  }
  case '*': {
    pos++;
    std::cout << "MULT" << " " << "*" << std::endl;
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
    std::cout << "DIV" << " " << "/" << std::endl;
    return Token(TokenType::DIV, "/", line);
  }
  case '%': {
    pos++;
    std::cout << "MOD" << " " << "%" << std::endl;
    return Token(TokenType::MOD, "%", line);
  }
  case '<': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      std::cout << "LEQ" << " " << "<=" << std::endl;
      return Token(TokenType::LEQ, "<=", line);
    }
    pos++;
    std::cout << "LSS" << " " << "<" << std::endl;
    return Token(TokenType::LSS, "<", line);
  }
  case '>': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      std::cout << "GEQ" << " " << ">=" << std::endl;
      return Token(TokenType::GEQ, ">=", line);
    }
    pos++;
    std::cout << "GRE" << " " << ">" << std::endl;
    return Token(TokenType::GRE, ">", line);
  }
  case '=': {
    if (pos + 1 < source.length() && source[pos + 1] == '=') {
      pos += 2;
      std::cout << "EQL" << " " << "==" << std::endl;
      return Token(TokenType::EQL, "==", line);
    }
    pos++;
    std::cout << "ASSIGN" << " " << "=" << std::endl;
    return Token(TokenType::ASSIGN, "=", line);
  }
  case ';': {
    pos++;
    std::cout << "SEMICN" << " " << ";" << std::endl;
    return Token(TokenType::SEMICN, ";", line);
  }
  case ',': {
    pos++;
    std::cout << "COMMA" << " " << "," << std::endl;
    return Token(TokenType::COMMA, ",", line);
  }
  case '(': {
    pos++;
    std::cout << "LPARENT" << " " << "(" << std::endl;
    return Token(TokenType::LPARENT, "(", line);
  }
  case ')': {
    pos++;
    std::cout << "RPARENT" << " " << ")" << std::endl;
    return Token(TokenType::RPARENT, ")", line);
  }
  case '[': {
    pos++;
    std::cout << "LBRACK" << " " << "[" << std::endl;
    return Token(TokenType::LBRACK, "[", line);
  }
  case ']': {
    pos++;
    std::cout << "RBRACK" << " " << "]" << std::endl;
    return Token(TokenType::RBRACK, "]", line);
  }
  case '{': {
    pos++;
    std::cout << "LBRACE" << " " << "{" << std::endl;
    return Token(TokenType::LBRACE, "{", line);
  }
  case '}': {
    pos++;
    std::cout << "RBRACE" << " " << "}" << std::endl;
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
    std::cout << "STRCON" << " " << strcon << std::endl;
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
