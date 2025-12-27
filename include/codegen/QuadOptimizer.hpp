#pragma once

#include "codegen/Function.hpp"
#include "optimize/DominatorTree.hpp"
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
  bool run(Function &fn) {
    bool changed = true;
    int round = 3;
    while (changed && round > 0) {
      changed = false;
      for (auto &p : passes) {
        changed |= p->run(fn);
      }
      round--;
    }
    return changed;
  }

private:
  std::vector<std::unique_ptr<QuadPass>> passes;
};

/**
 * @class LocalDCEPass
 * @brief Dead Code Elimination in funtion scope
 *
 */
class LocalDCEPass : public QuadPass {
public:
  bool run(Function &fn) override;
};

/**
 * @class ConstPropPass
 * @brief const propagation in function scope
 * if a temp is assigned a constant value and
 * never modified, we can replace all its uses
 * with the constant value.
 * for example:
 *    int a = 5;
 *    int b = a + 3;
 * we can replace 'a' with '5' in the second
 * line,and b can be computed at compile time.
 *
 */
class ConstPropPass : public QuadPass {
public:
  bool run(Function &fn) override;
};

/**
 * @class AlgebraicSimplifyPass
 * @brief simplify algebraic expressions
 * x + 0, 0 + x  -> x
 * x - 0 -> x
 * x * 0, 0 * x -> x
 * x * 1, 1 * x -> x
 * x / 1 -> x
 * x % 1 -> 0, only int
 *
 */
class AlgebraicSimplifyPass : public QuadPass {
public:
  bool run(Function &fn) override;
};

/**
 * @class CopyPropPass
 * @brief copy propagation in function scope
 * example:
 *    t0 = a
 *    t1 = t0
 * we can replace t1 with a directly.
 *
 */
class CopyPropPass : public QuadPass {
public:
  bool run(Function &fn) override;
};

/**
 * @class CSEPass
 * @brief Common Subexpression Elimination in function scope
 * Eliminates redundant computations of the same expression.
 * Example:
 *    t0 = a + b
 *    t1 = a + b  // Same expression, can be replaced with t1 = t0
 */
class CSEPass : public QuadPass {
public:
  explicit CSEPass(DominatorTree &dt) : dt(dt) {}
  bool run(Function &fn) override;

private:
  DominatorTree &dt;
  struct ExpressionHash {
    size_t operator()(
        const std::pair<OpCode, std::pair<Operand, Operand>> &expr) const;
  };

  struct ExpressionEqual {
    bool
    operator()(const std::pair<OpCode, std::pair<Operand, Operand>> &lhs,
               const std::pair<OpCode, std::pair<Operand, Operand>> &rhs) const;
  };
};
void runDefaultQuadOptimizations(Function &fn);
