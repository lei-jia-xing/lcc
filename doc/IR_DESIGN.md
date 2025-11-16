# 中间代码 (Intermediate Representation) 设计

本文档描述了 lcc 编译器的内部中间表示（IR）。该 IR 是一种基于控制流图（CFG）的四元式风格代码，旨在清晰地表示源程序的语义，并为后续的优化和目标代码生成提供便利。

## 核心设计思想

IR 的核心是**指令序列**，这些指令被组织在**基本块 (Basic Block)** 中。多个基本块构成一个**函数 (Function)** 的控制流图。

## 主要构成模块

我们的 IR 由以下四个核心组件构成：

### 1. `Operand` (操作数)

操作数是指令操作的对象。它是所有计算的来源和目标。

-   **类型**:
    -   `Variable`: 源代码中的变量或函数名，直接引用符号表中的 `Symbol`。
    -   `Temporary`: 编译器生成的临时变量，用于存储表达式的中间结果。
    -   `ConstantInt`: 整型字面量。
    -   `Label`: 用于标记跳转目标的地址。

### 2. `Instruction` (指令)

指令是执行的基本单位，采用标准四元式逻辑顺序：`(op, arg1, arg2, result)`。

打印格式：`OP arg1, arg2, result`，若某些操作数不存在则省略；不再采用旧的 `OP result, arg1, arg2` 顺序。

-   **主要指令类别 (`OpCode`)**:
  -   **算术/一元**: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`
  -   **比较 (结果约定为 0 或 1)**: `EQ`, `NEQ`, `LT`, `LE`, `GT`, `GE`
  -   **逻辑**: `AND`, `OR`, `NOT`
  -   **赋值**: `ASSIGN`  (语义：`ASSIGN src, dst` → dst = src)
  -   **内存访问**: `LOAD` (t = base[index])，`STORE` (base[index] = value)
  -   **控制流**: `IF` (条件为非零跳转：`IF cond, label`)，`GOTO`，`LABEL`
  -   **调用相关**: `PARAM`（按出现顺序收集实参），`CALL`（`CALL argCount, func, ret`），`RETURN`
  -   **内建/输出**: `PRINTF`
  -   **定义**: `DEF`（变量或数组声明，`DEF symbol, size`，size=1 代表标量）

不再使用：`IF_FALSE_GOTO`、`LOAD_ARRAY`、`STORE_ARRAY`、`FUNC_BEGIN`、`FUNC_END`、`STATIC_DEF`。

函数的边界由更高层的 `Function` 容器表示，而不是靠指令。

工厂方法（在实现中提供）用于创建具有清晰语义的指令：
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

## 示例

源C代码:
```c
int main() {
  int a = 10;
  int b;
  if (a > 5) {
    b = a * 2;
  } else {
    b = 0;
  }
  return b;
}
```

可能生成的 IR 伪代码（新指令格式）:
```
DEF a, 1
DEF b, 1
ASSIGN 10, a          ; a = 10
GT a, 5, t1           ; t1 = a > 5  (t1 取值 0/1)
IF t1, L_true         ; if (t1!=0) goto L_true
GOTO L_false

LABEL L_true
MUL a, 2, t2
ASSIGN t2, b
GOTO L_end

LABEL L_false
ASSIGN 0, b

LABEL L_end
RETURN b
```

说明：
- 条件跳转统一使用正极性 `IF cond, label`，不再使用 `IF_FALSE_GOTO`。
- `LABEL` 仅用于跳转目标；函数入口由 `Function` 的第一个基本块隐式表示。
- `DEF` 用于声明局部（或全局）变量/数组，可在后续阶段映射到栈空间或静态区。
- 若需要短路逻辑，例如 `a && b`，可通过构造多个 `IF` + `GOTO` + `LABEL` 实现，不强制使用 `AND`。
