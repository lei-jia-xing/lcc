#include "parser/Parser.hpp"
#include "lexer/Token.hpp"
#include "parser/AST.hpp"
#include <iostream>
#include <memory>

Parser::Parser(Lexer &&lexer, Token current)
    : lexer(std::move(lexer)), current(current) {}
void Parser::advance() { current = lexer.nextToken(); }
bool Parser::match(TokenType type) {
  if (current.type == type) {
    advance();
    return true;
  }
  return false;
}
void Parser::error(const int &line, const std::string errorType) {
  std::cerr << line << " " << errorType << std::endl;
}
std::unique_ptr<CompUnit> Parser::parseCompUnit() {
  auto compUnit = std::make_unique<CompUnit>();
  while (current.type == TokenType::CONSTTK ||
         current.type == TokenType::INTTK ||
         current.type == TokenType::STATICTK) {
    compUnit->decls.push_back(parseDecl());
  }
  while (current.type == TokenType::VOIDTK ||
         current.type == TokenType::INTTK) {
    compUnit->funcDefs.push_back(parseFuncDef());
  }
  compUnit->mainFuncDef = parseMainFuncDef();
  std::cout << "<CompUnit>" << std::endl;
  return compUnit;
}

std::unique_ptr<Decl> Parser::parseDecl() {
  auto decl = std::make_unique<Decl>();
  if (current.type == TokenType::CONSTTK) {
    decl = parseConstDecl();
  } else {
    decl = parseVarDecl();
  }
  std::cout << "<Decl>" << std::endl;
  return decl;
}

std::unique_ptr<ConstDecl> Parser::parseConstDecl() {
  auto constDecl = std::make_unique<ConstDecl>();
  advance(); // eat const
  constDecl->bType = parseBType();
  constDecl->constDefs.push_back(parseConstDef());
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    constDecl->constDefs.push_back(parseConstDef());
  }
  if (current.type != TokenType::SEMICN) {
    error(current.line, "i");
  }
  advance(); // eat semicn
  std::cout << "<ConstDecl>" << std::endl;
  return constDecl;
}

std::unique_ptr<VarDecl> Parser::parseVarDecl() {
  auto varDecl = std::make_unique<VarDecl>();
  if (current.type == TokenType::STATICTK) {
    varDecl->isStatic = true;
  }
  varDecl->bType = parseBType();
  varDecl->varDefs.push_back(parseVarDef());
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    varDecl->varDefs.push_back(parseVarDef());
  }
  if (current.type != TokenType::SEMICN) {
    error(current.line, "i");
  }
  advance(); // eat semicn
  std::cout << "<VarDecl>" << std::endl;
  return varDecl;
}

std::unique_ptr<BType> Parser::parseBType() {
  auto bType = std::make_unique<BType>();
  if (current.type == TokenType::INTTK) {
    bType->type = "int";
    bType->line = current.line;
  } else {
    // error();
  }
  advance(); // eat int
  std::cout << "<BType>" << std::endl;
  return bType;
}

std::unique_ptr<ConstDef> Parser::parseConstDef() {
  auto constDef = std::make_unique<ConstDef>();
  constDef->ident = current.lexeme;
  advance(); // eat ident
  if (current.type == TokenType::LBRACK) {
    constDef->arraySize = parseConstExp();
    advance(); // eat lbrack
    if (current.type != TokenType::RBRACK) {
      error(current.line, "k");
    }
    advance(); // eat rbrack
    if (current.type != TokenType::ASSIGN) {
      // error();
    }
    advance(); // eat assign
    constDef->constinitVal = parseConstInitVal();
    std::cout << "<ConstDef>" << std::endl;
    return constDef;

  } else if (current.type == TokenType::ASSIGN) {
    advance(); // eat assign
    constDef->constinitVal = parseConstInitVal();
    std::cout << "<ConstDef>" << std::endl;
    return constDef;
  } else {
    // error();
    return constDef;
  }
}

std::unique_ptr<VarDef> Parser::parseVarDef() {
  auto varDef = std::make_unique<VarDef>();
  varDef->ident = current.lexeme;
  advance(); // eat ident

  if (current.type == TokenType::LBRACK) {
    advance(); // eat lbrack
    varDef->arraySize = parseConstExp();
    if (current.type != TokenType::RBRACK) {
      error(current.line, "k");
    }
    advance(); // eat rbrack
  } else {
    varDef->arraySize = nullptr;
  }

  if (current.type == TokenType::ASSIGN) {
    advance(); // eat assign
    varDef->initVal = parseInitVal();
  } else {
    varDef->initVal = nullptr;
  }

  std::cout << "<VarDef>" << std::endl;
  return varDef;
}

std::unique_ptr<ConstInitVal> Parser::parseConstInitVal() {
  auto constInitVal = std::make_unique<ConstInitVal>();
  if (current.type == TokenType::LBRACE) {
    constInitVal->isArray = true;
    advance(); // eat lbrace
    if (current.type != TokenType::RBRACE) {
      constInitVal->arrayExps.push_back(parseConstExp());
      while (current.type == TokenType::COMMA) {
        advance(); // eat comma
        constInitVal->arrayExps.push_back(parseConstExp());
      }
    }
    if (current.type != TokenType::RBRACE) {
      // error();
    }
    advance(); // eat rbrace
  } else {
    constInitVal->exp = parseConstExp();
  }
  std::cout << "<ConstInitVal>" << std::endl;
  return constInitVal;
}

std::unique_ptr<InitVal> Parser::parseInitVal() {
  auto initVal = std::make_unique<InitVal>();
  if (current.type == TokenType::LBRACE) {
    initVal->isArray = true;
    advance(); // eat lbrace
    if (current.type != TokenType::RBRACE) {
      initVal->arrayExps.push_back(parseExp());
      while (current.type == TokenType::COMMA) {
        advance(); // eat comma
        initVal->arrayExps.push_back(parseExp());
      }
    }
    if (current.type != TokenType::RBRACE) {
      // error();
    }
    advance(); // eat rbrace
  } else {
    initVal->exp = parseExp();
  }
  std::cout << "<InitVal>" << std::endl;
  return initVal;
}

std::unique_ptr<FuncDef> Parser::parseFuncDef() {
  auto funcDef = std::make_unique<FuncDef>();
  funcDef->funcType = parseFuncType();
  funcDef->ident = current.lexeme;
  advance(); // eat ident
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  advance(); // eat lparent
  if (current.type != TokenType::RPARENT) {
    funcDef->params = parseFuncFParams();
    if (current.type != TokenType::RPARENT) {
      error(current.line, "j");
    }
  }
  advance(); // eat rparent
  funcDef->block = parseBlock();
  std::cout << "<FuncDef>" << std::endl;
  return funcDef;
}

std::unique_ptr<MainFuncDef> Parser::parseMainFuncDef() {
  auto mainFuncDef = std::make_unique<MainFuncDef>();
  if (current.type != TokenType::INTTK) {
    // error();
  }
  advance(); // eat int
  if (current.type != TokenType::MAINTK) {
    // error();
  }
  advance(); // eat main
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  advance(); // eat lparent
  if (current.type != TokenType::RPARENT) {
    error(current.line, "j");
  }
  advance(); // eat rparent
  mainFuncDef->block = parseBlock();
  std::cout << "<MainFuncDef>" << std::endl;
  return mainFuncDef;
}

std::unique_ptr<FuncType> Parser::parseFuncType() {
  auto funcType = std::make_unique<FuncType>();
  if (current.type == TokenType::VOIDTK) {
    funcType->type = "void";
    funcType->line = current.line;
  } else if (current.type == TokenType::INTTK) {
    funcType->type = "int";
    funcType->line = current.line;
  } else {
    // error();
  }
  advance();
  std::cout << "<FuncType>" << std::endl;
  return funcType;
}

std::unique_ptr<FuncFParams> Parser::parseFuncFParams() {
  auto funcFParams = std::make_unique<FuncFParams>();
  funcFParams->params.push_back(parseFuncFParam());
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    funcFParams->params.push_back(parseFuncFParam());
  }
  std::cout << "<FuncFParams>" << std::endl;
  return funcFParams;
}

std::unique_ptr<FuncFParam> Parser::parseFuncFParam() {
  auto funcFParam = std::make_unique<FuncFParam>();
  funcFParam->bType = parseBType();
  funcFParam->ident = current.lexeme;
  advance(); // eat ident
  if (current.type == TokenType::LBRACK) {
    advance(); // eat lbrack
    if (current.type != TokenType::RBRACK) {
      error(current.line, "k");
    }
    advance(); // eat rbrack
    funcFParam->isArray = true;
  }
  std::cout << "<FuncFParam>" << std::endl;
  return funcFParam;
}

std::unique_ptr<Block> Parser::parseBlock() {
  auto block = std::make_unique<Block>();
  if (current.type != TokenType::LBRACE) {
    // error();
  }
  advance(); // eat lbrace
  if (current.type != TokenType::RBRACE) {
    block->items.push_back(parseBlockItem());
    while (current.type != TokenType::RBRACE) {
      block->items.push_back(parseBlockItem());
    }
  }
  if (current.type != TokenType::RBRACE) {
    // error();
  }
  advance(); // eat rbrace
  std::cout << "<Block>" << std::endl;
  return block;
}

std::unique_ptr<BlockItem> Parser::parseBlockItem() {
  auto blockItem = std::make_unique<BlockItem>();
  if (current.type == TokenType::CONSTTK || current.type == TokenType::INTTK ||
      current.type == TokenType::STATICTK) {
    blockItem->decl = parseDecl();
  } else {
    blockItem->stmt = parseStmt();
  }
  std::cout << "<BlockItem>" << std::endl;
  return blockItem;
}

std::unique_ptr<IfStmt> Parser::parseIfStmt() {
  auto ifStmt = std::make_unique<IfStmt>();
  if (current.type != TokenType::IFTK) {
    // error();
  }
  advance(); // eat if
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  advance(); // eat lparent
  ifStmt->cond = parseCond();
  if (current.type != TokenType::RPARENT) {
    error(current.line, "j");
  }
  advance(); // eat rparent
  ifStmt->thenStmt = parseStmt();
  ifStmt->elseStmt = nullptr;
  if (current.type == TokenType::ELSETK) {
    advance(); // eat else
    ifStmt->elseStmt = parseStmt();
  }
  std::cout << "<Stmt>" << std::endl;
  return ifStmt;
}
std::unique_ptr<ForStmt> Parser::parseForStmtStmt() {
  auto forStmt = std::make_unique<ForStmt>();
  if (current.type != TokenType::FORTK) {
    // error();
  }
  advance(); // eat for
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  advance(); // eat lparent
  forStmt->initStmt = nullptr;
  if (current.type != TokenType::SEMICN) {
    forStmt->initStmt = parseForAssignStmt();
  }
  advance(); // eat semicn
  if (current.type != TokenType::SEMICN) {
    forStmt->cond = parseCond();
  }
  advance(); // eat semicn
  if (current.type != TokenType::RPARENT) {
    forStmt->updateStmt = parseForAssignStmt();
  }
  advance(); // eat rparent
  forStmt->bodyStmt = parseStmt();
  std::cout << "<Stmt>" << std::endl;
  return forStmt;
}
std::unique_ptr<BreakStmt> Parser::parseBreakStmt() {
  auto breakStmt = std::make_unique<BreakStmt>();
  if (current.type != TokenType::BREAKTK) {
    // error();
  }
  advance(); // eat break
  if (current.type != TokenType::SEMICN) {
    error(current.line, "i");
  }
  breakStmt->line = current.line;
  advance(); // eat semicn
  std::cout << "<Stmt>" << std::endl;
  return breakStmt;
}
std::unique_ptr<ContinueStmt> Parser::parseContinueStmt() {
  auto continueStmt = std::make_unique<ContinueStmt>();
  if (current.type != TokenType::CONTINUETK) {
    // error();
  }
  advance();
  if (current.type != TokenType::SEMICN) {
    error(current.line, "i");
  }
  continueStmt->line = current.line;
  advance();
  std::cout << "<Stmt>" << std::endl;
  return continueStmt;
}
std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
  auto returnStmt = std::make_unique<ReturnStmt>();
  if (current.type != TokenType::RETURNTK) {
    // error();
  }
  advance();
  returnStmt->exp = nullptr;
  if (current.type != TokenType::SEMICN) {
    returnStmt->exp = parseExp();
    if (current.type != TokenType::SEMICN) {
      error(current.line, "i");
    }
  }
  advance();
  std::cout << "<Stmt>" << std::endl;
  return returnStmt;
}
std::unique_ptr<PrintfStmt> Parser::parsePrintfStmt() {
  auto printfStmt = std::make_unique<PrintfStmt>();
  if (current.type != TokenType::PRINTFTK) {
    // error();
  }
  advance();
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  advance();
  printfStmt->formatString = current.lexeme;
  advance();
  while (current.type == TokenType::COMMA) {
    advance();
    printfStmt->args.push_back(parseExp());
  }
  if (current.type != TokenType::RPARENT) {
    error(current.line, "j");
  }
  advance();
  if (current.type != TokenType::SEMICN) {
    error(current.line, "i");
  }
  advance();
  std::cout << "<Stmt>" << std::endl;
  return printfStmt;
}
std::unique_ptr<AssignStmt> Parser::parseAssignStmt() {
  auto assignStmt = std::make_unique<AssignStmt>();
  assignStmt->lval = parseLVal();
  if (current.type != TokenType::ASSIGN) {
    // error();
  }
  advance();
  assignStmt->exp = parseExp();
  if (current.type != TokenType::SEMICN) {
    error(current.line, "i");
  }
  advance();
  std::cout << "<Stmt>" << std::endl;
  return assignStmt;
}
std::unique_ptr<ExpStmt> Parser::parseExpStmt() {
  auto expStmt = std::make_unique<ExpStmt>();
  if (current.type != TokenType::SEMICN) {
    expStmt->exp = parseExp();
    if (current.type != TokenType::SEMICN) {
      error(current.line, "i");
    }
  }
  advance();
  std::cout << "<Stmt>" << std::endl;
  return expStmt;
}
std::unique_ptr<BlockStmt> Parser::parseBlockStmt() {
  auto blockStmt = std::make_unique<BlockStmt>();
  blockStmt->block = parseBlock();
  std::cout << "<Stmt>" << std::endl;
  return blockStmt;
}

std::unique_ptr<Stmt> Parser::parseStmt() {

  if (current.type == TokenType::IFTK) {
    return parseIfStmt();
  } else if (current.type == TokenType::FORTK) {
    return parseForStmtStmt();
  } else if (current.type == TokenType::BREAKTK) {
    return parseBreakStmt();
  } else if (current.type == TokenType::CONTINUETK) {
    return parseContinueStmt();
  } else if (current.type == TokenType::RETURNTK) {
    return parseReturnStmt();
  } else if (current.type == TokenType::PRINTFTK) {
    return parsePrintfStmt();
  } else if (current.type == TokenType::LBRACE) {
    return parseBlockStmt();
  } else if (current.type == TokenType::IDENFR) {
    auto temp = lexer;
    auto tempCurrent = current;
    auto lval = parseLVal();
    if (current.type == TokenType::ASSIGN) {
      lexer = temp;
      current = tempCurrent;
      return parseAssignStmt();
    } else {
      lexer = temp;
      current = tempCurrent;
      return parseExpStmt();
    }
  } else {
    return parseExpStmt();
  }
}

std::unique_ptr<ForAssignStmt> Parser::parseForAssignStmt() {
  auto forAssignStmt = std::make_unique<ForAssignStmt>();
  auto lval = parseLVal();
  if (current.type != TokenType::ASSIGN) {
    // error();
  }
  advance();
  auto exp = parseExp();
  forAssignStmt->assignments.push_back({std::move(lval), std::move(exp)});
  while (current.type == TokenType::COMMA) {
    advance();
    auto lval = parseLVal();
    if (current.type != TokenType::ASSIGN) {
      // error();
    }
    advance();
    auto exp = parseExp();
    forAssignStmt->assignments.push_back({std::move(lval), std::move(exp)});
  }
  std::cout << "<ForStmt>" << std::endl;
  return forAssignStmt;
}

std::unique_ptr<Exp> Parser::parseExp() {
  auto exp = std::make_unique<Exp>();
  exp->addExp = parseAddExp();
  std::cout << "<Exp>" << std::endl;
  return exp;
}

std::unique_ptr<Cond> Parser::parseCond() {
  auto cond = std::make_unique<Cond>();
  cond->lOrExp = parseLOrExp();
  std::cout << "<Cond>" << std::endl;
  return cond;
}

std::unique_ptr<LVal> Parser::parseLVal() {
  auto lVal = std::make_unique<LVal>();
  lVal->ident = current.lexeme;
  advance(); // eat ident
  if (current.type == TokenType::LBRACK) {
    advance(); // eat lbrack
    lVal->arrayIndex = parseExp();
    if (current.type != TokenType::RBRACK) {
      error(current.line, "k");
    }
    advance(); // eat rbrack
  } else {
    lVal->arrayIndex = nullptr;
  }
  std::cout << "<LVal>" << std::endl;
  return lVal;
}

std::unique_ptr<PrimaryExp> Parser::parsePrimaryExp() {
  auto primaryExp = std::make_unique<PrimaryExp>();
  if (current.type == TokenType::LPARENT) {
    primaryExp->primaryType = PrimaryExp::PrimaryType::EXP;
    advance();
    primaryExp->exp = parseExp();
    if (current.type != TokenType::RPARENT) {
      error(current.line, "j");
    }
    advance();
  } else if (current.type == TokenType::IDENFR) {
    primaryExp->primaryType = PrimaryExp::PrimaryType::LVAL;
    primaryExp->lval = parseLVal();
  } else if (current.type == TokenType::INTCON) {
    primaryExp->primaryType = PrimaryExp::PrimaryType::NUMBER;
    primaryExp->number = parseNumber();
  } else {
    // error();
  }
  std::cout << "<PrimaryExp>" << std::endl;
  return primaryExp;
}

std::unique_ptr<Number> Parser::parseNumber() {
  auto number = std::make_unique<Number>();
  if (current.type == TokenType::INTCON) {
    number->value = std::get<int>(current.value);
  }
  advance();
  std::cout << "<Number>" << std::endl;
  return number;
}

std::unique_ptr<UnaryExp> Parser::parseUnaryExp() {
  auto unaryExp = std::make_unique<UnaryExp>();
  if (current.type == TokenType::IDENFR) {
    auto temp = lexer;
    auto tempCurrent = current;
    advance(); // eat ident
    if (current.type == TokenType::LPARENT) {
      unaryExp->unaryType = UnaryExp::UnaryType::FUNC_CALL;
      unaryExp->funcIdent = tempCurrent.lexeme;
      advance(); // eat lparent
      if (current.type != TokenType::RPARENT) {
        unaryExp->funcRParams = parseFuncRParams();
        if (current.type != TokenType::RPARENT) {
          error(current.line, "j");
        }
      }
      advance(); // eat rparent
      std::cout << "<UnaryExp>" << std::endl;
      return unaryExp;
    } else {
      lexer = temp;
      current = tempCurrent;
      unaryExp->unaryType = UnaryExp::UnaryType::PRIMARY;
      unaryExp->primaryExp = parsePrimaryExp();
      std::cout << "<UnaryExp>" << std::endl;
      return unaryExp;
    }
  } else if (current.type == TokenType::PLUS ||
             current.type == TokenType::MINU ||
             current.type == TokenType::NOT) {
    unaryExp->unaryType = UnaryExp::UnaryType::UNARY_OP;
    unaryExp->unaryOp = parseUnaryOp();
    unaryExp->unaryExp = parseUnaryExp();
    std::cout << "<UnaryExp>" << std::endl;
    return unaryExp;
  } else {
    unaryExp->unaryType = UnaryExp::UnaryType::PRIMARY;
    unaryExp->primaryExp = parsePrimaryExp();
    std::cout << "<UnaryExp>" << std::endl;
    return unaryExp;
  }
}

std::unique_ptr<UnaryOp> Parser::parseUnaryOp() {
  auto unaryOp = std::make_unique<UnaryOp>();
  if (current.type == TokenType::PLUS) {
    unaryOp->op = UnaryOp::OpType::PLUS;
  } else if (current.type == TokenType::MINU) {
    unaryOp->op = UnaryOp::OpType::MINUS;
  } else if (current.type == TokenType::NOT) {
    unaryOp->op = UnaryOp::OpType::NOT;
  } else {
    // error();
  }
  advance();
  std::cout << "<UnaryOp>" << std::endl;
  return unaryOp;
}

std::unique_ptr<FuncRParams> Parser::parseFuncRParams() {
  auto funcRParams = std::make_unique<FuncRParams>();
  funcRParams->exps.push_back(parseExp());
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    funcRParams->exps.push_back(parseExp());
  }
  std::cout << "<FuncRParams>" << std::endl;
  return funcRParams;
}

std::unique_ptr<MulExp> Parser::parseMulExp() {
  auto mulExp = std::make_unique<MulExp>();

  mulExp->unaryExp = parseUnaryExp();
  mulExp->op = MulExp::OpType::NONE;

  while (current.type == TokenType::MULT || current.type == TokenType::DIV ||
         current.type == TokenType::MOD) {
    auto newLeft = std::make_unique<MulExp>();
    newLeft->left = std::move(mulExp);
    if (current.type == TokenType::MULT) {
      newLeft->op = MulExp::OpType::MULT;
    } else if (current.type == TokenType::DIV) {
      newLeft->op = MulExp::OpType::DIV;
    } else if (current.type == TokenType::MOD) {
      newLeft->op = MulExp::OpType::MOD;
    }
    advance();
    newLeft->unaryExp = parseUnaryExp();
    mulExp = std::move(newLeft);
  }
  std::cout << "<MulExp>" << std::endl;
  return mulExp;
}

std::unique_ptr<AddExp> Parser::parseAddExp() {
  auto addExp = std::make_unique<AddExp>();
  addExp->mulExp = parseMulExp();
  addExp->op = AddExp::OpType::NONE;
  while (current.type == TokenType::PLUS || current.type == TokenType::MINU) {
    auto newLeft = std::make_unique<AddExp>();
    newLeft->left = std::move(addExp);
    if (current.type == TokenType::PLUS) {
      newLeft->op = AddExp::OpType::PLUS;
    } else if (current.type == TokenType::MINU) {
      newLeft->op = AddExp::OpType::MINU;
    }
    advance();
    newLeft->mulExp = parseMulExp();
    addExp = std::move(newLeft);
  }
  std::cout << "<AddExp>" << std::endl;
  return addExp;
}

std::unique_ptr<RelExp> Parser::parseRelExp() {
  auto relExp = std::make_unique<RelExp>();
  relExp->addExp = parseAddExp();
  relExp->op = RelExp::OpType::NONE;
  while (current.type == TokenType::LSS || current.type == TokenType::LEQ ||
         current.type == TokenType::GRE || current.type == TokenType::GEQ) {
    auto newLeft = std::make_unique<RelExp>();
    newLeft->left = std::move(relExp);
    if (current.type == TokenType::LSS) {
      newLeft->op = RelExp::OpType::LSS;
    } else if (current.type == TokenType::LEQ) {
      newLeft->op = RelExp::OpType::LEQ;
    } else if (current.type == TokenType::GRE) {
      newLeft->op = RelExp::OpType::GRE;
    } else if (current.type == TokenType::GEQ) {
      newLeft->op = RelExp::OpType::GEQ;
    }
    advance();
    newLeft->addExp = parseAddExp();
    relExp = std::move(newLeft);
  }
  std::cout << "<RelExp>" << std::endl;
  return relExp;
}

std::unique_ptr<EqExp> Parser::parseEqExp() {
  auto eqExp = std::make_unique<EqExp>();
  eqExp->relExp = parseRelExp();
  eqExp->op = EqExp::OpType::NONE;
  while (current.type == TokenType::EQL || current.type == TokenType::NEQ) {
    auto newLeft = std::make_unique<EqExp>();
    newLeft->left = std::move(eqExp);
    if (current.type == TokenType::EQL) {
      newLeft->op = EqExp::OpType::EQL;
    } else if (current.type == TokenType::NEQ) {
      newLeft->op = EqExp::OpType::NEQ;
    }
    advance();
    newLeft->relExp = parseRelExp();
    eqExp = std::move(newLeft);
  }
  std::cout << "<EqExp>" << std::endl;
  return eqExp;
}

std::unique_ptr<LAndExp> Parser::parseLAndExp() {
  auto lAndExp = std::make_unique<LAndExp>();
  lAndExp->eqExp = parseEqExp();
  while (current.type == TokenType::AND) {
    auto newLeft = std::make_unique<LAndExp>();
    newLeft->left = std::move(lAndExp);
    advance();
    newLeft->eqExp = parseEqExp();
    lAndExp = std::move(newLeft);
  }
  std::cout << "<LAndExp>" << std::endl;
  return lAndExp;
}

std::unique_ptr<LOrExp> Parser::parseLOrExp() {
  auto lOrExp = std::make_unique<LOrExp>();
  lOrExp->lAndExp = parseLAndExp();
  while (current.type == TokenType::OR) {
    auto newLeft = std::make_unique<LOrExp>();
    newLeft->left = std::move(lOrExp);
    advance();
    newLeft->lAndExp = parseLAndExp();
    lOrExp = std::move(newLeft);
  }
  std::cout << "<LOrExp>" << std::endl;
  return lOrExp;
}

std::unique_ptr<ConstExp> Parser::parseConstExp() {
  auto constExp = std::make_unique<ConstExp>();
  constExp->addExp = parseAddExp();
  std::cout << "<ConstExp>" << std::endl;
  return constExp;
}
