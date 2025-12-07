#include "codegen/BasicBlock.hpp"

BasicBlock::BasicBlock(int id) : _id(id) {}

void BasicBlock::addInstruction(std::unique_ptr<Instruction> inst) {
  _instructions.push_back(std::move(inst));
}

std::vector<std::unique_ptr<Instruction>> &BasicBlock::getInstructions() {
  return _instructions;
}

const std::vector<std::unique_ptr<Instruction>> &BasicBlock::getInstructions() const {
  return _instructions;
}

int BasicBlock::getId() const { return _id; }
