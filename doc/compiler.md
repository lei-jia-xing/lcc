# LCC 编译器设计文档

## 项目结构

下面的目录树由实际仓库生成，反映当前编译器完整的项目布局：

```
.
├── cliff.toml
├── CMakeLists.txt
├── config.json
├── doc
│   ├── backend.md
│   ├── compiler.md
│   ├── ebnf.md
│   ├── error.md
│   ├── errorReporter.md
│   ├── ir.md
│   ├── lexer.md
│   ├── optimize.md
│   ├── overview.md
│   ├── parser.md
│   └── semantic.md
├── Doxyfile
├── include
│   ├── backend
│   │   ├── AsmGen.hpp
│   │   └── RegisterAllocator.hpp
│   ├── codegen
│   │   ├── BasicBlock.hpp
│   │   ├── CodeGen.hpp
│   │   ├── Function.hpp
│   │   ├── Instruction.hpp
│   │   ├── Operand.hpp
│   │   └── QuadOptimizer.hpp
│   ├── errorReporter
│   │   └── ErrorReporter.hpp
│   ├── lexer
│   │   ├── Lexer.hpp
│   │   └── Token.hpp
│   ├── optimize
│   │   ├── DominatorTree.hpp
│   │   ├── GlobalConstEval.hpp
│   │   ├── LICM.hpp
│   │   ├── LoopAnalysis.hpp
│   │   ├── LoopUnroll.hpp
│   │   ├── Mem2Reg.hpp
│   │   └── PhiElimination.hpp
│   ├── parser
│   │   ├── AST.hpp
│   │   └── Parser.hpp
│   └── semantic
│       ├── SemanticAnalyzer.hpp
│       ├── Symbol.hpp
│       ├── SymbolTable.hpp
│       └── Type.hpp
├── LICENSE
├── main.cpp
├── MARS2025+.jar
├── README.md
├── scripts
│   └── test_mips.sh
└── src
    ├── backend
    │   ├── AsmGen.cpp
    │   └── RegisterAllocator.cpp
    ├── CMakeLists.txt
    ├── codegen
    │   ├── BasicBlock.cpp
    │   ├── CodeGen.cpp
    │   ├── Function.cpp
    │   ├── Instruction.cpp
    │   ├── Operand.cpp
    │   └── QuadOptimizer.cpp
    ├── errorReporter
    │   └── ErrorReporter.cpp
    ├── lexer
    │   └── Lexer.cpp
    ├── optimize
    │   ├── DominatorTree.cpp
    │   ├── GlobalConstEval.cpp
    │   ├── LICM.cpp
    │   ├── LoopAnalysis.cpp
    │   ├── LoopUnroll.cpp
    │   ├── Mem2Reg.cpp
    │   └── PhiElimination.cpp
    ├── parser
    │   └── Parser.cpp
    └── semantic
        └── SemanticAnalyzer.cpp

19 directories, 64 files
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
- `CodeGen` 依赖语义分析后的 `AST` 与符号表，生成 IR
- `Backend`（`AsmGen` + `RegisterAllocator`）依赖 IR，生成 MIPS 汇编
- `ErrorReporter` 被 `Lexer`/`Parser`/`SemanticAnalyzer` 共用，用来集中记录所有错误

## 编译流水线

整个编译过程从源文件 `testfile.txt` 到最终的 MIPS 汇编 `mips.txt`，大致分为以下阶段：

1. **输入与输出重定向**
    - 将错误输出重定向到 `error.txt`，用于记录所有阶段产生的错误信息。
    - 将标准输出重定向到 `ir.txt`，用于记录中间表示（IR）或相关调试信息。

2. **词法分析（`Lexer`）**
    - 根据文法定义和保留字表，将源代码字符流切分为 Token 流。
    - 识别关键字、标识符、各类常量、运算符与分隔符。
    - 发现非法字符（如单独的 `&`、`|` 等）时，记录错误类型 `a`，通过 ErrorReporter 统一收集。

3. **语法分析（`Parser`）**
    - 采用递归下降方式，根据给定的 EBNF 将 Token 流转为 AST。
    - 维护非终结符的行号信息，为后续错误定位服务。
    - 在缺分号、右括号等情况下记录 `i/j/k` 类型错误，同时尽量进行错误恢复。

4. **语义分析（`SemanticAnalyzer`）**
    - 在栈式符号表上完成声明、定义与使用检查。
    - 检查函数调用的参数个数与类型、返回语句的存在性与类型匹配、常量赋值合法性、循环控制语句位置等。
    - 将所有 `b`–`m` 类型的语义错误通过 `ErrorReporter` 统一收集。
    - 语义分析结束后导出完整的 `SymbolTable`，供 IR 生成阶段使用。

5. **错误收集与早期退出**
    - 若 `ErrorReporter` 中存在任意错误，则按照行号排序输出到 `error.txt`，并终止后续阶段。

6. **IR 生成与优化（`CodeGen`）**
    - 在语义分析通过的前提下，从 AST 和符号表生成函数级别 IR：
      - 构建函数、基本块与四元式指令序列。
      - 按 `ir.md` 中约定的 OpCode 与操作数规则生成中间代码。
    - 在 IR 上应用轻量级优化（例如常量折叠与块内死代码删除），具体算法详见 `ir.md` 与 `backend.md` 中的说明。
    - 汇总得到 IR 模块视图（函数列表、全局变量 IR、字符串字面量表）。

7. **后端 MIPS 代码生成（`RegisterAllocator` / `AsmGen`）**
    - 使用 `RegisterAllocator` 在函数内为 IR 临时变量分配有限数量的物理寄存器（图着色算法）。
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
- 优化策略：`optimize.md`

## 工作量统计

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Language              Files        Lines         Code     Comments       Blanks
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 CMake                     2           60           48            1           11
 C++                      20         8190         7281          234          675
 C++ Header               24         2465         1283          903          279
 JSON                      1            4            4            0            0
 Shell                     1            9            4            1            4
 TOML                      1           92           52           37            3
─────────────────────────────────────────────────────────────────────────────────
 Markdown                 12         2455            0         1776          679
 |- BASH                   1            7            7            0            0
 |- C++                    2          122          121            0            1
 (Total)                             2584          128         1776          680
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Total                    61        13404         8800         2952         1652
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```
