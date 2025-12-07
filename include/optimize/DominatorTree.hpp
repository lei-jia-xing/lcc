#pragma once

#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include <map>
#include <set>
#include <vector>

class DominatorTree {
public:
  /**
   * @brief runs the dominator tree analysis for the given function.
   *
   * @param F the given function
   */
  void run(Function &F);

  /**
   * @brief check if block A dominates block B
   *
   * @param A block A
   * @param B block B
   * @return whether A dominates B
   */
  bool dominates(BasicBlock *A, BasicBlock *B) const;

  /**
   * @brief get the immediate dominator of block B
   *
   * @param B block B
   * @return the immediate dominator of B
   */
  BasicBlock *getImmediateDominator(BasicBlock *B) const;

  /**
   * @brief get the set of blocks dominated by block B
   * (include B itself), read only
   *
   * @param B block B
   */
  const std::set<BasicBlock *> &getDominatedBlocks(BasicBlock *B) const;

private:
  /**
   * @brief Map from a BasicBlock to its set of dominators (including itself).
   */
  std::map<BasicBlock *, std::set<BasicBlock *>> _dominators;

  /**
   * @brief Map from a BasicBlock to its immediate dominator.
   */
  std::map<BasicBlock *, BasicBlock *> _idoms;

  /**
   * @brief Map from a BasicBlock to the set of blocks it immediately dominates.
   */
  std::map<BasicBlock *, std::set<BasicBlock *>> _dominatedBy;

  /**
   * @brief helper function to get all predecessors of a basic
   * block.
   *
   * @param BB basicBlock
   * @param F function
   */
  std::vector<BasicBlock *> getPredecessors(BasicBlock *BB, Function &F);
};
