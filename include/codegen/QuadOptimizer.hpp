#pragma once

#include "codegen/Function.hpp"
#include <memory>
#include <vector>

// 抽象 Pass：对单个 Function 做转换，返回是否修改
class QuadPass {
public:
  virtual ~QuadPass() = default;
  virtual bool run(Function &fn) = 0;
};

// Pass 管理器：按顺序运行多个 Pass
class PassManager {
public:
  void add(std::unique_ptr<QuadPass> pass) {
    passes.emplace_back(std::move(pass));
  }
  void run(Function &fn) {
    bool changed = true;
    // 简单的固定点迭代：直到一次循环没有改动
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

// 常量折叠
class ConstFoldPass : public QuadPass {
public:
  bool run(Function &fn) override;
};

// 局部死代码消除（基于基本块、无副作用指令）
class LocalDCEPass : public QuadPass {
public:
  bool run(Function &fn) override;
};

// 默认优化管线（可扩展）
void runDefaultQuadOptimizations(Function &fn);
