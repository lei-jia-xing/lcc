#include "codegen/BasicBlock.hpp"

BasicBlock::BasicBlock(int id) : _id(id) {}

void BasicBlock::addInstruction(Instruction inst) {
  _instructions.emplace_back(std::move(inst));
}

std::vector<Instruction> &BasicBlock::getInstructions() {
  return _instructions;
}

int BasicBlock::getId() const { return _id; }
