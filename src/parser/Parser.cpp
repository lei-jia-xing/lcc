#include "parser/Parser.hpp"
#include "lexer/Token.hpp"
#include "parser/AST.hpp"
#include <iostream>
#include <memory>

Parser::Parser(Lexer &&lexer, Token current)
    : lexer(std::move(lexer)), current(current), lastVnline(0) {}
void Parser::silentPV(bool silent) {
  if (silent) {
    silentDepth++;
    lexer.silentPV(true);
  } else {
    silentDepth > 0 ? silentDepth-- : silentDepth = 0;
    lexer.silentPV(false);
  }
}
void Parser::output(const std::string &type) {
  if (silentDepth == 0) {
    std::cout << type << std::endl;
  }
}
void Parser::advance() { current = lexer.nextToken(); }
bool Parser::expect(const std::vector<TokenType> &types,
                    const std::string &errorType) {
  bool matched = false;
  for (auto t : types) {
    if (lexer.peekToken(1).type == t) {
      matched = true;
      break;
    }
  }
  if (!matched) {
    error(lastVnline, errorType);
    return false;
  } else {
    advance();
    return true;
  }
}
void Parser::sync(const std::vector<TokenType> &types) {
  silentPV(true);

  while (lexer.peekToken(1).type != TokenType::EOFTK) {
    bool matched = false;
    for (auto t : types) {
      if (lexer.peekToken(1).type == t) {
        matched = true;
        break;
      }
    }
    if (matched) {
      silentPV(false);
      return;
    }
    advance();
  }
  silentPV(false);
}

void Parser::syncIfCond() {
  silentPV(true);
  while (lexer.peekToken(2).type != TokenType::EOFTK) {
    auto t = lexer.peekToken(2).type;
    if (t == TokenType::ELSETK || Stmt::FIRST.count(t)) {
      silentPV(false);
      return;
    }
    advance();
  }
  silentPV(false);
}
void Parser::error(const int &line, const std::string errorType) {
  std::cerr << line << " " << errorType << std::endl;
}
std::unique_ptr<CompUnit> Parser::parseCompUnit() {
  auto compUnit = std::make_unique<CompUnit>();
  compUnit->line = current.line;
  lastVnline = current.line;
  while (current.type == TokenType::CONSTTK ||
         current.type == TokenType::INTTK ||
         current.type == TokenType::STATICTK) {
    if (current.type == TokenType::INTTK) {
      // look ahead
      if (lexer.peekToken(1).type == TokenType::MAINTK) {
        compUnit->mainFuncDef = std::move(parseMainFuncDef());
        output("<CompUnit>");
        return compUnit;
      } else if (lexer.peekToken(1).type == TokenType::IDENFR &&
                 lexer.peekToken(2).type == TokenType::LPARENT) {
        break;
      }
    }
    compUnit->decls.push_back(parseDecl());
    advance();
  }
  while (current.type == TokenType::VOIDTK ||
         current.type == TokenType::INTTK) {
    if (current.type == TokenType::INTTK) {
      // look ahead
      if (lexer.peekToken(1).type == TokenType::MAINTK) {
        compUnit->mainFuncDef = parseMainFuncDef();
        output("<CompUnit>");
        return compUnit;
      }
    }
    compUnit->funcDefs.push_back(parseFuncDef());
    advance();
  }
  output("<CompUnit>");
  return compUnit;
}

std::unique_ptr<Decl> Parser::parseDecl() {
  auto decl = std::make_unique<Decl>();
  decl->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::CONSTTK) {
    decl = parseConstDecl();
  } else {
    decl = parseVarDecl();
  }
  return decl;
}

std::unique_ptr<ConstDecl> Parser::parseConstDecl() {
  auto constDecl = std::make_unique<ConstDecl>();
  constDecl->line = current.line;
  lastVnline = current.line;
  advance(); // eat const
  constDecl->bType = parseBType();
  advance();
  constDecl->constDefs.push_back(parseConstDef());
  expect({TokenType::COMMA, TokenType::SEMICN}, "i");
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    constDecl->constDefs.push_back(parseConstDef());
    expect({TokenType::COMMA, TokenType::SEMICN}, "i");
  }
  output("<ConstDecl>");
  return constDecl;
}

std::unique_ptr<VarDecl> Parser::parseVarDecl() {
  auto varDecl = std::make_unique<VarDecl>();
  varDecl->line = current.line;
  lastVnline = current.line;
  varDecl->isStatic = false;
  if (current.type == TokenType::STATICTK) {
    varDecl->isStatic = true;
    advance(); // eat static
  }
  varDecl->bType = parseBType();
  advance();
  varDecl->varDefs.push_back(parseVarDef());
  expect({TokenType::COMMA, TokenType::SEMICN}, "i");
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    varDecl->varDefs.push_back(parseVarDef());
    expect({TokenType::COMMA, TokenType::SEMICN}, "i");
  }
  output("<VarDecl>");
  return varDecl;
}

std::unique_ptr<BType> Parser::parseBType() {
  auto bType = std::make_unique<BType>();
  bType->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::INTTK) {
    bType->type = "int";
  } else {
    // error();
  }
  return bType;
}

std::unique_ptr<ConstDef> Parser::parseConstDef() {
  auto constDef = std::make_unique<ConstDef>();
  constDef->line = current.line;
  lastVnline = current.line;
  constDef->ident = current.lexeme;
  advance(); // eat ident
  if (current.type == TokenType::LBRACK) {
    advance(); // eat lbrack
    constDef->arraySize = parseConstExp();
    if (constDef->arraySize == nullptr) {
      sync({TokenType::RBRACK});
    }
    expect({TokenType::RBRACK}, "k");
    advance(); // eat rbrack
    if (current.type != TokenType::ASSIGN) {
      // error();
    }
    advance(); // eat assign
    auto constInitVal = parseConstInitVal();
    if (constInitVal == nullptr) {
      sync({TokenType::COMMA, TokenType::SEMICN});
    } else {
      constDef->constinitVal = std::move(constInitVal);
    }
    output("<ConstDef>");
    return constDef;

  } else if (current.type == TokenType::ASSIGN) {
    advance(); // eat assign
    auto constInitVal = parseConstInitVal();
    if (constInitVal == nullptr) {
      sync({TokenType::COMMA, TokenType::SEMICN});
    } else {
      constDef->constinitVal = std::move(constInitVal);
    }
    output("<ConstDef>");
    return constDef;
  } else {
    // error();
    output("<ConstDef>");
    return constDef;
  }
}

std::unique_ptr<VarDef> Parser::parseVarDef() {
  auto varDef = std::make_unique<VarDef>();
  varDef->line = current.line;
  lastVnline = current.line;
  varDef->ident = current.lexeme;
  if (lexer.peekToken(1).type == TokenType::LBRACK) {
    advance(); // eat ident
    advance(); // eat lbrack
    varDef->arraySize = parseConstExp();
    if (varDef->arraySize == nullptr) {
      sync({TokenType::RBRACK});
    }
    expect({TokenType::RBRACK}, "k");
    varDef->initVal = nullptr;
    if (lexer.peekToken(1).type == TokenType::ASSIGN) {
      advance(); // eat rbrack
      advance(); // eat assign
      auto initVal = parseInitVal();
      if (initVal == nullptr) {
        sync({TokenType::COMMA, TokenType::SEMICN});
      } else {
        varDef->initVal = std::move(initVal);
      }
    }
    output("<VarDef>");
    return varDef;
  } else if (lexer.peekToken(1).type == TokenType::ASSIGN) {
    advance(); // eat ident
    advance(); // eat assign
    auto initVal = parseInitVal();
    if (initVal == nullptr) {
      sync({TokenType::COMMA, TokenType::SEMICN});
    } else {
      varDef->initVal = std::move(initVal);
    }
    output("<VarDef>");
    return varDef;
  } else {
    output("<VarDef>");
    return varDef;
  }
}

std::unique_ptr<ConstInitVal> Parser::parseConstInitVal() {
  auto constInitVal = std::make_unique<ConstInitVal>();
  constInitVal->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::LBRACE) {
    constInitVal->isArray = true;
    advance(); // eat lbrace
    if (current.type != TokenType::RBRACE) {
      auto constExp = parseConstExp();
      if (constExp == nullptr) {
        sync({TokenType::COMMA, TokenType::RBRACE});
      } else {
        constInitVal->arrayExps.push_back(std::move(constExp));
      }
      advance();
      while (current.type == TokenType::COMMA) {
        advance(); // eat comma
        auto constExp = parseConstExp();
        if (constExp == nullptr) {
          sync({TokenType::COMMA, TokenType::RBRACE});
        } else {
          constInitVal->arrayExps.push_back(std::move(constExp));
        }
        advance();
      }
    }
    if (current.type != TokenType::RBRACE) {
      // error();
    }
  } else {
    auto constExp = parseConstExp();
    if (constExp == nullptr) {
      return nullptr;
    } else {
      constInitVal->exp = std::move(constExp);
    }
  }
  output("<ConstInitVal>");
  return constInitVal;
}

std::unique_ptr<InitVal> Parser::parseInitVal() {
  auto initVal = std::make_unique<InitVal>();
  initVal->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::LBRACE) {
    initVal->isArray = true;
    advance(); // eat lbrace
    if (current.type != TokenType::RBRACE) {
      auto exp = parseExp();
      if (exp == nullptr) {
        sync({TokenType::COMMA, TokenType::RBRACE});
      } else {
        initVal->arrayExps.push_back(std::move(exp));
      }
      advance();
      while (current.type == TokenType::COMMA) {
        advance(); // eat comma
        auto exp = parseExp();
        if (exp == nullptr) {
          sync({TokenType::COMMA, TokenType::RBRACE});
        } else {
          initVal->arrayExps.push_back(std::move(exp));
        }
        advance();
      }
    }
    if (current.type != TokenType::RBRACE) {
      // error();
    }
  } else {
    auto exp = parseExp();
    if (exp == nullptr) {
      return nullptr;
    } else {
      initVal->exp = std::move(exp);
    }
  }
  output("<InitVal>");
  return initVal;
}

std::unique_ptr<FuncDef> Parser::parseFuncDef() {
  auto funcDef = std::make_unique<FuncDef>();
  funcDef->line = current.line;
  lastVnline = current.line;
  funcDef->funcType = parseFuncType();
  advance();
  funcDef->ident = current.lexeme;
  advance(); // eat ident
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  if (lexer.peekToken(1).type == TokenType::INTTK) {
    advance();
    funcDef->params = parseFuncFParams();
  }
  expect({TokenType::RPARENT}, "j");
  advance();
  funcDef->block = parseBlock();
  output("<FuncDef>");
  return funcDef;
}

std::unique_ptr<MainFuncDef> Parser::parseMainFuncDef() {
  auto mainFuncDef = std::make_unique<MainFuncDef>();
  mainFuncDef->line = current.line;
  lastVnline = current.line;
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
  expect({TokenType::RPARENT}, "j");
  advance(); // eat rparent
  mainFuncDef->block = parseBlock();
  output("<MainFuncDef>");
  return mainFuncDef;
}

std::unique_ptr<FuncType> Parser::parseFuncType() {
  auto funcType = std::make_unique<FuncType>();
  funcType->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::VOIDTK) {
    funcType->type = "void";
    funcType->line = current.line;
  } else if (current.type == TokenType::INTTK) {
    funcType->type = "int";
    funcType->line = current.line;
  } else {
    // error();
  }
  output("<FuncType>");
  return funcType;
}

std::unique_ptr<FuncFParams> Parser::parseFuncFParams() {
  auto funcFParams = std::make_unique<FuncFParams>();
  funcFParams->line = current.line;
  lastVnline = current.line;
  funcFParams->params.push_back(parseFuncFParam());
  if (lexer.peekToken(1).type == TokenType::COMMA) {
    advance();
  }
  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    funcFParams->params.push_back(parseFuncFParam());
    if (lexer.peekToken(1).type == TokenType::COMMA) {
      advance();
    }
  }
  output("<FuncFParams>");
  return funcFParams;
}

std::unique_ptr<FuncFParam> Parser::parseFuncFParam() {
  auto funcFParam = std::make_unique<FuncFParam>();
  funcFParam->line = current.line;
  lastVnline = current.line;
  funcFParam->bType = parseBType();
  advance();
  funcFParam->ident = current.lexeme;
  // look ahead
  if (lexer.peekToken(1).type == TokenType::LBRACK) {
    advance(); // eat ident
    funcFParam->isArray = true;
    expect({TokenType::RBRACK}, "k");
  }
  output("<FuncFParam>");
  return funcFParam;
}

std::unique_ptr<Block> Parser::parseBlock() {
  auto block = std::make_unique<Block>();
  block->line = current.line;
  lastVnline = current.line;
  if (current.type != TokenType::LBRACE) {
    // error();
  }
  advance(); // eat lbrace
  if (current.type != TokenType::RBRACE) {
    block->items.push_back(parseBlockItem());
    advance();
    while (current.type != TokenType::RBRACE) {
      block->items.push_back(parseBlockItem());
      advance();
    }
  }
  if (current.type != TokenType::RBRACE) {
    // error();
  }
  output("<Block>");
  return block;
}

std::unique_ptr<BlockItem> Parser::parseBlockItem() {
  auto blockItem = std::make_unique<BlockItem>();
  blockItem->line = current.line;
  lastVnline = current.line;
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
  ifStmt->line = current.line;
  lastVnline = current.line;
  if (current.type != TokenType::IFTK) {
    // error();
  }
  advance(); // eat if
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  advance(); // eat lparent
  ifStmt->cond = parseCond();
  if (ifStmt->cond == nullptr) {
    syncIfCond();
  }
  bool ateRParen = false;
  if (ifStmt->cond != nullptr) {
    expect({TokenType::RPARENT}, "j");
    advance(); // eat rparent
    ateRParen = true;
  } else if (lexer.peekToken(1).type == TokenType::RPARENT) {
    expect({TokenType::RPARENT}, "j");
    advance(); // eat rparent
    ateRParen = true;
  } else {
    // missing ')', do not advance here; proceed based on next token
  }

  ifStmt->elseStmt = nullptr;

  // If next token is 'else', treat then-statement as missing and parse
  // else-part
  if (current.type == TokenType::ELSETK) {
    advance(); // eat else
    ifStmt->elseStmt = parseStmt();
    output("<Stmt>");
    return ifStmt;
  }

  // Otherwise parse then-statement starting at current
  ifStmt->thenStmt = parseStmt();

  // Handle optional else; cover both lookahead and current cases
  if (lexer.peekToken(1).type == TokenType::ELSETK) {
    advance();
    advance(); // eat else
    ifStmt->elseStmt = parseStmt();
  } else if (current.type == TokenType::ELSETK) {
    advance(); // eat else
    ifStmt->elseStmt = parseStmt();
  }

  output("<Stmt>");
  return ifStmt;
}
std::unique_ptr<ForStmt> Parser::parseForStmt() {
  auto forStmt = std::make_unique<ForStmt>();
  forStmt->line = current.line;
  lastVnline = current.line;
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
    if (forStmt->initStmt == nullptr) {
      sync({TokenType::SEMICN});
    }
    advance();
  }
  advance(); // eat semicn
  forStmt->cond = nullptr;
  if (current.type != TokenType::SEMICN) {
    forStmt->cond = parseCond();
    if (forStmt->cond == nullptr) {
      sync({TokenType::SEMICN});
    }
    advance();
  }
  advance(); // eat semicn
  forStmt->updateStmt = nullptr;
  if (current.type != TokenType::RPARENT) {
    forStmt->updateStmt = parseForAssignStmt();
    if (forStmt->updateStmt == nullptr) {
      sync({TokenType::RPARENT});
    }
    advance();
  }
  advance(); // eat rparent
  forStmt->bodyStmt = parseStmt();
  output("<Stmt>");
  return forStmt;
}
std::unique_ptr<BreakStmt> Parser::parseBreakStmt() {
  auto breakStmt = std::make_unique<BreakStmt>();
  breakStmt->line = current.line;
  lastVnline = current.line;
  if (current.type != TokenType::BREAKTK) {
    // error();
  }
  expect({TokenType::SEMICN}, "i");
  output("<Stmt>");
  return breakStmt;
}
std::unique_ptr<ContinueStmt> Parser::parseContinueStmt() {
  auto continueStmt = std::make_unique<ContinueStmt>();
  continueStmt->line = current.line;
  lastVnline = current.line;
  if (current.type != TokenType::CONTINUETK) {
    // error();
  }
  expect({TokenType::SEMICN}, "i");
  output("<Stmt>");
  return continueStmt;
}
std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
  auto returnStmt = std::make_unique<ReturnStmt>();
  returnStmt->line = current.line;
  lastVnline = current.line;
  if (current.type != TokenType::RETURNTK) {
    // error();
  }
  advance();
  returnStmt->exp = nullptr;
  if (current.type != TokenType::SEMICN) {
    auto parsedExp = parseExp();
    if (parsedExp == nullptr) {
      sync({TokenType::SEMICN});
    } else {
      returnStmt->exp = std::move(parsedExp);
    }
    expect({TokenType::SEMICN}, "i");
  }
  output("<Stmt>");
  return returnStmt;
}
std::unique_ptr<PrintfStmt> Parser::parsePrintfStmt() {
  auto printfStmt = std::make_unique<PrintfStmt>();
  printfStmt->line = current.line;
  lastVnline = current.line;
  if (current.type != TokenType::PRINTFTK) {
    // error();
  }
  advance();
  if (current.type != TokenType::LPARENT) {
    // error();
  }
  advance();
  if (current.type != TokenType::STRCON) {
    // error();
  }
  printfStmt->formatString = current.lexeme;
  expect({TokenType::RPARENT, TokenType::COMMA}, "j");
  while (current.type == TokenType::COMMA) {
    advance();
    auto arg = parseExp();
    if (arg == nullptr) {
      sync({TokenType::RPARENT, TokenType::COMMA});
    } else {
      printfStmt->args.push_back(std::move(arg));
    }
    expect({TokenType::RPARENT, TokenType::COMMA}, "j");
  }
  expect({TokenType::SEMICN}, "i");
  output("<Stmt>");
  return printfStmt;
}

std::unique_ptr<AssignStmt> Parser::parseAssignStmt() {
  auto assignStmt = std::make_unique<AssignStmt>();
  assignStmt->line = current.line;
  lastVnline = current.line;
  assignStmt->lval = parseLVal();
  advance();
  if (current.type != TokenType::ASSIGN) {
    // error();
  }
  advance();
  auto parsedExp = parseExp();
  if (parsedExp == nullptr) {
    sync({TokenType::SEMICN});
  } else {
    assignStmt->exp = std::move(parsedExp);
  }
  expect({TokenType::SEMICN}, "i");
  output("<Stmt>");
  return assignStmt;
}

std::unique_ptr<ExpStmt> Parser::parseExpStmt() {
  auto expStmt = std::make_unique<ExpStmt>();
  expStmt->line = current.line;
  lastVnline = current.line;
  if (current.type != TokenType::SEMICN) {
    auto parsedExp = parseExp();
    if (parsedExp == nullptr) {
      sync({TokenType::SEMICN});
    } else {
      expStmt->exp = std::move(parsedExp);
    }
    expect({TokenType::SEMICN}, "i");
  }
  output("<Stmt>");
  return expStmt;
}

std::unique_ptr<BlockStmt> Parser::parseBlockStmt() {
  auto blockStmt = std::make_unique<BlockStmt>();
  blockStmt->line = current.line;
  lastVnline = current.line;
  blockStmt->block = parseBlock();
  output("<Stmt>");
  return blockStmt;
}

std::unique_ptr<Stmt> Parser::parseStmt() {

  if (current.type == TokenType::IFTK) {
    return parseIfStmt();
  } else if (current.type == TokenType::FORTK) {
    return parseForStmt();
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
    advance();
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
  forAssignStmt->line = current.line;
  lastVnline = current.line;
  auto lval = parseLVal();
  advance();
  if (current.type != TokenType::ASSIGN) {
    // error();
  }
  advance();
  auto exp = parseExp();
  if (exp == nullptr) {
    sync({TokenType::COMMA, TokenType::SEMICN, TokenType::RPARENT});
  } else {
    forAssignStmt->assignments.push_back({std::move(lval), std::move(exp)});
  }
  if (lexer.peekToken(1).type == TokenType::COMMA) {
    advance();
  }
  while (current.type == TokenType::COMMA) {
    advance();
    auto lval = parseLVal();
    advance();
    if (current.type != TokenType::ASSIGN) {
      // error();
    }
    advance();
    auto exp = parseExp();
    if (exp == nullptr) {
      sync({TokenType::COMMA, TokenType::SEMICN, TokenType::RPARENT});
    } else {
      forAssignStmt->assignments.push_back({std::move(lval), std::move(exp)});
    }
    if (lexer.peekToken(1).type == TokenType::COMMA) {
      advance();
    }
  }
  output("<ForStmt>");
  return forAssignStmt;
}

std::unique_ptr<Exp> Parser::parseExp() {
  auto exp = std::make_unique<Exp>();
  exp->line = current.line;
  lastVnline = current.line;
  auto add = parseAddExp();
  if (add == nullptr) {
    return nullptr;
  }
  exp->addExp = std::move(add);
  output("<Exp>");
  return exp;
}

std::unique_ptr<Cond> Parser::parseCond() {
  auto cond = std::make_unique<Cond>();
  cond->line = current.line;
  lastVnline = current.line;
  auto LOrExp = parseLOrExp();
  if (LOrExp == nullptr) {
    return nullptr;
  }
  cond->lOrExp = std::move(LOrExp);
  output("<Cond>");
  return cond;
}

std::unique_ptr<LVal> Parser::parseLVal() {
  auto lVal = std::make_unique<LVal>();
  lVal->line = current.line;
  lastVnline = current.line;
  lVal->ident = current.lexeme;
  lVal->arrayIndex = nullptr;
  if (lexer.peekToken(1).type == TokenType::LBRACK) {
    advance(); // eat ident
    advance(); // eat lbrack
    lVal->arrayIndex = parseExp();
    expect({TokenType::RBRACK}, "k");
  }
  output("<LVal>");
  return lVal;
}

std::unique_ptr<PrimaryExp> Parser::parsePrimaryExp() {
  auto primaryExp = std::make_unique<PrimaryExp>();
  primaryExp->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::LPARENT) {
    primaryExp->primaryType = PrimaryExp::PrimaryType::EXP;
    advance();
    primaryExp->exp = parseExp();
    expect({TokenType::RPARENT}, "j");
  } else if (current.type == TokenType::IDENFR) {
    primaryExp->primaryType = PrimaryExp::PrimaryType::LVAL;
    primaryExp->lval = parseLVal();
  } else if (current.type == TokenType::INTCON) {
    primaryExp->primaryType = PrimaryExp::PrimaryType::NUMBER;
    primaryExp->number = parseNumber();
  } else {
    return nullptr;
  }
  output("<PrimaryExp>");
  return primaryExp;
}

std::unique_ptr<Number> Parser::parseNumber() {
  auto number = std::make_unique<Number>();
  number->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::INTCON) {
    number->value = std::get<int>(current.value);
  }
  output("<Number>");
  return number;
}

std::unique_ptr<UnaryExp> Parser::parseUnaryExp() {
  auto unaryExp = std::make_unique<UnaryExp>();
  unaryExp->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::PLUS || current.type == TokenType::MINU ||
      current.type == TokenType::NOT) {
    // UnaryExp -> UnaryOp UnaryExp
    unaryExp->unaryOp = parseUnaryOp();
    advance();
    auto newunaryExp = parseUnaryExp();
    if (newunaryExp == nullptr) {
      return nullptr;
    }
    unaryExp->unaryType = UnaryExp::UnaryType::UNARY_OP;
  } else if (current.type == TokenType::IDENFR) {
    if (lexer.peekToken(1).type == TokenType::LPARENT) {
      // UnaryExp -> Ident '(' [FuncRParams] ')'
      unaryExp->unaryType = UnaryExp::UnaryType::FUNC_CALL;
      unaryExp->funcIdent = current.lexeme;
      advance(); // eat ident
      advance(); // eat lparent
      if (current.type != TokenType::RPARENT) {
        unaryExp->funcRParams = parseFuncRParams();
        expect({TokenType::RPARENT}, "j");
      }
    } else {
      // UnaryExp -> PrimaryExp
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
  unaryOp->line = current.line;
  lastVnline = current.line;
  if (current.type == TokenType::PLUS) {
    unaryOp->op = UnaryOp::OpType::PLUS;
  } else if (current.type == TokenType::MINU) {
    unaryOp->op = UnaryOp::OpType::MINUS;
  } else if (current.type == TokenType::NOT) {
    unaryOp->op = UnaryOp::OpType::NOT;
  } else {
    // error();
  }
  output("<UnaryOp>");
  return unaryOp;
}

std::unique_ptr<FuncRParams> Parser::parseFuncRParams() {
  auto funcRParams = std::make_unique<FuncRParams>();
  funcRParams->line = current.line;
  lastVnline = current.line;
  funcRParams->exps.push_back(parseExp());
  if (lexer.peekToken(1).type == TokenType::COMMA) {
    advance();
  } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
    advance();
    return nullptr;
  }

  while (current.type == TokenType::COMMA) {
    advance(); // eat comma
    funcRParams->exps.push_back(parseExp());
    if (lexer.peekToken(1).type == TokenType::COMMA) {
      advance();
    } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
      advance();
      return nullptr;
    }
  }
  output("<FuncRParams>");
  return funcRParams;
}

std::unique_ptr<MulExp> Parser::parseMulExp() {
  auto mulExp = std::make_unique<MulExp>();
  mulExp->line = current.line;
  lastVnline = current.line;
  auto unaryExp = parseUnaryExp();
  if (unaryExp == nullptr) {
    return nullptr;
  }
  mulExp->unaryExp = std::move(unaryExp);
  mulExp->op = MulExp::OpType::NONE;
  if (lexer.peekToken(1).type == TokenType::MULT ||
      lexer.peekToken(1).type == TokenType::DIV ||
      lexer.peekToken(1).type == TokenType::MOD) {
    output("<MulExp>");
    advance();
  } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
    advance();
    return nullptr;
  }

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
    auto unaryExp = parseUnaryExp();
    if (unaryExp == nullptr) {
      return nullptr;
    }
    newLeft->unaryExp = std::move(unaryExp);
    mulExp = std::move(newLeft);
    // look ahead
    if (lexer.peekToken(1).type == TokenType::MULT ||
        lexer.peekToken(1).type == TokenType::DIV ||
        lexer.peekToken(1).type == TokenType::MOD) {
      output("<MulExp>");
      advance();
    } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
      advance();
      return nullptr;
    }
  }
  output("<MulExp>");
  return mulExp;
}

std::unique_ptr<AddExp> Parser::parseAddExp() {
  auto addExp = std::make_unique<AddExp>();
  addExp->line = current.line;
  lastVnline = current.line;
  auto mulExp = parseMulExp();
  if (mulExp == nullptr) {
    return nullptr;
  }
  addExp->mulExp = std::move(mulExp);
  addExp->op = AddExp::OpType::NONE;
  // look ahead
  if (lexer.peekToken(1).type == TokenType::PLUS ||
      lexer.peekToken(1).type == TokenType::MINU) {
    output("<AddExp>");
    advance();
  } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
    advance();
    return nullptr;
  }

  while (current.type == TokenType::PLUS || current.type == TokenType::MINU) {
    auto newLeft = std::make_unique<AddExp>();
    newLeft->left = std::move(addExp);
    if (current.type == TokenType::PLUS) {
      newLeft->op = AddExp::OpType::PLUS;
    } else if (current.type == TokenType::MINU) {
      newLeft->op = AddExp::OpType::MINU;
    }
    advance();
    auto mulExp = parseMulExp();
    if (mulExp == nullptr) {
      return nullptr;
    }
    newLeft->mulExp = std::move(mulExp);
    addExp = std::move(newLeft);
    // look ahead
    if (lexer.peekToken(1).type == TokenType::PLUS ||
        lexer.peekToken(1).type == TokenType::MINU) {
      output("<AddExp>");
      advance();
    } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
      advance();
      return nullptr;
    }
  }
  output("<AddExp>");
  return addExp;
}

std::unique_ptr<RelExp> Parser::parseRelExp() {
  auto relExp = std::make_unique<RelExp>();
  relExp->line = current.line;
  lastVnline = current.line;
  auto addExp = parseAddExp();
  if (addExp == nullptr) {
    return nullptr;
  }
  relExp->addExp = std::move(addExp);
  relExp->op = RelExp::OpType::NONE;
  // look ahead
  if (lexer.peekToken(1).type == TokenType::LSS ||
      lexer.peekToken(1).type == TokenType::LEQ ||
      lexer.peekToken(1).type == TokenType::GRE ||
      lexer.peekToken(1).type == TokenType::GEQ) {
    output("<RelExp>");
    advance();
  } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
    advance();
    return nullptr;
  }
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
    auto addExp = parseAddExp();
    if (addExp == nullptr) {
      return nullptr;
    }
    newLeft->addExp = std::move(addExp);
    relExp = std::move(newLeft);
    // look ahead
    if (lexer.peekToken(1).type == TokenType::LSS ||
        lexer.peekToken(1).type == TokenType::LEQ ||
        lexer.peekToken(1).type == TokenType::GRE ||
        lexer.peekToken(1).type == TokenType::GEQ) {
      output("<RelExp>");
      advance();
    } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
      advance();
      return nullptr;
    }
  }
  output("<RelExp>");
  return relExp;
}

std::unique_ptr<EqExp> Parser::parseEqExp() {
  auto eqExp = std::make_unique<EqExp>();
  eqExp->line = current.line;
  lastVnline = current.line;
  auto relExp = parseRelExp();
  if (relExp == nullptr) {
    return nullptr;
  }
  eqExp->relExp = std::move(relExp);
  eqExp->op = EqExp::OpType::NONE;
  // look ahead
  if (lexer.peekToken(1).type == TokenType::EQL ||
      lexer.peekToken(1).type == TokenType::NEQ) {
    output("<EqExp>");
    advance();
  } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
    advance();
    return nullptr;
  }
  while (current.type == TokenType::EQL || current.type == TokenType::NEQ) {
    auto newLeft = std::make_unique<EqExp>();
    newLeft->left = std::move(eqExp);
    if (current.type == TokenType::EQL) {
      newLeft->op = EqExp::OpType::EQL;
    } else if (current.type == TokenType::NEQ) {
      newLeft->op = EqExp::OpType::NEQ;
    }
    advance();
    auto relExp = parseRelExp();
    if (relExp == nullptr) {
      return nullptr;
    }
    newLeft->relExp = std::move(relExp);
    eqExp = std::move(newLeft);
    // look ahead
    if (lexer.peekToken(1).type == TokenType::EQL ||
        lexer.peekToken(1).type == TokenType::NEQ) {
      output("<EqExp>");
      advance();
    } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
      advance();
      return nullptr;
    }
  }
  output("<EqExp>");
  return eqExp;
}

std::unique_ptr<LAndExp> Parser::parseLAndExp() {
  auto lAndExp = std::make_unique<LAndExp>();
  lAndExp->line = current.line;
  lastVnline = current.line;
  auto eqExp = parseEqExp();
  if (eqExp == nullptr) {
    return nullptr;
  }
  lAndExp->eqExp = std::move(eqExp);
  // look ahead
  if (lexer.peekToken(1).type == TokenType::AND) {
    output("<LAndExp>");
    advance();
  } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
    advance();
    return nullptr;
  }
  while (current.type == TokenType::AND) {
    auto newLeft = std::make_unique<LAndExp>();
    newLeft->left = std::move(lAndExp);
    advance();
    auto eqExp = parseEqExp();
    if (eqExp == nullptr) {
      return nullptr;
    }
    newLeft->eqExp = std::move(eqExp);
    lAndExp = std::move(newLeft);
    // look ahead
    if (lexer.peekToken(1).type == TokenType::AND) {
      output("<LAndExp>");
      advance();
    } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
      advance();
      return nullptr;
    }
  }
  output("<LAndExp>");
  return lAndExp;
}

std::unique_ptr<LOrExp> Parser::parseLOrExp() {
  auto lOrExp = std::make_unique<LOrExp>();
  lOrExp->line = current.line;
  lastVnline = current.line;
  auto lAndExp = parseLAndExp();
  if (lAndExp == nullptr) {
    return nullptr;
  }
  lOrExp->lAndExp = std::move(lAndExp);
  // look ahead
  if (lexer.peekToken(1).type == TokenType::OR) {
    output("<LOrExp>");
    advance();
  } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
    advance();
    return nullptr;
  }
  while (current.type == TokenType::OR) {
    auto newLeft = std::make_unique<LOrExp>();
    newLeft->left = std::move(lOrExp);
    advance();
    auto LAndExp = parseLAndExp();
    if (LAndExp == nullptr) {
      return nullptr;
    }
    newLeft->lAndExp = std::move(LAndExp);
    lOrExp = std::move(newLeft);
    // look ahead
    if (lexer.peekToken(1).type == TokenType::OR) {
      output("<LOrExp>");
      advance();
    } else if (lexer.peekToken(1).type == TokenType::UNKNOWN) {
      advance();
      return nullptr;
    }
  }
  output("<LOrExp>");
  return lOrExp;
}

std::unique_ptr<ConstExp> Parser::parseConstExp() {
  auto constExp = std::make_unique<ConstExp>();
  constExp->line = current.line;
  lastVnline = current.line;
  auto addExp = parseAddExp();
  if (addExp == nullptr) {
    return nullptr;
  }
  constExp->addExp = std::move(addExp);
  output("<ConstExp>");
  return constExp;
}
