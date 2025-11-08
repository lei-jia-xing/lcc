# LCC 编译器 - ErrorReporter 设计文档

## 概述

ErrorReporter（错误报告器）是 LCC 编译器的统一错误收集和输出组件，负责收集、排序和打印所有编译阶段的错误。本文档详细介绍了 ErrorReporter 的设计架构、实现细节和使用方式。

### 设计动机

在最初的编译器设计中，计划采用流式处理架构，即分析到哪里错误就报到哪里。然而，在实现语义分析阶段时发现这种设计行不通，因为语义分析需要完整的 AST 树来进行类型检查和函数调用验证。因此，引入了专门的错误收集器来统一管理和输出错误信息。

## 架构设计

### 类结构

```cpp
/**
 * @file
 * @brief the definition of ErrorReporter class
 */

#pragma once
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

/**
 * @brief 错误报告器类
 * 负责收集、排序和打印所有编译错误
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
```

## 实现细节

### 单例模式

ErrorReporter 采用单例模式，确保整个编译过程中只有一个错误收集器实例：

```cpp
ErrorReporter &ErrorReporter::getInstance() {
  static ErrorReporter instance;
  return instance;
}
```

这种设计的优势：

- **全局访问**：所有编译阶段（Lexer、Parser、Semantic）都能访问同一个错误收集器
- **内存效率**：避免创建多个实例
- **数据一致性**：确保所有错误都存储在同一个容器中

### 错误信息结构

```cpp
struct ErrorInfo {
  int line;
  std::string type;

  ErrorInfo(int l, const std::string &t) : line(l), type(t) {}

  bool operator<(const ErrorInfo &other) const { return line < other.line; }
};
```

### 错误收集机制

```cpp
void ErrorReporter::addError(int line, const std::string &type) {
  errors.emplace_back(line, type);
}
```

### 错误输出机制

```cpp
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
```

- **按行号排序**：错误信息按行号升序排列
- **格式统一**：每行一个错误，格式为 "行号 错误类型"

## 错误类型

### 词法分析错误（Lexer）

- **a: 非法符号**：出现未定义的 Token，如单独的 `&` 或 `|`

### 语法分析错误（Parser）

- **i: 缺少分号**：语句末尾缺少分号
- **j: 缺少右括号')'**：括号不匹配
- **k: 缺少右中括号']'**：中括号不匹配

### 语义分析错误（Semantic）

- **b: 重定义错误**：标识符在同一作用域内重复定义
- **c: 未定义错误**：使用未定义的标识符
- **d: 函数参数数量错误**：函数调用时参数数量不匹配
- **e: 函数参数类型错误**：函数调用时参数类型不匹配
- **f: 返回值错误**：void 函数有返回值或返回值类型不匹配
- **g: 缺少返回语句**：有返回值的函数缺少 return 语句
- **h: 常量赋值错误**：试图给常量赋值
- **l: printf 参数数量不匹配**：格式字符串参数数量与实际参数不符
- **m: 循环控制语句错误**：break/continue 不在循环内使用

## 使用方式

### 在编译器组件中使用

#### Lexer 中的使用

```cpp
void Lexer::error(const int &line, const std::string errorType) {
  ErrorReporter::getInstance().addError(line, errorType);
}
```

#### Parser 中的使用

```cpp
void Parser::error(const int &line, const std::string errorType) {
  ErrorReporter::getInstance().addError(line, errorType);
}
```

#### Semantic Analyzer 中的使用

```cpp
void SemanticAnalyzer::error(const int &line, const std::string errorType) {
  ErrorReporter::getInstance().addError(line, errorType);
}
```

### 在主程序中输出错误

```cpp
int main() {
  // ... 编译过程

  // 输出所有错误
  ErrorReporter::getInstance().printErrors();

  // 根据是否有错误决定返回值
  return ErrorReporter::getInstance().hasErrors() ? 1 : 0;
}
```
