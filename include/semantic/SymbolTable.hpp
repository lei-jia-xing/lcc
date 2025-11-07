#pragma once
#include "semantic/Symbol.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class SymbolTable {
private:
  struct ScopeRecord {
    int level = 0;
    std::unordered_map<std::string, Symbol> table;
    std::vector<std::string> order;
  };

  /**
   * @brief records of all scopes
   */
  std::vector<ScopeRecord> records;
  /**
   * @brief current active scopes, storing indices into records
   */
  std::vector<size_t> active;
  /**
   * @brief the child scope level generator
   */
  int nextLevel = 1;

public:
  SymbolTable() { pushScope(); }

  void pushScope() {
    ScopeRecord rec;
    rec.level = nextLevel++;
    records.emplace_back(std::move(rec));
    active.push_back(records.size() - 1);
  }

  void popScope() {
    if (!active.empty()) {
      active.pop_back();
    }
  }

  bool addSymbol(const Symbol &symbol) {
    if (active.empty())
      return false;
    auto &rec = records[active.back()];
    if (rec.table.count(symbol.name))
      return false;
    rec.table.emplace(symbol.name, symbol);
    rec.order.push_back(symbol.name);
    return true;
  }

  std::optional<Symbol> findSymbol(const std::string &name) const {
    for (auto it = active.rbegin(); it != active.rend(); ++it) {
      const auto &rec = records[*it];
      auto f = rec.table.find(name);
      if (f != rec.table.end()) {
        return f->second;
      }
    }
    return std::nullopt;
  }

  void printTable() const {
    for (const auto &rec : records) {
      for (const auto &name : rec.order) {
        const auto &sym = rec.table.at(name);
        std::cout << rec.level << " " << sym.name << " " << to_string(sym.type)
                  << std::endl;
      }
    }
  }
};
