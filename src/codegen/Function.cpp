#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include <unordered_map>

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

BasicBlock *Function::findBlockOf(const Instruction *inst) const {
  for (const auto &bb_ptr : _blocks) {
    for (const auto &inst_ptr : bb_ptr->getInstructions()) {
      if (inst_ptr.get() == inst) {
        return bb_ptr.get();
      }
    }
  }
  return nullptr;
}

void Function::buildCFG() {
  if (_blocks.empty())
    return;

  std::vector<std::unique_ptr<Instruction>> insts;
  for (auto &blk : _blocks) {
    for (auto &inst : blk->getInstructions()) {
      insts.push_back(std::move(inst));
    }
  }
  _blocks.clear();

  if (insts.empty())
    return;

  // First pass: identify split points and create blocks
  // A new block starts:
  // 1. At the beginning
  // 2. At each LABEL instruction
  // 3. After each GOTO, IF, or RETURN instruction

  std::vector<std::shared_ptr<BasicBlock>> newBlocks;
  std::unordered_map<int, std::shared_ptr<BasicBlock>> labelToBlock;

  auto startNewBlock = [&]() -> std::shared_ptr<BasicBlock> {
    auto blk = std::make_shared<BasicBlock>(_nextBlockId++);
    newBlocks.push_back(blk);
    return blk;
  };

  std::shared_ptr<BasicBlock> curBlk = startNewBlock();

  for (size_t i = 0; i < insts.size(); ++i) {
    auto &inst = insts[i];
    OpCode op = inst->getOp();

    // If this is a LABEL, start a new block (unless current block is empty)
    if (op == OpCode::LABEL) {
      if (!curBlk->getInstructions().empty()) {
        // Current block falls through to this label
        auto newBlk = startNewBlock();
        curBlk->next = newBlk;
        curBlk = newBlk;
      }
      int labelId = inst->getResult().asInt();
      labelToBlock[labelId] = curBlk;
      curBlk->addInstruction(std::move(inst));
      continue;
    }

    curBlk->addInstruction(std::move(inst));

    // After GOTO, IF, or RETURN, start a new block
    if (op == OpCode::GOTO || op == OpCode::IF || op == OpCode::RETURN) {
      if (i + 1 < insts.size()) {
        auto newBlk = startNewBlock();
        // For IF, there's a fallthrough edge
        if (op == OpCode::IF) {
          curBlk->next = newBlk;
        }
        // For GOTO, no fallthrough
        // For RETURN, no fallthrough
        curBlk = newBlk;
      }
    }
  }

  // Second pass: set up jumpTarget edges
  for (auto &blk : newBlocks) {
    auto &insts = blk->getInstructions();
    if (insts.empty())
      continue;

    auto &lastInst = insts.back();
    OpCode op = lastInst->getOp();

    if (op == OpCode::GOTO) {
      int targetLabel = lastInst->getResult().asInt();
      auto it = labelToBlock.find(targetLabel);
      if (it != labelToBlock.end()) {
        blk->jumpTarget = it->second;
      }
    } else if (op == OpCode::IF) {
      // IF cond, -, label
      int targetLabel = lastInst->getResult().asInt();
      auto it = labelToBlock.find(targetLabel);
      if (it != labelToBlock.end()) {
        blk->jumpTarget = it->second;
      }
    }
  }

  _blocks = std::move(newBlocks);
}
