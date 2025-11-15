#pragma once

#include <memory>
#include <semantic/Symbol.hpp>
#include <string>
#include <variant>
namespace lcc::codegen {

enum class OperandType {
  Empty,       // 占位：指令缺省操作数
  Variable,    // 变量或函数名 (引用Symbol)
  Temporary,   // 临时变量 tN
  ConstantInt, // 整型常量
  Label        // 标签 LN
};

class Operand {
public:
  Operand();                                            // Empty
  explicit Operand(std::shared_ptr<Symbol> symbol);     // Variable
  static Operand Temporary(int tempId);                 // tN
  static Operand ConstantInt(int v);                    // 42
  static Operand Label(int id);                         // L3
  static Operand Variable(std::shared_ptr<Symbol> sym); // helper

  OperandType getType() const { return _type; }
  std::string toString() const;

  const std::shared_ptr<Symbol> &asSymbol() const; // Variable
  int asInt() const; // Temporary / ConstantInt / Label id

private:
  Operand(OperandType t, std::variant<std::shared_ptr<Symbol>, int> val);
  OperandType _type;
  std::variant<std::shared_ptr<Symbol>, int> _value;
};

} // namespace lcc::codegen
