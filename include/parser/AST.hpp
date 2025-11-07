#pragma once
#include <lexer/Token.hpp>
#include <memory>
#include <semantic/Type.hpp>
#include <string>
#include <vector>

class ASTNode;
class CompUnit;
class Decl;
class ConstDecl;
class VarDecl;
class BType;
class ConstDef;
class VarDef;
class ConstInitVal;
class InitVal;
class FuncDef;
class MainFuncDef;
class FuncType;
class FuncFParams;
class FuncFParam;
class Block;
class BlockItem;
class Stmt;
class ForStmt;
class Exp;
class Cond;
class LVal;
class PrimaryExp;
class Number;
class UnaryExp;
class UnaryOp;
class FuncRParams;
class MulExp;
class AddExp;
class RelExp;
class EqExp;
class LAndExp;
class LOrExp;
class ConstExp;

/**
 * @class ASTNode
 * @brief ASTNode base class
 *
 */
class ASTNode {
public:
  virtual ~ASTNode() = default;
  int line = 0;
};

/**
 * @class CompUnit
 * @brief CompUnit → {Decl} {FuncDef} MainFuncDef
 *
 */
class CompUnit : public ASTNode {
public:
  std::vector<std::unique_ptr<Decl>> decls;
  std::vector<std::unique_ptr<FuncDef>> funcDefs;
  std::unique_ptr<MainFuncDef> mainFuncDef;
};

/**
 * @class Decl
 * @brief Decl → ConstDecl | VarDecl
 *
 */
class Decl : public ASTNode {
public:
  virtual ~Decl() = default;
};

/**
 * @class ConstDecl
 * @brief ConstDecl → 'const' BType ConstDef { ',' ConstDef } ';' // i
 *
 */
class ConstDecl : public Decl {
public:
  std::unique_ptr<BType> bType;
  std::vector<std::unique_ptr<ConstDef>> constDefs;
};

/**
 * @class VarDecl
 * @brief VarDecl -> [ 'static' ] BType VarDef { ',' VarDef } ';' // i
 *
 */
class VarDecl : public Decl {
public:
  bool isStatic = false;
  std::unique_ptr<BType> bType;
  std::vector<std::unique_ptr<VarDef>> varDefs;
};

/**
 * @class BType
 * @brief Btype -> 'int'
 *
 */
class BType : public ASTNode {
public:
  std::string type;
};

/**
 * @class ConstDef
 * @brief ConstDef -> Ident [ '[' ConstExp ']'] '=' ConstInitVal // k
 *
 */
class ConstDef : public ASTNode {
public:
  std::string ident;
  std::unique_ptr<ConstExp> arraySize;
  std::unique_ptr<ConstInitVal> constinitVal;
};

/**
 * @class VarDef
 * @brief VarDef → Ident [ '[' ConstExp ']' ] | Ident [ '[' ConstExp ']' ] '='
 * InitVal // k
 *
 */
class VarDef : public ASTNode {
public:
  std::string ident;
  std::unique_ptr<ConstExp> arraySize;
  std::unique_ptr<InitVal> initVal;
};

/**
 * @class ConstInitVal
 * @brief ConstInitVal → ConstExp | '{' [ ConstExp { ',' ConstExp } ] '}'
 *
 */
class ConstInitVal : public ASTNode {
public:
  std::unique_ptr<ConstExp> exp;
  std::vector<std::unique_ptr<ConstExp>> arrayExps;
  bool isArray = false;
};

/**
 * @class InitVal
 * @brief InitVal → Exp | '{' [ Exp { ',' Exp } ] '}'
 *
 */
class InitVal : public ASTNode {
public:
  std::unique_ptr<Exp> exp;
  std::vector<std::unique_ptr<Exp>> arrayExps;
  bool isArray = false;
};

/**
 * @class FuncDef
 * @brief FuncDef → FuncType Ident '(' [FuncFParams] ')' Block // j
 *
 */
class FuncDef : public ASTNode {
public:
  std::unique_ptr<FuncType> funcType;
  std::string ident;
  int identLine = 0;
  std::unique_ptr<FuncFParams> params;
  std::unique_ptr<Block> block;
};

/**
 * @class MainFuncDef
 * @brief MainFuncDef → 'int' 'main' '(' ')' Block // j
 *
 */
class MainFuncDef : public ASTNode {
public:
  std::unique_ptr<Block> block;
};

/**
 * @class FuncType
 * @brief FuncType → 'void' | 'int'
 *
 */
class FuncType : public ASTNode {
public:
  std::string type;
};

/**
 * @class FuncFParams
 * @brief FuncFParams → FuncFParam { ',' FuncFParam }
 *
 */
class FuncFParams : public ASTNode {
public:
  std::vector<std::unique_ptr<FuncFParam>> params;
  std::vector<TypePtr> types;
};

/**
 * @class FuncFParam
 * @brief FuncFParam → BType Ident ['[' ']'] // k
 *
 */
class FuncFParam : public ASTNode {
public:
  std::unique_ptr<BType> bType;
  std::string ident;
  int identLine = 0;
  bool isArray = false;
  TypePtr type;
};

/**
 * @class Block
 * @brief Block → '{' { BlockItem } '}'
 *
 */
class Block : public ASTNode {
public:
  std::vector<std::unique_ptr<BlockItem>> items;
  int closingBraceLine = 0; // 记录结束大括号'}'的行号
};

/**
 * @class BlockItem
 * @brief BlockItem → Decl | Stmt
 *
 */
class BlockItem : public ASTNode {
public:
  std::unique_ptr<Decl> decl;
  std::unique_ptr<Stmt> stmt;
};

/**
 * @class Stmt
 * @brief Stmt base class
 *
 */
class Stmt : public ASTNode {
public:
  enum class StmtType {
    ASSIGN,
    EXP,
    BLOCK,
    IF,
    FOR,
    BREAK,
    CONTINUE,
    RETURN,
    PRINTF
  };

  StmtType stmtType;
};

/**
 * @class AssignStmt
 * @brief Stmt -> LVal '=' Exp ';' // i
 *
 */
class AssignStmt : public Stmt {
public:
  std::unique_ptr<LVal> lval;
  std::unique_ptr<Exp> exp;
  AssignStmt() { stmtType = StmtType::ASSIGN; }
};

/**
 * @class ExpStmt
 * @brief Stmt -> [Exp] ';' // i
 *
 */
class ExpStmt : public Stmt {
public:
  std::unique_ptr<Exp> exp;
  ExpStmt() { stmtType = StmtType::EXP; }
};

/**
 * @class BlockStmt
 * @brief Stmt -> Block
 *
 */
class BlockStmt : public Stmt {
public:
  std::unique_ptr<Block> block;
  BlockStmt() { stmtType = StmtType::BLOCK; }
};

/**
 * @class IfStmt
 * @brief Stmt -> 'if' '(' Cond ')' Stmt [ 'else' Stmt ] // j
 *
 */
class IfStmt : public Stmt {
public:
  std::unique_ptr<Cond> cond;
  std::unique_ptr<Stmt> thenStmt;
  std::unique_ptr<Stmt> elseStmt;
  IfStmt() { stmtType = StmtType::IF; }
};

/**
 * @class ForStmt
 * @brief 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt
 *
 */
class ForStmt : public Stmt {
public:
  std::unique_ptr<class ForAssignStmt> initStmt;
  std::unique_ptr<Cond> cond;
  std::unique_ptr<class ForAssignStmt> updateStmt;
  std::unique_ptr<Stmt> bodyStmt;
  ForStmt() { stmtType = StmtType::FOR; }
};

/**
 * @class BreakStmt
 * @brief Stmt -> 'break' ';' // i
 *
 */
class BreakStmt : public Stmt {
public:
  BreakStmt() { stmtType = StmtType::BREAK; }
};

/**
 * @class ContinueStmt
 * @brief Stmt -> 'continue' ';' // i
 *
 */
class ContinueStmt : public Stmt {
public:
  ContinueStmt() { stmtType = StmtType::CONTINUE; }
};

/**
 * @class ReturnStmt
 * @brief Stmt -> 'return' [Exp] // i
 *
 */
class ReturnStmt : public Stmt {
public:
  std::unique_ptr<Exp> exp;
  ReturnStmt() { stmtType = StmtType::RETURN; }
};

/**
 * @class PrintfStmt
 * @brief 'printf' '('StringConst {','Exp}')'';' // i j
 *
 */
class PrintfStmt : public Stmt {
public:
  std::string formatString;
  std::vector<std::unique_ptr<Exp>> args;
  PrintfStmt() { stmtType = StmtType::PRINTF; }
};

/**
 * @class ForAssignStmt
 * @brief ForStmt → LVal '=' Exp { ',' LVal '=' Exp }
 *
 */
class ForAssignStmt : public ASTNode {
public:
  struct Assignment {
    std::unique_ptr<LVal> lval;
    std::unique_ptr<Exp> exp;
  };
  std::vector<Assignment> assignments;
};

/**
 * @class Exp
 * @brief Exp -> AddExp
 *
 */
class Exp : public ASTNode {
public:
  std::unique_ptr<AddExp> addExp;
  TypePtr type;
};

/**
 * @class Cond
 * @brief Cond -> LOrExp
 *
 */
class Cond : public ASTNode {
public:
  std::unique_ptr<LOrExp> lOrExp;
  TypePtr type;
};

/**
 * @class LVal
 * @brief LVal -> Ident ['[' Exp ']] // k
 *
 */
class LVal : public ASTNode {
public:
  std::string ident;
  std::unique_ptr<Exp> arrayIndex;
  TypePtr type;
};

/**
 * @class PrimaryExp
 * @brief PrimaryExp → '(' Exp ')' | LVal | Number // j
 *
 */
class PrimaryExp : public ASTNode {
public:
  enum class PrimaryType { EXP, LVAL, NUMBER };
  PrimaryType primaryType;
  std::unique_ptr<Exp> exp;
  std::unique_ptr<LVal> lval;
  std::unique_ptr<Number> number;
  TypePtr type;
};

/**
 * @class Number
 * @brief Number -> IntConst
 *
 */
class Number : public ASTNode {
public:
  int value;
  TypePtr type;
};

/**
 * @class UnaryOp
 * @brief UnaryOp → '+' | '−' | '!'
 *
 */
class UnaryOp : public ASTNode {
public:
  enum class OpType { PLUS, MINUS, NOT };
  OpType op;
};

/**
 * @class UnaryExp
 * @brief UnaryExp → PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp // j
 *
 */
class UnaryExp : public ASTNode {
public:
  enum class UnaryType { PRIMARY, FUNC_CALL, UNARY_OP };
  UnaryType unaryType;

  std::unique_ptr<PrimaryExp> primaryExp;
  std::string funcIdent;
  std::unique_ptr<FuncRParams> funcRParams;
  std::unique_ptr<UnaryOp> unaryOp;
  std::unique_ptr<UnaryExp> unaryExp;
  TypePtr type;
};

/**
 * @class FuncRParams
 * @brief FuncRParams → Exp { ',' Exp }
 *
 */
class FuncRParams : public ASTNode {
public:
  std::vector<std::unique_ptr<Exp>> exps;
  std::vector<TypePtr> types;
};

/**
 * @class MulExp
 * @brief MulExp → UnaryExp | MulExp ('*' | '/' | '%') UnaryExp
 *
 */
class MulExp : public ASTNode {
public:
  enum class OpType { NONE, MULT, DIV, MOD };
  std::unique_ptr<MulExp> left;
  OpType op = OpType::NONE;
  std::unique_ptr<UnaryExp> unaryExp;
  TypePtr type;
};

/**
 * @class AddExp
 * @brief AddExp → MulExp | AddExp ('+' | '−') MulExp
 *
 */
class AddExp : public ASTNode {
public:
  enum class OpType { NONE, PLUS, MINU };
  std::unique_ptr<AddExp> left;
  OpType op = OpType::NONE;
  std::unique_ptr<MulExp> mulExp;
  TypePtr type;
};

/**
 * @class RelExp
 * @brief RelExp → AddExp | RelExp ('<' | '>' | '<=' | '>=') AddExp
 *
 */
class RelExp : public ASTNode {
public:
  enum class OpType { NONE, LSS, GRE, LEQ, GEQ };
  std::unique_ptr<RelExp> left;
  OpType op = OpType::NONE;
  std::unique_ptr<AddExp> addExp;
  TypePtr type;
};

/**
 * @class EqExp
 * @brief EqExp → RelExp | EqExp ('==' | '!=') RelExp
 *
 */
class EqExp : public ASTNode {
public:
  enum class OpType { NONE, EQL, NEQ };
  std::unique_ptr<EqExp> left;
  OpType op = OpType::NONE;
  std::unique_ptr<RelExp> relExp;
  TypePtr type;
};

/**
 * @class LAndExp
 * @brief LAndExp → EqExp | LAndExp '&&' EqExp
 *
 */
class LAndExp : public ASTNode {
public:
  std::unique_ptr<LAndExp> left;
  std::unique_ptr<EqExp> eqExp;
  TypePtr type;
};

/**
 * @class LOrExp
 * @brief LOrExp → LAndExp | LOrExp '||' LAndExp
 *
 */
class LOrExp : public ASTNode {
public:
  std::unique_ptr<LOrExp> left;
  std::unique_ptr<LAndExp> lAndExp;
  TypePtr type;
};

/**
 * @class ConstExp
 * @brief ConstExp -> AddExp
 *
 */
class ConstExp : public ASTNode {
public:
  std::unique_ptr<AddExp> addExp;
  TypePtr type;
};
