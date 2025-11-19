# 中间代码设计

本 IR 为三地址/四元式风格，按函数划分基本块（CFG），追求“易生成、易降级”，目前不做。

## 核心设计思想

IR 的核心是**指令序列**，这些指令被组织在**基本块 (Basic Block)** 中。多个基本块构成一个**函数 (Function)** 的控制流图。

## 主要构成

### 1. `Operand` (操作数)

操作数是指令操作的对象。它是所有计算的来源和目标。

-   **类型**:
    -   `Variable`: 源代码中的变量或函数名，直接引用符号表中的 `Symbol`。
    -   `Temporary`: 编译器生成的临时变量，用于存储表达式的中间结果。
    -   `ConstantInt`: 整型字面量。
    -   `Label`: 用于标记跳转目标的地址。

### 2. `Instruction` (指令)

四元式顺序：`(op, arg1, arg2, result)`。打印为 `OP arg1, arg2, result`（缺省项省略）。

-   **主要指令类别 (`OpCode`)**:
  -   **算术/一元**: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`
  -   **比较 (结果为 0/1)**: `EQ`, `NEQ`, `LT`, `LE`, `GT`, `GE`
  -   **逻辑**: `AND`, `OR`, `NOT`
  -   **赋值**: `ASSIGN`  (语义：`ASSIGN src, dst` → dst = src)
  -   **内存访问**: `LOAD` (t = base[index])，`STORE` (base[index] = value)
  -   **控制流**: `IF` (cond≠0 跳转：`IF cond, label`)，`GOTO`，`LABEL`
  -   **调用相关**: `PARAM`（按出现顺序收集实参），`CALL`（`CALL argCount, func, ret`），`RETURN`
  -   **内建/输出**: `PRINTF`
  -   **定义**: `DEF`（变量或数组声明，`DEF symbol, size`，size=1 代表标量）


函数的边界由 `Function` 容器表示，而不是靠指令。

工厂方法用于创建具有清晰语义的指令：
`MakeBinary` / `MakeUnary` / `MakeAssign` / `MakeLoad` / `MakeStore` / `MakeIf` / `MakeGoto` / `MakeLabel` / `MakeCall` / `MakeReturn` / `MakeDef`。

### 3. `BasicBlock` (基本块)

基本块是 IR 的结构单元，它包含一个线性的指令序列。

-   **属性**:
    -   入口：只有第一条指令是入口。
    -   出口：只有最后一条指令是出口。
    -   内部无任何跳转指令，跳转指令只可能在末尾。
-   **控制流**: 每个基本块通过 `next` (顺序执行的下一个块) 和 `jumpTarget` (条件或无条件跳转的目标块) 指针连接成图。

### 4. `Function` (函数)

函数是基本块的容器，代表了源程序中的一个完整函数。

-   **结构**:
    -   包含一个入口基本块和多个其他基本块。
    -   所有基本块共同构成了该函数的控制流图 (CFG)。

## 语义约定与示例

### 语义约定（关键点）
- 非 SSA：同名 `Variable` 可多次赋值；临时 `Temporary` 由 CodeGen 生成（单赋值）。
- 比较/逻辑结果：均为 0 或 1。
- 逻辑短路：`Cond` 生成 `IF/GOTO/LABEL` 序列（见 CodeGen::genCond）。
- 数组/标量：
  - `LOAD base, idx, dst`: 从 `base[idx]` 读到 `dst`。
  - `STORE val, base, idx`: 将 `val` 写到 `base[idx]`。
  - 是否“数组基址”由符号的类型判定（Array 按地址传递/使用；标量按值）。
- 调用约定：先发 `PARAM` 按出现顺序收集参数，随后 `CALL argCount, func, ret`，返回值放入 `ret`（可为空）。`RETURN` 可无返回值。
- 全局与局部：`DEF sym, size` 支持标量（size=1）与数组（size>1）。后端据此在 `.data` 或栈帧分配。

