#include "codegen/QuadOptimizer.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "optimize/DominatorTree.hpp"
#include "optimize/LICM.hpp"
#include "optimize/LoopAnalysis.hpp"
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
  case OpCode::PHI:
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
      if (op == OpCode::RETURN || op == OpCode::STORE || op == OpCode::ALLOCA ||
          op == OpCode::PARAM) {
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
        // PARAM: result is the variable
        if (op == OpCode::RETURN || op == OpCode::STORE ||
            op == OpCode::ALLOCA || op == OpCode::PARAM) {
          Operand res = inst->getResult();
          if (tryReplace(res)) {
            inst->setResult(res);
            changed = true;
          }
        }
      }

      // collect constant assignments
      const Operand &arg1 = inst->getArg1();
      const Operand &arg2 = inst->getArg2();
      const Operand &res = inst->getResult();

      if (res.getType() == OperandType::Temporary) {
        int tid = res.asInt();
        if (op == OpCode::ASSIGN &&
            arg1.getType() == OperandType::ConstantInt) {
          constMap[tid] = arg1.asInt();
        } else if (arg1.getType() == OperandType::ConstantInt &&
                   arg2.getType() == OperandType::ConstantInt) {
          int v1 = arg1.asInt();
          int v2 = arg2.asInt();
          int val = 0;
          bool folded = false;

          switch (op) {
          case OpCode::ADD:
            val = v1 + v2;
            folded = true;
            break;
          case OpCode::SUB:
            val = v1 - v2;
            folded = true;
            break;
          case OpCode::MUL:
            val = v1 * v2;
            folded = true;
            break;
          case OpCode::DIV:
            if (v2 != 0) {
              val = v1 / v2;
              folded = true;
            }
            break;
          case OpCode::MOD:
            if (v2 != 0) {
              val = v1 % v2;
              folded = true;
            }
            break;
          case OpCode::EQ:
            val = (v1 == v2);
            folded = true;
            break;
          case OpCode::NEQ:
            val = (v1 != v2);
            folded = true;
            break;
          case OpCode::LT:
            val = (v1 < v2);
            folded = true;
            break;
          case OpCode::LE:
            val = (v1 <= v2);
            folded = true;
            break;
          case OpCode::GT:
            val = (v1 > v2);
            folded = true;
            break;
          case OpCode::GE:
            val = (v1 >= v2);
            folded = true;
            break;
          case OpCode::AND:
            val = (v1 && v2);
            folded = true;
            break;
          case OpCode::OR:
            val = (v1 || v2);
            folded = true;
            break;
          default:
            break;
          }
          if (folded) {
            inst->setOp(OpCode::ASSIGN);
            inst->setArg1(Operand::ConstantInt(val));
            inst->setArg2(Operand());
            constMap[tid] = val;
            changed = true;
          }
        }
      }
    }
  }
  return changed;
}

bool AlgebraicPass::run(Function &fn) {
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
  std::function<Operand(const Operand &)> resolve =
      [&](const Operand &op) -> Operand {
    if (op.getType() == OperandType::Temporary) {
      auto it = copyMap.find(op.asInt());
      if (it != copyMap.end()) {
        return resolve(it->second);
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
        Operand r1 = resolve(inst->getArg1());
        if (!isSame(r1, inst->getArg1())) {
          inst->setArg1(r1);
          changed = true;
        }
        Operand r2 = resolve(inst->getArg2());
        if (!isSame(r2, inst->getArg2())) {
          inst->setArg2(r2);
          changed = true;
        }
        if (op == OpCode::RETURN || op == OpCode::STORE ||
            op == OpCode::ALLOCA || op == OpCode::PARAM) {
          Operand res = resolve(inst->getResult());
          if (!isSame(res, inst->getResult())) {
            inst->setResult(res);
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
              src.getType() == OperandType::ConstantInt) {
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

class ScopedExprMap {
public:
  using ExprKey = std::pair<OpCode, std::pair<Operand, Operand>>;

  struct Hash {
    size_t operator()(const ExprKey &k) const {
      size_t h1 = std::hash<int>{}(static_cast<int>(k.first));
      auto hashOp = [](const Operand &op) -> size_t {
        size_t h = std::hash<int>{}(static_cast<int>(op.getType()));
        switch (op.getType()) {
        case OperandType::Temporary:
        case OperandType::ConstantInt:
          h ^= std::hash<size_t>{}(op.asInt() << 1);
          break;
        case OperandType::Variable:
          h ^= (std::hash<void *>{}(op.asSymbol().get()) << 1);
          break;
        default:
          break;
        }
        return h;
      };
      size_t h2 = hashOp(k.second.first);
      size_t h3 = hashOp(k.second.second);
      bool commutative = (k.first == OpCode::ADD || k.first == OpCode::MUL ||
                          k.first == OpCode::EQ || k.first == OpCode::NEQ ||
                          k.first == OpCode::AND || k.first == OpCode::OR);
      if (commutative && h2 > h3)
        std::swap(h2, h3);
      return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
  };

  struct Equal {
    bool operator()(const ExprKey &lhs, const ExprKey &rhs) const {
      if (lhs.first != rhs.first)
        return false;

      const Operand &l1 = lhs.second.first;
      const Operand &l2 = lhs.second.second;
      const Operand &r1 = rhs.second.first;
      const Operand &r2 = rhs.second.second;

      bool commutative =
          (lhs.first == OpCode::ADD || lhs.first == OpCode::MUL ||
           lhs.first == OpCode::EQ || lhs.first == OpCode::NEQ ||
           lhs.first == OpCode::AND || lhs.first == OpCode::OR);

      if (commutative) {
        return (l1 == r1 && l2 == r2) || (l1 == r2 && l2 == r1);
      } else {
        return l1 == r1 && l2 == r2;
      }
    }
  };

  void enterScope() { scopeStack.push_back(history.size()); }
  void exitScope() {
    size_t limit = scopeStack.back();
    scopeStack.pop_back();
    while (history.size() > limit) {
      auto &kv = history.back();
      map.erase(kv.first);
      history.pop_back();
    }
  }
  void insert(ExprKey k, int temp) {
    if (map.find(k) == map.end()) {
      map[k] = temp;
      history.push_back({k, temp});
    }
  }
  int lookup(ExprKey k) {
    auto it = map.find(k);
    return it != map.end() ? it->second : -1;
  }

private:
  std::unordered_map<ExprKey, int, Hash, Equal> map;
  std::vector<std::pair<ExprKey, int>> history;
  std::vector<size_t> scopeStack;
};

bool CSEPass::run(Function &fn) {
  if (fn.getBlocks().empty())
    return false;

  bool changed = false;
  std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> domChildren;
  BasicBlock *root = fn.getBlocks().front().get();

  for (auto &bb_ptr : fn.getBlocks()) {
    BasicBlock *bb = bb_ptr.get();
    if (bb == root)
      continue;

    BasicBlock *idom = dt.getImmediateDominator(bb);
    if (idom) {
      domChildren[idom].push_back(bb);
    }
  }

  ScopedExprMap exprMap;
  std::function<void(BasicBlock *)> visit = [&](BasicBlock *bb) {
    exprMap.enterScope();

    for (auto &inst : bb->getInstructions()) {
      if (!inst)
        continue;
      OpCode op = inst->getOp();

      if (op == OpCode::ADD || op == OpCode::SUB || op == OpCode::MUL ||
          op == OpCode::DIV || op == OpCode::MOD || op == OpCode::EQ ||
          op == OpCode::NEQ || op == OpCode::LT || op == OpCode::LE ||
          op == OpCode::GT || op == OpCode::GE || op == OpCode::AND ||
          op == OpCode::OR) {

        if (inst->getArg1().getType() == OperandType::Variable ||
            inst->getArg2().getType() == OperandType::Variable) {
          continue;
        }
        std::pair<OpCode, std::pair<Operand, Operand>> key = {
            op, {inst->getArg1(), inst->getArg2()}};
        int existing = exprMap.lookup(key);

        if (existing != -1) {
          inst->setOp(OpCode::ASSIGN);
          inst->setArg1(Operand::Temporary(existing));
          inst->setArg2(Operand());
          changed = true;
        } else {
          if (inst->getResult().getType() == OperandType::Temporary) {
            exprMap.insert(key, inst->getResult().asInt());
          }
        }
      }
    }

    for (BasicBlock *child : domChildren[bb]) {
      visit(child);
    }

    exprMap.exitScope();
  };

  visit(root);
  return changed;
}

bool runDefaultQuadOptimizations(Function &fn, DominatorTree &dt) {
  // Run local optimizations
  PassManager pm;
  pm.add(std::make_unique<CopyPropPass>());
  pm.add(std::make_unique<ConstPropPass>());
  pm.add(std::make_unique<AlgebraicPass>());
  pm.add(std::make_unique<CSEPass>(dt));
  pm.add(std::make_unique<LocalDCEPass>());
  return pm.run(fn);
}
