#include "backend/AsmGen.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "semantic/Symbol.hpp"
#include "semantic/Type.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

struct MagicInfo {
  int multiplier;
  int shift;
};

static MagicInfo computeMagic(int d) {
  if (d == 0 || d == 1 || d == -1)
    return {1, 0};

  unsigned int ad = std::abs(d);
  unsigned long long t = 1ull << 31;
  unsigned long long anc = t - 1 - (t % ad);
  unsigned long long p = 31;
  unsigned long long q1 = t / ad;
  unsigned long long r1 = t % ad;
  unsigned long long q2 = t / ad;
  unsigned long long r2 = t % ad;
  unsigned long long delta;

  do {
    p++;
    q1 = 2 * q1;
    r1 = 2 * r1;
    if (r1 >= ad) {
      q1++;
      r1 -= ad;
    }
    q2 = 2 * q2;
    r2 = 2 * r2;
    if (r2 >= ad) {
      q2++;
      r2 -= ad;
    }
    delta = ad - 1 - r2;
  } while (q1 < delta || (q1 == delta && r1 == 0));

  MagicInfo info;
  info.multiplier = (int)(q2 + 1);
  info.shift = (int)(p - 32);
  return info;
}
// Check if n is a power of 2 and return the exponent, or -1 if not
static int log2IfPowerOf2(int n) {
  if (n <= 0)
    return -1;
  if ((n & (n - 1)) != 0)
    return -1; // Not a power of 2
  int exp = 0;
  while ((n >> exp) != 1)
    ++exp;
  return exp;
}

static const char *aregs[4] = {"$a0", "$a1", "$a2", "$a3"};
AsmGen::AsmGen() {
  static const char *Regs[NUM_ALLOCATABLE_REGS] = {"$s0", "$s1", "$s2", "$s3",
                                                   "$s4", "$s5", "$s6", "$s7"};
  regs_.reserve(NUM_ALLOCATABLE_REGS);
  for (int i = 0; i < NUM_ALLOCATABLE_REGS; ++i) {
    regs_.push_back({Regs[i], false, -1});
  }

  scratchRegs_.push_back({"$t0", 0});
  scratchRegs_.push_back({"$t1", 0});
  scratchRegs_.push_back({"$t2", 0});
  scratchRegs_.push_back({"$t3", 0});
  scratchRegs_.push_back({"$t4", 0});
  scratchRegs_.push_back({"$t5", 0});
  scratchRegs_.push_back({"$t6", 0});
  scratchRegs_.push_back({"$t7", 0});
  scratchRegs_.push_back({"$t8", 0});
  scratchRegs_.push_back({"$t9", 0});
}

std::string AsmGen::allocateScratch() {
  for (auto &scratch : scratchRegs_) {
    if (scratch.inUse == false) {
      scratch.inUse = true;
      return scratch.name;
    }
  }
  return "$zero"; // indicate no scratch available
}

void AsmGen::resetScratchState() {
  for (auto &scratch : scratchRegs_) {
    scratch.inUse = false;
  }
}

void AsmGen::generate(const IRModuleView &mod, std::ostream &out) {
  curMod_ = &mod;
  paramIndex_ = 0;
  emitDataSection(mod, out);
  emitTextSection(mod, out);
  curMod_ = nullptr;
}

void AsmGen::emitDataSection(const IRModuleView &mod, std::ostream &out) {
  out << ".data\n";
  for (auto &kv : mod.stringLiterals) {
    const std::string &literal = kv.first;
    const std::string &label = kv.second->name;
    size_t start = 0, end = literal.size();
    if (end >= 2 && literal.front() == '"' && literal.back() == '"') {
      start = 1;
      end -= 1;
    }
    out << label << ": .asciiz \"";
    for (size_t i = start; i < end; ++i) {
      char c = literal[i];
      out << c;
    }
    out << "\"\n";
  }
  struct GInfo {
    int size = 0;
    std::vector<int> inits;
    bool defined = false;
    const Symbol *sym = nullptr; // Store symbol pointer
  };
  // Use Symbol pointer as key instead of string name
  std::unordered_map<const Symbol *, GInfo> gmap;

  auto recordDef = [&](const Symbol *sym, int size) {
    auto &gi = gmap[sym];
    if (!gi.defined) {
      gi.size = size;
      gi.inits.assign(size, 0);
      gi.defined = true;
      gi.sym = sym;
    }
  };
  auto setVal = [&](const Symbol *sym, int idx, int val) {
    auto it = gmap.find(sym);
    if (it != gmap.end() && idx >= 0 && idx < it->second.inits.size()) {
      it->second.inits[idx] = val;
    }
  };
  for (size_t i = 0; i < mod.globals.size(); ++i) {
    const Instruction *inst = mod.globals[i];
    switch (inst->getOp()) {
    case OpCode::ALLOCA: {
      // ALLOCA var(var), -, size(var|temp|const)
      // global variable should be evaluated in compile time, so res must be
      // constint
      auto arg1 = inst->getArg1().getType();
      auto res = inst->getResult().getType();
      if (arg1 == OperandType::Variable && res == OperandType::ConstantInt) {
        const Symbol *sym = inst->getArg1().asSymbol().get();
        int sz = inst->getResult().asInt();
        recordDef(sym, sz);
      }
      break;
    }
    case OpCode::ASSIGN: {
      // ASSIGN src(var|temp|const), -, dst(var)
      const Operand &src = inst->getArg1();
      const Operand &dst = inst->getResult();
      if (src.getType() == OperandType::ConstantInt &&
          dst.getType() == OperandType::Variable) {
        const Symbol *sym = dst.asSymbol().get();
        setVal(sym, 0, src.asInt());
      }
      break;
    }
    case OpCode::STORE: {
      // STORE value(var|temp|const), base(var), index(var|temp|const)
      const Operand &val = inst->getArg1();
      const Operand &base = inst->getArg2();
      const Operand &idx = inst->getResult();
      if (val.getType() == OperandType::ConstantInt &&
          base.getType() == OperandType::Variable &&
          idx.getType() == OperandType::ConstantInt) {
        const Symbol *sym = base.asSymbol().get();
        setVal(sym, idx.asInt(), val.asInt());
      }
      break;
    }
    default:
      break;
    }
  }

  for (auto &kv : gmap) {
    const Symbol *sym = kv.first;
    const GInfo &gi = kv.second;
    if (!gi.defined)
      continue;
    // Use global unique name if available, otherwise original name
    out << (sym->globalName.empty() ? sym->name : sym->globalName)
        << ": .word ";
    for (int i = 0; i < gi.size; i++) {
      out << gi.inits[i];
      if (i + 1 < gi.size)
        out << ",";
    }
    out << "\n";
  }
  out << "\n";
}

void AsmGen::emitTextSection(const IRModuleView &mod, std::ostream &out) {
  out << ".text\n";
  for (auto *func : mod.functions) {
    out << ".globl " << func->getName() << "\n";
  }
  const Function *mainFunc = nullptr;
  // special handle main function first
  for (auto *func : mod.functions) {
    if (func && func->getName() == std::string("main")) {
      mainFunc = func;
      break;
    }
  }
  if (mainFunc)
    emitFunction(mainFunc, out);
  for (auto *func : mod.functions) {
    if (mainFunc && func->getName() == std::string("main"))
      continue;
    emitFunction(func, out);
  }
  out << "printf:\n";
  out << "  addiu $sp, $sp, -16\n";
  out << "  sw $a1, 4($sp)\n";
  out << "  sw $a2, 8($sp)\n";
  out << "  sw $a3, 12($sp)\n";
  out << "  move $t6, $a0          # t6: format string cursor\n";
  out << "  addiu $t7, $sp, 4      # t7: current argument pointer (starts at "
         "$a1)\n";
  out << "\n";
  out << "printf_loop:\n";
  out << "  lbu $a0, 0($t6)\n";
  out << "  beq $a0, $zero, printf_end\n";
  out << "  addiu $t6, $t6, 1\n"; // move cursor
  out << "  li $t8, 37         # 37 is '%'\n";
  out << "  beq $a0, $t8, printf_format\n";
  out << "  li $v0, 11\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "\n";
  out << "printf_format:\n";
  out << "  lbu $a0, 0($t6)\n";
  out << "  addiu $t6, $t6, 1\n";
  out << "  li $t8, 100        # 100 is 'd'\n";
  out << "  beq $a0, $t8, printf_int\n";
  out << "  li $v0, 11\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "\n";
  out << "printf_int:\n";
  out << "  lw $a0, 0($t7)     # Load arg from stack using t7\n";
  out << "  addiu $t7, $t7, 4  # Move arg pointer to next\n";
  out << "  li $v0, 1\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "\n";
  out << "printf_end:\n";
  out << "  addiu $sp, $sp, 16\n";
  out << "  jr $ra\n\n";
  out << "getint:\n";
  out << "  li $v0, 5\n";
  out << "  syscall\n";
  out << "  jr $ra\n\n";
}

void AsmGen::emitFunction(const Function *func, std::ostream &out) {
  /*
   * Caller's stack frame layout
   * Caller's extra params
   * used callee-saved registers
   * Spilled temp
   * local variables
   * $fp
   * $ra
   */
  curFuncName_ = func->getName();
  resetFunctionState();

  // detect leaf function
  bool isLeaf = true;
  for (auto &blk : func->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      if (inst->getOp() == OpCode::CALL) {
        isLeaf = false;
        break;
      }
    }
    if (!isLeaf)
      break;
  }
  if (func->getName() == "main")
    isLeaf = false;

  analyzeFunctionLocals(func);

  // if leaf function, no need to save $ra
  if (isLeaf) {
    frameSize_ -= 4;
    for (auto &kv : locals_) {
      kv.second.offset -= 4;
    }
  }

  _regAllocator.run(const_cast<Function *>(func));

  // just save all used callee-saved registers
  std::vector<int> calleeSavedRegs;
  const std::set<int> &usedRegs = _regAllocator.getUsedRegs();
  for (int r : usedRegs) {
    calleeSavedRegs.push_back(r);
  }

  int spillBaseOffset = frameSize_;
  const auto &spilled = _regAllocator.getSpilledNodes();
  for (int tempId : spilled) {
    _spillOffsets[tempId] = spillBaseOffset;
    spillBaseOffset += 4;
  }
  frameSize_ = spillBaseOffset;

  int savedRegBase = frameSize_;

  int savedRegSize = calleeSavedRegs.size() * 4;
  frameSize_ += savedRegSize;

  out << func->getName() << ":\n";
  if (frameSize_ > 32767) {
    out << "  li $t6, -" << frameSize_ << "\n";
    out << "  addu $sp, $sp, $t6\n" << "\n";
  } else {
    out << "  addiu $sp, $sp, -" << frameSize_ << "\n";
  }
  if (isLeaf) {
    out << "  sw $fp, 0($sp)\n";
  } else {
    out << "  sw $ra, 0($sp)\n";
    out << "  sw $fp, 4($sp)\n";
  }

  for (size_t i = 0; i < calleeSavedRegs.size(); ++i) {
    int regId = calleeSavedRegs[i];
    int offset = savedRegBase + i * 4;
    if (offset >= -32768 && offset <= 32767) {
      out << "  sw " << regs_[regId].name << ", " << offset << "($sp)\n";
    } else {
      out << "  li $t6, " << offset << "\n";
      out << "  addu $t6, $sp, $t6\n";
      out << "  sw " << regs_[regId].name << ", 0($t6)\n";
    }
  }
  out << "  move $fp, $sp\n";

  size_t fsz = formalParamByIndex_.size();
  for (size_t i = 0; i < fsz && i < 4; ++i) {
    const Symbol *sym = formalParamByIndex_[i];
    auto it = locals_.find(sym);
    if (it != locals_.end()) {
      int off = it->second.offset;
      if (off >= -32768 && off <= 32767) {
        out << "  sw " << aregs[i] << ", " << off << "($fp)\n";
      } else {
        out << "  li $t6, " << off << "\n";
        out << "  addu $t6, $fp, $t6\n";
        out << "  sw " << aregs[i] << ", 0($t7)\n";
      }
    }
  }
  for (size_t i = 4; i < fsz; ++i) {
    const Symbol *sym = formalParamByIndex_[i];
    if (!sym)
      continue;
    auto it = locals_.find(sym);
    if (it == locals_.end())
      continue;
    int offLocal = it->second.offset;

    int offCaller =
        frameSize_ + (i - 4) * 4; // jump out current frame to the previous
                                  // frame's stack to get extra params
    if (offCaller >= -32768 && offCaller <= 32767) {
      out << "  lw $t6, " << offCaller << "($fp)\n";
    } else {
      out << "  li $t7, " << offCaller << "\n";
      out << "  addu $t7, $fp, $t7\n";
      out << "  lw $t6, 0($t7)\n";
    }
    if (offLocal >= -32768 && offLocal <= 32767) {
      out << "  sw $t6, " << offLocal << "($fp)\n";
    } else {
      out << "  li $t7, " << offLocal << "\n";
      out << "  addu $t7, $fp, $t7\n";
      out << "  sw $t6, 0($t7)\n";
    }
  }
  currentEpilogueLabel_ = func->getName() + std::string("_END");

  if (func->getName() == "main") {
    for (auto *inst : curMod_->globals) {
      bool isConstInit = false;
      // some variable may be evaluated in runtime, unlike c
      if (inst->getOp() == OpCode::ALLOCA)
        continue;
      if (inst->getOp() == OpCode::ASSIGN) {
        if (inst->getArg1().getType() == OperandType::ConstantInt) {
          isConstInit = true;
        }
      }
      if (inst->getOp() == OpCode::STORE) {
        if (inst->getArg1().getType() == OperandType::ConstantInt &&
            inst->getResult().getType() == OperandType::ConstantInt) {
          isConstInit = true;
        }
      }
      if (!isConstInit) {
        lowerInstruction(inst, out);
      }
    }
  }
  for (auto &blk : func->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      lowerInstruction(inst.get(), out);
    }
  }
  out << currentEpilogueLabel_ << ":\n";
  for (size_t i = 0; i < calleeSavedRegs.size(); ++i) {
    int regId = calleeSavedRegs[i];
    int offset = savedRegBase + i * 4;
    if (offset >= -32768 && offset <= 32767) {
      out << "  lw " << regs_[regId].name << ", " << offset << "($sp)\n";
    } else {
      out << "  li $t6, " << offset << "\n";
      out << "  addu $t6, $sp, $t6\n";
      out << "  lw " << regs_[regId].name << ", 0($t6)\n";
    }
  }
  // leaf function no need to resume $ra
  if (isLeaf) {
    out << "  lw $fp, 0($fp)\n";
  } else {
    out << "  lw $ra, 0($fp)\n";
    out << "  lw $fp, 4($fp)\n";
  }
  if (frameSize_ > 32767) {
    out << "  li $t6, " << frameSize_ << "\n";
    out << "  addu $sp, $sp, $t6\n";
  } else {
    out << "  addiu $sp, $sp, " << frameSize_ << "\n";
  }
  if (func->getName() == "main") {
    out << "  move $a0, $v0\n";
    out << "  li $v0, 17\n";
    out << "  syscall\n";
  } else {
    out << "  jr $ra\n";
  }
  out << "\n";
  curFuncName_.clear();
  currentEpilogueLabel_.clear();
}

void AsmGen::lowerInstruction(const Instruction *inst, std::ostream &out) {
  resetScratchState();

  auto op = inst->getOp();
  auto &a1 = inst->getArg1();
  auto &a2 = inst->getArg2();
  auto &res = inst->getResult();

  auto isConst = [&](const Operand &o) {
    return o.getType() == OperandType::ConstantInt;
  };
  auto isTemp = [&](const Operand &o) {
    return o.getType() == OperandType::Temporary;
  };
  auto isVar = [&](const Operand &o) {
    return o.getType() == OperandType::Variable;
  };
  auto isLabel = [&](const Operand &o) {
    return o.getType() == OperandType::Label;
  };
  auto isEmpty = [&](const Operand &o) {
    return o.getType() == OperandType::Empty;
  };

  auto labelName = [&](const Operand &o) {
    return curFuncName_ + std::string("_L") + std::to_string(o.asInt());
  };

  auto storeToSpill = [&](int tempId, const std::string &reg) {
    // store in stack frame when register is spilled
    if (_spillOffsets.count(tempId)) {
      int offset = _spillOffsets[tempId];
      if (offset >= -32768 && offset <= 32767) {
        out << "  sw " << reg << ", " << offset << "($fp)\n";
      } else {
        std::string addrReg = allocateScratch();
        out << "  li " << addrReg << ", " << offset << "\n";
        out << "  addu " << addrReg << ", $fp, " << addrReg << "\n";
        out << "  sw " << reg << ", 0(" << addrReg << ")\n";
        releaseScratch(addrReg);
      }
    }
  };
  auto getResultReg = [&](const Operand &r) -> std::string {
    // allocate register according to Operand r
    if (isTemp(r)) {
      if (_spillOffsets.count(r.asInt())) {
        return allocateScratch();
      }
      int regIdx = _regAllocator.getReg(r.asInt());
      if (regIdx != -1) {
        return regForTemp(r.asInt());
      }
      return allocateScratch();
    }
    return allocateScratch();
  };

  switch (op) {
  case OpCode::LABEL: {
    if (isLabel(res)) {
      out << labelName(res) << ":\n";
    }
    break;
  }
  case OpCode::GOTO: {
    if (isLabel(res)) {
      out << "  j " << labelName(res) << "\n";
    }
    break;
  }
  case OpCode::IF: {
    std::string rcond = getRegister(a1, out);
    out << "  bne " << rcond << ", $zero, " << labelName(res) << "\n";
    releaseScratch(rcond);
    break;
  }
  case OpCode::ASSIGN: {
    // ASSIGN src(var|temp|const), -, dst(var|temp)
    if (isTemp(res)) {
      std::string dst = getResultReg(res);
      if (isConst(a1)) {
        out << "  li " << dst << ", " << a1.asInt() << "\n";
        storeToSpill(res.asInt(), dst);
      } else {
        std::string src = getRegister(a1, out);
        bool sameReg = (dst == src);
        if (!sameReg) {
          out << "  move " << dst << ", " << src << "\n";
        }
        // Important: store before releasing src when src==dst.
        storeToSpill(res.asInt(), dst);
        if (!sameReg) {
          releaseScratch(src);
        }
      }
    } else if (isVar(res)) {
      std::string src = getRegister(a1, out);
      storeResult(res, src, out);
      releaseScratch(src);
    }
    break;
  }
  case OpCode::MUL: {
    // MUL arg1(var|temp|const), arg2(var|temp|const), res(temp|const)
    // 2^n * x | x * 2^n with shift optimization
    std::string rd = getResultReg(res);
    bool optimized = false;

    if (isConst(a2)) {
      int val = a2.asInt();
      int shift = log2IfPowerOf2(val);
      if (shift >= 0) {
        std::string ra = getRegister(a1, out);
        out << "  sll " << rd << ", " << ra << ", " << shift << "\n";
        releaseScratch(ra);
        optimized = true;
      }
    }
    if (!optimized && isConst(a1)) {
      int val = a1.asInt();
      int shift = log2IfPowerOf2(val);
      if (shift >= 0) {
        std::string rb = getRegister(a2, out);
        out << "  sll " << rd << ", " << rb << ", " << shift << "\n";
        releaseScratch(rb);
        optimized = true;
      }
    }
    if (!optimized) {
      // Fall back to regular multiplication
      std::string ra = getRegister(a1, out);
      std::string rb = (a1 == a2) ? ra : getRegister(a2, out);
      out << "  mul " << rd << ", " << ra << ", " << rb << "\n";
      releaseScratch(ra);
      if (a1 != a2) {
        releaseScratch(rb);
      }
    }
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }
    break;
  }
  case OpCode::DIV: {
    // DIV arg1(var|temp|const), arg2(var|temp|const), res(temp|const)
    // x / 2^k => (x + (x < 0 ? (2^k - 1) : 0)) >> k
    std::string rd = getResultReg(res);
    bool optimized = false;

    if (isConst(a2)) {
      int d = a2.asInt();
      int absD = std::abs(d);
      int k = log2IfPowerOf2(absD);
      if (d == 1) {
        std::string ra = getRegister(a1, out);
        out << "  move " << rd << ", " << ra << "\n";
        releaseScratch(ra);
        optimized = true;
      } else if (d == -1) {
        std::string ra = getRegister(a1, out);
        out << "  subu " << rd << ", $zero, " << ra << "\n";
        releaseScratch(ra);
        optimized = true;
      } else if (k >= 0) {
        std::string ra = getRegister(a1, out);
        std::string t = allocateScratch();

        out << "  sra " << t << ", " << ra << ", 31\n";
        out << "  srl " << t << ", " << t << ", " << (32 - k) << "\n";
        out << "  addu " << t << ", " << ra << ", " << t << "\n";
        out << "  sra " << rd << ", " << t << ", " << k << "\n";
        if (d < 0) {
          out << "  subu " << rd << ", $zero, " << rd << "\n";
        }
        releaseScratch(t);
        releaseScratch(ra);
        optimized = true;
      } else {
        MagicInfo mag = computeMagic(d);
        std::string ra = getRegister(a1, out);
        std::string regM = allocateScratch();

        out << "  li " << regM << ", " << mag.multiplier << "\n";

        out << "  mult " << ra << ", " << regM << "\n";
        out << "  mfhi " << rd << "\n";

        if (mag.shift > 0) {
          out << "  sra " << rd << ", " << rd << ", " << mag.shift << "\n";
        }
        std::string sign = allocateScratch();
        out << "  srl " << sign << ", " << ra << ", 31\n";
        out << " addu " << rd << ", " << rd << ", " << sign << "\n";

        if (d < 0) {
          out << "  subu " << rd << ", $zero, " << rd << "\n";
        }
        releaseScratch(sign);
        releaseScratch(regM);
        releaseScratch(ra);
        optimized = true;
      }
    }
    if (!optimized) {
      std::string ra = getRegister(a1, out);
      std::string rb = (a1 == a2) ? ra : getRegister(a2, out);
      out << "  div " << ra << ", " << rb << "\n";
      out << "  mflo " << rd << "\n";
      releaseScratch(ra);
      if (a1 != a2) {
        releaseScratch(rb);
      }
    }
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }
    break;
  }
  case OpCode::MOD: {
    // MOD arg1(var|temp|const), arg2(var|temp|const), res(temp|const)
    // x % y => x - (x / y) * y
    std::string rd = getResultReg(res);
    bool optimized = false;
    if (isConst(a2)) {
      int d = a2.asInt();
      int absD = std::abs(d);
      int k = log2IfPowerOf2(absD);

      if (k >= 0) {
        std::string ra = getRegister(a1, out);
        std::string divRes = allocateScratch();
        std::string t = allocateScratch();
        out << "  sra " << t << ", " << ra << ", 31\n";
        out << "  srl " << t << ", " << t << ", " << (32 - k) << "\n";
        out << "  addu " << t << ", " << ra << ", " << t << "\n";
        out << "  sra " << divRes << ", " << t << ", " << k << "\n";
        releaseScratch(t);

        std::string mulRes = allocateScratch();
        out << "  sll " << mulRes << ", " << divRes << ", " << k << "\n";
        out << "  subu " << rd << ", " << ra << ", " << mulRes << "\n";

        releaseScratch(mulRes);
        releaseScratch(divRes);
        releaseScratch(ra);
        optimized = true;
      }
    }

    if (!optimized) {
      std::string ra = getRegister(a1, out);
      std::string rb = (a1 == a2) ? ra : getRegister(a2, out);
      out << "  div " << ra << ", " << rb << "\n";
      out << "  mfhi " << rd << "\n";
      releaseScratch(ra);
      if (a1 != a2) {
        releaseScratch(rb);
      }
    }

    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }
    break;
  }
  case OpCode::ADD:
  case OpCode::SUB: {
    // op arg1(var|temp|const), arg2(var|temp|const), res(temp|const)
    std::string ra = getRegister(a1, out);
    std::string rb = (a1 == a2) ? ra : getRegister(a2, out);
    std::string rd = getResultReg(res);

    switch (op) {
    case OpCode::ADD:
      out << "  addu " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::SUB:
      out << "  subu " << rd << ", " << ra << ", " << rb << "\n";
      break;
    default:
      break;
    }
    releaseScratch(ra);
    if (a1 != a2) {
      releaseScratch(rb);
    }
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }
    break;
  }
  case OpCode::NEG: {
    // NEG arg1(var|temp|const), -, res(temp|const)
    std::string ra = getRegister(a1, out);
    std::string rd = getResultReg(res);
    out << "  subu " << rd << ", $zero, " << ra << "\n";
    releaseScratch(ra);
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }

    break;
  }
  case OpCode::EQ:
  case OpCode::NEQ:
  case OpCode::LT:
  case OpCode::LE:
  case OpCode::GT:
  case OpCode::GE: {
    // op arg1(var|temp|const), arg2(var|temp|const), res(temp|const)
    std::string ra = getRegister(a1, out);
    std::string rb = (a1 == a2) ? ra : getRegister(a2, out);
    std::string rd = getResultReg(res);
    switch (op) {
    case OpCode::LT:
      out << "  slt " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::GT:
      out << "  slt " << rd << ", " << rb << ", " << ra << "\n";
      break;
    case OpCode::LE: {
      std::string temp = allocateScratch();
      out << "  slt " << temp << ", " << rb << ", " << ra << "\n";
      out << "  xori " << rd << ", " << temp << ", 1\n";
      releaseScratch(temp);
      break;
    }
    case OpCode::GE: {
      std::string temp = allocateScratch();
      out << "  slt " << temp << ", " << ra << ", " << rb << "\n";
      out << "  xori " << rd << ", " << temp << ", 1\n";
      releaseScratch(temp);
      break;
    }
    case OpCode::EQ: {
      std::string temp = allocateScratch();
      out << "  subu " << temp << ", " << ra << ", " << rb << "\n";
      out << "  sltiu " << rd << ", " << temp << ", 1\n";
      releaseScratch(temp);
      break;
    }
    case OpCode::NEQ: {
      std::string temp = allocateScratch();
      out << "  subu " << temp << ", " << ra << ", " << rb << "\n";
      out << "  sltu " << rd << ", $zero, " << temp << "\n";
      releaseScratch(temp);
      break;
    }
    default:
      break;
    }
    releaseScratch(ra);
    if (a1 != a2) {
      releaseScratch(rb);
    }
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }

    break;
  }
  case OpCode::NOT: {
    // NOT arg1(var|temp|const), -, res(temp,const)
    std::string ra = getRegister(a1, out);
    std::string rd = getResultReg(res);
    out << "  sltiu " << rd << ", " << ra << ", 1\n";
    releaseScratch(ra);

    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }

    break;
  }
  case OpCode::AND:
  case OpCode::OR: {
    // OR|AND arg1(var|temp|const), arg2(var|temp|const), dst(temp)
    std::string ra = getRegister(a1, out);
    std::string rb = (a1 == a2) ? ra : getRegister(a2, out);
    std::string rd = getResultReg(res);

    out << "  sltu " << rd << ", $zero, " << ra << "\n";
    std::string rt = allocateScratch();
    out << "  sltu " << rt << ", $zero, " << rb << "\n";
    if (op == OpCode::AND)
      out << "  and " << rd << ", " << rd << ", " << rt << "\n";
    else
      out << "  or " << rd << ", " << rd << ", " << rt << "\n";

    releaseScratch(rt);
    releaseScratch(ra);
    if (a1 != a2) {
      releaseScratch(rb);
    }
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }

    break;
  }
  case OpCode::LOAD: {
    // LOAD base(var|temp), index(var|temp|const), dst(temp)
    std::string baseReg;
    const Symbol *baseSym = nullptr;

    // Fast path: local (non-param) array with constant index can directly use
    // fp-relative addressing, avoiding an extra address materialization.
    if (isVar(a1) && isConst(a2)) {
      baseSym = a1.asSymbol().get();
      auto lit = locals_.find(baseSym);
      if (lit != locals_.end()) {
        bool isParam = false;
        for (auto &p : formalParamByIndex_) {
          if (p == baseSym) {
            isParam = true;
            break;
          }
        }
        bool isArray = baseSym && baseSym->type &&
                       baseSym->type->category == Type::Category::Array;
        if (isArray && !isParam) {
          int baseOff = lit->second.offset;
          int totalOff = baseOff + a2.asInt() * 4;
          if (totalOff >= -32768 && totalOff <= 32767) {
            std::string dstReg = getResultReg(res);
            out << "  lw " << dstReg << ", " << totalOff << "($fp)\n";
            storeResult(res, dstReg, out);
            break;
          }
        }
      }
    }

    if (isVar(a1)) {
      baseSym = a1.asSymbol().get();
      auto lit = locals_.find(baseSym);
      baseReg = allocateScratch();
      if (lit != locals_.end()) {
        int offset = lit->second.offset;
        bool isParam = false;
        for (auto &p : formalParamByIndex_) {
          if (p == baseSym) {
            isParam = true;
            break;
          }
        }
        if (isParam && !isEmpty(a2)) {
          if (offset >= -32768 && offset <= 32767) {
            out << "  lw " << baseReg << ", " << offset << "($fp)\n";
          } else {
            out << "  li " << baseReg << ", " << offset << "\n";
            out << "  addu " << baseReg << ", $fp, " << baseReg << "\n";
            out << "  lw " << baseReg << ", 0(" << baseReg << ")\n";
          }
        } else {
          if (offset >= -32768 && offset <= 32767) {
            out << "  addiu " << baseReg << ", $fp, " << offset << "\n";
          } else {
            out << "  li " << baseReg << ", " << offset << "\n";
            out << "  addu " << baseReg << ", $fp, " << baseReg << "\n";
          }
        }
      } else {
        // global varibale
        std::string varName =
            baseSym->globalName.empty() ? baseSym->name : baseSym->globalName;
        out << "  la " << baseReg << ", " << varName << "\n";
      }
    } else {
      // temp
      baseReg = getRegister(a1, out);
    }

    std::string dstReg = getResultReg(res);
    std::string indexReg = "";

    if (isEmpty(a2)) {
      bool isArrary = baseSym && baseSym->type &&
                      baseSym->type->category == Type::Category::Array;
      if (isArrary) {
        out << "  move " << dstReg << ", " << baseReg << "\n";
      } else {
        out << "  lw " << dstReg << ", 0(" << baseReg << ")\n";
      }
    } else if (isConst(a2)) {
      int offset = a2.asInt() * 4;
      if (offset >= -32768 && offset <= 32767) {
        out << "  lw " << dstReg << ", " << offset << "(" << baseReg << ")\n";
      } else {
        std::string offReg = allocateScratch();
        out << "  li " << offReg << ", " << offset << "\n";
        out << "  addu " << offReg << ", " << baseReg << ", " << offReg << "\n";
        out << "  lw " << dstReg << ", 0(" << offReg << ")\n";
        releaseScratch(offReg);
      }
    } else {
      indexReg = getRegister(a2, out);
      std::string offsetReg = allocateScratch();
      out << "  sll " << offsetReg << ", " << indexReg << ", 2\n";

      out << "  addu " << offsetReg << ", " << baseReg << ", " << offsetReg
          << "\n";
      out << "  lw " << dstReg << ", 0(" << offsetReg << ")\n";
      releaseScratch(offsetReg);
    }
    storeResult(res, dstReg, out);
    releaseScratch(baseReg);
    if (!indexReg.empty()) {
      releaseScratch(indexReg);
    }
    break;
  }
  case OpCode::STORE: {
    // STORE value(var|temp|const), base(var|temp), index(var|temp|const)
    std::string rv = getRegister(a1, out);
    std::string baseReg;
    const Symbol *baseSym = nullptr;

    // Fast path: local (non-param) array with constant index can directly use
    // fp-relative addressing, avoiding an extra address materialization.
    if (isVar(a2) && isConst(res)) {
      baseSym = a2.asSymbol().get();
      auto lit = locals_.find(baseSym);
      if (lit != locals_.end()) {
        bool isParam = false;
        for (auto &p : formalParamByIndex_) {
          if (p == baseSym) {
            isParam = true;
            break;
          }
        }
        bool isArray = baseSym && baseSym->type &&
                       baseSym->type->category == Type::Category::Array;
        if (isArray && !isParam) {
          int baseOff = lit->second.offset;
          int totalOff = baseOff + res.asInt() * 4;
          if (totalOff >= -32768 && totalOff <= 32767) {
            out << "  sw " << rv << ", " << totalOff << "($fp)\n";
            releaseScratch(rv);
            break;
          }
        }
      }
    }

    if (isVar(a2)) {
      baseSym = a2.asSymbol().get();
      auto lit = locals_.find(baseSym);
      baseReg = allocateScratch();
      if (lit != locals_.end()) {
        int offset = lit->second.offset;
        bool isParam = false;
        for (auto &p : formalParamByIndex_) {
          if (p == baseSym) {
            isParam = true;
            break;
          }
        }
        if (isParam && !isEmpty(res)) {
          if (offset >= -32768 && offset <= 32767) {
            out << "  lw " << baseReg << ", " << offset << "($fp)\n";
          } else {
            out << "  li " << baseReg << ", " << offset << "\n";
            out << "  addu " << baseReg << ", $fp, " << baseReg << "\n";
            out << "  lw " << baseReg << ", 0(" << baseReg << ")\n";
          }
        } else {
          if (offset >= -32768 && offset <= 32767) {
            out << "  addiu " << baseReg << ", $fp, " << offset << "\n";
          } else {
            out << "  li " << baseReg << ", " << offset << "\n";
            out << "  addu " << baseReg << ", $fp, " << baseReg << "\n";
          }
        }
      } else {
        // global variable
        std::string varName =
            baseSym->globalName.empty() ? baseSym->name : baseSym->globalName;
        out << "  la " << baseReg << ", " << varName << "\n";
      }
    } else {
      // temp
      baseReg = getRegister(a2, out);
    }

    std::string indexReg = "";
    auto &idxOp = res;

    if (isEmpty(idxOp)) {
      out << "  sw " << rv << ", 0(" << baseReg << ")\n";
    } else if (isConst(idxOp)) {
      int offset = idxOp.asInt() * 4;
      if (offset >= -32768 && offset <= 32767) {
        out << "  sw " << rv << ", " << offset << "(" << baseReg << ")\n";
      } else {
        std::string offReg = allocateScratch();
        out << "  li " << offReg << ", " << offset << "\n";
        out << "  addu " << offReg << ", " << baseReg << ", " << offReg << "\n";
        out << "  sw " << rv << ", 0(" << offReg << ")\n";
        releaseScratch(offReg);
      }
    } else {
      indexReg = getRegister(idxOp, out);
      std::string offsetReg = allocateScratch();
      out << "  sll " << offsetReg << ", " << indexReg << ", 2\n";
      out << "  addu " << offsetReg << ", " << baseReg << ", " << offsetReg
          << "\n";
      out << "  sw " << rv << ", 0(" << offsetReg << ")\n";
      releaseScratch(offsetReg);
    }
    releaseScratch(rv);
    releaseScratch(baseReg);
    if (!indexReg.empty()) {
      releaseScratch(indexReg);
    }
    break;
  }
  case OpCode::ARG: {
    // ARG arg(var|const|temp), -,-
    int idx = paramIndex_++;
    if (idx < 4) {
      static const char *aregs[4] = {"$a0", "$a1", "$a2", "$a3"};
      if (isVar(a1)) {
        std::string name = a1.asSymbol()->name;
        bool isStr = name.substr(0, 4) == ".fmt";
        if (isStr) {
          // string literal, load address
          out << "  la " << aregs[idx] << ", " << name << "\n";
        } else {
          std::string r = getRegister(a1, out);
          out << "  move " << aregs[idx] << ", " << r << "\n";
          releaseScratch(r);
        }
      } else if (isConst(a1)) {
        out << "  li " << aregs[idx] << ", " << a1.asInt() << "\n";
      } else {
        std::string r = getRegister(a1, out);
        out << "  move " << aregs[idx] << ", " << r << "\n";
        releaseScratch(r);
      }
    } else {
      pendingExtraArgs_.push_back(a1);
    }
    break;
  }
  case OpCode::PARAM: {
    // PARAM index(const), result(temp)
    // here is temp
    Operand idxOp = inst->getArg1();
    Operand resOp = inst->getResult();
    if (isTemp(res)) {
      int idx = idxOp.asInt();
      std::string dstReg = getResultReg(res);
      if (idx < 4) {
        out << "  move " << dstReg << ", " << aregs[idx] << "\n";
      } else {
        // come back to caller's frame
        int offset = frameSize_ + (idx - 4) * 4;
        if (offset >= -32768 && offset <= 32767) {
          out << "  lw " << dstReg << ", " << offset << "($fp)\n";
        } else {
          std::string addr = allocateScratch();
          out << "  li " << addr << ", " << offset << "\n";
          out << "  addu " << addr << ", $fp, " << addr << "\n";
          out << "  lw " << dstReg << ", 0(" << addr << ")\n";
          releaseScratch(addr);
        }
      }
      storeToSpill(resOp.asInt(), dstReg);
    }
    break;
  }
  case OpCode::CALL: {
    // CALL argc(const), func(label), res(temp)

    // Push extra arguments (beyond $a0-$a3) onto stack
    int extraCount = pendingExtraArgs_.size();
    int extraBytes = 0;
    if (extraCount > 0) {
      extraBytes = extraCount * 4;
      out << "  addiu $sp, $sp, -" << extraBytes << "\n";
      for (int i = 0; i < extraCount; ++i) {
        const Operand &arg = pendingExtraArgs_[i];
        std::string r = getRegister(arg, out);
        int offset = i * 4;
        if (offset >= -32768 && offset <= 32767) {
          out << "  sw " << r << ", " << offset << "($sp)\n";
        } else {
          std::string offReg = allocateScratch();
          out << "  li " << offReg << ", " << offset << "\n";
          out << "  addu " << offReg << ", $sp, " << offReg << "\n";
          out << "  sw " << r << ", 0(" << offReg << ")\n";
          releaseScratch(offReg);
        }
        releaseScratch(r);
      }
    }

    // Call function
    std::string fname = a2.asSymbol()->globalName.empty()
                            ? a2.asSymbol()->name
                            : a2.asSymbol()->globalName;
    out << "  jal " << fname << "\n";

    // Clean up extra arguments from stack
    if (extraCount > 0) {
      out << "  addiu $sp, $sp, " << extraBytes << "\n";
      pendingExtraArgs_.clear();
    }

    if (fname != "printf" && isTemp(res)) {
      std::string returnReg = getResultReg(res);
      out << "  move " << returnReg << ", $v0\n";
      storeToSpill(res.asInt(), returnReg);
    }

    paramIndex_ = 0;
    break;
  }
  case OpCode::RETURN: {
    if (!isEmpty(res)) {
      if (isConst(res)) {
        out << "  li $v0, " << res.asInt() << "\n";
      } else {
        std::string r = getRegister(res, out);
        out << "  move $v0, " << r << "\n";
        releaseScratch(r);
      }
    }
    if (!currentEpilogueLabel_.empty()) {
      out << "  j " << currentEpilogueLabel_ << "\n";
    }
    break;
  }
  case OpCode::ALLOCA:
  case OpCode::PHI:
  case OpCode::NOP: {
    break;
  }
  }
}

std::string AsmGen::regForTemp(int tempId) const {
  if (!_regAllocator.isSpilled(tempId)) {
    int idx = _regAllocator.getReg(tempId);
    if (idx >= 0 && idx < regs_.size()) {
      return regs_[idx].name;
    }
  }
  return "SPILLED";
}

void AsmGen::releaseScratch(const std::string &reg) {
  for (auto &sr : scratchRegs_) {
    if (sr.name == reg) {
      sr.inUse = false;
      return;
    }
  }
}

void AsmGen::comment(std::ostream &out, const std::string &txt) {
  if (emitComments_) {
    out << "# " << txt << "\n";
  }
}

std::string AsmGen::getRegister(const Operand &op, std::ostream &out) {
  switch (op.getType()) {
  case OperandType::Temporary:
    if (_spillOffsets.count(op.asInt())) {
      // Load from spill location
      std::string varScratch = allocateScratch();
      int off = _spillOffsets.at(op.asInt());
      if (off >= -32768 && off <= 32767) {
        out << "  lw " << varScratch << ", " << off << "($fp)\n";
      } else {
        std::string addrReg = allocateScratch();
        out << "  li " << addrReg << ", " << off << "\n";
        out << "  addu " << addrReg << ", $fp, " << addrReg << "\n";
        out << "  lw " << varScratch << ", 0(" << addrReg << ")\n";
        releaseScratch(addrReg);
      }
      return varScratch;
    }
    return regForTemp(op.asInt());

  case OperandType::ConstantInt: {
    int value = op.asInt();
    if (value == 0)
      return "$zero";
    std::string scratch = allocateScratch();
    out << "  li " << scratch << ", " << value << "\n";
    return scratch;
  }

  case OperandType::Variable: {
    auto sym = op.asSymbol();
    auto lit = locals_.find(sym.get());

    if (lit != locals_.end()) {
      std::string scratch = allocateScratch();
      bool isArray = sym->type && sym->type->category == Type::Category::Array;
      bool isParam = false;
      for (auto param : formalParamByIndex_) {
        if (param == sym.get()) {
          isParam = true;
          break;
        }
      }

      if (isArray && !isParam) {
        // Load address of local array
        int offset = lit->second.offset;
        if (offset >= -32768 && offset <= 32767) {
          out << "  addiu " << scratch << ", $fp, " << offset << "\n";
        } else {
          out << "  li " << scratch << ", " << offset << "\n";
          out << "  addu " << scratch << ", $fp, " << scratch << "\n";
        }
      } else {
        // Load value of local variable
        int offset = lit->second.offset;
        if (offset >= -32768 && offset <= 32767) {
          out << "  lw " << scratch << ", " << offset << "($fp)\n";
        } else {
          std::string addrReg = allocateScratch();
          out << "  li " << addrReg << ", " << offset << "\n";
          out << "  addu " << addrReg << ", $fp, " << addrReg << "\n";
          out << "  lw " << scratch << ", 0(" << addrReg << ")\n";
          releaseScratch(addrReg);
        }
      }
      return scratch;
    } else {
      std::string scratch = allocateScratch();
      bool isArray = sym->type && sym->type->category == Type::Category::Array;
      std::string varName =
          sym->globalName.empty() ? sym->name : sym->globalName;
      if (isArray) {
        out << "  la " << scratch << ", " << varName << "\n";
      } else {
        out << "  la " << scratch << ", " << varName << "\n";
        out << "  lw " << scratch << ", 0(" << scratch << ")\n";
      }
      return scratch;
    }
  }
  // no support
  case OperandType::Label:
    return "$zero";

  case OperandType::Empty:
    return "$zero";
  }

  return "$zero";
}

void AsmGen::storeResult(const Operand &op, const std::string &reg,
                         std::ostream &out) {
  switch (op.getType()) {
  case OperandType::Temporary:
    if (_spillOffsets.count(op.asInt())) {
      int off = _spillOffsets.at(op.asInt());
      if (off >= -32768 && off <= 32767) {
        out << "  sw " << reg << ", " << off << "($fp)\n";
      } else {
        std::string addrReg = allocateScratch();
        out << "  li " << addrReg << ", " << off << "\n";
        out << "  addu " << addrReg << ", $fp, " << addrReg << "\n";
        out << "  sw " << reg << ", 0(" << addrReg << ")\n";
        releaseScratch(addrReg);
      }
    }
    break;

  case OperandType::Variable: {
    auto sym = op.asSymbol();
    auto lit = locals_.find(sym.get());
    if (lit != locals_.end()) {
      // Store to local variable
      int off = lit->second.offset;
      if (off >= -32768 && off <= 32767) {
        out << "  sw " << reg << ", " << off << "($fp)\n";
      } else {
        std::string addrReg = allocateScratch();
        out << "  li " << addrReg << ", " << off << "\n";
        out << "  addu " << addrReg << ", $fp, " << addrReg << "\n";
        out << "  sw " << reg << ", 0(" << addrReg << ")\n";
        releaseScratch(addrReg);
      }
    } else {
      std::string scratch = allocateScratch();
      std::string varName =
          sym->globalName.empty() ? sym->name : sym->globalName;
      out << "  la " << scratch << ", " << varName << "\n";
      out << "  sw " << reg << ", 0(" << scratch << ")\n";
      releaseScratch(scratch);
    }
    break;
  }

  case OperandType::ConstantInt:
  case OperandType::Label:
  case OperandType::Empty:
    break;
  }
}

void AsmGen::resetFunctionState() {
  locals_.clear();
  frameSize_ = 0;
  formalParamByIndex_.clear();
  paramIndex_ = 0;
  pendingExtraArgs_.clear();
  _spillOffsets.clear();
}

void AsmGen::analyzeFunctionLocals(const Function *func) {
  int nextOffset = 8; // start after saved $ra and $fp
  formalParamByIndex_.clear();
  // trace PARAM -> STORE
  std::map<int, int> tempToParam;
  for (auto &blk : func->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      auto op = inst->getOp();
      if (op == OpCode::PARAM) {
        // record temp -> param index
        if (inst->getResult().getType() == OperandType::Temporary) {
          int tempId = inst->getResult().asInt();
          int paramIdx = inst->getArg1().asInt();
          tempToParam[tempId] = paramIdx;
        }
      }
      if (op == OpCode::ALLOCA) {
        const auto &sym = inst->getArg1();
        int sz = 1;
        if (inst->getResult().getType() == OperandType::ConstantInt) {
          sz = inst->getResult().asInt();
        }
        if (sym.getType() == OperandType::Variable) {
          auto sp = sym.asSymbol().get();
          if (locals_.find(sp) == locals_.end()) {
            int words = sz;
            locals_[sp] = {nextOffset, words};
            nextOffset += words * 4;
          }
        }
      }

      if (op == OpCode::STORE) {
        const Operand &val = inst->getArg1();
        const Operand &base = inst->getArg2();
        const Operand &idx = inst->getResult();
        // STORE paramTemp, variableSym
        if (val.getType() == OperandType::Temporary &&
            base.getType() == OperandType::Variable &&
            idx.getType() == OperandType::Empty) {
          auto it = tempToParam.find(val.asInt());
          if (it != tempToParam.end()) {
            int pIdx = it->second;
            const Symbol *sym = base.asSymbol().get();

            if (formalParamByIndex_.size() <= pIdx) {
              formalParamByIndex_.resize(pIdx + 1, nullptr);
            }
            formalParamByIndex_[pIdx] = sym;
          }
        }
      }
    }
  }
  frameSize_ = nextOffset;
}
