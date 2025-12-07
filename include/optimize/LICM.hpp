#pragma once

#include "LoopAnalysis.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include <map>
#include <set>
#include <vector>

class LoopAnalysis;
class DominatorTree;

class LICMPass {
public:
  /**
   * @brief runs the loop-invariant code motion optimization on the given
   * function.
   *
   * @param F fucntion to optimize
   * @param DT Dominator tree of the function
   * @param loops loop information of the function
   */
  void run(Function &F, DominatorTree &DT, std::vector<LoopInfo> &loops);

private:
  /**
   * @brief helper function to determine if an operand is loop-invariant.
   *
   * @param op Operand
   * @param loopBlocks set of blocks in the loop
   * @param operandDefs
   * @param invariantInstructions instructions already identified
   * as invariant
   * @return isOperandLoopInvariant?
   */
  bool isOperandLoopInvariant(
      const Operand &op, const std::set<BasicBlock *> &loopBlocks,
      const std::map<Operand *, BasicBlock *> &operandDefs,
      const std::set<const Instruction *> &invariantInstructions);

  /**
   * @brief helper function to determine if an instruction is loop-invariant.
   *
   * @param inst instruction to check
   * @param loopBlocks set of blocks in the loop
   * @param operandDefs operand definitions map
   * @param invariantInstructions
   * @return isInstructionLoopInvariant?
   */
  bool isInstructionLoopInvariant(
      const Instruction &inst, const std::set<BasicBlock *> &loopBlocks,
      const std::map<Operand *, BasicBlock *> &operandDefs,
      const std::set<const Instruction *> &invariantInstructions);

  /**
   * @brief helper to check if an instruction is safely moved out of the loop.
   *
   * @param inst instruction to check
   * @param currentBlock current block
   * @param preheader
   * @param DT DominatorTree
   * @param loopBlocks set of blocks in the loop
   * @param operandDefs Operand definitions map
   * @return isSafeToMove?
   */
  bool isSafeToMove(const Instruction &inst, BasicBlock *currentBlock,
                    BasicBlock *preheader, const DominatorTree &DT,
                    const std::set<BasicBlock *> &loopBlocks,
                    const std::map<Operand *, BasicBlock *> &operandDefs);

  /**
   * @brief maps temp id to the basic block where it is defined.
   */
  std::map<int, BasicBlock *> _tempDefinitions;
  /**
   * @brief maps Symbol to the basic block where it is defined.
   */
  std::map<std::shared_ptr<Symbol>, BasicBlock *> _varDefinitions;

  /**
   * @brief instructions identified as loop-invariant
   */
  std::set<const Instruction *> _identifiedInvariantInstructions;
};
