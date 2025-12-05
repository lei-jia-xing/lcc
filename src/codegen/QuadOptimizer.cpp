#include "codegen/QuadOptimizer.hpp"
#include "codegen/Instruction.hpp"
#include <unordered_map>

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
  for (auto &blk : fn.getBlocks()) {
    // 统计使用次数（仅在本基本块内）
    std::unordered_map<int, int> useCount;
    for (auto &inst : blk->getInstructions()) {
      auto addUse = [&useCount](const Operand &op) {
        if (op.getType() == OperandType::Temporary)
          useCount[op.asInt()]++;
      };
      addUse(inst.getArg1());
      addUse(inst.getArg2());

      // RETURN: result field is a "use" (the return value)
      // Format: RETURN -, -, res(var|temp|const)
      if (inst.getOp() == OpCode::RETURN) {
        addUse(inst.getResult());
      }
    }

    auto &insts = blk->getInstructions();
    for (int i = (int)insts.size() - 1; i >= 0; --i) {
      auto &inst = insts[i];
      if (!hasSideEffect(inst.getOp()) &&
          inst.getResult().getType() == OperandType::Temporary) {
        int t = inst.getResult().asInt();
        if (useCount[t] == 0) {
          insts.erase(insts.begin() + i);
          changed = true;
          continue;
        }
      }
    }
  }
  return changed;
}

// Local constant propagation: propagate constants within a basic block
bool ConstPropPass::run(Function &fn) {
  bool changed = false;
  for (auto &blk : fn.getBlocks()) {
    // Map from temporary id to constant value
    std::unordered_map<int, int> constMap;

    for (auto &inst : blk->getInstructions()) {
      auto op = inst.getOp();

      // Helper to replace operand if it's a known constant
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

      // Replace uses in arg1 and arg2
      Operand arg1 = inst.getArg1();
      Operand arg2 = inst.getArg2();
      tryReplace(arg1);
      tryReplace(arg2);
      if (arg1.getType() != inst.getArg1().getType() ||
          (arg1.getType() == OperandType::ConstantInt &&
           arg1.asInt() != inst.getArg1().asInt())) {
        inst.setArg1(arg1);
      }
      if (arg2.getType() != inst.getArg2().getType() ||
          (arg2.getType() == OperandType::ConstantInt &&
           arg2.asInt() != inst.getArg2().asInt())) {
        inst.setArg2(arg2);
      }

      // Track constant assignments: ASSIGN const -> temp
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
      // For other instructions that define a temp, invalidate it
      else if (inst.getResult().getType() == OperandType::Temporary) {
        constMap.erase(inst.getResult().asInt());
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

      bool isArg1Const =
          arg1.getType() == OperandType::ConstantInt;
      bool isArg2Const =
          arg2.getType() == OperandType::ConstantInt;
      int c1 = isArg1Const ? arg1.asInt() : 0;
      int c2 = isArg2Const ? arg2.asInt() : 0;

      // ADD optimizations
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
      // SUB optimizations
      else if (op == OpCode::SUB) {
        // x - 0 -> x
        if (isArg2Const && c2 == 0) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg2(Operand());
          changed = true;
        }
      }
      // MUL optimizations
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
      // DIV optimizations
      else if (op == OpCode::DIV) {
        // x / 1 -> x
        if (isArg2Const && c2 == 1) {
          inst.setOp(OpCode::ASSIGN);
          inst.setArg2(Operand());
          changed = true;
        }
      }
      // MOD optimizations
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

void runDefaultQuadOptimizations(Function &fn) {
  PassManager pm;
  pm.add(std::make_unique<ConstPropPass>());
  pm.add(std::make_unique<AlgebraicSimplifyPass>());
  // pm.add(std::make_unique<LocalDCEPass>());
  pm.run(fn);
}
