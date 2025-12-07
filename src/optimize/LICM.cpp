#include "optimize/LICM.hpp"
#include "codegen/Instruction.hpp"
#include <map>
#include <set>
#include <vector>

class DominatorTree;
class LoopAnalysis;

static std::vector<BasicBlock *> getPredecessors(BasicBlock *BB, Function &F) {
  std::vector<BasicBlock *> predecessors;
  for (const auto &b_ptr : F.getBlocks()) {
    if (b_ptr->next.get() == BB || b_ptr->jumpTarget.get() == BB) {
      predecessors.push_back(b_ptr.get());
    }
  }
  return predecessors;
}

void LICMPass::run(Function &F, DominatorTree &DT,
                   std::vector<LoopInfo> &loops) {
  if (loops.empty()) {
    return;
  }

  std::map<int, Instruction *> tempDefinitions;
  std::map<std::shared_ptr<Symbol>, Instruction *> varDefinitions;

  for (auto &bb_ptr : F.getBlocks()) {
    for (auto &inst_ptr : bb_ptr->getInstructions()) {
      const Operand &resultOp = inst_ptr->getResult();
      if (resultOp.getType() == OperandType::Temporary) {
        tempDefinitions[resultOp.asInt()] = inst_ptr.get();
      } else if (resultOp.getType() == OperandType::Variable) {
        varDefinitions[resultOp.asSymbol()] = inst_ptr.get();
      }
    }
  }

  for (LoopInfo &loop : loops) {
    // Find the block that contains GOTO to the header (from outside the loop)
    // This is where we'll insert the hoisted instructions
    BasicBlock *insertBlock = nullptr;
    for (auto &b_ptr : F.getBlocks()) {
      if (loop.blocks.count(b_ptr.get()) == 0) {
        // This block is outside the loop
        // Check if it jumps to the header
        if (b_ptr->jumpTarget.get() == loop.header ||
            b_ptr->next.get() == loop.header) {
          insertBlock = b_ptr.get();
          break;
        }
      }
    }

    if (!insertBlock) {
      continue;
    }

    std::set<const Instruction *> invariantInstructions;
    bool changed = true;
    while (changed) {
      changed = false;
      for (BasicBlock *bb : loop.blocks) {
        for (auto &inst_ptr : bb->getInstructions()) {
          const Instruction *inst = inst_ptr.get();
          if (invariantInstructions.count(inst)) {
            continue;
          }

          bool is_invariant = true;
          const auto check_operand = [&](const Operand &op) {
            if (op.getType() == OperandType::ConstantInt)
              return true;
            if (op.getType() == OperandType::Temporary) {
              auto it = tempDefinitions.find(op.asInt());
              if (it != tempDefinitions.end()) {
                Instruction *def_inst = it->second;
                return loop.blocks.find(F.findBlockOf(def_inst)) ==
                           loop.blocks.end() ||
                       invariantInstructions.count(def_inst);
              }
            } else if (op.getType() == OperandType::Variable) {
              auto it = varDefinitions.find(op.asSymbol());
              if (it != varDefinitions.end()) {
                Instruction *def_inst = it->second;
                return loop.blocks.find(F.findBlockOf(def_inst)) ==
                           loop.blocks.end() ||
                       invariantInstructions.count(def_inst);
              }
            }
            return true;
          };

          switch (inst->getOp()) {
          case OpCode::ASSIGN: {
            const Operand &src_op = inst->getArg1();
            if (!check_operand(src_op))
              is_invariant = false;
            break;
          }
          // Instructions with side-effects or that are unsafe to move
          case OpCode::STORE:
          case OpCode::CALL:
          case OpCode::IF:
          case OpCode::GOTO:
          case OpCode::RETURN:
          case OpCode::ALLOCA:
          case OpCode::PARAM:
          case OpCode::LABEL:
          case OpCode::LOAD: // LOAD can be tricky due to aliasing. For now,
                             // keep it conservative.
            is_invariant = false;
            break;
          default: // Pure arithmetic/logical operations
            if (!check_operand(inst->getArg1()))
              is_invariant = false;
            if (is_invariant && inst->getOp() != OpCode::NEG &&
                inst->getOp() != OpCode::NOT) {
              if (!check_operand(inst->getArg2()))
                is_invariant = false;
            }
            break;
          }

          if (is_invariant) {
            invariantInstructions.insert(inst);
            changed = true;
          }
        }
      }
    }

    if (invariantInstructions.empty()) {
      continue;
    }

    std::vector<std::unique_ptr<Instruction>> instructionsToMove;

    // First, collect instructions to move (without modifying the blocks yet)
    std::set<Instruction *> toRemove;
    for (BasicBlock *bb : loop.blocks) {
      for (auto &inst_ptr : bb->getInstructions()) {
        if (!inst_ptr)
          continue;
        if (invariantInstructions.count(inst_ptr.get())) {
          bool is_redefined = false;
          const Operand &result = inst_ptr->getResult();
          for (BasicBlock *inner_bb : loop.blocks) {
            for (auto &other_inst_ptr : inner_bb->getInstructions()) {
              if (!other_inst_ptr)
                continue;
              if (inst_ptr.get() == other_inst_ptr.get())
                continue;
              if (other_inst_ptr->getResult() == result) {
                is_redefined = true;
                break;
              }
            }
            if (is_redefined)
              break;
          }

          if (!is_redefined) {
            toRemove.insert(inst_ptr.get());
          }
        }
      }
    }

    // Now actually move the instructions
    for (BasicBlock *bb : loop.blocks) {
      auto &instructions = bb->getInstructions();
      for (auto it = instructions.begin(); it != instructions.end();) {
        if (*it && toRemove.count(it->get())) {
          instructionsToMove.push_back(std::move(*it));
          it = instructions.erase(it);
        } else {
          ++it;
        }
      }
    }

    // Insert hoisted instructions into insertBlock, before the last instruction
    // (GOTO)
    auto &insertInsts = insertBlock->getInstructions();
    // Find position to insert: before the last instruction if it's a GOTO/IF
    size_t insertPos = insertInsts.size();
    if (!insertInsts.empty()) {
      auto &lastInst = insertInsts.back();
      if (lastInst && (lastInst->getOp() == OpCode::GOTO ||
                       lastInst->getOp() == OpCode::IF)) {
        insertPos = insertInsts.size() - 1;
      }
    }

    for (auto &inst_to_move : instructionsToMove) {
      if (inst_to_move) {
        insertInsts.insert(insertInsts.begin() + insertPos,
                           std::move(inst_to_move));
        insertPos++;
      }
    }
  }
}
