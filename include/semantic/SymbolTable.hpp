#pragma once
#include "semantic/Symbol.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Minimal shared_ptr-based scope node with father link and insertion order
struct ScopeNode {
  int id = 0; // 1-based scope id
  std::weak_ptr<ScopeNode> father;
  std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols; // name -> symbol
  std::vector<std::shared_ptr<Symbol>> order;                        // declaration order
};

class SymbolTable {
private:
  std::shared_ptr<ScopeNode> cur_scope;                         // current active scope
  std::vector<std::shared_ptr<ScopeNode>> registry;             // all scopes by creation order
  int nextId = 1;                                               // next id to assign

public:
  SymbolTable() { pushScope(); }

  // Enter a new scope; set father to previous cur_scope
  void pushScope() {
    auto node = std::make_shared<ScopeNode>();
    node->id = nextId++;
    node->father = cur_scope; // empty for the first (global) scope
    registry.push_back(node);
    cur_scope = node;
  }

  // Leave current scope; move cur_scope to father
  void popScope() {
    if (cur_scope) {
      cur_scope = cur_scope->father.lock();
    }
  }

  // Add symbol to current scope; reject duplicate names within the same scope
  bool addSymbol(const Symbol &symbol) {
    if (!cur_scope)
      return false;
    if (cur_scope->symbols.count(symbol.name))
      return false;
    auto symPtr = std::make_shared<Symbol>(symbol);
    cur_scope->symbols.emplace(symbol.name, symPtr);
    cur_scope->order.push_back(symPtr);
    return true;
  }

  // Lookup by walking father chain from cur_scope outward
  std::optional<Symbol> findSymbol(const std::string &name) const {
    auto node = cur_scope;
    while (node) {
      auto it = node->symbols.find(name);
      if (it != node->symbols.end()) {
        return *(it->second);
      }
      node = node->father.lock();
    }
    return std::nullopt;
  }

  // Print by ascending scope id (creation order), and by declaration order within each scope
  void printTable() const {
    for (const auto &node : registry) {
      for (const auto &sym : node->order) {
        std::cout << node->id << " " << sym->name << " "
                  << to_string(sym->type) << std::endl;
      }
    }
  }
};
