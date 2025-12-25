#include "codegen/QuadOptimizer.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "optimize/DominatorTree.hpp"
#include "optimize/LICM.hpp"
#include "optimize/LoopAnalysis.hpp"
#include "optimize/Mem2Reg.hpp"
#include "optimize/PhiElimination.hpp"
#include <functional>
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

  auto addUse = [&useCount](const Operand &op) {
    if (op.getType() == OperandType::Temporary)
      useCount[op.asInt()]++;
  };
  for (auto &blk : fn.getBlocks()) {
    for (auto &inst : blk->getInstructions()) {

      OpCode op = inst->getOp();
      if (op == OpCode::PHI) {
        for (auto &pair : inst->getPhiArgs()) {
          addUse(pair.first);
        }
      } else {
        addUse(inst->getArg1());
        addUse(inst->getArg2());
      }
      if (op == OpCode::RETURN || op == OpCode::STORE || op == OpCode::ALLOCA) {
        addUse(inst->getResult());
      }
    }
  }
  // delete instructions whose result is temp with zero use count
  for (auto &blk : fn.getBlocks()) {
    auto &insts = blk->getInstructions();
    for (auto it = insts.begin(); it != insts.end();) {
      auto &inst = *it;
      if (!hasSideEffect(inst->getOp()) &&
          inst->getResult().getType() == OperandType::Temporary) {
        int t = inst->getResult().asInt();
        if (useCount[t] == 0) {
          it = insts.erase(it);
          changed = true;
          continue;
        }
      }
      ++it;
    }
  }
  return changed;
}

bool ConstPropPass::run(Function &fn) {
  bool changed = false;
  std::unordered_map<int, int> constMap;
  auto tryReplace = [&](Operand &operand) -> bool {
    if (operand.getType() == OperandType::Temporary) {
      auto it = constMap.find(operand.asInt());
      if (it != constMap.end()) {
        operand = Operand::ConstantInt(it->second);
        return true;
      }
    }
    return false;
  };

  for (auto &blk : fn.getBlocks()) {

    for (auto &inst : blk->getInstructions()) {
      auto op = inst->getOp();

      if (op == OpCode::PHI) {
        for (auto &pair : inst->getPhiArgs()) {
          if (tryReplace(pair.first)) {
            changed = true;
          }
        }
      } else {
        Operand arg1 = inst->getArg1();
        if (tryReplace(arg1)) {
          inst->setArg1(arg1);
          changed = true;
        }
        Operand arg2 = inst->getArg2();
        if (tryReplace(arg2)) {
          inst->setArg2(arg2);
          changed = true;
        }

        // For instructions where result is a "use", also try to replace
        // RETURN: result is the return value
        // STORE: result is the index
        // ALLOCA: result is the size
        if (op == OpCode::RETURN || op == OpCode::STORE ||
            op == OpCode::ALLOCA) {
          Operand res = inst->getResult();
          if (tryReplace(res)) {
            inst->setResult(res);
            changed = true;
          }
        }
      }

      // Track constant assignments: ASSIGN const -> temp
      // then the temp will be evaluated as const as rvalue
      if (op == OpCode::ASSIGN) {
        const Operand &src = inst->getArg1();
        const Operand &dst = inst->getResult();
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
      OpCode op = inst->getOp();
      const Operand &arg1 = inst->getArg1();
      const Operand &arg2 = inst->getArg2();
      const Operand &result = inst->getResult();

      bool isArg1Const = arg1.getType() == OperandType::ConstantInt;
      bool isArg2Const = arg2.getType() == OperandType::ConstantInt;
      int c1 = isArg1Const ? arg1.asInt() : 0;
      int c2 = isArg2Const ? arg2.asInt() : 0;

      if (op == OpCode::ADD) {
        // x + 0 -> x
        if (isArg2Const && c2 == 0) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg2(Operand());
          changed = true;
        }
        // 0 + x -> x
        else if (isArg1Const && c1 == 0) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg1(arg2);
          inst->setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::SUB) {
        // x - 0 -> x
        if (isArg2Const && c2 == 0) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::MUL) {
        // x * 0 -> 0, 0 * x -> 0
        if ((isArg1Const && c1 == 0) || (isArg2Const && c2 == 0)) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg1(Operand::ConstantInt(0));
          inst->setArg2(Operand());
          changed = true;
        }
        // x * 1 -> x
        else if (isArg2Const && c2 == 1) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg2(Operand());
          changed = true;
        }
        // 1 * x -> x
        else if (isArg1Const && c1 == 1) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg1(arg2);
          inst->setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::DIV) {
        // x / 1 -> x
        if (isArg2Const && c2 == 1) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg2(Operand());
          changed = true;
        }
      }

      else if (op == OpCode::MOD) {
        // x % 1 -> 0
        if (isArg2Const && c2 == 1) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg1(Operand::ConstantInt(0));
          inst->setArg2(Operand());
          changed = true;
        }
      }
    }
  }

  return changed;
}

bool CopyPropPass::run(Function &fn) {
  bool changed = false;
  std::unordered_map<int, Operand> copyMap;
  auto resolve = [&](const Operand &op) -> Operand {
    if (op.getType() == OperandType::Temporary) {
      int tid = op.asInt();
      auto it = copyMap.find(tid);
      if (it != copyMap.end()) {
        return it->second;
      }
    }
    return op;
  };
  auto isSame = [](const Operand &a, const Operand &b) -> bool {
    if (a.getType() != b.getType()) {
      return false;
    }
    switch (a.getType()) {
    case OperandType::ConstantInt:
      return a.asInt() == b.asInt();
    case OperandType::Temporary:
      return a.asInt() == b.asInt();
    case OperandType::Variable:
      return a.asSymbol() == b.asSymbol();
    default:
      return false;
    }
  };

  for (auto &blk : fn.getBlocks()) {
    // Map from temp id to its copy source (another temp or variable)
    // copyMap[t1] = t0 means t1 is a copy of t0

    for (auto &inst : blk->getInstructions()) {
      OpCode op = inst->getOp();
      if (op == OpCode::PHI) {
        for (auto &pair : inst->getPhiArgs()) {
          Operand resolved = resolve(pair.first);
          if (!isSame(resolved, pair.first)) {
            pair.first = resolved;
            changed = true;
          }
        }
      } else {
        Operand arg1 = inst->getArg1();
        Operand r1 = resolve(arg1);
        if (!isSame(r1, arg1)) {
          inst->setArg1(r1);
          changed = true;
        }
        Operand arg2 = inst->getArg2();
        Operand r2 = resolve(arg2);
        if (!isSame(r2, arg2)) {
          inst->setArg2(r2);
          changed = true;
        }

        if (op == OpCode::RETURN || op == OpCode::STORE ||
            op == OpCode::ALLOCA) {
          Operand res = inst->getResult();
          Operand rRes = resolve(res);
          if (!isSame(rRes, res)) {
            inst->setResult(rRes);
            changed = true;
          }
        }
      }

      // Track copy assignments: ASSIGN src -> temp
      // where src is a temp or variable (not const, that's for ConstProp)
      if (op == OpCode::ASSIGN) {
        const Operand &src = inst->getArg1();
        const Operand &dst = inst->getResult();

        if (dst.getType() == OperandType::Temporary) {
          if (src.getType() == OperandType::Temporary ||
              src.getType() == OperandType::Variable) {
            copyMap[dst.asInt()] = resolve(src);
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

// CSE Pass implementation
size_t CSEPass::ExpressionHash::operator()(
    const std::pair<OpCode, std::pair<Operand, Operand>> &expr) const {
  size_t h1 = std::hash<int>{}(static_cast<int>(expr.first));

  auto hashOperand = [](const Operand &op) {
    switch (op.getType()) {
    case OperandType::ConstantInt:
      return std::hash<int>{}(op.asInt());
    case OperandType::Temporary:
      return std::hash<int>{}(op.asInt() +
                              1000000); // Offset to distinguish from constants
    case OperandType::Variable:
      return std::hash<void *>{}(op.asSymbol().get());
    default:
      return std::hash<int>{}(0);
    }
  };

  size_t h2 = hashOperand(expr.second.first);
  size_t h3 = hashOperand(expr.second.second);

  return h1 ^ (h2 << 1) ^ (h3 << 2);
}

bool CSEPass::ExpressionEqual::operator()(
    const std::pair<OpCode, std::pair<Operand, Operand>> &lhs,
    const std::pair<OpCode, std::pair<Operand, Operand>> &rhs) const {
  if (lhs.first != rhs.first) {
    return false;
  }

  auto operandsEqual = [](const Operand &op1, const Operand &op2) {
    if (op1.getType() != op2.getType()) {
      return false;
    }
    switch (op1.getType()) {
    case OperandType::ConstantInt:
    case OperandType::Temporary:
      return op1.asInt() == op2.asInt();
    case OperandType::Variable:
      return op1.asSymbol() == op2.asSymbol();
    default:
      return true;
    }
  };

  return operandsEqual(lhs.second.first, rhs.second.first) &&
         operandsEqual(lhs.second.second, rhs.second.second);
}

bool CSEPass::run(Function &fn) {
  bool changed = false;

  // Expression to available temporary mapping
  // We use unordered_map with custom hash and equal functions for efficiency
  using Expression = std::pair<OpCode, std::pair<Operand, Operand>>;
  using ExpressionMap =
      std::unordered_map<Expression, int, ExpressionHash, ExpressionEqual>;

  for (auto &blk : fn.getBlocks()) {
    ExpressionMap availableExpressions;
    auto &instructions = blk->getInstructions();

    // First pass: collect available expressions and identify redundant ones
    std::vector<std::pair<size_t, int>>
        replacements; // (instruction_index, temp_to_use)

    for (size_t i = 0; i < instructions.size(); i++) {
      auto &inst = instructions[i];
      if (!inst)
        continue;

      OpCode op = inst->getOp();

      // Check if this is a computable expression
      if (op == OpCode::ADD || op == OpCode::SUB || op == OpCode::MUL ||
          op == OpCode::DIV || op == OpCode::MOD || op == OpCode::AND ||
          op == OpCode::OR || op == OpCode::LT || op == OpCode::LE ||
          op == OpCode::GT || op == OpCode::GE || op == OpCode::EQ ||
          op == OpCode::NEQ) {

        Expression expr = {op, {inst->getArg1(), inst->getArg2()}};

        auto it = availableExpressions.find(expr);
        if (it != availableExpressions.end()) {
          // Found a common subexpression - record replacement
          replacements.push_back({i, it->second});
          changed = true;
        } else {
          // New expression - make it available for future use
          const Operand &result = inst->getResult();
          if (result.getType() == OperandType::Temporary) {
            availableExpressions[expr] = result.asInt();
          }
        }
      }

      // Invalidate expressions that use the result of this instruction
      const Operand &result = inst->getResult();
      if (result.getType() == OperandType::Temporary ||
          result.getType() == OperandType::Variable) {
        // Remove expressions that use this operand
        for (auto it = availableExpressions.begin();
             it != availableExpressions.end();) {
          bool usesResult = false;

          auto operandUses = [&](const Operand &op) {
            if (op.getType() == OperandType::Temporary &&
                result.getType() == OperandType::Temporary) {
              return op.asInt() == result.asInt();
            }
            if (op.getType() == OperandType::Variable &&
                result.getType() == OperandType::Variable) {
              return op.asSymbol() == result.asSymbol();
            }
            return false;
          };

          Expression expr = it->first;
          if (operandUses(expr.second.first) ||
              operandUses(expr.second.second)) {
            usesResult = true;
          }

          if (usesResult) {
            it = availableExpressions.erase(it);
          } else {
            ++it;
          }
        }
      }
    }

    // Second pass: apply replacements (in reverse order to maintain indices)
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
      size_t idx = it->first;
      int replacementTemp = it->second;

      if (idx < instructions.size()) {
        auto &inst = instructions[idx];
        if (inst) {
          // Replace the computation with a simple assignment
          Operand result = inst->getResult();
          inst = std::make_unique<Instruction>(
              OpCode::ASSIGN, Operand::Temporary(replacementTemp), Operand(),
              result);
        }
      }
    }
  }

  return changed;
}

void runDefaultQuadOptimizations(Function &fn) {
  // build SSA
  DominatorTree dt;
  dt.run(fn);

  Mem2RegPass mem2reg;
  mem2reg.run(fn, dt);
  // LoopAnalysis la;
  // la.run(fn, dt);
  // if (!la.getLoops().empty()) {
  //   LICMPass licm;
  //   std::vector<LoopInfo> loops = la.getLoops();
  //   licm.run(fn, dt, loops);
  // }

  // Run local optimizations
  // PassManager pm;
  // pm.add(std::make_unique<ConstPropPass>());
  // pm.add(std::make_unique<CopyPropPass>());
  // pm.add(std::make_unique<AlgebraicSimplifyPass>());
  // pm.add(std::make_unique<CSEPass>()); // Add CSE pass
  // pm.add(std::make_unique<LocalDCEPass>());
  // pm.run(fn);

  PhiEliminationPass phiElim;
  phiElim.run(fn);
}
