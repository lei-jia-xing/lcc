#pragma once
#include "semantic/Symbol.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using Scope = std::unordered_map<std::string, Symbol>;

class SymbolTable {
private:
  std::vector<Scope> scopes;

public:
  SymbolTable() { pushScope(); }

  void pushScope() { scopes.emplace_back(); }

  void popScope() {
    if (!scopes.empty()) {
      scopes.pop_back();
    }
  }

  void printCurrentScopeLevel(std::ostream &os) const {
    os << "Current Scope Level: " << scopes.size() << std::endl;
  }

  bool addSymbol(const Symbol &symbol) {
    if (scopes.empty()) {
      return false;
    }
    if (scopes.back().count(symbol.name)) {
      return false;
    }
    scopes.back()[symbol.name] = symbol;
    return true;
  }

  std::optional<Symbol> findSymbol(const std::string &name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto symbol_it = it->find(name);
      if (symbol_it != it->end()) {
        return symbol_it->second;
      }
    }
    return std::nullopt;
  }

  void printTable(std::ostream &os) const {
    std::unordered_set<std::string> printed_symbols;
    for (int i = scopes.size() - 1; i >= 0; --i) {
      const auto &scope = scopes[i];
      int scope_level = i + 1;
      for (const auto &pair : scope) {
        const auto &symbol = pair.second;
        if (printed_symbols.find(symbol.name) == printed_symbols.end()) {
          os << scope_level << " " << symbol.name << " "
             << to_string(symbol.type) << std::endl;
          printed_symbols.insert(symbol.name);
        }
      }
    }
  }
};
