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

private:
  void computeUseDef(Function *func);
  void computeLiveInOut(Function *func);
  void buildInterferenceGraph(Function *func);
  void doColoring();

  static const int NumRegs = 10;

  std::map<const BasicBlock *, LiveSet> _use;
  std::map<const BasicBlock *, LiveSet> _def;
  std::map<const BasicBlock *, LiveSet> _liveIn;
  std::map<const BasicBlock *, LiveSet> _liveOut;

  std::map<int, LiveSet> _interferenceGraph;
  LiveSet _temps;

  std::vector<int> _nodeStack;
  std::map<int, int> _coloredNodes; // tempId -> reg index
  LiveSet _spilledNodes;
};
