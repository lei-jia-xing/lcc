#pragma once

#include "Operand.hpp"
#include <string>

enum class OpCode {
  ADD, // ADD arg1(var|temp|const), arg2(var|temp|const), res(temp)
  SUB, // SUB arg1(var|temp|const), arg2(var|temp|const), res(temp)
  MUL, // MUL arg1(var|temp|const), arg2(var|temp|const), res(temp)
  DIV, // DIV arg1(var|temp|const), arg2(var|temp|const), res(temp)
  MOD, // MOD arg1(var|temp|const), arg2(var|temp|const), res(temp)
  NEG, // NEG arg1(var|temp|const), -, res(temp)

  EQ,  // EQ arg1(var|temp|const), arg2(var|temp|const), res(temp)
  NEQ, // NEQ arg1(var|temp|const), arg2(var|temp|const), res(temp)
  LT,  // LT arg1(var|temp|const), arg2(var|temp|const), res(temp)
  LE,  // LE arg1(var|temp|const), arg2(var|temp|const), res(temp)
  GT,  // GT arg1(var|temp|const), arg2(var|temp|const), res(temp)
  GE,  // GE arg1(var|temp|const), arg2(var|temp|const), res(temp)

  AND, // AND arg1(var|temp|const), arg2(var|temp|const), res(temp)
  OR,  // OR arg1(var|temp|const), arg2(var|temp|const), res(temp)
  NOT, // NOT arg1(var|temp|const), -, res(temp)

  ASSIGN, // ASSIGN src(var|temp|const), -, dst(var)

  LOAD,  // LOAD base(var), index(var|temp|const), dst(temp)
  STORE, // STORE value(var|temp|const), base(var), index(var|temp|const)

  IF,    // IF cond(var|temp|const), -, res(label)
  GOTO,  // GOTO -, -, res(label)
  LABEL, // label -, -, res(label)

  PARAM,  // PARAM idx(const), -, var(var)          function def
  ARG,    // ARG arg(var|const|temp), -, -          function call
  CALL,   // CALL argCount(const), func(label), res(temp)
  RETURN, // RETURN -, -, res(var|temp|const)

  ALLOCA // ALLOCA var(var), -, size(var|temp|const)
};

class Instruction {
public:
  Instruction(OpCode op, Operand a1, Operand a2, Operand res);
  Instruction(OpCode op, Operand a1, Operand res);
  Instruction(OpCode op, Operand res);
  Instruction(OpCode op);

  static Instruction MakeBinary(OpCode op, const Operand &a, const Operand &b,
                                const Operand &dst);
  static Instruction MakeUnary(OpCode op, const Operand &a, const Operand &dst);
  static Instruction MakeAssign(const Operand &src, const Operand &dst);
  static Instruction MakeLoad(const Operand &base, const Operand &index,
                              const Operand &dst);
  static Instruction MakeStore(const Operand &value, const Operand &base,
                               const Operand &index);
  static Instruction MakeIf(const Operand &cond, const Operand &label);
  static Instruction MakeGoto(const Operand &label);
  static Instruction MakeLabel(const Operand &label);
  static Instruction MakeParam(const Operand &idx, const Operand &var);
  static Instruction MakeArg(const Operand &arg);
  static Instruction MakeCall(const Operand &func, int argCount,
                              const Operand &ret);
  static Instruction MakeReturn(const Operand &value);
  static Instruction MakeAlloca(const Operand &symbol, const Operand &size);

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
