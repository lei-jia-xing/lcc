#include "optimize/Mem2Reg.hpp"
#include "codegen/Operand.hpp"

static std::map<BasicBlock *, std::vector<BasicBlock *>>
buildPredMap(Function &F) {
  std::map<BasicBlock *, std::vector<BasicBlock *>> preds;
  for (auto &bb_ptr : F.getBlocks()) {
    BasicBlock *bb = bb_ptr.get();
    if (bb->next) {
      preds[bb->next.get()].push_back(bb);
    }
    if (bb->jumpTarget) {
      preds[bb->jumpTarget.get()].push_back(bb);
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
    renameVariables(F.getBlocks().front().get(), DT);
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
        if (inst->getArg1().getType() == OperandType::Empty) {
          remove = true;
        }
      }
      if (!remove) {
        newInsts.push_back(std::move(inst));
      }
    }
    insts = std::move(newInsts);
  }
  eliminatePhis(F);
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
  for (auto &bb : F.getBlocks()) {
    for (auto &inst : bb->getInstructions()) {
      OpCode op = inst->getOp();
      if (op == OpCode::ASSIGN) {
        Operand dst = inst->getResult();
        if (dst.getType() == OperandType::Variable) {
          int id = dst.asSymbol()->id;
          if (_allocas.count(id)) {
            _allocas[id].defBlocks.insert(bb.get());
          }
        }
      } else if (op == OpCode::STORE) {
        Operand base = inst->getArg2();
        Operand idx = inst->getResult();
        bool isScalarStore = idx.getType() == OperandType::Empty;
        if (base.getType() == OperandType::Variable && isScalarStore) {
          int id = base.asSymbol()->id;
          if (_allocas.count(id)) {
            _allocas[id].defBlocks.insert(bb.get());
          }
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
    size_t i = 0;
    while (i < worklist.size()) {
      BasicBlock *X = worklist[i++];
      for (BasicBlock *Y : _domFrontiers[X]) {
        if (hasPhi.find(Y) == hasPhi.end()) {
          Operand phiRes = Operand::Temporary(F.allocateTemp());
          auto phi =
              std::make_unique<Instruction>(Instruction::MakePhi(phiRes));
          _phiToVarId[phi.get()] = varId;
          Y->getInstructions().insert(Y->getInstructions().begin(),
                                      std::move(phi));
          hasPhi.insert(Y);
          if (visited.find(Y) == visited.end()) {
            visited.insert(Y);
            worklist.push_back(Y);
          }
        }
      }
    }
  }
}

void Mem2RegPass::renameVariables(BasicBlock *bb, DominatorTree &DT) {
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
    auto tryReplaceUse = [&](Operand &opToReplace) {
      if (opToReplace.getType() == OperandType::Variable) {
        int id = opToReplace.asSymbol()->id;
        if (_allocas.count(id)) {
          if (!_varStacks[id].empty()) {
            opToReplace = _varStacks[id].top();
          } else {
            opToReplace = Operand::ConstantInt(0);
          }
        }
      }
    };
    if (op != OpCode::PHI && op != OpCode::ALLOCA) {
      Operand arg1 = inst->getArg1();
      tryReplaceUse(arg1);
      inst->setArg1(arg1);

      if (op != OpCode::STORE) {
        Operand arg2 = inst->getArg2();
        tryReplaceUse(arg2);
        inst->setArg2(arg2);
      }
    }
    if (op == OpCode::ASSIGN) {
      Operand dst = inst->getResult();
      if (dst.getType() == OperandType::Variable) {
        int id = dst.asSymbol()->id;
        if (_allocas.count(id)) {
          Operand newVal = inst->getArg1();
          _varStacks[id].push(newVal);
          pushCount[id]++;

          inst->setOp(OpCode::ALLOCA);
          inst->setArg1(Operand());
        }
      }
    } else if (op == OpCode::STORE) {
      Operand base = inst->getArg2();
      Operand idx = inst->getResult();
      bool isScalarStore = (idx.getType() == OperandType::Empty);
      if (base.getType() == OperandType::Variable && isScalarStore) {
        int id = base.asSymbol()->id;
        if (_allocas.count(id)) {
          Operand newVal = inst->getArg1();
          _varStacks[id].push(newVal);
          pushCount[id]++;

          inst->setOp(OpCode::ALLOCA);
          inst->setArg1(Operand());
        }
      }
    }
  }

  // fill phi arguments in successor blocks
  std::vector<BasicBlock *> successors;
  if (bb->next)
    successors.push_back(bb->next.get());
  if (bb->jumpTarget)
    successors.push_back(bb->jumpTarget.get());

  for (BasicBlock *succ : successors) {
    for (auto &inst : succ->getInstructions()) {
      if (inst->getOp() == OpCode::PHI) {
        if (_phiToVarId.count(inst.get())) {
          int varId = _phiToVarId[inst.get()];
          Operand val;
          if (!_varStacks[varId].empty()) {
            val = _varStacks[varId].top();
          } else {
            val = Operand::ConstantInt(0);
          }
          inst->addPhiArg(val, bb);
        }
      }
    }
  }

  const auto &children = DT.getDominatedBlocks(bb);
  for (BasicBlock *child : children) {
    renameVariables(child, DT);
  }
  for (auto const &[varId, count] : pushCount) {
    for (int i = 0; i < count; i++) {
      _varStacks[varId].pop();
    }
  }
}

void Mem2RegPass::eliminatePhis(Function &F) {
  for (auto &bb : F.getBlocks()) {
    auto &insts = bb->getInstructions();
    // collect all the phi nodes
    std::vector<std::unique_ptr<Instruction>> phiNodes;
    for (auto it = insts.begin(); it != insts.end();) {
      if ((*it)->getOp() == OpCode::PHI) {
        phiNodes.push_back(std::move(*it));
        it = insts.erase(it);
      } else {
        break;
      }
    }

    for (auto &phi : phiNodes) {
      Operand dest = phi->getResult();
      const auto &args = phi->getPhiArgs();
      for (auto &pair : args) {
        Operand src = pair.first;
        BasicBlock *pred = pair.second;
        auto copy =
            std::make_unique<Instruction>(Instruction::MakeAssign(src, dest));
        // insert into the end of the predecessor block
        auto &pInsts = pred->getInstructions();
        auto insertIt = pInsts.end();

        if (!pInsts.empty()) {
          OpCode lastOp = pInsts.back()->getOp();

          if (lastOp == OpCode::GOTO || lastOp == OpCode::IF ||
              lastOp == OpCode::RETURN) {
            insertIt--;
          }
        }
        pInsts.insert(insertIt, std::move(copy));
      }
    }
  }
}
