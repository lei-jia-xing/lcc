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
  /**
   * @brief collect alloca instructions that can be promoted to registers
   *
   * @param F function to process
   */
  void collectPromotableAllocas(Function &F);
  /**
   * @brief compute dominance frontiers for the function
   *
   * @param F function to process
   * @param DT dominator tree of the function
   */
  void computeDominanceFrontiers(Function &F, DominatorTree &DT);
  /**
   * @brief insert phi nodes for variables that need them
   *
   * @param F function to process
   */
  void insertPhiNodes(Function &F);
  /**
   * @brief rename variables in the function to use SSA form
   *
   * @param BB basic block to start renaming from
   * @param DT dominator tree of the function
   * @param F function to process
   */
  void renameVariables(BasicBlock *BB, DominatorTree &DT, Function &F);

  std::map<int, AllocaInfo> _allocas;
  std::map<BasicBlock *, std::set<BasicBlock *>> _domFrontiers;
  std::map<Instruction *, int> _phiToVarId;
  std::map<int, std::stack<Operand>> _varStacks;
};
