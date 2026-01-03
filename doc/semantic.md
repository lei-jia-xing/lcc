# LCC 编译器 - Semantic Analyzer 设计文档

## 概述

Semantic Analyzer（语义分析器）是 LCC 编译器的第三个阶段，负责对 Parser 生成的 AST 进行语义分析，包括类型检查、作用域管理、函数调用验证等。本文档详细介绍了 Semantic Analyzer 的设计架构、实现细节和使用方式。

## 核心组件

### 1. 类型系统

#### 基础类型

```cpp
enum class BaseType { VOID, INT };
```

#### 类型分类

```cpp
enum class Category { Basic, Array, Function };
```

#### 类型类设计

对类型设计了几个工厂方法，方便创建不同类型的实例

```cpp
enum class BaseType { VOID, INT };

class Type {
public:
  enum class Category { Basic, Array, Function };

  Category category;
  BaseType base_type;
  bool is_const = false;
  bool is_static = false;

  TypePtr array_element_type;
  int array_size = 0; // 一点用都没有

  TypePtr return_type;
  std::vector<TypePtr> params;

  Type(Category cat) : category(cat) {}

  static TypePtr create_base_type(BaseType base, bool is_const = false,
                                  bool is_static = false) {
    auto type = std::make_shared<Type>(Category::Basic);
    type->is_const = is_const;
    type->is_static = is_static;
    type->base_type = base;
    return type;
  }

  static TypePtr create_array_type(TypePtr element_type, int size) {
    auto type = std::make_shared<Type>(Category::Array);
    type->array_element_type = element_type;
    type->array_size = size;
    return type;
  }

  static TypePtr create_function_type(TypePtr ret_type,
                                      const std::vector<TypePtr> &params) {
    auto type = std::make_shared<Type>(Category::Function);
    type->return_type = ret_type;
    type->params = params;
    return type;
  }
  static TypePtr getIntType() { return create_base_type(BaseType::INT); }
  static TypePtr getVoidType() { return create_base_type(BaseType::VOID); }
};
```

### 2. 符号表（Symbol Table）

#### 符号定义

```cpp
struct Symbol {
  std::string name;
  TypePtr type;
  int line;
};
```

#### 作用域管理

定义了一个栈式符号表，使用了`ScopeRecord`结构体来记录每个作用域的信息,对于整个
符号表使用`records`来存储所有作用域的`level`,使用active来存储当前活跃的所有作用域索引。

**规则**：
进入新作用域时，调用`pushScope()`，离开作用域时，调用`popScope()`。在当前作用域添加符号时，调用`addSymbol()`。查找符号时，调用`findSymbol()`，从当前作用域开始向外查找。

```cpp
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
```

## 语义分析流程

### 整体分析流程

1. **初始化**：创建符号表，设置全局作用域
2. **编译单元分析**：遍历所有声明、函数定义和主函数
3. **符号表构建**：在分析过程中填充栈式符号表
4. **类型检查**：对表达式进行类型推导和检查
5. **控制流检查**：验证 break/continue/return 语句的正确性
6. **输出生成**：输出符号表信息供后续阶段使用

## 错误处理

### 错误类型

语义分析器检测以下错误类型：

- **b: 重定义错误**：标识符在同一作用域内重复定义
- **c: 未定义错误**：使用未定义的标识符
- **d: 函数参数数量错误**：函数调用时参数数量不匹配
- **e: 函数参数类型错误**：函数调用时参数类型不匹配
- **f: 返回值错误**：void 函数有返回值或返回值类型不匹配
- **g: 缺少返回语句**：有返回值的函数缺少 return 语句
- **h: 常量赋值错误**：试图给常量赋值
- **l: printf 参数数量不匹配**：格式字符串参数数量与实际参数不符
- **m: 循环控制语句错误**：break/continue 不在循环内使用

所有错误通过 `ErrorReporter` 统一收集，最后按行号排序输出。

## 与其他组件的交互

### 与 Parser 的交互

- 接收完整的 AST 树进行遍历分析
- 利用 AST 节点中的行号信息进行错误定位
- 将类型信息写回 AST 节点供后续阶段使用
