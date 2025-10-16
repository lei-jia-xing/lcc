# LCC 编译器 - Lexer 设计文档

## 概述

Lexer（词法分析器）是 LCC 编译器的第一个阶段，负责将源代码字符串转换为一系列的 Token 流。本文档详细介绍了 Lexer 的设计架构、实现细节和使用方式。

## 功能特性

- **多类型 Token 支持**：标识符、整型常量、字符串常量、运算符、分隔符
- **注释处理**：支持单行注释 `//` 和多行注释 `/* */`
- **错误处理**：非法字符检测与错误报告
- **预读机制**：支持 Token 的前瞻性查看
- **静默模式**：支持语法分析阶段的临时静默输出

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
    std::string source;    // 源代码字符串
    size_t pos;           // 当前位置
    int line;             // 当前行号

    static std::unordered_map<std::string, TokenType> reserveWords;

public:
    Token nextToken();     // 获取下一个 Token
    Token peekToken(int n); // 预读第 n 个 Token
    void skipwhitespace(); // 跳过空白字符
    // ... 其他方法
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

1. **预处理阶段**
   - 跳过空白字符（空格、制表符、换行符等）
   - 维护当前行号计数

2. **Token 识别**
   - **数字识别**：连续数字字符组成整型常量
   - **标识符/关键字**：字母、下划线开头，后跟字母数字下划线
   - **运算符识别**：支持单字符和双字符运算符
   - **字符串常量**：双引号包围的字符序列
   - **分隔符识别**：括号、分号、逗号等

3. **错误处理**
   - 非法字符识别（如单独的 `&`、`|`）
   - 输出错误信息到 `stderr`

### 关键算法

#### 数字识别算法

```cpp
if (isdigit(source[pos])) {
    std::string digit;
    for (index = pos; isdigit(source[index]) && index < source.length(); index++) {
        digit.push_back(source[index]);
    }
    pos = index;
    return Token(TokenType::INTCON, digit, line, std::stoi(digit));
}
```

#### 标识符/关键字识别算法

```cpp
if (isalpha(source[pos]) || source[pos] == '_') {
    std::string word;
    // 收集所有合法的标识符字符
    for (index = pos; (isalnum(source[index]) || source[index] == '_') && index < source.length(); index++) {
        word.push_back(source[index]);
    }
    pos = index;
    // 检查是否为关键字
    if (reserveWords.find(word) != reserveWords.end()) {
        return Token(reserveWords[word], word, line);
    } else {
        return Token(TokenType::IDENFR, word, line, word);
    }
}
```

#### 注释处理算法

```cpp
case '/': {
    if (pos + 1 < source.length() && source[pos + 1] == '*') {
        // 多行注释处理
        index = pos + 2;
        while (index + 1 < source.length() &&
               (source[index] != '*' || source[index + 1] != '/')) {
            if (source[index] == '\n') line++;
            index++;
        }
        pos = index + 2;
        return nextToken(); // 递归调用继续分析
    } else if (pos + 1 < source.length() && source[pos + 1] == '/') {
        // 单行注释处理
        pos += 2;
        while (pos < source.length() && source[pos] != '\n') {
            pos++;
        }
        return nextToken(); // 递归调用继续分析
    }
    // 除法运算符处理
}
```

## 使用示例

### 基本用法

```cpp
#include "lexer/Lexer.hpp"

int main() {
    std::string source = "int main() { return 0; }";
    Lexer lexer(source);

    Token token;
    while ((token = lexer.nextToken()).type != TokenType::EOFTK) {
        std::cout << token.getTokenType() << " " << token.lexeme << std::endl;
    }

    return 0;
}
```

### 预读机制使用

```cpp
Lexer lexer("int a = 1 + 2;");

// 获取当前 Token
Token current = lexer.nextToken(); // int

// 预读下一个 Token，不移动位置
Token next = lexer.peekToken(1);   // IDENFR "a"

// 再次预读
Token next2 = lexer.peekToken(2);  // ASSIGN "="
```

## 🔍 错误处理

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

## 性能特性

- **线性时间复杂度**：O(n)，其中 n 为源代码长度
- **空间复杂度**：O(1)，只维护少量状态变量
- **预读优化**：`peekToken` 使用状态保存和恢复，避免重复分析
- **静态哈希表**：关键字查找使用 `unordered_map`，保证 O(1) 查找时间

## 配置选项

### 静默模式

Lexer 支持静默模式，用于语法分析阶段的临时 Token 分析：

```cpp
lexer.silentPV(true);  // 开启静默模式
lexer.silentPV(false); // 关闭静默模式
```

在静默模式下，Token 输出和错误报告都会被抑制。

