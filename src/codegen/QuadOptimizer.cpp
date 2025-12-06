#include "codegen/QuadOptimizer.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include <set>
#include <unordered_map>
#include <unordered_set>

static bool hasSideEffect(OpCode op) {
  switch (op) {
  case OpCode::STORE:
  case OpCode::GOTO:
  case OpCode::IF:
  case OpCode::LABEL:
  case OpCode::CALL:
  case OpCode::RETURN:
  case OpCode::PARAM:
  case OpCode::ALLOCA:
  case OpCode::ARG:
    return true;
  default:
    return false;
  }
}

bool LocalDCEPass::run(Function &fn) {
  bool changed = false;

  std::unordered_map<int, int> useCount;

  for (auto &blk : fn.getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      auto addUse = [&useCount](const Operand &op) {
        if (op.getType() == OperandType::Temporary)
          useCount[op.asInt()]++;
      };

      addUse(inst.getArg1());
      addUse(inst.getArg2());

      OpCode op = inst.getOp();
      switch (op) {
      case OpCode::RETURN:
        // RETURN -, -, res(var|temp|const)
        addUse(inst.getResult());
        break;
      case OpCode::STORE:
        // STORE value(arg1), base(arg2), index(result)
        addUse(inst.getResult());
        break;
      case OpCode::ALLOCA:
        // ALLOCA var(arg1), -, size(result)
        addUse(inst.getResult());
        break;
      default:
        break;
      }
    }
  }

  for (auto &blk : fn.getBlocks()) {
    auto &insts = blk->getInstructions();
    for (int i = (int)insts.size() - 1; i >= 0; --i) {
      auto &inst = insts[i];
      if (!hasSideEffect(inst.getOp()) &&
          inst.getResult().getType() == OperandType::Temporary) {
        int t = inst.getResult().asInt();
        if (useCount[t] == 0) {
          insts.erase(insts.begin() + i);
          changed = true;
        }
      }
    }
  }
  return changed;
}

bool ConstPropPass::run(Function &fn) {
  bool changed = false;
  for (auto &blk : fn.getBlocks()) {
    // Map from temporary id to constant value
    std::unordered_map<int, int> constMap;

    for (auto &inst : blk->getInstructions()) {
      auto op = inst.getOp();

      // Helper to replace temp into constant if it's a known constant
      auto tryReplace = [&](Operand &operand) {
        if (operand.getType() == OperandType::Temporary) {
          int tid = operand.asInt();
          auto it = constMap.find(tid);
          if (it != constMap.end()) {
            operand = Operand::ConstantInt(it->second);
            changed = true;
          }
        }
      };

      Operand arg1 = inst.getArg1();
      Operand arg2 = inst.getArg2();
      tryReplace(arg1);
      tryReplace(arg2);
      if (arg1.getType() == OperandType::ConstantInt) {
        inst.setArg1(arg1);
      }
      if (arg2.getType() == OperandType::ConstantInt) {
        inst.setArg2(arg2);
      }

      // For instructions where result is a "use", also try to replace
      // RETURN: result is the return value
      // STORE: result is the index
      // ALLOCA: result is the size
      if (op == OpCode::RETURN || op == OpCode::STORE || op == OpCode::ALLOCA) {
        Operand res = inst.getResult();
        tryReplace(res);
        if (res.getType() == OperandType::ConstantInt) {
          inst.setResult(res);
        }
      }

      // Track constant assignments: ASSIGN const -> temp
      // then the temp will be evaluated as const as rvalue
      if (op == OpCode::ASSIGN) {
        const Operand &src = inst.getArg1();
        const Operand &dst = inst.getResult();
        if (dst.getType() == OperandType::Temporary) {
          int tid = dst.asInt();
          if (src.getType() == OperandType::ConstantInt) {
            // Record this temp as having a constant value
            constMap[tid] = src.asInt();
          } else {
            // temp is assigned a non-constant, invalidate
            constMap.erase(tid);
          }
        }
      }
    }
  }
  return changed;
}

bool AlgebraicSimplifyPass::run(Function &fn) {
  bool changed = false;

  for (auto &blk : fn.getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      OpCode op = inst.getOp();
      const Operand &arg1 = inst.getArg1();
      const Operand &arg2 = inst.getArg2();
      const Operand &result = inst.getResult();

      bool isArg1Const = arg1.getType() == OperandType::ConstantInt;
      bool isArg2Const = arg2.getType() == OperandType::ConstantInt;
      int c1 = isArg1Const ? arg1.asInt() : 0;
      int c2 = isArg2Const ? arg2.asInt() : 0;

      if (op == OpCode::ADD) {
        // x + 0 -> x
        if (isArg2Const && c2 == 0) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg2(Operand());
          changed = true;
        }
        // 0 + x -> x
        else if (isArg1Const && c1 == 0) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg1(arg2);
          inst.setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::SUB) {
        // x - 0 -> x
        if (isArg2Const && c2 == 0) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::MUL) {
        // x * 0 -> 0, 0 * x -> 0
        if ((isArg1Const && c1 == 0) || (isArg2Const && c2 == 0)) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg1(Operand::ConstantInt(0));
          inst.setArg2(Operand());
          changed = true;
        }
        // x * 1 -> x
        else if (isArg2Const && c2 == 1) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg2(Operand());
          changed = true;
        }
        // 1 * x -> x
        else if (isArg1Const && c1 == 1) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg1(arg2);
          inst.setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::DIV) {
        // x / 1 -> x
        if (isArg2Const && c2 == 1) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::MOD) {
        // x % 1 -> 0
        if (isArg2Const && c2 == 1) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg1(Operand::ConstantInt(0));
          inst.setArg2(Operand());
          changed = true;
        }
      }
    }
  }

  return changed;
}

bool CopyPropPass::run(Function &fn) {
  bool changed = false;

  for (auto &blk : fn.getBlocks()) {
    // Map from temp id to its copy source (another temp or variable)
    // copyMap[t1] = t0 means t1 is a copy of t0
    std::unordered_map<int, Operand> copyMap;

    for (auto &inst : blk->getInstructions()) {
      OpCode op = inst.getOp();

      // Helper to replace operand if it's a known copy
      auto tryReplace = [&](Operand &operand) -> bool {
        if (operand.getType() == OperandType::Temporary) {
          int tid = operand.asInt();
          auto it = copyMap.find(tid);
          if (it != copyMap.end()) {
            operand = it->second;
            return true;
          }
        }
        return false;
      };

      Operand arg1 = inst.getArg1();
      Operand arg2 = inst.getArg2();
      if (tryReplace(arg1)) {
        inst.setArg1(arg1);
        changed = true;
      }
      if (tryReplace(arg2)) {
        inst.setArg2(arg2);
        changed = true;
      }

      // For instructions where result is a "use" (not a definition), also replace
      // RETURN: result is the return value
      // STORE: result is the index
      // ALLOCA: result is the size (but usually constant, safe to try)
      if (op == OpCode::RETURN || op == OpCode::STORE || op == OpCode::ALLOCA) {
        Operand res = inst.getResult();
        if (tryReplace(res)) {
          inst.setResult(res);
          changed = true;
        }
      }

      // Track copy assignments: ASSIGN src -> temp
      // where src is a temp or variable (not const, that's for ConstProp)
      if (op == OpCode::ASSIGN) {
        const Operand &src = inst.getArg1();
        const Operand &dst = inst.getResult();

        if (dst.getType() == OperandType::Temporary) {
          int dstId = dst.asInt();

          if (src.getType() == OperandType::Temporary ||
              src.getType() == OperandType::Variable) {
            // This is a copy: dst = src
            // Check if src itself is a copy, follow the chain
            Operand realSrc = src;
            if (src.getType() == OperandType::Temporary) {
              auto it = copyMap.find(src.asInt());
              if (it != copyMap.end()) {
                realSrc = it->second;
              }
            }
            copyMap[dstId] = realSrc;
          } else {
            // dst is assigned a non-copy value, invalidate
            copyMap.erase(dstId);
          }
        }
      }
      // For other instructions that define a temp, invalidate it
      else if (inst.getResult().getType() == OperandType::Temporary) {
        copyMap.erase(inst.getResult().asInt());
      }

      // If a variable is redefined (STORE), invalidate any temp that copies it
      // This is a conservative approach
      if (op == OpCode::STORE) {
        const Operand &base = inst.getArg2();
        if (base.getType() == OperandType::Variable) {
          // Remove all copies that reference this variable
          for (auto it = copyMap.begin(); it != copyMap.end();) {
            if (it->second.getType() == OperandType::Variable &&
                it->second.asSymbol() == base.asSymbol()) {
              it = copyMap.erase(it);
            } else {
              ++it;
            }
          }
        }
      }
    }
  }

  return changed;
}

// Helper to check if an operand is loop-invariant
static bool
isOperandInvariant(const Operand &op,
                   const std::unordered_set<int> &loopDefinedTemps,
                   const std::set<std::shared_ptr<Symbol>> &loopModifiedVars) {
  if (op.getType() == OperandType::ConstantInt ||
      op.getType() == OperandType::Label ||
      op.getType() == OperandType::Empty) {
    return true; // Constants, labels and empty are always invariant
  }
  if (op.getType() == OperandType::Variable) {
    // Variable is invariant only if not modified in loop
    return loopModifiedVars.find(op.asSymbol()) == loopModifiedVars.end();
  }
  if (op.getType() == OperandType::Temporary) {
    // Invariant if not defined in loop
    return loopDefinedTemps.find(op.asInt()) == loopDefinedTemps.end();
  }
  return false;
}

void runDefaultQuadOptimizations(Function &fn) {
  PassManager pm;
  pm.add(std::make_unique<ConstPropPass>());
  pm.add(std::make_unique<CopyPropPass>());
  pm.add(std::make_unique<AlgebraicSimplifyPass>());
  pm.add(std::make_unique<LocalDCEPass>());
  pm.run(fn);
}
