#include "optimize/CFGCleanup.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Instruction.hpp"
#include <algorithm>
#include <map>
#include <set>

bool CFGCleanupPass::run(Function &func) {
  bool changed = false;
  bool localChanged = true;
  while (localChanged) {
    localChanged = false;
    if (removeUnreachableBlocks(func)) {
      localChanged = true;
      changed = true;
    }
    if (mergeBlocks(func)) {
      localChanged = true;
      changed = true;
    }
    if (removeNops(func)) {
      localChanged = true;
      changed = true;
    }
  }
  return changed;
}

bool CFGCleanupPass::removeNops(Function &func) {
  bool changed = false;
  for (auto &bb : func.getBlocks()) {
    auto &insts = bb->getInstructions();
    auto it = std::remove_if(insts.begin(), insts.end(),
                             [](const std::unique_ptr<Instruction> &inst) {
                               return inst->getOp() == OpCode::NOP;
                             });
    if (it != insts.end()) {
      insts.erase(it, insts.end());
      changed = true;
    }
  }
  return changed;
}

bool CFGCleanupPass::removeUnreachableBlocks(Function &func) {
  if (func.getBlocks().empty())
    return false;

  std::set<BasicBlock *> visited;
  std::vector<BasicBlock *> worklist;

  BasicBlock *entry = func.getBlocks().front().get();
  visited.insert(entry);
  worklist.push_back(entry);

  while (!worklist.empty()) {
    BasicBlock *bb = worklist.back();
    worklist.pop_back();

    std::vector<BasicBlock *> succs;
    if (bb->next)
      succs.push_back(bb->next.get());
    if (bb->jumpTarget)
      succs.push_back(bb->jumpTarget.get());

    for (auto *succ : succs) {
      if (visited.find(succ) == visited.end()) {
        visited.insert(succ);
        worklist.push_back(succ);
      }
    }
  }

  auto &blocks = func.getBlocks();
  auto originalSize = blocks.size();

  auto it = std::remove_if(blocks.begin(), blocks.end(),
                           [&](const std::shared_ptr<BasicBlock> &bb) {
                             return visited.find(bb.get()) == visited.end();
                           });

  if (it == blocks.end()) {
    return false;
  }

  blocks.erase(it, blocks.end());
  return true;
}

bool CFGCleanupPass::mergeBlocks(Function &func) {
  // Build predecessor map
  std::map<BasicBlock *, std::vector<BasicBlock *>> preds;
  for (auto &bb : func.getBlocks()) {
    if (bb->next)
      preds[bb->next.get()].push_back(bb.get());
    if (bb->jumpTarget)
      preds[bb->jumpTarget.get()].push_back(bb.get());
  }

  auto &blocks = func.getBlocks();
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    BasicBlock *bb = it->get();

    // Case 1: bb falls through to next, and next has only bb as pred
    if (bb->next && !bb->jumpTarget) {
      BasicBlock *succ = bb->next.get();
      if (preds[succ].size() == 1 && preds[succ][0] == bb &&
          succ != blocks.front().get()) {
        // Merge succ into bb
        auto &succInsts = succ->getInstructions();
        for (auto &inst : succInsts) {
          if (inst->getOp() == OpCode::LABEL)
            continue;
          inst->setParent(bb);
          bb->addInstruction(std::move(inst));
        }

        bb->next = succ->next;
        bb->jumpTarget = succ->jumpTarget;

        auto succIt = std::find_if(blocks.begin(), blocks.end(),
                                   [&](const std::shared_ptr<BasicBlock> &b) {
                                     return b.get() == succ;
                                   });
        if (succIt != blocks.end()) {
          blocks.erase(succIt);
        }
        return true; // Restart to be safe
      }
    }

    // Case 2: bb jumps unconditionally to jumpTarget (GOTO), and jumpTarget has
    // only bb as pred
    if (bb->jumpTarget && !bb->next) {
      BasicBlock *succ = bb->jumpTarget.get();
      if (preds[succ].size() == 1 && preds[succ][0] == bb &&
          succ != blocks.front().get()) {
        // Remove the GOTO from bb
        if (!bb->getInstructions().empty() &&
            bb->getInstructions().back()->getOp() == OpCode::GOTO) {
          bb->getInstructions().pop_back();
        }

        auto &succInsts = succ->getInstructions();
        for (auto &inst : succInsts) {
          if (inst->getOp() == OpCode::LABEL)
            continue;
          inst->setParent(bb);
          bb->addInstruction(std::move(inst));
        }

        bb->next = succ->next;
        bb->jumpTarget = succ->jumpTarget;

        auto succIt = std::find_if(blocks.begin(), blocks.end(),
                                   [&](const std::shared_ptr<BasicBlock> &b) {
                                     return b.get() == succ;
                                   });
        if (succIt != blocks.end()) {
          blocks.erase(succIt);
        }
        return true;
      }
    }
  }

  return false;
}
