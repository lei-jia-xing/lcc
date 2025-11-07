# LCC 编译器 - Semantic Analyzer 设计文档

## 概述

Semantic Analyzer（语义分析器）是 LCC 编译器的第三个阶段，负责对 Parser 生成的 AST 进行语义分析，包括类型检查、作用域管理、函数调用验证等。本文档详细介绍了 Semantic Analyzer 的设计架构、实现细节和使用方式。

## 架构设计

### 类结构

```cpp
class SemanticAnalyzer {
private:
  SymbolTable symbolTable;                        // 符号表
  int loop = 0;                                   // 循环嵌套深度
  TypePtr current_function_return_type = nullptr; // 当前函数返回类型

  void error(const int &line, const std::string errorType);

public:
  SemanticAnalyzer();
  void visit(CompUnit *node);

private:
  // 声明访问器
  void visit(Decl *node);
  void visit(ConstDecl *node);
  void visit(VarDecl *node);
  void visit(ConstDef *node, TypePtr type);
  void visit(VarDef *node, TypePtr type);

  // 函数访问器
  void visit(FuncDef *node);
  void visit(MainFuncDef *node);
  void visit(FuncFParams *node);
  void visit(FuncFParam *node);
  TypePtr visit(FuncType *node);

  // 语句访问器
  void visit(Block *node);
  void visit(BlockItem *node);
  void visit(Stmt *node);
  void visit(AssignStmt *node);
  void visit(ExpStmt *node);
  void visit(BlockStmt *node);
  void visit(IfStmt *node);
  void visit(ForStmt *node);
  void visit(BreakStmt *node);
  void visit(ContinueStmt *node);
  void visit(ReturnStmt *node);
  void visit(PrintfStmt *node);
  void visit(ForAssignStmt *node);

  // 表达式访问器（返回类型信息）
  TypePtr visit(Exp *node);
  TypePtr visit(Cond *node);
  TypePtr visit(LVal *node);
  TypePtr visit(PrimaryExp *node);
  TypePtr visit(Number *node);
  TypePtr visit(UnaryExp *node);
  void visit(UnaryOp *node);
  std::vector<TypePtr> visit(FuncRParams *node);
  TypePtr visit(MulExp *node);
  TypePtr visit(AddExp *node);
  TypePtr visit(RelExp *node);
  TypePtr visit(EqExp *node);
  TypePtr visit(LAndExp *node);
  TypePtr visit(LOrExp *node);
  TypePtr visit(ConstExp *node);

  // 初始化值访问器
  void visit(ConstInitVal *node);
  void visit(InitVal *node);
};
```

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

```cpp
class Type {
public:
  Category category;
  BaseType base_type;
  bool is_const = false;
  bool is_static = false;

  // 数组类型相关
  TypePtr array_element_type;
  int array_size = 0;

  // 函数类型相关
  TypePtr return_type;
  std::vector<TypePtr> params;

  // 类型创建工厂方法
  static TypePtr create_base_type(BaseType base, bool is_const = false, bool is_static = false);
  static TypePtr create_array_type(TypePtr element_type, int size);
  static TypePtr create_function_type(TypePtr ret_type, const std::vector<TypePtr> &params);

  // 预定义类型
  static TypePtr getIntType();
  static TypePtr getVoidType();
};
```

### 2. 符号表（Symbol Table）

#### 符号定义

```cpp
struct Symbol {
  std::string name;    // 符号名
  TypePtr type;        // 符号类型
  int line;           // 定义行号
};
```

#### 作用域管理

```cpp
class SymbolTable {
private:
  struct ScopeRecord {
    int level = 0;
    std::unordered_map<std::string, Symbol> table;
    std::vector<std::string> order;
  };

  std::vector<ScopeRecord> records;    // 所有作用域记录
  std::vector<size_t> active;          // 当前活跃作用域栈
  int nextLevel = 1;                   // 下一层级别号

public:
  SymbolTable();

  // 作用域管理
  void pushScope();
  void popScope();

  // 符号管理
  bool addSymbol(const Symbol &symbol);
  std::optional<Symbol> findSymbol(const std::string &name) const;

  // 调试输出
  void printTable() const;
};
```

## 语义分析流程

### 1. 整体分析流程

1. **初始化**：创建符号表，设置全局作用域
2. **编译单元分析**：遍历所有声明、函数定义和主函数
3. **符号表构建**：在分析过程中填充符号表
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

所有错误通过 ErrorReporter 统一收集，最后按行号排序输出。

## 实现特性

### 1. 作用域嵌套

- 支持多层作用域嵌套（全局 → 函数 → 复合语句）
- 符号查找遵循就近原则
- 作用域退出时自动清理局部符号

### 2. 类型推导

- 自动推导表达式类型
- 支持基本类型、数组类型、函数类型
- 类型信息存储在 AST 节点中供后续使用

### 3. 常量和静态支持

- 支持常量定义和类型检查
- 支持静态变量声明
- 防止常量赋值操作

### 4. 函数调用验证

- 参数数量检查
- 参数类型检查
- 返回值类型验证
- 内置函数特殊处理

## 与其他组件的交互

### 与 Parser 的交互

- 接收完整的 AST 树进行遍历分析
- 利用 AST 节点中的行号信息进行错误定位
- 将类型信息写回 AST 节点供后续阶段使用

### 符号表输出

语义分析完成后，符号表内容会输出到标准输出，格式为：

```
<作用域级别> <符号名> <类型>
```

```

