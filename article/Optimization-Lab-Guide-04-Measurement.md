# 4. 性能评估方法

事实上，教学组提供的MIPS仿真器MARS可以支持命令行参数，通过这个我们可以构建快速测试脚本，或者说优化效果报告

```

#!/bin/bash

set -e

cd build

./Compiler

java -jar ../MARS2025+.jar nc mips.txt
```

运行这个脚本，我们会在`Compiler`同目录下得到一个 `InstructionStatistics.txt` ，里面列出了你的`Cycle`得分

这里有两个经验：

1. **先看 Memory 与 Jump/Branch**：load/store 和跳转往往是大头（局部变量的使用，函数递归调用...），我们可以通过Mem2Reg，以及图着色算法作为基础（想要优化效果好，必须有这个作为良好的基础）
2. **看冗余模式**：比如同一段里重复的 `la`、`addiu`、`move`，这些通常来自 IR 的临时变量搬运或后端地址生成策略。

如果你发现：

- IR 里 `NOP` 很多：说明优化把指令“标记为删除”但没有做统一清理。
- IR 里 `ASSIGN` 链很多：说明 SSA 销毁、复制传播或参数传递产生了大量 copy。

它们都属于“肉眼可见、很值得做”的优化点。
