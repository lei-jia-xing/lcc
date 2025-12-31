#pragma once

#include "codegen/Function.hpp"
#include "codegen/QuadOptimizer.hpp"

class GlobalConstEvalPass : public QuadPass {
public:
  static constexpr int MAX_RECURSION_DEPTH = 50;
  static constexpr int MAX_INSTRUCTIONS = 100000;

  explicit GlobalConstEvalPass(
      const std::vector<std::shared_ptr<Function>> &funcs)
      : functions(funcs) {}

  bool run(Function &fn) override;

private:
  /**
   * @brief the reference of global function list
   */
  const std::vector<std::shared_ptr<Function>> &functions;

  /**
   * @brief memory cache for evaluated functions
   */
  std::map<std::pair<std::string, std::vector<int>>, int> evalCache;

  /**
   * @brief find function by name
   *
   * @param name function identify
   * @return function pointer
   */
  Function *findFunction(const std::string &name);

  /**
   * @brief interpreter
   *
   * @param fn object function pointer
   * @param args input arguments
   * @param depth {success ?, result}
   */
  std::pair<bool, int> evaluate(Function *fn, const std::vector<int> &args,
                                int depth);
};
