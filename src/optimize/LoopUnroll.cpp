#include "optimize/LoopUnroll.hpp"
#include "codegen/Instruction.hpp"
#include <map>

bool LoopUnrollPass::run(Function &func, const std::vector<LoopInfo> &loops) {
  for (const auto &loop : loops) {
    if (tryUnrollLoop(func, loop)) {
      return true;
    }
  }
  return false;
}

bool LoopUnrollPass::isSimpleLoop(const LoopInfo &loop, int &tripCount,
                                  Operand &iv, int &step, int &initVal) {
  if (loop.blocks.size() != 1)
    return false;

  BasicBlock *bb = loop.header;
  if (bb->getInstructions().empty())
    return false;
  Instruction *term = bb->getInstructions().back().get();

  if (term->getOp() != OpCode::IF)
    return false;

  Operand target = term->getResult();
  if (target.getType() != OperandType::Label)
    return false;

  if (bb->getLabelId() != target.asInt())
    return false;

  Instruction *phi = nullptr;
  int phiCount = 0;
  for (auto &inst : bb->getInstructions()) {
    if (inst->getOp() == OpCode::PHI) {
      phiCount++;
      if (inst->getPhiArgs().size() == 2) {
        bool hasBackEdge = false;
        for (auto &pair : inst->getPhiArgs()) {
          if (pair.second == bb)
            hasBackEdge = true;
        }
        if (hasBackEdge) {
          phi = inst.get();
        }
      }
    }
  }

  if (phiCount != 1 || !phi)
    return false;
  iv = phi->getResult();

  Operand initOp;
  Operand nextOp;
  for (auto &pair : phi->getPhiArgs()) {
    if (pair.second == bb) {
      nextOp = pair.first;
    } else {
      initOp = pair.first;
    }
  }

  if (initOp.getType() != OperandType::ConstantInt)
    return false;
  initVal = initOp.asInt();

  Instruction *updateInst = nullptr;
  for (auto &inst : bb->getInstructions()) {
    if (inst->getResult() == nextOp) {
      updateInst = inst.get();
      break;
    }
  }

  if (!updateInst)
    return false;

  if (updateInst->getOp() != OpCode::ADD)
    return false;

  Operand stepOp;
  if (updateInst->getArg1() == iv) {
    stepOp = updateInst->getArg2();
  } else if (updateInst->getArg2() == iv) {
    stepOp = updateInst->getArg1();
  } else {
    return false;
  }

  if (stepOp.getType() != OperandType::ConstantInt)
    return false;
  step = stepOp.asInt();

  Operand condOp = term->getArg1();
  Instruction *condInst = nullptr;
  for (auto &inst : bb->getInstructions()) {
    if (inst->getResult() == condOp) {
      condInst = inst.get();
      break;
    }
  }
  if (!condInst)
    return false;

  int limit = 0;
  OpCode cmpOp = condInst->getOp();
  Operand cmpL = condInst->getArg1();
  Operand cmpR = condInst->getArg2();

  if (cmpR == iv || cmpR == nextOp) {
    std::swap(cmpL, cmpR);
    if (cmpOp == OpCode::LT)
      cmpOp = OpCode::GT;
    else if (cmpOp == OpCode::LE)
      cmpOp = OpCode::GE;
    else if (cmpOp == OpCode::GT)
      cmpOp = OpCode::LT;
    else if (cmpOp == OpCode::GE)
      cmpOp = OpCode::LE;
  }

  if (cmpL != iv && cmpL != nextOp)
    return false;
  if (cmpR.getType() != OperandType::ConstantInt)
    return false;
  limit = cmpR.asInt();

  if (cmpOp == OpCode::LT && step > 0) {
    if (initVal >= limit)
      tripCount = 0;
    else
      tripCount = (limit - initVal + step - 1) / step;
  } else if (cmpOp == OpCode::LE && step > 0) {
    if (initVal > limit)
      tripCount = 0;
    else
      tripCount = (limit - initVal) / step + 1;
  } else {
    return false;
  }

  return true;
}

bool LoopUnrollPass::tryUnrollLoop(Function &func, const LoopInfo &loop) {
  int tripCount = 0;
  Operand iv;
  int step = 0;
  int initVal = 0;

  if (!isSimpleLoop(loop, tripCount, iv, step, initVal))
    return false;

  BasicBlock *header = loop.header;

  std::vector<Instruction *> body;
  for (auto &inst : header->getInstructions()) {
    if (inst->getOp() == OpCode::PHI)
      continue;
    if (inst->getOp() == OpCode::IF)
      continue;
    if (inst->getOp() == OpCode::LABEL)
      continue;
    if (inst->getOp() == OpCode::GOTO)
      continue;
    body.push_back(inst.get());
  }

  BasicBlock *exitBlock = nullptr;
  std::shared_ptr<BasicBlock> exitBlockPtr = nullptr;

  if (header->next &&
      loop.blocks.find(header->next.get()) == loop.blocks.end()) {
    exitBlock = header->next.get();
    exitBlockPtr = header->next;
  } else if (header->jumpTarget &&
             loop.blocks.find(header->jumpTarget.get()) == loop.blocks.end()) {
    exitBlock = header->jumpTarget.get();
    exitBlockPtr = header->jumpTarget;
  }

  if (!exitBlock)
    return false;

  std::map<int, int> varMap;

  auto mapOp = [&](Operand op) {
    if (op.getType() == OperandType::Temporary) {
      if (varMap.count(op.asInt())) {
        return Operand::Temporary(varMap[op.asInt()]);
      }
    }
    return op;
  };

  std::vector<std::unique_ptr<Instruction>> oldInsts =
      std::move(header->getInstructions());
  header->getInstructions().clear();

  if (!oldInsts.empty() && oldInsts.front()->getOp() == OpCode::LABEL) {
    header->addInstruction(std::move(oldInsts.front()));
  }

  int currentIV = initVal;
  BasicBlock *currentBlock = header;

  for (int i = 0; i < tripCount; ++i) {
    for (Instruction *inst : body) {
      auto newInst = std::make_unique<Instruction>(inst->getOp());

      Operand a1 = inst->getArg1();
      if (a1 == iv)
        a1 = Operand::ConstantInt(currentIV);
      else
        a1 = mapOp(a1);

      Operand a2 = inst->getArg2();
      if (a2 == iv)
        a2 = Operand::ConstantInt(currentIV);
      else
        a2 = mapOp(a2);

      newInst->setArg1(a1);
      newInst->setArg2(a2);

      Operand res = inst->getResult();
      if (res.getType() == OperandType::Temporary) {
        int oldT = res.asInt();
        int newT = func.allocateTemp();
        varMap[oldT] = newT;
        newInst->setResult(Operand::Temporary(newT));
      } else {
        newInst->setResult(res);
      }

      currentBlock->addInstruction(std::move(newInst));
    }
    currentIV += step;
  }

  currentBlock->addInstruction(std::make_unique<Instruction>(
      Instruction::MakeGoto(Operand::Label(exitBlock->getLabelId()))));
  currentBlock->next = nullptr;
  currentBlock->jumpTarget = exitBlockPtr;

  return true;
}
