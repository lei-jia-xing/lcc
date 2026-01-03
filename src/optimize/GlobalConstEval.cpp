#include "optimize/GlobalConstEval.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include <unordered_map>
#include <unordered_set>

Function *GlobalConstEvalPass::findFunction(const std::string &name) {
  for (const auto &fn : functions) {
    if (fn->getName() == name)
      return fn.get();
  }
  return nullptr;
}

bool GlobalConstEvalPass::run(Function &fn) {
  bool changed = false;

  for (auto &bb : fn.getBlocks()) {
    auto &insts = bb->getInstructions();

    std::vector<int> currentArgs;
    std::vector<Instruction *> argInsts;

    for (size_t i = 0; i < insts.size(); ++i) {
      Instruction *inst = insts[i].get();
      OpCode op = inst->getOp();

      if (op == OpCode::ARG) {
        if (inst->getArg1().getType() == OperandType::ConstantInt) {
          currentArgs.push_back(inst->getArg1().asInt());
          argInsts.push_back(inst);
        } else {
          currentArgs.clear();
          argInsts.clear();
        }
      } else if (op == OpCode::CALL) {
        Operand funcOp = inst->getArg2();
        if (funcOp.getType() == OperandType::Variable) {
          auto sym = funcOp.asSymbol();
          std::string name = funcOp.asSymbol()->globalName;
          Function *callee = findFunction(name);

          if (callee) {
            auto res = evaluate(callee, currentArgs, 0);

            if (res.first) {
              int constVal = res.second;
              inst->setOp(OpCode::ASSIGN);
              inst->setArg1(Operand::ConstantInt(constVal));
              inst->setArg2(Operand());
              changed = true;

              for (auto *argInst : argInsts) {
                argInst->setOp(OpCode::NOP);
              }
            }
          }
        }
        currentArgs.clear();
        argInsts.clear();
      } else if (op != OpCode::NOP) {
        currentArgs.clear();
        argInsts.clear();
      }
    }
  }
  return changed;
}

std::pair<bool, int> GlobalConstEvalPass::evaluate(Function *fn,
                                                   const std::vector<int> &args,
                                                   int depth) {
  if (depth > MAX_RECURSION_DEPTH)
    return {false, 0};

  // memory search
  std::pair<std::string, std::vector<int>> cacheKey = {fn->getName(), args};
  std::unordered_map<int, std::unordered_map<int, int>> memory;
  std::unordered_set<int> localAllocas;
  auto cacheIt = evalCache.find(cacheKey);
  if (cacheIt != evalCache.end()) {
    return {true, cacheIt->second};
  }

  if (fn->getBlocks().empty())
    return {false, 0};

  std::unordered_map<int, int> env;       // TempID -> Value
  std::unordered_map<int, int> localVars; // VarID -> Value
  BasicBlock *currentBlock = fn->getBlocks().front().get();
  BasicBlock *prevBlock = nullptr;
  int instructionsExecuted = 0;
  // emulator args stack
  std::vector<int> pendingArgs;

  int argIdx = 0;
  for (auto &inst : currentBlock->getInstructions()) {
    if (inst->getOp() == OpCode::PARAM) {
      if (argIdx < args.size()) {
        Operand dest = inst->getResult();
        if (dest.getType() == OperandType::Temporary) {
          env[dest.asInt()] = args[argIdx];
        } else if (dest.getType() == OperandType::Variable) {
          localVars[dest.asSymbol()->id] = args[argIdx];
          memory[dest.asSymbol()->id][0] = args[argIdx];
        }
      }
      argIdx++;
    }
  }
  for (auto &inst : currentBlock->getInstructions()) {
    if (inst->getOp() == OpCode::ALLOCA) {
      if (inst->getArg1().getType() == OperandType::Variable) {
        localAllocas.insert(inst->getArg1().asSymbol()->id);
      }
    }
  }

  while (currentBlock) {
    for (auto &inst : currentBlock->getInstructions()) {
      if (inst->getOp() == OpCode::PHI) {
        if (!prevBlock)
          return {false, 0};

        bool foundPath = false;
        for (auto &pair : inst->getPhiArgs()) {
          if (pair.second == prevBlock) {
            Operand valOp = pair.first;
            int val = 0;
            if (valOp.getType() == OperandType::ConstantInt) {
              val = valOp.asInt();
            } else if (valOp.getType() == OperandType::Temporary) {
              auto it = env.find(valOp.asInt());
              if (it != env.end())
                val = it->second;
              else
                return {false, 0};
            } else {
              return {false, 0};
            }
            env[inst->getResult().asInt()] = val;
            foundPath = true;
            break;
          }
        }
        if (!foundPath)
          return {false, 0};
      } else if (inst->getOp() != OpCode::LABEL) {
        break;
      }
    }

    for (auto &inst : currentBlock->getInstructions()) {
      OpCode op = inst->getOp();
      if (op == OpCode::PHI || op == OpCode::LABEL || op == OpCode::NOP ||
          op == OpCode::PARAM || op == OpCode::ALLOCA)
        continue;

      instructionsExecuted++;
      if (instructionsExecuted > MAX_INSTRUCTIONS) {
        return {false, 0};
      }

      auto getVal = [&](const Operand &o) -> std::pair<bool, int> {
        if (o.getType() == OperandType::ConstantInt)
          return {true, o.asInt()};
        if (o.getType() == OperandType::Temporary) {
          auto iter = env.find(o.asInt());
          if (iter != env.end())
            return {true, iter->second};
        }
        if (o.getType() == OperandType::Variable) {
          auto iter = localVars.find(o.asSymbol()->id);
          if (iter != localVars.end())
            return {true, iter->second};
        }
        return {false, 0};
      };
      if (op == OpCode::LOAD) {
        Operand base = inst->getArg1();
        Operand index = inst->getArg2();
        if (base.getType() == OperandType::Variable) {
          int id = base.asSymbol()->id;
          int offset = 0;
          if (index.getType() != OperandType::Empty) {
            auto res = getVal(index);
            if (!res.first)
              return {false, 0};
            offset = res.second;
          }

          if (memory.count(id) && memory[id].count(offset)) {
            env[inst->getResult().asInt()] = memory[id][offset];
            continue;
          } else if (offset == 0 && localVars.count(id)) {
            env[inst->getResult().asInt()] = localVars[id];
            continue;
          }
        }
        return {false, 0};
      }
      if (op == OpCode::STORE) {
        Operand val = inst->getArg1();
        Operand base = inst->getArg2();
        Operand index = inst->getResult();

        if (base.getType() == OperandType::Variable) {
          int baseId = base.asSymbol()->id;
          bool isLocalAlloca = localAllocas.count(baseId);
          bool isLocalScalar =
              localVars.count(baseId) && index.getType() == OperandType::Empty;
          // do not write to global variable & param array to avoid side effect
          if (!isLocalAlloca && !isLocalScalar)
            return {false, 0};

          auto valRes = getVal(val);
          if (!valRes.first)
            return {false, 0};
          int offset = 0;
          if (index.getType() != OperandType::Empty) {
            auto idxRes = getVal(index);
            if (!idxRes.first)
              return {false, 0};
            offset = idxRes.second;
          }
          memory[base.asSymbol()->id][offset] = valRes.second;
          if (offset == 0) {
            localVars[base.asSymbol()->id] = valRes.second;
          }
          continue;
        }
        return {false, 0};
      }

      if (op == OpCode::RETURN) {
        int retVal = 0;
        if (inst->getResult().getType() != OperandType::Empty) {
          auto res = getVal(inst->getResult());
          if (!res.first)
            return {false, 0};
          retVal = res.second;
        }
        evalCache[cacheKey] = retVal;
        return {true, retVal};
      }

      if (op == OpCode::ARG) {
        auto val = getVal(inst->getArg1());
        if (!val.first)
          return {false, 0};
        pendingArgs.push_back(val.second);
        continue;
      }

      if (op == OpCode::CALL) {
        // CALL argc(const), func(variable), res(temp)
        Operand funcOp = inst->getArg2();
        if (funcOp.getType() != OperandType::Variable)
          return {false, 0};
        std::string funcName = funcOp.asSymbol()->globalName;

        // can't do IO in compiler time
        if (funcName == "getint" || funcName == "printf")
          return {false, 0};

        Function *callee = findFunction(funcName);
        if (!callee)
          return {false, 0};

        // recursive evaluate
        auto result = evaluate(callee, pendingArgs, depth + 1);

        pendingArgs.clear();

        if (!result.first)
          return {false, 0};

        if (inst->getResult().getType() == OperandType::Temporary) {
          env[inst->getResult().asInt()] = result.second;
        }
        continue;
      }

      if (!pendingArgs.empty())
        pendingArgs.clear();

      if (op == OpCode::GOTO) {
        BasicBlock *target = nullptr;
        if (currentBlock->jumpTarget)
          target = currentBlock->jumpTarget.get();

        if (target) {
          prevBlock = currentBlock;
          currentBlock = target;
          goto NextBlockLabel;
        }
        return {false, 0};
      }

      if (op == OpCode::IF) {
        auto cond = getVal(inst->getArg1());
        if (!cond.first)
          return {false, 0};

        if (cond.second != 0) {
          BasicBlock *target = nullptr;
          if (currentBlock->jumpTarget)
            target = currentBlock->jumpTarget.get();
          else if (inst->getResult().getType() == OperandType::Label) {
            int id = inst->getResult().asInt();
            for (auto &b : fn->getBlocks())
              if (b->getLabelId() == id) {
                target = b.get();
                break;
              }
          }
          if (target) {
            prevBlock = currentBlock;
            currentBlock = target;
            goto NextBlockLabel;
          } else {
            return {false, 0};
          }
        }
        continue;
      }

      if (inst->getResult().getType() == OperandType::Temporary) {
        auto v1 = getVal(inst->getArg1());
        auto v2 = getVal(inst->getArg2());

        if (op == OpCode::ASSIGN) {
          if (v1.first) {
            env[inst->getResult().asInt()] = v1.second;
            continue;
          } else
            return {false, 0};
        }

        if (v1.first &&
            (inst->getArg2().getType() == OperandType::Empty || v2.first)) {
          int val1 = v1.second;
          int val2 = v2.first ? v2.second : 0;
          int resVal = 0;

          switch (op) {
          case OpCode::ADD:
            resVal = val1 + val2;
            break;
          case OpCode::SUB:
            resVal = val1 - val2;
            break;
          case OpCode::MUL:
            resVal = val1 * val2;
            break;
          case OpCode::DIV:
            if (val2 == 0)
              return {false, 0};
            resVal = val1 / val2;
            break;
          case OpCode::MOD:
            if (val2 == 0)
              return {false, 0};
            resVal = val1 % val2;
            break;
          case OpCode::NEG:
            resVal = -val1;
            break;
          case OpCode::NOT:
            resVal = !val1;
            break;
          case OpCode::EQ:
            resVal = (val1 == val2);
            break;
          case OpCode::NEQ:
            resVal = (val1 != val2);
            break;
          case OpCode::LT:
            resVal = (val1 < val2);
            break;
          case OpCode::LE:
            resVal = (val1 <= val2);
            break;
          case OpCode::GT:
            resVal = (val1 > val2);
            break;
          case OpCode::GE:
            resVal = (val1 >= val2);
            break;
          case OpCode::AND:
            resVal = (val1 && val2);
            break;
          case OpCode::OR:
            resVal = (val1 || val2);
            break;
          default:
            return {false, 0};
          }
          env[inst->getResult().asInt()] = resVal;
        } else {
          return {false, 0};
        }
      } else {
        return {false, 0};
      }
    }

    if (currentBlock->next) {
      prevBlock = currentBlock;
      currentBlock = currentBlock->next.get();
    } else {
      break;
    }

  NextBlockLabel:;
  }

  return {false, 0};
}
