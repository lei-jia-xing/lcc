# LCC编译器 - backend 设计文档

> 本文档描述 `src/backend/AsmGen.cpp` 与 `src/backend/RegisterAllocator.cpp` 的实现细节，重点解释寄存器分配、栈帧布局以及 IR → MIPS 的翻译流程。

## 模块概览

后端主要由两个模块组成：

- `RegisterAllocator`：对每个函数进行活跃性分析，构建冲突图，并将 IR 临时 (`Temporary`) 着色到有限数量的物理寄存器上（`$s0-$s7`）。
- `AsmGen`：遍历 IR 模块，将 IR 指令翻译为 MIPS 汇编；负责数据段/文本段输出、栈帧布局、调用约定及溢出（spill）变量的访存。

前端生成的 IR 通过 `IRModuleView` 暴露给后端：

```cpp
struct IRModuleView {
 std::vector<const Function *> functions;            // 所有函数
 std::vector<const Instruction *> globals;           // 全局 ALLOCA/初始化指令
 std::unordered_map<std::string, std::shared_ptr<Symbol>> stringLiterals;
};
```

`AsmGen::generate` 是后端入口：

```cpp
void AsmGen::generate(const IRModuleView &mod, std::ostream &out);
```

它会先输出 `.data` 段（字符串常量与全局变量），再输出 `.text` 段（所有函数及运行时辅助例程）。

## 2. 数据段生成（emitDataSection）

### 字符串常量

`stringLiterals` 存储从前端收集到的所有字符串常量，key 为**源代码字面量**（如 `"%d\n"`），value 为指向全局 `Symbol` 的智能指针。`emitDataSection` 对每个条目输出：

```mips
label: .asciiz "原始字符串内容"
```

当前实现直接逐字符写入，不额外处理转义（即假定前端传入的字面量已经是希望写入 `.asciiz` 的内容）。

### 全局变量

`globals` 中的指令序列编码了全局变量的分配与常量初始化，主要使用：

- `ALLOCA var, -, size`：声明一个大小为 `size`（以 word 计）的全局数组或变量。
- `ASSIGN const, -, var`：为标量全局变量设置初值。
- `STORE const, base, index`：为全局数组的第 `index` 个元素设置初值。

`emitDataSection` 先扫描所有 `ALLOCA` 指令，记录每个全局符号的大小，并初始化一个全为 0 的数组；随后处理 `ASSIGN/STORE`，将常量写入对应位置，最后输出 `.word` 指令：

```mips
global_or_mangled_name: .word v0, v1, v2, ...
```

这里使用 `Symbol::globalName` 作为 label，若为空则退回原始 `name`，保证不同作用域下的同名变量仍映射到唯一的全局符号。

## 文本段生成（emitTextSection）

文本段生成包含三部分：

1. 为每个函数输出 `.globl name` 与标签 `name:`。
2. 优先输出 `main` 函数，其后输出其他函数。
3. 在末尾内联简化版 `printf` 与 `getint` 运行时例程。

`emitTextSection` 内部对每个 `Function` 调用 `emitFunction` 完成具体翻译。

## 寄存器分配（RegisterAllocator）

后端采用经典的图着色寄存器分配算法，将 IR 临时变量映射到 8 个**可分配寄存器**：

```cpp
// RegisterAllocator 内部约定：
static const int NumRegs = 8; // 对应 AsmGen 中的 $s0-$s7
```

`AsmGen` 在构造时会初始化一张寄存器描述表：

```cpp
static const char *Regs[NUM_ALLOCATABLE_REGS] =
  {"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7"};
```

每个函数在生成汇编前，都会调用：

```cpp
_regAllocator.run(const_cast<Function *>(func));
```

### Use/Def 与活跃性分析

寄存器分配的第一步是对每个基本块计算：

- `use[B]`：在块内**第一次被使用**、但在该块中尚未定义的临时集合；
- `def[B]`：在块内被定义（作为 `result`）的临时集合；
- `liveIn[B]` / `liveOut[B]`：经典数据流方程：

$$
liveOut[B] = \bigcup_{S \in succ(B)} liveIn[S] \\
liveIn[B] = use[B] \cup (liveOut[B] - def[B])
$$

实现上，`computeUseDef` 逐块扫描指令；`computeLiveInOut` 反复迭代直至收敛。`STORE` 指令的 `result`（数组索引）被视为“使用”而非“定义”。

### 冲突图构建

`buildInterferenceGraph` 从每个基本块的末尾向前遍历，维护当前 `live` 集合：

- 若指令定义了某个临时 `t`，则为 `t` 与当前 `live` 中的所有其它临时添加冲突边；然后将 `t` 从 `live` 中移除。
- 对于指令使用的每个临时，将其加入 `live`。

最终得到一个无向图：

- 节点：所有出现过的临时 ID；
- 边：生命周期重叠的临时对。

### 图着色与溢出

`doColoring` 采用简化的栈式图着色：

1. 重复选择**度数 < NumRegs** 的节点入栈，并从图中删除相关边；
2. 若不存在此类节点，则选择任意节点标记为**潜在溢出**，入栈后从图中删除；
3. 图清空后，逆序弹栈，对每个节点选择一个未被邻居占用的颜色（0~7），若无可用颜色则将其加入 `_spilledNodes`。

`RegisterAllocator` 对外提供：

- `int getReg(int tempId) const;`：返回某临时对应的寄存器编号（0~7），若未分配寄存器返回 -1；
- `bool isSpilled(int tempId) const;`：查询某临时是否被溢出到内存；
- `const LiveSet &getSpilledNodes() const;`：所有溢出的临时集合；
- `std::set<int> getUsedRegs() const;`：本函数实际用到的寄存器编号集合，用于选择保存/恢复的 `$sX` 列表。

## AsmGen 中的寄存器与栈帧策略

### 寄存器分类

AsmGen 逻辑中，MIPS 寄存器按角色划分：

| 类别 | 实际寄存器 | 用途 |
|------|------------|------|
| **可分配寄存器** | `$s0-$s7` | 由 `RegisterAllocator` 分配给 IR 临时；在函数入口保存、函数退出恢复 |
| **Scratch 寄存器** | `$t0-$t9` | 指令级临时：加载常量、访存地址计算、溢出变量读写等；不参与图着色 |
| **参数寄存器** | `$a0-$a3` | 函数调用时前 4 个参数 |
| **返回值寄存器** | `$v0` | 函数返回值 |
| **栈/帧/返回地址** | `$sp/$fp/$ra` | 维护栈帧和控制流 |

> 当前实现中，`AsmGen::allocateScratch` 会在 `$t0-$t9` 中线性扫描一个 `inUse == false` 的寄存器并标记为使用，若耗尽则返回 `$zero` 作为失败占位；每条 IR 指令开始时通过 `resetScratchState()` 清空 `inUse` 标记，保证 scratch 寄存器不跨指令泄漏。

### 栈帧布局

`emitFunction` 为每个函数构建如下栈帧（从低地址到高地址）：

```text
低地址
 0($sp): 保存的 $ra
 4($sp): 保存的 $fp
 8($sp)...: 局部变量、局部数组（由 ALLOCA 决定大小）
 ...: 为溢出临时分配的空间（每个 4 字节）
 ...: 保存的 $s0-$s7（仅保存实际使用到的寄存器）
高地址（= 调用前的 $sp）
```

`analyzeFunctionLocals` 在函数内第一次扫描中完成：

- 起始偏移 `nextOffset = 8`，为每个形式参数（通过入口块的 `PARAM` 指令识别）与局部变量（`ALLOCA`）分配连续的 word 大小区域；
- 为数组变量分配 `size` 个 word；
- 记录到 `locals_ : Symbol* -> {offset, size}`。

完成 `RegisterAllocator::run` 后，AsmGen 再：

- 为 `_spilledNodes` 中的每个临时从当前 `frameSize_` 往上分配 4 字节空间，并记录到 `_spillOffsets[tempId]`；
- 更新 `frameSize_`；
- 根据 `getUsedRegs()` 计算需要保存的 `$sX` 个数，并为其在栈帧顶部预留空间；
- 最终 `frameSize_` 即整个栈帧大小（字节）。

### 序言与尾声

**序言（prologue）：**

```mips
func_name:
 addiu $sp, $sp, -frameSize   # 若 frameSize 超出 16bit，则使用 li+addu
 sw   $ra, 0($sp)
 sw   $fp, 4($sp)
 sw   $sX, offset($sp)        # 对每个使用的 $sX 保存
 move $fp, $sp

 # 将 $a0-$a3 中的前 4 个参数复制到对应局部槽位
 sw   $a0, off0($fp)
 sw   $a1, off1($fp)
 ...
 # 超过 4 个的参数从调用者栈帧中读出，写入自己的局部槽
 lw   $t?, caller_off($fp)
 sw   $t?, local_off($fp)
```

**尾声（epilogue）：**

```mips
func_name_END:
 lw   $sX, offset($sp)
 lw   $ra, 0($fp)
 lw   $fp, 4($fp)
 addiu $sp, $sp, frameSize
 jr   $ra                  # main 特殊：用 syscall 17 退出
```

所有 `RETURN` 指令都会先将返回值写入 `$v0`，然后跳转到统一的 `func_name_END` 标签，避免重复展开恢复逻辑。

## IR 指令到 MIPS 的映射（lowerInstruction）

`lowerInstruction` 是 IR → MIPS 的核心函数。它在每条指令开始时调用 `resetScratchState()`，然后根据 `OpCode` 生成具体指令。

### 寄存器获取与结果存储

两个重要的内部工具函数：

- `std::string AsmGen::getRegister(const Operand &op, std::ostream &out);`
 	- `Temporary`：若未溢出，直接返回其分配的 `$sX`；若溢出，则使用 scratch 寄存器从 `_spillOffsets` 指示的栈位置 `lw` 进来；
 	- `ConstantInt`：若为 0，直接返回 `$zero`；否则分配一个 scratch，输出 `li scratch, imm` 并返回；
 	- `Variable`：根据符号是否为局部/全局、是否数组，决定使用 `lw` 读值或 `la` 取地址；
 	- 其他类型（Label/Empty）：统一返回 `$zero`（不会被算术路径使用）。

- `void AsmGen::storeResult(const Operand &op, const std::string &reg, std::ostream &out);`
 	- 若目标为溢出临时：按照 `_spillOffsets` 中的偏移写回栈帧；
 	- 若为局部变量：根据 `locals_` 查找偏移，`sw reg, offset($fp)`；
 	- 若为全局变量：通过 `la scratch, globalName` 获取地址，再 `sw reg, 0(scratch)`。

`lowerInstruction` 在多数算术/逻辑指令中遵循以下模板：

1. 使用 `getRegister` 为 `arg1/arg2` 获取寄存器（可能是 `$sX` 或 `$t?`）。
2. 通过 `regForTemp` 或 `allocateScratch` 决定结果寄存器：
  - 若 `result` 是未溢出的临时，则使用其专属 `$sX`；
  - 否则使用一个 scratch。
3. 输出对应的 MIPS 指令（`addu/subu/mul/div/mflo/...`）。
4. 若 `result` 是临时且被标记为溢出，在寄存器计算完成后调用 `storeToSpill(tempId, rd)` 落栈。

### 典型指令映射

- **算术**：

 ```mips
 # ADD a, b, res
 addu rd, ra, rb
 ```

- **比较**（如 `EQ`）：

 ```mips
 subu  $t?, ra, rb
 sltiu rd, $t?, 1   # rd = (ra == rb)
 ```

- **逻辑 AND/OR**：

 使用 `sltu` 将任意值归一化为 0/1，再 `and/or` 组合。

- **LOAD/STORE**：

 	- 局部变量：经 `locals_` 查找偏移，使用 `lw/sw offset($fp)`；
 	- 全局变量：

  ```mips
  la  $t?, globalName
  lw  rd, 0($t?)    # 读
  sw  rv, 0($t?)    # 写
  ```

 	- 数组访问：对动态索引用 `sll indexReg, 2` 计算字节偏移，再与基址相加。

- **控制流**：

 	- `LABEL`：输出 `<func>_L<id>:`；
 	- `GOTO`：`j <func>_L<id>`；
 	- `IF`：`bne condReg, $zero, <func>_L<id>`。

- **CALL/RETURN**：

 	- `ARG`：前 4 个参数写 `$a0-$a3`；其余暂存于 `pendingExtraArgs_`，在 `CALL` 中统一压栈；
 	- `CALL`：将额外参数按顺序 `sw` 到栈上，`jal funcName` 后再 `addiu $sp, $sp, extraBytes` 弹栈；若有返回值，将 `$v0` 拷贝到对应临时并按需落栈；
 	- `RETURN`：将常量或表达式结果写入 `$v0` 后，`j func_END`。

## 运行时辅助例程

`emitTextSection` 在所有函数之后内联了两个简化运行时函数：

- `printf`：
 	- 使用 syscall 1/11 实现整数与字符输出；
 	- 遍历格式字符串，仅支持 `%d` 占位符，其余字符逐字打印；
 	- 在进入时保存 `$t0-$t2` 与 `$a0-$a3`，返回前恢复，并使用 `jr $ra` 返回。

- `getint`：
 	- 调用 syscall 5 从标准输入读取一个整数；
 	- 结果存入 `$v0`，`jr $ra` 返回。

这些例程不通过 IR 生成，而是直接由 AsmGen 拼接文本，方便在所有程序中复用。

---

以上描述了当前后端实现的整体结构与关键策略，尤其是寄存器分配与栈帧布局的细节。若后续修改调用约定、引入新的寄存器分类或增加优化（如跨块重写），请同步更新本文件以保持文档与代码一致。
