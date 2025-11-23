#include "codegen/Function.hpp"

Function::Function(std::string name) : _name(std::move(name)) {}

std::shared_ptr<BasicBlock> Function::createBlock() {
  auto blk = std::make_shared<BasicBlock>(_nextBlockId++);
  _blocks.emplace_back(blk);
  return blk;
}

const std::vector<std::shared_ptr<BasicBlock>> &Function::getBlocks() const {
  return _blocks;
}

std::vector<std::shared_ptr<BasicBlock>> &Function::getBlocks() {
  return _blocks;
}

const std::string &Function::getName() const { return _name; }
