// optimize/Inliner.hpp
#pragma once
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include <unordered_map>

class InlinerPass {
public:
  void run(const std::vector<std::shared_ptr<Function>> &funcs);

private:
  /**
   * @brief count the instructions in a function
   *
   * @param func function to count
   * @return count of instructions
   */
  int countInstructions(Function *func);

  /**
   * @brief helper to check if inlining would create recursion
   *
   * @param caller calling function
   * @param callee called function
   * @return whether inlining would create recursion
   */
  bool isRecursive(Function *caller, Function *callee);

  bool inlineFunction(Function *caller, BasicBlock *callBlock,
                      Instruction *callInst, Function *callee);

  std::unique_ptr<Instruction>
  copyInstruction(Instruction *inst, std::unordered_map<int, int> &tempMap,
                  std::unordered_map<int, int> &labelMap,
                  std::unordered_map<int, int> &varMap, Function *caller);

  int getNewTemp(int oldTemp, std::unordered_map<int, int> &tempMap,
                 Function *caller);
  int getNewLabel(int oldLabel, std::unordered_map<int, int> &labelMap,
                  Function *caller);

  Operand mapOperand(Operand op, std::unordered_map<int, int> &tempMap,
                     std::unordered_map<int, int> &labelMap,
                     std::unordered_map<int, int> &varMap, Function *caller);

  const int INLINE_THRESHOLD = 100000;
};
