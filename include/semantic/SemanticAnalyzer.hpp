#pragma once
#include "parser/AST.hpp"
#include "semantic/SymbolTable.hpp"
#include "semantic/Type.hpp"
#include <iostream>

class SemanticAnalyzer {
public:
  SemanticAnalyzer(std::ostream &errorStream) : errorStream(errorStream) {}

  void visit(CompUnit *node);

private:
  SymbolTable symbolTable;
  std::ostream &errorStream;

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
  TypePtr visit(ConstInitVal *node);
  TypePtr visit(InitVal *node);
  TypePtr visit(FuncType *node);
  TypePtr visit(Exp *node);
  TypePtr visit(Cond *node);
  TypePtr visit(LVal *node);
  TypePtr visit(PrimaryExp *node);
  TypePtr visit(Number *node);
  TypePtr visit(UnaryExp *node);
  TypePtr visit(UnaryOp *node);
  TypePtr visit(FuncRParams *node);
  TypePtr visit(MulExp *node);
  TypePtr visit(AddExp *node);
  TypePtr visit(RelExp *node);
  TypePtr visit(EqExp *node);
  TypePtr visit(LAndExp *node);
  TypePtr visit(LOrExp *node);
  TypePtr visit(ConstExp *node);
};
