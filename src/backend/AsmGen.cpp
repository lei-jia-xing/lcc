#include "backend/AsmGen.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "semantic/Symbol.hpp"
#include "semantic/Type.hpp"
#include <iostream>

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
  // Copy in https://gist.github.com/KaceCottam/37a065a2c194c0eb50b417cf67455af1
  out << "printf:\n";
  out << "  addiu $sp, $sp, -28\n";
  out << "  sw $t0, 0($sp)\n";
  out << "  sw $t1, 4($sp)\n";
  out << "  sw $t2, 8($sp)\n";
  out << "  sw $a0, 12($sp)\n";
  out << "  sw $a1, 16($sp)\n";
  out << "  sw $a2, 20($sp)\n";
  out << "  sw $a3, 24($sp)\n";
  out << "\n";
  out << "  la $t0, ($a0)      # t0: current position in format string\n";
  out << "  addiu $t1, $sp, 16 # t1: pointer to current argument\n";
  out << "  li $t2, 0          # t2: temporary for comparison\n";
  out << "\n";
  out << "printf_loop:\n";
  out << "  lbu $a0, 0($t0)\n";
  out << "  beq $a0, $zero, printf_end\n";
  out << "  addiu $t0, $t0, 1\n";
  out << "  ori $t2, $zero, '%'\n";
  out << "  subu $t2, $a0, $t2\n";
  out << "  beq $t2, $zero, printf_format\n";
  out << "  j printf_char\n";
  out << "\n";
  out << "printf_format:\n";
  out << "  lbu $a0, 0($t0)\n";
  out << "  addiu $t0, $t0, 1\n";
  out << "  ori $t2, $zero, 'd'\n";
  out << "  subu $t2, $a0, $t2\n";
  out << "  beq $t2, $zero, printf_int\n";
  out << "  ori $t2, $zero, 'c'\n";
  out << "  subu $t2, $a0, $t2\n";
  out << "  beq $t2, $zero, printf_char\n";
  out << "  ori $t2, $zero, 's'\n";
  out << "  subu $t2, $a0, $t2\n";
  out << "  beq $t2, $zero, printf_str\n";
  out << "  j printf_char\n";
  out << "\n";
  out << "printf_int:\n";
  out << "  li $v0, 1\n";
  out << "  lw $a0, 0($t1)\n";
  out << "  addiu $t1, $t1, 4\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "\n";
  out << "printf_str:\n";
  out << "  li $v0, 4\n";
  out << "  lw $a0, 0($t1)\n";
  out << "  addiu $t1, $t1, 4\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "\n";
  out << "printf_char:\n";
  out << "  li $v0, 11\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "\n";
  out << "printf_end:\n";
  out << "  # Restore registers\n";
  out << "  lw $t0, 0($sp)\n";
  out << "  lw $t1, 4($sp)\n";
  out << "  lw $t2, 8($sp)\n";
  out << "  lw $a0, 12($sp)\n";
  out << "  lw $a1, 16($sp)\n";
  out << "  lw $a2, 20($sp)\n";
  out << "  lw $a3, 24($sp)\n";
  out << "  addiu $sp, $sp, 28\n";
  out << "  move $v0, $zero\n";
  out << "  jr $ra\n\n";
  out << "getint:\n";
  out << "  li $v0, 5\n";
  out << "  syscall\n";
  out << "  jr $ra\n\n";
}

void AsmGen::emitFunction(const Function *func, std::ostream &out) {
  curFuncName_ = func->getName();
  resetFunctionState();
  analyzeFunctionLocals(func);

  _regAllocator.run(const_cast<Function *>(func));

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

  int savedRegSize = calleeSavedRegs.size() * 4;
  frameSize_ += savedRegSize;

  int rem = frameSize_ % 8;
  if (rem != 0)
    // allign to 8 bytes
    frameSize_ += (8 - rem);

  out << func->getName() << ":\n";
  if (frameSize_ < 8)
    frameSize_ = 8;
  if (frameSize_ > 32767) {
    out << "  li $t0, -" << frameSize_ << "\n";
    out << "  addu $sp, $sp, $t0\n" << "\n";
  } else {
    out << "  addiu $sp, $sp, -" << frameSize_ << "\n";
  }
  out << "  sw $ra, 0($sp)\n";
  out << "  sw $fp, 4($sp)\n";

  for (size_t i = 0; i < calleeSavedRegs.size(); ++i) {
    int regId = calleeSavedRegs[i];
    int offset = 8 + i * 4;
    out << "  sw " << regs_[regId].name << ", " << offset << "($sp)\n";
  }
  out << "  move $fp, $sp\n";

  static const char *aregs[4] = {"$a0", "$a1", "$a2", "$a3"};
  size_t fsz = formalParamByIndex_.size();
  for (size_t i = 0; i < fsz && i < 4; ++i) {
    const Symbol *sym = formalParamByIndex_[i];
    auto it = locals_.find(sym);
    if (it != locals_.end()) {
      int off = it->second.offset;
      out << "  sw " << aregs[i] << ", " << off << "($fp)\n";
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
    std::string tempReg = allocateScratch();
    out << "  lw " << tempReg << ", " << offCaller << "($fp)\n";
    out << "  sw " << tempReg << ", " << offLocal << "($fp)\n";
  }
  currentEpilogueLabel_ = func->getName() + std::string("_END");

  if (func->getName() == "main") {
    for (auto *inst : curMod_->globals) {
      bool isConstInit = false;
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
      lowerInstruction(&inst, out);
    }
  }
  out << currentEpilogueLabel_ << ":\n";
  for (size_t i = 0; i < calleeSavedRegs.size(); ++i) {
    int regId = calleeSavedRegs[i];
    int offset = 8 + i * 4;
    out << "  lw " << regs_[regId].name << ", " << offset << "($sp)\n";
  }

  out << "  lw $ra, 0($fp)\n";
  out << "  lw $fp, 4($fp)\n";
  if (frameSize_ > 32767) {
    out << "  li $t0, " << frameSize_ << "\n";
    out << "  addu $sp, $sp, $t0\n";
  } else {
    out << "  addiu $sp, $sp, " << frameSize_ << "\n";
  }
  if (func->getName() == "main") {
    out << "  li $v0, 10\n";
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
    if (_regAllocator.isSpilled(tempId)) {
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
    if (isTemp(r) && !_regAllocator.isSpilled(r.asInt())) {
      return regForTemp(r.asInt());
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
    break;
  }
  case OpCode::ASSIGN: {
    // ASSIGN src(var|temp|const), -, dst(var|temp)
    if (isTemp(res)) {
      std::string dst = getResultReg(res);
      if (isConst(a1)) {
        out << "  li " << dst << ", " << a1.asInt() << "\n";
      } else {
        std::string src = getRegister(a1, out);
        out << "  move " << dst << ", " << src << "\n";
      }
      storeToSpill(res.asInt(), dst);
    } else if (isVar(res)) {
      std::string src = getRegister(a1, out);
      storeResult(res, src, out);
    }
    break;
  }
  case OpCode::ADD:
  case OpCode::SUB:
  case OpCode::MUL:
  case OpCode::DIV:
  case OpCode::MOD: {
    // op arg1(var|temp|const), arg2(var|temp|const), res(temp|const)
    std::string ra = getRegister(a1, out);
    std::string rb = getRegister(a2, out);
    std::string rd = getResultReg(res);

    switch (op) {
    case OpCode::ADD:
      out << "  addu " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::SUB:
      out << "  subu " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::MUL:
      out << "  mul " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::DIV:
      out << "  div " << ra << ", " << rb << "\n";
      out << "  mflo " << rd << "\n";
      break;
    case OpCode::MOD:
      out << "  div " << ra << ", " << rb << "\n";
      out << "  mfhi " << rd << "\n";
      break;
    default:
      break;
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
    std::string ra = getRegister(a1, out);
    std::string rb = getRegister(a2, out);
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
      break;
    }
    case OpCode::GE: {
      std::string temp = allocateScratch();
      out << "  slt " << temp << ", " << ra << ", " << rb << "\n";
      out << "  xori " << rd << ", " << temp << ", 1\n";
      break;
    }
    case OpCode::EQ: {
      std::string temp = allocateScratch();
      out << "  subu " << temp << ", " << ra << ", " << rb << "\n";
      out << "  sltiu " << rd << ", " << temp << ", 1\n";
      break;
    }
    case OpCode::NEQ: {
      std::string temp = allocateScratch();
      out << "  subu " << temp << ", " << ra << ", " << rb << "\n";
      out << "  sltu " << rd << ", $zero, " << temp << "\n";
      break;
    }
    default:
      break;
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

    // Store result if needed
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }

    break;
  }
  case OpCode::AND:
  case OpCode::OR: {
    // OR|AND arg1(var|temp|const), arg2(var|temp|const), dst(temp)
    std::string ra = getRegister(a1, out);
    std::string rb = getRegister(a2, out);

    std::string rd = getResultReg(res);

    out << "  sltu " << rd << ", $zero, " << ra << "\n";
    std::string rt = allocateScratch();
    out << "  sltu " << rt << ", $zero, " << rb << "\n";
    if (op == OpCode::AND)
      out << "  and " << rd << ", " << rd << ", " << rt << "\n";
    else
      out << "  or " << rd << ", " << rd << ", " << rt << "\n";

    // Store result if needed
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    }

    break;
  }
  case OpCode::LOAD: {
    // LOAD base(var|temp), [index(var|temp|const)], dst(temp)
    std::string baseReg;
    const Symbol *baseSym = nullptr;

    if (isVar(a1)) {
      baseSym = a1.asSymbol().get();
      auto lit = locals_.find(baseSym);
      baseReg = allocateScratch();
      if (lit != locals_.end()) {
        bool isArray =
            baseSym->type && baseSym->type->category == Type::Category::Array;
        bool isParam = false;
        for (auto &p : formalParamByIndex_) {
          if (p == baseSym) {
            isParam = true;
            break;
          }
        }
        if (isArray && !isParam) {
          out << "  addiu " << baseReg << ", $fp, " << lit->second.offset
              << "\n";
        } else {
          out << "  lw " << baseReg << ", " << lit->second.offset << "($fp)\n";
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
      }
    } else {
      std::string indexReg = getRegister(a2, out);
      std::string offsetReg = allocateScratch();
      out << "  sll " << offsetReg << ", " << indexReg << ", 2\n";

      out << "  addu " << offsetReg << ", " << baseReg << ", " << offsetReg
          << "\n";
      out << "  lw " << dstReg << ", 0(" << offsetReg << ")\n";
    }
    storeResult(res, dstReg, out);
    break;
  }
  case OpCode::STORE: {
    // STORE value(var|temp|const), base(var|temp), index(var|temp|const)
    std::string rv = getRegister(a1, out);
    std::string baseReg;
    const Symbol *baseSym = nullptr;

    if (isVar(a2)) {
      baseSym = a2.asSymbol().get();
      auto lit = locals_.find(baseSym);
      baseReg = allocateScratch();
      if (lit != locals_.end()) {
        bool isArray =
            baseSym->type && baseSym->type->category == Type::Category::Array;
        bool isParam = false;
        for (auto &p : formalParamByIndex_) {
          if (p == baseSym) {
            isParam = true;
            break;
          }
        }
        if (isArray && !isParam) {
          out << "  addiu " << baseReg << ", $fp, " << lit->second.offset
              << "\n";
        } else {
          out << "  lw " << baseReg << ", " << lit->second.offset << "($fp)\n";
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
      }
    } else {
      std::string indexReg = getRegister(idxOp, out);
      std::string offsetReg = allocateScratch();
      out << "  sll " << offsetReg << ", " << indexReg << ", 2\n";
      out << "  addu " << offsetReg << ", " << baseReg << ", " << offsetReg
          << "\n";
      out << "  sw " << rv << ", 0(" << offsetReg << ")\n";
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
        }
      } else if (isConst(a1)) {
        out << "  li " << aregs[idx] << ", " << a1.asInt() << "\n";
      } else {
        std::string r = getRegister(a1, out);
        out << "  move " << aregs[idx] << ", " << r << "\n";
      }
    } else {
      pendingExtraArgs_.push_back(a1);
    }
    break;
  }
  case OpCode::PARAM: {
    // The parameter handling is done in analyzeFunctionLocals
    break;
  }
  case OpCode::CALL: {
    // CALL argc(const), func(label), res(temp)

    // Push extra arguments (beyond $a0-$a3) onto stack
    int extraCount = pendingExtraArgs_.size();
    int extraBytes = 0;
    if (extraCount > 0) {
      extraBytes = extraCount * 4;
      if (extraBytes % 8 != 0) {
        extraBytes += 4;
      }
      out << "  addiu $sp, $sp, -" << extraBytes << "\n";
      for (int i = 0; i < extraCount; ++i) {
        const Operand &arg = pendingExtraArgs_[i];
        std::string r = getRegister(arg, out);
        out << "  sw " << r << ", " << (i * 4) << "($sp)\n";
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

    if (isTemp(res)) {
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
      }
    }
    if (!currentEpilogueLabel_.empty()) {
      out << "  j " << currentEpilogueLabel_ << "\n";
    }
    break;
  }
  case OpCode::ALLOCA: {
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
    if (_regAllocator.isSpilled(op.asInt())) {
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
    // Always load constants into registers for proper MIPS assembly
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
      // Global variable - use global unique name if available, otherwise
      // original name
      std::string scratch = allocateScratch();
      bool isArray = sym->type && sym->type->category == Type::Category::Array;
      std::string varName =
          sym->globalName.empty() ? sym->name : sym->globalName;
      if (isArray) {
        // Load address of global array
        out << "  la " << scratch << ", " << varName << "\n";
      } else {
        // Load value of global variable
        out << "  la " << scratch << ", " << varName << "\n";
        out << "  lw " << scratch << ", 0(" << scratch << ")\n";
      }
      return scratch;
    }
  }

  case OperandType::Label:
    return "$zero"; // Labels are not loaded into registers

  case OperandType::Empty:
    return "$zero";
  }

  return "$zero";
}

void AsmGen::storeResult(const Operand &op, const std::string &reg,
                         std::ostream &out) {
  switch (op.getType()) {
  case OperandType::Temporary:
    if (_regAllocator.isSpilled(op.asInt())) {
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
        out << "  sw " << reg << ", " << lit->second.offset << "($fp)\n";
      } else {
        std::string addrReg = allocateScratch();
        out << "  li " << addrReg << ", " << off << "\n";
        out << "  addu " << addrReg << ", $fp, " << addrReg << "\n";
        out << "  sw " << reg << ", 0(" << addrReg << ")\n";
        releaseScratch(addrReg);
      }
    } else {
      // Store to global variable - use global unique name if available,
      // otherwise original name
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
    // Constants don't need storing
    break;

  case OperandType::Label:
  case OperandType::Empty:
    // Nothing to do
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
  int nextOffset = 8;
  formalParamByIndex_.clear();
  bool inEntryParamRun = true;
  bool isFirstBlock = true;
  for (auto &blk : func->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      auto op = inst.getOp();
      if (op == OpCode::LABEL) {
        continue;
      }
      if (isFirstBlock && op == OpCode::PARAM) {
        const auto &a1 = inst.getArg1();
        const auto &res = inst.getResult();
        if (inEntryParamRun && a1.getType() == OperandType::ConstantInt &&
            res.getType() == OperandType::Variable) {
          int idx = a1.asInt();
          if (idx >= 0) {
            if (formalParamByIndex_.size() <= idx)
              formalParamByIndex_.resize(idx + 1);
            auto sym = res.asSymbol();
            formalParamByIndex_[idx] = sym.get();
            if (locals_.find(sym.get()) == locals_.end()) {
              locals_[sym.get()] = {nextOffset, 1};
              nextOffset += 4;
            }
          }
          continue;
        }
      }
      if (op == OpCode::ALLOCA) {
        const auto &sym = inst.getArg1();
        int sz = 1;
        if (inst.getResult().getType() == OperandType::ConstantInt) {
          sz = inst.getResult().asInt();
        }
        if (sym.getType() == OperandType::Variable) {
          auto sp = sym.asSymbol().get();
          if (locals_.find(sp) == locals_.end()) {
            int words = sz;
            locals_[sp] = {nextOffset, words};
            nextOffset += words * 4;
          }
        }
        continue;
      }
      if (isFirstBlock)
        inEntryParamRun = false;
    }
    isFirstBlock = false;
  }
  frameSize_ = nextOffset;
  int rem = frameSize_ % 8;
  if (rem != 0)
    // align to 8 bytes
    frameSize_ += (8 - rem);
}
