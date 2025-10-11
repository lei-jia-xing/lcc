#include "parser/Parser.hpp"
#include "lexer/Token.hpp"
#include "parser/AST.hpp"
#include <iostream>
#include <memory>

Parser::Parser(Lexer &&lexer, Token current)
    : lexer(std::move(lexer)), current(current) {}
void Parser::silentPV(bool silent) {
  if (silent) {
    silentDepth++;
    lexer.silentPV(true);
  } else {
    silentDepth--;
    lexer.silentPV(false);
  }
}
void Parser::output(const std::string &type) {
  if (silentDepth == 0) {
    std::cout << type << std::endl;
  }
}
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
    auto temp = lexer;
    auto currentTemp = current;
    if (current.type == TokenType::INTTK) {
      // look ahead
      silentPV(true);
      advance(); // eat int
      silentPV(false);
      if (current.type == TokenType::MAINTK) {
        lexer = std::move(temp);
        current = currentTemp;
        compUnit->mainFuncDef = parseMainFuncDef();
        output("<CompUnit>");
        return compUnit;
      } else if (current.type == TokenType::IDENFR) {
        lexer = std::move(temp);
        current = currentTemp;
        break;
      } else {
        lexer = std::move(temp);
        current = currentTemp;
      }
    }
    compUnit->decls.push_back(parseDecl());
  }
  while (current.type == TokenType::VOIDTK ||
         current.type == TokenType::INTTK) {
    auto temp = lexer;
    auto currentTemp = current;
    if (current.type == TokenType::INTTK) {
      // look ahead
      silentPV(true);
      advance(); // eat int
      silentPV(false);
      if (current.type == TokenType::MAINTK) {
        lexer = std::move(temp);
        current = currentTemp;
        compUnit->mainFuncDef = parseMainFuncDef();
        output("<CompUnit>");
        return compUnit;
      } else {
        lexer = std::move(temp);
        current = currentTemp;
      }
    }
    compUnit->funcDefs.push_back(parseFuncDef());
  }
  return compUnit;
}

std::unique_ptr<Decl> Parser::parseDecl() {
  auto decl = std::make_unique<Decl>();
  if (current.type == TokenType::CONSTTK) {
    decl = parseConstDecl();
  } else {
    decl = parseVarDecl();
  }
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
  output("<ConstDecl>");
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
  output("<VarDecl>");
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
    output("<ConstDef>");
    return constDef;

  } else if (current.type == TokenType::ASSIGN) {
    advance(); // eat assign
    constDef->constinitVal = parseConstInitVal();
    output("<ConstDef>");
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

  output("<VarDef>");
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
  output("<ConstInitVal>");
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
  output("<InitVal>");
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
  output("<FuncDef>");
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
  output("<MainFuncDef>");
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
  output("<FuncType>");
  return funcType;
}

std::unique_ptr<FuncFParams> Parser::parseFuncFParams() {
  auto funcFParams = std::make_unique<FuncFParams>();
  funcFParams->params.push_back(parseFuncFParam());
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    funcFParams->params.push_back(parseFuncFParam());
  }
  output("<FuncFParams>");
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
  output("<FuncFParam>");
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
  output("<Block>");
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
  output("<Stmt>");
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
  output("<Stmt>");
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
  output("<Stmt>");
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
  output("<Stmt>");
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
  output("<Stmt>");
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
  output("<Stmt>");
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
  output("<Stmt>");
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
  output("<Stmt>");
  return expStmt;
}
std::unique_ptr<BlockStmt> Parser::parseBlockStmt() {
  auto blockStmt = std::make_unique<BlockStmt>();
  blockStmt->block = parseBlock();
  output("<Stmt>");
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
    // look ahead
    auto temp = lexer;
    auto tempCurrent = current;
    silentPV(true);
    parseLVal();
    if (current.type == TokenType::ASSIGN) {
      lexer = temp;
      current = tempCurrent;
      silentPV(false);
      return parseAssignStmt();
    } else {
      lexer = temp;
      current = tempCurrent;
      silentPV(false);
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
  output("<ForAssignStmt>");
  return forAssignStmt;
}

std::unique_ptr<Exp> Parser::parseExp() {
  auto exp = std::make_unique<Exp>();
  exp->addExp = parseAddExp();
  output("<Exp>");
  return exp;
}

std::unique_ptr<Cond> Parser::parseCond() {
  auto cond = std::make_unique<Cond>();
  cond->lOrExp = parseLOrExp();
  output("<Cond>");
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
  output("<LVal>");
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
  output("<PrimaryExp>");
  return primaryExp;
}

std::unique_ptr<Number> Parser::parseNumber() {
  auto number = std::make_unique<Number>();
  if (current.type == TokenType::INTCON) {
    number->value = std::get<int>(current.value);
  }
  advance();
  output("<Number>");
  return number;
}

std::unique_ptr<UnaryExp> Parser::parseUnaryExp() {
  auto unaryExp = std::make_unique<UnaryExp>();
  if (current.type == TokenType::PLUS || current.type == TokenType::MINU ||
      current.type == TokenType::NOT) {
    // UnaryExp -> UnaryOp UnaryExp
    unaryExp->unaryOp = parseUnaryOp();
    unaryExp->unaryExp = parseUnaryExp();
    unaryExp->unaryType = UnaryExp::UnaryType::UNARY_OP;
  } else if (current.type == TokenType::IDENFR) {
    auto temp = lexer;
    auto tempCurrent = current;
    silentPV(true);
    advance(); // eat ident
    if (current.type == TokenType::LPARENT) {
      // UnaryExp -> Ident '(' [FuncRParams] ')'
      silentPV(false);
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
    } else {
      // UnaryExp -> PrimaryExp
      lexer = temp;
      current = tempCurrent;
      silentPV(false);
      unaryExp->unaryType = UnaryExp::UnaryType::PRIMARY;
      unaryExp->primaryExp = parsePrimaryExp();
    }
  } else {
    // UnaryExp -> PrimaryExp
    unaryExp->unaryType = UnaryExp::UnaryType::PRIMARY;
    unaryExp->primaryExp = parsePrimaryExp();
  }
  output("<UnaryExp>");
  return unaryExp;
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
  output("<UnaryOp>");
  return unaryOp;
}

std::unique_ptr<FuncRParams> Parser::parseFuncRParams() {
  auto funcRParams = std::make_unique<FuncRParams>();
  funcRParams->exps.push_back(parseExp());
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    funcRParams->exps.push_back(parseExp());
  }
  output("<FuncRParams>");
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
  output("<MulExp>");
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
  output("<AddExp>");
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
  output("<RelExp>");
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
  output("<EqExp>");
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
  output("<LAndExp>");
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
  output("<LOrExp>");
  return lOrExp;
}

std::unique_ptr<ConstExp> Parser::parseConstExp() {
  auto constExp = std::make_unique<ConstExp>();
  constExp->addExp = parseAddExp();
  output("<ConstExp>");
  return constExp;
}
