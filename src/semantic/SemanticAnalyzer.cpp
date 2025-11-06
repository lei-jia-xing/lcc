#include "parser/AST.hpp"
#include <semantic/SemanticAnalyzer.hpp>
#include <semantic/Symbol.hpp>
#include <semantic/SymbolTable.hpp>
#include <semantic/Type.hpp>

SemanticAnalyzer::SemanticAnalyzer() {}

void SemanticAnalyzer::error(const int &line, const std::string errorType) {
  std::cerr << line << " " << errorType << std::endl;
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
  symbolTable.printTable();
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
  if (node->arraySize == nullptr) {
    type->category = Type::Category::Basic;
  } else {
    type->category = Type::Category::Array;
    visit(node->arraySize.get());
  }
  Symbol symbol(node->ident, type, node->line);
  if (!symbolTable.addSymbol(symbol)) {
    error(node->line, "b");
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
  Symbol symbol(node->ident, type, node->line);
  if (!symbolTable.addSymbol(symbol)) {
    error(node->line, "b");
  }
}

std::vector<Type::FuncParam> SemanticAnalyzer::visit(FuncFParams *node) {
  if (node == nullptr)
    return {};

  std::vector<Type::FuncParam> params;
  for (auto &paramNode : node->params) {
    params.push_back(visit(paramNode.get()));
  }
  return params;
}

Type::FuncParam SemanticAnalyzer::visit(FuncFParam *node) {
  if (node == nullptr)
    return {};

  TypePtr type = visit(node->bType.get());
  if (node->isArray) {
    type = Type::create_array_type(type, -1);
  }
  Type::FuncParam param{node->ident, type};

  if (!symbolTable.findSymbol(node->ident)) {
    error(node->line, "b");
    return {};
  }
  return param;
}

void SemanticAnalyzer::visit(FuncDef *node) {
  if (node == nullptr)
    return;

  auto returnType = visit(node->funcType.get());

  std::vector<Type::FuncParam> params = visit(node->params.get());

  auto funcType = Type::create_function_type(returnType, params);

  if (!symbolTable.addSymbol(Symbol(node->ident, funcType, node->line))) {
    error(node->line, "b");
  }

  symbolTable.pushScope();

  has_return = false;
  current_function_return_type = returnType;
  bool needs_return = (returnType->base_type == BaseType::INT);
  for (auto param : params) {
    if (!symbolTable.addSymbol(Symbol(param.name, param.type, node->line))) {
      error(node->line, "b");
    }
  }
  visit(node->block.get());

  if (needs_return && !has_return) {
    // Use the closing brace line number for error reporting
    int errorLine = (node->block->closingBraceLine > 0)
                        ? node->block->closingBraceLine
                        : node->line;
    error(errorLine, "g");
  }

  current_function_return_type = nullptr;
  symbolTable.popScope();
}

void SemanticAnalyzer::visit(MainFuncDef *node) {
  if (node == nullptr)
    return;

  // Main function returns int, so it needs a return statement
  symbolTable.pushScope();

  has_return = false;
  current_function_return_type =
      Type::getIntType(); // main function returns int
  bool needs_return = true;

  visit(node->block.get());

  // Check if main function is missing return statement
  if (needs_return && !has_return) {
    // Use the closing brace line number for error reporting
    int errorLine = (node->block->closingBraceLine > 0)
                        ? node->block->closingBraceLine
                        : node->line;
    error(errorLine, "g");
  }

  current_function_return_type = nullptr;
  symbolTable.popScope();
}

void SemanticAnalyzer::visit(Block *node) {
  if (node == nullptr)
    return;
  symbolTable.pushScope();
  for (auto &item : node->items) {
    visit(item.get());
  }
  symbolTable.popScope();
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
  auto symbolOpt = visit(node->lval.get());
  visit(node->exp.get());
  if (symbolOpt.has_value() && symbolOpt->type->is_const) {
    error(node->line, "h");
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
  in_loop = true;
  visit(node->bodyStmt.get());
  in_loop = false;
}
void SemanticAnalyzer::visit(BreakStmt *node) {
  if (node == nullptr)
    return;
  if (!in_loop) {
    error(node->line, "m");
  }
}
void SemanticAnalyzer::visit(ContinueStmt *node) {
  if (node == nullptr)
    return;
  if (!in_loop) {
    error(node->line, "m");
  }
}
void SemanticAnalyzer::visit(ReturnStmt *node) {
  if (node == nullptr)
    return;

  has_return = true;

  // Check return type mismatch (error type f)
  if (current_function_return_type) {
    if (node->exp) {
      TypePtr exprType = getExpressionType(node->exp.get());
      // If function expects void but has return value
      if (current_function_return_type->base_type == BaseType::VOID &&
          exprType) {
        error(node->line, "f");
      }
    } else {
      // If function expects int but no return value
      if (current_function_return_type->base_type == BaseType::INT) {
        error(node->line, "f");
      }
    }
  }

  visit(node->exp.get());
}
void SemanticAnalyzer::visit(PrintfStmt *node) {
  if (node == nullptr)
    return;

  // Count format specifiers in the format string
  int formatCount = 0;
  for (size_t i = 0; i < node->formatString.length(); ++i) {
    if (node->formatString[i] == '%' && i + 1 < node->formatString.length() &&
        node->formatString[i + 1] == 'd') {
      formatCount++;
      i++; // Skip the 'd'
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
    auto symbolOpt = visit(assignment.lval.get());
    if (symbolOpt.has_value() && symbolOpt->type->is_const) {
      error(node->line, "h");
    }
    visit(assignment.exp.get());
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
    return Type::create_function_type(Type::create_base_type(BaseType::INT),
                                      {});
  } else if (node->type == "void") {
    return Type::create_function_type(Type::create_base_type(BaseType::VOID),
                                      {});
  }
  return nullptr;
}
void SemanticAnalyzer::visit(Exp *node) {
  if (node == nullptr)
    return;
  visit(node->addExp.get());
}
void SemanticAnalyzer::visit(Cond *node) {
  if (node == nullptr)
    return;
  visit(node->lOrExp.get());
}
std::optional<Symbol> SemanticAnalyzer::visit(LVal *node) {
  if (node == nullptr)
    return std::nullopt;

  // Check symbol table, if not found, error type c (undefined name)
  auto symbolOpt = symbolTable.findSymbol(node->ident);
  if (!symbolOpt.has_value()) {
    error(node->line, "c");
    return std::nullopt;
  }

  // Visit array index if present
  if (node->arrayIndex) {
    visit(node->arrayIndex.get());
  }

  return symbolOpt;
}
void SemanticAnalyzer::visit(PrimaryExp *node) {
  if (node == nullptr)
    return;
  if (node->primaryType == PrimaryExp::PrimaryType::EXP) {
    visit(node->exp.get());
  } else if (node->primaryType == PrimaryExp::PrimaryType::LVAL) {
    visit(node->lval.get());
  } else if (node->primaryType == PrimaryExp::PrimaryType::NUMBER) {
    visit(node->number.get());
  }
}
void SemanticAnalyzer::visit(Number *node) {
  if (node == nullptr)
    return;
}
void SemanticAnalyzer::visit(UnaryExp *node) {
  if (node == nullptr)
    return;
  if (node->unaryType == UnaryExp::UnaryType::PRIMARY) {
    visit(node->primaryExp.get());
  } else if (node->unaryType == UnaryExp::UnaryType::UNARY_OP) {
    visit(node->unaryOp.get());
    visit(node->unaryExp.get());
  } else if (node->unaryType == UnaryExp::UnaryType::FUNC_CALL) {
    auto funcSymbolOpt = symbolTable.findSymbol(node->funcIdent);
    if (!funcSymbolOpt.has_value()) {
      error(node->line, "c");
      return;
    }
    if (funcSymbolOpt.value().type->params.size() !=
        node->funcRParams->exps.size()) {
      error(node->line, "d");
      return;
    }
    for (auto param : funcSymbolOpt.value().type->params) {
      if (param.type != getLValType(param.name)) {
        error(node->line, "e");
        return;
      }
    }
    visit(node->funcRParams.get());
  }
  return;
}
void SemanticAnalyzer::visit(UnaryOp *node) {
  if (node == nullptr)
    return;
}
void SemanticAnalyzer::visit(FuncRParams *node) {
  if (node == nullptr)
    return;
  for (auto &exp : node->exps) {
    visit(exp.get());
  }
}
void SemanticAnalyzer::visit(MulExp *node) {
  if (node == nullptr)
    return;
  visit(node->left.get());
  visit(node->unaryExp.get());
}
void SemanticAnalyzer::visit(AddExp *node) {
  if (node == nullptr)
    return;
  visit(node->left.get());
  visit(node->mulExp.get());
}
void SemanticAnalyzer::visit(RelExp *node) {
  if (node == nullptr)
    return;
  visit(node->left.get());
  visit(node->addExp.get());
}
void SemanticAnalyzer::visit(EqExp *node) {
  if (node == nullptr)
    return;
  visit(node->left.get());
  visit(node->relExp.get());
}
void SemanticAnalyzer::visit(LAndExp *node) {
  if (node == nullptr)
    return;
  visit(node->left.get());
  visit(node->eqExp.get());
}
void SemanticAnalyzer::visit(LOrExp *node) {
  if (node == nullptr)
    return;
  visit(node->left.get());
  visit(node->lAndExp.get());
}
void SemanticAnalyzer::visit(ConstExp *node) {
  if (node == nullptr)
    return;
  visit(node->addExp.get());
}

TypePtr SemanticAnalyzer::getExpressionType(Exp *exp) {
  if (exp == nullptr)
    return nullptr;

  // For now, assume all expressions return int type
  // This is a simplified implementation
  return Type::getIntType();
}

// Helper method to get LVal type (can be array or int)
TypePtr SemanticAnalyzer::getLValType(const std::string &ident) {
  auto symbolOpt = symbolTable.findSymbol(ident);
  if (!symbolOpt.has_value()) {
    return nullptr;
  }
  return symbolOpt.value().type;
}
