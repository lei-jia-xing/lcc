#include "optimize/DominatorTree.hpp"
#include <algorithm>

void DominatorTree::run(Function &F) {
  _dominators.clear();
  _idoms.clear();
  _dominatedBy.clear();

  const auto &blocks = F.getBlocks();
  if (blocks.empty()) {
    return;
  }

  std::vector<BasicBlock *> rawBlocks;
  for (const auto &b_ptr : blocks) {
    rawBlocks.push_back(b_ptr.get());
  }

  BasicBlock *entryBlock = rawBlocks[0];

  // The entry block dominates only itself.
  _dominators[entryBlock].insert(entryBlock);
  // All other blocks initially are dominated by all blocks.
  for (BasicBlock *bb : rawBlocks) {
    if (bb != entryBlock) {
      for (BasicBlock *otherBb : rawBlocks) {
        _dominators[bb].insert(otherBb);
      }
    }
  }

  // Dom(entry) = {entry}
  // Dom(b) = b + intersection of Dom(p) for all predecessors p of b, b != entry
  bool changed = true;
  while (changed) {
    changed = false;
    for (BasicBlock *bb : rawBlocks) {

      if (bb == entryBlock)
        continue; // entry block no need to be processed

      std::vector<BasicBlock *> preds = getPredecessors(bb, F);
      if (preds.empty()) { // Unreachable block
        if (!_dominators[bb].empty()) {
          _dominators[bb].clear(); // Clear dominators for unreachable block
          changed = true;
        }
        continue;
      }

      // Compute intersection of dominators of predecessors
      std::set<BasicBlock *> newDominators = _dominators[preds[0]];
      for (size_t i = 1; i < preds.size(); ++i) {
        std::set<BasicBlock *> predDom = _dominators[preds[i]];
        std::set<BasicBlock *> intersection;
        std::set_intersection(
            newDominators.begin(), newDominators.end(), predDom.begin(),
            predDom.end(), std::inserter(intersection, intersection.begin()));
        newDominators = intersection;
      }

      // Add current block itself
      newDominators.insert(bb);

      if (newDominators != _dominators[bb]) {
        _dominators[bb] = newDominators;
        changed = true;
      }
    }
  }

  /*
   * defination of immediate dominator:
   * 1. A dominates B
   * 2. A != B
   * 3. A is not dominated by any other dominator of B(the closest to B)
   */

  // Compute immediate dominators (_idoms) and dominatedBy sets (_dominatedBy)
  for (BasicBlock *bb : rawBlocks) {
    if (bb == entryBlock) {
      _idoms[bb] = nullptr;
      continue;
    }

    BasicBlock *idom = nullptr;

    // Simplified idom computation:
    // The immediate dominator of B is the dominator A (A!=B) with the largest
    // dominator set.
    size_t maxDoms = 0;
    BasicBlock *bestIdom = nullptr;
    for (BasicBlock *dom : _dominators[bb]) {
      if (dom == bb)
        continue;
      if (_dominators[dom].size() > maxDoms) {
        maxDoms = _dominators[dom].size();
        bestIdom = dom;
      }
    }
    _idoms[bb] = bestIdom;
    if (bestIdom) {
      _dominatedBy[bestIdom].insert(bb);
    }
  }
}

bool DominatorTree::dominates(BasicBlock *A, BasicBlock *B) const {
  auto it = _dominators.find(B);
  if (it != _dominators.end()) {
    return it->second.count(A) > 0;
  }
  return false;
}

BasicBlock *DominatorTree::getImmediateDominator(BasicBlock *B) const {
  auto it = _idoms.find(B);
  if (it != _idoms.end()) {
    return it->second;
  }
  return nullptr;
}

const std::set<BasicBlock *> &
DominatorTree::getDominatedBlocks(BasicBlock *B) const {
  auto it = _dominatedBy.find(B);
  if (it != _dominatedBy.end()) {
    return it->second;
  }
  // Return an empty set if B does not dominate any other block
  static const std::set<BasicBlock *> emptySet;
  return emptySet;
}

std::vector<BasicBlock *> DominatorTree::getPredecessors(BasicBlock *BB,
                                                         Function &F) {
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
