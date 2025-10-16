# LCC 编译器 - Lexer 设计文档

## 概述

Lexer（词法分析器）是 LCC 编译器的第一个阶段，负责将源代码字符串转换为一系列的 Token 流。本文档详细介绍了 Lexer 的设计架构、实现细节和使用方式。

## 功能特性

- **多类型 Token 支持**：标识符、整型常量、字符串常量、运算符、分隔符
- **注释处理**：支持单行注释 `//` 和多行注释 `/* */`
- **错误处理**：非法字符检测与错误报告
- **预读机制**：支持 Token 的前瞻性查看
- **静默模式**：支持语法分析阶段的临时静默以禁用输出和错误打印

## 架构设计

### 核心组件

```
Lexer
├── Token (Token 定义)
├── TokenType (Token 类型枚举)
├── 保留字表 (reserveWords)
└── 词法分析引擎
```

### 类结构

```cpp
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
```

## 实现细节

### Token 类型定义

使用宏定义自动生成 Token 类型枚举，确保代码的一致性和可维护性：

```cpp
#define TOKEN_LIST \
    X(IDENFR)    // 标识符
    X(INTCON)    // 整型常量
    X(STRCON)    // 字符串常量
    X(CONSTTK)   // const 关键字
    X(INTTK)     // int 关键字
    // ... 更多 Token 类型
```

### 词法分析流程

1. **Token 识别**
   - **数字识别**：连续数字字符组成整型常量
   - **标识符/关键字**：字母、下划线开头，后跟字母数字下划线
   - **运算符识别**：支持单字符和双字符运算符
   - **字符串常量**：双引号包围的字符序列
   - **分隔符识别**：括号、分号、逗号等
   - 注释自动跳过：直接获取下一个Token

2. **错误处理**
   - 非法字符识别（如单独的 `&`、`|`）
   - 输出错误信息到 `stderr`

### 静默模式

此Parser设计了一个名为`silentDepth`的静默深度变量（也可以说是静默引用变量）,为什么需要设置设个引用计数呢？
因为lcc的前端目标是设计成流式的，在Parser使用向前看过程中，我们必须要要把静默模式关掉，同时为了可拓展性，
我们允许多层静默嵌套,只有当silentDepth为0时，才允许输出和错误打印
```c++
void Lexer::silentPV(bool silent) {
  if (silent) {
    silentDepth++;
  } else {
    silentDepth > 0 ? silentDepth-- : silentDepth = 0;
  }
}
```

## 错误处理

### 错误类型

Lexer 主要识别以下错误：

- **非法字符**（错误类型 'a'）
  - 单独的 `&` 字符
  - 单独的 `|` 字符
  - 其他无法识别的字符序列

### 错误报告格式

```
<行号> <错误类型>
```

示例：

```
5 a
```

表示第 5 行出现非法字符错误。

