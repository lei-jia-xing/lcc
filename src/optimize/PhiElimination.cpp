#include "optimize/PhiElimination.hpp"
#include "codegen/BasicBlock.hpp"
#include "codegen/Instruction.hpp"
#include <vector>

void PhiEliminationPass::run(Function &F) {
  for (auto &bbPtr : F.getBlocks()) {
    BasicBlock *bb = bbPtr.get();
    auto &insts = bb->getInstructions();

    std::vector<std::unique_ptr<Instruction>> phiNodes;

    for (auto it = insts.begin(); it != insts.end();) {
      if ((*it)->getOp() == OpCode::PHI) {
        phiNodes.push_back(std::move(*it));
        it = insts.erase(it);
      } else {
        ++it;
      }
    }

    if (phiNodes.empty()) {
      continue;
    }

    for (auto &phi : phiNodes) {
      Operand dest = phi->getResult();
      const auto &args = phi->getPhiArgs();

      for (auto &pair : args) {
        Operand src = pair.first;
        BasicBlock *pred = pair.second;
        if (!pred)
          continue;

        auto copy =
            std::make_unique<Instruction>(Instruction::MakeAssign(src, dest));

        auto &pInsts = pred->getInstructions();
        auto insertIt = pInsts.end();

        if (!pInsts.empty()) {
          auto it = pInsts.end();
          while (it != pInsts.begin()) {
            auto prev = std::prev(it);
            OpCode op = (*prev)->getOp();
            if (op == OpCode::GOTO || op == OpCode::RETURN) {
              it = prev;
            } else {
              break;
            }
          }
          insertIt = it;
        }
        // insert into predecessor block
        pInsts.insert(insertIt, std::move(copy));
      }
    }
  }
}
