#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

  // Instruction lowering (stubs for now)
  void lowerInstruction(const codegen::Instruction *inst, std::ostream &out);

  // Operand/register helpers
  std::string regForTemp(int tempId) const;
  // Ensure an operand value is in a register and return that register name.
  // May emit li/la+lw 指令以装载常量或变量值。默认使用 $t9/$t8 作为临时。
  std::string ensureInReg(const codegen::Operand &op, std::ostream &out,
                          const char *immScratch = "$t9",
                          const char *varScratch = "$t8");

  // Register allocation (placeholder: linear scan of temporaries)
  int acquireTempRegister(int tempId); // returns register index
  void releaseTempRegister(int tempId);
  const RegDesc &reg(int idx) const { return regs_[idx]; }
  RegDesc &reg(int idx) { return regs_[idx]; }

  // Helpers
  void comment(std::ostream &out, const std::string &txt);

  // Function-local analysis
  void analyzeGlobals(const IRModuleView &mod);
  void analyzeFunctionLocals(const codegen::Function *func);
  void resetFunctionState();

private:
  bool emitComments_ = true;
  std::vector<RegDesc> regs_; // $t0-$t9 pool for temporaries
  // CALL 参数缓冲管理（简化版，仅处理 <=4 参数寄存器路径）
  int paramIndex_ = 0;                   // 0..3 -> $a0..$a3
  const IRModuleView *curMod_ = nullptr; // 提供字符串表等
  std::string curFuncName_;              // 当前函数名，用于唯一化本地标签

  // Globals set for quick check
  std::unordered_set<std::string> globals_;
  // Per-function locals: name -> {offset, size}
  struct LocalInfo {
    int offset = -1; // 相对 $sp 的正偏移
    int size = 1;    // 以 word 为单位
  };
  std::unordered_map<std::string, LocalInfo> locals_;
  int frameSize_ = 0; // 包含保存 $ra 的槽位（最小 4）
  // Formal parameter binding: index -> var name (记录所有)
  std::vector<std::string> formalParamByIndex_;
  // Pending extra (>=4) call-site arguments collected until CALL
  std::vector<codegen::Operand> pendingExtraArgs_;
};

} // namespace lcc::backend
