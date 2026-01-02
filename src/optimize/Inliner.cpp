#include "optimize/Inliner.hpp"

int InlinerPass::countInstructions(Function *func) {
  int count = 0;
  for (auto &bb : func->getBlocks()) {
    count += bb->getInstructions().size();
  }
  return count;
}

bool InlinerPass::isRecursive(Function *caller, Function *callee) {
  return caller == callee;
}

int InlinerPass::getNewTemp(int oldTemp, std::unordered_map<int, int> &tempMap,
                            Function *caller) {
  if (tempMap.find(oldTemp) == tempMap.end()) {
    tempMap[oldTemp] = caller->allocateTemp();
  }
  return tempMap[oldTemp];
}

int InlinerPass::getNewLabel(int oldLabel,
                             std::unordered_map<int, int> &labelMap,
                             Function *caller) {
  if (labelMap.find(oldLabel) == labelMap.end()) {
    labelMap[oldLabel] = caller->allocateLabel();
  }
  return labelMap[oldLabel];
}

Operand InlinerPass::mapOperand(Operand op,
                                std::unordered_map<int, int> &tempMap,
                                std::unordered_map<int, int> &labelMap,
                                std::unordered_map<int, int> &varMap,
                                Function *caller) {
  if (op.getType() == OperandType::Temporary) {
    return Operand::Temporary(getNewTemp(op.asInt(), tempMap, caller));
  }
  if (op.getType() == OperandType::Label) {
    return Operand::Label(getNewLabel(op.asInt(), labelMap, caller));
  }
  if (op.getType() == OperandType::Variable) {
    int id = op.asSymbol()->id;
    if (varMap.count(id)) {
      return Operand::Temporary(varMap[id]);
    }
  }
  return op;
}

std::unique_ptr<Instruction> InlinerPass::copyInstruction(
    Instruction *inst, std::unordered_map<int, int> &tempMap,
    std::unordered_map<int, int> &labelMap,
    std::unordered_map<int, int> &varMap, Function *caller) {

  auto newInst = std::make_unique<Instruction>(inst->getOp());
  newInst->setArg1(
      mapOperand(inst->getArg1(), tempMap, labelMap, varMap, caller));
  newInst->setArg2(
      mapOperand(inst->getArg2(), tempMap, labelMap, varMap, caller));

  Operand res = inst->getResult();
  if (res.getType() == OperandType::Temporary) {
    res = Operand::Temporary(getNewTemp(res.asInt(), tempMap, caller));
  } else if (res.getType() == OperandType::Label) {
    res = Operand::Label(getNewLabel(res.asInt(), labelMap, caller));
  } else if (res.getType() == OperandType::Variable) {
    int id = res.asSymbol()->id;
    if (varMap.count(id)) {
      res = Operand::Temporary(varMap[id]);
    }
  }
  newInst->setResult(res);
  return newInst;
}

void InlinerPass::run(const std::vector<std::shared_ptr<Function>> &funcs) {
  bool changed = true;
  int round = 0;
  while (changed && round < 10) {
    changed = false;
    round++;

    for (auto &caller : funcs) {
      if (caller->getBlocks().empty())
        continue;

      bool localChanged = false;
      auto &blocks = caller->getBlocks();
      for (size_t i = 0; i < blocks.size(); ++i) {
        BasicBlock *bb = blocks[i].get();
        auto &insts = bb->getInstructions();

        for (auto iit = insts.begin(); iit != insts.end(); ++iit) {
          Instruction *inst = iit->get();
          if (inst->getOp() == OpCode::CALL) {
            const Operand &funcOp = inst->getArg2();

            if (funcOp.getType() != OperandType::Variable) {
              continue;
            }

            std::string calleeName = funcOp.asSymbol()->globalName.empty()
                                         ? funcOp.asSymbol()->name
                                         : funcOp.asSymbol()->globalName;

            Function *callee = nullptr;
            for (auto &f : funcs) {
              if (f->getName() == calleeName) {
                callee = f.get();
                break;
              }
            }

            if (callee && !callee->getBlocks().empty() &&
                !isRecursive(caller.get(), callee) &&
                countInstructions(callee) < INLINE_THRESHOLD) {

              if (inlineFunction(caller.get(), bb, inst, callee)) {
                changed = true;
                localChanged = true;
                goto next_function;
              }
            }
          }
        }
      }
    next_function:
      if (localChanged) {
        caller->buildCFG();
      }
    }
  }
}

bool InlinerPass::inlineFunction(Function *caller, BasicBlock *callBlock,
                                 Instruction *callInst, Function *callee) {
  std::unordered_map<int, int> tempMap;
  std::unordered_map<int, int> labelMap;
  std::unordered_map<int, int> varMap;

  auto splitAfterPtr = caller->createBlock();
  BasicBlock *splitAfter = splitAfterPtr.get();
  int splitAfterLabel = caller->allocateLabel();
  splitAfter->addInstruction(std::make_unique<Instruction>(
      Instruction::MakeLabel(Operand::Label(splitAfterLabel))));

  auto &insts = callBlock->getInstructions();
  auto callIt = insts.begin();
  while (callIt != insts.end() && callIt->get() != callInst)
    callIt++;

  if (callIt != insts.end()) {
    auto nextIt = std::next(callIt);
    while (nextIt != insts.end()) {
      (*nextIt)->setParent(splitAfter);
      splitAfter->getInstructions().push_back(std::move(*nextIt));
      nextIt = insts.erase(nextIt);
    }
  }

  int argCount = callInst->getArg1().asInt();
  auto argIt = callIt;
  std::vector<Instruction *> argsToRemove;
  std::vector<Operand> realArgs;

  for (int i = 0; i < argCount; i++) {
    if (argIt == insts.begin())
      break;
    argIt--;
    while (argIt != insts.begin() && (*argIt)->getOp() != OpCode::ARG)
      argIt--;
    if ((*argIt)->getOp() == OpCode::ARG) {
      realArgs.insert(realArgs.begin(), (*argIt)->getArg1());
      argsToRemove.push_back(argIt->get());
    }
  }

  BasicBlock *calleeEntry = callee->getBlocks().front().get();
  int paramIdx = 0;
  for (auto &inst : calleeEntry->getInstructions()) {
    if (inst->getOp() == OpCode::PARAM) {
      if (paramIdx < realArgs.size()) {
        Operand paramOp = inst->getResult();

        int newParamTemp = caller->allocateTemp();
        if (paramOp.getType() == OperandType::Variable) {
          varMap[paramOp.asSymbol()->id] = newParamTemp;
        } else if (paramOp.getType() == OperandType::Temporary) {
          tempMap[paramOp.asInt()] = newParamTemp;
        }

        callBlock->addInstruction(
            std::make_unique<Instruction>(Instruction::MakeAssign(
                realArgs[paramIdx], Operand::Temporary(newParamTemp))));
      }
      paramIdx++;
    }
  }

  for (auto rm : argsToRemove) {
    rm->setOp(OpCode::NOP);
  }
  callInst->setOp(OpCode::NOP);

  std::vector<BasicBlock *> newBlocks;
  BasicBlock *firstCloned = nullptr;

  for (auto &cbb : callee->getBlocks()) {
    getNewLabel(cbb->getLabelId(), labelMap, caller);
  }

  for (auto &cbb : callee->getBlocks()) {
    auto newBBPtr = caller->createBlock();
    BasicBlock *newBB = newBBPtr.get();
    newBlocks.push_back(newBB);
    if (!firstCloned)
      firstCloned = newBB;

    int oldLabel = cbb->getLabelId();
    int newLabel = getNewLabel(oldLabel, labelMap, caller);

    bool hasLabelInst =
        (!cbb->getInstructions().empty() &&
         cbb->getInstructions().front()->getOp() == OpCode::LABEL);

    if (!hasLabelInst) {
      newBB->addInstruction(std::make_unique<Instruction>(
          Instruction::MakeLabel(Operand::Label(newLabel))));
    }

    for (auto &cinst : cbb->getInstructions()) {
      if (cinst->getOp() == OpCode::PARAM)
        continue;

      auto newInst =
          copyInstruction(cinst.get(), tempMap, labelMap, varMap, caller);
      newInst->setParent(newBB);

      if (newInst->getOp() == OpCode::RETURN) {
        Operand retVal = newInst->getResult();
        Operand callRes = callInst->getResult();

        if (retVal.getType() != OperandType::Empty) {

          // RETURN val -> ASSIGN callRes, val
          newInst->setOp(OpCode::ASSIGN);
          newInst->setArg1(retVal);
          newInst->setResult(callRes);
          newInst->setArg2(Operand());
          newBB->addInstruction(std::move(newInst));
        }

        newBB->addInstruction(std::make_unique<Instruction>(
            Instruction::MakeGoto(Operand::Label(splitAfterLabel))));
      } else {
        newBB->addInstruction(std::move(newInst));
      }
    }
  }

  int entryLabelId = labelMap[callee->getBlocks().front()->getLabelId()];
  callBlock->addInstruction(std::make_unique<Instruction>(
      Instruction::MakeGoto(Operand::Label(entryLabelId))));

  return true;
}
