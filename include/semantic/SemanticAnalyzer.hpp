#pragma once
#include "parser/AST.hpp"
#include "semantic/Symbol.hpp"
#include "semantic/SymbolTable.hpp"
#include "semantic/Type.hpp"
#include <iostream>
#include <optional>
#include <vector>

class SemanticAnalyzer {
public:
  SemanticAnalyzer(std::ostream &errorStream) : errorStream(errorStream) {}

  void visit(CompUnit *node);

private:
  SymbolTable symbolTable;
  std::ostream &errorStream;
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
  std::vector<Type::FuncParam> visit(FuncFParams *node);
  Type::FuncParam visit(FuncFParam *node); // 帮助上层节点确定参数类型
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

  TypePtr visit(BType *node);    // 帮助上层节点确定Type
  TypePtr visit(FuncType *node); // 帮助上层节点确定functype
  void visit(ConstInitVal *node);
  void visit(InitVal *node);
  void visit(Exp *node);
  void visit(Cond *node);
  std::optional<Symbol> visit(LVal *node); // 帮助上层节点找到符号表中的符号
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
