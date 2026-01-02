#pragma once

#include "LoopAnalysis.hpp"
#include "codegen/Function.hpp"

class LoopUnrollPass {
public:
  bool run(Function &func, const std::vector<LoopInfo> &loops);

private:
  bool tryUnrollLoop(Function &func, const LoopInfo &loop);
  bool isSimpleLoop(const LoopInfo &loop, int &tripCount, Operand &iv,
                    int &step, int &initVal);
};
