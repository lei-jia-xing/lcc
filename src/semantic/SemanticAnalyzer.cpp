#include "parser/AST.hpp"
#include <errorReporter/ErrorReporter.hpp>
#include <optional>
#include <semantic/SemanticAnalyzer.hpp>
#include <semantic/Symbol.hpp>
#include <semantic/SymbolTable.hpp>
#include <semantic/Type.hpp>

SemanticAnalyzer::SemanticAnalyzer() {}

void SemanticAnalyzer::error(const int &line, const std::string& errorType) {
  ErrorReporter::getInstance().addError(line, errorType);
}

bool endsWithReturn(Block *block) {
  if (!block)
    return false;
  if (block->items.empty())
    return false;
  auto &item = block->items.back();
  return item->stmt && item->stmt->stmtType == Stmt::StmtType::RETURN;
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
  if (outputenabled) {
    symbolTable.printTable();
  }
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
    return Type::getIntType();
  }
  return nullptr;
}

void SemanticAnalyzer::visit(ConstDef *node, TypePtr type) {
  if (node == nullptr)
    return;
  TypePtr defType;
  if (node->arraySize == nullptr) {
    defType = std::make_shared<Type>(*type);
    defType->category = Type::Category::Basic;
  } else {
    visit(node->arraySize.get());
    TypePtr elem = std::make_shared<Type>(*type);
    elem->category = Type::Category::Basic;
    defType = Type::create_array_type(elem, 0);
  }
  node->type = defType; // Store type in AST node
  Symbol symbol(node->ident, defType, node->line);
  if (!symbolTable.addSymbol(symbol)) {
    error(node->line, "b");
  }
}

void SemanticAnalyzer::visit(VarDef *node, TypePtr type) {
  if (node == nullptr)
    return;
  TypePtr defType;
  if (node->arraySize == nullptr) {
    defType = std::make_shared<Type>(*type);
    defType->category = Type::Category::Basic;
  } else {
    visit(node->arraySize.get());
    TypePtr elem = std::make_shared<Type>(*type);
    elem->category = Type::Category::Basic;
    defType = Type::create_array_type(elem, 0);
  }
  node->type = defType; // Store type in AST node
  Symbol symbol(node->ident, defType, node->line);
  if (!symbolTable.addSymbol(symbol)) {
    error(node->line, "b");
  }
}

void SemanticAnalyzer::visit(FuncFParams *node) {
  if (node == nullptr)
    return;

  for (auto &paramNode : node->params) {
    visit(paramNode.get());
  }
}

void SemanticAnalyzer::visit(FuncFParam *node) {
  if (node == nullptr)
    return;

  TypePtr type = visit(node->bType.get());
  if (node->isArray) {
    type = Type::create_array_type(type, -1);
  }

  Symbol paramSymbol(node->ident, type, node->line);
  if (!symbolTable.addSymbol(paramSymbol)) {
    error(node->identLine, "b");
  }
}

void SemanticAnalyzer::visit(FuncDef *node) {
  if (node == nullptr)
    return;

  TypePtr returnType = visit(node->funcType.get());
  std::vector<TypePtr> params;
  if (node->params != nullptr) {
    for (auto &paramNode : node->params->params) {
      TypePtr paramType = visit(paramNode->bType.get());
      if (paramNode->isArray) {
        paramType = Type::create_array_type(paramType, -1);
      }
      params.push_back(paramType);
    }
  }
  auto funcType = Type::create_function_type(returnType, params);

  if (!symbolTable.addSymbol(Symbol(node->ident, funcType, node->line))) {
    error(node->identLine, "b");
  }

  symbolTable.pushScope();

  current_function_return_type = returnType;
  bool needs_return = (returnType->base_type == BaseType::INT);

  visit(node->params.get());
  visit(node->block.get());

  if (needs_return) {
    bool ok = endsWithReturn(node->block.get());
    if (!ok) {
      error(node->block->closingBraceLine, "g");
    }
  }

  current_function_return_type = nullptr;
  symbolTable.popScope();
}

void SemanticAnalyzer::visit(MainFuncDef *node) {
  if (node == nullptr)
    return;

  current_function_return_type = Type::getIntType();
  bool needs_return = true;

  symbolTable.pushScope();
  visit(node->block.get());
  symbolTable.popScope();

  if (needs_return) {
    bool ok = endsWithReturn(node->block.get());
    if (!ok) {
      error(node->block->closingBraceLine, "g");
    }
  }

  current_function_return_type = nullptr;
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
    symbolTable.pushScope();
    visit(static_cast<BlockStmt *>(node));
    symbolTable.popScope();
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
  TypePtr type = visit(node->lval.get());
  visit(node->exp.get());
  if (type && type->is_const) {
    error(node->lval->line, "h");
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
  loop++;
  visit(node->cond.get());
  visit(node->updateStmt.get());
  visit(node->bodyStmt.get());
  loop--;
}
void SemanticAnalyzer::visit(BreakStmt *node) {
  if (node == nullptr)
    return;
  if (loop == 0) {
    error(node->line, "m");
  }
}
void SemanticAnalyzer::visit(ContinueStmt *node) {
  if (node == nullptr)
    return;
  if (loop == 0) {
    error(node->line, "m");
  }
}
void SemanticAnalyzer::visit(ReturnStmt *node) {
  if (node == nullptr)
    return;

  if (current_function_return_type->base_type == BaseType::VOID) {
    if (node->exp != nullptr) {
      error(node->line, "f");
    }
  }
}
void SemanticAnalyzer::visit(PrintfStmt *node) {
  if (node == nullptr)
    return;

  int formatCount = 0;
  for (size_t i = 0; i < node->formatString.length(); ++i) {
    if (node->formatString[i] == '%' && i + 1 < node->formatString.length() &&
        node->formatString[i + 1] == 'd') {
      formatCount++;
      i++;
    }
  }

  int argCount = node->args.size();

  if (formatCount != argCount) {
    error(node->line, "l");
  }

  for (auto &exp : node->args) {
    visit(exp.get());
  }
}

void SemanticAnalyzer::visit(ForAssignStmt *node) {
  if (node == nullptr)
    return;
  for (auto &assignment : node->assignments) {
    TypePtr type = visit(assignment.lval.get());
    if (type && type->is_const) {
      error(assignment.lval->line, "h");
    }
  }
}
void SemanticAnalyzer::visit(ConstInitVal *node) {
  if (node == nullptr)
    return;
  if (node->isArray) {
    for (auto &constexp : node->arrayExps) {
      visit(constexp.get());
    }
  } else {
    visit(node->exp.get());
  }
}
void SemanticAnalyzer::visit(InitVal *node) {
  if (node == nullptr)
    return;
  if (node->isArray) {
    for (auto &exp : node->arrayExps) {
      visit(exp.get());
    }
  } else {
    visit(node->exp.get());
  }
}
TypePtr SemanticAnalyzer::visit(FuncType *node) {
  if (node == nullptr)
    return nullptr;
  if (node->type == "int") {
    return Type::getIntType();
  } else if (node->type == "void") {
    return Type::getVoidType();
  }
  return nullptr;
}
TypePtr SemanticAnalyzer::visit(Exp *node) {
  if (node == nullptr)
    return nullptr;
  node->type = visit(node->addExp.get());
  return node->type;
}
TypePtr SemanticAnalyzer::visit(Cond *node) {
  if (node == nullptr)
    return nullptr;
  node->type = visit(node->lOrExp.get());
  return node->type;
}
TypePtr SemanticAnalyzer::visit(LVal *node) {
  if (node == nullptr)
    return nullptr;

  auto symbolOpt = symbolTable.findSymbol(node->ident);
  if (!symbolOpt.has_value()) {
    error(node->line, "c");
    return nullptr;
  }
  if (node->arrayIndex) {
    visit(node->arrayIndex.get());
    return symbolOpt.value().type->array_element_type;
  }
  return symbolOpt.value().type;
}
TypePtr SemanticAnalyzer::visit(PrimaryExp *node) {
  if (node == nullptr)
    return nullptr;
  if (node->primaryType == PrimaryExp::PrimaryType::EXP) {
    node->type = visit(node->exp.get());
  } else if (node->primaryType == PrimaryExp::PrimaryType::LVAL) {
    node->type = visit(node->lval.get());
  } else if (node->primaryType == PrimaryExp::PrimaryType::NUMBER) {
    node->type = visit(node->number.get());
  }
  return node->type;
}
TypePtr SemanticAnalyzer::visit(Number *node) {
  if (node == nullptr)
    return nullptr;
  return Type::getIntType();
}
TypePtr SemanticAnalyzer::visit(UnaryExp *node) {
  if (node == nullptr)
    return nullptr;
  if (node->unaryType == UnaryExp::UnaryType::PRIMARY) {
    node->type = visit(node->primaryExp.get());
  } else if (node->unaryType == UnaryExp::UnaryType::UNARY_OP) {
    visit(node->unaryOp.get());
    node->type = visit(node->unaryExp.get());
  } else if (node->unaryType == UnaryExp::UnaryType::FUNC_CALL) {
    if (node->funcIdent == "getint") {
      size_t actualParams =
          (node->funcRParams != nullptr) ? node->funcRParams->exps.size() : 0;
      if (actualParams != 0) {
        error(node->line, "d");
        node->type = nullptr;
        return nullptr;
      }
      node->type = Type::getIntType();
      return node->type;
    }

    auto funcSymbolOpt = symbolTable.findSymbol(node->funcIdent);
    if (!funcSymbolOpt.has_value()) {
      error(node->line, "c");
      node->type = nullptr;
      return nullptr;
    }
    size_t expectedParams = funcSymbolOpt.value().type->params.size();
    size_t actualParams =
        (node->funcRParams != nullptr) ? node->funcRParams->exps.size() : 0;

    if (expectedParams != actualParams) {
      error(node->line, "d");
      node->type = nullptr;
      return nullptr;
    }
    std::vector<TypePtr> argTypes;
    if (node->funcRParams != nullptr) {
      argTypes = visit(node->funcRParams.get());
    }
    for (size_t i = 0; i < argTypes.size(); ++i) {
      TypePtr expectedType = funcSymbolOpt.value().type->params[i];
      TypePtr actualType = argTypes[i];
      if (expectedType->category != actualType->category) {
        error(node->line, "e");
      }
    }
    node->type = funcSymbolOpt.value().type->return_type;
  }
  return node->type;
}
void SemanticAnalyzer::visit(UnaryOp *node) {
  if (node == nullptr)
    return;
}
std::vector<TypePtr> SemanticAnalyzer::visit(FuncRParams *node) {
  if (node == nullptr)
    return {};
  // Prevent accumulation when node visited multiple times (avoids duplicate 'e'
  // and potential OOB)
  node->types.clear();
  for (auto &exp : node->exps) {
    node->types.push_back(visit(exp.get()));
  }
  return node->types;
}
TypePtr SemanticAnalyzer::visit(MulExp *node) {
  if (node == nullptr)
    return nullptr;
  auto ltype = visit(node->left.get());
  auto rtype = visit(node->unaryExp.get());
  if (rtype && !ltype) {
    node->type = rtype;
  } else {
    node->type = Type::getIntType();
  }
  return node->type;
}

TypePtr SemanticAnalyzer::visit(AddExp *node) {
  if (node == nullptr)
    return nullptr;
  auto ltype = visit(node->left.get());
  auto rtype = visit(node->mulExp.get());
  if (rtype && !ltype) {
    node->type = rtype;
  } else {
    node->type = Type::getIntType();
  }
  return node->type;
}
TypePtr SemanticAnalyzer::visit(RelExp *node) {
  if (node == nullptr)
    return nullptr;
  auto ltype = visit(node->left.get());
  auto rtype = visit(node->addExp.get());
  if (rtype && !ltype) {
    node->type = rtype;
  } else {
    node->type = Type::getIntType();
  }
  return node->type;
}
TypePtr SemanticAnalyzer::visit(EqExp *node) {
  if (node == nullptr)
    return nullptr;
  auto ltype = visit(node->left.get());
  auto rtype = visit(node->relExp.get());
  if (rtype && !ltype) {
    node->type = rtype;
  } else {
    node->type = Type::getIntType();
  }
  return node->type;
}
TypePtr SemanticAnalyzer::visit(LAndExp *node) {
  if (node == nullptr)
    return nullptr;
  auto ltype = visit(node->left.get());
  auto rtype = visit(node->eqExp.get());
  if (rtype && !ltype) {
    node->type = rtype;
  } else {
    node->type = Type::getIntType();
  }
  return node->type;
}
TypePtr SemanticAnalyzer::visit(LOrExp *node) {
  if (node == nullptr)
    return nullptr;
  auto ltype = visit(node->left.get());
  auto rtype = visit(node->lAndExp.get());
  if (rtype && !ltype) {
    node->type = rtype;
  } else {
    node->type = Type::getIntType();
  }
  return node->type;
}
TypePtr SemanticAnalyzer::visit(ConstExp *node) {
  if (node == nullptr)
    return nullptr;
  node->type = visit(node->addExp.get());
  return node->type;
}
