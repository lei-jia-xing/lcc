/**
 * @file
 * @brief the definition of ErrorReporter class
 */

#pragma once
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

class ErrorReporter {
public:
  /**
   * @brief get singleton instance of ErrorReporter
   *
   * @return instance reference
   */
  static ErrorReporter &getInstance();

  /**
   * @brief add an error record
   *
   * @param line error line number
   * @param type error type description
   */
  void addError(int line, const std::string &type);

  /**
   * @brief print all errors to the given output stream
   *
   * @param outputStream given output stream (default: std::cerr)
   */
  void printErrors(std::ostream &outputStream = std::cerr) const;

  /**
   * @brief check if there are any errors recorded
   *
   * @return true if there are errors, false otherwise
   */
  bool hasError() const { return !errors.empty(); }

private:
  ErrorReporter() = default;
  ~ErrorReporter() = default;

  struct ErrorInfo {
    int line;
    std::string type;

    ErrorInfo(int l, const std::string &t) : line(l), type(t) {}

    bool operator<(const ErrorInfo &other) const { return line < other.line; }
  };

  std::vector<ErrorInfo> errors;
};
