#include "optimize/PhiElimination.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Instruction.hpp"
#include <map>
#include <vector>

void PhiEliminationPass::run(Function &F) {
  // analyze CFG
  std::map<BasicBlock *, int> predCounts;
  std::map<BasicBlock *, int> succCounts;
  std::map<BasicBlock *, int> blockLabelId;
  for (auto &blk : F.getBlocks()) {
    BasicBlock *bb = blk.get();
    if (!bb->getInstructions().empty()) {
      Instruction *firstInst = bb->getInstructions().front().get();
      if (firstInst->getOp() == OpCode::LABEL) {
        blockLabelId[bb] = firstInst->getResult().asInt();
      }
    }
    if (bb->jumpTarget) {
      succCounts[bb]++;
      predCounts[bb->jumpTarget.get()]++;
    }
    if (bb->next) {
      succCounts[bb]++;
      predCounts[bb->next.get()]++;
    }
  }
  // collect all parallel copies required by phi nodes
  // {Predecessor, Successor} -> [{dest, src}, ...]
  std::map<std::pair<BasicBlock *, BasicBlock *>,
           std::vector<std::pair<Operand, Operand>>>
      edgeCopies;
  for (auto &blk : F.getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      if (inst->getOp() == OpCode::PHI) {
        Operand dest = inst->getResult();
        for (auto &pair : inst->getPhiArgs()) {
          edgeCopies[{pair.second, blk.get()}].emplace_back(dest, pair.first);
        }
        inst->setOp(OpCode::NOP);
      }
    }
  }
  // process edges & split critical edges
  for (auto &[edge, copies] : edgeCopies) {
    if (copies.empty()) {
      continue;
    }
    BasicBlock *pred = edge.first;
    BasicBlock *succ = edge.second;

    bool isCritical = (succCounts[pred] > 1) && (predCounts[succ] > 1);
    BasicBlock *insertBlock = nullptr;
    bool appendToEnd = false;

    if (!isCritical) {
      if (predCounts[succ] == 1) {
        insertBlock = succ;
        appendToEnd = false;
      } else {
        insertBlock = pred;
        appendToEnd = true;
      }
    } else {
      auto midBlockPtr = F.createBlock();
      insertBlock = midBlockPtr.get();
      int midLabelId = F.allocateLabel();
      int succLabelId;
      if (blockLabelId.count(succ)) {
        succLabelId = blockLabelId[succ];
      } else {
        succLabelId = F.allocateLabel();
        blockLabelId[succ] = succLabelId;
        succ->getInstructions().insert(
            succ->getInstructions().begin(),
            std::make_unique<Instruction>(
                Instruction::MakeLabel(Operand::Label(succLabelId))));
      }

      insertBlock->addInstruction(std::make_unique<Instruction>(
          Instruction::MakeLabel(Operand::Label(midLabelId))));
      insertBlock->addInstruction(std::make_unique<Instruction>(
          Instruction::MakeGoto(Operand::Label(succLabelId))));
      auto succShared = getBlockSharedPtr(F, succ);
      bool redirected = false;
      if (pred->jumpTarget.get() == succ) {
        pred->jumpTarget = midBlockPtr;
        redirected = true;

        auto &insts = pred->getInstructions();
        if (!insts.empty()) {
          Instruction *last = insts.back().get();
          if (last->getOp() == OpCode::GOTO || last->getOp() == OpCode::IF) {
            if (last->getResult().getType() == OperandType::Label &&
                last->getResult().asInt() == succLabelId) {
              last->setResult(Operand::Label(midLabelId));
            } else if (last->getArg1().getType() == OperandType::Label &&
                       last->getArg1().asInt() == succLabelId) {
              last->setArg1(Operand::Label(midLabelId));
            } else if (last->getArg2().getType() == OperandType::Label &&
                       last->getArg2().asInt() == succLabelId) {
              last->setArg2(Operand::Label(midLabelId));
            }
          }
        }
      }
      if (pred->next.get() == succ) {
        pred->next = midBlockPtr;
        redirected = true;

        pred->addInstruction(std::make_unique<Instruction>(
            Instruction::MakeGoto(Operand::Label(midLabelId))));
      }
      if (redirected) {
        insertBlock->jumpTarget = succShared;
      }

      appendToEnd = false;
    }
    std::vector<std::unique_ptr<Instruction>> copyInsts;
    std::vector<Operand> temps;

    for (auto &copy : copies) {
      Operand t = Operand::Temporary(F.allocateTemp());
      temps.push_back(t);
      copyInsts.push_back(std::make_unique<Instruction>(
          Instruction::MakeAssign(copy.second, t)));
    }

    for (size_t i = 0; i < copies.size(); i++) {
      copyInsts.push_back(std::make_unique<Instruction>(
          Instruction::MakeAssign(temps[i], copies[i].first)));
    }
    auto &targetInsts = insertBlock->getInstructions();
    auto it = targetInsts.begin();
    if (appendToEnd) {
      it = targetInsts.end();
      if (!targetInsts.empty()) {
        Instruction *last = targetInsts.back().get();
        if (last->getOp() == OpCode::GOTO || last->getOp() == OpCode::IF ||
            last->getOp() == OpCode::RETURN) {
          it--;
        }
      }
    } else {
      it = targetInsts.begin();
      while (it != targetInsts.end() && (*it)->getOp() == OpCode::LABEL) {
        ++it;
      }
    }

    for (auto &inst : copyInsts) {
      it = targetInsts.insert(it, std::move(inst));
      it++;
    }
  }
}

std::shared_ptr<BasicBlock>
PhiEliminationPass::getBlockSharedPtr(Function &F, BasicBlock *rawPtr) {
  for (auto &sp : F.getBlocks()) {
    if (sp.get() == rawPtr) {
      return sp;
    }
  }
  return nullptr;
}
