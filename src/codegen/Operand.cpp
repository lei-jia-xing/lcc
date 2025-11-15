// Operand.cpp - implementation of Operand
#include "codegen/Operand.hpp"
#include <cassert>

using namespace lcc::codegen;

Operand::Operand() : _type(OperandType::Empty), _value(0) {}

Operand::Operand(std::shared_ptr<Symbol> symbol)
    : _type(OperandType::Variable), _value(std::move(symbol)) {}

Operand::Operand(OperandType t, std::variant<std::shared_ptr<Symbol>, int> val)
    : _type(t), _value(std::move(val)) {}

Operand Operand::Temporary(int tempId) {
  return Operand(OperandType::Temporary, tempId);
}
Operand Operand::ConstantInt(int v) {
  return Operand(OperandType::ConstantInt, v);
}
Operand Operand::Label(int id) { return Operand(OperandType::Label, id); }
Operand Operand::Variable(std::shared_ptr<Symbol> sym) { return Operand(sym); }

const std::shared_ptr<Symbol> &Operand::asSymbol() const {
  assert(_type == OperandType::Variable && "Operand is not a Variable");
  return std::get<std::shared_ptr<Symbol>>(_value);
}

int Operand::asInt() const {
  assert((_type == OperandType::Temporary ||
          _type == OperandType::ConstantInt || _type == OperandType::Label) &&
         "Operand does not hold an int");
  return std::get<int>(_value);
}

std::string Operand::toString() const {
  switch (_type) {
  case OperandType::Empty:
    return "";
  case OperandType::Variable: {
    auto sym = std::get<std::shared_ptr<Symbol>>(_value);
    return sym ? sym->name : std::string("<null>");
  }
  case OperandType::Temporary:
    return "t" + std::to_string(std::get<int>(_value));
  case OperandType::ConstantInt:
    return std::to_string(std::get<int>(_value));
  case OperandType::Label:
    return "L" + std::to_string(std::get<int>(_value));
  }
  return "";
}
