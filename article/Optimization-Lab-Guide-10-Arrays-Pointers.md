# 10. 数组与指针

> [!NOTE]
> 这里的内容是和我的ir设计绑定的，我没有特意区分传递数组参数还是值，因为我们可以把指针也看作值传递。
>
## 10.1 数组形参不是“数组对象”

在 C-like 语言里，`int arr[]` 作为函数参数，语义上等价于 `int* arr`。

在本项目的 IR lowering 中，数组形参常表现为：

- `ALLOCA arr, 1`：在栈上分配一个 slot
- `STORE t0, arr`：把传入的地址写进 slot

这意味着：

- `arr` 这个符号代表的是“slot 的地址”（address-of-address）
- 想取真正数组基址，需要先 `LOAD arr, 0` 读出 slot 里的指针值

## 10.2 一个典型错误优化（以及如何修）

错误的想法：

- “既然数组访问经常出现，我把 base 先提出来：插入 `base = LOAD arr, Empty`，后面都用 base”

问题在于：

- 对数组形参的 slot 执行 `LOAD arr, Empty`，取到的是 slot 的地址，不是 slot 里存的指针
- 后续 `LOAD base, idx` 就会读错地址，轻则结果错，重则 crash

正确的修法（保守策略）：

- 区分真实数组对象 vs 数组形参
- 对数组形参（常见特征：`ALLOCA size==1` 且类型是 Array/Pointer-like），跳过这类 base hoist

如果你想进一步优化数组形参，需要引入更明确的“指针类型”建模或专门的 AddressOf/LoadAddr IR，把语义拆清楚。
