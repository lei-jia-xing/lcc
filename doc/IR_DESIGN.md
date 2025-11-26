# 中间代码（IR）设计文档

## 概述

LCC 编译器采用基于三地址码（Three-Address Code）的中间表示（Intermediate Representation, IR）。本 IR 设计为非 SSA（Static Single Assignment）形式，按基本块（Basic Block）组织为控制流图（Control Flow Graph, CFG）。

**设计目标**：

- **简洁性**：易于从 AST 生成，易于翻译到 MIPS 汇编
- **可读性**：便于调试和优化
- **扩展性**：为未来的优化 pass 提供良好基础

**核心特性**：

- 四元式风格：`(op, arg1, arg2, result)`
- 非 SSA：变量可多次赋值
- 显式控制流：通过 `LABEL`、`GOTO`、`IF` 构造
- 类型简单：仅支持 32 位有符号整型和 `void`
- 数组语义：数组按"地址"传递，标量按"值"传递

## IR 模型

### 1. 层次结构

```
Module (整个程序)
├── GlobalInstructions (全局变量定义和初始化)
├── Function 1
│   ├── BasicBlock 1
│   │   ├── Instruction 1
│   │   ├── Instruction 2
│   │   └── ...
│   ├── BasicBlock 2
│   └── ...
├── Function 2
└── ...
```

### 2. 操作数（Operand）

操作数是 IR 指令的参数或结果，有以下类型：

| 类型 | 说明 | 示例 | 用途 |
|------|------|------|------|
| `Variable` | 源程序中的变量/函数名 | `x`, `arr`, `func` | 变量、数组、函数 |
| `Temporary` | 编译器生成的临时名 | `%t0`, `%t1` | 中间计算结果 |
| `ConstantInt` | 整数常量 | `0`, `42`, `-1` | 立即数 |
| `Label` | 跳转目标标签 | `L0`, `L1` | 控制流 |

**设计说明**：

- **Variable**：关联符号表条目，包含类型信息（是否数组、是否常量等）
- **Temporary**：由 `CodeGen` 自动分配，通常一次赋值（但非强制 SSA）
- **ConstantInt**：32 位有符号整数，范围 [-2³¹, 2³¹-1]
- **Label**：基本块入口，用整数 ID 标识（如 `L0` 表示 ID 为 0）

### 3. 基本块（BasicBlock）

基本块是线性指令序列，满足以下性质：

- **单入口**：只有第一条指令是标签或从前一个基本块顺序进入
- **单出口**：只有最后一条指令可能是跳转（`GOTO`、`IF`）或返回（`RETURN`）
- **顺序执行**：中间指令按顺序执行，无跳转

**控制流连接**：

- `next`：顺序后继（fallthrough）
- `jumpTarget`：跳转目标（`GOTO` 或 `IF` 的目标）

### 4. 函数（Function）

函数包含：

- **名称**：函数标识符
- **参数列表**：形式参数（符号表条目）
- **返回类型**：`int` 或 `void`
- **基本块序列**：函数体的线性化表示

## 指令集

### 1. 算术与一元运算

#### 1.1 二元算术

| 指令 | 语法 | 语义 | MIPS 对应 |
|------|------|------|-----------|
| `ADD` | `ADD a, b, rd` | `rd = a + b` | `addu` |
| `SUB` | `SUB a, b, rd` | `rd = a - b` | `subu` |
| `MUL` | `MUL a, b, rd` | `rd = a * b` | `mul` |
| `DIV` | `DIV a, b, rd` | `rd = a / b` | `div` + `mflo` |
| `MOD` | `MOD a, b, rd` | `rd = a % b` | `div` + `mfhi` |

**示例**：

```
# C: int z = x + y;
ADD x, y, %t0
ASSIGN %t0, -, z

# C: int w = (a + b) * c;
ADD a, b, %t0
MUL %t0, c, %t1
ASSIGN %t1, -, w
```

**语义说明**：

- 除法向零截断（truncate towards zero）
- 取模结果符号与被除数相同
- 溢出行为未定义（依赖硬件）

#### 1.2 一元运算

| 指令 | 语法 | 语义 | MIPS 对应 |
|------|------|------|-----------|
| `NEG` | `NEG a, -, rd` | `rd = -a` | `subu rd, $zero, a` |

**示例**：

```
# C: int y = -x;
NEG x, -, %t0
ASSIGN %t0, -, y
```

### 2. 比较运算

比较运算结果恒为 `0`（假）或 `1`（真）。

| 指令 | 语法 | 语义 |
|------|------|------|
| `EQ` | `EQ a, b, rd` | `rd = (a == b)` |
| `NEQ` | `NEQ a, b, rd` | `rd = (a != b)` |
| `LT` | `LT a, b, rd` | `rd = (a < b)` |
| `LE` | `LE a, b, rd` | `rd = (a <= b)` |
| `GT` | `GT a, b, rd` | `rd = (a > b)` |
| `GE` | `GE a, b, rd` | `rd = (a >= b)` |

**示例**：

```
# C: int flag = x < y;
LT x, y, %t0
ASSIGN %t0, -, flag

# C: int result = (a >= b);
GE a, b, %t0
ASSIGN %t0, -, result
```

### 3. 逻辑运算

逻辑运算通常结合控制流使用，支持短路求值。

| 指令 | 语法 | 语义 |
|------|------|------|
| `AND` | `AND a, b, rd` | `rd = (a && b)` |
| `OR` | `OR a, b, rd` | `rd = (a \|\| b)` |
| `NOT` | `NOT a, -, rd` | `rd = (!a)` |

**示例**：

```
# C: int result = !flag;
NOT flag, -, %t0
ASSIGN %t0, -, result

# C: if (a && b) { ... }
# 通常展开为控制流，见"短路求值"章节
```

### 4. 赋值与数据移动

| 指令 | 语法 | 语义 |
|------|------|------|
| `ASSIGN` | `ASSIGN src, -, dst` | `dst = src` |

**示例**：

```
# C: x = 42;
ASSIGN 42, -, x

# C: y = x;
ASSIGN x, -, y

# 临时变量赋值
ASSIGN %t0, -, %t1
```

### 5. 内存访问

数组按地址语义处理，索引为元素索引（非字节偏移）。

| 指令 | 语法 | 语义 |
|------|------|------|
| `LOAD` | `LOAD base, idx, dst` | `dst = base[idx]` |
| `STORE` | `STORE val, base, idx` | `base[idx] = val` |

**示例**：

```
# C: int x = arr[i];
LOAD arr, i, %t0
ASSIGN %t0, -, x

# C: arr[i] = 42;
STORE 42, arr, i

# C: arr[i] = arr[j];
LOAD arr, j, %t0
STORE %t0, arr, i

# 二维数组：arr[i][j]（假设列数为 COL）
# int offset = i * COL + j;
MUL i, COL, %t0
ADD %t0, j, %t1
LOAD arr, %t1, %t2
```

**地址计算**：

- IR 层：索引为元素索引
- 后端：`address = base + index * element_size`（MIPS 中 `int` 为 4 字节）

### 6. 控制流

| 指令 | 语法 | 语义 |
|------|------|------|
| `LABEL` | `LABEL L` | 定义标签 L（基本块入口） |
| `GOTO` | `GOTO L` | 无条件跳转到 L |
| `IF` | `IF cond, L` | 若 `cond != 0`，跳转到 L |

**示例**：

#### 6.1 条件语句

```c
// C code:
if (x > 0) {
    y = 1;
} else {
    y = 2;
}
```

```
// IR:
GT x, 0, %t0
IF %t0, L0         # 如果 x > 0，跳转到 L0（then 分支）
ASSIGN 2, -, y     # else 分支
GOTO L1            # 跳过 then 分支
LABEL L0           # then 分支开始
ASSIGN 1, -, y
LABEL L1           # if 语句结束
```

#### 6.2 循环语句

```c
// C code:
for (i = 0; i < 10; i = i + 1) {
    sum = sum + i;
}
```

```
// IR:
ASSIGN 0, -, i          # 初始化
LABEL L0                # 循环开始
LT i, 10, %t0           # 条件判断
IF %t0, L1              # 条件为真，进入循环体
GOTO L2                 # 条件为假，跳出循环
LABEL L1                # 循环体
ADD sum, i, %t1
ASSIGN %t1, -, sum
ADD i, 1, %t2           # 更新
ASSIGN %t2, -, i
GOTO L0                 # 回到循环开始
LABEL L2                # 循环结束
```

### 7. 函数相关

#### 7.1 调用协议

调用函数分为三步：

1. **参数传递**：连续发射 `PARAM` 指令
2. **调用**：`CALL` 指令，指定参数数量、函数名、返回值目标
3. **返回值**：通过 `CALL` 的第三个参数接收（可为空）

| 指令 | 语法 | 语义 |
|------|------|------|
| `PARAM` | `PARAM arg` | 追加一个参数到参数列表 |
| `CALL` | `CALL argc, func, ret` | 调用函数，`ret` 接收返回值 |
| `RETURN` | `RETURN [val]` | 返回（可带返回值） |

**示例**：

```c
// C code:
int result = foo(1, 2, x);
```

```
// IR:
PARAM 1
PARAM 2
PARAM x
CALL 3, foo, %t0
ASSIGN %t0, -, result
```

```c
// C code:
void bar(int a) {
    printf("%d", a);
}
```

```
// IR (函数定义):
// 函数: bar(int a)
PARAM a               # 形式参数（语义层面，实际不生成此指令）
PRINTF "%d", a
RETURN                # void 函数

// IR (调用):
PARAM 42
CALL 1, bar, -        # 无返回值
```

#### 7.2 返回语句

| 指令 | 语法 | 用途 |
|------|------|------|
| `RETURN` | `RETURN val` | 返回值 `val` |
| `RETURN` | `RETURN` | `void` 函数返回 |

**示例**：

```c
int max(int a, int b) {
    if (a > b) return a;
    return b;
}
```

```
// IR:
// 函数: max(int a, int b)
GT a, b, %t0
IF %t0, L0
RETURN b           # else 分支
LABEL L0
RETURN a           # then 分支
```

### 8. 声明与分配

| 指令 | 语法 | 语义 |
|------|------|------|
| `ALLOCA` | `ALLOCA sym, size` | 为符号 `sym` 分配 `size` 个元素的空间 |

**用途**：

- 全局变量定义（`.data` 段）
- 局部变量分配（栈帧）
- 数组分配（元素数量）

**示例**：

```c
// 全局变量
int global_x;
int global_arr[10];

// 局部变量
void func() {
    int local_y;
    int local_arr[5];
}
```

```
// IR (全局):
ALLOCA global_x, 1
ALLOCA global_arr, 10

// IR (局部，在函数开始):
ALLOCA local_y, 1
ALLOCA local_arr, 5
```

### 9. 内建函数

#### 9.1 Printf

`PRINTF` 指令是受限的格式化输出，仅支持 `%d` 格式符。

**限制**：

- 仅支持 `%d`（整数）
- 最多 3 个整数参数（受 MIPS 实现限制）
- 格式字符串可包含字面字符（如 `\n`）

**示例**：

```c
printf("x = %d\n", x);
```

```
// IR:
PARAM "x = %d\n"
PARAM x
CALL 2, printf, -
```

**实现**：后端生成内建的 `printf` 函数（基于 MARS syscall）。

#### 9.2 Getint

`getint()` 读取一个整数。

**示例**：

```c
int x = getint();
```

```
// IR:
CALL 0, getint, %t0
ASSIGN %t0, -, x
```

## 高级特性

### 1. 短路求值（Short-Circuit Evaluation）

逻辑运算符 `&&` 和 `||` 需要短路求值。

#### 1.1 逻辑与（&&）

```c
if (a && b) {
    // then
}
```

```
// IR:
IF a, L0           # 如果 a 为真，检查 b
GOTO L2            # a 为假，跳过 then
LABEL L0
IF b, L1           # b 为真，进入 then
GOTO L2            # b 为假，跳过 then
LABEL L1           # then 分支
// ...
LABEL L2           # 结束
```

#### 1.2 逻辑或（||）

```c
if (a || b) {
    // then
}
```

```
// IR:
IF a, L1           # a 为真，直接进入 then
IF b, L1           # a 为假，检查 b
GOTO L2            # 都为假，跳过 then
LABEL L1           # then 分支
// ...
LABEL L2           # 结束
```

#### 1.3 复杂条件

```c
if (a && (b || c)) {
    // then
}
```

```
// IR:
IF a, L0           # a 为真，继续
GOTO L3            # a 为假，跳过
LABEL L0
IF b, L2           # b 为真，进入 then
IF c, L2           # b 为假，检查 c
GOTO L3            # 都为假，跳过
LABEL L2           # then 分支
// ...
LABEL L3           # 结束
```

### 2. 数组初始化

常量数组初始化在全局指令中完成。

```c
int arr[3] = {1, 2, 3};
```

```
// IR (全局):
ALLOCA arr, 3
STORE 1, arr, 0
STORE 2, arr, 1
STORE 3, arr, 2
```

### 3. 多维数组

多维数组按行优先（row-major）顺序线性化。

```c
int matrix[3][4];  // 3 行 4 列
matrix[i][j] = 42;
```

```
// IR:
ALLOCA matrix, 12              # 3 * 4 = 12 个元素
// 访问 matrix[i][j]:
MUL i, 4, %t0                  # offset = i * 4
ADD %t0, j, %t1                # offset += j
STORE 42, matrix, %t1
```

## IR 生成

### 1. 从 AST 到 IR

`CodeGen` 访问者（Visitor）遍历 AST，生成 IR 指令。

**主要步骤**：

1. **表达式生成**：递归生成子表达式，返回结果操作数（临时变量或常量）
2. **语句生成**：顺序生成指令，处理控制流
3. **函数生成**：创建函数，生成序言/结尾
4. **全局初始化**：收集全局变量定义

**示例**（简化）：

```cpp
// 生成表达式 IR
Operand CodeGen::visitBinaryExp(BinaryExp *node) {
    Operand left = visit(node->left);
    Operand right = visit(node->right);
    Operand result = newTemp();
    
    switch (node->op) {
        case ADD:
            emit(Instruction::MakeBinary(OpCode::ADD, left, right, result));
            break;
        // ...
    }
    return result;
}

// 生成条件语句 IR
void CodeGen::visitIfStmt(IfStmt *node) {
    int labelThen = newLabel();
    int labelEnd = newLabel();
    
    Operand cond = visit(node->cond);
    emit(Instruction::MakeIf(cond, Operand::Label(labelThen)));
    
    // else 分支
    if (node->elseStmt) {
        visit(node->elseStmt);
    }
    emit(Instruction::MakeGoto(Operand::Label(labelEnd)));
    
    // then 分支
    emit(Instruction::MakeLabel(Operand::Label(labelThen)));
    visit(node->thenStmt);
    
    // 结束
    emit(Instruction::MakeLabel(Operand::Label(labelEnd)));
}
```

### 2. 基本块划分

基本块在指令生成后构建：

1. 识别基本块边界（标签、跳转、返回）
2. 划分指令序列
3. 建立控制流连接（`next`、`jumpTarget`）

### 3. 符号表交互

- **Variable 操作数**：引用符号表条目，携带类型信息
- **作用域**：全局变量 vs. 局部变量由符号表作用域决定
- **类型检查**：语义分析阶段完成，IR 生成假设类型正确

## IR 优化

### 1. 已实现的优化

#### 1.1 常量折叠（Constant Folding）

```
// 优化前:
ADD 1, 2, %t0
MUL %t0, 3, %t1

// 优化后:
ASSIGN 9, -, %t1
```

#### 1.2 无用代码消除（Dead Code Elimination）

```
// 优化前:
ADD x, y, %t0     # %t0 未被使用
SUB a, b, %t1

// 优化后:
SUB a, b, %t1
```

### 2. 未来可能的优化

- **公共子表达式消除**（CSE）
- **复制传播**（Copy Propagation）
- **循环不变代码外提**（Loop Invariant Code Motion）
- **强度削减**（Strength Reduction）：如 `x * 2` → `x + x` 或 `x << 1`

## IR 到 MIPS 的翻译

详见 `REGISTER_CONVENTION.md`，此处仅概述：

### 1. 操作数映射

| IR 操作数 | MIPS 表示 |
|-----------|-----------|
| `Temporary` (已分配) | `$t0-$t7` |
| `Temporary` (溢出) | 内存 `offset($fp)` |
| `ConstantInt` | 立即数或 `li` 加载 |
| `Variable` (局部) | `offset($fp)` |
| `Variable` (全局) | `.data` 段标签 |
| `Label` | MIPS 标签 `function_L0` |

### 2. 指令翻译

| IR 指令 | MIPS 指令 |
|---------|-----------|
| `ADD` | `addu` |
| `SUB` | `subu` |
| `MUL` | `mul` |
| `DIV` | `div` + `mflo` |
| `MOD` | `div` + `mfhi` |
| `LT` | `slt` |
| `EQ` | `subu` + `sltiu` |
| `LOAD` | `lw` (带地址计算) |
| `STORE` | `sw` (带地址计算) |
| `IF` | `bne ... $zero` |
| `GOTO` | `j` |
| `CALL` | `jal` (带参数设置) |
| `RETURN` | `jr $ra` 或 `syscall` (main) |

### 3. 函数翻译

- **序言（Prologue）**：
  1. 分配栈帧：`addiu $sp, $sp, -frameSize`
  2. 保存 `$ra`、`$fp`
  3. 设置 `$fp`：`move $fp, $sp`
  4. 保存参数寄存器到栈

- **结尾（Epilogue）**：
  1. 恢复 `$ra`、`$fp`
  2. 恢复栈指针：`addiu $sp, $sp, frameSize`
  3. 返回：`jr $ra`

## 设计决策与权衡

### 1. 为什么非 SSA？

**优点**：

- 生成简单：直接从 AST 翻译，无需 φ 函数
- 易于理解：接近源程序语义
- 足够优化：常量折叠、死代码消除等仍然可行

**缺点**：

- 某些优化困难：如 GVN（Global Value Numbering）需要 SSA

**结论**：对于教育项目，非 SSA 已足够。

### 2. 为什么四元式而非三元式？

**四元式**：`(op, arg1, arg2, result)`

- 显式结果，易于修改和优化

**三元式**：`(op, arg1, arg2)`，结果隐含在指令序号

- 更紧凑，但难以优化（重排序困难）

**结论**：四元式更灵活，适合优化。

### 3. 为什么基本块而非单纯线性？

- **CFG 分析**：活跃性分析、到达定义等需要 CFG
- **优化**：基于基本块的优化更高效
- **寄存器分配**：图着色算法需要基本块边界

### 4. 数组语义：地址 vs. 值

- **数组按地址**：传递指针，支持修改，符合 C 语义
- **标量按值**：传递拷贝，简单高效
- **实现**：符号表类型信息决定语义

## 调试与输出

### 1. IR 文本格式

```
# 全局变量
ALLOCA global_x, 1
ASSIGN 0, -, global_x

# 函数定义
Function: foo(int a, int b)
  BasicBlock 0:
    ADD a, b, %t0
    RETURN %t0

Function: main()
  BasicBlock 0:
    PARAM 1
    PARAM 2
    CALL 2, foo, %t0
    ASSIGN %t0, -, result
    RETURN 0
```

### 2. 可视化

可使用 Graphviz 生成 CFG：

- 节点：基本块（包含指令列表）
- 边：`next`（实线）、`jumpTarget`（虚线）

## 总结

LCC 的 IR 设计在简洁性和表达力之间取得了平衡：

- **足够抽象**：屏蔽硬件细节，便于优化
- **足够具体**：接近机器指令，易于翻译
- **灵活扩展**：为未来优化提供基础
- **易于调试**：可读的文本格式

这种设计使得从前端（AST）到后端（MIPS）的转换流畅且高效，同时为编译器的进一步发展留出了空间。

