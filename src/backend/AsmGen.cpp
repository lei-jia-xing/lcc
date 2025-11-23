#include "backend/AsmGen.hpp"
#include "codegen/Function.hpp"
#include "semantic/Symbol.hpp"
#include "semantic/Type.hpp"
#include <iostream>

AsmGen::AsmGen() {
  static const char *tRegs[10] = {"$t0", "$t1", "$t2", "$t3", "$t4",
                                  "$t5", "$t6", "$t7", "$t8", "$t9"};
  regs_.reserve(10);
  for (int i = 0; i < 10; ++i) {
    regs_.push_back({tRegs[i], false, -1});
  }
}

void AsmGen::generate(const IRModuleView &mod, std::ostream &out) {
  curMod_ = &mod;
  paramIndex_ = 0;
  analyzeGlobals(mod);
  emitDataSection(mod, out);
  emitTextSection(mod, out);
  curMod_ = nullptr;
}

void AsmGen::emitDataSection(const IRModuleView &mod, std::ostream &out) {
  out << ".data\n";
  for (auto &kv : mod.stringLiterals) {
    const std::string &raw = kv.second;
    size_t start = 0, end = raw.size();
    if (end >= 2 && raw.front() == '"' && raw.back() == '"') {
      start = 1;
      end -= 1;
    }
    out << kv.first << ": .asciiz \"";
    for (size_t i = start; i < end; ++i) {
      char c = raw[i];
      out << c;
    }
    out << "\"\n";
  }
  struct GInfo {
    int size = 0;
    std::vector<int> inits;
    bool defined = false;
  };
  std::unordered_map<std::string, GInfo> gmap;
  auto recordDef = [&](const std::string &name, int size) {
    auto &gi = gmap[name];
    if (!gi.defined) {
      gi.size = size;
      gi.inits.assign(size, 0);
      gi.defined = true;
    }
  };
  auto setVal = [&](const std::string &name, int idx, int val) {
    auto it = gmap.find(name);
    if (it != gmap.end() && idx >= 0 && idx < it->second.inits.size()) {
      it->second.inits[idx] = val;
    }
  };
  for (size_t i = 0; i < mod.globals.size(); ++i) {
    const Instruction *inst = mod.globals[i];
    switch (inst->getOp()) {
    case OpCode::ALLOCA: {
      auto t1 = inst->getArg1().getType();
      auto tr = inst->getResult().getType();
      if (t1 == OperandType::Variable && tr == OperandType::ConstantInt) {
        std::string name = inst->getArg1().asSymbol()->name;
        int sz = (t1 == OperandType::ConstantInt) ? inst->getArg2().asInt()
                                                  : inst->getResult().asInt();
        recordDef(name, sz);
      }
      break;
    }
    case OpCode::ASSIGN: {
      const Operand &src = inst->getArg1();
      const Operand &dst = inst->getResult();
      if (src.getType() == OperandType::ConstantInt &&
          dst.getType() == OperandType::Variable) {
        std::string name = dst.asSymbol()->name;
        setVal(name, 0, src.asInt());
      }
      break;
    }
    case OpCode::STORE: {
      const Operand &val = inst->getArg1();
      const Operand &base = inst->getArg2();
      const Operand &idx = inst->getResult();
      if (val.getType() == OperandType::ConstantInt &&
          base.getType() == OperandType::Variable &&
          idx.getType() == OperandType::ConstantInt) {
        std::string name = base.asSymbol()->name;
        setVal(name, idx.asInt(), val.asInt());
      }
      break;
    }
    default:
      break;
    }
  }

  for (auto &kv : gmap) {
    const std::string &name = kv.first;
    const GInfo &gi = kv.second;
    if (!gi.defined)
      continue;
    out << name << ": .word ";
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
  out << "printf:\n";
  out << "  move $t4, $a0\n";
  out << "  li   $t5, 0\n";
  out << "printf_loop:\n";
  out << "  lbu  $t0, 0($t4)\n";
  out << "  beq  $t0, $zero, printf_end\n";
  out << "  addiu $t4, $t4, 1\n";
  out << "  li   $t1, '%'\n";
  out << "  bne  $t0, $t1, printf_emit_char\n";
  out << "  lbu  $t2, 0($t4)\n";
  out << "  addiu $t4, $t4, 1\n";
  out << "  li   $t1, 'd'\n";
  out << "  beq  $t2, $t1, printf_emit_int\n";
  out << "  move $a0, $t2\n";
  out << "  li   $v0, 11\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "printf_emit_char:\n";
  out << "  move $a0, $t0\n";
  out << "  li   $v0, 11\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "printf_emit_int:\n";
  out << "  beq  $t5, $zero, printf_use_a1\n";
  out << "  li   $t1, 1\n";
  out << "  beq  $t5, $t1,  printf_use_a2\n";
  out << "  li   $t1, 2\n";
  out << "  beq  $t5, $t1,  printf_use_a3\n";
  out << "  addiu $t1, $t5, -3\n";
  out << "  sll  $t1, $t1, 2\n";
  out << "  addu $t7, $t6, $t1\n";
  out << "  lw   $a0, 0($t7)\n";
  out << "  j printf_print_int\n";
  out << "printf_use_a1:\n";
  out << "  move $a0, $a1\n";
  out << "  j printf_print_int\n";
  out << "printf_use_a2:\n";
  out << "  move $a0, $a2\n";
  out << "  j printf_print_int\n";
  out << "printf_use_a3:\n";
  out << "  move $a0, $a3\n";
  out << "printf_print_int:\n";
  out << "  addiu $t5, $t5, 1\n";
  out << "  li   $v0, 1\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
  out << "printf_end:\n";
  out << "  move $v0, $zero\n";
  out << "  jr   $ra\n\n";
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

  int spillBaseOffset = frameSize_;
  const auto &spilled = _regAllocator.getSpilledNodes();
  for (int tempId : spilled) {
    _spillOffsets[tempId] = spillBaseOffset;
    spillBaseOffset += 4;
  }
  frameSize_ = spillBaseOffset;
  int rem = frameSize_ % 8;
  if (rem != 0)
    frameSize_ += (8 - rem);

  out << func->getName() << ":\n";
  if (frameSize_ < 8)
    frameSize_ = 8;
  out << "  addiu $sp,$sp,-" << frameSize_ << "\n";
  out << "  sw $ra, 0($sp)\n";
  out << "  sw $fp, 4($sp)\n";
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

    int offCaller = frameSize_ + 8 + (i - 4) * 4;
    out << "  lw $t8, " << offCaller << "($fp)\n";
    out << "  sw $t8, " << offLocal << "($fp)\n";
  }
  currentEpilogueLabel_ = func->getName() + std::string("_END");

  if (func->getName() == "main") {
    for (auto *inst : curMod_->globals) {
      lowerInstruction(inst, out);
    }
  }

  for (auto &blk : func->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      lowerInstruction(&inst, out);
    }
  }
  out << currentEpilogueLabel_ << ":\n";
  out << "  lw $ra, 0($fp)\n";
  out << "  lw $fp, 4($fp)\n";
  out << "  addiu $sp, $sp, " << frameSize_ << "\n";
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

  auto labelName = [&](const Operand &o) {
    return curFuncName_ + std::string("_L") + std::to_string(o.asInt());
  };

  auto storeToSpill = [&](int tempId, const std::string &reg) {
    if (_regAllocator.isSpilled(tempId)) {
      out << "  sw " << reg << ", " << _spillOffsets[tempId] << "($fp)\n";
    }
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
    std::string rcond = ensureInReg(a1, out);
    out << "  bne " << rcond << ", $zero, " << labelName(res) << "\n";
    break;
  }
  case OpCode::ASSIGN: {
    if (isTemp(res)) {
      std::string dst = regForTemp(res.asInt());
      if (isConst(a1)) {
        out << "  li " << dst << ", " << a1.asInt() << "\n";
      } else {
        std::string rsrc = ensureInReg(a1, out);
        out << "  move " << dst << ", " << rsrc << "\n";
      }
      storeToSpill(res.asInt(), dst);
    } else if (isVar(res)) {
      std::string rsrc = ensureInReg(a1, out);
      auto sym = res.asSymbol();
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        out << "  sw " << rsrc << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << sym->name << "\n";
        out << "  sw " << rsrc << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::ADD:
  case OpCode::SUB:
  case OpCode::MUL:
  case OpCode::DIV:
  case OpCode::MOD: {
    std::string ra = ensureInReg(a1, out, "$t9", "$t8");
    std::string rb = ensureInReg(a2, out, "$t7", "$t7");
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    if (isTemp(res) && _regAllocator.isSpilled(res.asInt())) {
      rd = "$t6";
    }

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
    } else if (isVar(res)) {
      auto sym = res.asSymbol();
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << sym->name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::NEG: {
    std::string ra = ensureInReg(a1, out);
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    if (isTemp(res) && _regAllocator.isSpilled(res.asInt())) {
      rd = "$t6";
    }
    out << "  subu " << rd << ", $zero, " << ra << "\n";
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    } else if (isVar(res)) {
      auto sym = res.asSymbol();
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << sym->name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::EQ:
  case OpCode::NEQ:
  case OpCode::LT:
  case OpCode::LE:
  case OpCode::GT:
  case OpCode::GE: {
    std::string ra = ensureInReg(a1, out, "$t9", "$t8");
    std::string rb = ensureInReg(a2, out, "$t7", "$t7");
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    if (isTemp(res) && _regAllocator.isSpilled(res.asInt())) {
      rd = "$t6";
    }
    switch (op) {
    case OpCode::LT:
      out << "  slt " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::GT:
      out << "  slt " << rd << ", " << rb << ", " << ra << "\n";
      break;
    case OpCode::LE:
      out << "  slt $t6, " << rb << ", " << ra << "\n";
      out << "  xori " << rd << ", $t6, 1\n";
      break;
    case OpCode::GE:
      out << "  slt $t6, " << ra << ", " << rb << "\n";
      out << "  xori " << rd << ", $t6, 1\n";
      break;
    case OpCode::EQ:
      out << "  subu $t6, " << ra << ", " << rb << "\n";
      out << "  sltiu " << rd << ", $t6, 1\n";
      break;
    case OpCode::NEQ:
      out << "  subu $t6, " << ra << ", " << rb << "\n";
      out << "  sltu " << rd << ", $zero, $t6\n";
      break;
    default:
      break;
    }
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    } else if (isVar(res)) {
      auto sym = res.asSymbol();
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << sym->name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::NOT: {
    std::string ra = ensureInReg(a1, out);
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    if (isTemp(res) && _regAllocator.isSpilled(res.asInt())) {
      rd = "$t6";
    }
    out << "  sltiu " << rd << ", " << ra << ", 1\n";
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    } else if (isVar(res)) {
      auto sym = res.asSymbol();
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << sym->name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::AND:
  case OpCode::OR: {
    std::string ra = ensureInReg(a1, out);
    std::string rb = ensureInReg(a2, out);
    std::string ra1 = "$t6";
    std::string rb1 = "$t7";
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t5";
    if (isTemp(res) && _regAllocator.isSpilled(res.asInt())) {
      rd = "$t5";
    }
    out << "  sltu " << ra1 << ", $zero, " << ra << "\n";
    out << "  sltu " << rb1 << ", $zero, " << rb << "\n";
    if (op == OpCode::AND)
      out << "  and " << rd << ", " << ra1 << ", " << rb1 << "\n";
    else
      out << "  or " << rd << ", " << ra1 << ", " << rb1 << "\n";
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    } else if (isVar(res)) {
      auto sym = res.asSymbol();
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << sym->name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::LOAD: {
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    if (isTemp(res) && _regAllocator.isSpilled(res.asInt())) {
      rd = "$t6";
    }
    if (isVar(a1)) {
      auto sym = a1.asSymbol();
      std::string name = sym->name;
      bool isArray =
          sym && sym->type && sym->type->category == Type::Category::Array;
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        if (isArray) {
          out << "  addiu " << rd << ", $fp, " << lit->second.offset << "\n";
        } else {
          out << "  lw " << rd << ", " << lit->second.offset << "($fp)\n";
        }
      } else {
        out << "  la " << rd << ", " << name << "\n";
      }
      if (isConst(a2)) {
        int off = a2.asInt() * 4;
        out << "  lw " << rd << ", " << off << "(" << rd << ")\n";
      } else {
        if (a2.getType() == OperandType::Empty) {
          // do nothing, rd already holds the address or value
        } else {
          std::string ri = ensureInReg(a2, out, "$t9", "$t7");
          out << "  sll $t7, " << ri << ", 2\n";
          out << "  addu " << rd << ", " << rd << ", $t7\n";
          out << "  lw " << rd << ", 0(" << rd << ")\n";
        }
      }
    }
    if (isTemp(res)) {
      storeToSpill(res.asInt(), rd);
    } else if (isVar(res)) {
      auto sym = res.asSymbol();
      auto lit = locals_.find(sym.get());
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << sym->name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::STORE: {
    std::string rv = ensureInReg(a1, out);
    if (isVar(a2)) {
      auto sym = a2.asSymbol();
      std::string name = sym->name;
      bool isArray =
          sym && sym->type && sym->type->category == Type::Category::Array;
      auto lit = locals_.find(sym.get());
      std::string baseReg = "$t8";
      if (lit != locals_.end()) {
        if (isArray) {
          out << "  addiu " << baseReg << ", $fp, " << lit->second.offset
              << "\n";
        } else {
          out << "  lw " << baseReg << ", " << lit->second.offset << "($fp)\n";
        }
      } else {
        out << "  la " << baseReg << ", " << name << "\n";
      }

      auto &idxOp = res;
      if (isConst(idxOp)) {
        out << "  sw " << rv << ", " << (idxOp.asInt() * 4) << "(" << baseReg
            << ")\n";
      } else {
        std::string ri = ensureInReg(idxOp, out, "$t9", "$t9");
        out << "  sll $t9, " << ri << ", 2\n";
        out << "  addu " << baseReg << ", " << baseReg << ", $t9\n";
        out << "  sw " << rv << ", 0(" << baseReg << ")\n";
      }
    }
    break;
  }
  case OpCode::PARAM: {
    if (isConst(a1) && isVar(res)) {
      break;
    }
    int idx = paramIndex_++;
    if (idx < 4) {
      static const char *aregs[4] = {"$a0", "$a1", "$a2", "$a3"};
      if (isVar(a1)) {
        std::string name = a1.asSymbol()->name;
        bool isStr = curMod_ && curMod_->stringLiterals.find(name) !=
                                    curMod_->stringLiterals.end();
        if (isStr) {
          out << "  la " << aregs[idx] << ", " << name << "\n";
        } else {
          std::string r = ensureInReg(a1, out);
          out << "  move " << aregs[idx] << ", " << r << "\n";
        }
      } else if (isConst(a1)) {
        out << "  li " << aregs[idx] << ", " << a1.asInt() << "\n";
      } else {
        std::string r = ensureInReg(a1, out);
        out << "  move " << aregs[idx] << ", " << r << "\n";
      }
    } else {
      pendingExtraArgs_.push_back(a1);
    }
    break;
  }
  case OpCode::CALL: {
    int saveSize = regs_.size() * 4;
    if (saveSize > 0) {
      out << "  addiu $sp, $sp, -" << saveSize << "\n";
      for (size_t i = 0; i < regs_.size(); ++i) {
        out << "  sw " << regs_[i].name << ", " << (i * 4) << "($sp)\n";
      }
    }

    int extraCount = pendingExtraArgs_.size();
    if (extraCount > 0) {
      int extraBytes = extraCount * 4;
      out << "  addiu $sp, $sp, -" << extraBytes << "\n";
      out << "  addiu $t6, $sp, 0\n";
      for (int i = 0; i < extraCount; ++i) {
        const Operand &arg = pendingExtraArgs_[i];
        std::string r = ensureInReg(arg, out);
        int off = i * 4;
        out << "  sw " << r << ", " << off << "($t6)\n";
      }
    } else {
      out << "  move $t6, $zero\n";
    }

    std::string fname = isVar(a2) ? a2.asSymbol()->name : std::string("func");
    out << "  jal " << fname << "\n";

    if (extraCount > 0) {
      int extraBytes = extraCount * 4;
      out << "  addiu $sp, $sp, " << extraBytes << "\n";
      pendingExtraArgs_.clear();
    }

    if (saveSize > 0) {
      for (size_t i = 0; i < regs_.size(); ++i) {
        out << "  lw " << regs_[i].name << ", " << (i * 4) << "($sp)\n";
      }
      out << "  addiu $sp, $sp, " << saveSize << "\n";
    }

    if (isTemp(res)) {
      std::string rd = regForTemp(res.asInt());
      out << "  move " << rd << ", $v0\n";
      storeToSpill(res.asInt(), rd);
    }

    paramIndex_ = 0;
    break;
  }
  case OpCode::RETURN: {
    if (res.getType() != OperandType::Empty) {
      if (isConst(res)) {
        out << "  li $v0, " << res.asInt() << "\n";
      } else {
        std::string r = ensureInReg(res, out);
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

std::string AsmGen::ensureInReg(const Operand &op, std::ostream &out,
                                const char *immScratch,
                                const char *varScratch) {
  switch (op.getType()) {
  case OperandType::Temporary:
    if (_regAllocator.isSpilled(op.asInt())) {
      out << "  lw " << varScratch << ", " << _spillOffsets.at(op.asInt())
          << "($fp)\n";
      return varScratch;
    }
    return regForTemp(op.asInt());
  case OperandType::ConstantInt:
    out << "  li " << immScratch << ", " << op.asInt() << "\n";
    return immScratch;
  case OperandType::Variable: {
    auto sym = op.asSymbol();
    std::string name = sym->name;
    bool isArray =
        sym && sym->type && sym->type->category == Type::Category::Array;
    auto lit = locals_.find(sym.get());
    if (lit != locals_.end()) {
      if (isArray) {
        out << "  addiu " << varScratch << ", $fp, " << lit->second.offset
            << "\n";
      } else {
        out << "  lw " << varScratch << ", " << lit->second.offset << "($fp)\n";
      }
      return varScratch;
    }
    if (isArray) {
      out << "  la " << varScratch << ", " << name << "\n";
      return varScratch;
    } else {
      out << "  la " << varScratch << ", " << name << "\n";
      out << "  lw " << varScratch << ", 0(" << varScratch << ")\n";
      return varScratch;
    }
  }
  case OperandType::Label:
    comment(out, "unexpected label operand where register expected");
    return "$zero";
  case OperandType::Empty:
    return "$zero";
  }
  return "$zero";
}

void AsmGen::comment(std::ostream &out, const std::string &txt) {
  if (emitComments_) {
    out << "# " << txt << "\n";
  }
}

void AsmGen::analyzeGlobals(const IRModuleView &mod) {
  for (auto *inst : mod.globals) {
    if (inst->getOp() == OpCode::ALLOCA) {
      const auto &a1 = inst->getArg1();
      if (a1.getType() == OperandType::Variable) {
        std::string name = a1.asSymbol()->name;
        int sz = 1;
        if (inst->getResult().getType() == OperandType::ConstantInt) {
          sz = inst->getResult().asInt();
        }
      }
    }
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
  if (!func)
    return;
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
            int words = sz > 0 ? sz : 1;
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
    frameSize_ += (8 - rem);
}
