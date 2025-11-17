#pragma once

#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "parser/AST.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lcc::codegen {

class CodeGen {
public:
  CodeGen() = default;
  ~CodeGen() = default;

  void generate(CompUnit *root);

  void reset();

private:
  void genFunction(FuncDef *funcDef);
  void genMainFuncDef(MainFuncDef *mainDef);

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
  void emitGlobal(const Instruction &inst);
  Operand newTemp();
  Operand newLabel();
  void placeLabel(const Operand &label);
  void output(const std::string &line);

  // Try to evaluate an Exp as a constant integer; returns true if succeed.
  bool tryEvalConst(class Exp *exp, int &outVal);
  bool tryEvalConst(class AddExp *ae, int &outVal);
  bool tryEvalConst(class MulExp *me, int &outVal);
  bool tryEvalConst(class UnaryExp *ue, int &outVal);
  bool tryEvalConst(class PrimaryExp *pe, int &outVal);
  bool tryEvalConst(class Number *num, int &outVal);

  // General constant folding helpers for value-form ops
  bool foldUnary(class Operand const &a, OpCode op, int &outVal);
  bool foldBinary(class Operand const &a, class Operand const &b, OpCode op, int &outVal);

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
  bool outputEnabled_ = true;
  bool curDeclIsStatic_ = false; // transient while lowering a VarDecl
  std::unordered_set<std::string> definedGlobals_;
  struct LoopContext {
    int breakLabel;
    int continueLabel;
  };
  std::vector<LoopContext> loopStack_;
  std::vector<Instruction> globalsIR_;
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
