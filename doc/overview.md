# LCC 编译器设计文档

## 概述

LCC (Lightweight C Compiler) 是一个用 C++17 实现的轻量级 C 语言子集编译器,目标架构为 MIPS。本文档详细描述了编译器的整体架构、各组件设计、实现细节

## 编译器架构

### 项目结构

```
lcc/
├── src/                          # 源代码目录
│   ├── lexer/                    # 词法分析器
│   │   └── Lexer.cpp
│   └── parser/                   # 语法分析器
│       └── Parser.cpp
├── include/                      # 头文件目录
│   ├── lexer/
│   │   ├── Lexer.hpp
│   │   └── Token.hpp
│   └── parser/
│       ├── Parser.hpp
│       └── AST.hpp
├── doc/                          # 文档目录
│   └── compiler.md
├── build/                        # 构建目录
├── main.cpp                      # 主程序入口
├── CMakeLists.txt                # 构建配置
└── README.md                     # 项目说明
```

## 词法分析器 (Lexer)

### 设计目标

词法分析器负责将源代码字符流转换为 Token 流，为语法分析器提供输入。支持关键字、标识符、运算符、分隔符和字面量的识别。

### 词法规则

| 类别 | 模式 | Token 类型 | 示例 |
|------|------|------------|------|
| 标识符 | `[a-zA-Z_][a-zA-Z0-9_]*` | `IDENFR` | `count`, `_temp` |
| 整型常量 | `[0-9]+` | `INTCON` | `42`, `0` |
| 字符串常量 | `"[^"]*"` | `STRCON` | `"Hello"` |
| 关键字 | 预定义 | `CONSTTK`, `INTTK` 等 | `const`, `int` |
| 运算符 | 预定义模式 | `PLUS`, `ASSIGN` 等 | `+`, `=` |

## 语法分析器 (Parser)

### 设计目标

语法分析器采用递归下降解析法，将 Token 流转换为抽象语法树 (AST)。支持完整的语法分析、错误恢复和 AST 构建。

### AST 设计

#### 1. AST 节点层次结构

```
ASTNode (基类)
├── CompUnit (编译单元)
├── Decl (声明)
│   ├── ConstDecl (常量声明)
│   └── VarDecl (变量声明)
├── FuncDef (函数定义)
├── MainFuncDef (主函数定义)
├── Stmt (语句)
│   ├── AssignStmt (赋值语句)
│   ├── ExpStmt (表达式语句)
│   ├── BlockStmt (复合语句)
│   ├── IfStmt (条件语句)
│   ├── ForStmt (循环语句)
│   ├── BreakStmt (break语句)
│   ├── ContinueStmt (continue语句)
│   ├── ReturnStmt (return语句)
│   └── PrintfStmt (printf语句)
└── Exp (表达式)
    ├── LVal (左值)
    ├── PrimaryExp (基本表达式)
    ├── UnaryExp (一元表达式)
    ├── MulExp (乘除模表达式)
    ├── AddExp (加减表达式)
    ├── RelExp (关系表达式)
    ├── EqExp (相等性表达式)
    ├── LAndExp (逻辑与表达式)
    └── LOrExp (逻辑或表达式)
```

#### 2. 内存管理

- 使用智能指针 (`std::unique_ptr`) 管理 AST 节点生命周期
- 采用移动语义传递 AST 节点，避免不必要的拷贝
- 所有 AST 节点都继承自 `ASTNode` 基类

### 解析策略

#### 1. 递归下降解析

- 每个语法规则对应一个解析函数
- 左递归消除：通过改写语法规则避免左递归
- 运算符优先级：通过语法层次结构实现

#### 2. 错误恢复

- **局部话处理**: 遇到错误直接忽略，假装他是正确的，继续往下解析
- **静默解析**: 错误期间静默临时解析，避免级联错误

#### 3. 前瞻解析

- 利用 `peekToken()` 进行多 Token 前瞻
- 支持歧义消解（如赋值语句与表达式语句的区分）
- 预检查语法构造的合法性

## 编译流程

### 主程序流程

```cpp
int main() {
    // 1. 文件 I/O 重定向
    std::ifstream inputfile("testfile.txt");
    std::string fileContent = /* 读取文件 */;

    // 2. 词法分析
    Lexer lexer(fileContent);
    auto firstToken = lexer.nextToken();

    // 3. 语法分析
    Parser parser(std::move(lexer), firstToken);
    auto compUnit = parser.parseCompUnit();

    // 4. (未来) 语义分析与代码生成

    return 0;
}
```

### 输出格式

- **词法分析输出**: `Token类型 值` 格式，输出到 `parser.txt`
- **语法分析输出**: `非终结符名称` 格式，输出到 `parser.txt`
- **错误输出**: `行号 错误类型` 格式，输出到 `error.txt`

## 构建系统

### CMake 配置

```cmake
cmake_minimum_required(VERSION 3.10)
project(Compiler)
set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR}/include)

# 词法分析器库
add_library(lexer lexer/Lexer.cpp)

# 语法分析器库
add_library(parser parser/Parser.cpp)

# 主程序
add_executable(Compiler main.cpp)

target_link_libraries(Compiler lexer parser)
```

### 构建步骤

```bash
mkdir build && cd build
cmake ..
make
./Compiler
```

## 开发指南

### 代码风格

- 使用 Doxygen 风格注释
- 遵循 Google C++ 编码规范
- 使用现代 C++ 特性 (C++17)

# LCC 编译器 - Lexer 设计文档

## 概述

Lexer（词法分析器）是 LCC 编译器的第一个阶段，负责将源代码字符串转换为一系列的 Token 流。本文档详细介绍了 Lexer 的设计架构、实现细节和使用方式。

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
  X(EOFTK)                                                                     \
  X(UNKNOWN)
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

此`lexer`设计了一个名为`silentDepth`的静默深度变量（也可以说是静默引用变量）,为什么需要设置成引用计数而不是一个bool数呢？
因为lcc的前端目标是设计成流式处理的，在`Parser`使用向前看过程中，我们必须要要把静默模式关掉，同时为了可拓展性，我们允许
多层静默嵌套,只有当`silentDepth`为`0`时，才允许输出和错误打印。

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

lexer 主要识别以下语法错误：

- **a: 非法符号** :出现未定义的Token,比如&或者|

## 文档补充

做到语义分析的时候，我预想的流式编译器设计似乎行不通了,想要实现分析到哪里
就报错到哪里，这样错误也不用排序.

并且之前是没有做输出的开关的，语义分析需要关闭，因此又做了一个极其简单
的开关来控制输出

# LCC 编译器 - Parser 设计文档

## 概述

Parser（语法分析器）是 LCC 编译器的第二个阶段，负责将 Lexer 生成的 Token 流转换为抽象语法树（AST）。本文档详细介绍了 Parser 的设计架构、实现细节和使用方式。

## 架构设计

### 类结构

```cpp
/**
 * @class Parser
 * @brief a class to parse tokens into an AST
 *
 */
class Parser {
private:
  /**
   * @brief lexer for parser to get tokens
   */
  Lexer lexer;
  /**
   * @brief current token being processed and output
   */
  Token current;
  /**
   * @brief the last line number of a non-terminal being processed
   */
  int lastVnline = 0;

  /**
   * @brief the output enabled flag
   */
  bool outputEnabled = false;
  /**
   * @brief to the next token
   */
  void advance();
  /**
   * @brief a function to check whether the next token is in the expected set
   *
   * @param types expected token types
   * @param errorType if not matched, report this error type
   * @return if matched, return true and advance to the next token; else report
   * the error and return false
   */
  bool expect(const std::vector<TokenType> &types,
              const std::string &errorType);
  inline static int silentDepth = 0;

public:
  /**
   * @brief a function to report an error
   *
   * @param line report line number
   * @param errorType report error type
   */
  void error(const int &line, const std::string errorType);
  /**
   * @brief a constructor of Parser
   *
   * @param lexer lexer to get stream of tokens
   * @param current the first token from lexer
   */
  explicit Parser(Lexer &&lexer, Token current);
  /**
   * @brief a function to control whether to output the parse tree
   *
   * @param silent whether to silent the output
   */
  void silentPV(bool silent);
  /**
   * @brief output the current non-terminal being processed
   *
   * @param type the output non-terminal
   */
  void output(const std::string &type);
  // 编译单元 CompUnit → {Decl} {FuncDef} MainFuncDef
  std::unique_ptr<CompUnit> parseCompUnit();

private:
  std::unique_ptr<IfStmt> parseIfStmt();
  std::unique_ptr<ForStmt> parseForStmt();
  std::unique_ptr<BreakStmt> parseBreakStmt();
  std::unique_ptr<ContinueStmt> parseContinueStmt();
  std::unique_ptr<ReturnStmt> parseReturnStmt();
  std::unique_ptr<PrintfStmt> parsePrintfStmt();
  std::unique_ptr<AssignStmt> parseAssignStmt();
  std::unique_ptr<ExpStmt> parseExpStmt();
  std::unique_ptr<BlockStmt> parseBlockStmt();

private:
  // 声明 Decl → ConstDecl | VarDecl
  std::unique_ptr<Decl> parseDecl();

  // 常量声明 ConstDecl → 'const' BType ConstDef { ',' ConstDef } ';'
  std::unique_ptr<ConstDecl> parseConstDecl();

  // 变量声明 VarDecl → [ 'static' ] BType VarDef { ',' VarDef } ';'
  std::unique_ptr<VarDecl> parseVarDecl();

  // 基本类型 BType → 'int'
  std::unique_ptr<BType> parseBType();

  // 常量定义 ConstDef → Ident [ '[' ConstExp ']' ] '=' ConstInitVal
  std::unique_ptr<ConstDef> parseConstDef();

  // 变量定义 VarDef → Ident [ '[' ConstExp ']' ] | Ident [ '[' ConstExp ']' ]
  // '=' InitVal
  std::unique_ptr<VarDef> parseVarDef();

  // 常量初值 ConstInitVal → ConstExp | '{' [ ConstExp { ',' ConstExp } ] '}'
  std::unique_ptr<ConstInitVal> parseConstInitVal();

  // 变量初值 InitVal → Exp | '{' [ Exp { ',' Exp } ] '}'
  std::unique_ptr<InitVal> parseInitVal();

  // 函数定义 FuncDef → FuncType Ident '(' [FuncFParams] ')' Block
  std::unique_ptr<FuncDef> parseFuncDef();

  // 主函数定义 MainFuncDef → 'int' 'main' '(' ')' Block
  std::unique_ptr<MainFuncDef> parseMainFuncDef();

  // 函数类型 FuncType → 'void' | 'int'
  std::unique_ptr<FuncType> parseFuncType();

  // 函数形参表 FuncFParams → FuncFParam { ',' FuncFParam }
  std::unique_ptr<FuncFParams> parseFuncFParams();

  // 函数形参 FuncFParam → BType Ident ['[' ']']
  std::unique_ptr<FuncFParam> parseFuncFParam();

  // 语句块 Block → '{' { BlockItem } '}'
  std::unique_ptr<Block> parseBlock();

  // 语句块项 BlockItem → Decl | Stmt
  std::unique_ptr<BlockItem> parseBlockItem();

  // 语句 Stmt → LVal '=' Exp ';'
  //   | [Exp] ';'
  //   | Block
  //   | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
  //   | 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt
  //   | 'break' ';' | 'continue' ';'
  //   | 'return' [Exp] ';'
  //   | 'printf''('StringConst {','Exp}')'';'
  std::unique_ptr<Stmt> parseStmt();

  // 语句 ForStmt → LVal '=' Exp { ',' LVal '=' Exp }
  std::unique_ptr<ForAssignStmt> parseForAssignStmt();

  // 表达式 Exp → AddExp
  std::unique_ptr<Exp> parseExp();

  // 条件表达式 Cond → LOrExp
  std::unique_ptr<Cond> parseCond();

  // 左值表达式 LVal → Ident ['[' Exp ']']
  std::unique_ptr<LVal> parseLVal();

  // 基本表达式 PrimaryExp → '(' Exp ')' | LVal | Number
  std::unique_ptr<PrimaryExp> parsePrimaryExp();

  // 数值 Number → IntConst
  std::unique_ptr<Number> parseNumber();

  // 一元表达式 UnaryExp → PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp
  // UnaryExp
  std::unique_ptr<UnaryExp> parseUnaryExp();

  // 单目运算符 UnaryOp → '+' | '−' | '!'
  std::unique_ptr<UnaryOp> parseUnaryOp();

  // 函数实参表达式 FuncRParams → Exp { ',' Exp }
  std::unique_ptr<FuncRParams> parseFuncRParams();

  // 乘除模表达式 MulExp → UnaryExp | MulExp ('*' | '/' | '%') UnaryExp
  std::unique_ptr<MulExp> parseMulExp();

  // 加减表达式 AddExp → MulExp | AddExp ('+' | '−') MulExp
  std::unique_ptr<AddExp> parseAddExp();

  // 关系表达式 RelExp → AddExp | RelExp ('<' | '>' | '<=' | '>=') AddExp
  std::unique_ptr<RelExp> parseRelExp();

  // 相等性表达式 EqExp → RelExp | EqExp ('==' | '!=') RelExp
  std::unique_ptr<EqExp> parseEqExp();

  // 逻辑与表达式 LAndExp → EqExp | LAndExp '&&' EqExp
  std::unique_ptr<LAndExp> parseLAndExp();

  // 逻辑或表达式 LOrExp → LAndExp | LOrExp '||' LAndExp
  std::unique_ptr<LOrExp> parseLOrExp();

  // 常量表达式 ConstExp → AddExp
  std::unique_ptr<ConstExp> parseConstExp();
};
```

## 实现细节

### AST 节点设计

使用继承体系构建 AST，所有节点都继承自 `ASTNode` 基类：

```cpp
class ASTNode {
public:
  virtual ~ASTNode() = default;
  int line = 0;  // 记录源代码行号
};
```

### 递归下降解析策略

1. **自顶向下解析**：从文法开始符号 `CompUnit` 开始，递归调用各子解析函数
2. **前向查看**：使用 `lexer.peekToken(1)` 进行单 Token 前瞻，避免回溯
3. **左递归消除**：将左递归文法转换为右递归，适合递归下降解析

### 错误处理机制

#### 期望检查（Expect）

```cpp
bool Parser::expect(const std::vector<TokenType> &types, const std::string &errorType) {
  // 检查当前 Token 是否在期望的类型列表中
  // 如果不匹配，报告错误并返回 false
  // 如果匹配，消耗 Token 并返回 true
}
```

### 静默模式

与 Lexer 类似，Parser 也实现了静默模式机制：

```cpp
void Parser::silentPV(bool silent) {
  if (silent) {
    silentDepth++;
    lexer.silentPV(true);
  } else {
    silentDepth > 0 ? silentDepth-- : silentDepth = 0;
    lexer.silentPV(false);
  }
}
```

静默模式用于：

- 解析过程中的临时分析
- 错误恢复期间的 Token 跳过
- 前瞻性解析时的临时禁用输出

## 解析流程

### 主流程

1. **初始化**：构造 Parser 对象，传入 Lexer 和初始 Token
2. **解析编译单元**：调用 `parseCompUnit()` 开始解析
3. **递归解析**：根据当前 Token 类型调用相应的解析函数
4. **构建 AST**：每个解析函数返回对应的 AST 节点
5. **错误处理**：遇到语法错误时报告并尝试恢复
6. **输出结果**：输出语法分析结果的产生式

### 语句解析策略

由于部分语句存在二义性（如标识符开头的可能是赋值语句或表达式语句），Parser 采用以下策略向前看，并在这个过程中
启动静默模式以避免不必要的输出，判断出要解析的语句类型后再恢复状态，按正确的语句类型进行解析：

```cpp
if (current.type == TokenType::IDENFR) {
  // 临时保存状态
  auto temp = lexer;
  auto tempCurrent = current;
  silentPV(true);

  // 尝试解析左值
  parseLVal();
  advance();

  // 检查是否为赋值操作
  if (current.type == TokenType::ASSIGN) {
    // 恢复状态，按赋值语句解析
    lexer = temp;
    current = tempCurrent;
    silentPV(false);
    return parseAssignStmt();
  } else {
    // 恢复状态，按表达式语句解析
    lexer = temp;
    current = tempCurrent;
    silentPV(false);
    return parseExpStmt();
  }
}
```

## 错误处理

### 错误类型

Parser 主要识别以下语法错误：

- **i 类型错误**：缺少分号
- **j 类型错误**： 缺少右括号')'
- **k 类型错误**：缺少右中括号']'

## 与其他组件的交互

### 与 Lexer 的交互

- **Token 获取**：通过 `lexer.nextToken()` 获取下一个 Token
- **前向查看**：通过 `lexer.peekToken(n)` 查看第 n 个未来的 Token
- **静默同步**：通过 `silentPV()` 方法与 Lexer 的静默模式同步

## 文档补充

做到语义分析的时候，我预想的流式编译器设计似乎行不通了,还是需要扫描
完整的AST树，因为在做Parser的时候，我根本没有想过类型相关的东西,因此
我在AST节点中加入了类型信息，这部分的内容由语义分析来填充。

并且之前是没有做输出的开关的，语义分析需要关闭，因此又做了一个极其简单
的开关来控制输出,另外语义分析是需要其他的信息的，比如函数名的所在行数,
'}'的所在行数，因此AST树又补充了一点信息。
