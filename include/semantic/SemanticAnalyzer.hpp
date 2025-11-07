#pragma once
#include "parser/AST.hpp"
#include "semantic/SymbolTable.hpp"
#include "semantic/Type.hpp"

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void visit(CompUnit *node);

private:
  SymbolTable symbolTable;
  int loop = 0;
  TypePtr current_function_return_type = nullptr;

  void error(const int &line, const std::string errorType);

  void visit(Decl *node);
  void visit(ConstDecl *node);
  void visit(VarDecl *node);
  void visit(ConstDef *node, TypePtr type);
  void visit(VarDef *node, TypePtr type);
  void visit(FuncDef *node);
  void visit(MainFuncDef *node);
  void visit(FuncFParams *node);
  void visit(FuncFParam *node);
  void visit(Block *node);
  void visit(BlockItem *node);

  void visit(Stmt *node);
  void visit(AssignStmt *node);
  void visit(ExpStmt *node);
  void visit(BlockStmt *node);
  void visit(IfStmt *node);
  void visit(ForStmt *node);
  void visit(BreakStmt *node);
  void visit(ContinueStmt *node);
  void visit(ReturnStmt *node);
  void visit(PrintfStmt *node);
  void visit(ForAssignStmt *node);

  TypePtr visit(BType *node);
  TypePtr visit(FuncType *node);
  void visit(ConstInitVal *node);
  void visit(InitVal *node);
  TypePtr visit(Exp *node);
  TypePtr visit(Cond *node);
  TypePtr visit(LVal *node);
  TypePtr visit(PrimaryExp *node);
  TypePtr visit(Number *node);
  TypePtr visit(UnaryExp *node);
  void visit(UnaryOp *node);
  std::vector<TypePtr> visit(FuncRParams *node);
  TypePtr visit(MulExp *node);
  TypePtr visit(AddExp *node);
  TypePtr visit(RelExp *node);
  TypePtr visit(EqExp *node);
  TypePtr visit(LAndExp *node);
  TypePtr visit(LOrExp *node);
  TypePtr visit(ConstExp *node);
};
