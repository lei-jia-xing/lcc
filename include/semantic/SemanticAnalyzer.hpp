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
  bool in_loop = false;
  bool has_return = false;
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

  void visit(BType *node);
  void visit(FuncType *node);
  void visit(ConstInitVal *node);
  void visit(InitVal *node);
  void visit(Exp *node);
  void visit(Cond *node);
  void visit(LVal *node);
  void visit(PrimaryExp *node);
  void visit(Number *node);
  void visit(UnaryExp *node);
  void visit(UnaryOp *node);
  void visit(FuncRParams *node);
  void visit(MulExp *node);
  void visit(AddExp *node);
  void visit(RelExp *node);
  void visit(EqExp *node);
  void visit(LAndExp *node);
  void visit(LOrExp *node);
  void visit(ConstExp *node);

private:
  TypePtr getExpressionType(Exp *exp);

  TypePtr getLValType(const std::string &ident);
};
