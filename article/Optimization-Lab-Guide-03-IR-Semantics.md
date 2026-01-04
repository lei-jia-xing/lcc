# 3. 先立规矩：IR 语义、use/def、副作用 (这个非常重要！！！)

优化写不动、bug 修不完，十有八九是你对 IR 的语义没有建立一套严格规则。

## 3.1 一条 IR 指令到底“读了什么/写了什么”？

一定要先把 use/def 定义清楚。以本项目的 IR 为例：

- `STORE value, base, index`
  - **use**：`value`、`base`、`index`
  - **def**：无（它写内存，但不产生 SSA 值）
- `LOAD base, index, dst`
  - **use**：`base`、`index`
  - **def**：`dst`
- `RETURN x`
  - **use**：`x`
  - **def**：无
- `CALL argc, f, dst`
  - **use**：参数序列（通常由 `ARG` 提供）
  - **def**：`dst`（如果有返回值）
  - **副作用**：默认当作可能读/写任意内存（除非实现纯函数分析）

典型坑：把 `STORE` 的 `result` 当作 def 或忽略其 index/use，会导致活跃性/复制传播/DCE 全乱。

## 3.2 副作用集合要保守

DCE/CSE 这类优化最怕删错。一个足够保守的副作用集合通常包含：

- `STORE`
- `CALL`
- 控制流：`IF/GOTO/LABEL/RETURN`
- `ALLOCA/PARAM/ARG/PHI`（在一些实现中它们承担结构信息/管线约束）

保守不意味着不优化，而是先保证正确性，再逐步放宽。
