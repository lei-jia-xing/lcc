#pragma once

#include "LoopAnalysis.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include <map>
#include <set>
#include <vector>

class LoopAnalysis;
class DominatorTree;

class LICMPass {
public:
  /**
   * @brief runs the loop-invariant code motion optimization on the given
   * function.
   *
   * @param F fucntion to optimize
   * @param DT Dominator tree of the function
   * @param loops loop information of the function
   */
  void run(Function &F, DominatorTree &DT, std::vector<LoopInfo> &loops);

private:
  struct DefInfo {
    Instruction *inst;
    BasicBlock *block;
  };
  bool isLoopInvariant(const Instruction *inst, const LoopInfo &loop,
                       const std::map<int, DefInfo> &defMap,
                       const std::set<const Instruction *> &invariants,
                       const std::set<int> &modifiedVars, bool hasCall);
  BasicBlock *getOrCreatePreheader(LoopInfo &loop, Function &F);
};
