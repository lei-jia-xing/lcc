#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <semantic/Symbol.hpp>

namespace lcc {
namespace codegen {
class Function;
class BasicBlock;
class Instruction;
class Operand;
} // namespace codegen
} // namespace lcc

namespace lcc::backend {

struct IRModuleView {
  std::vector<const codegen::Function *> functions;
  std::vector<const codegen::Instruction *> globals;
  std::unordered_map<std::string, std::string> stringLiterals;
};

struct RegDesc {
  std::string name;
  bool inUse = false;
  int tempId = -1;
};

class AsmGen {
public:
  AsmGen();
  void generate(const IRModuleView &mod, std::ostream &out);

private:
  void emitHeader(std::ostream &out);
  void emitDataSection(const IRModuleView &mod, std::ostream &out);
  void emitTextSection(const IRModuleView &mod, std::ostream &out);
  void emitFunction(const codegen::Function *func, std::ostream &out);

  void lowerInstruction(const codegen::Instruction *inst, std::ostream &out);

  std::string regForTemp(int tempId) const;
  std::string ensureInReg(const codegen::Operand &op, std::ostream &out,
                          const char *immScratch = "$t9",
                          const char *varScratch = "$t8");

  int acquireTempRegister(int tempId);
  void releaseTempRegister(int tempId);
  const RegDesc &reg(int idx) const { return regs_[idx]; }
  RegDesc &reg(int idx) { return regs_[idx]; }

  void comment(std::ostream &out, const std::string &txt);

  void analyzeGlobals(const IRModuleView &mod);
  void analyzeFunctionLocals(const codegen::Function *func);
  void resetFunctionState();

private:
  bool emitComments_ = true;
  std::vector<RegDesc> regs_;
  int paramIndex_ = 0;
  const IRModuleView *curMod_ = nullptr;
  std::string curFuncName_;

  std::unordered_set<std::string> globals_;
  std::unordered_map<std::string, int> globalSizes_;
  struct LocalInfo {
    int offset = -1; // 相对 $sp 的正偏移
    int size = 1;    // 以 word 为单位
  };
  // Key locals by Symbol* instead of name to avoid shadowing conflicts.
  std::unordered_map<const Symbol *, LocalInfo> locals_;
  int frameSize_ = 0; // 包含保存 $ra 的槽位（最小 4）
  /**
   * @brief param index -> formal parameter variable name mapping for current
   */
  std::vector<const Symbol *> formalParamByIndex_;
  /**
   * @brief >=4th arguments pending to be stored to stack for current function
   */
  std::vector<codegen::Operand> pendingExtraArgs_;
  /**
   * @brief unified epilogue label for current function
   */
  std::string currentEpilogueLabel_;
};

} // namespace lcc::backend
