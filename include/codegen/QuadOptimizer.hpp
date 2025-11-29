#pragma once

#include "codegen/Function.hpp"
#include <memory>
#include <vector>

class QuadPass {
public:
  virtual ~QuadPass() = default;
  virtual bool run(Function &fn) = 0;
};

class PassManager {
public:
  void add(std::unique_ptr<QuadPass> pass) {
    passes.emplace_back(std::move(pass));
  }
  void run(Function &fn) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto &p : passes) {
        changed |= p->run(fn);
      }
    }
  }

private:
  std::vector<std::unique_ptr<QuadPass>> passes;
};

class ConstFoldPass : public QuadPass {
public:
  bool run(Function &fn) override;
};

class LocalDCEPass : public QuadPass {
public:
  bool run(Function &fn) override;
};
void runDefaultQuadOptimizations(Function &fn);
