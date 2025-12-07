#pragma once

#include "BasicBlock.hpp"
#include <memory>
#include <string>
#include <vector>

class Function {
public:
  explicit Function(std::string name);

  std::shared_ptr<BasicBlock> createBlock();
  /**
   * @brief getter for blocks, read only
   */
  const std::vector<std::shared_ptr<BasicBlock>> &getBlocks() const;
  /**
   * @brief getter for blocks, read, write
   */
  std::vector<std::shared_ptr<BasicBlock>> &getBlocks();
  /**
   * @brief getter for name
   */
  const std::string &getName() const;
  /**
   * @brief helper function to find the block the instruction in
   *
   * @param inst instruction
   * @return the outter basicblock
   */
  BasicBlock *findBlockOf(const Instruction *inst) const;

  /**
   * @brief Build the CFG by splitting basic blocks at control flow
   * instructions.
   *
   */
  void buildCFG();

private:
  /**
   * @brief funtion name
   */
  std::string _name;
  /**
   * @brief function local basicblocks
   */
  std::vector<std::shared_ptr<BasicBlock>> _blocks;
  /**
   * @brief function local block id
   */
  int _nextBlockId = 0;
};
