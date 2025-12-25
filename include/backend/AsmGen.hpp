#pragma once
#include <backend/RegisterAllocator.hpp>
#include <codegen/Function.hpp>
#include <ostream>
#include <semantic/Symbol.hpp>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class IRModuleView
 * @brief all the ir module information needed for assembly generation
 *
 */
struct IRModuleView {
  std::vector<const Function *> functions;

  std::vector<const Instruction *> globals;
  std::unordered_map<std::string, std::shared_ptr<Symbol>> stringLiterals;
};

/**
 * @class RegDesc
 * @brief Register descriptor
 *
 */
struct RegDesc {
  std::string name;
  bool inUse = false;
  int tempId = -1;
};

class AsmGen {
public:
  AsmGen();
  /**
   * @brief entrance point for assembly generation
   *
   * @param mod IR module view
   * @param out output stream
   */
  void generate(const IRModuleView &mod, std::ostream &out);

  /**
   * @brief Allocate a scratch register for temporary use
   * @return Register name like "$t8" or "$t9", or empty if none available
   */
  std::string allocateScratch();

  /**
   * @brief Release a scratch register when done
   * @param reg The register to release (e.g., "$t8")
   */
  void releaseScratch(const std::string &reg);

  /**
   * @brief Reset scratch register state (called at instruction boundaries)
   */
  void resetScratchState();

private:
  /**
   * @brief emit data section
   *
   * @param mod ir module view
   * @param out mips assemble output stream
   */
  void emitDataSection(const IRModuleView &mod, std::ostream &out);
  /**
   * @brief emit text section
   *
   * @param mod ir module view
   * @param out mips assemble aoutput stream
   */
  void emitTextSection(const IRModuleView &mod, std::ostream &out);
  /**
   * @brief emit Function assembly
   *
   * @param func function to be emitted
   * @param out mips assemble output stream
   */
  void emitFunction(const Function *func, std::ostream &out);

  /**
   * @brief tranfer a instruction to assembly instructions
   *
   * @param inst instruction to be lowered
   * @param out mips assemble output stream
   */
  void lowerInstruction(const Instruction *inst, std::ostream &out);

  /**
   * @brief allocate a register for a temporary variable
   *
   * @param tempId the temporary variable id
   */
  std::string regForTemp(int tempId) const;

  /**
   * @brief unified register retrieval for any operand type
   *
   * @param op operand to get register for
   * @param out mips assemble output stream
   * @param forResult whether the constint is for result storage
   */
  std::string getRegister(const Operand &op, std::ostream &out);

  /**
   * @brief unified result storage for any operand type
   *
   * @param op operand to store from register
   * @param reg register containing value to store
   * @param out output stream
   */
  void storeResult(const Operand &op, const std::string &reg,
                   std::ostream &out);

  /**
   * @brief helper function to emit comments in the assembly output
   *
   * @param out output stream
   * @param txt content of the comment
   */
  void comment(std::ostream &out, const std::string &txt);

  /**
   * @brief inside fucntion block
   *
   * @param func function to be analyzed
   */
  void analyzeFunctionLocals(const Function *func);
  /**
   * @brief reset function state before processing a new function
   */
  void resetFunctionState();

private:
  /**
   * @brief Scratch register reference counting
   */
  struct ScratchRegState {
    std::string name;
    bool inUse = false;
  };
  /**
   * @brief remaining scratch registers
   */
  std::vector<ScratchRegState> scratchRegs_;

private:
  /**
   * @brief register allocator instance
   */
  RegisterAllocator _regAllocator;
  /**
   * @brief spill offsets for spilled temporary variables
   */
  std::map<int, int> _spillOffsets;
  /**
   * @brief flag to control comment emission
   */
  bool emitComments_ = true;
  std::vector<RegDesc> regs_;
  static const int NUM_ALLOCATABLE_REGS = 8; // $s0-$s7
  int paramIndex_ = 0;
  const IRModuleView *curMod_ = nullptr;
  std::string curFuncName_;

  /**
   * @class LocalInfo
   * @brief the local variable info in stack frame
   *
   */
  struct LocalInfo {
    int offset = -1;
    int size = 1;
  };
  std::unordered_map<const Symbol *, LocalInfo> locals_;
  /**
   * @brief at least 8 bytes for saved $ra and $fp
   */
  int frameSize_ = 0;
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
