# LCC编译器 - IR设计文档

## 设计总览

- **表示形式**：四元式三地址码 `(op, arg1, arg2, result)`。
- **组织结构**：`Module → Function → BasicBlock → Instruction`，并显式维护 CFG（fallthrough + 跳转）。
- **SSA 支持**：前端生成的 IR 默认非 SSA，可多次赋值；Mem2Reg 会插入 `PHI` 节点进入 SSA，PhiElimination 在销毁阶段移除 `PHI`，恢复非 SSA。常量折叠后，`result` 可直接写常量。
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
| `ADD/SUB/MUL/DIV/MOD` | `op arg1(var\|temp\|const), arg2(var\|temp\|const), res(temp\|const)` | `DIV/MOD` 由后端降为 `div` + `mflo/mfhi` |
| `NEG` | `NEG arg1(...), -, res(temp\|const)` | 一元取负 |
| `EQ/NEQ/LT/LE/GT/GE` | 同二元格式 | 比较结果归一化到 0/1 |
| `AND/OR` | `AND/OR arg1, arg2, res` | 逻辑值模式（非短路）。短路由控制流构造实现 |
| `NOT` | `NOT arg1, -, res` | `res = !arg1` |

### 数据移动与内存访问

| OpCode | 形式 | 说明 |
|--------|------|------|
| `ASSIGN` | `ASSIGN src(var\|temp\|const), -, dst(var\|temp)` | 通用赋值，后端会判定是否需要落栈 |
| `LOAD` | `LOAD base(var\|temp), index(var\|temp\|const\|empty), dst(var\|temp)` | 读取数组/指针元素,`index` 为空视为 `*base` |
| `STORE` | `STORE value(var\|temp\|const), base(var\|temp), index(var\|temp\|const\|empty)` | 写数组/指针,`index`为空视为指针(虽然测试样例并不会出现指针赋值的操作) |
| `ALLOCA` | `ALLOCA var(var), -, size(var\|temp\|const)` | 分配 `size` 个 word。全局进入 `.data`，局部在栈帧分配 |

> 这里地址语义的确定我们通过简单的的判断`index`是否为`Empty`来判断

### 控制流

| OpCode | 形式 | 说明 |
|--------|------|------|
| `LABEL` | `LABEL -, -, res(label)` | 基本块入口。`CodeGen` 负责插入 |
| `GOTO`  | `GOTO -, -, res(label)` | 无条件跳转 |
| `IF`    | `IF cond(var\|temp\|const), -, res(label)` | `cond != 0` 时跳转 |
| `RETURN`| `RETURN -, -, res(var\|temp\|const\|empty)` | 函数返回。空表示 `void` |

### 函数调用

| OpCode | 形式 | 说明 |
|--------|------|------|
| `PARAM` | `PARAM idx(const), -, res(var)`  | **函数定义阶段**使用：记录“第 idx 个形式参数绑定哪个符号”。必须出现在入口块的开头 |
| `ARG` | `ARG arg(var\|temp\|const), -, -` | **调用方**使用：将一个实参排入队列。后端在 `CALL` 时顺序消费 |
| `CALL` | `CALL argc(const), func(var), res(temp)` | 发起调用。`func` 是函数符号（`Variable`），`argc` = 之前发射的 `ARG` 数；`res` 可为空表示忽略返回值 |

> 前端保证：`CALL` 之前连续出现 `argc` 条 `ARG`，且中途不会夹杂其他 `CALL`。

### SSA 相关

| OpCode | 形式 | 说明 |
|--------|------|------|
| `PHI` | `PHI -, -, res(var\|temp)` | SSA 形态使用。实际参数存于 `Instruction::getPhiArgs()`，记录 `(value, predBB)` 列表；结果写入 `res` |
| `NOP` | `NOP -, -, -` | 占位或被消解后的指令；后端将会直接忽略 |
