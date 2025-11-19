# IR 设计与指令速查

本 IR 是三地址/四元式风格，非 SSA，按基本块组织为控制流图（CFG）。本页力求“一眼看懂”：模型、命名、每条指令做什么、如何组合出控制流与调用。

— 一屏速览 —
- 模型：函数 Function → 基本块 BasicBlock → 线性指令序列（跳转只出现在块末尾）
- 操作数 Operand：Variable（变量/函数名）、Temporary（临时）、ConstantInt（常量）、Label（标签）
- 记法：四元式 `(op, a1, a2, rd)`，打印为 `OP a1, a2, rd`（缺省省略）
- SSA：否。Variable 可多次赋值；Temporary 由编译器分配，通常一次赋值
- 值域：整型有符号 32 位；比较/逻辑结果恒为 0/1
- 数组语义：数组按“地址”参与运算与传参；标量按“值”参与
- 调用协议（IR 层）：先连续发 `PARAM`，再 `CALL argc, func, ret`，返回用 `ret` 带回（可为空）

更多降级细节见 `doc/IR_to_MIPS.md`；总览参见 `doc/overview.md`。

## 操作数（Operand）

- Variable：符号表条目（可为全局/局部变量、函数名）
- Temporary：编译期生成的临时名（如 `%t3`），承载中间结果
- ConstantInt：整数常量（如 `0`、`42`）
- Label：跳转目标（基本块入口标签）

## 指令参考（按类别）

说明约定：语义采用伪代码，rd 表示结果位置；无 rd 的为“仅产生控制/副作用”。

1) 算术/一元（整型算术）
- `ADD a, b, rd` → rd = a + b
- `SUB a, b, rd` → rd = a - b
- `MUL a, b, rd` → rd = a * b
- `DIV a, b, rd` → rd = a / b  （向零截断，行为与后端一致）
- `MOD a, b, rd` → rd = a % b  （余数，遵循后端 MIPS div/mfhi 语义）
- `NEG a, -, rd` → rd = -a

2) 比较（结果恒 0/1）
- `EQ a, b, rd`  → rd = (a == b)
- `NEQ a, b, rd` → rd = (a != b)
- `LT a, b, rd`  → rd = (a < b)
- `LE a, b, rd`  → rd = (a <= b)
- `GT a, b, rd`  → rd = (a > b)
- `GE a, b, rd`  → rd = (a >= b)

3) 逻辑（短路由控制流构造，不在此硬编码）
- `AND a, b, rd` → rd = (a && b)   （常由 `IF/GOTO` + 常量折叠构造）
- `OR a, b, rd`  → rd = (a || b)
- `NOT a, -, rd` → rd = (!a)

4) 赋值/数据移动
- `ASSIGN src, -, dst` → dst = src

5) 内存（数组按地址语义）
- `LOAD base, idx, dst`  → dst = base[idx]
- `STORE val, base, idx` → base[idx] = val
  - base 的数组/标量属性来自符号类型：Array 代表可下标的线性内存；标量不允许下标
  - idx 为元素索引（单位“元素”），后端按元素宽度换算字节偏移

6) 控制流
- `LABEL L`         → 定义跳转目标 L（通常等价于基本块入口）
- `GOTO L`          → 无条件跳转到 L
- `IF cond, L`      → 若 cond != 0，跳转到 L
  - 复杂条件与短路：由 CodeGen 组合 `IF/GOTO/LABEL` 序列生成

7) 声明/分配
- `DEF sym, size` → 定义符号 `sym` 并分配空间。size=1 为标量，>1 为数组长度
  - 是否全局/局部由符号表作用域决定；后端据此发射到 `.data` 或栈帧

8) 调用相关
- `PARAM arg`            → 追加一个按位次的实参
- `CALL argc, func, ret` → 调用 `func`，`argc` 为紧邻之前收集的参数个数；可选 `ret`
- `RETURN val?`          → 带或不带返回值返回

9) 输出（内建）
- `PRINTF fmt_or_val, ...` → 受限的打印能力（实现上走后端内建），常配合 `PARAM/CALL` 使用

## 组合规则与约定

- 非 SSA：允许对同一 Variable 多次 `ASSIGN`；Temporary 通常只写一次
- 短路构造：`x && y` 通常展开为
  1. 若 x==0 直接跳过
  2. 再判断 y
  3. 将布尔结果聚合到目标 rd（用 `ASSIGN` 写 0/1）
- 数组按地址传参：`PARAM arr` 传入的是数组首址；标量 `PARAM x` 传入的是值
- 全局/局部：`DEF` 的作用域由符号表决定。全局在 `.data`，局部在栈帧

## 最小示例

1) `a[i] = b + 1`：

```
  t1 = ADD b, 1
  STORE t1, a, i
```

2) `if (x && y) z = 1; else z = 0;`（示意）

```
  IF x, L_then
  GOTO L_else
L_then:
  IF y, L_set1
  GOTO L_set0
L_set1:
  ASSIGN 1, -, z
  GOTO L_end
L_set0:
  ASSIGN 0, -, z
L_end:
```

3) `t0 = f(arr, i); return t0;`

```
  PARAM arr
  PARAM i
  CALL 2, f, t0
  RETURN t0
```

— 参考 —
- MIPS 降级细节与示例：`doc/IR_to_MIPS.md`
- 汇总导航：`doc/overview.md`

# 中间代码设计

本 IR 为三地址/四元式风格，按函数划分基本块（CFG）。

## 核心设计思想

IR 的核心是**指令序列**，这些指令被组织在**基本块 (Basic Block)** 中。多个基本块构成一个**函数 (Function)** 的控制流图。

## 主要构成

### 1. `Operand` (操作数)

操作数是指令操作的对象。它是所有计算的来源和目标。

- **类型**:
  - `Variable`: 源代码中的变量或函数名，直接引用符号表中的 `Symbol`。
  - `Temporary`: 编译器生成的临时变量，用于存储表达式的中间结果。
  - `ConstantInt`: 整型字面量。
  - `Label`: 用于标记跳转目标的地址。

### 2. `Instruction` (指令)

四元式顺序：`(op, arg1, arg2, result)`。打印为 `OP arg1, arg2, result`（缺省项省略）。

- **主要指令类别 (`OpCode`)**:
- **算术/一元**: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`
- **比较 (结果为 0/1)**: `EQ`, `NEQ`, `LT`, `LE`, `GT`, `GE`
- **逻辑**: `AND`, `OR`, `NOT`
- **赋值**: `ASSIGN`  (语义：`ASSIGN src, dst` → dst = src)
- **内存访问**: `LOAD` (t = base[index])，`STORE` (base[index] = value)
- **控制流**: `IF` (cond!=0 跳转：`IF cond, label`)，`GOTO`，`LABEL`
- **调用相关**: `PARAM`（按出现顺序收集实参），`CALL`（`CALL argCount, func, ret`），`RETURN`
- **内建/输出**: `PRINTF`
- **定义**: `DEF`（变量或数组分配空间，`DEF symbol, size`，size=1 代表标量）

函数的边界由 `Function` 容器表示，而不是靠指令。

工厂方法用于创建具有清晰语义的指令：
`MakeBinary` / `MakeUnary` / `MakeAssign` / `MakeLoad` / `MakeStore` / `MakeIf` / `MakeGoto` / `MakeLabel` / `MakeCall` / `MakeReturn` / `MakeDef`。

### 3. `BasicBlock` (基本块)

基本块是 IR 的结构单元，它包含一个线性的指令序列。

- **属性**:
  - 入口：只有第一条指令是入口。
  - 出口：只有最后一条指令是出口。
  - 内部无任何跳转指令，跳转指令只可能在末尾。
- **控制流**: 每个基本块通过 `next` (顺序执行的下一个块) 和 `jumpTarget` (条件或无条件跳转的目标块) 指针连接成图。

### 4. `Function` (函数)

函数是基本块的容器，代表了源程序中的一个完整函数。

- **结构**:
  - 包含一个入口基本块和多个其他基本块。
  - 所有基本块共同构成了该函数的控制流图 (CFG)。

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
