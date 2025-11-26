# LCC 编译器设计文档总览

## 1. 引言

LCC（Lightweight C Compiler）采用经典的多阶段编译架构，目标平台为 MIPS 架构。本文档概述了编译器各阶段的核心设计思想。

### 1.1 整体架构

```
.
├── CMakeLists.txt
├── config.json
├── doc
│   ├── compiler.md
│   ├── EBNF.md
│   ├── error.md
│   ├── errorReporter.md
│   ├── IR_DESIGN.md
│   ├── lexer.md
│   ├── overview.md
│   ├── parser.md
│   ├── REGISTER_CONVENTION.md
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
    ├── parser
    │   └── Parser.cpp
    └── semantic
        └── SemanticAnalyzer.cpp
```

---

## 2. 词法分析

### 2.1 核心设计

词法分析器将源代码字符流转换为Token流。

**设计要点：**

- **宏Token定义**：使用 `TOKEN_LIST` `X`宏统一管理所有Token类型
- **关键字表**：使用 `std::unordered_map` 存储保留字，O(1) 识别
- **注释处理**：遇到注释自动跳过，递归调用返回下一个有效Token

### 2.2 静默模式

**静默深度机制**：

- 使用**引用计数**而非布尔值，支持多层嵌套的静默(这是因为我的向前看已经和输出绑定了，需要引入一个机制来解绑定)
- 当 `silentDepth > 0` 时，禁止输出和错误打印
- 用于Parser的向前看，避免污染输出流

---

## 3. 语法分析

### 3.1 核心设计

语法分析器采用**递归下降解析**，将Token流构建为抽象语法树（AST）。

**设计要点：**

- **EBNF驱动**：每个非终结符对应一个解析函数
- **左递归消除**：将左递归文法改写为右递归
- **Token前瞻**：通过 `lexer.peekToken(n)` 实现向前看`n`个`Token`
- **错误恢复**：通过 `expect()` 机制进行错误检测,遇到错误忽略

### 3.2 二义性处理

**试探性解析策略**：对于标识符开头的语句（赋值 vs 表达式语句），采用以下方法：

1. 保存当前状态
2. 静默深度++，尝试解析
3. 根据后续Token判断类型
4. 静默深度--，按正确类型解析

---

## 4. 语义分析

### 4.1 核心设计

语义分析器遍历AST，构建符号表并进行类型检查。

**设计要点：**

- **单遍遍历**：采用访问者模式，一次遍历完成所有语义检查
- **类型回填**：将推导出的类型信息回填到AST节点
- **栈式符号表**：支持作用域嵌套，自动管理符号生命周期

### 4.2 类型系统

**三层分类（Category）**：

- `Basic`：基本类型（int, void）
- `Array`：数组类型，记录元素类型和维度
- `Function`：函数类型，记录返回类型和参数列表

### 4.3 符号表设计

**栈式作用域管理**：

- `records`：存储所有作用域历史
- `active`：索引栈维护当前活跃作用域链
- 支持从内层向外层逐层查找符号

---

## 5. 中间代码生成

### 5.1 IR 模型概述

LCC 采用**四元式形式的三地址码**作为中间表示，组织为**基本块（Basic Block）**和**控制流图（CFG）**。

**设计目标：**

- **简洁性**：易于从AST生成，形成一些中间数据结构，易于翻译到MIPS
- **可读性**：便于调试和优化
- **非SSA形式**：变量可多次赋值，降低实现复杂度，但是在中间数据结构的形式是单次赋值的,会分配一个递增id，目前未实现phi函数

**层次结构：**

```
Module (整个程序)
├── GlobalInstructions (全局变量定义)
└── Function[]
    └── BasicBlock[]
        └── Instruction[]
```

### 5.2 操作数设计

操作数是IR指令的参数或结果，统一表示为：

```cpp
enum class OperandType { Empty, Variable, Temporary, ConstantInt, Label };

class Operand {
public:
  Operand();
  explicit Operand(std::shared_ptr<Symbol> symbol);
  static Operand Temporary(int tempId);
  static Operand ConstantInt(int v);
  static Operand Label(int id);
  static Operand Variable(std::shared_ptr<Symbol> sym);

  OperandType getType() const { return _type; }
  std::string toString() const;

  const std::shared_ptr<Symbol> &asSymbol() const; // Variable
  int asInt() const; // Temporary / ConstantInt / Label id

private:
  Operand(OperandType t, std::variant<std::shared_ptr<Symbol>, int> val);
  OperandType _type;
  std::variant<std::shared_ptr<Symbol>, int> _value;
};
```

**四种操作数类型：**

| 类型 | 说明 | 示例 | 语义 |
|------|------|------|------|
| `Variable` | 程序变量 | `x`, `arr`, `func` | 关联符号表，包含类型信息 |
| `Temporary` | 临时变量 | `%t0`, `%t1` | 中间计算结果，ID唯一标识 |
| `ConstantInt` | 整数常量 | `42`, `0`, `-1` | 32位有符号整数 |
| `Label` | 跳转标签 | `L0`, `L1` | 基本块入口，ID唯一标识 |

**设计要点：**

- 统一的操作数表示简化指令处理逻辑
- 符号内联（Symbol Interning）：全局唯一的符号对象，支持指针比较
- 临时变量自动分配，避免命名冲突

### 5.3 指令集设计

指令采用四元式格式：`(OpCode, src1, src2, dest)`,以下的`-`代表置空

#### 5.3.1 算术运算

| 指令 | 格式 | 语义 | MIPS映射 |
|------|------|------|----------|
| `ADD` | `ADD a, b, rd` | `rd = a + b` | `addu rd, ra, rb` |
| `SUB` | `SUB a, b, rd` | `rd = a - b` | `subu rd, ra, rb` |
| `MUL` | `MUL a, b, rd` | `rd = a * b` | `mul rd, ra, rb` |
| `DIV` | `DIV a, b, rd` | `rd = a / b` | `div ra, rb; mflo rd` |
| `MOD` | `MOD a, b, rd` | `rd = a % b` | `div ra, rb; mfhi rd` |
| `NEG` | `NEG a, -, rd` | `rd = -a` | `subu rd, $zero, ra` |

**设计要点**：

- `ADD`,`SUB`,`MUL`,`DIV`,`MOD` 这些都是`Binary`类型的二元运算
- `NEG`是`Unary`类型的一元运算
- 在实现中使用`MakeBinary`和`MakeUnary`工厂函数来封装实现

#### 5.3.2 比较运算

比较运算结果恒为 `0`或 `1`。

| 指令 | 格式 | 语义 | 实现策略 |
|------|------|------|----------|
| `EQ` | `EQ a, b, rd` | `rd = (a == b)` | `sub + sltiu` 组合 |
| `NEQ` | `NEQ a, b, rd` | `rd = (a != b)` | `sub + sltu` 组合 |
| `LT` | `LT a, b, rd` | `rd = (a < b)` | `slt rd, ra, rb` |
| `LE` | `LE a, b, rd` | `rd = (a <= b)` | `slt + xori` 组合 |
| `GT` | `GT a, b, rd` | `rd = (a > b)` | `slt rd, rb, ra`（交换操作数）|
| `GE` | `GE a, b, rd` | `rd = (a >= b)` | `slt + xori` 组合 |

#### 5.3.3 逻辑运算

逻辑运算结果恒为 `0`或 `1`。

| 指令 | 格式 | 语义 | 实现策略 |
|------|------|------|----------|
| `AND`| `AND arg1, arg2, rd` | `rd = arg1 && arg2` | `and + sltiu` 组合 |
| `OR` | `OR arg1, arg2, rd` | `rd = arg1 \|\| arg2` | `or + sltu` 组合 |
| `NOT`| `NOT arg, -, rd` | `rd = !arg` | `sltiu rd, ra, 1` |

**设计要点**：

- `AND` `OR`,这是都是`Binary`类型的二元运算
- `NOT`是`Unary`类型的一元运算
- 在实现中使用`MakeBinary`和`MakeUnary`工厂函数来封装实现

#### 5.3.4 赋值操作

赋值采用**值语义**，即右值计算后赋给左值。

| 指令 | 格式 | 语义 | 实现策略 |
|------|------|------|----------|
| `ASSIGN` | `ASSIGN src, -, dst` | `dst = src` | 直接赋值 |

**设计要点**

- `ASSIGN`在此`EBNF`文法中是一条语句，不是一个表达式，因此我们没有把`res`放到临时变量中

#### 5.3.5 内存访问

数组按**地址语义**处理，索引为**元素索引**（非字节偏移）。

| 指令 | 格式 | 语义 | 地址计算 |
|------|------|------|----------|
| `LOAD` | `LOAD base, idx, dst` | `dst = base[idx]` | `addr = base + idx * 4` |
| `STORE` | `STORE val, base, idx` | `base[idx] = val` | `addr = base + idx * 4` |

**设计要点**

- `LOAD` 以及`STORE`用于元素的读写操作，主要在后端代码生成阶段读取在栈帧上的变量或者在`.data`上的变量

#### 5.3.6 控制流

| 指令 | 格式 | 语义 | 用途 |
|------|------|------|------|
| `LABEL` | `LABEL L` | 定义标签L | 基本块入口 |
| `GOTO` | `GOTO L` | 无条件跳转到L | goto, 循环 |
| `IF` | `IF cond - L` | 条件跳转到L | if + goto label |
**设计要点**：

-

#### 5.3.5 函数调用

函数调用分为三步：参数传递、调用、返回值接收。

| 指令 | 格式 | 语义 |
|------|------|------|
| `PARAM` | `PARAM arg1 - res` | 追加参数到参数列表以及函数声明 |
| `CALL` | `CALL argc, func, ret` | 调用函数，ret接收返回值 |
| `RETURN` | `RETURN - - res` | 函数返回,返回值与`genExp`相关，若`AST`节点无对应信息，则返回一个`EMPTY`的`Operand`|

**设计要点**：

- `PARAM`指令采取二合一的策略，`CALLER`和`CALLEE`都是用他来生成中间代码
  - 对于`CALLER`,`res`总是置`EMPTY`,`CALLER`只负责把参数压入栈中,这个参数可能是`Variable`,`Temporary`,`ConstantInt`
  - 对于`CALLEE`,也就是`Function`,负责声明参数列表和数量,`arg1`代表参数索引（从0开始），`res`代表参数符号
  
#### 5.3.6 声明与分配

| 指令 | 格式 | 语义 |
|------|------|------|
| `ALLOCA` | `ALLOCA base - size` | 分配空间，`size`单位为word,后端根据变量语义看分配在栈帧还是`.data`段 |

### 5.4 短路求值（核心特性）

逻辑运算符（`&&`, `||`）采用**控制流实现**而非值计算，支持短路求值。

#### 5.4.1 双模式生成

**条件模式**（用于 `if/for` 的条件）：

```c
// C: if (a && b) { then_block }
```

```
// IR:
BEQ a, 0, false_label    // a为假，跳过b的求值
BEQ b, 0, false_label    // b为假，跳转到false
// then_block
JUMP end_label
LABEL false_label
// else_block (可选)
LABEL end_label
```

**值模式**（用于赋值语句）：

```c
// C: int result = a && b;
```

```
// IR:
BEQ a, 0, false_label    // a为假，结果为0
BEQ b, 0, false_label    // b为假，结果为0
ASSIGN 1, -, %t0         // 都为真，结果为1
JUMP end_label
LABEL false_label
ASSIGN 0, -, %t0         // 结果为0
LABEL end_label
ASSIGN %t0, -, result
```

#### 5.4.2 逻辑或（||）

```c
// C: if (a || b) { then_block }
```

```
// IR:
BNE a, 0, true_label     // a为真，直接进入then
BNE b, 0, true_label     // b为真，进入then
// else_block
JUMP end_label
LABEL true_label
// then_block
LABEL end_label
```

**设计理由：**

- 避免不必要的计算，生成高效的MIPS代码
- 双模式设计兼顾条件表达式和值表达式的不同需求

### 5.5 基本块（Basic Block）划分

**基本块性质：**

- **单入口**：只能从第一条指令进入
- **单出口**：只有最后一条指令可能是跳转或返回
- **顺序执行**：中间指令无跳转

**自动划分规则：**

1. 遇到 `LABEL` 指令，开始新的基本块
2. 遇到跳转指令（`JUMP/BEQ/BNE/RET`），结束当前基本块
3. 跳转指令的下一条指令（如果存在）开始新的基本块

**控制流连接：**

- 顺序后继（fallthrough）：条件跳转的不跳转分支
- 跳转后继（jumpTarget）：跳转指令的目标

**设计理由：**

- 基本块是数据流分析和优化的基本单位
- 为寄存器分配的活跃性分析提供了清晰的分析边界

### 5.6 符号内联（Symbol Interning）

**设计思想：**

- 全局唯一的符号对象，相同名称的符号共享同一对象
- 支持指针比较，`sym1.get() == sym2.get()` 即可判断是否为同一符号
- 简化IR优化中的符号查找和比较

**实现：**

```cpp
std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols_;

std::shared_ptr<Symbol> internSymbol(const std::string &name, TypePtr type) {
  auto it = symbols_.find(name);
  if (it != symbols_.end()) return it->second;
  auto sym = std::make_shared<Symbol>(name, type);
  symbols_[name] = sym;
  return sym;
}
```

### 5.7 IR 优化

#### 5.7.1 常量折叠（Constant Folding）

**策略**：在IR生成阶段，尝试编译期计算常量表达式。

**示例：**

```c
// C: int x = 2 + 3;
// 优化前IR:
ADD 2, 3, %t0
ASSIGN %t0, -, x

// 优化后IR:
ASSIGN 5, -, x    // 常量折叠为5
```

#### 5.7.2 死代码消除（Dead Code Elimination）

**策略**：移除无法到达的基本块。

**示例：**

```c
// C: return 1; x = 2;  // return后的代码无法到达
// 优化：移除 x = 2 所在的基本块
```

---

## 6. 后端代码生成（Backend Code Generation）

### 6.1 核心设计

后端代码生成器（AsmGen）将IR翻译为MIPS汇编代码，核心任务包括：

- **寄存器分配**：图着色算法
- **指令选择**：IR指令到MIPS指令的映射
- **栈帧管理**：局部变量和参数的内存布局

### 6.2 MIPS 寄存器概览

MIPS架构提供32个通用寄存器，按用途分类：

| 寄存器 | 名称 | 用途 | 调用约定 |
|--------|------|------|----------|
| `$zero` | 常量0 | 硬件常量 | N/A |
| `$v0-$v1` | 返回值 | 函数返回值 | Caller-Save |
| `$a0-$a3` | 参数 | 前4个参数 | Caller-Save |
| `$t0-$t7` | 临时 | 可分配寄存器 | Caller-Save |
| `$t8-$t9` | 临时 | Scratch寄存器 | Caller-Save |
| `$s0-$s7` | 保存 | 跨函数保存 | Callee-Save（未使用）|
| `$sp` | 栈指针 | 栈顶 | Callee-Save |
| `$fp` | 帧指针 | 栈帧基址 | Callee-Save |
| `$ra` | 返回地址 | 函数返回 | Callee-Save |

### 6.3 寄存器使用策略

#### 6.3.1 寄存器分类

**分配寄存器（Allocatable Registers）**：

- **寄存器**：`$t0-$t7`（共8个）
- **用途**：通过图着色算法动态分配给IR临时变量
- **特点**：参与寄存器分配，生命周期由活跃性分析确定

**Scratch 寄存器（Scratch Registers）**：

- **寄存器**：`$t8, $t9`（共2个）
- **用途**：指令翻译过程中的临时计算
- **特点**：**不参与**寄存器分配，每条IR指令翻译后自动释放

**参数寄存器**：`$a0-$a3`，用于函数调用时传递前4个参数

**返回值寄存器**：`$v0`，存放函数返回值

**特殊寄存器**：`$ra`（返回地址）、`$sp`（栈指针）、`$fp`（帧指针）

### 6.4 寄存器分配算法

#### 6.4.1 图着色（Graph Coloring）

**核心思想**：将寄存器分配问题转化为图着色问题。

**步骤：**

**1. 活跃性分析（Liveness Analysis）**

基于数据流方程计算每个基本块的活跃变量集合：

```
liveOut[B] = ∪(liveIn[S]) for all successors S of B
liveIn[B]  = use[B] ∪ (liveOut[B] - def[B])
```

- `use[B]`：基本块B中在定义前使用的变量
- `def[B]`：基本块B中被定义的变量
- `liveOut[B]`：基本块B结束时活跃的变量
- `liveIn[B]`：基本块B开始时活跃的变量

迭代计算直到不动点（所有基本块的 `liveIn` 和 `liveOut` 不再变化）。

**2. 构建冲突图（Interference Graph）**

- **节点**：所有IR临时变量
- **边**：如果两个临时变量的生命周期重叠（同时活跃），则它们之间有边
- 判断依据：如果 `t1 ∈ liveOut[B]` 且 `t2 ∈ def[B]`，则 `t1` 和 `t2` 冲突

**3. 图着色**

- **简化**：迭代移除度数 < K 的节点（K=8，可分配寄存器数量），压入栈
- **潜在溢出**：若所有节点度数 ≥ K，选择一个节点标记为潜在溢出
- **着色**：从栈中弹出节点，为每个节点选择一个颜色（寄存器ID：0-7），该颜色不与已着色的邻居冲突
- **溢出**：若无法找到可用颜色，标记为溢出（spill）

**4. 寄存器映射**

| 颜色ID | 物理寄存器 |
|--------|-----------|
| 0      | `$t0`     |
| 1      | `$t1`     |
| 2      | `$t2`     |
| 3      | `$t3`     |
| 4      | `$t4`     |
| 5      | `$t5`     |
| 6      | `$t6`     |
| 7      | `$t7`     |

**5. 溢出处理（Spill Handling）**

- 溢出的临时变量分配在栈帧中
- 使用时从内存加载到scratch寄存器（`$t8/$t9`）
- 定义时从scratch寄存器存储到内存

**示例：**

```mips
# 假设 %t10 溢出到 spillOffset($fp)
# IR: ADD %t10, %t1, %t2
lw $t8, spillOffset($fp)   # 加载溢出的 %t10
addu $t2, $t8, $t1          # 执行计算
```

#### 6.4.2 设计理由

- **图着色是NP完全问题**，但对于小规模（函数级别）问题，贪心算法效果良好
- **8个可分配寄存器**足以应对大多数函数的寄存器需求
- **Caller-Save策略**简化实现，函数调用时所有 `$t` 寄存器失效

### 6.5 Scratch 寄存器管理

#### 6.5.1 引用计数机制

```cpp
struct ScratchRegState {
  std::string name;  // "$t8" or "$t9"
  int refCount = 0;  // 当前引用计数
};
```

**操作：**

- `allocateScratch()`：优先选择 `refCount == 0` 的寄存器，增加引用计数
- `releaseScratch(reg)`：减少指定寄存器的引用计数
- `resetScratchState()`：每条IR指令翻译完成后调用，重置所有引用计数为0

#### 6.5.2 使用场景

**1. 加载立即数**

```mips
li $t8, 42
```

**2. 加载变量**

```mips
lw $t8, offset($fp)     # 局部变量
la $t8, global_var      # 全局变量
lw $t8, 0($t8)
```

**3. 溢出变量的加载与存储**

```mips
lw $t8, spillOffset($fp)  # 加载溢出变量
# ... 使用 $t8 ...
sw $t8, spillOffset($fp)  # 存回溢出位置
```

**4. 数组地址计算**

```mips
la $t8, array_base       # 基址
sll $t9, index, 2        # index * 4
addu $t8, $t8, $t9       # base + offset
lw $t8, 0($t8)           # 加载元素
```

#### 6.5.3 冲突避免

在翻译二元运算时，确保两个操作数使用不同的scratch寄存器：

```cpp
std::string ra = ensureInReg(a1, out, "$t8", "$t8");
const char *rb_scratch = (ra == "$t8") ? "$t9" : "$t8";
std::string rb = ensureInReg(a2, out, rb_scratch, rb_scratch);
```

### 6.6 函数调用约定

#### 6.6.1 调用者（Caller）

**函数调用前：**

1. 准备参数：前4个参数放入 `$a0-$a3`，第5+个参数压栈
2. 调用函数：`jal function_name`
3. 获取返回值：返回值在 `$v0` 中

#### 6.6.2 被调用者（Callee）

**函数序言（Prologue）：**

```mips
function_name:
    # 1. 分配栈帧
    addiu $sp, $sp, -frameSize
    
    # 2. 保存 $ra 和 $fp
    sw $ra, 0($sp)
    sw $fp, 4($sp)
    
    # 3. 设置新的帧指针
    move $fp, $sp
    
    sw $a0, 8($fp)      # 第1个参数
    sw $a1, 12($fp)     # 第2个参数
    sw $a2, 16($fp)     # 第3个参数
    sw $a3, 20($fp)     # 第4个参数
```

**函数尾声（Epilogue）：**

```mips
function_END:
    # 1. 恢复 $ra 和 $fp
    lw $ra, 0($fp)
    lw $fp, 4($fp)
    
    # 2. 恢复栈指针
    addiu $sp, $sp, frameSize
    
    # 3. 返回
    jr $ra
```

**对齐要求**：栈帧大小向上取整到8字节对齐。

---

## 7. 错误处理机制

### 7.1 统一错误管理

**ErrorReporter单例模式**：

- 各阶段通过 `ErrorReporter::getInstance().addError()` 报告错误
- 错误信息包含行号和错误类型
- 最终统一按行号排序输出到 `error.txt`

**设计理由**：

- 统一管理避免错误信息分散
- 延迟输出支持错误排序和去重
- 单例模式保证全局唯一的错误收集器

### 7.2 错误分类

| 阶段     | 错误类型           | 错误代码 | 示例 |
|----------|-------------------|---------|------|
| 词法分析 | 非法符号           | a       | `@`, `#` |
| 语法分析 | 缺少分号           | i       | `int x` |
| 语法分析 | 缺少右括号 `)`     | j       | `if (x > 0 {` |
| 语法分析 | 缺少右括号 `]`     | k       | `arr[0 = 1;` |
| 语义分析 | 标识符重定义       | b       | `int x; int x;` |
| 语义分析 | 标识符未定义       | c       | `y = x;`（x未定义）|
| 语义分析 | 函数参数数量不匹配 | d       | `foo(1)`（期望2个）|
| 语义分析 | 函数参数类型不匹配 | e       | `foo(arr)`（期望标量）|
| 语义分析 | void函数返回值错误 | f       | `void f() { return 1; }` |
| 语义分析 | 缺少返回语句       | g       | `int f() { }`（无return）|
| 语义分析 | 常量赋值           | h       | `const int x=1; x=2;` |
| 语义分析 | printf参数不匹配   | l       | `printf("%d%d", x)`（缺一个）|
| 语义分析 | break/continue错误 | m       | `if (x) break;` |

---

## 11. 参考文档

### 11.1 语言与文法

- [EBNF.md](EBNF.md) - 完整的语言文法定义（BNF范式）

### 11.2 前端设计

- [lexer.md](lexer.md) - 词法分析器详细设计
- [parser.md](parser.md) - 语法分析器详细设计
- [semantic.md](semantic.md) - 语义分析器详细设计

### 11.3 中间表示与后端

- [IR_DESIGN.md](IR_DESIGN.md) - 中间代码详细设计（指令集、优化）
- [REGISTER_CONVENTION.md](REGISTER_CONVENTION.md) - 寄存器约定详细说明

### 11.4 错误处理

- [error.md](error.md) - 错误处理机制
- [errorReporter.md](errorReporter.md) - ErrorReporter实现

---

## 12. 总结

### 12.1 核心设计原则

1. **模块化与职责分离**：各阶段独立，接口清晰
2. **经典算法实践**：递归下降解析、图着色寄存器分配、数据流分析
3. **渐进式信息构建**：Token → AST → 类型化AST → IR → Assembly
4. **简洁性优先**：非SSA形式、简单的优化策略、清晰的调用约定

### 12.2 重点设计特色

**IR设计**：

- 四元式风格，易于理解和调试
- 基本块组织，为优化提供基础
- 短路求值的双模式生成

**寄存器分配**：

- 图着色算法的经典实现
- 活跃性分析驱动
- Scratch寄存器的引用计数管理

**前端机制**：

- 静默模式的引用计数设计
- 试探性解析处理二义性
- 栈式符号表的优雅实现
