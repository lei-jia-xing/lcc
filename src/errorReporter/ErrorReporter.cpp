#include "errorReporter/ErrorReporter.hpp"
#include <algorithm>

ErrorReporter &ErrorReporter::getInstance() {
  static ErrorReporter instance;
  return instance;
}

void ErrorReporter::addError(int line, const std::string &type) {
  errors.emplace_back(line, type);
}

void ErrorReporter::printErrors(std::ostream &outputStream) const {
  if (errors.empty()) {
    return;
  }

  // 按行号排序
  std::vector<ErrorInfo> sortedErrors = errors;
  std::sort(sortedErrors.begin(), sortedErrors.end());

  // 打印错误
  for (const auto &error : sortedErrors) {
    outputStream << error.line << " " << error.type << std::endl;
  }
}
