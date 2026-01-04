# 6. SSA 优化循环：常量传播、DCE、CSE、SCCP

这一部分建议做成一个 PassManager 循环跑几轮：

- SCCP（跨 CFG 的稀疏常量传播，顺便删不可达）
- ConstProp（块内常量传播/折叠）
- CopyProp（复制传播）
- Algebraic（代数化简）
- CSE（公共子表达式消除，支配树驱动）
- LocalDCE（死代码消除）
- Cleanup（统一清 NOP、自拷贝、copy 链）

## 6.1 SCCP：不是“更强的 ConstProp”，而是 CFG 简化器

SCCP 的收益常常不是“算出一个常量”，而是：

- 把 `IF const` 改成 `GOTO` 或删掉，这类我们可以在编译期间就可以知道分支的走向的我们可以直接简化
- 删除不可达块
- Phi 退化成 ASSIGN

它会显著缩短后续 pass 的工作量。

## 6.2 DCE 与副作用

DCE 的正确性来自“如果结果不被 use 且无副作用，就能删”。

## 6.3 CSE 的关键：表达式等价与作用域

- 交换律：`a+b` 与 `b+a` 的 key 要一致
- 作用域：用支配树 DFS + 栈式作用域哈希表（进入/退出作用域）
