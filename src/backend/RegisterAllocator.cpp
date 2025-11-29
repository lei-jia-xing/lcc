#include "backend/RegisterAllocator.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include <algorithm>
#include <vector>

RegisterAllocator::RegisterAllocator() {}
RegisterAllocator::~RegisterAllocator() {}

void RegisterAllocator::run(Function *func) {
  computeUseDef(func);
  computeLiveInOut(func);
  buildInterferenceGraph(func);
  doColoring();
}

int RegisterAllocator::getReg(int tempId) const {
  auto it = _coloredNodes.find(tempId);
  if (it != _coloredNodes.end()) {
    return it->second;
  }
  return -1;
}

bool RegisterAllocator::isSpilled(int tempId) const {
  return _spilledNodes.count(tempId) > 0;
}

const RegisterAllocator::LiveSet &RegisterAllocator::getSpilledNodes() const {
  return _spilledNodes;
}

std::set<int> RegisterAllocator::getUsedRegs() const {
  std::set<int> usedRegs;
  for (const auto &pair : _coloredNodes) {
    usedRegs.insert(pair.second);
  }
  return usedRegs;
}

void RegisterAllocator::computeUseDef(Function *func) {
  _temps.clear();
  for (auto &block : func->getBlocks()) {
    LiveSet definedInBlock;
    _use[block.get()] = LiveSet();
    _def[block.get()] = LiveSet();

    for (auto &inst : block->getInstructions()) {
      auto checkUse = [&](const Operand &op) {
        if (op.getType() == OperandType::Temporary) {
          _temps.insert(op.asInt());
          if (definedInBlock.find(op.asInt()) == definedInBlock.end()) {
            _use[block.get()].insert(op.asInt());
          }
        }
      };

      checkUse(inst.getArg1());
      checkUse(inst.getArg2());
      if (inst.getOp() == OpCode::STORE) {
        // result of STORE is index, is use, not def
        checkUse(inst.getResult());
      } else {
        if (inst.getResult().getType() == OperandType::Temporary) {
          int tempId = inst.getResult().asInt();
          _def[block.get()].insert(tempId);
          definedInBlock.insert(tempId);
          _temps.insert(tempId);
        }
      }
    }
  }
}

void RegisterAllocator::computeLiveInOut(Function *func) {
  for (auto &block : func->getBlocks()) {
    _liveIn[block.get()] = LiveSet();
    _liveOut[block.get()] = LiveSet();
  }

  bool changed = true;
  while (changed) {
    changed = false;

    std::vector<BasicBlock *> blocks;
    for (auto &block : func->getBlocks()) {
      blocks.push_back(block.get());
    }
    std::reverse(blocks.begin(), blocks.end());

    for (auto *block : blocks) {
      LiveSet newLiveOut;
      if (block->next) {
        newLiveOut.insert(_liveIn[block->next.get()].begin(),
                          _liveIn[block->next.get()].end());
      }
      if (block->jumpTarget) {
        newLiveOut.insert(_liveIn[block->jumpTarget.get()].begin(),
                          _liveIn[block->jumpTarget.get()].end());
      }

      if (newLiveOut != _liveOut[block]) {
        _liveOut[block] = newLiveOut;
        changed = true;
      }

      LiveSet newLiveIn = _use[block];
      LiveSet diff;
      std::set_difference(_liveOut[block].begin(), _liveOut[block].end(),
                          _def[block].begin(), _def[block].end(),
                          std::inserter(diff, diff.begin()));
      newLiveIn.insert(diff.begin(), diff.end());

      if (newLiveIn != _liveIn[block]) {
        _liveIn[block] = newLiveIn;
        changed = true;
      }
    }
  }
}

void RegisterAllocator::buildInterferenceGraph(Function *func) {
  _interferenceGraph.clear();
  for (int temp : _temps) {
    _interferenceGraph[temp] = LiveSet();
  }

  for (auto &block : func->getBlocks()) {
    LiveSet live = _liveOut[block.get()];

    for (auto it = block->getInstructions().rbegin();
         it != block->getInstructions().rend(); ++it) {
      auto &inst = *it;
      if (inst.getOp() != OpCode::STORE) {
        if (inst.getResult().getType() == OperandType::Temporary) {
          int def = inst.getResult().asInt();
          for (int t : live) {
            if (def != t) {
              _interferenceGraph[def].insert(t);
              _interferenceGraph[t].insert(def);
            }
          }
          live.erase(def);
        }
      }

      auto addLive = [&](const Operand &op) {
        if (op.getType() == OperandType::Temporary) {
          live.insert(op.asInt());
        }
      };
      addLive(inst.getArg1());
      addLive(inst.getArg2());
      if (inst.getOp() == OpCode::STORE) {
        addLive(inst.getResult());
      }
    }
  }
}

void RegisterAllocator::doColoring() {
  _nodeStack.clear();
  _coloredNodes.clear();
  _spilledNodes.clear();

  auto graph = _interferenceGraph;
  LiveSet nodes = _temps;

  while (nodes.size() > 0) {
    bool simplified = false;
    auto it = nodes.begin();
    while (it != nodes.end()) {
      int node = *it;
      bool condition;
      auto graph_it_node = graph.find(node);
      if (graph_it_node == graph.end()) {
        condition = true;
      } else {
        condition = graph_it_node->second.size() < NumRegs;
      }

      if (condition) {
        _nodeStack.push_back(node);
        if (graph_it_node != graph.end()) {
          for (int neighbor : graph_it_node->second) {
            auto neighbor_it = graph.find(neighbor);
            if (neighbor_it != graph.end()) {
              neighbor_it->second.erase(node);
            }
          }
          graph.erase(graph_it_node);
        }
        it = nodes.erase(it);
        simplified = true;
      } else {
        ++it;
      }
    }

    if (simplified)
      continue;

    if (nodes.size() > 0) {
      int spillNode = *nodes.begin();
      _nodeStack.push_back(spillNode);
      auto spill_it = graph.find(spillNode);
      if (spill_it != graph.end()) {
        for (int neighbor : spill_it->second) {
          auto neighbor_it = graph.find(neighbor);
          if (neighbor_it != graph.end()) {
            neighbor_it->second.erase(spillNode);
          }
        }
        graph.erase(spill_it);
      }
      nodes.erase(spillNode);
    }
  }

  std::reverse(_nodeStack.begin(), _nodeStack.end());

  for (int node : _nodeStack) {
    LiveSet neighborColors;
    if (_interferenceGraph.count(node) > 0) {
      for (int neighbor : _interferenceGraph.at(node)) {
        if (_coloredNodes.count(neighbor)) {
          neighborColors.insert(_coloredNodes.at(neighbor));
        }
      }
    }

    int color = -1;
    for (int i = 0; i < NumRegs; ++i) {
      if (neighborColors.find(i) == neighborColors.end()) {
        color = i;
        break;
      }
    }

    if (color != -1) {
      _coloredNodes[node] = color;
    } else {
      _spilledNodes.insert(node);
    }
  }
}
