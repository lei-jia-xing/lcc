#pragma once
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

/**
 * @brief 错误报告器类
 * 负责收集、排序和打印所有编译错误
 * 供 lexer、parser 和 semantic 共同使用
 */
class ErrorReporter {
public:
  /**
   * @brief 获取单例实例
   */
  static ErrorReporter &getInstance();

  /**
   * @brief 添加一个错误
   * @param line 行号
   * @param type 错误类型
   */
  void addError(int line, const std::string &type);

  /**
   * @brief 打印所有错误（按行号排序）
   * @param outputStream 输出流，默认为std::cerr
   */
  void printErrors(std::ostream &outputStream = std::cerr) const;

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
