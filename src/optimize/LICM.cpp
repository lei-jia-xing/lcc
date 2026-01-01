#include "optimize/LICM.hpp"
#include "codegen/Instruction.hpp"
#include "optimize/DominatorTree.hpp"
#include "optimize/LoopAnalysis.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <vector>

static std::vector<BasicBlock *> getPredecessors(BasicBlock *BB, Function &F) {
  std::vector<BasicBlock *> predecessors;
  for (const auto &b_ptr : F.getBlocks()) {
    BasicBlock *pred = b_ptr.get();
    if (pred->next.get() == BB || pred->jumpTarget.get() == BB) {
      predecessors.push_back(pred);
    }
  }
  return predecessors;
}

BasicBlock *LICMPass::getOrCreatePreheader(const LoopInfo &loop, Function &F) {
  BasicBlock *header = loop.header;

  int headerLabelId = header->getLabelId();
  if (headerLabelId == -1) {
    headerLabelId = F.allocateLabel();
    auto labelInst = std::make_unique<Instruction>(
        Instruction::MakeLabel(Operand::Label(headerLabelId)));
    header->getInstructions().insert(header->getInstructions().begin(),
                                     std::move(labelInst));
  }
  auto predecessors = getPredecessors(header, F);

  std::vector<BasicBlock *> outsidePreds;
  for (auto *pred : predecessors) {
    if (loop.blocks.find(pred) == loop.blocks.end()) {
      outsidePreds.push_back(pred);
    }
  }
  // only one pred from outside the loop
  if (outsidePreds.size() == 1) {
    BasicBlock *pred = outsidePreds[0];

    bool uniqueSuccessor = false;
    if (pred->jumpTarget.get() == header && pred->next == nullptr) {
      uniqueSuccessor = true;
    } else if (pred->next.get() == header && pred->jumpTarget == nullptr) {
      uniqueSuccessor = true;
    } else if (pred->jumpTarget.get() == header && pred->next.get() == header) {
      uniqueSuccessor = true;
    }
    if (uniqueSuccessor) {
      return pred;
    }
  }

  // need to create a new preheader
  auto newBlockPtr = F.createBlock();
  BasicBlock *preheader = newBlockPtr.get();
  // allocate new label for new preheader
  int preHeaderLabelId = F.allocateLabel();
  preheader->addInstruction(std::make_unique<Instruction>(
      Instruction::MakeLabel(Operand::Label(preHeaderLabelId))));
  preheader->addInstruction(std::make_unique<Instruction>(
      Instruction::MakeGoto(Operand::Label(header->getLabelId()))));
  preheader->jumpTarget = F.getBlockSharedPtr(header);

  int targetLabelId = header->getLabelId();
  for (BasicBlock *pred : outsidePreds) {
    if (pred->jumpTarget.get() == header) {
      pred->jumpTarget = F.getBlockSharedPtr(preheader);

      // modify jump target label
      Instruction *term = pred->getInstructions().back().get();
      if (term->getOp() == OpCode::GOTO || term->getOp() == OpCode::IF) {
        if (term->getResult().getType() == OperandType::Label &&
            term->getResult().asInt() == targetLabelId) {
          term->setResult(Operand::Label(preHeaderLabelId));
        } else if (term->getArg1().getType() == OperandType::Label &&
                   term->getArg1().asInt() == targetLabelId) {
          term->setArg1(Operand::Label(preHeaderLabelId));
        } else if (term->getArg2().getType() == OperandType::Label &&
                   term->getArg2().asInt() == targetLabelId) {
          term->setArg2(Operand::Label(preHeaderLabelId));
        }
      }
    }
    if (pred->next.get() == header) {
      pred->next = F.getBlockSharedPtr(preheader);

      if (pred->getInstructions().empty() ||
          pred->getInstructions().back()->getOp() != OpCode::GOTO) {
        pred->addInstruction(std::make_unique<Instruction>(
            Instruction::MakeGoto(Operand::Label(preHeaderLabelId))));
      }
    }
  }

  // update phi node in header
  for (auto &inst : header->getInstructions()) {
    if (inst->getOp() != OpCode::PHI) {
      break;
    }
    std::vector<std::pair<Operand, BasicBlock *>> outsideIncoming;
    std::vector<std::pair<Operand, BasicBlock *>> loopIncoming;
    auto &phiArgs = inst->getPhiArgs();

    for (auto it = phiArgs.begin(); it != phiArgs.end();) {
      BasicBlock *incomingBlock = it->second;
      if (loop.blocks.count(incomingBlock)) {
        loopIncoming.push_back(*it);
        it++;
      } else {
        outsideIncoming.push_back(*it);
        it = phiArgs.erase(it);
      }
    }
    Operand mergedVal;
    if (outsideIncoming.empty()) {
      continue;
    } else if (outsideIncoming.size() == 1) {
      mergedVal = outsideIncoming[0].first;
    } else {
      // multi phi sources, need to create a new phi in preheader
      mergedVal = Operand::Temporary(F.allocateTemp());
      auto newPhi =
          std::make_unique<Instruction>(Instruction::MakePhi(mergedVal));

      for (auto &pair : outsideIncoming) {
        newPhi->addPhiArg(pair.first, pair.second);
      }
      auto insertIt = preheader->getInstructions().begin();
      while (insertIt != preheader->getInstructions().end() &&
             (*insertIt)->getOp() == OpCode::LABEL) {
        insertIt++;
      }
      preheader->getInstructions().insert(insertIt, std::move(newPhi));
    }
    inst->addPhiArg(mergedVal, preheader);
  }
  return preheader;
}

static bool isSafeToSpeculate(OpCode op) {
  if (op == OpCode::DIV || op == OpCode::MOD) {
    return false;
  }
  return true;
}

bool LICMPass::isLoopInvariant(const Instruction *inst, const LoopInfo &loop,
                               const std::map<int, DefInfo> &defMap,
                               const std::set<const Instruction *> &invariants,
                               const std::set<int> &modifiedVars, bool hasCall,
                               DominatorTree &DT) {
  OpCode op = inst->getOp();
  if (op == OpCode::STORE || op == OpCode::CALL || op == OpCode::RETURN ||
      op == OpCode::IF || op == OpCode::GOTO || op == OpCode::LABEL ||
      op == OpCode::ALLOCA || op == OpCode::PHI || op == OpCode::LOAD ||
      op == OpCode::ARG || op == OpCode::PARAM) {
    return false;
  }
  // ban any instruction that defines a variable
  if (inst->getResult().getType() == OperandType::Variable) {
    return false;
  }
  if (!isSafeToSpeculate(op)) {
    BasicBlock *bb = inst->getParent();
    return false;
  }
  auto checkOp = [&](const Operand &op) -> bool {
    if (op.getType() == OperandType::Variable) {
      if (op.asSymbol() && modifiedVars.count(op.asSymbol()->id)) {
        return false;
      }
      if (hasCall)
        return false;
      return true;
    }
    if (op.getType() != OperandType::Temporary) {
      return true;
    }
    auto it = defMap.find(op.asInt());

    if (it == defMap.end())
      return true;
    BasicBlock *defBlock = it->second.block;
    if (loop.blocks.count(defBlock)) {
      if (invariants.count(it->second.inst)) {
        return true;
      }
      return false;
    }

    if (DT.dominates(defBlock, loop.header)) {
      return true;
    }
    return false;
  };

  if (!checkOp(inst->getArg1()))
    return false;
  if (!checkOp(inst->getArg2()))
    return false;

  return true;
}
void LICMPass::run(Function &F, DominatorTree &DT,
                   const std::vector<LoopInfo> &loops) {
  if (loops.empty())
    return;
  std::map<int, DefInfo> defInfoMap;

  for (auto &bb_ptr : F.getBlocks()) {
    for (auto &inst : bb_ptr->getInstructions()) {
      const Operand &res = inst->getResult();
      if (res.getType() == OperandType::Temporary) {
        defInfoMap[res.asInt()] = {inst.get(), bb_ptr.get()};
      }
    }
  }

  for (auto &loop : loops) {
    BasicBlock *preheader = getOrCreatePreheader(loop, F);
    if (!preheader) {
      continue;
    }
    // CFG changes, update dom
    DT.run(F);

    for (auto &inst : preheader->getInstructions()) {
      const Operand &res = inst->getResult();
      if (res.getType() == OperandType::Temporary) {
        defInfoMap[res.asInt()] = {inst.get(), preheader};
      }
    }
    std::set<int> modifiedVars;
    // flag for function call inside the loop
    bool hasCall = false;
    for (BasicBlock *bb : loop.blocks) {
      for (auto &inst : bb->getInstructions()) {
        if (inst->getOp() == OpCode::CALL) {
          hasCall = true;
        }
        const Operand &res = inst->getResult();
        if (res.getType() == OperandType::Variable && res.asSymbol()) {
          modifiedVars.insert(res.asSymbol()->id);
        }

        if (inst->getOp() == OpCode::STORE) {
          const Operand &base = inst->getArg2();
          if (base.getType() == OperandType::Variable && base.asSymbol()) {
            modifiedVars.insert(base.asSymbol()->id);
          }
        }
      }
    }
    std::set<const Instruction *> invariantInstructions;
    std::vector<const Instruction *> orderedInvariants;
    bool changed = true;
    // iteratively find loop-invariant instructions
    while (changed) {
      changed = false;
      for (BasicBlock *bb : loop.blocks) {
        for (auto &inst_ptr : bb->getInstructions()) {
          Instruction *inst = inst_ptr.get();

          if (invariantInstructions.count(inst))
            continue;

          if (isLoopInvariant(inst, loop, defInfoMap, invariantInstructions,
                              modifiedVars, hasCall, DT)) {
            invariantInstructions.insert(inst);
            orderedInvariants.push_back(inst);
            changed = true;
          }
        }
      }
    }
    if (invariantInstructions.empty()) {
      continue;
    }
    // hoisting
    std::vector<std::unique_ptr<Instruction>> toMove;

    for (BasicBlock *bb : loop.blocks) {
      auto &insts = bb->getInstructions();
      for (auto it = insts.begin(); it != insts.end();) {
        if (invariantInstructions.count(it->get())) {
          toMove.push_back(std::move(*it));
          it = insts.erase(it);
        } else {
          ++it;
        }
      }
    }

    std::map<const Instruction *, int> orderMap;
    for (size_t i = 0; i < orderedInvariants.size(); i++) {
      orderMap[orderedInvariants[i]] = i;
    }
    std::sort(toMove.begin(), toMove.end(),
              [&](const std::unique_ptr<Instruction> &a,
                  const std::unique_ptr<Instruction> &b) {
                return orderMap[a.get()] < orderMap[b.get()];
              });

    auto &preInsts = preheader->getInstructions();

    size_t insertIdx = preInsts.size();
    if (!preInsts.empty() && (preInsts.back()->getOp() == OpCode::GOTO ||
                              preInsts.back()->getOp() == OpCode::IF)) {
      insertIdx--;
    }
    for (auto &inst : toMove) {
      if (inst->getResult().getType() == OperandType::Temporary) {
        defInfoMap[inst->getResult().asInt()] = {inst.get(), preheader};
      }
      preInsts.insert(preInsts.begin() + insertIdx, std::move(inst));
      insertIdx++;
    }
  }
}
