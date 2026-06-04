# NG 编译器重构优化方案

> 创建日期：2026-06-03
> 目标：在保证行为不变的前提下，简化代码复杂度、减小文件规模、提升质量和可维护性
> 原则：每步可独立提交，每步都有测试覆盖，逐步推进不破坏现有功能

---

## 目录

1. [总体架构依赖关系](#1-总体架构依赖关系)
2. [Phase 1: Lexer 重构](#2-phase-1-lexer-重构)
3. [Phase 2: Parser 重构](#3-phase-2-parser-重构)
4. [Phase 3: ORGASM VM 重构](#4-phase-3-orgasm-vm-重构)
5. [Phase 4: ORGASM Compiler 重构](#5-phase-4-orgasm-compiler-重构)
6. [Phase 5: TypeChecker 重构](#6-phase-5-typechecker-重构)
7. [Phase 6: 跨组件优化](#7-phase-6-跨组件优化)
8. [测试策略](#8-测试策略)

---

## 1. 总体架构依赖关系

```
Lexer → Parser → AST
                   ↓
            TypeChecker → TypeInfo
                   ↓
         ORGASM Compiler → BytecodeModule
                   ↓
            ORGASM VM → Runtime
```

**依赖规则：** 从底层开始重构（Lexer → Parser → VM → Compiler → TypeChecker），每层完成后才进入下一层。这样每一步的测试都可以依赖下层的稳定性。

**当前代码规模：**

| 组件 | 主文件 | 行数 | 问题严重度 |
|------|--------|------|-----------|
| Lexer | `Lexer.cpp` | 837 | 中 |
| Parser | `ParserImpl.cpp` | 2583 | 中 |
| TypeChecker | `typecheck.cpp` | **9631** | **极高** |
| Compiler | `Compiler.cpp` | 2925 | 高 |
| VM | `VM.cpp` | 1546 | 高 |
| **合计** | | **17522** | |

---

## 2. Phase 1: Lexer 重构

### 2.1 当前问题

| 问题 | 影响 | 行数 |
|------|------|------|
| `operator_types` 与 `tokenType` 重复 | ~70 行冗余 | 35-67 |
| `Lexer::next()` 巨函数（175 行） | 可读性差 | 247-421 |
| Token 构造样板代码重复 12 次 | ~36 行冗余 | 多处 |
| `withStream` 使用 `std::function` | 每次调用堆分配 | 225-240 |
| `hex2dec()` 30-case switch | 可简化为 4 行 | 682-715 |
| `revert()` 跨行回退 bug | 列号计算错误 | 67-82 |
| `resetLineAndCol` O(n) 回退 | 性能问题 | 48-65 |
| `reserved.inc` 与关键字重复 | 9 处静默覆盖 | 207 |

### 2.2 新设计

#### Step 1: 消除重复的 `operator_types` 映射（低风险，-70 行）

**方案：** 将 `|>` 添加到 `tokenType` 映射，删除整个 `operator_types` 映射。重写 `is_operator()` 为 `Set<TokenType>` 查找。

```
Before:  operator_types (27 entries) + tokenType (130+ entries) + is_operator() 线性扫描
After:   统一 tokenType + operator_token_types Set<TokenType> + O(1) 查找
```

**测试：** 现有词法分析器测试应全部通过。新增测试验证 `|>` 正确词法化。

#### Step 2: 提取 Token 构造辅助函数（低风险，-36 行）

**方案：** 添加 `emitToken()` 辅助函数：

```cpp
auto emitToken(Vec<Token> &tokens, TokenType type, const Str &repr, TokenPosition pos) -> Token
{
    Token token{.type = type, .repr = repr, .position = pos};
    tokens.push_back(token);
    return token;
}
```

将 `Lexer::next()` 中 12 处重复的 Token 构造+推送+返回模式替换为单行调用。

**测试：** 现有测试全部通过。

#### Step 3: 拆分 `Lexer::next()` 巨函数（低风险，175→~80 行）

**方案：** 提取以下子函数：

```
Lexer::next()
├── lexWhitespace()      // 跳过空白
├── lexLineComment()     // // 和 # 注释
├── lexBlockComment()    // /* */ 注释
├── lexColon()           // :: 和 :=
├── lexDot()             // ... 和 ..= 和 ..
├── lexMinus()           // 负数字面量
└── lexOperator()        // 通用运算符（已有）
```

**新 `Lexer::next()` 结构：**
```cpp
auto Lexer::next() -> Token
{
    while (const char current = state.current())
    {
        if (isblank(current) || isspace(current)) { state.next(); continue; }
        if (isdigit(current)) return lexNumber(state, tokens);
        if (current == '"') return lexString(state, tokens);
        if (isalpha(current) || current == '_') return lexSymbol(state, tokens);
        if (current == '/' && state.lookAhead() == '/') { lexLineComment(); continue; }
        if (current == '/' && state.lookAhead() == '*') { lexBlockComment(); continue; }
        if (current == '#') { lexLineComment(); continue; }
        if (current == ':') return lexColon();
        if (current == '.') return lexDot();
        if (current == '-') return lexMinus();
        return lexOperator(state, tokens);
    }
    // EOF token
    return emitToken(tokens, TokenType::END_OF_FILE, "", {.line = state.line, .col = state.col});
}
```

**测试：** 现有测试全部通过。新增边界测试（注释嵌套、运算符组合）。

#### Step 4: 修复 `revert()` 跨行 bug（正确性修复）

**方案：** 将 `resetLineAndCol` 的 O(n) 全文扫描改为维护行起始偏移表：

```cpp
struct LexState {
    Str source;
    size_t index = 0;
    size_t line = 1;
    size_t col = 1;
    Vec<size_t> lineStarts = {0};  // 新增：行起始偏移表
};
```

- `nextLine()` 时记录当前 `index` 到 `lineStarts`
- `revert()` 时用 `std::lower_bound` 在 `lineStarts` 中二分查找，O(log n)

**测试：** 新增跨行回退的精确列号测试。

#### Step 5: 模板化 `withStream`（低风险，性能提升）

**方案：**
```cpp
template <typename F>
inline auto withStream(LexState &state, F &&func) -> Str
{
    auto current = state.index;
    try {
        Str result;
        Stream stream{result};
        func(state, stream);
        return result;
    } catch (LexException &) {
        state.revert(current);
        return "";
    }
}
```

用 `std::string` + `operator<<` 替代 `std::stringstream`。

**测试：** 现有测试全部通过。

#### Step 6: 简化 `hex2dec`（低风险，-30 行）

**方案：**
```cpp
auto hex2dec(char c) -> int
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw LexException("Unknown hex digit: " + std::string(1, c));
}
```

**测试：** 现有十六进制字面量测试全部通过。

### 2.3 预期成果

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| `Lexer.cpp` 行数 | 837 | ~650 |
| `Lexer::next()` 行数 | 175 | ~25 |
| 重复代码 | ~106 行 | ~0 |
| `revert()` 复杂度 | O(n) | O(log n) |
| `withStream` 堆分配 | 每次 1 次 | 0 次 |

---

## 3. Phase 2: Parser 重构

### 3.1 当前问题

| 问题 | 影响 | 行数 |
|------|------|------|
| 导出注册模式重复 8 次 | ~56 行冗余 | 100-300 |
| `typeDef()` 218 行处理 5 种形式 | 可读性差 | 1072-1290 |
| 逗号分隔列表模式重复 9 次 | ~40 行冗余 | 多处 |
| `postfixExpression` 与 `expression` 重复 | ~15 行冗余 | 372-411, 2028-2105 |
| `typeAnnotationBase` 深拷贝 ParseState | O(n) 性能 | 1641-1663 |
| `numberLiteral` 16-case switch | 可简化 | 2486-2548 |
| `ParseState.tokens` 按值存储 | 不必要的深拷贝 | parser.hpp:121 |

### 3.2 新设计

#### Step 1: 提取 `registerDefinition` 辅助函数（低风险，-56 行）

**方案：**
```cpp
void registerDefinition(ASTRef<Definition> def, bool exported)
{
    if (exported) {
        for (auto &&name : def->names()) {
            current_mod->exports.push_back(name);
        }
    }
    current_mod->definitions.push_back(std::move(def));
}
```

将 `parse()` 中 8 处重复的导出注册逻辑替换为单行调用。

**测试：** 现有模块导入导出测试全部通过。

#### Step 2: 提取 `parseSeparatedList` 辅助函数（低风险，-40 行）

**方案：**
```cpp
template <typename ParseFn>
auto parseSeparatedList(ParseFn parseItem, TokenType closing, TokenType separator = TokenType::COMMA)
    -> Vec<decltype(parseItem())>
{
    Vec<decltype(parseItem())> items;
    while (!expect(closing)) {
        items.push_back(parseItem());
        if (!expect(separator)) break;
        accept(separator);
    }
    accept(closing);
    return items;
}
```

**测试：** 现有测试全部通过。

#### Step 3: 拆分 `typeDef()` 为子函数（低风险，218→~50 行调度）

**方案：**
```cpp
auto typeDef() -> ASTRef<TypeDef>
{
    // 共享设置：名称、泛型参数、特化模式
    auto name = state->repr;
    accept(TokenType::ID);
    auto generics = genericParams();
    // ... 特化检测 ...

    // 根据首 token 分发
    if (expect(TokenType::EQ))     return typeAliasDef(name, generics);
    if (expect(TokenType::WRAPS))  return newTypeDef(name, generics);
    if (expect(TokenType::LEFT_CURLY)) return structuralTypeDef(name, generics);
    if (expect(TokenType::SEMICOLON))  return abstractTypeDef(name, generics);
    unexpected("Expected =, wraps, {, or ;");
}

auto typeAliasDef(Str name, Vec<ASTRef<GenericParam>> generics) -> ASTRef<TypeDef>;
auto taggedUnionDef(Str name, ...) -> ASTRef<TaggedUnionDef>;
auto newTypeDef(Str name, ...) -> ASTRef<NewTypeDef>;
auto structuralTypeDef(Str name, ...) -> ASTRef<TypeDef>;
```

同时提取 `parseVariant()` 辅助函数消除 tagged union 变体解析的重复。

**测试：** 现有类型定义测试全部通过。新增各类型形式的独立测试。

#### Step 4: 提取 `parseCommaSeparatedExprList`（低风险，-20 行）

**方案：** 从 `funCallExpression`、`idAccessorExpression`、`staticQualifiedTraitCallExpression` 中提取共享的参数列表解析逻辑。

**测试：** 现有测试全部通过。

#### Step 5: `ParseState` 改为非拥有引用（中风险，性能提升）

**方案：** 将 `ParseState.tokens` 从 `Vec<Token>` 改为 `const Vec<Token>*`（非拥有指针）或 `std::span<const Token>`。`ParseState` 的拷贝只复制 `index`，不再复制整个 token 向量。

```cpp
struct ParseState {
    const Vec<Token> &tokens;  // 改为引用
    size_t size;
    size_t index = 0;
    // ...
};
```

**风险：** 需要确保 `Parser` 拥有 token 向量的生命周期长于 `ParseState`。当前 `Parser::parse()` 已经满足这个条件。

**测试：** 现有测试全部通过。新增性能基准测试验证解析速度提升。

#### Step 6: 表驱动 `numberLiteral`（低风险，-30 行）

**方案：** 将 16-case switch 改为查找表：

```cpp
static const Map<TokenType, std::function<ASTRef<Expression>(const Str&)>> integralParsers = {
    {TokenType::NUMBER, [](const Str &s) { return createNode<IntegralValue<int32_t>>(std::stoi(s)); }},
    {TokenType::NUMBER_I8, [](const Str &s) { return createNode<IntegralValue<int8_t>>(std::stoi(s)); }},
    // ...
};
```

**测试：** 现有数字字面量测试全部通过。

### 3.3 预期成果

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| `ParserImpl.cpp` 行数 | 2583 | ~2300 |
| `parse()` 行数 | 200 | ~80 |
| `typeDef()` 行数 | 218 | ~50（调度）+ 4×40（子函数） |
| 重复代码 | ~116 行 | ~0 |
| `ParseState` 拷贝开销 | O(n) | O(1) |

---

## 4. Phase 3: ORGASM VM 重构

### 4.1 当前问题

| 问题 | 影响 | 行数 |
|------|------|------|
| `execute_slots()` 1261 行巨函数 | I-cache 不友好 | 284-1545 |
| 13 个死代码/冗余 opcode | 18% 死代码 | opcode.hpp |
| `clone_value_slot` 定义两次 | 代码重复 | 271, 302 |
| drop 逻辑实现 3 次 | 代码重复 | 129, 409, 483 |
| MOVE_* 3 个近似处理器 | 代码重复 | 884-917 |
| 函数名线性查找 3+ 次 | O(n) 性能 | 352, 1011, 1227 |
| 每次 push 都克隆值 | 性能瓶颈 | 306-311 |
| `sequence_slots` 动态分发 | 性能问题 | 493-576 |

### 4.2 新设计

#### Step 1: 清理死代码 opcode（低风险，-13 opcode）

**方案：**

删除从未使用 opcode：
- `ADD_F64`, `SUB_F64`, `MUL_F64`, `DIV_F64`, `NEG_F64`（5 个）
- `EQ_F64`, `LT_F64`, `GT_F64`（3 个）
- `ADD_I32`, `SUB_I32`, `MUL_I32`, `DIV_I32`（4 个）
- `MOD_I32`（1 个，重命名为 `MOD`）

重命名误导性 opcode：
- `EQ_I32` → `EQ`
- `LT_I32` → `LT`
- `GT_I32` → `GT`
- `NEG_I32` → `NEG`

**测试：** 全量测试通过。新增 opcode 覆盖率测试。

#### Step 2: 提取共享辅助函数（低风险，-100 行）

**方案：**

```cpp
// 替代两处 clone_value_slot lambda
static auto clone_slot(const RuntimeRef<StorageCell> &source, const Str &name = "stack")
    -> RuntimeRef<StorageCell>;

// 替代 3 处 drop 逻辑
auto drop_cell(const RuntimeRef<StorageCell> &cell, const BytecodeModule &module,
               const Str &functionName) -> void;

// 替代 3 处 MOVE_* 处理器
auto move_slot(Vec<RuntimeRef<StorageCell>> &source, size_t index,
               const Str &name) -> RuntimeRef<StorageCell>;
```

**测试：** 现有测试全部通过。

#### Step 3: 构建函数名索引（低风险，O(n)→O(1)）

**方案：** 在 `BytecodeModule` 上添加 `Map<Str, size_t> functionIndex`，在模块加载时构建。

```cpp
struct BytecodeModule {
    // ... 现有字段 ...
    Map<Str, size_t> functionIndex;  // 新增

    void buildIndex() {
        for (size_t i = 0; i < functions.size(); ++i) {
            functionIndex[functions[i].name] = i;
        }
    }
};
```

**测试：** 现有测试全部通过。

#### Step 4: 拆分 `execute_slots()` 为按类别分组的函数（中风险，1261→~200 调度）

**方案：** 将 82 个 case 拆分为按类别分组的处理函数：

```cpp
auto VM::execute_slots(...) -> RuntimeRef<StorageCell>
{
    while (call_stack.size() > baseFrameDepth) {
        OpCode op = static_cast<OpCode>(code[ip++]);
        switch (op) {
            // Stack ops
            case OpCode::PUSH_I8: case OpCode::PUSH_U8: /* ... */
                return handle_push(op);
            // Data access
            case OpCode::LOAD_LOCAL: case OpCode::STORE_LOCAL: /* ... */
                return handle_data_access(op);
            // Arithmetic
            case OpCode::ADD: case OpCode::SUB: /* ... */
                return handle_arithmetic(op);
            // Control flow
            case OpCode::JUMP: case OpCode::CALL: /* ... */
                return handle_control_flow(op);
            // Object/Array
            case OpCode::NEW_OBJECT: case OpCode::GET_PROPERTY: /* ... */
                return handle_object_ops(op);
            // ...
        }
    }
}
```

每个 `handle_*` 函数 50-150 行，独立可测试。

**测试：** 全量测试通过。可按类别新增单元测试。

#### Step 5: 消除整数类型的不必要克隆（中风险，性能提升）

**方案：** 对 `PUSH_I8`/`U8`/`I16`/`U16`/`I32`/`U32`/`I64`/`U64`/`F32`/`F64`/`PUSH_BOOL`，直接 push 到栈而不克隆：

```cpp
case OpCode::PUSH_I32: {
    int32_t val = std::bit_cast<int32_t>(read_le_bytes_checked<uint32_t>(code, ip));
    auto cell = numeral_cell_from_value<int32_t>(val);
    stack.push_back(cell);  // 直接 push，不 clone
    break;
}
```

**测试：** 全量测试通过。新增性能基准测试。

### 4.3 预期成果

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| `VM.cpp` 行数 | 1546 | ~1200 |
| `execute_slots()` 行数 | 1261 | ~200（调度）+ 6×150（处理函数） |
| 死代码 opcode | 13 个 | 0 |
| 函数名查找 | O(n) | O(1) |
| 整数 push 开销 | 堆分配+克隆 | 直接构造 |

---

## 5. Phase 4: ORGASM Compiler 重构

### 5.1 当前问题

| 问题 | 影响 | 行数 |
|------|------|------|
| `visit(Module*)` 616 行 | 过于复杂 | 245-861 |
| `visit(FunCallExpression*)` 311 行 | 过于复杂 | 1372-1683 |
| 函数参数设置代码重复 4 次 | ~60 行冗余 | 670, 720, 770, 932 |
| 函数名线性查找 7 次 | O(n) 性能 | 多处 |
| `collect_generic_function_instances` 210 行 | 应用 visitor | 961-1170 |
| `compile_function_body` 只用于泛型 | 未充分利用 | 912 |

### 5.2 新设计

#### Step 1: 统一使用 `compile_function_body`（低风险，-60 行）

**方案：** 将 `visit(Module*)` 中 4 处重复的函数参数设置代码统一到 `compile_function_body`：

```cpp
auto compile_function_body(Function &func, const Vec<ASTRef<Statement>> &body) -> void;
```

所有函数编译路径（普通函数、类型成员函数、impl 方法、泛型实例）都通过此函数。

**测试：** 全量测试通过。

#### Step 2: 拆分 `visit(Module*)`（中风险，616→~200 调度）

**方案：** 将多遍编译拆分为独立函数：

```cpp
void Compiler::visit(Module *mod) {
    collectExports(mod);           // 第一遍：收集导出
    compileTypeDefinitions(mod);   // 第二遍：编译类型定义
    compileFunctionDeclarations(mod); // 第三遍：编译函数声明
    compileFunctionBodies(mod);    // 第四遍：编译函数体
    compileImplBlocks(mod);        // 第五遍：编译 impl 块
}
```

**测试：** 全量测试通过。

#### Step 3: 拆分 `visit(FunCallExpression*)`（中风险，311→~100 调度）

**方案：** 提取以下子函数：

```cpp
auto compileRegularCall(FunCallExpression *expr) -> void;
auto compileGenericCall(FunCallExpression *expr) -> void;
auto compileTraitCall(FunCallExpression *expr) -> void;
auto compileTaggedUnionConstructor(FunCallExpression *expr) -> void;
auto compileFoldCall(FunCallExpression *expr) -> void;
```

**测试：** 全量测试通过。

#### Step 4: 构建函数名索引（与 VM 共享方案）

**方案：** 在 `Compiler` 中也使用 `BytecodeModule::functionIndex`。

**测试：** 全量测试通过。

#### Step 5: 用 visitor 替换 `collect_generic_function_instances`（中风险，-150 行）

**方案：** 创建 `GenericInstanceCollector` visitor，利用现有的 AST visitor 基础设施自动遍历 AST 树。

**测试：** 泛型实例化测试全部通过。

### 5.3 预期成果

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| `Compiler.cpp` 行数 | 2925 | ~2400 |
| `visit(Module*)` 行数 | 616 | ~200 |
| `visit(FunCallExpression*)` 行数 | 311 | ~100 |
| 重复代码 | ~60 行 | ~0 |

---

## 6. Phase 5: TypeChecker 重构

### 6.1 当前问题（最严重）

| 问题 | 影响 |
|------|------|
| TypeChecker 8353 行单文件 | **无法维护** |
| 276 个 dynamic_cast | **性能差、脆弱** |
| 12 个 inline static 成员 | **线程不安全** |
| `visit(FunCallExpression)` 1595 行 | **无法理解** |
| 5+ 条平行类型分发链 | **违反 OCP** |
| 255 个异常抛出点 | **错误处理混乱** |

### 6.2 新设计：分层架构

```
TypeChecker (协调层，~1000 行)
├── TypeEnvironment      (作用域管理)
├── OverloadResolver     (过载解析)
├── GenericInstantiator  (泛型单态化)
├── TypePatternMatcher   (类型模式匹配)
├── TraitRegistry        (trait/impl 管理)
├── ModuleCache          (模块缓存)
└── TypeCoercion         (类型转换)
```

#### Step 1: 提取 `TypeEnvironment`（中风险，~500 行）

**职责：** 管理作用域、绑定、移动状态、导入

```cpp
class TypeEnvironment {
    TypeIndex type_index;
    Map<Str, CheckingRef<TypeInfo>> locals;
    Map<Str, bool> movedBindings;
    Map<Str, Str> importAliases;
    Vec<Map<Str, CheckingRef<TypeInfo>>> scopeStack;

    void pushScope();
    void popScope();
    auto lookup(const Str &name) -> std::optional<CheckingRef<TypeInfo>>;
    void bind(const Str &name, CheckingRef<TypeInfo> type);
    void markMoved(const Str &name);
    auto isMoved(const Str &name) -> bool;
};
```

**测试：** 独立单元测试，不依赖解析器。

#### Step 2: 提取 `OverloadResolver`（中风险，~400 行）

**职责：** 函数过载解析、候选匹配、优先级排序

```cpp
class OverloadResolver {
    auto selectCandidate(const Vec<FunctionDef *> &candidates,
                         const Vec<CheckingRef<TypeInfo>> &argTypes,
                         const CheckingRef<TypeInfo> &expectedType)
        -> std::optional<FunctionDef *>;
    auto matchesSignature(const FunctionType *sig, const Vec<CheckingRef<TypeInfo>> &args) -> bool;
    auto specificity(const FunctionType *a, const FunctionType *b) -> int;
};
```

**测试：** 直接构造 FunctionType 和参数类型列表进行测试。

#### Step 3: 提取 `GenericInstantiator`（中风险，~500 行）

**职责：** 泛型函数/类型实例化、类型参数绑定提取

```cpp
class GenericInstantiator {
    auto extractBindings(const TypeAnnotation *param, const TypeInfo *arg)
        -> Map<Str, CheckingRef<TypeInfo>>;
    auto monomorphizeFunction(FunctionDef *generic, const Map<Str, CheckingRef<TypeInfo>> &bindings)
        -> FunctionDef *;
    auto instantiateType(GenericTypeDef *generic, const Vec<CheckingRef<TypeInfo>> &args)
        -> CheckingRef<TypeInfo>;
};
```

**测试：** 直接构造泛型定义和具体类型进行测试。

#### Step 4: 提取 `TraitRegistry`（中风险，~300 行）

**职责：** trait impl 注册、自动 trait 派生、impl 查找

```cpp
class TraitRegistry {
    Map<Str, Vec<TraitImplRecord>> impls_by_type;
    Set<Str> autoTraitNames;
    Set<Str> derivedTraitImplKeys;

    void registerImpl(const Str &typeName, const Str &traitName, ImplDef *impl);
    auto findImpl(const Str &typeName, const Str &traitName) -> ImplDef *;
    auto satisfiesTrait(const TypeInfo &type, const Str &traitName) -> bool;
};
```

**测试：** 独立单元测试。

#### Step 5: 统一类型分发链（中风险，-276 dynamic_cast）

**方案 A（推荐）：** 为 `TypeInfo` 添加虚方法：

```cpp
class TypeInfo {
    // 现有虚方法：tag(), match(), repr()
    virtual auto canonicalName() const -> Str;  // 新增：用于 name mangling
    virtual auto displayName() const -> Str;    // 新增：用于错误消息
    virtual auto sequenceElementType() const -> const TypeInfo *;  // 新增：序列元素类型
};
```

这将消除 `mangling.cpp`、`typeinfo.cpp`、`typecheck.cpp` 中的大部分 `dynamic_cast` 链。

**方案 B：** 创建 `TypeInfoVisitor`：

```cpp
class TypeInfoVisitor {
    virtual void visit(PrimitiveType *) = 0;
    virtual void visit(CustomizedType *) = 0;
    virtual void visit(FunctionType *) = 0;
    // ... 每种 TypeInfo 子类一个方法
};
```

**测试：** 现有类型检查测试全部通过。新增各虚方法的独立测试。

#### Step 6: 修复静态状态线程安全（中风险）

**方案：** 将 12 个 `inline static` 成员替换为 `PreludeContext`：

```cpp
struct PreludeContext {
    Map<Str, Vec<TypeAliasDef *>> typeAliasSpecializations;
    Map<Str, Vec<ConstDef *>> constPredicates;
    Map<Str, Vec<FunctionDef *>> constFunctions;
    Set<Str> autoTraits;
    Set<Str> derivedTraitImplKeys;
    Vec<ASTRef<ASTNode>> retainedImportAsts;

    static auto instance() -> PreludeContext & {
        static PreludeContext ctx;
        return ctx;
    }
};
```

用 `std::call_once` 保护初始化。

**测试：** 现有测试全部通过。

### 6.3 预期成果

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| `typecheck.cpp` 行数 | 9631 | ~3000（协调层） |
| 新增文件 | 0 | 6 个（每个子模块） |
| TypeChecker 成员数 | 20+ 实例 + 12 静态 | ~10 实例 + 0 静态 |
| dynamic_cast 数量 | 276 | <50 |
| `visit(FunCallExpression)` 行数 | 1595 | ~200 |

---

## 7. Phase 6: 跨组件优化

### 7.1 AST 所有权统一

**问题：** `ASTRef<T>` 有时是 `T*`（裸指针），有时是 `shared_ptr<T>`。`TypeAnnotation::genericArgs` 使用 `shared_ptr` 而其他字段使用裸指针。

**方案：** 统一为 `unique_ptr<T>`，用 `ASTRef<T> = unique_ptr<T>`。需要修改所有 `createNode` 和 `destroyast` 模式。

**影响：** 全局性变更，需要逐步推进。

### 7.2 Visitor 模式简化

**问题：** 45+ 纯虚方法，每添加一个 AST 节点需改 6 个文件。

**方案：** 引入 CRTP 默认 visitor：

```cpp
template <typename Derived>
class DefaultVisitor : public AstVisitor {
    void visit(FunctionDef *node) override { static_cast<Derived*>(this)->visitDefault(node); }
    // ... 所有 visit 方法都有默认实现
};
```

新 visitor 只需覆盖关心的方法。

### 7.3 错误处理统一

**问题：** 混用 `RuntimeException`、`std::logic_error`、`std::out_of_range` 等。

**方案：** 建立统一的异常层次：

```
NGException
├── LexException (词法)
├── ParseException (语法)
├── TypeCheckingException (类型)
├── RuntimeException (运行时)
│   ├── VMException (VM)
│   └── AssertionException (断言)
└── InternalException (内部错误，不应暴露给用户)
```

---

## 8. 测试策略

### 8.1 每步测试要求

| 步骤类型 | 测试要求 |
|---------|---------|
| 提取辅助函数 | 现有测试全部通过 + 新增辅助函数的单元测试 |
| 拆分大函数 | 现有测试全部通过 + 按子函数新增集成测试 |
| 性能优化 | 现有测试全部通过 + 性能基准测试验证提升 |
| Bug 修复 | 现有测试全部通过 + 新增回归测试 |
| 架构重构 | 现有测试全部通过 + 新模块的独立单元测试 |

### 8.2 测试覆盖率目标

| 组件 | 当前测试 | 目标新增 |
|------|---------|---------|
| Lexer | 16 个测试文件 | +10 边界测试 |
| Parser | 16 个测试文件 | +15 结构验证测试 |
| TypeChecker | 16 个测试文件 | +20 子模块单元测试 |
| Compiler | 5 个测试文件 | +10 按类别测试 |
| VM | 5 个测试文件 | +15 边界检查和性能测试 |

### 8.3 提交规范

每步提交格式：
```
refactor(<component>): <简短描述>

- 具体变更 1
- 具体变更 2

Tests: <测试结果>
```

---

## 附录：依赖关系图

```
Phase 1 (Lexer) ──→ Phase 2 (Parser) ──→ Phase 5 (TypeChecker)
                                              ↓
                                    Phase 4 (Compiler) ←── Phase 3 (VM)
                                              ↓
                                    Phase 6 (跨组件优化)
```

**关键约束：**
- Phase 2 依赖 Phase 1（Parser 使用 Lexer 的 token 输出）
- Phase 4 依赖 Phase 3（Compiler 生成 VM 执行的字节码）
- Phase 5 依赖 Phase 2（TypeChecker 遍历 Parser 生成的 AST）
- Phase 6 依赖所有其他 Phase（跨组件变更需要各组件稳定）
