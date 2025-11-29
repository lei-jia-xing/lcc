#pragma once
#include "semantic/Type.hpp"
#include <string>

struct Symbol {
  std::string name;
  std::string globalName; // Global unique name
  TypePtr type;
  int line;

  Symbol(std::string name, TypePtr type, int line)
      : name(std::move(name)), type(type), line(line) {}
};
