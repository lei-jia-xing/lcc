#pragma once

#include "codegen/Function.hpp"
class PhiEliminationPass {

public:
  void run(Function &F);
};
