# 2. 优化管线长什么样

先看整体结构。

在 LCC 编译器中，大致是：

1. 前端：Lexer/Parser/Semantic
2. 生成 IR（四元式/基本块）
3. **SSA 构建**：DominatorTree + Mem2Reg
4. **优化循环**：SCCP/ConstProp/CopyProp/CSE/DCE 等（迭代至不动点或跑几轮）
5. 循环相关：LoopAnalysis + LICM + LoopUnroll
6. **SSA 销毁**：PhiElimination
7. 后端：AsmGen（IR -> MIPS）
