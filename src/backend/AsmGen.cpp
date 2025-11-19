#include "backend/AsmGen.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include <iostream>

using namespace lcc::backend;
using namespace lcc::codegen;

AsmGen::AsmGen() {
  // Reserve $t7, $t8, $t9 exclusively for assembler scratch usage to avoid
  // clobbering temporary results. Map temporaries only to $t0 - $t6.
  static const char *tRegs[7] = {"$t0", "$t1", "$t2", "$t3",
                                 "$t4", "$t5", "$t6"};
  regs_.reserve(7);
  for (int i = 0; i < 7; ++i) {
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
    if (it != gmap.end() && idx >= 0 && idx < (int)it->second.inits.size()) {
      it->second.inits[idx] = val;
    }
  };
  for (size_t i = 0; i < mod.globals.size(); ++i) {
    const Instruction *inst = mod.globals[i];
    if (!inst)
      continue;
    switch (inst->getOp()) {
    case OpCode::DEF: {
      auto t1 = inst->getArg1().getType();
      auto t2 = inst->getArg2().getType();
      auto tr = inst->getResult().getType();
      if (t1 == OperandType::Variable &&
          (t2 == OperandType::ConstantInt || tr == OperandType::ConstantInt)) {
        std::string name = inst->getArg1().asSymbol()->name;
        int sz = (t2 == OperandType::ConstantInt) ? inst->getArg2().asInt()
                                                  : inst->getResult().asInt();
        if (sz < 1)
          sz = 1;
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
      const Operand &idxOp = inst->getResult();
      if (val.getType() == OperandType::ConstantInt &&
          base.getType() == OperandType::Variable &&
          idxOp.getType() == OperandType::ConstantInt) {
        std::string name = base.asSymbol()->name;
        setVal(name, idxOp.asInt(), val.asInt());
      }
      break;
    }
    default:
      break;
    }
  }
  std::unordered_map<int, int> tempValues;
  std::unordered_map<std::string, int> scalarValues;
  for (auto &kvInit : gmap) {
    if (kvInit.second.defined && kvInit.second.size == 1) {
      scalarValues[kvInit.first] = kvInit.second.inits[0];
    }
  }
  auto isConstArrayElement = [&](const std::string &arrName, int idx,
                                 int &outVal) -> bool {
    auto it = gmap.find(arrName);
    if (it == gmap.end())
      return false;
    if (!it->second.defined)
      return false;
    if (idx < 0 || idx >= it->second.size)
      return false;
    outVal = it->second.inits[idx];
    return true;
  };
  for (size_t i = 0; i < mod.globals.size(); ++i) {
    const Instruction *inst = mod.globals[i];
    if (!inst)
      continue;
    switch (inst->getOp()) {
    case OpCode::ASSIGN: {
      const Operand &src = inst->getArg1();
      const Operand &dst = inst->getResult();
      if (src.getType() == OperandType::ConstantInt &&
          dst.getType() == OperandType::Variable) {
        scalarValues[dst.asSymbol()->name] = src.asInt();
      } else if (src.getType() == OperandType::Temporary &&
                 dst.getType() == OperandType::Variable) {
        int tid = src.asInt();
        auto tIt = tempValues.find(tid);
        if (tIt != tempValues.end()) {
          scalarValues[dst.asSymbol()->name] = tIt->second;
        }
      }
      break;
    }
    case OpCode::LOAD: {
      // LOAD base, idx, dstTemp
      const Operand &base = inst->getArg1();
      const Operand &idxOp = inst->getArg2();
      const Operand &dst = inst->getResult();
      if (base.getType() == OperandType::Variable &&
          dst.getType() == OperandType::Temporary) {
        int idxVal = -1;
        bool hasIdx = false;
        if (idxOp.getType() == OperandType::ConstantInt) {
          idxVal = idxOp.asInt();
          hasIdx = true;
        } else if (idxOp.getType() == OperandType::Variable) {
          auto vname = idxOp.asSymbol()->name;
          auto svIt = scalarValues.find(vname);
          if (svIt != scalarValues.end()) {
            idxVal = svIt->second;
            hasIdx = true;
          }
        }
        if (hasIdx) {
          int val = 0;
          if (isConstArrayElement(base.asSymbol()->name, idxVal, val)) {
            tempValues[dst.asInt()] = val;
          }
        }
      }
      break;
    }
    default:
      break;
    }
  }
  for (auto &pair : scalarValues) {
    auto gIt = gmap.find(pair.first);
    if (gIt != gmap.end() && gIt->second.size == 1) {
      gIt->second.inits[0] = pair.second;
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
    if (!func)
      continue;
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
  out << "  # got '%'\n";
  out << "  lbu  $t2, 0($t4)\n";
  out << "  addiu $t4, $t4, 1\n";
  out << "  li   $t1, 'd'\n";
  out << "  beq  $t2, $t1, printf_emit_int\n";
  out << "  # unknown specifier -> print the char literally\n";
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
  out << "  # select arg by index in $t5 (0->$a1,1->$a2,2->$a3)\n";
  out << "  beq  $t5, $zero, printf_use_a1\n";
  out << "  li   $t1, 1\n";
  out << "  beq  $t5, $t1,  printf_use_a2\n";
  out << "  li   $t1, 2\n";
  out << "  beq  $t5, $t1,  printf_use_a3\n";
  out << "  # too many args: print '?'\n";
  out << "  li   $a0, '?'\n";
  out << "  li   $v0, 11\n";
  out << "  syscall\n";
  out << "  j printf_loop\n";
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
  out << func->getName() << ":\n";
  if (frameSize_ < 8)
    frameSize_ = 8; // reserve at least slots for $ra(0) and $fp(4)
  out << "addiu $sp,$sp,-" << frameSize_ << "\n";
  out << "sw $ra,0($sp)\n";
  out << "sw $fp,4($sp)\n";
  out << "move $fp, $sp\n";

  static const char *aregs[4] = {"$a0", "$a1", "$a2", "$a3"};
  size_t fsz = formalParamByIndex_.size();
  for (size_t i = 0; i < fsz && i < 4; ++i) {
    const std::string &v = formalParamByIndex_[i];
    if (v.empty())
      continue;
    auto it = locals_.find(v);
    if (it != locals_.end()) {
      int off = it->second.offset;
      out << "sw " << aregs[i] << ", " << off << "($fp)\n";
    }
  }
  for (size_t i = 4; i < fsz; ++i) {
    const std::string &v = formalParamByIndex_[i];
    if (v.empty())
      continue;
    auto it = locals_.find(v);
    if (it == locals_.end())
      continue;
    int offLocal = it->second.offset;
    int offCaller = static_cast<int>((i - 4) * 4);
    int baseFromFp = frameSize_ + 16 + offCaller;
    out << "lw $t8, " << baseFromFp << "($fp)\n";
    out << "sw $t8, " << offLocal << "($fp)\n";
  }
  currentEpilogueLabel_ = func->getName() + std::string("_END");

  for (auto &blk : func->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      lowerInstruction(&inst, out);
    }
  }
  out << currentEpilogueLabel_ << ":\n";
  if (func->getName() == "main") {
    out << "  move $sp, $fp\n";
    out << "  lw $fp,4($sp)\n";
    out << "  lw $ra,0($sp)\n";
    out << "  addiu $sp,$sp," << frameSize_ << "\n";
    out << "  li $v0, 10\n";
    out << "  syscall\n";
  } else {
    out << "  move $sp, $fp\n";
    out << "  lw $fp,4($sp)\n";
    out << "  lw $ra,0($sp)\n";
    out << "  addiu $sp,$sp," << frameSize_ << "\n";
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
    } else if (isVar(res)) {
      std::string rsrc = ensureInReg(a1, out);
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "  sw " << rsrc << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << name << "\n";
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
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::NEG: {
    std::string ra = ensureInReg(a1, out);
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    out << "  subu " << rd << ", $zero, " << ra << "\n";
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << name << "\n";
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
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset
            << "($fp)\n"; // store comparison result in local
      } else {
        out << "  la $t7, " << name << "\n";
        out << "sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::NOT: {
    std::string ra = ensureInReg(a1, out);
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    out << "  sltiu " << rd << ", " << ra << ", 1\n";
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << name << "\n";
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
    out << "  sltu " << ra1 << ", $zero, " << ra << "\n";
    out << "  sltu " << rb1 << ", $zero, " << rb << "\n";
    if (op == OpCode::AND)
      out << "  and " << rd << ", " << ra1 << ", " << rb1 << "\n";
    else
      out << "  or " << rd << ", " << ra1 << ", " << rb1 << "\n";
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::LOAD: {
    // LOAD base, idx, dst
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    if (isVar(a1)) {
      auto sym = a1.asSymbol();
      std::string name = sym->name;
      bool isArray =
          sym && sym->type && sym->type->category == Type::Category::Array;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        if (isArray) {
          out << "  addiu $t8, $fp, " << lit->second.offset << "\n";
        } else {
          out << "  lw $t8, " << lit->second.offset << "($fp)\n";
        }
      } else {
        if (isArray) {
          out << "  la $t8, " << name << "\n";
        } else {
          out << "  la $t8, " << name << "\n";
          out << "  lw $t8, 0($t8)\n";
        }
      }
      if (isConst(a2)) {
        int off = a2.asInt() * 4;
        out << "  lw " << rd << ", " << off << "($t8)\n";
      } else {
        std::string ri = ensureInReg(a2, out, "$t9", "$t7");
        out << "  sll $t7, " << ri << ", 2\n";
        out << "  addu $t8, $t8, $t7\n";
        out << "  lw " << rd << ", 0($t8)\n";
      }
    }
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "  sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "  la $t7, " << name << "\n";
        out << "  sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::STORE: {
    // STORE val, base, index
    std::string rv = ensureInReg(a1, out);
    if (isVar(a2)) {
      auto sym = a2.asSymbol();
      std::string name = sym->name;
      bool isArray =
          sym && sym->type && sym->type->category == Type::Category::Array;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        if (isArray) {
          out << "  addiu $t8, $fp, " << lit->second.offset << "\n";
        } else {
          out << "  lw $t8, " << lit->second.offset << "($fp)\n";
        }
      } else {
        if (isArray) {
          out << "  la $t8, " << name << "\n";
        } else {
          out << "  la $t8, " << name << "\n";
          out << "  lw $t8, 0($t8)\n";
        }
      }
    }
    auto &idxOp = res;
    if (isConst(idxOp)) {
      out << "  sw " << rv << ", " << (idxOp.asInt() * 4) << "($t8)\n";
    } else {
      std::string ri = ensureInReg(idxOp, out, "$t9", "$t7");
      out << "  sll $t7, " << ri << ", 2\n";
      out << "  addu $t8, $t8, $t7\n";
      out << "  sw " << rv << ", 0($t8)\n";
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
          auto sym = a1.asSymbol();
          bool isArray =
              sym && sym->type && sym->type->category == Type::Category::Array;

          if (locals_.find(name) == locals_.end()) {
            out << "  la " << aregs[idx] << ", " << name << "\n";
            if (!isArray) {
              out << "  lw " << aregs[idx] << ", 0(" << aregs[idx] << ")\n";
            }
          } else {
            std::string r = ensureInReg(a1, out);
            out << "  move " << aregs[idx] << ", " << r << "\n";
          }
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
    if (!pendingExtraArgs_.empty()) {
      int n = static_cast<int>(pendingExtraArgs_.size());
      int total = 16 + n * 4;
      out << "  move $t1, $sp\n";
      out << "  addiu $sp, $sp, -" << total << "\n";
      out << "  addiu $t1, $t1, 16\n";
      for (int i = 0; i < n; ++i) {
        const Operand &arg = pendingExtraArgs_[static_cast<size_t>(i)];
        std::string r = ensureInReg(arg, out);
        int off = i * 4;
        out << "  sw " << r << ", " << off << "($t1)\n";
      }
    }
    std::string fname = isVar(a2) ? a2.asSymbol()->name : std::string("func");
    out << "  jal " << fname << "\n";
    if (isTemp(res)) {
      std::string rd = regForTemp(res.asInt());
      out << "  move " << rd << ", $v0\n";
    }
    if (!pendingExtraArgs_.empty()) {
      int n = static_cast<int>(pendingExtraArgs_.size());
      int bytes = 16 + n * 4;
      out << "  addiu $sp, $sp, " << bytes << "\n";
      pendingExtraArgs_.clear();
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
  case OpCode::DEF: {
    break;
  }
  case OpCode::PRINTF: {
    break;
  }
  }
}

std::string AsmGen::regForTemp(int tempId) const {
  int idx = (tempId % static_cast<int>(regs_.size()));
  return regs_[idx].name;
}

std::string AsmGen::ensureInReg(const Operand &op, std::ostream &out,
                                const char *immScratch,
                                const char *varScratch) {
  switch (op.getType()) {
  case OperandType::Temporary:
    return regForTemp(op.asInt());
  case OperandType::ConstantInt:
    out << "  li " << immScratch << ", " << op.asInt() << "\n";
    return immScratch;
  case OperandType::Variable: {
    auto sym = op.asSymbol();
    std::string name = sym->name;
    bool isArray =
        sym && sym->type && sym->type->category == Type::Category::Array;
    auto lit = locals_.find(name);
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

int AsmGen::acquireTempRegister(int tempId) {
  for (size_t i = 0; i < regs_.size(); ++i) {
    if (!regs_[i].inUse) {
      regs_[i].inUse = true;
      regs_[i].tempId = tempId;
      return static_cast<int>(i);
    }
  }
  return -1;
}

void AsmGen::releaseTempRegister(int tempId) {
  for (auto &r : regs_) {
    if (r.inUse && r.tempId == tempId) {
      r.inUse = false;
      r.tempId = -1;
      return;
    }
  }
}

void AsmGen::comment(std::ostream &out, const std::string &txt) {
  if (emitComments_) {
    out << "# " << txt << "\n";
  }
}

void AsmGen::analyzeGlobals(const IRModuleView &mod) {
  globals_.clear();
  globalSizes_.clear();
  for (auto *inst : mod.globals) {
    if (!inst)
      continue;
    if (inst->getOp() == OpCode::DEF) {
      const auto &a1 = inst->getArg1();
      if (a1.getType() == OperandType::Variable) {
        std::string name = a1.asSymbol()->name;
        globals_.insert(name);
        int sz = 1;
        if (inst->getArg2().getType() == OperandType::ConstantInt) {
          sz = inst->getArg2().asInt();
        } else if (inst->getResult().getType() == OperandType::ConstantInt) {
          sz = inst->getResult().asInt();
        }
        if (sz < 1)
          sz = 1;
        globalSizes_[name] = sz;
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
}

void AsmGen::analyzeFunctionLocals(const Function *func) {
  int nextOffset = 8; // 0: $ra, 4: $fp, >=8: locals/params
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
            if (formalParamByIndex_.size() <= static_cast<size_t>(idx))
              formalParamByIndex_.resize(static_cast<size_t>(idx + 1));
            std::string vname = res.asSymbol()->name;
            formalParamByIndex_[static_cast<size_t>(idx)] = vname;
            if (locals_.find(vname) == locals_.end()) {
              locals_[vname] = {nextOffset, 1};
              nextOffset += 4;
            }
          }
          continue;
        }
      }
      if (op == OpCode::DEF) {
        const auto &sym = inst.getArg1();
        int sz = 1;
        if (inst.getArg2().getType() == OperandType::ConstantInt) {
          sz = inst.getArg2().asInt();
        } else if (inst.getResult().getType() == OperandType::ConstantInt) {
          sz = inst.getResult().asInt();
        }
        if (sym.getType() == OperandType::Variable) {
          std::string name = sym.asSymbol()->name;
          if (locals_.find(name) == locals_.end()) {
            int words = sz > 0 ? sz : 1;
            locals_[name] = {nextOffset, words};
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
  // align frame size to 8 bytes
  frameSize_ = nextOffset;
  int rem = frameSize_ % 8;
  if (rem != 0)
    frameSize_ += (8 - rem);
}
