#include "codegen/CodeGen.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "parser/AST.hpp"
#include "semantic/Symbol.hpp"
#include <functional>
#include <iostream>

using namespace lcc::codegen;

void CodeGen::reset() {
  ctx_.func = nullptr;
  ctx_.curBlk.reset();
  ctx_.nextTempId = 0;
  ctx_.nextLabelId = 0;
  symbols_.clear();
  stringLiterals_.clear();
  nextStringId_ = 0;
}

Operand CodeGen::newTemp() { return Operand::Temporary(ctx_.nextTempId++); }

Operand CodeGen::newLabel() { return Operand::Label(ctx_.nextLabelId++); }

void CodeGen::placeLabel(const Operand &label) {
  emit(Instruction::MakeLabel(label));
}

std::shared_ptr<Symbol> CodeGen::internStringLiteral(const std::string &literal) {
  auto it = stringLiterals_.find(literal);
  if (it != stringLiterals_.end()) return it->second;
  // Create a unique symbol name for this literal
  std::string name = ".fmt" + std::to_string(nextStringId_++);
  auto sym = std::make_shared<Symbol>(name, nullptr, 0);
  stringLiterals_[literal] = sym;
  return sym;
}
void CodeGen::generate(CompUnit *root) {
  reset();
  if (!root) return;
  for (auto &Decl : root->decls) {
    genDecl(Decl.get());
  }
  for (auto &FuncDef : root->funcDefs) {
    genFunction(FuncDef.get());
  }
  if (root->mainFuncDef) {
    genMainFuncDef(root->mainFuncDef.get());
  }

}

void CodeGen::emit(const Instruction &inst) {
  if (ctx_.curBlk) {
    ctx_.curBlk->addInstruction(inst);
  }
}

Function CodeGen::genFunction(FuncDef *funcDef) {
  if (!funcDef)
    return Function("empty");

  Function func(funcDef->ident);

  auto savedFunc = ctx_.func;
  auto savedBlk = ctx_.curBlk;
  auto savedTempId = ctx_.nextTempId;
  auto savedLabelId = ctx_.nextLabelId;

  ctx_.func = &func;
  ctx_.nextTempId = 0;
  ctx_.nextLabelId = 0;

  ctx_.curBlk = func.createBlock();

  // Handle function parameters: define locals and receive args
  if (funcDef->params) {
    int idx = 0;
    for (auto &p : funcDef->params->params) {
      if (!p)
        continue;
      // Intern symbol for parameter name
      auto sym = internSymbol(p->ident, p->type);
      // Define storage for the parameter (scalar or array-as-pointer treated as
      // size 1)
      emit(Instruction::MakeDef(Operand::Variable(sym),
                                Operand::ConstantInt(1)));
      // Receive argument idx into this parameter variable: PARAM idx, <var>
      emit(Instruction(OpCode::PARAM, Operand::ConstantInt(idx),
                       Operand::Variable(sym)));
      idx++;
    }
  }

  if (funcDef->block) {
    genBlock(funcDef->block.get());
  }

  // Internalize IR printing here
  for (auto &blk : func.getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      std::cout << inst.toString() << '\n';
    }
  }

  ctx_.func = savedFunc;
  ctx_.curBlk = savedBlk;
  ctx_.nextTempId = savedTempId;
  ctx_.nextLabelId = savedLabelId;

  return func;
}

Function CodeGen::genMainFuncDef(MainFuncDef *mainDef) {
  Function func("main");

  auto savedFunc = ctx_.func;
  auto savedBlk = ctx_.curBlk;
  auto savedTempId = ctx_.nextTempId;
  auto savedLabelId = ctx_.nextLabelId;

  ctx_.func = &func;
  ctx_.nextTempId = 0;
  ctx_.nextLabelId = 0;

  ctx_.curBlk = func.createBlock();

  if (mainDef->block) {
    genBlock(mainDef->block.get());
  }

  // 不必补return 0 ,没有return 0, 根据语义直接就报错了
  // emit(Instruction::MakeReturn(Operand::ConstantInt(0)));

  // Internalize IR printing for main
  for (auto &blk : func.getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      std::cout << inst.toString() << '\n';
    }
  }

  ctx_.func = savedFunc;
  ctx_.curBlk = savedBlk;
  ctx_.nextTempId = savedTempId;
  ctx_.nextLabelId = savedLabelId;

  return func;
}
void CodeGen::genBlock(Block *block) {
  if (!block)
    return;

  for (auto &item : block->items) {
    if (item) {
      genBlockItem(item.get());
    }
  }
}

void CodeGen::genBlockItem(BlockItem *item) {
  if (!item)
    return;

  if (item->decl) {
    genDecl(item->decl.get());
  } else if (item->stmt) {
    genStmt(item->stmt.get());
  }
}

void CodeGen::genStmt(Stmt *stmt) {
  if (!stmt)
    return;

  switch (stmt->stmtType) {
  case Stmt::StmtType::ASSIGN:
    genAssign(static_cast<AssignStmt *>(stmt));
    break;
  case Stmt::StmtType::EXP:
    genExpStmt(static_cast<ExpStmt *>(stmt));
    break;
  case Stmt::StmtType::BLOCK:
    genBlock(static_cast<BlockStmt *>(stmt)->block.get());
    break;
  case Stmt::StmtType::IF:
    genIf(static_cast<IfStmt *>(stmt));
    break;
  case Stmt::StmtType::FOR:
    genFor(static_cast<ForStmt *>(stmt));
    break;
  case Stmt::StmtType::BREAK:
    genBreak(static_cast<BreakStmt *>(stmt));
    break;
  case Stmt::StmtType::CONTINUE:
    genContinue(static_cast<ContinueStmt *>(stmt));
    break;
  case Stmt::StmtType::RETURN:
    genReturn(static_cast<ReturnStmt *>(stmt));
    break;
  case Stmt::StmtType::PRINTF:
    genPrintf(static_cast<PrintfStmt *>(stmt));
    break;
  }
}

void CodeGen::genAssign(AssignStmt *stmt) {
  if (!stmt)
    return;

  Operand value = genExp(stmt->exp.get());
  Operand var = genLVal(stmt->lval.get());

  emit(Instruction::MakeAssign(value, var));
}
void CodeGen::genExpStmt(ExpStmt *stmt) {
  if (!stmt)
    return;

  if (stmt->exp) {
    genExp(stmt->exp.get());
  }
}

void CodeGen::genIf(IfStmt *stmt) {
  if (!stmt)
    return;

  Operand trueLabel = newLabel();
  Operand falseLabel = newLabel();
  Operand endLabel = newLabel();

  genCondBranch(stmt->cond.get(), trueLabel.asInt(), falseLabel.asInt());

  placeLabel(trueLabel);
  if (stmt->thenStmt) {
    genStmt(stmt->thenStmt.get());
  }
  emit(Instruction::MakeGoto(endLabel));

  placeLabel(falseLabel);
  if (stmt->elseStmt) {
    genStmt(stmt->elseStmt.get());
  }

  placeLabel(endLabel);
}

void CodeGen::genFor(ForStmt *stmt) {
  if (!stmt)
    return;

  // for 语义：init; while (cond) { body; step; }
  // 构造标签： 条件(L_cond) 体(L_body) 步进(L_step) 结束(L_end)
  Operand L_cond = newLabel();
  Operand L_body = newLabel();
  Operand L_step = newLabel();
  Operand L_end = newLabel();

  if (stmt->initStmt)
    genForAssign(stmt->initStmt.get());

  emit(Instruction::MakeGoto(L_cond));

  pushLoop(L_end.asInt(), L_step.asInt());

  placeLabel(L_body);
  if (stmt->bodyStmt)
    genStmt(stmt->bodyStmt.get());
  emit(Instruction::MakeGoto(L_step));

  placeLabel(L_step);
  if (stmt->updateStmt)
    genForAssign(stmt->updateStmt.get());
  emit(Instruction::MakeGoto(L_cond));

  placeLabel(L_cond);
  if (stmt->cond) {
    genCondBranch(stmt->cond.get(), L_body.asInt(), L_end.asInt());
  } else {
    emit(Instruction::MakeGoto(L_body));
  }

  placeLabel(L_end);
  popLoop();
}

void CodeGen::genBreak(BreakStmt *stmt) {
  if (!stmt)
    return;
  auto *loop = currentLoop();
  if (!loop)
    return;
  emit(Instruction::MakeGoto(Operand::Label(loop->breakLabel)));
}

void CodeGen::genContinue(ContinueStmt *stmt) {
  if (!stmt)
    return;
  auto *loop = currentLoop();
  if (!loop)
    return;
  emit(Instruction::MakeGoto(Operand::Label(loop->continueLabel)));
}

void CodeGen::genReturn(ReturnStmt *stmt) {
  if (!stmt)
    return;

  Operand result;
  if (stmt->exp) {
    result = genExp(stmt->exp.get());
  } else {
    // void
  }

  emit(Instruction::MakeReturn(result));
}

void CodeGen::genPrintf(PrintfStmt *stmt) {
  if (!stmt)
    return;
  // Lower printf to PARAMs + CALL printf
  // First param: format string literal symbol
  auto fmtSym = internStringLiteral(stmt->formatString);
  emit(Instruction(OpCode::PARAM, Operand::Variable(fmtSym), Operand()));
  // Then each expression argument
  int argc = 1; // include format string
  for (auto &e : stmt->args) {
    if (!e) continue;
    Operand v = genExp(e.get());
    emit(Instruction(OpCode::PARAM, v, Operand()));
    argc++;
  }
  // Call printf(symbol), argc, discard return
  Operand fnSym = Operand::Variable(internSymbol("printf"));
  Operand ret = newTemp(); // ignore by not using it later
  emit(Instruction::MakeCall(fnSym, argc, ret));
}

void CodeGen::genForAssign(ForAssignStmt *stmt) {
  if (!stmt)
    return;
  // 形如：i = i + 1, j = j + 2
  for (auto &as : stmt->assignments) {
    if (!as.lval || !as.exp)
      continue;
    Operand rhs = genExp(as.exp.get());
    Operand addr;
    Operand lhs = genLVal(as.lval.get(), &addr);
    if (addr.getType() == OperandType::Empty) {
      emit(Instruction::MakeAssign(rhs, lhs));
    } else {
      emit(Instruction::MakeStore(rhs, lhs, addr));
    }
  }
}
void CodeGen::genDecl(Decl *decl) {
  if (!decl)
    return;

  if (auto constDecl = dynamic_cast<ConstDecl *>(decl)) {
    genConstDecl(constDecl);
  } else if (auto varDecl = dynamic_cast<VarDecl *>(decl)) {
    genVarDecl(varDecl);
  }
}

void CodeGen::genConstDecl(ConstDecl *decl) {
  if (!decl)
    return;

  for (auto &constDef : decl->constDefs) {
    if (constDef) {
      genConstDef(constDef.get());
    }
  }
}

void CodeGen::genVarDecl(VarDecl *decl) {
  if (!decl)
    return;

  for (auto &varDef : decl->varDefs) {
    if (varDef) {
      genVarDef(varDef.get());
    }
  }
}

void CodeGen::genConstDef(ConstDef *def) {
  if (!def)
    return;
  auto sym = internSymbol(def->ident);
  Operand sizeOp = Operand::ConstantInt(1);
  if (def->arraySize) {
    Operand sz = genConstExp(def->arraySize.get());
    if (sz.getType() == OperandType::ConstantInt)
      sizeOp = sz;
  }
  emit(Instruction::MakeDef(Operand::Variable(sym), sizeOp));
  if (def->constinitVal) {
    genConstInitVal(def->constinitVal.get(), sym);
  }
}

void CodeGen::genVarDef(VarDef *def) {
  if (!def)
    return;
  auto sym = internSymbol(def->ident);
  Operand sizeOp = Operand::ConstantInt(1);
  if (def->arraySize) {
    Operand sz = genConstExp(def->arraySize.get());
    if (sz.getType() == OperandType::ConstantInt)
      sizeOp = sz;
  }
  emit(Instruction::MakeDef(Operand::Variable(sym), sizeOp));
  if (def->initVal) {
    genInitVal(def->initVal.get(), sym);
  }
}

void CodeGen::genConstInitVal(ConstInitVal *init,
                              const std::shared_ptr<Symbol> &sym) {
  if (!init || !sym)
    return;
  Operand var = Operand::Variable(sym);
  if (!init->isArray) {
    Operand v = genConstExp(init->exp.get());
    emit(Instruction::MakeAssign(v, var));
    return;
  }
  for (size_t i = 0; i < init->arrayExps.size(); ++i) {
    auto &ce = init->arrayExps[i];
    if (!ce)
      continue;
    Operand v = genConstExp(ce.get());
    emit(Instruction::MakeStore(v, var,
                                Operand::ConstantInt(static_cast<int>(i))));
  }
}

void CodeGen::genInitVal(InitVal *init, const std::shared_ptr<Symbol> &sym) {
  if (!init || !sym)
    return;
  Operand var = Operand::Variable(sym);
  if (!init->isArray) {
    if (init->exp) {
      Operand v = genExp(init->exp.get());
      emit(Instruction::MakeAssign(v, var));
    }
    return;
  }
  for (size_t i = 0; i < init->arrayExps.size(); ++i) {
    auto &e = init->arrayExps[i];
    if (!e)
      continue; // 空位可视为默认值
    Operand v = genExp(e.get());
    emit(Instruction::MakeStore(v, var,
                                Operand::ConstantInt(static_cast<int>(i))));
  }
}
Operand CodeGen::genExp(Exp *exp) {
  if (!exp)
    return Operand();

  return genAdd(exp->addExp.get());
}

Operand CodeGen::genConstExp(ConstExp *ce) {
  if (!ce)
    return Operand::ConstantInt(0);
  return genAdd(ce->addExp.get());
}

Operand CodeGen::genCond(Cond *cond) {
  if (!cond)
    return Operand::ConstantInt(0);
  // 以值形式求 Cond：将 LOrExp 计算成整数，再与 0 比较得到 0/1
  Operand v = genLOr(cond->lOrExp.get());
  Operand r = newTemp();
  emit(Instruction::MakeBinary(OpCode::NEQ, v, Operand::ConstantInt(0), r));
  return r;
}

void CodeGen::genCondBranch(Cond *cond, int tLbl, int fLbl) {
  if (!cond)
    return;

  // 递归处理 LAnd：若左为假，直接 f；否则继续右侧
  std::function<void(LAndExp *, int, int)> branchLAnd = [&](LAndExp *node,
                                                            int t, int f) {
    if (!node) {
      emit(Instruction::MakeGoto(Operand::Label(f)));
      return;
    }
    if (node->left) {
      Operand mid = newLabel();
      branchLAnd(node->left.get(), mid.asInt(), f);
      placeLabel(mid);
      if (node->eqExp) {
        Operand v = genEq(node->eqExp.get());
        emit(Instruction::MakeIf(v, Operand::Label(t)));
        emit(Instruction::MakeGoto(Operand::Label(f)));
      } else {
        emit(Instruction::MakeGoto(Operand::Label(f)));
      }
    } else {
      if (node->eqExp) {
        Operand v = genEq(node->eqExp.get());
        emit(Instruction::MakeIf(v, Operand::Label(t)));
        emit(Instruction::MakeGoto(Operand::Label(f)));
      } else {
        emit(Instruction::MakeGoto(Operand::Label(f)));
      }
    }
  };

  // 递归处理 LOr：若左为真，直接 t；否则继续右侧
  std::function<void(LOrExp *, int, int)> branchLOr = [&](LOrExp *node, int t,
                                                          int f) {
    if (!node) {
      emit(Instruction::MakeGoto(Operand::Label(f)));
      return;
    }
    if (node->left) {
      Operand mid = newLabel();
      branchLOr(node->left.get(), t, mid.asInt());
      placeLabel(mid);
      branchLAnd(node->lAndExp.get(), t, f);
    } else {
      branchLAnd(node->lAndExp.get(), t, f);
    }
  };

  // 顶层从 LOrExp 开始
  branchLOr(cond->lOrExp.get(), tLbl, fLbl);
}

Operand CodeGen::genLVal(LVal *lval, Operand *addrOut) {
  if (!lval)
    return Operand();

  // 变量/数组元素
  auto sym = internSymbol(lval->ident);
  Operand base = Operand::Variable(sym);
  if (!lval->arrayIndex) {
    if (addrOut)
      *addrOut = Operand();
    return base; // 标量
  }
  // 数组：返回加载到的值，并把 addrOut 设为下标供 STORE 使用
  Operand idx = genExp(lval->arrayIndex.get());
  if (addrOut)
    *addrOut = idx;
  Operand dst = newTemp();
  emit(Instruction::MakeLoad(base, idx, dst));
  return dst;
}
Operand CodeGen::genPrimary(PrimaryExp *pe) {
  if (!pe)
    return Operand();

  switch (pe->primaryType) {
  case PrimaryExp::PrimaryType::EXP:
    return genExp(pe->exp.get());
  case PrimaryExp::PrimaryType::LVAL:
    return genLVal(pe->lval.get());
  case PrimaryExp::PrimaryType::NUMBER:
    return genNumber(pe->number.get());
  }
  return Operand();
}

Operand CodeGen::genNumber(Number *num) {
  if (!num)
    return Operand();
  return Operand::ConstantInt(num->value);
}

Operand CodeGen::genUnary(UnaryExp *ue) {
  if (!ue)
    return Operand();

  switch (ue->unaryType) {
  case UnaryExp::UnaryType::PRIMARY:
    return genPrimary(ue->primaryExp.get());
  case UnaryExp::UnaryType::FUNC_CALL: {
    Operand func = Operand::Variable(internSymbol(ue->funcIdent));
    std::vector<Operand> args;
    if (ue->funcRParams) {
      args = genFuncRParams(ue->funcRParams.get());
    }

    // Emit parameter instructions first
    for (size_t i = 0; i < args.size(); i++) {
      emit(Instruction(OpCode::PARAM, args[i], Operand()));
    }

    Operand result = newTemp();
    emit(Instruction::MakeCall(func, args.size(), result));

    return result;
  }
  case UnaryExp::UnaryType::UNARY_OP: {
    Operand operand = genUnary(ue->unaryExp.get());
    Operand result = newTemp();

    switch (ue->unaryOp->op) {
    case UnaryOp::OpType::PLUS:
      // Unary plus, no operation needed
      return operand;
    case UnaryOp::OpType::MINUS:
      emit(Instruction::MakeUnary(OpCode::NEG, operand, result));
      return result;
    case UnaryOp::OpType::NOT:
      emit(Instruction::MakeUnary(OpCode::NOT, operand, result));
      return result;
    }
    break;
  }
  }
  return Operand();
}

Operand CodeGen::genMul(MulExp *me) {
  if (!me)
    return Operand();

  if (me->op == MulExp::OpType::NONE) {
    return genUnary(me->unaryExp.get());
  }

  Operand left = genMul(me->left.get());
  Operand right = genUnary(me->unaryExp.get());
  Operand result = newTemp();

  OpCode op;
  switch (me->op) {
  case MulExp::OpType::MULT:
    op = OpCode::MUL;
    break;
  case MulExp::OpType::DIV:
    op = OpCode::DIV;
    break;
  case MulExp::OpType::MOD:
    op = OpCode::MOD;
    break;
  default:
    return Operand();
  }

  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genAdd(AddExp *ae) {
  if (!ae)
    return Operand();

  if (ae->op == AddExp::OpType::NONE) {
    return genMul(ae->mulExp.get());
  }

  Operand left = genAdd(ae->left.get());
  Operand right = genMul(ae->mulExp.get());
  Operand result = newTemp();

  OpCode op = (ae->op == AddExp::OpType::PLUS) ? OpCode::ADD : OpCode::SUB;
  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genRel(RelExp *re) {
  if (!re)
    return Operand();

  if (re->op == RelExp::OpType::NONE) {
    return genAdd(re->addExp.get());
  }

  Operand left = genRel(re->left.get());
  Operand right = genAdd(re->addExp.get());
  Operand result = newTemp();

  OpCode op;
  switch (re->op) {
  case RelExp::OpType::LSS:
    op = OpCode::LT;
    break;
  case RelExp::OpType::GRE:
    op = OpCode::GT;
    break;
  case RelExp::OpType::LEQ:
    op = OpCode::LE;
    break;
  case RelExp::OpType::GEQ:
    op = OpCode::GE;
    break;
  default:
    return Operand();
  }

  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genEq(EqExp *ee) {
  if (!ee)
    return Operand();

  if (ee->op == EqExp::OpType::NONE) {
    return genRel(ee->relExp.get());
  }

  Operand left = genEq(ee->left.get());
  Operand right = genRel(ee->relExp.get());
  Operand result = newTemp();

  OpCode op = (ee->op == EqExp::OpType::EQL) ? OpCode::EQ : OpCode::NEQ;
  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genLAnd(LAndExp *la) {
  if (!la)
    return Operand();

  if (!la->left) {
    return genEq(la->eqExp.get());
  }

  Operand left = genLAnd(la->left.get());
  Operand right = genEq(la->eqExp.get());
  Operand result = newTemp();

  emit(Instruction::MakeBinary(OpCode::AND, left, right, result));
  return result;
}

Operand CodeGen::genLOr(LOrExp *lo) {
  if (!lo)
    return Operand();

  if (!lo->left) {
    return genLAnd(lo->lAndExp.get());
  }

  Operand left = genLOr(lo->left.get());
  Operand right = genLAnd(lo->lAndExp.get());
  Operand result = newTemp();

  emit(Instruction::MakeBinary(OpCode::OR, left, right, result));
  return result;
}

std::vector<Operand> CodeGen::genFuncRParams(FuncRParams *params) {
  std::vector<Operand> args;

  if (!params)
    return args;

  for (auto &exp : params->exps) {
    if (exp) {
      args.push_back(genExp(exp.get()));
    }
  }

  return args;
}

std::shared_ptr<Symbol> CodeGen::internSymbol(const std::string &name,
                                              TypePtr type) {
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    return it->second;
  }

  auto sym = std::make_shared<Symbol>(name, type, 0);
  symbols_[name] = sym;
  return sym;
}
