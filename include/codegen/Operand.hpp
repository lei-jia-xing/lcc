#pragma once

#include <memory>
#include <semantic/Symbol.hpp>
#include <string>
#include <variant>

enum class OperandType { Empty, Variable, Temporary, ConstantInt, Label };

class Operand {
public:
  Operand();
  explicit Operand(std::shared_ptr<Symbol> symbol);
  static Operand Temporary(int tempId);
  static Operand ConstantInt(int v);
  static Operand Label(int id);
  static Operand Variable(std::shared_ptr<Symbol> sym);

  OperandType getType() const { return _type; }
  std::string toString() const;

  const std::shared_ptr<Symbol> &asSymbol() const; // Variable
  int asInt() const; // Temporary / ConstantInt / Label id

  bool operator==(const Operand &other) const {
    if (_type != other._type)
      return false;
    if (_type == OperandType::Empty)
      return true;
    if (_type == OperandType::Variable) {
      return asSymbol() == other.asSymbol();
    }
    return asInt() == other.asInt();
  }
  bool operator!=(const Operand &other) const { return !(*this == other); }

private:
  Operand(OperandType t, std::variant<std::shared_ptr<Symbol>, int> val);
  OperandType _type;
  std::variant<std::shared_ptr<Symbol>, int> _value;
};
