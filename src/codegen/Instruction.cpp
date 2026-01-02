#include "codegen/Instruction.hpp"
#include <sstream>

Instruction::Instruction(OpCode op, Operand a1, Operand a2, Operand res)
    : _op(op), _arg1(std::move(a1)), _arg2(std::move(a2)),
      _result(std::move(res)), _parent(nullptr) {}

Instruction::Instruction(OpCode op, Operand a1, Operand res)
    : _op(op), _arg1(std::move(a1)), _arg2(Operand()), _result(std::move(res)),
      _parent(nullptr) {}

Instruction::Instruction(OpCode op, Operand res)
    : _op(op), _arg1(Operand()), _arg2(Operand()), _result(std::move(res)),
      _parent(nullptr) {}

Instruction::Instruction(OpCode op)
    : _op(op), _arg1(Operand()), _arg2(Operand()), _result(Operand()),
      _parent(nullptr) {}

static const char *opToStr(OpCode op) {
  switch (op) {
  case OpCode::ADD:
    return "ADD";
  case OpCode::SUB:
    return "SUB";
  case OpCode::MUL:
    return "MUL";
  case OpCode::DIV:
    return "DIV";
  case OpCode::MOD:
    return "MOD";
  case OpCode::NEG:
    return "NEG";
  case OpCode::EQ:
    return "EQ";
  case OpCode::NEQ:
    return "NEQ";
  case OpCode::LT:
    return "LT";
  case OpCode::LE:
    return "LE";
  case OpCode::GT:
    return "GT";
  case OpCode::GE:
    return "GE";
  case OpCode::AND:
    return "AND";
  case OpCode::OR:
    return "OR";
  case OpCode::NOT:
    return "NOT";
  case OpCode::ASSIGN:
    return "ASSIGN";
  case OpCode::LOAD:
    return "LOAD";
  case OpCode::STORE:
    return "STORE";
  case OpCode::IF:
    return "IF";
  case OpCode::GOTO:
    return "GOTO";
  case OpCode::LABEL:
    return "LABEL";
  case OpCode::PARAM:
    return "PARAM";
  case OpCode::ARG:
    return "ARG";
  case OpCode::CALL:
    return "CALL";
  case OpCode::RETURN:
    return "RETURN";
  case OpCode::ALLOCA:
    return "ALLOCA";
  case OpCode::PHI:
    return "PHI";
  case OpCode::NOP:
    return "NOP";
  }
  return "OP";
}

std::string Instruction::toString() const {
  std::ostringstream oss;
  if (_op == OpCode::NOP) {
    oss << "NOP";
    return oss.str();
  }
  oss << opToStr(_op);
  bool hasA1 = _arg1.getType() == OperandType::Variable ||
               _arg1.getType() == OperandType::Temporary ||
               _arg1.getType() == OperandType::ConstantInt ||
               _arg1.getType() == OperandType::Label;
  bool hasA2 = _arg2.getType() == OperandType::Variable ||
               _arg2.getType() == OperandType::Temporary ||
               _arg2.getType() == OperandType::ConstantInt ||
               _arg2.getType() == OperandType::Label;
  bool hasRes = _result.getType() == OperandType::Variable ||
                _result.getType() == OperandType::Temporary ||
                _result.getType() == OperandType::ConstantInt ||
                _result.getType() == OperandType::Label;
  if (hasA1)
    oss << " " << _arg1.toString();
  if (hasA2)
    oss << (hasA1 ? ", " : " ") << _arg2.toString();
  if (hasRes)
    oss << ((hasA1 || hasA2) ? ", " : " ") << _result.toString();
  return oss.str();
}

Instruction Instruction::MakeBinary(OpCode op, const Operand &a,
                                    const Operand &b, const Operand &dst) {
  return Instruction(op, a, b, dst);
}
Instruction Instruction::MakeUnary(OpCode op, const Operand &a,
                                   const Operand &dst) {
  return Instruction(op, a, dst);
}
Instruction Instruction::MakeAssign(const Operand &src, const Operand &dst) {
  return Instruction(OpCode::ASSIGN, src, dst);
}
Instruction Instruction::MakeLoad(const Operand &base, const Operand &index,
                                  const Operand &dst) {
  return Instruction(OpCode::LOAD, base, index, dst);
}
Instruction Instruction::MakeStore(const Operand &value, const Operand &base,
                                   const Operand &index) {
  return Instruction(OpCode::STORE, value, base, index);
}
Instruction Instruction::MakeIf(const Operand &cond, const Operand &label) {
  return Instruction(OpCode::IF, cond, label);
}
Instruction Instruction::MakeGoto(const Operand &label) {
  return Instruction(OpCode::GOTO, label);
}
Instruction Instruction::MakeLabel(const Operand &label) {
  return Instruction(OpCode::LABEL, label);
}
Instruction Instruction::MakeParam(const Operand &idx, const Operand &var) {
  return Instruction(OpCode::PARAM, idx, var);
}
Instruction Instruction::MakeArg(const Operand &arg) {
  return Instruction(OpCode::ARG, arg, Operand(), Operand());
}
Instruction Instruction::MakeCall(const Operand &func, int argCount,
                                  const Operand &ret) {
  return Instruction(OpCode::CALL, Operand::ConstantInt(argCount), func, ret);
}
Instruction Instruction::MakeReturn(const Operand &value) {
  return Instruction(OpCode::RETURN, value);
}
Instruction Instruction::MakeAlloca(const Operand &symbol,
                                    const Operand &size) {
  return Instruction(OpCode::ALLOCA, symbol, size);
}
Instruction Instruction::MakePhi(const Operand &res) {
  return Instruction(OpCode::PHI, res);
}
Instruction Instruction::MakeNop() { return Instruction(OpCode::NOP); }
void Instruction::addPhiArg(const Operand &val, BasicBlock *bb) {
  _phiArgs.emplace_back(val, bb);
}
const std::vector<std::pair<Operand, BasicBlock *>> &
Instruction::getPhiArgs() const {
  return _phiArgs;
}
std::vector<std::pair<Operand, BasicBlock *>> &Instruction::getPhiArgs() {
  return _phiArgs;
}
