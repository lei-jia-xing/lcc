# LCC 编译器设计文档

## 概述

LCC (Lightweight C Compiler) 是一个用 C++17 实现的轻量级 C 语言子集编译器，采用现代编译器设计原理，目标架构为 MIPS。本文档详细描述了编译器的整体架构、各组件设计、实现细节和未来扩展计划。

## 编译器架构

### 整体架构图

```
源代码 (.c) → 词法分析器 → 语法分析器 → 语义分析器 → 中间代码生成 → 代码优化 → 目标代码生成 (.s)
     ↓              ↓            ↓            ↓              ↓           ↓              ↓
  字符流        Token流      AST语法树    带类型的AST      IR代码     优化IR      MIPS汇编
```

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

### 核心组件

#### 1. Token 定义

Token 类使用 `std::variant` 存储不同类型的值，支持类型安全的访问：

```cpp
enum class TokenType {
    IDENFR,        // 标识符
    INTCON,        // 整型常量
    STRCON,        // 字符串常量
    // 关键字
    CONSTTK, INTTK, STATICTK, BREAKTK, CONTINUETK,
    IFTK, MAINTK, ELSETK, FORTK, RETURNTK, VOIDTK, PRINTFTK,
    // 运算符
    PLUS, MINU, MULT, DIV, MOD, NOT, AND, OR,
    LSS, LEQ, GRE, GEQ, EQL, NEQ, ASSIGN,
    // 分隔符
    SEMICN, COMMA, LPARENT, RPARENT, LBRACK, RBRACK,
    LBRACE, RBRACE,
    EOFTK, UNKNOWN  // 文件结束符, 未知符号
};
```

#### 2. Lexer 类设计

- **源码管理**: 使用 `std::string source` 存储源码，`size_t pos` 跟踪当前位置
- **行号跟踪**: `int line` 维护当前行号，用于错误报告
- **关键字表**: 静态哈希表 `reserveWords` 存储保留字映射
- **错误处理**: 支持静默模式，便于解析恢复

#### 3. 关键特性

- **错误恢复**: 识别未知字符时报告错误但继续解析
- **前瞻能力**: `peekToken(n)` 提供前瞻功能，支持任意距离的 Token 预览
- **静默机制**: `silentPV()` 控制输出，用于解析过程中的临时解析

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

### 语法规则

编译器支持以下 C 语言子集：

#### 1. 编译单元

```
CompUnit → {Decl} {FuncDef} MainFuncDef
```

#### 2. 声明

```
Decl → ConstDecl | VarDecl
ConstDecl → 'const' BType ConstDef { ',' ConstDef } ';'
VarDecl → [ 'static' ] BType VarDef { ',' VarDef } ';'
```

#### 3. 函数定义

```
FuncDef → FuncType Ident '(' [FuncFParams] ')' Block
MainFuncDef → 'int' 'main' '(' ')' Block
```

#### 4. 语句

```
Stmt → LVal '=' Exp ';'                    // 赋值语句
    | [Exp] ';'                           // 表达式语句
    | Block                               // 复合语句
    | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]  // 条件语句
    | 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt  // 循环语句
    | 'break' ';' | 'continue' ';'        // 控制语句
    | 'return' [Exp] ';'                  // 返回语句
    | 'printf''('StringConst {','Exp}')'';'  // 输出语句
```

#### 5. 表达式

```
Exp → AddExp
AddExp → MulExp | AddExp ('+' | '−') MulExp
MulExp → UnaryExp | MulExp ('*' | '/' | '%') UnaryExp
UnaryExp → PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
PrimaryExp → '(' Exp ')' | LVal | Number
```

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

- **期望检查**: `expect()` 函数验证预期 Token 类型
- **同步机制**: `sync()` 函数在错误发生后找到恢复点
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

## 错误处理

### 错误分类

| 错误类型 | 编码 | 描述 |
|----------|------|------|
| 词法错误 | 'a' | 非法字符 |
| 语法错误 | 'i' | 缺少';' |
| 括号匹配 | 'j' | 缺少')' |
| 数组边界 | 'k' | 缺少']' |

### 错误恢复策略

1. **局部化处理**: 错误发生后忽略错误,继续解析

## 开发指南

### 代码风格

- 使用 Doxygen 风格注释
- 遵循 Google C++ 编码规范
- 使用现代 C++ 特性 (C++17)

## 总结

LCC 编译器采用现代编译器设计原理，具有清晰的模块化架构、完善的错误处理机制和良好的扩展性。当前已完成词法分析和语法分析阶段，为实现完整的编译器奠定了坚实基础。通过持续的迭代和优化，可以扩展支持更复杂的语言特性和优化策略。

