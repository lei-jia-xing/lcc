#pragma once

#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "parser/AST.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace lcc::codegen {

class CodeGen {
public:
  CodeGen() = default;
  ~CodeGen() = default;

  void generate(CompUnit *root);

  void reset();

private:
  Function genFunction(FuncDef *funcDef);
  Function genMainFuncDef(MainFuncDef *mainDef);

  void genBlock(Block *block);
  void genBlockItem(BlockItem *item);
  void genStmt(Stmt *stmt);
  void genAssign(AssignStmt *stmt);
  void genExpStmt(ExpStmt *stmt);
  void genIf(IfStmt *stmt);
  void genFor(ForStmt *stmt);
  void genBreak(BreakStmt *stmt);
  void genContinue(ContinueStmt *stmt);
  void genReturn(ReturnStmt *stmt);
  void genPrintf(PrintfStmt *stmt);
  void genForAssign(ForAssignStmt *stmt);

  void genDecl(Decl *decl);
  void genConstDecl(ConstDecl *decl);
  void genVarDecl(VarDecl *decl);
  void genConstDef(ConstDef *def);
  void genVarDef(VarDef *def);
  void genConstInitVal(ConstInitVal *init, const std::shared_ptr<Symbol> &sym);
  void genInitVal(InitVal *init, const std::shared_ptr<Symbol> &sym);
  Operand genConstExp(ConstExp *ce);

  Operand genExp(Exp *exp);
  Operand genCond(Cond *cond);
  void genCondBranch(Cond *cond, int tLbl, int fLbl);
  Operand genLVal(LVal *lval, Operand *addrOut = nullptr);
  Operand genPrimary(PrimaryExp *pe);
  Operand genNumber(Number *num);
  Operand genUnary(UnaryExp *ue);
  Operand genMul(MulExp *me);
  Operand genAdd(AddExp *ae);
  Operand genRel(RelExp *re);
  Operand genEq(EqExp *ee);
  Operand genLAnd(LAndExp *la);
  Operand genLOr(LOrExp *lo);
  std::vector<Operand> genFuncRParams(FuncRParams *params);

  void emit(const Instruction &inst);
  Operand newTemp();
  Operand newLabel();
  void placeLabel(const Operand &label);

  std::shared_ptr<Symbol> internStringLiteral(const std::string &literal);

  std::shared_ptr<Symbol> internSymbol(const std::string &name,
                                       TypePtr type = nullptr);

private:
  struct Context {
    Function *func = nullptr;
    std::shared_ptr<BasicBlock> curBlk;
    int nextTempId = 0;
    int nextLabelId = 0;
  } ctx_;

  std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols_;
  std::unordered_map<std::string, std::shared_ptr<Symbol>> stringLiterals_;
  int nextStringId_ = 0;
  struct LoopContext {
    int breakLabel;
    int continueLabel;
  };
  std::vector<LoopContext> loopStack_;
  void pushLoop(int breakLbl, int continueLbl) {
    loopStack_.push_back({breakLbl, continueLbl});
  }
  void popLoop() {
    if (!loopStack_.empty())
      loopStack_.pop_back();
  }
  LoopContext *currentLoop() {
    return loopStack_.empty() ? nullptr : &loopStack_.back();
  }
};

} // namespace lcc::codegen
