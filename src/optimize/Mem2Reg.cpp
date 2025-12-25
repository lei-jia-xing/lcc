#include "optimize/Mem2Reg.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"

static std::map<BasicBlock *, std::vector<BasicBlock *>>
buildPredMap(Function &F) {
  std::map<BasicBlock *, std::vector<BasicBlock *>> preds;
  for (auto &bb_ptr : F.getBlocks()) {
    BasicBlock *bb = bb_ptr.get();
    std::vector<BasicBlock *> successors;
    if (bb->jumpTarget) {
      successors.push_back(bb->jumpTarget.get());
    }
    if (bb->next) {
      successors.push_back(bb->next.get());
    }
    // fill in the pred map
    for (BasicBlock *succ : successors) {
      preds[succ].push_back(bb);
    }
  }
  return preds;
}

bool Mem2RegPass::run(Function &F, DominatorTree &DT) {
  _allocas.clear();
  _domFrontiers.clear();
  _phiToVarId.clear();
  _varStacks.clear();

  collectPromotableAllocas(F);
  if (_allocas.empty()) {
    return false;
  }
  computeDominanceFrontiers(F, DT);
  insertPhiNodes(F);
  if (!F.getBlocks().empty()) {
    renameVariables(F.getBlocks().front().get(), DT, F);
  }
  for (auto &bb : F.getBlocks()) {
    auto &insts = bb->getInstructions();
    std::vector<std::unique_ptr<Instruction>> newInsts;
    for (auto &inst : insts) {
      bool remove = false;
      if (inst->getOp() == OpCode::ALLOCA) {
        Operand sym = inst->getArg1();
        if (sym.getType() == OperandType::Variable) {
          if (_allocas.count(sym.asSymbol()->id)) {
            remove = true;
          }
        }
      }
      if (!remove) {
        auto usesPromotedVar = [&](const Operand &op) {
          return op.getType() == OperandType::Variable &&
                 _allocas.count(op.asSymbol()->id);
        };
        if (usesPromotedVar(inst->getArg1()) ||
            usesPromotedVar(inst->getArg2()) ||
            usesPromotedVar(inst->getResult())) {
          remove = true;
        }
      }
      if (!remove) {
        newInsts.push_back(std::move(inst));
      }
    }
    insts = std::move(newInsts);
  }
  return true;
}

void Mem2RegPass::collectPromotableAllocas(Function &F) {
  for (auto &bb : F.getBlocks()) {
    for (auto &inst : bb->getInstructions()) {
      if (inst->getOp() == OpCode::ALLOCA) {
        Operand symOp = inst->getArg1();
        if (symOp.getType() == OperandType::Variable) {
          auto sym = symOp.asSymbol();
          bool isArray =
              (sym->type && sym->type->category == Type::Category::Array);
          if (!isArray) {
            AllocaInfo info;
            info.allocaInst = inst.get();
            info.varId = sym->id;
            info.isPromotable = true;
            _allocas[sym->id] = info;
          }
        }
      }
    }
  }
  // collect definition blocks
  for (auto &bb : F.getBlocks()) {
    for (auto &inst : bb->getInstructions()) {
      OpCode op = inst->getOp();
      if (op == OpCode::STORE) {
        Operand base = inst->getArg2();
        Operand index = inst->getResult();
        bool isScalar =
            index.getType() == OperandType::Empty ||
            (index.getType() == OperandType::ConstantInt && index.asInt() == 0);
        if (base.getType() == OperandType::Variable && isScalar) {
          int varId = base.asSymbol()->id;
          if (_allocas.count(varId)) {
            _allocas[varId].defBlocks.insert(bb.get());
          }
        }
        continue;
      }
      Operand dst = inst->getResult();
      if (dst.getType() == OperandType::Variable) {
        int varId = dst.asSymbol()->id;
        if (_allocas.count(varId)) {
          _allocas[varId].defBlocks.insert(bb.get());
        }
      }
    }
  }
}

void Mem2RegPass::computeDominanceFrontiers(Function &F, DominatorTree &DT) {
  auto predsMap = buildPredMap(F);
  for (auto &bb_ptr : F.getBlocks()) {
    BasicBlock *bb = bb_ptr.get();
    auto &preds = predsMap[bb];
    if (preds.size() >= 2) {
      for (BasicBlock *p : preds) {
        BasicBlock *runner = p;
        while (runner != DT.getImmediateDominator(bb)) {
          _domFrontiers[runner].insert(bb);
          runner = DT.getImmediateDominator(runner);
          if (!runner) {
            break;
          }
        }
      }
    }
  }
}

void Mem2RegPass::insertPhiNodes(Function &F) {
  for (auto &entry : _allocas) {
    int varId = entry.first;
    AllocaInfo &info = entry.second;
    if (!info.isPromotable)
      continue;
    std::vector<BasicBlock *> worklist(info.defBlocks.begin(),
                                       info.defBlocks.end());
    std::set<BasicBlock *> visited;
    std::set<BasicBlock *> hasPhi;
    for (auto *bb : info.defBlocks) {
      visited.insert(bb);
    }
    size_t i = 0;
    while (i < worklist.size()) {
      BasicBlock *X = worklist[i++];
      for (BasicBlock *Y : _domFrontiers[X]) {
        if (hasPhi.find(Y) == hasPhi.end()) {
          Operand phiRes = Operand::Temporary(F.allocateTemp());
          auto phi =
              std::make_unique<Instruction>(Instruction::MakePhi(phiRes));
          _phiToVarId[phi.get()] = varId;
          auto &insts = Y->getInstructions();
          auto insertIt = insts.begin();

          while (insertIt != insts.end() &&
                 (*insertIt)->getOp() == OpCode::LABEL) {
            ++insertIt;
          }
          insts.insert(insertIt, std::move(phi));
          hasPhi.insert(Y);
          if (visited.find(Y) == visited.end()) {
            worklist.push_back(Y);
            visited.insert(Y);
          }
        }
      }
    }
  }
}

void Mem2RegPass::renameVariables(BasicBlock *bb, DominatorTree &DT,
                                  Function &F) {
  std::map<int, int> pushCount;
  for (auto &inst : bb->getInstructions()) {
    if (inst->getOp() == OpCode::PHI) {
      if (_phiToVarId.count(inst.get())) {
        int varId = _phiToVarId[inst.get()];
        _varStacks[varId].push(inst->getResult());
        pushCount[varId]++;
      }
    }
  }
  // handle basic instructions in the block
  auto &insts = bb->getInstructions();
  for (auto &inst : insts) {
    OpCode op = inst->getOp();
    // jump over phi(already handled) and alloca(to be removed)
    if (op == OpCode::PHI || op == OpCode::ALLOCA) {
      continue;
    }
    auto replaceUse = [&](Operand &opToReplace) {
      if (opToReplace.getType() == OperandType::Variable) {
        int id = opToReplace.asSymbol()->id;
        if (_allocas.count(id)) {
          if (!_varStacks[id].empty()) {
            opToReplace = _varStacks[id].top();
          }
        }
      }
    };

    if (op == OpCode::LOAD) {
      Operand base = inst->getArg1();
      if (base.getType() == OperandType::Variable) {
        int id = base.asSymbol()->id;
        if (_allocas.count(id)) {
          Operand val = _varStacks[id].empty() ? Operand::ConstantInt(0)
                                               : _varStacks[id].top();
          inst->setOp(OpCode::ASSIGN);
          inst->setArg1(val);
          inst->setArg2(Operand());
          op = OpCode::ASSIGN;
        }
      }
    }
    // handle uses
    if (op == OpCode::ASSIGN) {
      Operand src = inst->getArg1();
      replaceUse(src);
      inst->setArg1(src);
    } else if (op == OpCode::STORE) {
      Operand val = inst->getArg1();
      replaceUse(val);
      inst->setArg1(val);

      Operand idx = inst->getResult();
      replaceUse(idx);
      inst->setResult(idx);
    } else {
      Operand arg1 = inst->getArg1();
      replaceUse(arg1);
      inst->setArg1(arg1);

      Operand arg2 = inst->getArg2();
      replaceUse(arg2);
      inst->setArg2(arg2);

      if (op == OpCode::RETURN) {
        Operand res = inst->getResult();
        replaceUse(res);
        inst->setResult(res);
      }
    }
    // handle definitions
    if (op == OpCode::ASSIGN) {
      // ASSIGN src, -, dst
      Operand dst = inst->getResult();
      if (dst.getType() == OperandType::Variable) {
        int id = dst.asSymbol()->id;
        if (_allocas.count(id)) {
          Operand src = inst->getArg1();

          bool needFreeze = false;
          if (src.getType() == OperandType::Variable) {
            if (!_allocas.count(src.asSymbol()->id)) {
              needFreeze = true;
            }
          }
          if (needFreeze) {
            Operand temp = Operand::Temporary(F.allocateTemp());
            inst->setResult(temp);
            _varStacks[id].push(temp);
            pushCount[id]++;
          } else {
            _varStacks[id].push(src);
            pushCount[id]++;
            inst->setOp(OpCode::NOP);
          }
        }
      }
    } else if (op == OpCode::STORE) {
      Operand base = inst->getArg2();
      Operand index = inst->getResult();
      bool isScalar =
          index.getType() == OperandType::Empty ||
          (index.getType() == OperandType::ConstantInt && index.asInt() == 0);
      if (base.getType() == OperandType::Variable && isScalar) {
        int id = base.asSymbol()->id;
        if (_allocas.count(id)) {
          _varStacks[id].push(inst->getArg1());
          pushCount[id]++;
          inst->setOp(OpCode::NOP);
        }
      }
    } else {
      Operand dst = inst->getResult();
      if (dst.getType() == OperandType::Variable) {
        int id = dst.asSymbol()->id;
        if (_allocas.count(id)) {
          Operand newTemp = Operand::Temporary(F.allocateTemp());
          inst->setResult(newTemp);
          _varStacks[id].push(newTemp);
          pushCount[id]++;
        }
      }
    }
  }

  // fill phi arguments in successor blocks
  std::vector<BasicBlock *> successors;
  if (bb->jumpTarget) {
    successors.push_back(bb->jumpTarget.get());
  }
  if (bb->next) {
    successors.push_back(bb->next.get());
  }

  for (BasicBlock *succ : successors) {
    for (auto &inst : succ->getInstructions()) {
      if (inst->getOp() == OpCode::PHI) {
        if (_phiToVarId.count(inst.get())) {
          int varId = _phiToVarId[inst.get()];
          Operand val = _varStacks[varId].empty() ? Operand::ConstantInt(0)
                                                  : _varStacks[varId].top();
          inst->addPhiArg(val, bb);
        }
      }
    }
  }

  const auto &children = DT.getDominatedBlocks(bb);
  for (BasicBlock *child : children) {
    renameVariables(child, DT, F);
  }
  for (auto const &[varId, count] : pushCount) {
    for (int i = 0; i < count; i++) {
      _varStacks[varId].pop();
    }
  }
}
