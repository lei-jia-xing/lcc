#include "backend/AsmGen.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include <iostream>

using namespace lcc::backend;
using namespace lcc::codegen;

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
  out << "# printf shim: supports only %d (up to 3 integer args in $a1-$a3)\n";
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

  // Bind formal parameters ($a0-$a3) into their local stack slots
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
  // For parameters beyond 4: load from caller stack at entry
  // old_sp = $fp + frameSize_ ; extras start at old_sp + 16 (home area for
  // $a0-$a3)
  if (fsz > 4) {
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
  }
  // Walk basic blocks sequentially (current IR stores linear order)
  for (auto &blk : func->getBlocks()) {
    comment(out, std::string("block ") + std::to_string(blk->getId()));
    for (auto &inst : blk->getInstructions()) {
      lowerInstruction(&inst, out);
    }
  }
  // Epilogue
  if (func->getName() == "main") {
    // Exit program in SPIM/MARS: syscall 10 (exit)
    out << "move $sp, $fp\n";
    out << "lw $fp,4($sp)\n";
    out << "lw $ra,0($sp)\n";
    out << "addiu $sp,$sp," << frameSize_ << "\n";
    out << "li $v0, 10\n";
    out << "syscall\n";
  } else {
    out << "move $sp, $fp\n";
    out << "lw $fp,4($sp)\n";
    out << "lw $ra,0($sp)\n";
    out << "addiu $sp,$sp," << frameSize_ << "\n";
    comment(out, "epilogue");
    out << "jr $ra\n";
  }
  out << "\n";
  curFuncName_.clear();
}

void AsmGen::lowerInstruction(const Instruction *inst, std::ostream &out) {
  auto op = inst->getOp();
  auto opToStr = [&](OpCode oc) {
    switch (oc) {
    case OpCode::ADD:
      return "ADD";
    case OpCode::SUB:
      return "SUB";
    case OpCode::MUL:
      return "MUL";
    case OpCode::DIV:
      return "DIV";
    case OpCode::MOD:
      return "MOD";
    case OpCode::NEG:
      return "NEG";
    case OpCode::EQ:
      return "EQ";
    case OpCode::NEQ:
      return "NEQ";
    case OpCode::LT:
      return "LT";
    case OpCode::LE:
      return "LE";
    case OpCode::GT:
      return "GT";
    case OpCode::GE:
      return "GE";
    case OpCode::AND:
      return "AND";
    case OpCode::OR:
      return "OR";
    case OpCode::NOT:
      return "NOT";
    case OpCode::ASSIGN:
      return "ASSIGN";
    case OpCode::LOAD:
      return "LOAD";
    case OpCode::STORE:
      return "STORE";
    case OpCode::IF:
      return "IF";
    case OpCode::GOTO:
      return "GOTO";
    case OpCode::LABEL:
      return "LABEL";
    case OpCode::PARAM:
      return "PARAM";
    case OpCode::CALL:
      return "CALL";
    case OpCode::RETURN:
      return "RETURN";
    case OpCode::PRINTF:
      return "PRINTF";
    case OpCode::DEF:
      return "DEF";
    }
    return "OP";
  };

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
      out << "j " << labelName(res) << "\n";
    }
    break;
  }
  case OpCode::IF: {
    // IF cond, label (label in result)
    std::string rcond = ensureInReg(a1, out);
    out << "bne " << rcond << ", $zero, " << labelName(res) << "\n";
    break;
  }
  case OpCode::ASSIGN: {
    if (isTemp(res)) {
      std::string dst = regForTemp(res.asInt());
      if (isConst(a1)) {
        out << "li " << dst << ", " << a1.asInt() << "\n";
      } else {
        std::string rsrc = ensureInReg(a1, out);
        out << "move " << dst << ", " << rsrc << "\n";
      }
    } else if (isVar(res)) {
      std::string rsrc = ensureInReg(a1, out);
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "sw " << rsrc << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "la $t7, " << name << "\n";
        out << "sw " << rsrc << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::ADD:
  case OpCode::SUB:
  case OpCode::MUL:
  case OpCode::DIV:
  case OpCode::MOD: {
    // Use distinct scratch registers for two operands to avoid clobbering
    std::string ra = ensureInReg(a1, out, "$t9", "$t8");
    std::string rb = ensureInReg(a2, out, "$t5", "$t7");
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    switch (op) {
    case OpCode::ADD:
      out << "addu " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::SUB:
      out << "subu " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::MUL:
      out << "mul " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::DIV:
      out << "div " << ra << ", " << rb << "\n";
      out << "mflo " << rd << "\n";
      break;
    case OpCode::MOD:
      out << "div " << ra << ", " << rb << "\n";
      out << "mfhi " << rd << "\n";
      break;
    default:
      break;
    }
    // If destination is a variable, store back
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "la $t7, " << name << "\n";
        out << "sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::NEG: {
    std::string ra = ensureInReg(a1, out);
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    out << "subu " << rd << ", $zero, " << ra << "\n";
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "la $t7, " << name << "\n";
        out << "sw " << rd << ", 0($t7)\n";
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
    std::string rb = ensureInReg(a2, out, "$t5", "$t7");
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    switch (op) {
    case OpCode::LT:
      out << "slt " << rd << ", " << ra << ", " << rb << "\n";
      break;
    case OpCode::GT:
      out << "slt " << rd << ", " << rb << ", " << ra << "\n";
      break;
    case OpCode::LE:
      out << "slt $t6, " << rb << ", " << ra << "\n";
      out << "xori " << rd << ", $t6, 1\n";
      break;
    case OpCode::GE:
      out << "slt $t6, " << ra << ", " << rb << "\n";
      out << "xori " << rd << ", $t6, 1\n";
      break;
    case OpCode::EQ:
      out << "subu $t6, " << ra << ", " << rb << "\n";
      out << "sltiu " << rd << ", $t6, 1\n";
      break;
    case OpCode::NEQ:
      out << "subu $t6, " << ra << ", " << rb << "\n";
      out << "sltu " << rd << ", $zero, $t6\n";
      break;
    default:
      break;
    }
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "sw " << rd << ", " << lit->second.offset << "($sp)\n";
      } else {
        out << "la $t7, " << name << "\n";
        out << "sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::NOT: {
    std::string ra = ensureInReg(a1, out);
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    out << "sltiu " << rd << ", " << ra << ", 1\n";
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "la $t7, " << name << "\n";
        out << "sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::AND:
  case OpCode::OR: {
    // 规范化为 0/1 后做位与/或
    std::string ra = ensureInReg(a1, out);
    std::string rb = ensureInReg(a2, out);
    std::string ra1 = "$t6";
    std::string rb1 = "$t7";
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t5";
    out << "sltu " << ra1 << ", $zero, " << ra << "\n";
    out << "sltu " << rb1 << ", $zero, " << rb << "\n";
    if (op == OpCode::AND)
      out << "and " << rd << ", " << ra1 << ", " << rb1 << "\n";
    else
      out << "or " << rd << ", " << ra1 << ", " << rb1 << "\n";
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "la $t7, " << name << "\n";
        out << "sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::LOAD: {
    // LOAD base, idx, dst
    std::string rd = isTemp(res) ? regForTemp(res.asInt()) : "$t6";
    if (isVar(a1)) {
      auto name = a1.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        // $t8 = $fp + offset
        out << "addiu $t8, $fp, " << lit->second.offset << "\n";
      } else {
        out << "la $t8, " << name << "\n";
      }
      if (isConst(a2)) {
        int off = a2.asInt() * 4;
        out << "lw " << rd << ", " << off << "($t8)\n";
      } else {
        std::string ri = ensureInReg(a2, out);
        out << "sll $t7, " << ri << ", 2\n";
        out << "addu $t8, $t8, $t7\n";
        out << "lw " << rd << ", 0($t8)\n";
      }
    }
    if (isVar(res)) {
      auto name = res.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "sw " << rd << ", " << lit->second.offset << "($fp)\n";
      } else {
        out << "la $t7, " << name << "\n";
        out << "sw " << rd << ", 0($t7)\n";
      }
    }
    break;
  }
  case OpCode::STORE: {
    // STORE val, base, index
    std::string rv = ensureInReg(a1, out);
    if (isVar(a2)) {
      auto name = a2.asSymbol()->name;
      auto lit = locals_.find(name);
      if (lit != locals_.end()) {
        out << "addiu $t8, $fp, " << lit->second.offset << "\n";
      } else {
        out << "la $t8, " << name << "\n";
      }
    }
    // index is in result operand per MakeStore
    auto &idxOp = res; // index stored in result per Instruction::MakeStore
    if (isConst(idxOp)) {
      out << "sw " << rv << ", " << (idxOp.asInt() * 4) << "($t8)\n";
    } else {
      std::string ri = ensureInReg(idxOp, out);
      out << "sll $t7, " << ri << ", 2\n";
      out << "addu $t8, $t8, $t7\n";
      out << "sw " << rv << ", 0($t8)\n";
    }
    break;
  }
  case OpCode::PARAM: {
    // Two forms:
    // 1) Formal parameter binding at function entry: a1=idx(const), result=var
    //    -> handled in prologue; here no-op.
    if (isConst(a1) && isVar(res)) {
      // formal binding already stored to stack; ignore
      break;
    }
    // 2) Call-site argument: collect or place into $a0-$a3
    int idx = paramIndex_++;
    if (idx < 4) {
      static const char *aregs[4] = {"$a0", "$a1", "$a2", "$a3"};
      if (isVar(a1)) {
        // string literal or variable
        std::string name = a1.asSymbol()->name;
        bool isStr = curMod_ && curMod_->stringLiterals.find(name) !=
                                    curMod_->stringLiterals.end();
        if (isStr) {
          out << "la " << aregs[idx] << ", " << name << "\n";
        } else {
          std::string r = ensureInReg(a1, out);
          out << "move " << aregs[idx] << ", " << r << "\n";
        }
      } else if (isConst(a1)) {
        out << "li " << aregs[idx] << ", " << a1.asInt() << "\n";
      } else {
        std::string r = ensureInReg(a1, out);
        out << "move " << aregs[idx] << ", " << r << "\n";
      }
    } else {
      // Buffer extra args; we'll push them at CALL in reverse order
      pendingExtraArgs_.push_back(a1);
    }
    break;
  }
  case OpCode::CALL: {
    // arg1 = argc, arg2 = func symbol, result = ret temp
    // Push extra (>4) args in reverse order so that at callee entry
    // arg with idx=4 is at 0($sp), idx=5 at 4($sp), ...
    if (!pendingExtraArgs_.empty()) {
      // Reserve 16-byte home area for $a0-$a3 plus space for extras.
      int n = static_cast<int>(pendingExtraArgs_.size());
      int total = 16 + n * 4;
      // Preserve old $sp in $t1 to compute base = old_sp + 16
      out << "move $t1, $sp\n";
      out << "addiu $sp, $sp, -" << total << "\n";
      out << "addiu $t1, $t1, 16\n"; // base to store extras
      for (int i = 0; i < n; ++i) {
        const Operand &arg = pendingExtraArgs_[static_cast<size_t>(i)];
        std::string r = ensureInReg(arg, out);
        int off = i * 4;
        out << "sw " << r << ", " << off << "($t1)\n";
      }
    }
    std::string fname = isVar(a2) ? a2.asSymbol()->name : std::string("func");
    out << "jal " << fname << "\n";
    if (isTemp(res)) {
      std::string rd = regForTemp(res.asInt());
      out << "move " << rd << ", $v0\n";
    }
    // Pop extra args
    if (!pendingExtraArgs_.empty()) {
      int n = static_cast<int>(pendingExtraArgs_.size());
      int bytes = 16 + n * 4;
      out << "addiu $sp, $sp, " << bytes << "\n";
      pendingExtraArgs_.clear();
    }
    paramIndex_ = 0; // reset
    break;
  }
  case OpCode::RETURN: {
    // Note: RETURN value is encoded in 'result' operand by MakeReturn
    if (res.getType() != OperandType::Empty) {
      if (isConst(res))
        out << "li $v0, " << res.asInt() << "\n";
      else {
        std::string r = ensureInReg(res, out);
        out << "move $v0, " << r << "\n";
      }
    }
    // epilogue and jr $ra 由 emitFunction 统一输出
    break;
  }
  case OpCode::DEF: {
    // 已由 .data 处理，这里忽略
    break;
  }
  case OpCode::PRINTF: {
    // 不会直接出现，printf 通过 PARAM/CALL 实现
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
    out << "li " << immScratch << ", " << op.asInt() << "\n";
    return immScratch;
  case OperandType::Variable: {
    auto name = op.asSymbol()->name;
    auto lit = locals_.find(name);
    if (lit != locals_.end()) {
      out << "lw " << varScratch << ", " << lit->second.offset << "($fp)\n";
      return varScratch;
    }
    out << "la " << varScratch << ", " << name << "\n";
    out << "lw " << varScratch << ", 0(" << varScratch << ")\n";
    return varScratch;
  }
  case OperandType::Label:
    // 不作为通用寄存器值使用
    comment(out, "unexpected label operand where register expected");
    return "$zero";
  case OperandType::Empty:
    return "$zero";
  }
  return "$zero";
}

int AsmGen::acquireTempRegister(int tempId) {
  // Very naive: first free reg
  for (size_t i = 0; i < regs_.size(); ++i) {
    if (!regs_[i].inUse) {
      regs_[i].inUse = true;
      regs_[i].tempId = tempId;
      return static_cast<int>(i);
    }
  }
  return -1; // signal spill needed later
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
  for (auto *inst : mod.globals) {
    if (!inst)
      continue;
    if (inst->getOp() == OpCode::DEF) {
      const auto &a1 = inst->getArg1();
      if (a1.getType() == OperandType::Variable) {
        globals_.insert(a1.asSymbol()->name);
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
  // Collect local DEFs and formal PARAM bindings at the beginning of block 0
  // Reserve 0($fp) for $ra, 4($fp) for saved $fp; locals start at 8($fp)
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
            // Ensure a local slot exists for this param (size 1)
            if (globals_.find(vname) == globals_.end() &&
                locals_.find(vname) == locals_.end()) {
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
          // DEF is encoded as (op, symbol, size) using the 2-operand ctor,
          // so size may be stored in 'result'
          sz = inst.getResult().asInt();
        }
        if (sym.getType() == OperandType::Variable) {
          std::string name = sym.asSymbol()->name;
          if (globals_.find(name) == globals_.end() &&
              locals_.find(name) == locals_.end()) {
            int words = sz > 0 ? sz : 1;
            locals_[name] = {nextOffset, words};
            nextOffset += words * 4;
          }
        }
        continue; // DEFs do not terminate the entry PARAM run
      }
      // Any other op ends the formal PARAM run
      if (isFirstBlock)
        inEntryParamRun = false;
    }
    isFirstBlock = false;
  }
  // Align frame size up to 8 bytes (MIPS o32 stack alignment)
  frameSize_ = nextOffset; // include $ra and $fp slots
  int rem = frameSize_ % 8;
  if (rem != 0)
    frameSize_ += (8 - rem);
}
