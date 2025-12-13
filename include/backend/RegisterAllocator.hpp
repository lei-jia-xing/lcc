#pragma once

#include "codegen/Function.hpp"
#include <map>
#include <set>
#include <vector>

class RegisterAllocator {
public:
  using LiveSet = std::set<int>;

  RegisterAllocator();
  ~RegisterAllocator();

  void run(Function *func);

  int getReg(int tempId) const;
  bool isSpilled(int tempId) const;
  const LiveSet &getSpilledNodes() const;
  std::set<int> getUsedRegs() const;

private:
  void computeUseDef(Function *func);
  void computeLiveInOut(Function *func);
  void buildInterferenceGraph(Function *func);
  void doColoring();

  // $s0-$s7, $t0-t5
  static const int NumRegs = 14;

  std::map<const BasicBlock *, LiveSet> _use;
  std::map<const BasicBlock *, LiveSet> _def;
  std::map<const BasicBlock *, LiveSet> _liveIn;
  std::map<const BasicBlock *, LiveSet> _liveOut;

  std::map<int, LiveSet> _interferenceGraph;
  LiveSet _temps;

  std::vector<int> _nodeStack;
  std::map<int, int> _coloredNodes; // tempId -> reg index
  /**
   * @brief spilled nodes after coloring
   */
  LiveSet _spilledNodes;
};
