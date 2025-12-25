#pragma once

#include "codegen/Function.hpp"
class PhiEliminationPass {

public:
  void run(Function &F);

private:
  std::shared_ptr<BasicBlock> getBlockSharedPtr(Function &F,
                                                BasicBlock *rawPtr);
};
