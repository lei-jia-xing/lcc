#pragma once
#include "semantic/Type.hpp" // 引入新的Type系统
#include <string>
#include <vector>

struct Symbol {
  std::string name;
  TypePtr type;
  int line;

  std::vector<int> values;

  Symbol(std::string name, TypePtr type, std::vector<int> values, int line)
      : name(std::move(name)), type(type), values(values), line(line) {}
};
