<!--toc:start-->
- [LCC 编译器设计文档](#lcc-编译器设计文档)
  - [项目结构](#项目结构)
  - [概述](#概述)
  - [编译流水线](#编译流水线)
  - [构建系统概览](#构建系统概览)
  - [模块文档索引](#模块文档索引)
- [LCC 编译器 - Lexer 设计文档](#lcc-编译器-lexer-设计文档)
  - [概述](#概述)
    - [类结构](#类结构)
  - [实现细节](#实现细节)
    - [Token 类型定义](#token-类型定义)
    - [词法分析流程](#词法分析流程)
    - [静默模式](#静默模式)
  - [错误处理](#错误处理)
    - [错误类型](#错误类型)
  - [文档补充](#文档补充)
- [LCC 编译器 - Parser 设计文档](#lcc-编译器-parser-设计文档)
  - [概述](#概述)
  - [架构设计](#架构设计)
    - [类结构](#类结构)
  - [实现细节](#实现细节)
    - [AST 节点设计](#ast-节点设计)
    - [递归下降解析策略](#递归下降解析策略)
    - [错误处理机制](#错误处理机制)
      - [期望检查（Expect）](#期望检查expect)
    - [静默模式](#静默模式)
  - [解析流程](#解析流程)
    - [主流程](#主流程)
    - [语句解析策略](#语句解析策略)
  - [错误处理](#错误处理)
    - [错误类型](#错误类型)
  - [与其他组件的交互](#与其他组件的交互)
    - [与 Lexer 的交互](#与-lexer-的交互)
  - [文档补充](#文档补充)
- [LCC 编译器 - Semantic Analyzer 设计文档](#lcc-编译器-semantic-analyzer-设计文档)
  - [概述](#概述)
  - [架构设计](#架构设计)
    - [类结构](#类结构)
  - [核心组件](#核心组件)
    - [1. 类型系统](#1-类型系统)
      - [基础类型](#基础类型)
      - [类型分类](#类型分类)
      - [类型类设计](#类型类设计)
    - [2. 符号表（Symbol Table）](#2-符号表symbol-table)
      - [符号定义](#符号定义)
      - [作用域管理](#作用域管理)
  - [语义分析流程](#语义分析流程)
    - [整体分析流程](#整体分析流程)
  - [错误处理](#错误处理)
    - [错误类型](#错误类型)
  - [与其他组件的交互](#与其他组件的交互)
    - [与 Parser 的交互](#与-parser-的交互)
- [LCC编译器 - IR设计文档](#lcc编译器-ir设计文档)
  - [设计总览](#设计总览)
  - [操作数（Operand）](#操作数operand)
  - [指令集（OpCode）](#指令集opcode)
    - [算术 / 比较 / 逻辑](#算术-比较-逻辑)
    - [数据移动与内存访问](#数据移动与内存访问)
    - [控制流](#控制流)
    - [函数调用](#函数调用)
  - [基本块与 CFG](#基本块与-cfg)
  - [函数结构与模块接口](#函数结构与模块接口)
  - [默认优化 Pass](#默认优化-pass)
  - [示例](#示例)
    - [条件语句](#条件语句)
    - [函数调用](#函数调用)
  - [与后端交互要点](#与后端交互要点)
- [LCC编译器 - backend 设计文档](#lcc编译器-backend-设计文档)
  - [模块概览](#模块概览)
  - [2. 数据段生成（emitDataSection）](#2-数据段生成emitdatasection)
    - [字符串常量](#字符串常量)
    - [全局变量](#全局变量)
  - [文本段生成（emitTextSection）](#文本段生成emittextsection)
  - [寄存器分配（RegisterAllocator）](#寄存器分配registerallocator)
    - [Use/Def 与活跃性分析](#usedef-与活跃性分析)
    - [冲突图构建](#冲突图构建)
    - [图着色与溢出](#图着色与溢出)
  - [AsmGen 中的寄存器与栈帧策略](#asmgen-中的寄存器与栈帧策略)
    - [寄存器分类](#寄存器分类)
    - [栈帧布局](#栈帧布局)
    - [序言与尾声](#序言与尾声)
  - [IR 指令到 MIPS 的映射（lowerInstruction）](#ir-指令到-mips-的映射lowerinstruction)
    - [寄存器获取与结果存储](#寄存器获取与结果存储)
    - [典型指令映射](#典型指令映射)
  - [运行时辅助例程](#运行时辅助例程)
<!--toc:end-->

# LCC 编译器设计文档

## 项目结构

下面的目录树由实际仓库生成，反映当前编译器完整的项目布局：

```
.
├── CMakeLists.txt
├── config.json
├── doc
│   ├── backend.md
│   ├── compiler.md
│   ├── ebnf.md
│   ├── error.md
│   ├── errorReporter.md
│   ├── ir.md
│   ├── lexer.md
│   ├── overview.md
│   ├── parser.md
│   └── semantic.md
├── Doxyfile
├── include
│   ├── backend
│   │   ├── AsmGen.hpp
│   │   └── RegisterAllocator.hpp
│   ├── codegen
│   │   ├── BasicBlock.hpp
│   │   ├── CodeGen.hpp
│   │   ├── Function.hpp
│   │   ├── Instruction.hpp
│   │   ├── Operand.hpp
│   │   └── QuadOptimizer.hpp
│   ├── errorReporter
│   │   └── ErrorReporter.hpp
│   ├── lexer
│   │   ├── Lexer.hpp
│   │   └── Token.hpp
│   ├── parser
│   │   ├── AST.hpp
│   │   └── Parser.hpp
│   └── semantic
│       ├── SemanticAnalyzer.hpp
│       ├── Symbol.hpp
│       ├── SymbolTable.hpp
│       └── Type.hpp
├── LICENSE
├── main.cpp
├── MARS2025+.jar
├── README.md
├── scripts
│   └── test_mips.sh
└── src
    ├── backend
    │   ├── AsmGen.cpp
    │   └── RegisterAllocator.cpp
    ├── CMakeLists.txt
    ├── codegen
    │   ├── BasicBlock.cpp
    │   ├── CodeGen.cpp
    │   ├── Function.cpp
    │   ├── Instruction.cpp
    │   ├── Operand.cpp
    │   └── QuadOptimizer.cpp
    ├── errorReporter
    │   └── ErrorReporter.cpp
    ├── lexer
    │   └── Lexer.cpp
    ├── parser
    │   └── Parser.cpp
    └── semantic
        └── SemanticAnalyzer.cpp
```

## 概述

LCC (Lightweight C Compiler) 是一个用 C++17 实现的轻量级 C 语言子集编译器，目标架构为 MIPS。项目采用典型的多阶段编译流水线：

- 前端：Lexer（词法分析）→ Parser（语法分析）→ SemanticAnalyzer（语义分析）
- 中间层：AST → 中间表示 IR（四元式形式），见 `ir.md`
- 后端：CodeGen（IR 生成与简单优化）→ AsmGen + RegisterAllocator（MIPS 汇编生成），见 `backend.md`

本文件作为顶层总览，给出模块划分与主流程，不再展开每个模块的内部细节；具体设计参见对应的子文档：

- `lexer.md`：词法分析器设计
- `parser.md`：语法分析器与 AST 设计
- `semantic.md`：语义分析与符号表
- `ir.md`：中间表示与指令集
- `backend.md`：后端与寄存器分配
- `errorReporter.md`：统一错误收集与输出

模块之间的依赖关系为：

- `Lexer` 依赖 `Token`
- `Parser` 依赖 `Lexer` 和 `AST`
- `SemanticAnalyzer` 依赖 `AST`、`Type`、`SymbolTable`
- `CodeGen` 依赖语义分析后的 AST 与符号表，生成 IR
- `Backend`（AsmGen + RegisterAllocator）依赖 IR，生成 MIPS 汇编
- `ErrorReporter` 被 Lexer/Parser/SemanticAnalyzer 共用，用来集中记录所有错误

## 编译流水线

整个编译过程从源文件 `testfile.txt` 到最终的 MIPS 汇编 `mips.txt`，大致分为以下阶段：

1. **输入与输出重定向**
    - 将错误输出重定向到 `error.txt`，用于记录所有阶段产生的错误信息。
    - 将标准输出重定向到 `ir.txt`，用于记录中间表示（IR）或相关调试信息。

2. **词法分析（Lexer）**
    - 根据文法定义和保留字表，将源代码字符流切分为 Token 流。
    - 识别关键字、标识符、各类常量、运算符与分隔符。
    - 发现非法字符（如单独的 `&`、`|` 等）时，记录错误类型 `a`，通过 ErrorReporter 统一收集。

3. **语法分析（Parser）**
    - 采用递归下降方式，根据给定的 EBNF 将 Token 流转为 AST。
    - 维护非终结符的行号信息，为后续错误定位服务。
    - 在缺分号、右括号等情况下记录 `i/j/k` 类型错误，同时尽量进行错误恢复。

4. **语义分析（SemanticAnalyzer）**
    - 在栈式符号表上完成声明、定义与使用检查。
    - 检查函数调用的参数个数与类型、返回语句的存在性与类型匹配、常量赋值合法性、循环控制语句位置等。
    - 将所有 `b`–`m` 类型的语义错误通过 ErrorReporter 统一收集。
    - 语义分析结束后导出完整的 `SymbolTable`，供 IR 生成阶段使用。

5. **错误收集与早期退出**
    - 若 ErrorReporter 中存在任意错误，则按照行号排序输出到 `error.txt`，并终止后续阶段。

6. **IR 生成与优化（CodeGen）**
    - 在语义分析通过的前提下，从 AST 和符号表生成函数级别 IR：
      - 构建函数、基本块与四元式指令序列。
      - 按 `ir.md` 中约定的 OpCode 与操作数规则生成中间代码。
    - 在 IR 上应用轻量级优化（例如常量折叠与块内死代码删除），具体算法详见 `ir.md` 与 `backend.md` 中的说明。
    - 汇总得到 IR 模块视图（函数列表、全局变量 IR、字符串字面量表）。

7. **后端 MIPS 代码生成（Backend / AsmGen）**
    - 使用 RegisterAllocator 在函数内为 IR 临时变量分配有限数量的物理寄存器（图着色算法）。
    - 按函数生成栈帧、保存/恢复必要的寄存器、处理溢出（spill）到栈上的内存槽。
    - 根据 IR 指令序列生成对应的 MIPS 指令序列，遵循约定的调用约定与运行时辅助例程（如 `printf`、`getint`）。
    - 最终将 MIPS 汇编输出到 `mips.txt` 文件。

## 构建系统概览

构建系统采用 CMake，核心库目标在 `src/CMakeLists.txt` 中定义，主要包括：

- `Lexer`：`lexer/Lexer.cpp`
- `Parser`：`parser/Parser.cpp`
- `Semanticanalyzer`：`semantic/SemanticAnalyzer.cpp`
- `ErrorReporter`：`errorReporter/ErrorReporter.cpp`
- `Codegen`：IR 构建与相关组件（`CodeGen.cpp`、`Function.cpp`、`BasicBlock.cpp`、`Instruction.cpp`、`Operand.cpp` 等）
- `Backend`：后端实现（`AsmGen.cpp`、`RegisterAllocator.cpp`）

顶层 `CMakeLists.txt` 将这些库与 `main.cpp` 链接生成最终的可执行文件 `Compiler`。各模块的详细构建与依赖关系可在 `src/CMakeLists.txt` 中查看。

## 模块文档索引

本总览文件只提供高层架构图与阶段划分，每个阶段的具体设计与实现细节请参考：

- 词法分析：`lexer.md`
- 语法分析与 AST：`parser.md`
- 语义分析与符号表：`semantic.md`
- 错误收集与输出：`errorReporter.md` 与 `error.md`
- 中间表示与优化：`ir.md`
- 后端与寄存器分配：`backend.md`

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

# LCC 编译器 - Semantic Analyzer 设计文档

## 概述

Semantic Analyzer（语义分析器）是 LCC 编译器的第三个阶段，负责对 Parser 生成的 AST 进行语义分析，包括类型检查、作用域管理、函数调用验证等。本文档详细介绍了 Semantic Analyzer 的设计架构、实现细节和使用方式。

## 架构设计

### 类结构

`SemanticAnalyzer` 的对外接口和内部成员在头文件 `SemanticAnalyzer.hpp` 中定义，核心结构如下所示（省略了一些与实现无关的细节）：

```cpp
class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  // 语义分析入口：从编译单元根节点开始遍历 AST
  void visit(CompUnit *node);

  // 语义分析完成后导出符号表，供后续 CodeGen 使用
  const SymbolTable &getSymbolTable() const;

private:
  // 初始化内建函数（printf、getint 等），在全局作用域中插入相应符号
  void initializeBuiltinFunctions();

  // 语义分析使用的符号表（栈式作用域管理）
  SymbolTable symbolTable;

  // 当前所在的循环嵌套深度，用于检查 break / continue 是否出现在合法位置
  int loop = 0;

  // 当前正在检查的函数返回类型，用于验证 return 语句
  TypePtr current_function_return_type = nullptr;

  // 是否启用与语义分析相关的额外输出（调试用）
  bool outputenabled = false;

  // 统一的错误上报接口，内部通过 ErrorReporter 收集错误
  void error(const int &line, const std::string errorType);

  // 下面是一系列针对不同 AST 节点的 visit 重载声明
  void visit(Decl *node);
  void visit(ConstDecl *node);
  void visit(VarDecl *node);
  void visit(ConstDef *node, TypePtr type);
  void visit(VarDef *node, TypePtr type);
  void visit(FuncDef *node);
  void visit(MainFuncDef *node);
  void visit(FuncFParams *node);
  void visit(FuncFParam *node);
  void visit(Block *node);
  void visit(BlockItem *node);

  void visit(Stmt *node);
  void visit(AssignStmt *node);
  void visit(ExpStmt *node);
  void visit(BlockStmt *node);
  void visit(IfStmt *node);
  void visit(ForStmt *node);
  void visit(BreakStmt *node);
  void visit(ContinueStmt *node);
  void visit(ReturnStmt *node);
  void visit(PrintfStmt *node);
  void visit(ForAssignStmt *node);

  TypePtr visit(BType *node);
  TypePtr visit(FuncType *node);
  void visit(ConstInitVal *node);
  void visit(InitVal *node);
  TypePtr visit(Exp *node);
  TypePtr visit(Cond *node);
  TypePtr visit(LVal *node);
  TypePtr visit(PrimaryExp *node);
  TypePtr visit(Number *node);
  TypePtr visit(UnaryExp *node);
  void visit(UnaryOp *node);
  std::vector<TypePtr> visit(FuncRParams *node);
  TypePtr visit(MulExp *node);
  TypePtr visit(AddExp *node);
  TypePtr visit(RelExp *node);
  TypePtr visit(EqExp *node);
  TypePtr visit(LAndExp *node);
  TypePtr visit(LOrExp *node);
  TypePtr visit(ConstExp *node);
};
```

## 核心组件

### 类型系统

#### 基础类型

```cpp
enum class BaseType { VOID, INT };
```

#### 类型分类

```cpp
enum class Category { Basic, Array, Function };
```

#### 类型类设计

对类型设计了几个工厂方法，方便创建不同类型的实例

```cpp
enum class BaseType { VOID, INT };

class Type {
public:
  enum class Category { Basic, Array, Function };

  Category category;
  BaseType base_type;
  bool is_const = false;
  bool is_static = false;

  TypePtr array_element_type;
  int array_size = 0; // 似乎没有用

  TypePtr return_type;
  std::vector<TypePtr> params;

  Type(Category cat) : category(cat) {}

  static TypePtr create_base_type(BaseType base, bool is_const = false,
                                  bool is_static = false) {
    auto type = std::make_shared<Type>(Category::Basic);
    type->is_const = is_const;
    type->is_static = is_static;
    type->base_type = base;
    return type;
  }

  static TypePtr create_array_type(TypePtr element_type, int size) {
    auto type = std::make_shared<Type>(Category::Array);
    type->array_element_type = element_type;
    type->array_size = size;
    return type;
  }

  static TypePtr create_function_type(TypePtr ret_type,
                                      const std::vector<TypePtr> &params) {
    auto type = std::make_shared<Type>(Category::Function);
    type->return_type = ret_type;
    type->params = params;
    return type;
  }
  static TypePtr getIntType() { return create_base_type(BaseType::INT); }
  static TypePtr getVoidType() { return create_base_type(BaseType::VOID); }
};
```

### 符号表（Symbol Table）

#### 符号定义

```cpp
struct Symbol {
  std::string name;
  TypePtr type;
  int line;
};
```

#### 作用域管理

定义了一个栈式符号表，使用了`ScopeRecord`结构体来记录每个作用域的信息,对于整个
符号表使用`records`来存储所有作用域的`level`,使用active来存储当前活跃的所有作用域索引。

**规则**：
进入新作用域时，调用`pushScope()`，离开作用域时，调用`popScope()`。在当前作用域添加符号时，调用`addSymbol()`。查找符号时，调用`findSymbol()`，从当前作用域开始向外查找。

```cpp
class SymbolTable {
private:
  struct ScopeRecord {
    int level = 0;
    std::unordered_map<std::string, Symbol> table;
    std::vector<std::string> order;
  };

  /**
   * @brief records of all scopes
   */
  std::vector<ScopeRecord> records;
  /**
   * @brief current active scopes, storing indices into records
   */
  std::vector<size_t> active;
  /**
   * @brief the child scope level generator
   */
  int nextLevel = 1;

public:
  SymbolTable() { pushScope(); }

  void pushScope() {
    ScopeRecord rec;
    rec.level = nextLevel++;
    records.emplace_back(std::move(rec));
    active.push_back(records.size() - 1);
  }

  void popScope() {
    if (!active.empty()) {
      active.pop_back();
    }
  }

  bool addSymbol(const Symbol &symbol) {
    if (active.empty())
      return false;
    auto &rec = records[active.back()];
    if (rec.table.count(symbol.name))
      return false;
    rec.table.emplace(symbol.name, symbol);
    rec.order.push_back(symbol.name);
    return true;
  }

  std::optional<Symbol> findSymbol(const std::string &name) const {
    for (auto it = active.rbegin(); it != active.rend(); ++it) {
      const auto &rec = records[*it];
      auto f = rec.table.find(name);
      if (f != rec.table.end()) {
        return f->second;
      }
    }
    return std::nullopt;
  }

  void printTable() const {
    for (const auto &rec : records) {
      for (const auto &name : rec.order) {
        const auto &sym = rec.table.at(name);
        std::cout << rec.level << " " << sym.name << " " << to_string(sym.type)
                  << std::endl;
      }
    }
  }
};
```

## 语义分析流程

### 整体分析流程

1. **初始化**：创建符号表，设置全局作用域
2. **编译单元分析**：遍历所有声明、函数定义和主函数
3. **符号表构建**：在分析过程中填充栈式符号表
4. **类型检查**：对表达式进行类型推导和检查
5. **控制流检查**：验证 break/continue/return 语句的正确性
6. **输出生成**：输出符号表信息供后续阶段使用

## 错误处理

### 错误类型

语义分析器检测以下错误类型：

- **b: 重定义错误**：标识符在同一作用域内重复定义
- **c: 未定义错误**：使用未定义的标识符
- **d: 函数参数数量错误**：函数调用时参数数量不匹配
- **e: 函数参数类型错误**：函数调用时参数类型不匹配
- **f: 返回值错误**：void 函数有返回值或返回值类型不匹配
- **g: 缺少返回语句**：有返回值的函数缺少 return 语句
- **h: 常量赋值错误**：试图给常量赋值
- **l: printf 参数数量不匹配**：格式字符串参数数量与实际参数不符
- **m: 循环控制语句错误**：break/continue 不在循环内使用

所有错误通过 ErrorReporter 统一收集，最后按行号排序输出。

## 与其他组件的交互

### 与 Parser 的交互

- 接收完整的 AST 树进行遍历分析
- 利用 AST 节点中的行号信息进行错误定位
- 将类型信息写回 AST 节点供后续阶段使用

# LCC编译器 - IR设计文档

> 本文档与 `include/codegen/Instruction.hpp`、`src/codegen/*.cpp` 以及后端 `backend/AsmGen.cpp`、`backend/RegisterAllocator.cpp` 的当前实现同步。若新增指令或修改语义，请同时更新此文件。

## 设计总览

- **表示形式**：四元式三地址码 `(op, arg1, arg2, result)`。
- **组织结构**：`Module → Function → BasicBlock → Instruction`，并显式维护 CFG（fallthrough + 跳转）。
- **非 SSA**：变量与临时均可多次赋值。常量折叠后，`result` 甚至可直接是常量。
- **类型模型**：语言仅有 `int` 与 `void`。数组在 IR 层表现为 `int*`（地址），索引一律以“元素”为单位。
- **后端接口**：`IRModuleView` 暴露 `functions`、`globals`（全局 ALLOCA 与常量初始化）以及 `stringLiterals`。

```
Module
├── globals : std::vector<Instruction>
├── functions : std::vector<const Function*>
│   └── Function
│       ├── params : std::vector<SymbolPtr>
│       ├── blocks : std::vector<std::unique_ptr<BasicBlock>>
│       │   ├── instructions : std::vector<Instruction>
│       │   ├── next : BasicBlock* (fallthrough)
│       │   └── jumpTarget : BasicBlock* (显式跳转)
│       └── metadata : name / returnType / symbol table hook
└── stringLiterals : unordered_map<std::string, SymbolPtr>
```

## 操作数（Operand）

| 类型 | 构造方式 | 说明 |
|------|----------|------|
| `Variable` | `Operand::Variable(std::shared_ptr<Symbol>)` | 绑定语义分析阶段的符号，携带类型/作用域/是否全局等属性 |
| `Temporary` | `Operand::Temporary(int id)` | `CodeGen` 自动分配。`id` 是活跃性分析、寄存器分配的键 |
| `ConstantInt` | `Operand::ConstantInt(int32)` | 立即数。后端直接 `li` 或折叠 |
| `Label` | `Operand::Label(int id)` | 基本块入口。后端生成真实 label：`<func>_L<id>` |
| `Empty` | `Operand()` | 表示该槽位无值（例如 `ARG` 的 `arg2/result`，或 `RETURN` 无返回值） |

> `Instruction` 不限制将任意 `Operand` 放入任意槽位，但后端按 `OpCode` 假设类型。`CodeGen` 必须遵循下文所列规则。

## 指令集（OpCode）

所有枚举详见 `Instruction.hpp` 注释。表格中的 `res(temp|const)` 表示：结果通常写入临时，常量折叠后可直接写常量。

### 算术 / 比较 / 逻辑

| OpCode | 形式 | 说明 |
|--------|------|------|
| `ADD/SUB/MUL/DIV/MOD` | `op arg1(var|temp|const), arg2(var|temp|const), res(temp|const)` | `DIV/MOD` 由后端降为 `div` + `mflo/mfhi` |
| `NEG` | `NEG arg1(...), -, res(temp|const)` | 一元取负 |
| `EQ/NEQ/LT/LE/GT/GE` | 同二元格式 | 比较结果归一化到 0/1 |
| `AND/OR` | `AND/OR arg1, arg2, res` | 逻辑值模式（非短路）。短路由控制流构造实现 |
| `NOT` | `NOT arg1, -, res` | `res = !arg1` |

### 数据移动与内存访问

| OpCode | 形式 | 说明 |
|--------|------|------|
| `ASSIGN` | `ASSIGN src(var|temp|const), -, dst(var|temp)` | 通用赋值，后端会判定是否需要落栈 |
| `LOAD` | `LOAD base(var|temp), index(var|temp|const|empty), dst(var|temp)` | 读取数组/指针元素。`index` 为空视为 `*base` |
| `STORE` | `STORE value(var|temp|const), base(var|temp), index(var|temp|const|empty)` | 写数组/指针 |
| `ALLOCA` | `ALLOCA var(var), -, size(var|temp|const)` | 分配 `size` 个 word。全局进入 `.data`，局部在栈帧分配 |

### 控制流

| OpCode | 形式 | 含义 |
|--------|------|------|
| `LABEL` | `LABEL -, -, res(label)` | 基本块入口。`CodeGen` 负责插入 |
| `GOTO`  | `GOTO -, -, res(label)` | 无条件跳转 |
| `IF`    | `IF cond(var|temp|const), -, res(label)` | `cond != 0` 时跳转 |
| `RETURN`| `RETURN -, -, res(var|temp|const|empty)` | 函数返回。空表示 `void` |

### 函数调用

| OpCode | 形式 | 角色 | 说明 |
|--------|------|------|------|
| `PARAM` | `PARAM idx(const), -, res(var)` | **函数定义阶段**使用：记录“第 idx 个形式参数绑定哪个符号”。必须出现在入口块的开头 |
| `ARG` | `ARG arg(var|temp|const), -, -` | **调用方**使用：将一个实参排入队列。后端在 `CALL` 时顺序消费 |
| `CALL` | `CALL argc(const), func(label), res(temp|empty)` | 发起调用。`argc` = 之前发射的 `ARG` 数；`res` 可为空表示忽略返回值 |

> 前端保证：`CALL` 之前连续出现 `argc` 条 `ARG`，且中途不会夹杂其他 `CALL`。

## 基本块与 CFG

1. 新的 `LABEL` 启动基本块。
2. `GOTO`、`IF`、`RETURN` 结束当前块；若后面还有指令必须显式插入新的 `LABEL`。
3. `BasicBlock::next` 连接 fallthrough，`jumpTarget` 记录显式分支。

该结构为活跃性分析（寄存器分配）、短路求值与优化 pass 提供边界。

## 函数结构与模块接口

- `Function` 保存：名称、返回类型、形式参数向量、基本块列表以及 CFG 边。
- `CodeGen` 在函数入口自动发射 `PARAM idx, var`，同时根据 AST 插入 `ALLOCA`、`ASSIGN`、`LOAD/STORE` 等。
- `IRModuleView` 仅提供对函数/全局的只读访问，后端在遍历过程中不会修改 IR。

## 默认优化 Pass

`runDefaultQuadOptimizations(Function &fn)` 依次执行：

1. **ConstFoldPass**

- 若算术/比较/逻辑的两个操作数均为常量，则改写为 `ASSIGN const`。
- `IF const`：真变 `GOTO label`，假变空操作（`ASSIGN` 占位）。

2. **LocalDCEPass**

- 在基本块内反向扫描，删除“无副作用、结果为临时且未再使用”的指令。
- `STORE/GOTO/IF/CALL/RETURN/PARAM/ALLOCA` 等都视为有副作用，不会被删除。

尚未启用跨块优化（CSE、循环优化等）。IR 保持易读，方便后续扩展。

## 示例

### 条件语句

```c
if (x > 0) {
 y = 1;
} else {
 y = 2;
}
```

```
LABEL L0
GT x, 0, %t0
IF %t0, L1
ASSIGN 2, -, y
GOTO L2
LABEL L1
ASSIGN 1, -, y
LABEL L2
```

### 函数调用

```c
int bar(int x) {
 int t = foo(x, 1);
 return t + 2;
}
```

```
LABEL L0
PARAM 0, x
ARG x
ARG 1
CALL 2, foo, %t0
ADD %t0, 2, %t1
RETURN %t1
```

## 与后端交互要点

- `Label` ID 在函数内必须唯一且为非负整数，后端以 `func_L{id}` 命名。
- `ALLOCA` 的 `size` 单位是 **word**。后端在 `.data` 或栈帧中实际分配 `size * 4` 字节。
- `globals` 中允许混合 `ALLOCA/ASSIGN/STORE`，`AsmGen::emitDataSection` 会利用常量信息输出 `.word` 或运行期初始化代码。
- `ARG/CALL`：IR 层需保证不会把 `ARG` 与其他 `CALL`/`ARG` 交叉嵌套，避免后端状态污染。

---

配合同目录下的 `backend.txt`、`overview.md`、`semantic.md`，本文档构成了 AST → IR → MIPS 的中间层说明。后续若扩展指令或引入新优化，请以此为基准同步更新。

# LCC编译器 - backend 设计文档

> 本文档描述 `src/backend/AsmGen.cpp` 与 `src/backend/RegisterAllocator.cpp` 的实现细节，重点解释寄存器分配、栈帧布局以及 IR → MIPS 的翻译流程。

## 模块概览

后端主要由两个模块组成：

- `RegisterAllocator`：对每个函数进行活跃性分析，构建冲突图，并将 IR 临时 (`Temporary`) 着色到有限数量的物理寄存器上（`$s0-$s7`）。
- `AsmGen`：遍历 IR 模块，将 IR 指令翻译为 MIPS 汇编；负责数据段/文本段输出、栈帧布局、调用约定及溢出（spill）变量的访存。

前端生成的 IR 通过 `IRModuleView` 暴露给后端：

```cpp
struct IRModuleView {
 std::vector<const Function *> functions;            // 所有函数
 std::vector<const Instruction *> globals;           // 全局 ALLOCA/初始化指令
 std::unordered_map<std::string, std::shared_ptr<Symbol>> stringLiterals;
};
```

`AsmGen::generate` 是后端入口：

```cpp
void AsmGen::generate(const IRModuleView &mod, std::ostream &out);
```

它会先输出 `.data` 段（字符串常量与全局变量），再输出 `.text` 段（所有函数及运行时辅助例程）。

## 数据段生成（emitDataSection）

### 字符串常量

`stringLiterals` 存储从前端收集到的所有字符串常量，key 为**源代码字面量**（如 `"%d\n"`），value 为指向全局 `Symbol` 的智能指针。`emitDataSection` 对每个条目输出：

```mips
label: .asciiz "原始字符串内容"
```

当前实现直接逐字符写入，不额外处理转义（即假定前端传入的字面量已经是希望写入 `.asciiz` 的内容）。

### 全局变量

`globals` 中的指令序列编码了全局变量的分配与常量初始化，主要使用：

- `ALLOCA var, -, size`：声明一个大小为 `size`（以 word 计）的全局数组或变量。
- `ASSIGN const, -, var`：为标量全局变量设置初值。
- `STORE const, base, index`：为全局数组的第 `index` 个元素设置初值。

`emitDataSection` 先扫描所有 `ALLOCA` 指令，记录每个全局符号的大小，并初始化一个全为 0 的数组；随后处理 `ASSIGN/STORE`，将常量写入对应位置，最后输出 `.word` 指令：

```mips
global_or_mangled_name: .word v0, v1, v2, ...
```

这里使用 `Symbol::globalName` 作为 label，若为空则退回原始 `name`，保证不同作用域下的同名变量仍映射到唯一的全局符号。

## 文本段生成（emitTextSection）

文本段生成包含三部分：

1. 为每个函数输出 `.globl name` 与标签 `name:`。
2. 优先输出 `main` 函数，其后输出其他函数。
3. 在末尾内联简化版 `printf` 与 `getint` 运行时例程。

`emitTextSection` 内部对每个 `Function` 调用 `emitFunction` 完成具体翻译。

## 寄存器分配（RegisterAllocator）

后端采用经典的图着色寄存器分配算法，将 IR 临时变量映射到 8 个**可分配寄存器**：

```cpp
// RegisterAllocator 内部约定：
static const int NumRegs = 8; // 对应 AsmGen 中的 $s0-$s7
```

`AsmGen` 在构造时会初始化一张寄存器描述表：

```cpp
static const char *Regs[NUM_ALLOCATABLE_REGS] =
  {"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7"};
```

每个函数在生成汇编前，都会调用：

```cpp
_regAllocator.run(const_cast<Function *>(func));
```

### Use/Def 与活跃性分析

寄存器分配的第一步是对每个基本块计算：

- `use[B]`：在块内**第一次被使用**、但在该块中尚未定义的临时集合；
- `def[B]`：在块内被定义（作为 `result`）的临时集合；
- `liveIn[B]` / `liveOut[B]`：经典数据流方程：

$$
liveOut[B] = \bigcup_{S \in succ(B)} liveIn[S] \\
liveIn[B] = use[B] \cup (liveOut[B] - def[B])
$$

实现上，`computeUseDef` 逐块扫描指令；`computeLiveInOut` 反复迭代直至收敛。`STORE` 指令的 `result`（数组索引）被视为“使用”而非“定义”。

### 冲突图构建

`buildInterferenceGraph` 从每个基本块的末尾向前遍历，维护当前 `live` 集合：

- 若指令定义了某个临时 `t`，则为 `t` 与当前 `live` 中的所有其它临时添加冲突边；然后将 `t` 从 `live` 中移除。
- 对于指令使用的每个临时，将其加入 `live`。

最终得到一个无向图：

- 节点：所有出现过的临时 ID；
- 边：生命周期重叠的临时对。

### 图着色与溢出

`doColoring` 采用简化的栈式图着色：

1. 重复选择**度数 < NumRegs** 的节点入栈，并从图中删除相关边；
2. 若不存在此类节点，则选择任意节点标记为**潜在溢出**，入栈后从图中删除；
3. 图清空后，逆序弹栈，对每个节点选择一个未被邻居占用的颜色（0~7），若无可用颜色则将其加入 `_spilledNodes`。

`RegisterAllocator` 对外提供：

- `int getReg(int tempId) const;`：返回某临时对应的寄存器编号（0~7），若未分配寄存器返回 -1；
- `bool isSpilled(int tempId) const;`：查询某临时是否被溢出到内存；
- `const LiveSet &getSpilledNodes() const;`：所有溢出的临时集合；
- `std::set<int> getUsedRegs() const;`：本函数实际用到的寄存器编号集合，用于选择保存/恢复的 `$sX` 列表。

## AsmGen 中的寄存器与栈帧策略

### 寄存器分类

AsmGen 逻辑中，MIPS 寄存器按角色划分：

| 类别 | 实际寄存器 | 用途 |
|------|------------|------|
| **可分配寄存器** | `$s0-$s7` | 由 `RegisterAllocator` 分配给 IR 临时；在函数入口保存、函数退出恢复 |
| **Scratch 寄存器** | `$t0-$t9` | 指令级临时：加载常量、访存地址计算、溢出变量读写等；不参与图着色 |
| **参数寄存器** | `$a0-$a3` | 函数调用时前 4 个参数 |
| **返回值寄存器** | `$v0` | 函数返回值 |
| **栈/帧/返回地址** | `$sp/$fp/$ra` | 维护栈帧和控制流 |

> 当前实现中，`AsmGen::allocateScratch` 会在 `$t0-$t9` 中线性扫描一个 `inUse == false` 的寄存器并标记为使用，若耗尽则返回 `$zero` 作为失败占位；每条 IR 指令开始时通过 `resetScratchState()` 清空 `inUse` 标记，保证 scratch 寄存器不跨指令泄漏。

### 栈帧布局

`emitFunction` 为每个函数构建如下栈帧（从低地址到高地址）：

```text
低地址
 0($sp): 保存的 $ra
 4($sp): 保存的 $fp
 8($sp)...: 局部变量、局部数组（由 ALLOCA 决定大小）
 ...: 为溢出临时分配的空间（每个 4 字节）
 ...: 保存的 $s0-$s7（仅保存实际使用到的寄存器）
高地址（= 调用前的 $sp）
```

`analyzeFunctionLocals` 在函数内第一次扫描中完成：

- 起始偏移 `nextOffset = 8`，为每个形式参数（通过入口块的 `PARAM` 指令识别）与局部变量（`ALLOCA`）分配连续的 word 大小区域；
- 为数组变量分配 `size` 个 word；
- 记录到 `locals_ : Symbol* -> {offset, size}`。

完成 `RegisterAllocator::run` 后，AsmGen 再：

- 为 `_spilledNodes` 中的每个临时从当前 `frameSize_` 往上分配 4 字节空间，并记录到 `_spillOffsets[tempId]`；
- 更新 `frameSize_`；
- 根据 `getUsedRegs()` 计算需要保存的 `$sX` 个数，并为其在栈帧顶部预留空间；
- 最终 `frameSize_` 即整个栈帧大小（字节）。

### 序言与尾声

**序言（prologue）：**

```mips
func_name:
 addiu $sp, $sp, -frameSize   # 若 frameSize 超出 16bit，则使用 li+addu
 sw   $ra, 0($sp)
 sw   $fp, 4($sp)
 sw   $sX, offset($sp)        # 对每个使用的 $sX 保存
 move $fp, $sp

 # 将 $a0-$a3 中的前 4 个参数复制到对应局部槽位
 sw   $a0, off0($fp)
 sw   $a1, off1($fp)
 ...
 # 超过 4 个的参数从调用者栈帧中读出，写入自己的局部槽
 lw   $t?, caller_off($fp)
 sw   $t?, local_off($fp)
```

**尾声（epilogue）：**

```mips
func_name_END:
 lw   $sX, offset($sp)
 lw   $ra, 0($fp)
 lw   $fp, 4($fp)
 addiu $sp, $sp, frameSize
 jr   $ra                  # main 特殊：用 syscall 17 退出
```

所有 `RETURN` 指令都会先将返回值写入 `$v0`，然后跳转到统一的 `func_name_END` 标签，避免重复展开恢复逻辑。

## IR 指令到 MIPS 的映射（lowerInstruction）

`lowerInstruction` 是 IR → MIPS 的核心函数。它在每条指令开始时调用 `resetScratchState()`，然后根据 `OpCode` 生成具体指令。

### 寄存器获取与结果存储

两个重要的内部工具函数：

- `std::string AsmGen::getRegister(const Operand &op, std::ostream &out);`
  - `Temporary`：若未溢出，直接返回其分配的 `$sX`；若溢出，则使用 scratch 寄存器从 `_spillOffsets` 指示的栈位置 `lw` 进来；
  - `ConstantInt`：若为 0，直接返回 `$zero`；否则分配一个 scratch，输出 `li scratch, imm` 并返回；
  - `Variable`：根据符号是否为局部/全局、是否数组，决定使用 `lw` 读值或 `la` 取地址；
  - 其他类型（Label/Empty）：统一返回 `$zero`（不会被算术路径使用）。

- `void AsmGen::storeResult(const Operand &op, const std::string &reg, std::ostream &out);`
  - 若目标为溢出临时：按照 `_spillOffsets` 中的偏移写回栈帧；
  - 若为局部变量：根据 `locals_` 查找偏移，`sw reg, offset($fp)`；
  - 若为全局变量：通过 `la scratch, globalName` 获取地址，再 `sw reg, 0(scratch)`。

`lowerInstruction` 在多数算术/逻辑指令中遵循以下模板：

1. 使用 `getRegister` 为 `arg1/arg2` 获取寄存器（可能是 `$sX` 或 `$t?`）。
2. 通过 `regForTemp` 或 `allocateScratch` 决定结果寄存器：

- 若 `result` 是未溢出的临时，则使用其专属 `$sX`；
- 否则使用一个 scratch。

3. 输出对应的 MIPS 指令（`addu/subu/mul/div/mflo/...`）。
4. 若 `result` 是临时且被标记为溢出，在寄存器计算完成后调用 `storeToSpill(tempId, rd)` 落栈。

### 典型指令映射

- **算术**：

 ```mips
 # ADD a, b, res
 addu rd, ra, rb
 ```

- **比较**（如 `EQ`）：

 ```mips
 subu  $t?, ra, rb
 sltiu rd, $t?, 1   # rd = (ra == rb)
 ```

- **逻辑 AND/OR**：

 使用 `sltu` 将任意值归一化为 0/1，再 `and/or` 组合。

- **LOAD/STORE**：

  - 局部变量：经 `locals_` 查找偏移，使用 `lw/sw offset($fp)`；
  - 全局变量：

  ```mips
  la  $t?, globalName
  lw  rd, 0($t?)    # 读
  sw  rv, 0($t?)    # 写
  ```

  - 数组访问：对动态索引用 `sll indexReg, 2` 计算字节偏移，再与基址相加。

- **控制流**：

  - `LABEL`：输出 `<func>_L<id>:`；
  - `GOTO`：`j <func>_L<id>`；
  - `IF`：`bne condReg, $zero, <func>_L<id>`。

- **CALL/RETURN**：

  - `ARG`：前 4 个参数写 `$a0-$a3`；其余暂存于 `pendingExtraArgs_`，在 `CALL` 中统一压栈；
  - `CALL`：将额外参数按顺序 `sw` 到栈上，`jal funcName` 后再 `addiu $sp, $sp, extraBytes` 弹栈；若有返回值，将 `$v0` 拷贝到对应临时并按需落栈；
  - `RETURN`：将常量或表达式结果写入 `$v0` 后，`j func_END`。

## 运行时辅助例程

`emitTextSection` 在所有函数之后内联了两个简化运行时函数：

- `printf`：
  - 使用 syscall 1/11 实现整数与字符输出；
  - 遍历格式字符串，仅支持 `%d` 占位符，其余字符逐字打印；
  - 在进入时保存 `$t0-$t2` 与 `$a0-$a3`，返回前恢复，并使用 `jr $ra` 返回。

- `getint`：
  - 调用 syscall 5 从标准输入读取一个整数；
  - 结果存入 `$v0`，`jr $ra` 返回。

这些例程不通过 IR 生成，而是直接由 AsmGen 拼接文本，方便在所有程序中复用。

---

以上描述了当前后端实现的整体结构与关键策略，尤其是寄存器分配与栈帧布局的细节。若后续修改调用约定、引入新的寄存器分类或增加优化（如跨块重写），请同步更新本文件以保持文档与代码一致。
