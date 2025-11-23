#pragma once
#include <backend/RegisterAllocator.hpp>
#include <codegen/Function.hpp>
#include <ostream>
#include <semantic/Symbol.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct IRModuleView {
  std::vector<const Function *> functions;

  std::vector<const Instruction *> globals;
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
  void emitDataSection(const IRModuleView &mod, std::ostream &out);
  void emitTextSection(const IRModuleView &mod, std::ostream &out);
  void emitFunction(const Function *func, std::ostream &out);

  /**
   * @brief tranfer a instruction to assembly instructions
   *
   * @param inst instruction to be lowered
   * @param output stream
   */
  void lowerInstruction(const Instruction *inst, std::ostream &out);

  /**
   * @brief allocate a register for a temporary variable
   *
   * @param tempId the temporary variable id
   */
  std::string regForTemp(int tempId) const;
  std::string ensureInReg(const Operand &op, std::ostream &out,
                          const char *immScratch = "$t9",
                          const char *varScratch = "$t8");

  void comment(std::ostream &out, const std::string &txt);

  void analyzeGlobals(const IRModuleView &mod);
  void analyzeFunctionLocals(const Function *func);
  void resetFunctionState();

private:
  RegisterAllocator _regAllocator;
  std::map<int, int> _spillOffsets;
  bool emitComments_ = true;
  std::vector<RegDesc> regs_;
  int paramIndex_ = 0;
  const IRModuleView *curMod_ = nullptr;
  std::string curFuncName_;

  struct LocalInfo {
    int offset = -1;
    int size = 1;
  };
  std::unordered_map<const Symbol *, LocalInfo> locals_;
  int frameSize_ = 0; // 包含保存 $ra $fp 的槽位（最小 8）
  /**
   * @brief param index -> formal parameter variable name mapping for current
   */
  std::vector<const Symbol *> formalParamByIndex_;
  /**
   * @brief >=4th arguments pending to be stored to stack for current function
   */
  std::vector<Operand> pendingExtraArgs_;
  /**
   * @brief unified epilogue label for current function
   */
  std::string currentEpilogueLabel_;
};
