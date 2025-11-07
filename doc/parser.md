# LCC 编译器 - Parser 设计文档

## 概述

Parser（语法分析器）是 LCC 编译器的第二个阶段，负责将 Lexer 生成的 Token 流转换为抽象语法树（AST）。本文档详细介绍了 Parser 的设计架构、实现细节和使用方式。

## 架构设计

### 类结构

```cpp
class Parser {
private:
  /**
   * @brief lexer for parser to get tokens
   */
  Lexer lexer;
  /**
   * @brief current token being processed and output
   */
  Token current;
  /**
   * @brief the last line number of a non-terminal being processed
   */
  int lastVnline = 0;
  /**
   * @brief to the next token
   */
  void advance();
  /**
   * @brief a function to check whether the next token is in the expected set
   *
   * @param types expected token types
   * @param errorType if not matched, report this error type
   * @return if matched, return true and advance to the next token; else report
   * the error and return false
   */
  bool expect(const std::vector<TokenType> &types,
              const std::string &errorType);

public:
  /**
   * @brief a function to report an error
   *
   * @param line report line number
   * @param errorType report error type
   */
  void error(const int &line, const std::string errorType);
  /**
   * @brief a constructor of Parser
   *
   * @param lexer lexer to get stream of tokens
   * @param current the first token from lexer
   */
  explicit Parser(Lexer &&lexer, Token current);
  /**
   * @brief a function to control whether to output the parse tree
   *
   * @param silent whether to silent the output
   */
  void silentPV(bool silent);
  /**
   * @brief output the current non-terminal being processed
   *
   * @param type the output non-terminal
   */
  void output(const std::string &type);
  // 编译单元 CompUnit → {Decl} {FuncDef} MainFuncDef
  std::unique_ptr<CompUnit> parseCompUnit();

private:
  std::unique_ptr<IfStmt> parseIfStmt();
  std::unique_ptr<ForStmt> parseForStmt();
  std::unique_ptr<BreakStmt> parseBreakStmt();
  std::unique_ptr<ContinueStmt> parseContinueStmt();
  std::unique_ptr<ReturnStmt> parseReturnStmt();
  std::unique_ptr<PrintfStmt> parsePrintfStmt();
  std::unique_ptr<AssignStmt> parseAssignStmt();
  std::unique_ptr<ExpStmt> parseExpStmt();
  std::unique_ptr<BlockStmt> parseBlockStmt();

private:
  // 声明 Decl → ConstDecl | VarDecl
  std::unique_ptr<Decl> parseDecl();

  // 常量声明 ConstDecl → 'const' BType ConstDef { ',' ConstDef } ';'
  std::unique_ptr<ConstDecl> parseConstDecl();

  // 变量声明 VarDecl → [ 'static' ] BType VarDef { ',' VarDef } ';'
  std::unique_ptr<VarDecl> parseVarDecl();

  // 基本类型 BType → 'int'
  std::unique_ptr<BType> parseBType();

  // 常量定义 ConstDef → Ident [ '[' ConstExp ']' ] '=' ConstInitVal
  std::unique_ptr<ConstDef> parseConstDef();

  // 变量定义 VarDef → Ident [ '[' ConstExp ']' ] | Ident [ '[' ConstExp ']' ]
  // '=' InitVal
  std::unique_ptr<VarDef> parseVarDef();

  // 常量初值 ConstInitVal → ConstExp | '{' [ ConstExp { ',' ConstExp } ] '}'
  std::unique_ptr<ConstInitVal> parseConstInitVal();

  // 变量初值 InitVal → Exp | '{' [ Exp { ',' Exp } ] '}'
  std::unique_ptr<InitVal> parseInitVal();

  // 函数定义 FuncDef → FuncType Ident '(' [FuncFParams] ')' Block
  std::unique_ptr<FuncDef> parseFuncDef();

  // 主函数定义 MainFuncDef → 'int' 'main' '(' ')' Block
  std::unique_ptr<MainFuncDef> parseMainFuncDef();

  // 函数类型 FuncType → 'void' | 'int'
  std::unique_ptr<FuncType> parseFuncType();

  // 函数形参表 FuncFParams → FuncFParam { ',' FuncFParam }
  std::unique_ptr<FuncFParams> parseFuncFParams();

  // 函数形参 FuncFParam → BType Ident ['[' ']']
  std::unique_ptr<FuncFParam> parseFuncFParam();

  // 语句块 Block → '{' { BlockItem } '}'
  std::unique_ptr<Block> parseBlock();

  // 语句块项 BlockItem → Decl | Stmt
  std::unique_ptr<BlockItem> parseBlockItem();

  // 语句 Stmt → LVal '=' Exp ';'
  //   | [Exp] ';'
  //   | Block
  //   | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
  //   | 'for' '(' [ForStmt] ';' [Cond] ';' [ForStmt] ')' Stmt
  //   | 'break' ';' | 'continue' ';'
  //   | 'return' [Exp] ';'
  //   | 'printf''('StringConst {','Exp}')'';'
  std::unique_ptr<Stmt> parseStmt();

  // 语句 ForStmt → LVal '=' Exp { ',' LVal '=' Exp }
  std::unique_ptr<ForAssignStmt> parseForAssignStmt();

  // 表达式 Exp → AddExp
  std::unique_ptr<Exp> parseExp();

  // 条件表达式 Cond → LOrExp
  std::unique_ptr<Cond> parseCond();

  // 左值表达式 LVal → Ident ['[' Exp ']']
  std::unique_ptr<LVal> parseLVal();

  // 基本表达式 PrimaryExp → '(' Exp ')' | LVal | Number
  std::unique_ptr<PrimaryExp> parsePrimaryExp();

  // 数值 Number → IntConst
  std::unique_ptr<Number> parseNumber();

  // 一元表达式 UnaryExp → PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp
  // UnaryExp
  std::unique_ptr<UnaryExp> parseUnaryExp();

  // 单目运算符 UnaryOp → '+' | '−' | '!'
  std::unique_ptr<UnaryOp> parseUnaryOp();

  // 函数实参表达式 FuncRParams → Exp { ',' Exp }
  std::unique_ptr<FuncRParams> parseFuncRParams();

  // 乘除模表达式 MulExp → UnaryExp | MulExp ('*' | '/' | '%') UnaryExp
  std::unique_ptr<MulExp> parseMulExp();

  // 加减表达式 AddExp → MulExp | AddExp ('+' | '−') MulExp
  std::unique_ptr<AddExp> parseAddExp();

  // 关系表达式 RelExp → AddExp | RelExp ('<' | '>' | '<=' | '>=') AddExp
  std::unique_ptr<RelExp> parseRelExp();

  // 相等性表达式 EqExp → RelExp | EqExp ('==' | '!=') RelExp
  std::unique_ptr<EqExp> parseEqExp();

  // 逻辑与表达式 LAndExp → EqExp | LAndExp '&&' EqExp
  std::unique_ptr<LAndExp> parseLAndExp();

  // 逻辑或表达式 LOrExp → LAndExp | LOrExp '||' LAndExp
  std::unique_ptr<LOrExp> parseLOrExp();

  // 常量表达式 ConstExp → AddExp
  std::unique_ptr<ConstExp> parseConstExp();
};
```

## 实现细节

### AST 节点设计

使用继承体系构建 AST，所有节点都继承自 `ASTNode` 基类：

```cpp
class ASTNode {
public:
  virtual ~ASTNode() = default;
  int line = 0;  // 记录源代码行号
};
```

### 递归下降解析策略

1. **自顶向下解析**：从文法开始符号 `CompUnit` 开始，递归调用各子解析函数
2. **前向查看**：使用 `lexer.peekToken(1)` 进行单 Token 前瞻，避免回溯
3. **左递归消除**：将左递归文法转换为右递归，适合递归下降解析

### 错误处理机制

#### 期望检查（Expect）

```cpp
bool Parser::expect(const std::vector<TokenType> &types, const std::string &errorType) {
  // 检查当前 Token 是否在期望的类型列表中
  // 如果不匹配，报告错误并返回 false
  // 如果匹配，消耗 Token 并返回 true
}
```

### 静默模式

与 Lexer 类似，Parser 也实现了静默模式机制：

```cpp
void Parser::silentPV(bool silent) {
  if (silent) {
    silentDepth++;
    lexer.silentPV(true);
  } else {
    silentDepth > 0 ? silentDepth-- : silentDepth = 0;
    lexer.silentPV(false);
  }
}
```

静默模式用于：

- 解析过程中的临时分析
- 错误恢复期间的 Token 跳过
- 前瞻性解析时的临时禁用输出

### 表达式解析优化

表达式解析采用优先级递增的方式：

1. **PrimaryExp**：括号表达式、左值、数字常量
2. **UnaryExp**：一元运算符、函数调用
3. **MulExp**：乘除模运算
4. **AddExp**：加减运算
5. **RelExp**：关系运算
6. **EqExp**：相等性运算
7. **LAndExp**：逻辑与运算
8. **LOrExp**：逻辑或运算

每个层次都处理该优先级的运算符，并递归调用下一优先级的解析函数。

## AST 节点类型

### 声明节点

- **CompUnit**：编译单元，包含声明、函数定义和主函数
- **Decl**：声明基类
- **ConstDecl**：常量声明
- **VarDecl**：变量声明
- **BType**：基本类型（int）
- **ConstDef**：常量定义
- **VarDef**：变量定义
- **ConstInitVal**：常量初始值
- **InitVal**：变量初始值

### 函数节点

- **FuncDef**：函数定义
- **MainFuncDef**：主函数定义
- **FuncType**：函数类型
- **FuncFParams**：函数形参列表
- **FuncFParam**：函数形参
- **FuncRParams**：函数实参列表

### 语句节点

- **Stmt**：语句基类，包含语句类型枚举
- **AssignStmt**：赋值语句
- **ExpStmt**：表达式语句
- **BlockStmt**：语句块
- **IfStmt**：条件语句
- **ForStmt**：循环语句
- **BreakStmt**：跳出语句
- **ContinueStmt**：继续语句
- **ReturnStmt**：返回语句
- **PrintfStmt**：输出语句
- **ForAssignStmt**：for 循环赋值语句

### 表达式节点

- **Exp**：表达式
- **Cond**：条件表达式
- **LVal**：左值
- **PrimaryExp**：基本表达式
- **Number**：数字常量
- **UnaryOp**：一元运算符
- **UnaryExp**：一元表达式
- **MulExp**：乘除模表达式
- **AddExp**：加减表达式
- **RelExp**：关系表达式
- **EqExp**：相等性表达式
- **LAndExp**：逻辑与表达式
- **LOrExp**：逻辑或表达式
- **ConstExp**：常量表达式

## 解析流程

### 主流程

1. **初始化**：构造 Parser 对象，传入 Lexer 和初始 Token
2. **解析编译单元**：调用 `parseCompUnit()` 开始解析
3. **递归解析**：根据当前 Token 类型调用相应的解析函数
4. **构建 AST**：每个解析函数返回对应的 AST 节点
5. **错误处理**：遇到语法错误时报告并尝试恢复
6. **输出结果**：输出语法分析结果的产生式

### 语句解析策略

由于部分语句存在二义性（如标识符开头的可能是赋值语句或表达式语句），Parser 采用以下策略：

```cpp
if (current.type == TokenType::IDENFR) {
  // 临时保存状态
  auto temp = lexer;
  auto tempCurrent = current;
  silentPV(true);

  // 尝试解析左值
  parseLVal();
  advance();

  // 检查是否为赋值操作
  if (current.type == TokenType::ASSIGN) {
    // 恢复状态，按赋值语句解析
    lexer = temp;
    current = tempCurrent;
    silentPV(false);
    return parseAssignStmt();
  } else {
    // 恢复状态，按表达式语句解析
    lexer = temp;
    current = tempCurrent;
    silentPV(false);
    return parseExpStmt();
  }
}
```

## 错误处理

### 错误类型

Parser 主要识别以下语法错误：

- **i 类型错误**：缺少分号
- **j 类型错误**： 缺少右括号')'
- **k 类型错误**：缺少右中括号']'

## 与其他组件的交互

### 与 Lexer 的交互

- **Token 获取**：通过 `lexer.nextToken()` 获取下一个 Token
- **前向查看**：通过 `lexer.peekToken(n)` 查看第 n 个未来的 Token
- **静默同步**：通过 `silentPV()` 方法与 Lexer 的静默模式同步

## 文档补充

做到语义分析的时候，我预想的流式编译器设计似乎行不通了,还是需要扫描
完整的AST树，因为在做Parser的时候，我根本没有想过类型相关的东西,因此
我在AST节点中加入了类型信息，这部分的内容由语义分析来填充。

并且之前是没有做输出的开关的，语义分析需要关闭，因此又做了一个极其简单
的开关来控制输出,另外语义分析是需要其他的信息的，比如函数名的所在行数,
'}'的所在行数，因此AST树又补充了一点信息，之前的设计和实际代码其实是不
完全一样的,不过也差不多，反映了我的设计过程。
