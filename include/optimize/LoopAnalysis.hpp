#pragma once

#include "DominatorTree.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include <set>
#include <vector>

class DominatorTree;

struct LoopInfo {
  /**
   * @brief the header block of the loop
   */
  BasicBlock *header;
  /**
   * @brief all blocks in the loop
   */
  std::set<BasicBlock *> blocks;
  /**
   * @brief Blocks outside the loop that have edges from within the loop
   */
  std::set<BasicBlock *> exitBlocks;

  LoopInfo(BasicBlock *h) : header(h) {}
};

class LoopAnalysis {
public:
  void run(Function &F, DominatorTree &DT);

  /**
   * @brief all the loops found in the function
   */
  const std::vector<LoopInfo> &getLoops() const;

private:
  /**
   * @brief all the loops found in the function
   */
  std::vector<LoopInfo> _loops;

  // NOTE:
  // header must dominates backEdgeSrc

  /**
   * @brief helper function to find all blocks in a natural loop given a header
   *
   * @param header first block of the loop
   * @param backEdgeSrc the block where the back edge comes from (back to
   * header)
   * @param DT Dominator tree of the function
   * @param F current function
   * @param loopBlocks all the blocks in a loop
   */
  void findLoopBlocks(BasicBlock *header, BasicBlock *backEdgeSrc,
                      const DominatorTree &DT, Function &F,
                      std::set<BasicBlock *> &loopBlocks);
};
