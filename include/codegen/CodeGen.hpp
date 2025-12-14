#pragma once

#include "codegen/BasicBlock.hpp"
#include "codegen/Function.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Operand.hpp"
#include "parser/AST.hpp"
#include "semantic/Symbol.hpp"
#include "semantic/SymbolTable.hpp"
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CodeGen {
public:
  CodeGen() = default;
  explicit CodeGen(const SymbolTable &symbolTable);
  ~CodeGen() = default;

  void generate(CompUnit *root);

  void reset();

  const std::vector<std::shared_ptr<Function>> &getFunctions() const {
    return functions_;
  }
  const std::vector<std::unique_ptr<Instruction>> &getGlobalsIR() const {
    return globalsIR_;
  }
  const std::unordered_map<std::string, std::shared_ptr<Symbol>> &
  getStringLiteralSymbols() const {
    return stringLiterals_;
  }

private:
  void genFunction(FuncDef *funcDef);
  void genMainFuncDef(MainFuncDef *mainDef);

  void genBlock(Block *block);
  void genBlockItem(BlockItem *item);
  void genStmt(Stmt *stmt);
  void genAssign(AssignStmt *stmt);
  void genExpStmt(ExpStmt *stmt);
  void genIf(IfStmt *stmt);
  void genFor(ForStmt *stmt);
  void genBreak(BreakStmt *stmt);
  void genContinue(ContinueStmt *stmt);
  void genReturn(ReturnStmt *stmt);
  void genPrintf(PrintfStmt *stmt);
  void genForAssign(ForAssignStmt *stmt);

  void genDecl(Decl *decl);
  void genConstDecl(ConstDecl *decl);
  void genVarDecl(VarDecl *decl);
  void genConstDef(ConstDef *def);
  void genVarDef(VarDef *def, bool isStaticCtx);
  void genConstInitVal(ConstInitVal *init, const std::shared_ptr<Symbol> &sym);
  void genInitVal(InitVal *init, const std::shared_ptr<Symbol> &sym);
  Operand genConstExp(ConstExp *ce);

  Operand genExp(Exp *exp);
  void genCond(Cond *cond, int tLbl, int fLbl);
  Operand genLVal(LVal *lval, Operand *idx = nullptr);
  Operand genPrimary(PrimaryExp *pe);
  Operand genNumber(Number *num);
  Operand genUnary(UnaryExp *ue);
  Operand genMul(MulExp *me);
  Operand genAdd(AddExp *ae);
  Operand genRel(RelExp *re);
  Operand genEq(EqExp *ee);
  Operand genLAnd(LAndExp *la);
  Operand genLOr(LOrExp *lo);
  std::vector<Operand> genFuncRParams(FuncRParams *params);

  void emit(std::unique_ptr<Instruction> inst);
  void emitGlobal(std::unique_ptr<Instruction> inst);
  Operand newTemp();
  Operand newLabel();
  void placeLabel(const Operand &label);
  void output(const std::string &line);
  // folding constant expressions
  bool tryEvalExp(class Exp *exp, int &outVal);
  bool tryEvalConst(class ConstExp *ce, int &outVal);
  bool tryEvalConst(class AddExp *ae, int &outVal);
  bool tryEvalConst(class MulExp *me, int &outVal);
  bool tryEvalConst(class UnaryExp *ue, int &outVal);
  bool tryEvalConst(class PrimaryExp *pe, int &outVal);
  bool tryEvalConst(class Number *num, int &outVal);
  bool tryEvalConst(class RelExp *re, int &outVal);
  bool tryEvalConst(class EqExp *ee, int &outVal);
  bool tryEvalConst(class LAndExp *la, int &outVal);
  bool tryEvalConst(class LOrExp *lo, int &outVal);

  // short-circuit evaluation helper functions
  void branchLAndForCond(class LAndExp *node, int trueLbl, int falseLbl);
  void branchLOrForCond(class LOrExp *node, int trueLbl, int falseLbl);
  void branchLAndForVal(class LAndExp *node, int trueLbl, int falseLbl);
  void branchLOrForVal(class LOrExp *node, int trueLbl, int falseLbl);

  /**
   * @brief record the string literal
   *
   * @param literal the string literal content
   */
  std::shared_ptr<Symbol> internStringLiteral(const std::string &literal);

  /**
   * @brief intern a symbol with the given name and type
   *
   * @param name the symbol name
   * @param type the symbol type
   */

private:
  /**
   * @class Context
   * @brief a structure to hold the current code generation context
   *
   */
  struct Context {
    /**
     * @brief current function being generated
     */
    Function *func = nullptr;
    /**
     * @brief current basic block being generated
     */
    std::shared_ptr<BasicBlock> curBlk;
  } ctx_;

  /**
   * @brief record of constant values for variables known at compile time
   */
  std::unordered_map<std::shared_ptr<Symbol>, int> constValues_;
  /**
   * @brief reference to the frontend symbol table from semantic analysis
   */
  const SymbolTable *symbolTable_;
  /**
   * @brief all string literal symbols
   */
  std::unordered_map<std::string, std::shared_ptr<Symbol>> stringLiterals_;
  /**
   * @brief all constant array values
   */
  std::map<std::shared_ptr<Symbol>, std::vector<int>> constArrayValues_;
  /**
   * @brief the next string literal ID to use
   */
  int nextStringId_ = 0;
  /**
   * @brief the next static variable ID to use
   */
  int nextStaticId_ = 0;
  /**
   * @brief simple ir output control flag
   */
  bool outputEnabled_ = false;
  /**
   * @brief record of defined global variables to avoid duplicate definitions
   */
  std::unordered_set<std::string> definedGlobals_;
  /**
   * @class LoopContext
   * @brief current loop context for break/continue statement resolution
   *
   */
  struct LoopContext {
    int breakLabel;
    int continueLabel;
  };
  /**
   * @brief stack of loop contexts for break/continue statement resolution
   */
  std::vector<LoopContext> loopStack_;
  /**
   * @brief global IR instructions (for global variable definitions)
   */
  std::vector<std::unique_ptr<Instruction>> globalsIR_;
  /**
   * @brief global list of functions in the module
   */
  std::vector<std::shared_ptr<Function>> functions_;
  /**
   * @brief push current loop context onto the loop stack(where break/continue
   * jump to)
   *
   * @param breakLbl the label to jump to on break
   * @param continueLbl the label to jump to on continue
   */
  void pushLoop(int breakLbl, int continueLbl) {
    loopStack_.push_back({breakLbl, continueLbl});
  }
  /**
   * @brief pop current loop context from the loop stack(when quiting a loop)
   */
  void popLoop() {
    if (!loopStack_.empty())
      loopStack_.pop_back();
  }
  /**
   * @brief get current loop context
   *
   * @return nullptr if not in a loop,otherwise the current LoopContext pointer
   */
  LoopContext *currentLoop() {
    return loopStack_.empty() ? nullptr : &loopStack_.back();
  }
};
