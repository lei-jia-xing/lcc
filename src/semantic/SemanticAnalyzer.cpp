#include "parser/AST.hpp"
#include <optional>
#include <semantic/SemanticAnalyzer.hpp>
#include <semantic/Symbol.hpp>
#include <semantic/SymbolTable.hpp>
#include <semantic/Type.hpp>

void SemanticAnalyzer::error(const int &line, const std::string errorType) {
  errorStream << line << " " << errorType << std::endl;
}

void SemanticAnalyzer::output(const Symbol &symbol) {
  std::cout << symbol.name << " " << to_string(symbol.type) << std::endl;
}
void SemanticAnalyzer::visit(CompUnit *node) {
  if (node == nullptr)
    return;
  for (const auto &decl : node->decls) {
    visit(decl.get());
  }
  for (const auto &funcDef : node->funcDefs) {
    visit(funcDef.get());
  }
  visit(node->mainFuncDef.get());
}
void SemanticAnalyzer::visit(Decl *node) {
  if (node == nullptr)
    return;
  if (auto constDecl = dynamic_cast<ConstDecl *>(node)) {
    visit(constDecl);
  } else if (auto varDecl = dynamic_cast<VarDecl *>(node)) {
    visit(varDecl);
  }
}
void SemanticAnalyzer::visit(ConstDecl *node) {
  if (node == nullptr)
    return;
  TypePtr baseType = visit(node->bType.get());
  baseType->is_const = true;

  for (const auto &constDef : node->constDefs) {
    visit(constDef.get(), baseType);
  }
}

void SemanticAnalyzer::visit(VarDecl *node) {
  if (node == nullptr)
    return;
  TypePtr baseType = visit(node->bType.get());
  if (node->isStatic) {
    baseType->is_static = true;
  }
  for (const auto &varDef : node->varDefs) {
    visit(varDef.get(), baseType);
  }
}

TypePtr SemanticAnalyzer::visit(BType *node) {
  if (node == nullptr)
    return nullptr;
  if (node->type == "int") {
    return Type::create_base_type(BaseType::INT);
  }
  return nullptr;
}

void SemanticAnalyzer::visit(ConstDef *node, TypePtr type) {
  if (node == nullptr)
    return;
  if (node->arraySize == nullptr) {
    type->category = Type::Category::Basic;
  } else {
    type->category = Type::Category::Array;
    visit(node->arraySize.get());
  }
  visit(node->constinitVal.get());
  Symbol symbol(node->ident, type, {}, node->line);
  if (!symbolTable.addSymbol(symbol)) {
    // error
  }
}

void SemanticAnalyzer::visit(VarDef *node, TypePtr type) {
  if (node == nullptr)
    return;
  if (node->arraySize == nullptr) {
    type->category = Type::Category::Basic;
  } else {
    type->category = Type::Category::Array;
    visit(node->arraySize.get());
  }
  visit(node->initVal.get());
  Symbol symbol(node->ident, type, {}, node->line);
  if (!symbolTable.addSymbol(symbol)) {
    // error
  }
}

void SemanticAnalyzer::visit(FuncDef *node) {
  if (node == nullptr)
    return;
  TypePtr returnType = visit(node->funcType.get());
  for (auto &funcFParam : node->params->params) {
    visit(funcFParam.get());
  }
  Symbol symbol(node->ident, returnType, {}, node->line);
}
void SemanticAnalyzer::visit(MainFuncDef *node) {
  if (node == nullptr)
    return;
  visit(node->block.get());
}
void SemanticAnalyzer::visit(FuncFParams *node) {
  if (node == nullptr)
    return;
  for (auto &param : node->params) {
    visit(param.get());
  }
}
void SemanticAnalyzer::visit(FuncFParam *node) {
  if (node == nullptr)
    return;
  TypePtr type = visit(node->bType.get());
  if (node->isArray) {
    type->category = Type::Category::Array;
  } else {
    type->category = Type::Category::Basic;
  }
}
void SemanticAnalyzer::visit(Block *node) {
  if (node == nullptr)
    return;
  for (auto &item : node->items) {
    visit(item.get());
  }
}
void SemanticAnalyzer::visit(BlockItem *node) {
  if (node == nullptr)
    return;
  if (node->decl) {
    visit(node->decl.get());
  } else if (node->stmt) {
    visit(node->stmt.get());
  }
}

void SemanticAnalyzer::visit(Stmt *node) {
  if (node == nullptr)
    return;
  if (node->stmtType == Stmt::StmtType::ASSIGN) {
    visit(static_cast<AssignStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::EXP) {
    visit(static_cast<ExpStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::BLOCK) {
    visit(static_cast<BlockStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::IF) {
    visit(static_cast<IfStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::FOR) {
    visit(static_cast<ForStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::BREAK) {
    visit(static_cast<BreakStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::CONTINUE) {
    visit(static_cast<ContinueStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::RETURN) {
    visit(static_cast<ReturnStmt *>(node));
  } else if (node->stmtType == Stmt::StmtType::PRINTF) {
    visit(static_cast<PrintfStmt *>(node));
  }
}
void SemanticAnalyzer::visit(AssignStmt *node) {
  if (node == nullptr)
    return;
  TypePtr lvalType = visit(node->lval.get());
  visit(node->exp.get());
  if (symbolTable.findSymbol(node->lval->ident) == std::nullopt) {
    // error
  }
}
void SemanticAnalyzer::visit(ExpStmt *node) {
  if (node == nullptr)
    return;
  visit(node->exp.get());
}
void SemanticAnalyzer::visit(BlockStmt *node) {
  if (node == nullptr)
    return;
  visit(node->block.get());
}
void SemanticAnalyzer::visit(IfStmt *node) {
  if (node == nullptr)
    return;
  visit(node->cond.get());
  visit(node->thenStmt.get());
  visit(node->elseStmt.get());
}
void SemanticAnalyzer::visit(ForStmt *node) {
  if (node == nullptr)
    return;
  visit(node->initStmt.get());
  visit(node->cond.get());
  visit(node->updateStmt.get());
  visit(node->bodyStmt.get());
}
void SemanticAnalyzer::visit(BreakStmt *node) {
  if (node == nullptr)
    return;
  // in loop?
}
void SemanticAnalyzer::visit(ContinueStmt *node) {
  if (node == nullptr)
    return;
  // in loop?
}
void SemanticAnalyzer::visit(ReturnStmt *node) {
  if (node == nullptr)
    return;
  visit(node->exp.get());
}
void SemanticAnalyzer::visit(PrintfStmt *node) {
  if (node == nullptr)
    return;
  for (auto &exp : node->args) {
    visit(exp.get());
  }
}

void SemanticAnalyzer::visit(ForAssignStmt *node) {
  if (node == nullptr)
    return;
  for (auto &assignment : node->assignments) {
    TypePtr lvalType = visit(assignment.lval.get());
    visit(assignment.exp.get());
    if (symbolTable.findSymbol(assignment.lval->ident) == std::nullopt) {
      // error
    }
  }
}
TypePtr SemanticAnalyzer::visit(ConstInitVal *node) {
  if (node == nullptr)
    return nullptr;
  if (node->isArray) {
    for (auto &constexp : node->arrayExps) {
      visit(constexp.get());
    }
  } else {
    visit(node->exp.get());
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(InitVal *node) {
  if (node == nullptr)
    return nullptr;
  if (node->isArray) {
    for (auto &exp : node->arrayExps) {
      visit(exp.get());
    }
  } else {
    visit(node->exp.get());
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(FuncType *node) {
  if (node == nullptr)
    return nullptr;
  if (node->type == "int") {
    return Type::create_function_type(Type::create_base_type(BaseType::INT),
                                      {});
  } else if (node->type == "void") {
    return Type::create_function_type(Type::create_base_type(BaseType::VOID),
                                      {});
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(Exp *node) {
  if (node == nullptr)
    return nullptr;
  visit(node->addExp.get());
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(Cond *node) {
  if (node == nullptr)
    return nullptr;
  visit(node->lOrExp.get());
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(LVal *node) {
  if (node == nullptr)
    return nullptr;
  if (node->arrayIndex) {

  } else {
    if (symbolTable.findSymbol(node->ident) == std::nullopt) {
      // error
    }
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(PrimaryExp *node) {
  if (node == nullptr)
    return nullptr;
  if (node->primaryType == PrimaryExp::PrimaryType::EXP) {
    visit(node->exp.get());
  } else if (node->primaryType == PrimaryExp::PrimaryType::LVAL) {
    visit(node->lval.get());
  } else if (node->primaryType == PrimaryExp::PrimaryType::NUMBER) {
    visit(node->number.get());
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(Number *node) {
  if (node == nullptr)
    return nullptr;
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(UnaryExp *node) {
  if (node == nullptr)
    return nullptr;
  if (node->unaryType == UnaryExp::UnaryType::PRIMARY) {
    visit(node->primaryExp.get());
  } else if (node->unaryType == UnaryExp::UnaryType::UNARY_OP) {
    visit(node->unaryOp.get());
    visit(node->unaryExp.get());
  } else if (node->unaryType == UnaryExp::UnaryType::FUNC_CALL) {
    visit(node->funcRParams.get());
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(UnaryOp *node) {
  if (node == nullptr)
    return nullptr;
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(FuncRParams *node) {
  if (node == nullptr)
    return nullptr;
  for (auto &exp : node->exps) {
    visit(exp.get());
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(MulExp *node) {
  if (node == nullptr)
    return nullptr;
  do {
    visit(node->left.get());
    visit(node->unaryExp.get());
  } while (node->left);
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(AddExp *node) {
  if (node == nullptr)
    return nullptr;
  do {
    visit(node->left.get());
    visit(node->mulExp.get());
  } while (node->left);
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(RelExp *node) {
  if (node == nullptr)
    return nullptr;
  do {
    visit(node->left.get());
    visit(node->addExp.get());
  } while (node->left);
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(EqExp *node) {
  if (node == nullptr)
    return nullptr;
  do {
    visit(node->left.get());
    visit(node->relExp.get());
  } while (node->left);
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(LAndExp *node) {
  if (node == nullptr)
    return nullptr;
  do {
    visit(node->left.get());
    visit(node->eqExp.get());
  } while (node->left);
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(LOrExp *node) {
  if (node == nullptr)
    return nullptr;
  do {
    visit(node->left.get());
    visit(node->lAndExp.get());
  } while (node->left);
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(ConstExp *node) {
  if (node == nullptr)
    return nullptr;
  visit(node->addExp.get());
  return nullptr;
}
