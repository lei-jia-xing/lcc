#pragma once

#include "codegen/Function.hpp"
#include "optimize/DominatorTree.hpp"
#include <set>
#include <stack>

/**
 * @class AllocaInfo
 * @brief helper struct to store information about an alloca instruction
 *
 */
struct AllocaInfo {
  Instruction *allocaInst;
  int varId;
  std::set<BasicBlock *> defBlocks;
  std::vector<Instruction *> usingInsts;
  bool isPromotable;
};

class Mem2RegPass {
public:
  Mem2RegPass() = default;

  bool run(Function &F, DominatorTree &DT);

private:
  void collectPromotableAllocas(Function &F);
  void computeDominanceFrontiers(Function &F, DominatorTree &DT);
  void insertPhiNodes(Function &F);
  void renameVariables(BasicBlock *BB, DominatorTree &DT);
  void eliminatePhis(Function &F);

  std::map<int, AllocaInfo> _allocas;
  std::map<BasicBlock *, std::set<BasicBlock *>> _domFrontiers;
  std::map<Instruction *, int> _phiToVarId;
  std::map<int, std::stack<Operand>> _varStacks;
};
