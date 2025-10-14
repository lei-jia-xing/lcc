#pragma once
#include "../lexer/Lexer.hpp"
#include "../lexer/Token.hpp"
#include "AST.hpp"
#include <memory>
#include <vector>

class Parser {
private:
  Lexer lexer;
  Token current;
  int lastVnline = 0;
  void advance();
  bool expect(const std::vector<TokenType> &types,
              const std::string &errorType);
  void sync(const std::vector<TokenType> &types);
  void syncIfCond();
  inline static int silentDepth = 0;

public:
  void error(const int &line, const std::string errorType);
  explicit Parser(Lexer &&lexer, Token current);
  void silentPV(bool silent);
  void output(const std::string &type);
  // 编译单元 CompUnit → {Decl} {FuncDef} MainFuncDef
  std::unique_ptr<CompUnit> parseCompUnit();

private:
  std::unique_ptr<IfStmt> parseIfStmt();
  std::unique_ptr<ForStmt> parseForStmt();
  std::unique_ptr<BreakStmt> parseBreakStmt();
  std::unique_ptr<ContinueStmt> parseContinueStmt();
  std::unique_ptr<ReturnStmt> parseReturnStmt();
  std::unique_ptr<PrintfStmt> parsePrintfStmt();
  std::unique_ptr<AssignStmt> parseAssignStmt();
  std::unique_ptr<ExpStmt> parseExpStmt();
  std::unique_ptr<BlockStmt> parseBlockStmt();

private:
  // 声明 Decl → ConstDecl | VarDecl
  std::unique_ptr<Decl> parseDecl();

  // 常量声明 ConstDecl → 'const' BType ConstDef { ',' ConstDef } ';'
  std::unique_ptr<ConstDecl> parseConstDecl();

  // 变量声明 VarDecl → [ 'static' ] BType VarDef { ',' VarDef } ';'
  std::unique_ptr<VarDecl> parseVarDecl();

  // 基本类型 BType → 'int'
  std::unique_ptr<BType> parseBType();

  // 常量定义 ConstDef → Ident [ '[' ConstExp ']' ] '=' ConstInitVal
  std::unique_ptr<ConstDef> parseConstDef();

  // 变量定义 VarDef → Ident [ '[' ConstExp ']' ] | Ident [ '[' ConstExp ']' ]
  // '=' InitVal
  std::unique_ptr<VarDef> parseVarDef();

  // 常量初值 ConstInitVal → ConstExp | '{' [ ConstExp { ',' ConstExp } ] '}'
  std::unique_ptr<ConstInitVal> parseConstInitVal();

  // 变量初值 InitVal → Exp | '{' [ Exp { ',' Exp } ] '}'
  std::unique_ptr<InitVal> parseInitVal();

  // 函数定义 FuncDef → FuncType Ident '(' [FuncFParams] ')' Block
  std::unique_ptr<FuncDef> parseFuncDef();

  // 主函数定义 MainFuncDef → 'int' 'main' '(' ')' Block
  std::unique_ptr<MainFuncDef> parseMainFuncDef();

  // 函数类型 FuncType → 'void' | 'int'
  std::unique_ptr<FuncType> parseFuncType();

  // 函数形参表 FuncFParams → FuncFParam { ',' FuncFParam }
  std::unique_ptr<FuncFParams> parseFuncFParams();

  // 函数形参 FuncFParam → BType Ident ['[' ']']
  std::unique_ptr<FuncFParam> parseFuncFParam();

  // 语句块 Block → '{' { BlockItem } '}'
  std::unique_ptr<Block> parseBlock();

  // 语句块项 BlockItem → Decl | Stmt
  std::unique_ptr<BlockItem> parseBlockItem();

  // 语句 Stmt → LVal '=' Exp ';'
  //   | [Exp] ';'
  //   | Block
  //   | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
  //   | 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt
  //   | 'break' ';' | 'continue' ';'
  //   | 'return' [Exp] ';'
  //   | 'printf''('StringConst {','Exp}')'';'
  std::unique_ptr<Stmt> parseStmt();

  // 语句 ForStmt → LVal '=' Exp { ',' LVal '=' Exp }
  std::unique_ptr<ForAssignStmt> parseForAssignStmt();

  // 表达式 Exp → AddExp
  std::unique_ptr<Exp> parseExp();

  // 条件表达式 Cond → LOrExp
  std::unique_ptr<Cond> parseCond();

  // 左值表达式 LVal → Ident ['[' Exp ']']
  std::unique_ptr<LVal> parseLVal();

  // 基本表达式 PrimaryExp → '(' Exp ')' | LVal | Number
  std::unique_ptr<PrimaryExp> parsePrimaryExp();

  // 数值 Number → IntConst
  std::unique_ptr<Number> parseNumber();

  // 一元表达式 UnaryExp → PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp
  // UnaryExp
  std::unique_ptr<UnaryExp> parseUnaryExp();

  // 单目运算符 UnaryOp → '+' | '−' | '!'
  std::unique_ptr<UnaryOp> parseUnaryOp();

  // 函数实参表达式 FuncRParams → Exp { ',' Exp }
  std::unique_ptr<FuncRParams> parseFuncRParams();

  // 乘除模表达式 MulExp → UnaryExp | MulExp ('*' | '/' | '%') UnaryExp
  std::unique_ptr<MulExp> parseMulExp();

  // 加减表达式 AddExp → MulExp | AddExp ('+' | '−') MulExp
  std::unique_ptr<AddExp> parseAddExp();

  // 关系表达式 RelExp → AddExp | RelExp ('<' | '>' | '<=' | '>=') AddExp
  std::unique_ptr<RelExp> parseRelExp();

  // 相等性表达式 EqExp → RelExp | EqExp ('==' | '!=') RelExp
  std::unique_ptr<EqExp> parseEqExp();

  // 逻辑与表达式 LAndExp → EqExp | LAndExp '&&' EqExp
  std::unique_ptr<LAndExp> parseLAndExp();

  // 逻辑或表达式 LOrExp → LAndExp | LOrExp '||' LAndExp
  std::unique_ptr<LOrExp> parseLOrExp();

  // 常量表达式 ConstExp → AddExp
  std::unique_ptr<ConstExp> parseConstExp();
};
