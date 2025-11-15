// CodeGenCustom.cpp - build minimal custom IR Function
#include "codegen/CodeGen.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"

using namespace lcc::codegen;

Function CodeGen::generate(CompUnit *root) {
  // TODO: traverse AST. For now create main with single return 0.
  Function fn("main");
  auto blk = fn.createBlock();
  blk->addInstruction(Instruction::MakeReturn(Operand::ConstantInt(0)));
  return fn;
}
Function CodeGen::genFunction(FuncDef *funcDef) {}
Function CodeGen::genMain(MainFuncDef *mainDef) {}
void CodeGen::genBlock(Block *block) {}
void CodeGen::genBlockItem(BlockItem *item) {}
void CodeGen::genStmt(Stmt *stmt) {}
void CodeGen::genAssign(AssignStmt *stmt) {}
void CodeGen::genExpStmt(ExpStmt *stmt) {}
void CodeGen::genIf(IfStmt *stmt) {}
void CodeGen::genFor(ForStmt *stmt) {}
void CodeGen::genBreak(BreakStmt *stmt) {}
void CodeGen::genContinue(ContinueStmt *stmt) {}
void CodeGen::genReturn(ReturnStmt *stmt) {}
void CodeGen::genPrintf(PrintfStmt *stmt) {}
void CodeGen::genForAssign(ForAssignStmt *stmt) {}
void CodeGen::genDecl(Decl *decl) {}
void CodeGen::genConstDecl(ConstDecl *decl) {}
void CodeGen::genVarDecl(VarDecl *decl) {}
void CodeGen::genConstDef(ConstDef *def) {}
void CodeGen::genVarDef(VarDef *def) {}
void CodeGen::genConstInitVal(ConstInitVal *init,
                              const std::shared_ptr<Symbol> &sym) {}
void CodeGen::genInitVal(InitVal *init, const std::shared_ptr<Symbol> &sym) {}
Operand CodeGen::genExp(Exp *exp) {}
Operand CodeGen::genCond(Cond *cond) {}
void genCondBranch(Cond *cond, int tLbl, int fLbl) {}
Operand CodeGen::genLVal(LVal *lval, Operand *addrOut) {}
Operand CodeGen::genPrimary(PrimaryExp *pe) {}
Operand CodeGen::genNumber(Number *num) {}
Operand CodeGen::genUnary(UnaryExp *ue) {}
Operand CodeGen::genMul(MulExp *me) {}
Operand CodeGen::genAdd(AddExp *ae) {}
Operand CodeGen::genRel(RelExp *re) {}
Operand CodeGen::genEq(EqExp *ee) {}
Operand CodeGen::genLAnd(LAndExp *la) {}
Operand CodeGen::genLOr(LOrExp *lo) {}
std::vector<Operand> CodeGen::genFuncRParams(FuncRParams *params) {}
