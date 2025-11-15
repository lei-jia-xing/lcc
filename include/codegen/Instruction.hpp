#pragma once

#include "Operand.hpp"
#include <string>

namespace lcc::codegen {

enum class OpCode {
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  NEG,

  EQ,
  NEQ,
  LT,
  LE,
  GT,
  GE,

  AND,
  OR,
  NOT,

  ASSIGN,

  LOAD,  // t = base[index]
  STORE, // base[index] = value  (result 可为空)

  IF, // IF cond GOTO label
  GOTO,
  LABEL,

  PARAM,
  CALL, // ret = CALL func
  RETURN,

  PRINTF,

  DEF
};

class Instruction {
public:
  // format op arg1, arg2, result
  Instruction(OpCode op, Operand a1, Operand a2, Operand res);
  Instruction(OpCode op, Operand a1, Operand res); // 一元或赋值
  Instruction(OpCode op,
              Operand res); // LABEL, RETURN(值在res), GOTO(res=label)
  Instruction(OpCode op);   // 无操作数占位

  static Instruction MakeBinary(OpCode op, const Operand &a, const Operand &b,
                                const Operand &dst);
  static Instruction MakeUnary(OpCode op, const Operand &a, const Operand &dst);
  static Instruction MakeAssign(const Operand &src,
                                const Operand &dst); // ASSIGN src -> dst
  static Instruction MakeLoad(const Operand &base, const Operand &index,
                              const Operand &dst); // LOAD
  static Instruction MakeStore(const Operand &value, const Operand &base,
                               const Operand &index); // STORE
  static Instruction MakeIf(const Operand &cond,
                            const Operand &label); // IF cond GOTO label
  static Instruction MakeGoto(const Operand &label);
  static Instruction MakeLabel(const Operand &label);
  static Instruction MakeCall(const Operand &func, int argCount,
                              const Operand &ret);
  static Instruction MakeReturn(const Operand &value);
  static Instruction MakeDef(const Operand &symbol,
                             const Operand &size); // DEF symbol size

  std::string toString() const;

  OpCode getOp() const { return _op; }
  const Operand &getArg1() const { return _arg1; }
  const Operand &getArg2() const { return _arg2; }
  const Operand &getResult() const { return _result; }

  void setOp(OpCode op) { _op = op; }
  void setArg1(const Operand &v) { _arg1 = v; }
  void setArg2(const Operand &v) { _arg2 = v; }
  void setResult(const Operand &v) { _result = v; }

private:
  OpCode _op;
  Operand _arg1;
  Operand _arg2;
  Operand _result;
};

} // namespace lcc::codegen
