# IR 设计与指令速查

本 IR 是三地址/四元式风格，非 SSA，按基本块组织为控制流图（CFG）。本页力求“一眼看懂”：模型、命名、每条指令做什么、如何组合出控制流与调用。

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

说明约定：语义采用伪代码，rd 表示结果位置；无 rd 的为“仅产生控制/副作用”,'-'表示缺省参数。

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

3) 逻辑

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

- `LABEL L`         -> 定义跳转目标 L（通常等价于基本块入口）
- `GOTO L`          -> 无条件跳转到 L
- `IF cond, L`      -> 若 cond != 0，跳转到 L
  - 复杂条件与短路：由 CodeGen 组合 `IF/GOTO/LABEL` 序列生成

7) 声明/分配

- `ALLOCA sym, size` -> 定义符号 `sym` 并分配空间。`size`为分配单元数
  - 是否全局/局部由符号表作用域决定；后端据此发射到 `.data` 或栈帧

8) 调用相关

- `PARAM arg`            -> 追加一个按位次的实参
- `CALL argc, func, ret` -> 调用 `func`，`argc` 为紧邻之前收集的参数个数；可选 `ret`
- `RETURN [val]`          -> 带或不带返回值返回

9) 输出（内建函数）

- `PRINTF fmt_or_val, ...` -> 受限的打印能力（实现上走后端内建），常配合 `PARAM/CALL` 使用

## 组合规则与约定

- 非 SSA：允许对同一 Variable 多次 `ASSIGN`；Temporary 通常只写一次
- 短路构造：`x && y` 通常展开为
  1. 若 x==0 直接跳过
  2. 再判断 y
  3. 将布尔结果聚合到目标 rd（用 `ASSIGN` 写 0/1）
- 数组按地址传参：`PARAM arr` 传入的是数组首址；标量 `PARAM x` 传入的是值
- 全局/局部：`DEF` 的作用域由符号表决定。全局在 `.data`，局部在栈帧
