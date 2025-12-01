# 优化 TODO 总览

当前状态：

- CodeGen 中已经做了表达式级的常量折叠（`tryEvalConst` 系列）。
- IR 层保留了 `LocalDCEPass`，通过 `OptimizationOptions` 开关控制，在 `main` 里默认开启。
- 后端有图着色寄存器分配，但尚未有专门的汇编 peephole pass。

目标：

- 先在 IR 上完善局部/全局 DCE 与简单传播，再在汇编层加 peephole 和栈帧精简，最后考虑循环相关优化。

---

## 一、IR 层优化 TODO（以 `QuadOptimizer` / `Function` 为中心）

### 1. 增强 LocalDCE（块内）

#### 1.1 块内 copy-propagation（简单版）

**目标**：

- 减少“临时只被转手一次”的无用指令。

**范围**：

- 单个基本块内，不跨块。

**思路**（在 `LocalDCEPass::run(Function &fn)` 中，对每个基本块）：

1. 保留现有的 `useCount` 统计逻辑。
2. 在反向 DCE 之前新增一轮“正向扫描”用于 copy-prop：
   - 维护 `copyMap: tempDst -> Operand src`；
   - 对每条指令：
     - 先用 `copyMap` 替换 `arg1/arg2` 中的 temp：
       - 如果 `arg1/arg2` 是 `Temporary`，且存在于 `copyMap`，则改为其映射源（可以是 `Temporary` 或 `ConstantInt`）。
     - 检查当前指令是否是简单拷贝：
       - 条件：`op == ASSIGN` 且
         - `arg1` 是 `Temporary` 或 `ConstantInt`；
         - `result` 是 `Temporary`；
       - 若满足：设置 `copyMap[resultTemp] = arg1`。
     - 若当前指令把某个 temp 重新定义为非简单拷贝：
       - 对该 `resultTemp` 执行 `copyMap.erase(resultTemp)`，避免旧映射污染后续使用。
3. 正向替换完成后继续执行现有的“反向遍历 + DCE”逻辑：
   - 仅删除“无副作用 && result 是 temp && useCount[temp] == 0”的指令。

#### 1.2 块内常量传播（基于 CodeGen）

**目标**：

- 利用 CodeGen 产生的常量，把简单的 `ASSIGN const, t` 向后传播，给 DCE 更多删指令的机会。

**思路**：

- 在同一轮正向扫描中维护 `constMap: temp -> int`：
  - 遇到 `ASSIGN ConstantInt(k) -> temp`：
    - 记录 `constMap[temp] = k`；
  - 后续若在 `arg1/arg2` 中使用 `temp`，先查 `constMap`，若存在则替换为 `ConstantInt(k)`；
  - 一旦某条指令将 `temp` 赋值为非常量（或通过算术产生），就 `constMap.erase(temp)`；
- 不在 IR 层新增算术常量折叠（避免与 CodeGen 重复），仅做“常量传播”。

---

### 2. CFG 级 DCE（全局死代码删除）

#### 2.1 全函数数据流 DCE

**目标**：

- 跨基本块删掉不被任何路径使用的纯计算，进一步减少 IR 体积和寄存器压力。

**副作用指令定义**：

- 下列 IR 指令被视为“有副作用”（不能删除）：
  - `STORE`
  - `CALL`
  - `RETURN`
  - `IF`
  - `GOTO`
  - `LABEL`
  - `PARAM`
  - `ALLOCA`

**算法草图**：

1. 初始化 `liveDefs` 集合为空。
2. 遍历所有基本块与指令：
   - 对每条“有副作用”的指令，将其依赖的 temp 操作数对应的**定义指令**加入 `liveDefs` 初始工作队列。
3. 迭代数据流：
   - 当某条定义指令被标记为 live 时，将其依赖的 temp 定义也加入 `liveDefs`，直到没有新增。
4. 删除所有：
   - 不在 `liveDefs` 中，且
   - 无副作用的算术/赋值类指令（`ASSIGN/ADD/SUB/MUL/...`）。

---

### 3. 循环相关优化（后期考虑）

#### 3.1 Loop-Invariant Code Motion（LICM-lite）

**目标**：

- 将循环体内部与循环变量无关、无副作用的计算，移动到循环外，减少重复计算。

**约束**：

- 初期仅支持“单基本块 for 形态的循环”：
  - 一个入口 `LABEL`；
  - 一个判断条件 `IF` 跳出循环；
  - 一个回跳 `GOTO` 到入口。

**思路**：

- 分析循环体中每条指令：
  - 如果其所有操作数均在循环之前定义，且无副作用，则可以移动到循环前。

#### 3.2 Strength Reduction（强度削弱）

**目标**：

- 降低循环内部代价高的运算（例如乘法）。

**典型模式**：

- 循环变量更新：`i = i + 1`；
- 循环体中有 `t = i * C`（`C` 为常量）。

**转换**：

- 在循环入口前增加：`t_init = 0`；
- 在循环体内：
  - 用 `t` 代替 `i * C`；
  - 在每一轮末尾增加 `t = t + C`。

---

## 二、后端（MIPS Asm）优化 TODO

### 4. Peephole 框架

#### 4.1 AsmPass / AsmPassManager

**目标**：

- 在 `AsmGen` 生成指令序列后，对汇编进行小窗口优化。

**设计**：

- 定义：
  - `class AsmPass { virtual bool run(std::vector<AsmInstr> &) = 0; };`
  - `class AsmPassManager`，结构仿照 IR 侧的 `PassManager`；
- 在 AsmGen 中：
  - 完成所有函数和全局指令生成后，调用 `runAsmOptimizations(functionAsm)`。

### 5. 初始 Peephole 规则（保守安全）

#### 5.1 删除冗余 move / li

**可删模式示例**：

- `move $t0, $t0` → 直接删除；
- `li $t0, imm` 紧接着被另一条写 `$t0` 的指令覆盖，且中间无读 `$t0` → 删除前一条 `li`。

#### 5.2 清理多余栈调整

**模式**：

- `addiu $sp, $sp, -X`
- 紧跟
- `addiu $sp, $sp, +X`
- 且中间没有以 `$sp` 为基址的 `sw/lw` 等访问 → 删除这一对 `addiu`。

### 6. 栈帧与寄存器保存精简

#### 6.1 按实际使用的 callee-saved 保存/恢复

**目标**：

- 只保存/恢复真正被本函数使用的 `$s0-$s7`，而非模板化全保存。

**思路**：

- 由 `RegisterAllocator` 提供每个函数使用的 `$s` 寄存器集合；
- 在 AsmGen 生成 prologue/epilogue 时：
  - 仅对这些寄存器生成 `sw` / `lw` 指令；
  - 相应调整栈帧大小计算。

---

## 三、优化开关与配置

### 7. IR 优化开关（`OptimizationOptions`）

当前定义：

```cpp
struct OptimizationOptions {
  bool enableLocalDCE = true;

  static OptimizationOptions fromLevel(int level);
};
```

后续可扩展字段（功能就绪后再启用）：

- `bool enableCopyProp = false;`
- `bool enableConstProp = false;`
- `bool enableGlobalDCE = false;`
- `bool enableLICM = false;`
- `bool enableStrengthReduction = false;`

建议等级约定：

- `level 0`：全部关闭；
- `level 1`：启用安全、局部的优化：`LocalDCE + CopyProp + ConstProp`；
- `level 2`：在 `level 1` 基础上开启：`GlobalDCE / LICM / StrengthReduction`。

### 8. Asm 优化开关（预留）

可以类似 IR 定义：

```cpp
struct AsmOptimizationOptions {
  bool enablePeephole = false;
  bool enableStackFrameTuning = false;
  // ... 其他汇编级优化
};
```

由 `main` 或配置文件映射优化等级到 `AsmOptimizationOptions` 与 `OptimizationOptions`。
