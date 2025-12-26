#pragma once

#include "Instruction.hpp"
#include <memory>
#include <vector>

class BasicBlock {
public:
  explicit BasicBlock(int id);

  void addInstruction(std::unique_ptr<Instruction> inst);
  std::vector<std::unique_ptr<Instruction>> &getInstructions();
  const std::vector<std::unique_ptr<Instruction>> &getInstructions() const;

  /**
   * @brief identify basicBlock
   *
   * @return basicBlock id
   */
  int getId() const;

  /**
   * @brief get the label id associated with this basic block
   *
   * @return label id or -1 if no label found
   */
  int getLabelId() const;

  std::shared_ptr<BasicBlock> next = nullptr;
  std::shared_ptr<BasicBlock> jumpTarget = nullptr;

private:
  int _id;
  std::vector<std::unique_ptr<Instruction>> _instructions;
};
