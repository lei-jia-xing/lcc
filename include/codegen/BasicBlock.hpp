#pragma once

#include "Instruction.hpp"
#include <memory>
#include <vector>

class BasicBlock {
public:
  explicit BasicBlock(int id);

  void addInstruction(Instruction inst);
  std::vector<Instruction> &getInstructions();

  int getId() const;

  std::shared_ptr<BasicBlock> next = nullptr;
  std::shared_ptr<BasicBlock> jumpTarget = nullptr;

private:
  int _id;
  std::vector<Instruction> _instructions;
};
