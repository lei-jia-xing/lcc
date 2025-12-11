#pragma once
#include "semantic/Type.hpp"
#include <string>

struct Symbol {
  int id;
  std::string name;
  std::string globalName; // Global unique name
  TypePtr type;
  int line;

  // for string literals
  Symbol(std::string name, TypePtr type, int line)
      : name(std::move(name)), type(type), line(line) {}
  // for variables and functions
  Symbol(int id, std::string name, TypePtr type, int line)
      : id(id), name(std::move(name)), type(type), line(line) {}
};
