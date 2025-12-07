#include "optimize/LoopAnalysis.hpp"
#include <set>
#include <stack>

// Helper to get successors of a basic block.
// This is a common utility function that might be moved to a shared place
// later.
static std::vector<BasicBlock *> getSuccessors(BasicBlock *BB) {
  std::vector<BasicBlock *> successors;
  if (BB->next) {
    successors.push_back(BB->next.get());
  }
  if (BB->jumpTarget) {
    successors.push_back(BB->jumpTarget.get());
  }
  return successors;
}

// Helper to get predecessors of a basic block.
static std::vector<BasicBlock *> getPredecessors(BasicBlock *BB, Function &F) {
  std::vector<BasicBlock *> predecessors;
  const auto &blocks = F.getBlocks();

  for (const auto &b_ptr : blocks) {
    BasicBlock *currentBlock = b_ptr.get();
    if (currentBlock == BB)
      continue; // A block cannot be its own predecessor in this context

    // Check fallthrough edge
    if (currentBlock->next.get() == BB) {
      predecessors.push_back(currentBlock);
    }
    // Check jump target edge
    if (currentBlock->jumpTarget.get() == BB) {
      predecessors.push_back(currentBlock);
    }
  }
  return predecessors;
}

void LoopAnalysis::run(Function &F, DominatorTree &DT) {
  _loops.clear();

  const auto &blocks = F.getBlocks();
  if (blocks.empty()) {
    return;
  }

  // Convert shared_ptr to raw pointers for iteration
  std::vector<BasicBlock *> rawBlocks;
  for (const auto &b_ptr : blocks) {
    rawBlocks.push_back(b_ptr.get());
  }

  // Find back edges and identify natural loops
  for (BasicBlock *currentBlock : rawBlocks) {
    std::vector<BasicBlock *> successors = getSuccessors(currentBlock);
    for (BasicBlock *successor : successors) {
      // An edge (currentBlock -> successor) is a back edge if successor
      // dominates currentBlock
      if (DT.dominates(successor, currentBlock)) {
        // Found a back edge, 'successor' is the loop header
        LoopInfo newLoop(successor);
        findLoopBlocks(successor, currentBlock, DT, F, newLoop.blocks);

        // Populate exit blocks
        for (BasicBlock *loopBb : newLoop.blocks) {
          std::vector<BasicBlock *> loopBbSuccessors = getSuccessors(loopBb);
          for (BasicBlock *succ : loopBbSuccessors) {
            if (!newLoop.contains(succ)) { // If successor is not in the loop,
                                           // it's an exit block
              newLoop.exitBlocks.insert(succ);
            }
          }
        }
        _loops.push_back(newLoop);
      }
    }
  }
}

const std::vector<LoopInfo> &LoopAnalysis::getLoops() const { return _loops; }

void LoopAnalysis::findLoopBlocks(BasicBlock *header, BasicBlock *backEdgeSrc,
                                  const DominatorTree &DT, Function &F,
                                  std::set<BasicBlock *> &loopBlocks) {
  std::stack<BasicBlock *> worklist;

  loopBlocks.clear();        // Ensure it's clean for this loop
  loopBlocks.insert(header); // Header is always part of the loop

  // Start with the source of the back edge, if it's not the header itself
  if (backEdgeSrc != header) {
    worklist.push(backEdgeSrc);
  }

  while (!worklist.empty()) {
    BasicBlock *current = worklist.top();
    worklist.pop();

    if (loopBlocks.find(current) == loopBlocks.end()) { // If not already added
      loopBlocks.insert(current);
      // Add all predecessors of 'current' to the worklist
      std::vector<BasicBlock *> predecessors = getPredecessors(current, F);
      for (BasicBlock *pred : predecessors) {
        worklist.push(pred);
      }
    }
  }
}
