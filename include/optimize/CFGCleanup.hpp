#pragma once

#include "codegen/Function.hpp"

class CFGCleanupPass {
public:
  bool run(Function &func);

private:
  /**
   * @brief remove unreachable blocks from the function's CFG
   *
   * @param func function to optimize
   * @return whether any blocks were removed
   */
  bool removeUnreachableBlocks(Function &func);
  /**
   * @brief merge basic blocks where possible
   *
   * @param func function to optimize
   * @return whether any blocks were merged
   */
  bool mergeBlocks(Function &func);
  /**
   * @brief remove empty basic blocks from the function's CFG
   *
   * @param func function to optimize
   * @return whether any blocks were removed
   */
  bool removeEmptyBlocks(Function &func);
  /**
   * @brief remove NOP instructions from the function
   *
   * @param func function to optimize
   * @return whether any NOPs were removed
   */
  bool removeNops(Function &func);
};
