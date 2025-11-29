#include "codegen/CodeGen.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "parser/AST.hpp"
#include "semantic/Symbol.hpp"
#include <iostream>

CodeGen::CodeGen(const SymbolTable &symbolTable) : symbolTable_(&symbolTable) {}

void CodeGen::reset() {
  ctx_.func = nullptr;
  ctx_.curBlk.reset();
  ctx_.nextTempId = 0;
  ctx_.nextLabelId = 0;
  constValues_.clear();
  stringLiterals_.clear();
  nextStringId_ = 0;
  nextStaticId_ = 0;
  globalsIR_.clear();
  functions_.clear();
}

Operand CodeGen::newTemp() { return Operand::Temporary(ctx_.nextTempId++); }

Operand CodeGen::newLabel() { return Operand::Label(ctx_.nextLabelId++); }

void CodeGen::placeLabel(const Operand &label) {
  emit(Instruction::MakeLabel(label));
}

void CodeGen::output(const std::string &line) {
  if (!outputEnabled_)
    return;
  std::cout << line << '\n';
}

std::shared_ptr<Symbol>
CodeGen::internStringLiteral(const std::string &literal) {
  auto it = stringLiterals_.find(literal);
  if (it != stringLiterals_.end())
    return it->second;
  std::string name = ".fmt" + std::to_string(nextStringId_++);
  auto sym = std::make_shared<Symbol>(name, nullptr, 0);
  stringLiterals_[literal] = sym;
  return sym;
}

bool CodeGen::tryEvalConst(Number *num, int &outVal) {
  if (!num)
    return false;
  outVal = num->value;
  return true;
}

bool CodeGen::tryEvalExp(Exp *exp, int &outVal) {
  if (!exp)
    return false;
  return tryEvalConst(exp->addExp.get(), outVal);
}
bool CodeGen::tryEvalConst(ConstExp *ce, int &outVal) {
  if (!ce)
    return false;
  return tryEvalConst(ce->addExp.get(), outVal);
}
bool CodeGen::tryEvalConst(PrimaryExp *pe, int &outVal) {
  if (!pe)
    return false;
  switch (pe->primaryType) {
  case PrimaryExp::PrimaryType::NUMBER:
    return tryEvalConst(pe->number.get(), outVal);
  case PrimaryExp::PrimaryType::EXP:
    return tryEvalExp(pe->exp.get(), outVal);
  case PrimaryExp::PrimaryType::LVAL:
    // Try to evaluate constant variable
    if (pe->lval) {
      std::shared_ptr<Symbol> sym =
          pe->lval->symbol; // Use symbol from AST node
      if (sym && sym->type && sym->type->is_const) {
        auto it = constValues_.find(sym);
        if (it != constValues_.end()) {
          outVal = it->second;
          return true;
        }
      }
    }
    return false;
  }
  return false;
}

bool CodeGen::tryEvalConst(UnaryExp *ue, int &outVal) {
  if (!ue)
    return false;
  switch (ue->unaryType) {
  case UnaryExp::UnaryType::PRIMARY:
    return tryEvalConst(ue->primaryExp.get(), outVal);
  case UnaryExp::UnaryType::FUNC_CALL:
    return false;
  case UnaryExp::UnaryType::UNARY_OP: {
    int v;
    if (!tryEvalConst(ue->unaryExp.get(), v))
      return false;
    if (!ue->unaryOp)
      return false;
    switch (ue->unaryOp->op) {
    case UnaryOp::OpType::PLUS:
      outVal = v;
      return true;
    case UnaryOp::OpType::MINUS:
      outVal = -v;
      return true;
    case UnaryOp::OpType::NOT:
      outVal = (v == 0 ? 1 : 0);
      return true;
    }
    return false;
  }
  }
  return false;
}

bool CodeGen::tryEvalConst(MulExp *me, int &outVal) {
  if (!me)
    return false;
  if (me->op == MulExp::OpType::NONE) {
    return tryEvalConst(me->unaryExp.get(), outVal);
  }
  int lv, rv;
  if (!tryEvalConst(me->left.get(), lv))
    return false;
  if (!tryEvalConst(me->unaryExp.get(), rv))
    return false;
  switch (me->op) {
  case MulExp::OpType::MULT:
    outVal = lv * rv;
    return true;
  case MulExp::OpType::DIV:
    if (rv == 0)
      return false;
    outVal = lv / rv;
    return true;
  case MulExp::OpType::MOD:
    if (rv == 0)
      return false;
    outVal = lv % rv;
    return true;
  default:
    return false;
  }
}

bool CodeGen::tryEvalConst(AddExp *ae, int &outVal) {
  if (!ae)
    return false;
  if (ae->op == AddExp::OpType::NONE) {
    return tryEvalConst(ae->mulExp.get(), outVal);
  }
  int lv, rv;
  if (!tryEvalConst(ae->left.get(), lv))
    return false;
  if (!tryEvalConst(ae->mulExp.get(), rv))
    return false;
  switch (ae->op) {
  case AddExp::OpType::PLUS:
    outVal = lv + rv;
    return true;
  case AddExp::OpType::MINU:
    outVal = lv - rv;
    return true;
  default:
    return false;
  }
}

bool CodeGen::tryEvalConst(RelExp *re, int &outVal) {
  if (!re)
    return false;
  if (re->op == RelExp::OpType::NONE)
    return tryEvalConst(re->addExp.get(), outVal);
  int lv, rv;
  if (!tryEvalConst(re->left.get(), lv))
    return false;
  if (!tryEvalConst(re->addExp.get(), rv))
    return false;
  switch (re->op) {
  case RelExp::OpType::LSS:
    outVal = (lv < rv) ? 1 : 0;
    return true;
  case RelExp::OpType::GRE:
    outVal = (lv > rv) ? 1 : 0;
    return true;
  case RelExp::OpType::LEQ:
    outVal = (lv <= rv) ? 1 : 0;
    return true;
  case RelExp::OpType::GEQ:
    outVal = (lv >= rv) ? 1 : 0;
    return true;
  default:
    return false;
  }
}

bool CodeGen::tryEvalConst(EqExp *ee, int &outVal) {
  if (!ee)
    return false;
  if (!ee->left)
    return tryEvalConst(ee->relExp.get(), outVal);
  int lv, rv;
  if (!tryEvalConst(ee->left.get(), lv))
    return false;
  if (!tryEvalConst(ee->relExp.get(), rv))
    return false;
  switch (ee->op) {
  case EqExp::OpType::EQL:
    outVal = (lv == rv) ? 1 : 0;
    return true;
  case EqExp::OpType::NEQ:
    outVal = (lv != rv) ? 1 : 0;
    return true;
  default:
    return false;
  }
}

bool CodeGen::tryEvalConst(LAndExp *la, int &outVal) {
  if (!la)
    return false;
  if (!la->left)
    return tryEvalConst(la->eqExp.get(), outVal);
  int lv, rv;
  if (!tryEvalConst(la->left.get(), lv))
    return false;
  if (!tryEvalConst(la->eqExp.get(), rv))
    return false;
  outVal = ((lv != 0) && (rv != 0)) ? 1 : 0;
  return true;
}

bool CodeGen::tryEvalConst(LOrExp *lo, int &outVal) {
  if (!lo)
    return false;
  if (!lo->left)
    return tryEvalConst(lo->lAndExp.get(), outVal);
  int lv, rv;
  if (!tryEvalConst(lo->left.get(), lv))
    return false;
  if (!tryEvalConst(lo->lAndExp.get(), rv))
    return false;
  outVal = ((lv != 0) || (rv != 0)) ? 1 : 0;
  return true;
}

void CodeGen::generate(CompUnit *root) {
  reset();
  if (!root)
    return;
  for (auto &Decl : root->decls) {
    genDecl(Decl.get());
  }
  for (auto &FuncDef : root->funcDefs) {
    genFunction(FuncDef.get());
  }
  if (root->mainFuncDef) {
    genMainFuncDef(root->mainFuncDef.get());
  }
  for (auto &inst : globalsIR_) {
    output(inst.toString());
  }
}

void CodeGen::emit(const Instruction &inst) {
  if (ctx_.curBlk) {
    // local
    ctx_.curBlk->addInstruction(inst);
  } else {
    // global
    globalsIR_.push_back(inst);
  }
}

void CodeGen::emitGlobal(const Instruction &inst) {
  globalsIR_.push_back(inst);
}

void CodeGen::genFunction(FuncDef *funcDef) {
  if (!funcDef)
    return;

  auto funcPtr = std::make_shared<Function>("fn_" + funcDef->ident);
  std::string globalName = "fn_" + funcDef->ident;
  funcDef->symbol->globalName = globalName;
  // save current context
  auto savedFunc = ctx_.func;
  auto savedBlk = ctx_.curBlk;
  auto savedTempId = ctx_.nextTempId;
  auto savedLabelId = ctx_.nextLabelId;

  ctx_.func = funcPtr.get();
  ctx_.nextTempId = 0;
  ctx_.nextLabelId = 0;

  ctx_.curBlk = funcPtr->createBlock();

  if (funcDef->params) {
    int idx = 0;
    for (auto &p : funcDef->params->params) {
      auto sym = p->symbol; // Use symbol from AST node
      if (sym) {
        emit(Instruction::MakeParam(Operand::ConstantInt(idx),
                                    Operand::Variable(sym)));
      }
      idx++;
    }
  }

  if (funcDef->block) {
    genBlock(funcDef->block.get());
  }

  for (auto &blk : funcPtr->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      output(inst.toString());
    }
  }

  functions_.push_back(funcPtr);
  ctx_.func = savedFunc;
  ctx_.curBlk = savedBlk;
  ctx_.nextTempId = savedTempId;
  ctx_.nextLabelId = savedLabelId;
}

void CodeGen::genMainFuncDef(MainFuncDef *mainDef) {
  auto funcPtr = std::make_shared<Function>("main");

  auto savedFunc = ctx_.func;
  auto savedBlk = ctx_.curBlk;
  auto savedTempId = ctx_.nextTempId;
  auto savedLabelId = ctx_.nextLabelId;

  ctx_.func = funcPtr.get();
  ctx_.nextTempId = 0;
  ctx_.nextLabelId = 0;

  ctx_.curBlk = funcPtr->createBlock();

  if (mainDef->block) {
    genBlock(mainDef->block.get());
  }

  for (auto &blk : funcPtr->getBlocks()) {
    for (auto &inst : blk->getInstructions()) {
      output(inst.toString());
    }
  }

  functions_.push_back(funcPtr);
  ctx_.func = savedFunc;
  ctx_.curBlk = savedBlk;
  ctx_.nextTempId = savedTempId;
  ctx_.nextLabelId = savedLabelId;
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
  Operand idx;
  Operand baseOrValue = genLVal(stmt->lval.get(), &idx);
  Operand rhs = genExp(stmt->exp.get());
  if (idx.getType() != OperandType::Empty) {
    auto sym = stmt->lval->symbol; // Use symbol from AST node
    emit(Instruction::MakeStore(rhs, Operand::Variable(sym), idx));
  } else {
    emit(Instruction::MakeAssign(rhs, baseOrValue));
  }
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

  genCond(stmt->cond.get(), trueLabel.asInt(), falseLabel.asInt());

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
    genCond(stmt->cond.get(), L_body.asInt(), L_end.asInt());
  } else {
    // default is true
    emit(Instruction::MakeGoto(L_body));
  }

  placeLabel(L_end);
  popLoop();
}

void CodeGen::genBreak(BreakStmt *stmt) {
  if (!stmt)
    return;
  auto *loop = currentLoop();
  emit(Instruction::MakeGoto(Operand::Label(loop->breakLabel)));
}

void CodeGen::genContinue(ContinueStmt *stmt) {
  if (!stmt)
    return;
  auto *loop = currentLoop();
  emit(Instruction::MakeGoto(Operand::Label(loop->continueLabel)));
}

void CodeGen::genReturn(ReturnStmt *stmt) {
  if (!stmt)
    return;

  Operand result;
  if (stmt->exp) {
    result = genExp(stmt->exp.get());
  }

  emit(Instruction::MakeReturn(result));
}

void CodeGen::genPrintf(PrintfStmt *stmt) {
  if (!stmt)
    return;

  auto fmtSym = internStringLiteral(stmt->formatString);
  std::vector<Operand> vals;
  vals.reserve(stmt->args.size());
  for (auto &e : stmt->args) {
    vals.push_back(genExp(e.get()));
  }
  emit(Instruction::MakeArg(Operand::Variable(fmtSym)));
  int argc = 1;
  for (auto &v : vals) {
    emit(Instruction::MakeArg(v));
    argc++;
  }
  auto printfSym = symbolTable_->findSymbol("printf");
  Operand fnSym = Operand::Variable(printfSym);
  Operand ret = newTemp();
  emit(Instruction::MakeCall(fnSym, argc, ret));
}

void CodeGen::genForAssign(ForAssignStmt *stmt) {
  if (!stmt)
    return;
  for (auto &as : stmt->assignments) {
    Operand rhs = genExp(as.exp.get());
    Operand idx;
    Operand lhs = genLVal(as.lval.get(), &idx);
    if (idx.getType() == OperandType::Empty) {
      emit(Instruction::MakeAssign(rhs, lhs));
    } else {
      // arrary
      emit(Instruction::MakeStore(rhs, lhs, idx));
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
    genConstDef(constDef.get());
  }
}

void CodeGen::genVarDecl(VarDecl *decl) {
  if (!decl)
    return;
  bool isStatic = decl->isStatic;
  for (auto &varDef : decl->varDefs) {
    genVarDef(varDef.get(), isStatic);
  }
}

void CodeGen::genConstDef(ConstDef *def) {
  if (!def)
    return;
  auto sym = def->symbol; // Use symbol from AST node
  if (!sym) {
    return;
  }

  // size alloca, default to 1 word
  Operand sizeOp = Operand::ConstantInt(1);
  if (def->arraySize) {
    sizeOp = genConstExp(def->arraySize.get());
  }
  emit(Instruction::MakeAlloca(Operand::Variable(sym), sizeOp));
  if (def->constinitVal) {
    genConstInitVal(def->constinitVal.get(), sym);
  }
}

void CodeGen::genVarDef(VarDef *def, bool isStaticCtx) {
  if (!def)
    return;
  auto sym = def->symbol; // Use symbol from AST node
  if (!sym) {
    return;
  }

  int sizeInt = 1;
  if (def->arraySize) {
    int val = 0;
    if (tryEvalConst(def->arraySize.get(), val)) {
      sizeInt = val;
    }
  }

  if (isStaticCtx && ctx_.curBlk) {
    // for local static variable, allocate a special name
    std::string gname = "_S_" + ctx_.func->getName() + "_" +
                        std::to_string(nextStaticId_++) + "_" + def->ident;
    sym->globalName = gname;
    if (!definedGlobals_.count(gname)) {
      definedGlobals_.insert(gname);
      // static variable store in .data
      emitGlobal(Instruction::MakeAlloca(Operand::Variable(sym),
                                         Operand::ConstantInt(sizeInt)));
      if (def->initVal) {
        if (!def->initVal->isArray) {
          if (def->initVal->exp) {
            int val = 0;
            if (tryEvalExp(def->initVal->exp.get(), val)) {
              emitGlobal(Instruction::MakeAssign(Operand::ConstantInt(val),
                                                 Operand::Variable(sym)));
            }
          }
        } else {
          for (size_t i = 0; i < def->initVal->arrayExps.size(); ++i) {
            int val = 0;
            if (tryEvalExp(def->initVal->arrayExps[i].get(), val)) {
              emitGlobal(Instruction::MakeStore(Operand::ConstantInt(val),
                                                Operand::Variable(sym),
                                                Operand::ConstantInt(i)));
            }
          }
        }
      }
    }
    return;
  }

  emit(Instruction::MakeAlloca(Operand::Variable(sym),
                               Operand::ConstantInt(sizeInt)));
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
    if (v.getType() == OperandType::ConstantInt) {
      constValues_[sym] = v.asInt();
    }
    emit(Instruction::MakeAssign(v, var));
    return;
  }
  // array
  for (size_t i = 0; i < init->arrayExps.size(); ++i) {
    auto &ce = init->arrayExps[i];
    Operand v = genConstExp(ce.get());
    emit(Instruction::MakeStore(v, var, Operand::ConstantInt(i)));
  }
}

void CodeGen::genInitVal(InitVal *init, const std::shared_ptr<Symbol> &sym) {
  if (!init || !sym)
    return;
  Operand var = Operand::Variable(sym);
  if (!init->isArray) {
    if (init->exp) {
      int constValue = 0;
      Operand v = genExp(init->exp.get());
      emit(Instruction::MakeAssign(v, var));
    }
    return;
  }
  for (size_t i = 0; i < init->arrayExps.size(); ++i) {
    auto &e = init->arrayExps[i];
    Operand v = genExp(e.get());
    emit(Instruction::MakeStore(v, var, Operand::ConstantInt(i)));
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

void CodeGen::genCond(Cond *cond, int tLbl, int fLbl) {
  if (!cond)
    return;

  branchLOrForCond(cond->lOrExp.get(), tLbl, fLbl);
}

Operand CodeGen::genLVal(LVal *lval, Operand *index) {
  if (!lval)
    return Operand();
  auto sym = lval->symbol; // Use symbol from AST node
  if (!sym) {
    return Operand();
  }

  Operand base = Operand::Variable(sym);
  if (!lval->arrayIndex) {
    if (lval->type && lval->type->is_const) {
      // is const rvalue, because const should not be lvalue
      auto it = constValues_.find(sym);
      if (it != constValues_.end()) {
        if (index)
          *index = Operand();
        return Operand::ConstantInt(it->second);
      }
    }
    if (lval->type && lval->type->category == Type::Category::Array) {
      if (!index) { // is rvalue, as caller param
        Operand addr = newTemp();
        // pass address
        emit(Instruction::MakeAssign(base, addr));
        return addr;
      }
      // otherwise is lvalue, return base
    }
    // variable
    if (index)
      *index = Operand();
    return base;
  }
  // arrary element
  Operand idx = genExp(lval->arrayIndex.get());
  if (index) {
    *index = idx;
    return base;
  }
  Operand dst = newTemp(); // pass by value
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
    auto funcSym = symbolTable_->findSymbol(ue->funcIdent);
    Operand func = Operand::Variable(funcSym);
    std::vector<Operand> args;
    if (ue->funcRParams) {
      args = genFuncRParams(ue->funcRParams.get());
    }

    for (size_t i = 0; i < args.size(); i++) {
      emit(Instruction::MakeArg(args[i]));
    }

    Operand result = newTemp();
    emit(Instruction::MakeCall(func, args.size(), result));

    return result;
  }
  case UnaryExp::UnaryType::UNARY_OP: {

    int cv;
    if (tryEvalConst(ue, cv)) {
      return Operand::ConstantInt(cv);
    }

    Operand operand = genUnary(ue->unaryExp.get());
    if (!ue->unaryOp)
      return operand;
    switch (ue->unaryOp->op) {
    case UnaryOp::OpType::PLUS:
      return operand;
    case UnaryOp::OpType::MINUS: {
      Operand result = newTemp();
      emit(Instruction::MakeUnary(OpCode::NEG, operand, result));
      return result;
    }
    case UnaryOp::OpType::NOT: {
      Operand result = newTemp();
      emit(Instruction::MakeUnary(OpCode::NOT, operand, result));
      return result;
    }
    }
    break;
  }
  }
  return Operand();
}

Operand CodeGen::genMul(MulExp *me) {
  if (!me)
    return Operand();
  int cv;
  if (tryEvalConst(me, cv))
    return Operand::ConstantInt(cv);
  if (me->op == MulExp::OpType::NONE) {
    return genUnary(me->unaryExp.get());
  }

  Operand left = genMul(me->left.get());
  Operand right = genUnary(me->unaryExp.get());
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
  Operand result = newTemp();
  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genAdd(AddExp *ae) {
  if (!ae)
    return Operand();
  int cv;
  if (tryEvalConst(ae, cv))
    return Operand::ConstantInt(cv);
  if (ae->op == AddExp::OpType::NONE) {
    return genMul(ae->mulExp.get());
  }

  Operand left = genAdd(ae->left.get());
  Operand right = genMul(ae->mulExp.get());
  OpCode op = (ae->op == AddExp::OpType::PLUS) ? OpCode::ADD : OpCode::SUB;
  Operand result = newTemp();
  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genRel(RelExp *re) {
  if (!re)
    return Operand();
  int cv;
  if (tryEvalConst(re, cv))
    return Operand::ConstantInt(cv);
  if (re->op == RelExp::OpType::NONE) {
    return genAdd(re->addExp.get());
  }

  Operand left = genRel(re->left.get());
  Operand right = genAdd(re->addExp.get());

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
  Operand result = newTemp();
  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genEq(EqExp *ee) {
  if (!ee)
    return Operand();
  int cv;
  if (tryEvalConst(ee, cv))
    return Operand::ConstantInt(cv);
  if (ee->op == EqExp::OpType::NONE) {
    return genRel(ee->relExp.get());
  }

  Operand left = genEq(ee->left.get());
  Operand right = genRel(ee->relExp.get());
  OpCode op = (ee->op == EqExp::OpType::EQL) ? OpCode::EQ : OpCode::NEQ;
  Operand result = newTemp();
  emit(Instruction::MakeBinary(op, left, right, result));
  return result;
}

Operand CodeGen::genLAnd(LAndExp *la) {
  if (!la)
    return Operand();
  int cv;
  if (tryEvalConst(la, cv))
    return Operand::ConstantInt(cv);
  if (la->op == LAndExp::OpType::NONE) {
    return genEq(la->eqExp.get());
  }

  Operand result = newTemp();
  emit(Instruction::MakeAssign(Operand::ConstantInt(0), result));

  Operand L_true = newLabel();
  Operand L_end = newLabel();

  branchLAndForVal(la, L_true.asInt(), L_end.asInt());

  placeLabel(L_true);
  emit(Instruction::MakeAssign(Operand::ConstantInt(1), result));
  placeLabel(L_end);
  return result;
}

Operand CodeGen::genLOr(LOrExp *lo) {
  if (!lo)
    return Operand();
  int cv;
  if (tryEvalConst(lo, cv))
    return Operand::ConstantInt(cv);
  if (lo->op == LOrExp::OpType::NONE) {
    return genLAnd(lo->lAndExp.get());
  }

  Operand result = newTemp();
  emit(Instruction::MakeAssign(Operand::ConstantInt(0), result));

  Operand L_true = newLabel();
  Operand L_end = newLabel();

  branchLOrForVal(lo, L_true.asInt(), L_end.asInt());

  placeLabel(L_true);
  emit(Instruction::MakeAssign(Operand::ConstantInt(1), result));
  placeLabel(L_end);
  return result;
}

std::vector<Operand> CodeGen::genFuncRParams(FuncRParams *params) {
  std::vector<Operand> args;

  if (!params)
    return args;

  for (auto &exp : params->exps) {
    args.push_back(genExp(exp.get()));
  }

  return args;
}

void CodeGen::branchLAndForCond(LAndExp *node, int trueLbl, int falseLbl) {
  if (!node) {
    emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    return;
  }
  if (node->left) {
    Operand mid = newLabel();
    branchLAndForCond(node->left.get(), mid.asInt(), falseLbl);
    placeLabel(mid);
    if (node->eqExp) {
      Operand v = genEq(node->eqExp.get());
      emit(Instruction::MakeIf(v, Operand::Label(trueLbl)));
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    } else {
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    }
  } else {
    if (node->eqExp) {
      Operand v = genEq(node->eqExp.get());
      emit(Instruction::MakeIf(v, Operand::Label(trueLbl)));
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    } else {
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    }
  }
}

void CodeGen::branchLOrForCond(LOrExp *node, int trueLbl, int falseLbl) {
  if (!node) {
    emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    return;
  }
  if (node->left) {
    Operand mid = newLabel();
    branchLOrForCond(node->left.get(), trueLbl, mid.asInt());
    placeLabel(mid);
    branchLAndForCond(node->lAndExp.get(), trueLbl, falseLbl);
  } else {
    branchLAndForCond(node->lAndExp.get(), trueLbl, falseLbl);
  }
}

void CodeGen::branchLAndForVal(LAndExp *node, int trueLbl, int falseLbl) {
  if (!node) {
    emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    return;
  }
  if (node->left) {
    Operand mid = newLabel();
    branchLAndForVal(node->left.get(), mid.asInt(), falseLbl);
    placeLabel(mid);
    if (node->eqExp) {
      Operand v = genEq(node->eqExp.get());
      emit(Instruction::MakeIf(v, Operand::Label(trueLbl)));
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    } else {
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    }
  } else {
    if (node->eqExp) {
      Operand v = genEq(node->eqExp.get());
      emit(Instruction::MakeIf(v, Operand::Label(trueLbl)));
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    } else {
      emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    }
  }
}

void CodeGen::branchLOrForVal(LOrExp *node, int trueLbl, int falseLbl) {
  if (!node) {
    emit(Instruction::MakeGoto(Operand::Label(falseLbl)));
    return;
  }
  if (node->left) {
    Operand mid = newLabel();
    branchLOrForVal(node->left.get(), trueLbl, mid.asInt());
    placeLabel(mid);
    branchLAndForVal(node->lAndExp.get(), trueLbl, falseLbl);
  } else {
    branchLAndForVal(node->lAndExp.get(), trueLbl, falseLbl);
  }
}
