#include "codegen/QuadOptimizer.hpp"
#include "codegen/Instruction.hpp"
#include <unordered_map>
#include <optional>
#include <algorithm>

using namespace lcc::codegen;

static bool isConst(const Operand &op) {
  return op.getType() == OperandType::ConstantInt;
}

static std::optional<int> getConst(const Operand &op) {
  if (!isConst(op)) return std::nullopt;
  return op.asInt();
}

static bool hasSideEffect(OpCode op) {
  switch (op) {
  case OpCode::STORE:
  case OpCode::GOTO:
  case OpCode::IF:
  case OpCode::LABEL:
  case OpCode::CALL:
  case OpCode::RETURN:
  case OpCode::PRINTF:
  case OpCode::PARAM:
  case OpCode::DEF:
    return true;
  default:
    return false;
  }
}

static std::optional<int> evalBinary(OpCode op, int a, int b) {
  switch (op) {
  case OpCode::ADD: return a + b;
  case OpCode::SUB: return a - b;
  case OpCode::MUL: return a * b;
  case OpCode::DIV: if (b != 0) return a / b; else return std::nullopt;
  case OpCode::MOD: if (b != 0) return a % b; else return std::nullopt;
  case OpCode::EQ: return a == b;
  case OpCode::NEQ: return a != b;
  case OpCode::LT: return a < b;
  case OpCode::LE: return a <= b;
  case OpCode::GT: return a > b;
  case OpCode::GE: return a >= b;
  case OpCode::AND: return (a != 0) && (b != 0);
  case OpCode::OR: return (a != 0) || (b != 0);
  default: return std::nullopt;
  }
}

static std::optional<int> evalUnary(OpCode op, int a) {
  switch (op) {
  case OpCode::NEG: return -a;
  case OpCode::NOT: return !a;
  default: return std::nullopt;
  }
}

bool ConstFoldPass::run(Function &fn) {
  bool changed = false;
  for (auto &blk : fn.getBlocks()) {
    auto &insts = blk->getInstructions();
    for (auto &inst : insts) {
      auto op = inst.getOp();
      // 二元
      if (op == OpCode::ADD || op == OpCode::SUB || op == OpCode::MUL || op == OpCode::DIV || op == OpCode::MOD ||
          op == OpCode::EQ || op == OpCode::NEQ || op == OpCode::LT || op == OpCode::LE || op == OpCode::GT || op == OpCode::GE ||
          op == OpCode::AND || op == OpCode::OR) {
        if (isConst(inst.getArg1()) && isConst(inst.getArg2())) {
          auto ca = inst.getArg1().asInt();
          auto cb = inst.getArg2().asInt();
          if (auto res = evalBinary(op, ca, cb)) {
            inst.setOp(OpCode::ASSIGN);
            inst.setArg1(Operand::ConstantInt(*res));
            inst.setArg2(Operand());
            changed = true;
          }
        }
      }
      // 一元
      else if (op == OpCode::NEG || op == OpCode::NOT) {
        if (isConst(inst.getArg1())) {
          if (auto res = evalUnary(op, inst.getArg1().asInt())) {
            inst.setOp(OpCode::ASSIGN);
            inst.setArg1(Operand::ConstantInt(*res));
            inst.setArg2(Operand());
            changed = true;
          }
        }
      }
      // 恒真/恒假跳转
      else if (op == OpCode::IF) {
        if (isConst(inst.getArg1())) {
          int cond = inst.getArg1().asInt();
          if (cond == 0) {
            // 条件恒假: 去掉跳转 => 变成 no-op (ASSIGN cond -> 临时保持占位)
            inst.setOp(OpCode::ASSIGN);
            inst.setArg2(Operand());
            inst.setResult(Operand());
            changed = true;
          } else {
            // 条件恒真: 变为无条件跳转
            inst.setOp(OpCode::GOTO);
            // 将 label 放在 result 位置 => 我们当前 IF 格式: IF cond, label
            // 构造新指令: GOTO label
            inst.setArg1(inst.getArg2()); // label 移动到 arg1
            inst.setArg2(Operand());
            inst.setResult(Operand());
            changed = true;
          }
        }
      }
    }
  }
  return changed;
}

bool LocalDCEPass::run(Function &fn) {
  bool changed = false;
  for (auto &blk : fn.getBlocks()) {
    // 统计使用次数（仅在本基本块内）
    std::unordered_map<int, int> useCount;
    for (auto &inst : blk->getInstructions()) {
      auto addUse = [&useCount](const Operand &op) {
        if (op.getType() == OperandType::Temporary) useCount[op.asInt()]++;
      };
      addUse(inst.getArg1());
      addUse(inst.getArg2());
    }

    auto &insts = blk->getInstructions();
    // 使用 erase-remove 习语来高效删除元素
    auto new_end = std::remove_if(insts.begin(), insts.end(), [&](const Instruction& inst) {
      // 仅删除产生临时结果且无副作用的指令
      if (!hasSideEffect(inst.getOp()) && inst.getResult().getType() == OperandType::Temporary) {
        int t = inst.getResult().asInt();
        if (useCount[t] == 0) {
          changed = true;
          return true;  // 标记为需要删除
        }
      }
      return false;  // 保留该指令
    });
    insts.erase(new_end, insts.end());
  }
  return changed;
}

void runDefaultQuadOptimizations(Function &fn) {
  PassManager pm;
  pm.add(std::make_unique<ConstFoldPass>());
  pm.add(std::make_unique<LocalDCEPass>());
  pm.run(fn);
}
