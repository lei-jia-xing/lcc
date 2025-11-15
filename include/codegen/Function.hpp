#pragma once

#include "BasicBlock.hpp"
#include <memory>
#include <string>
#include <vector>

namespace lcc::codegen {

class Function {
public:
  explicit Function(std::string name);

  std::shared_ptr<BasicBlock> createBlock();
  std::vector<std::shared_ptr<BasicBlock>> &getBlocks();
  const std::string &getName() const;

private:
  std::string _name;
  std::vector<std::shared_ptr<BasicBlock>> _blocks;
  int _nextBlockId = 0;
};

} // namespace lcc::codegen
