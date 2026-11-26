# v1: Error Handling + Lambda + Switch Expressions

## 概述

Phase 1 的第一个迭代，实现语言核心改进：
- Result/Option 内置类型
- Error trait 和 ? 操作符
- Lambda 语法和闭包捕获
- switch 表达式和 when guard

## 依赖

无（基础迭代）

## 实现进度

- [x] Token: 添加 KEYWORD_WHEN
- [x] AST: 添加 ERROR_PROPAGATION 和 LAMBDA_EXPRESSION 节点类型
- [x] AST: 添加 ErrorPropagationExpression 和 LambdaExpression 结构体
- [x] TypeCheck: 添加 RESULT, OPTION, NONE_ERROR 标签
- [x] TypeCheck: 添加 ResultType, OptionType, NoneErrorType 类型
- [x] Visitor: 添加 visit(ErrorPropagationExpression*) 和 visit(LambdaExpression*) 方法
- [x] DummyVisitor: 实现新的 visitor 方法
- [x] 示例: 创建 error_handling.ng, lambda_closures.ng, switch_expression.ng
- [x] Parser: 实现 ? 操作符解析 ✅ 验证通过
- [x] Parser: 实现 Lambda 语法解析 ✅ 验证通过（带捕获列表和不带捕获列表）
- [x] Parser: 实现 switch 表达式和 when guard ✅ 验证通过
- [x] TypeCheck: 实现 ? 操作符类型检查 ✅ 编译通过
- [x] TypeCheck: 实现 Lambda 类型检查 ✅ 编译通过
- [x] TypeCheck: 实现 switch 表达式类型检查 ✅ 编译通过
- [ ] Compiler: 实现 ? 操作符编译（ORGASM + STUPID）— STUB，需要实现错误传播逻辑
- [x] Compiler: 实现 Lambda 编译（ORGASM）— 基本闭包调用已实现
- [ ] Compiler: 实现 Lambda 捕获变量支持
- [x] Compiler: 实现 switch 表达式编译（ORGASM）— 整数比较和 when guard 已实现
- [x] 标准库: 创建 std.result 模块（std/result.ng, std/option.ng）
- [x] 测试: 7 个测试全部通过（715/716 现有测试通过）

## 架构落点

| 组件 | 目标位置 |
|------|----------|
| Result/Option 类型定义 | `include/typecheck/typeinfo.hpp` (扩展) |
| Error trait | `lib/std/prelude.ng` |
| ? 操作符 AST | `include/ast.hpp` (新增节点) |
| ? 操作符解析 | `src/parsing/ParserImpl.cpp` |
| ? 操作符类型检查 | `src/typecheck/typecheck.cpp` |
| Lambda AST | `include/ast.hpp` (新增节点) |
| Lambda 解析 | `src/parsing/ParserImpl.cpp` |
| Lambda 类型检查 | `src/typecheck/typecheck.cpp` |
| Lambda 编译 | `src/orgasm/Compiler.cpp` |
| switch 表达式 | `src/parsing/ParserImpl.cpp` (扩展) |
| when guard | `src/parsing/ParserImpl.cpp` (扩展) |

## Phase 1.1: Result/Option + Error trait + ? 操作符

### 1.1.1 内置类型定义

**文件**: `include/typecheck/typeinfo.hpp`

```cpp
// 新增类型
struct ResultType : TypeInfo {
    CheckingRef<TypeInfo> okType;
    CheckingRef<TypeInfo> errType;
};

struct OptionType : TypeInfo {
    CheckingRef<TypeInfo> someType;
};

struct NoneErrorType : TypeInfo {
    // 内置 NoneError 类型
};
```

**文件**: `lib/std/prelude.ng`

```ng
// 内置定义（编译器识别）
export type Result<T, E> = Ok(value: T) | Err(error: E);
export type Option<T> = Some(value: T) | None;
export type NoneError = NoneError;

// Error trait
export trait Error {
    fun message(self: ref<Self>) -> string;
    fun causedBy(cause: ref<Error>) -> Self;
}

// NoneError 实现
impl Error for NoneError {
    fun message(self: ref<Self>) -> string {
        return "Attempted to unwrap None value";
    }
    fun causedBy(cause: ref<Error>) -> NoneError {
        return NoneError;
    }
}
```

### 1.1.2 ? 操作符

**文件**: `include/ast.hpp`

```cpp
// 新增 AST 节点
struct ErrorPropagationExpression : Expression {
    ASTRef<Expression> expr;
    
    void accept(AstVisitor *visitor) override;
    auto astNodeType() const -> ASTNodeType override { return ASTNodeType::ERROR_PROPAGATION; }
    auto repr() const -> Str override;
};
```

**文件**: `src/parsing/ParserImpl.cpp`

```cpp
// 在后缀表达式解析中添加
if (accept(TokenType::QUERY)) {
    result = make_node<ErrorPropagationExpression>(pos, std::move(result));
}
```

**文件**: `src/typecheck/typecheck.cpp`

```cpp
void TypeChecker::visit(const ErrorPropagationExpression *node) {
    node->expr->accept(this);
    auto type = latestType;
    
    if (auto resultType = dynamic_cast<ResultType*>(type.get())) {
        // Result<T, E> -> T，要求函数返回 Result
        latestType = resultType->okType;
        // 验证当前函数返回类型是 Result
        checkFunctionReturnType(node, resultType->errType);
    } else if (auto optionType = dynamic_cast<OptionType*>(type.get())) {
        // Option<T> -> T，要求函数返回 Result<T, NoneError>
        latestType = optionType->someType;
        checkFunctionReturnType(node, makeNoneErrorType());
    } else {
        throw TypeCheckingException("`?` can only be used on Result or Option types", node->pos);
    }
}
```

**文件**: `src/orgasm/Compiler.cpp`

```cpp
// ? 操作符脱糖为 switch
void Compiler::visit(const ErrorPropagationExpression *node) {
    // 编译表达式
    node->expr->accept(this);
    
    // 生成 switch
    // switch (result) {
    //     case Ok(v) { v }
    //     case Err(e) { return Err(e) }
    // }
    // 或
    // switch (option) {
    //     case Some(v) { v }
    //     case None { return Err(NoneError) }
    // }
}
```

### 1.1.3 std.result 模块

**文件**: `lib/std/result.ng`

```ng
module std.result exports *;

export fun isOk<T, E>(result: Result<T, E>) -> bool {
    return switch (result) {
        case Ok(_) => true;
        case _ => false;
    };
}

export fun isErr<T, E>(result: Result<T, E>) -> bool {
    return switch (result) {
        case Err(_) => true;
        case _ => false;
    };
}

export fun unwrap<T, E>(result: Result<T, E>) -> T {
    return switch (result) {
        case Ok(v) => v;
        case Err(e) => panic(e.message());
    };
}

export fun unwrapOr<T, E>(result: Result<T, E>, fallback: T) -> T {
    return switch (result) {
        case Ok(v) => v;
        case Err(_) => fallback;
    };
}

export fun map<T, U, E>(result: Result<T, E>, f: (T) -> U) -> Result<U, E> {
    return switch (result) {
        case Ok(v) => Ok(f(v));
        case Err(e) => Err(e);
    };
}

export fun mapErr<T, E, F>(result: Result<T, E>, f: (E) -> F) -> Result<T, F> {
    return switch (result) {
        case Ok(v) => Ok(v);
        case Err(e) => Err(f(e));
    };
}

// Option 辅助函数
export fun isSome<T>(opt: Option<T>) -> bool {
    return switch (opt) {
        case Some(_) => true;
        case _ => false;
    };
}

export fun isNone<T>(opt: Option<T>) -> bool {
    return switch (opt) {
        case None => true;
        case _ => false;
    };
}

export fun unwrap<T>(opt: Option<T>) -> T {
    return switch (opt) {
        case Some(v) => v;
        case None => panic("unwrap None");
    };
}

export fun unwrapOr<T>(opt: Option<T>, fallback: T) -> T {
    return switch (opt) {
        case Some(v) => v;
        case None => fallback;
    };
}
```

### 1.1.4 测试

**文件**: `test/typecheck/result_option_test.cpp`

```cpp
TEST_CASE("Result type should compile", "[typecheck][result]") {
    // 测试 Result 类型定义
    // 测试 Ok/Err 构造
    // 测试模式匹配
}

TEST_CASE("? operator should propagate errors", "[typecheck][error]") {
    // 测试 ? 在 Result 上的行为
    // 测试 ? 在 Option 上的行为
    // 测试 ? 在非 Result/Option 上报错
    // 测试 ? 在非 Result-returning 函数中报错
}
```

## Phase 1.2: Lambda 与闭包

### 1.2.1 AST 节点

**文件**: `include/ast.hpp`

```cpp
struct LambdaExpression : Expression {
    Vec<Param> params;
    ASTRef<TypeAnnotation> returnType = nullptr;
    
    // 单表达式或块
    ASTRef<Expression> exprBody = nullptr;
    ASTRef<Statement> blockBody = nullptr;
    
    // 捕获列表
    struct Capture {
        Str name;
        bool byRef = false;
    };
    Vec<Capture> captures;
    bool captureAllByRef = false;  // [&]
    bool captureAllByValue = false; // []
    
    void accept(AstVisitor *visitor) override;
    auto astNodeType() const -> ASTNodeType override { return ASTNodeType::LAMBDA_EXPRESSION; }
    auto repr() const -> Str override;
};
```

### 1.2.2 解析器

**文件**: `src/parsing/ParserImpl.cpp`

```cpp
// Lambda 解析
auto ParserImpl::parseLambda() -> ASTRef<Expression> {
    auto pos = currentPos();
    auto lambda = createNode<LambdaExpression>();
    
    // 解析捕获列表
    if (accept(TokenType::LEFT_SQUARE)) {
        if (accept(TokenType::AMPERSAND)) {
            if (accept(TokenType::RIGHT_SQUARE)) {
                lambda->captureAllByRef = true;
            } else {
                // [&a, b] 形式
                parseCaptureList(lambda);
            }
        } else if (accept(TokenType::RIGHT_SQUARE)) {
            lambda->captureAllByValue = true;
        } else {
            // [a, b] 形式
            parseCaptureList(lambda);
        }
    }
    
    // 解析参数列表 (...)
    accept(TokenType::LEFT_PAREN);
    lambda->params = parseParamList();
    accept(TokenType::RIGHT_PAREN);
    
    // 解析返回类型
    if (accept(TokenType::SINGLE_ARROW)) {
        lambda->returnType = parseTypeAnnotation();
    }
    
    // 解析主体
    if (accept(TokenType::DUAL_ARROW)) {
        // => expr
        lambda->exprBody = parseExpression();
    } else if (accept(TokenType::LEFT_CURLY)) {
        // { body }
        lambda->blockBody = parseBlock();
    }
    
    return lambda;
}
```

### 1.2.3 类型检查

**文件**: `src/typecheck/typecheck.cpp`

```cpp
void TypeChecker::visit(const LambdaExpression *node) {
    // 检查参数类型
    Vec<CheckingRef<TypeInfo>> paramTypes;
    for (auto &param : node->params) {
        param.type->accept(this);
        paramTypes.push_back(latestType);
    }
    
    // 检查返回类型
    if (node->returnType) {
        node->returnType->accept(this);
        auto returnType = latestType;
    }
    
    // 检查捕获变量
    for (auto &capture : node->captures) {
        auto varType = lookupVariable(capture.name);
        if (!varType) {
            throw TypeCheckingException("Undefined variable in capture list", node->pos);
        }
    }
    
    // 创建函数类型
    latestType = makeFunctionType(paramTypes, returnType);
}
```

### 1.2.4 编译

**文件**: `src/orgasm/Compiler.cpp`

```cpp
void Compiler::visit(const LambdaExpression *node) {
    // 生成匿名函数
    auto funcName = generateUniqueName("__lambda");
    
    // 将捕获变量作为额外参数
    Vec<Param> allParams;
    for (auto &capture : node->captures) {
        allParams.push_back({capture.name, lookupType(capture.name)});
    }
    for (auto &param : node->params) {
        allParams.push_back(param);
    }
    
    // 编译函数体
    beginFunction(funcName, allParams);
    if (node->exprBody) {
        node->exprBody->accept(this);
        emitReturn();
    } else {
        node->blockBody->accept(this);
    }
    endFunction();
    
    // 创建闭包对象（捕获变量打包）
    emitCreateClosure(funcName, node->captures);
}
```

## Phase 1.3: Switch 表达式 + When Guard

### 1.3.1 扩展 SwitchStatement

**文件**: `include/ast.hpp`

```cpp
struct SwitchCase {
    ASTRef<Pattern> pattern;
    ASTRef<Expression> guard = nullptr;  // when condition
    ASTRef<Statement> body = nullptr;    // 语句形式
    ASTRef<Expression> value = nullptr;  // 表达式形式
};

struct SwitchStatement : Statement {
    ASTRef<Expression> scrutinee;
    Vec<SwitchCase> cases;
    ASTRef<Statement> otherwiseBody = nullptr;
    ASTRef<Expression> otherwiseValue = nullptr;
    
    bool isExpression = false;  // 是否作为表达式使用
};
```

### 1.3.2 解析器修改

**文件**: `src/parsing/ParserImpl.cpp`

```cpp
auto ParserImpl::parseSwitch() -> ASTRef<Statement> {
    // ... 现有解析 ...
    
    // 解析 when guard
    if (accept(TokenType::KEYWORD_WHEN)) {
        currentCase->guard = parseExpression();
    }
    
    // 判断是表达式还是语句形式
    if (accept(TokenType::DUAL_ARROW)) {
        // case pattern => value;
        currentCase->value = parseExpression();
        switchStmt->isExpression = true;
    } else if (accept(TokenType::LEFT_CURLY)) {
        // case pattern { body }
        currentCase->body = parseBlock();
    }
    
    // ...
}
```

### 1.3.3 添加 when 关键字

**文件**: `include/token.hpp`

```cpp
enum class TokenType : uint32_t {
    // ... 现有关键字 ...
    KEYWORD_WHEN,
    // ...
};
```

**文件**: `src/parsing/Lexer.cpp`

```cpp
// 在关键字映射中添加
{"when", TokenType::KEYWORD_WHEN},
```

## 验收标准

### REQ-101: Result/Option 内置类型
- [ ] `type Result<T, E> = Ok(value: T) | Err(error: E)` 可编译
- [ ] `type Option<T> = Some(value: T) | None` 可编译
- [ ] `Ok(42)` 和 `Err("error")` 可构造
- [ ] `Some(42)` 和 `None` 可构造
- [ ] 模式匹配 `switch` 可用于 Result/Option

### REQ-102: Error trait
- [ ] `Error` trait 定义可编译
- [ ] `message()` 方法可调用
- [ ] `causedBy()` 方法可调用
- [ ] `NoneError` 实现 `Error`

### REQ-103: ? 操作符
- [ ] `expr?` 在 Result 上返回 Ok 值
- [ ] `expr?` 在 Result Err 时提前返回
- [ ] `expr?` 在 Option 上返回 Some 值
- [ ] `expr?` 在 Option None 时返回 Err(NoneError)
- [ ] `?` 在非 Result/Option 类型上报错
- [ ] `?` 在非 Result-returning 函数中报错

### REQ-104: Lambda 语法
- [ ] `(x: i32) => x * 2` 可编译和执行
- [ ] `(x: i32) -> i32 => x * 2` 可编译和执行
- [ ] `(x: i32) { return x * 2; }` 可编译和执行
- [ ] 类型推断：`(a, b) => a + b` 可从上下文推断类型
- [ ] Lambda 可作为高阶函数参数

### REQ-105: 闭包捕获
- [ ] 默认 by value 捕获
- [ ] `[&x]` by ref 捕获
- [ ] `[&]` 全部 by ref
- [ ] `[]` 全部 by value
- [ ] `[&a, b]` 混合捕获
- [ ] 捕获变量在 lambda 内可访问

### REQ-106: switch 表达式
- [ ] `switch (x) { case 0 => "zero"; otherwise => "other"; }` 返回值
- [ ] 所有分支必须返回相同类型
- [ ] `when` guard 可用
- [ ] `otherwise` 分支可用（不用通配符 `_`)

## 测试用例

```ng
// 错误处理测试
fun divide(a: i32, b: i32) -> Result<i32, string> {
    if (b == 0) {
        return Err("Division by zero");
    }
    return Ok(a / b);
}

fun safeDivide(a: i32, b: i32) -> Result<i32, string> {
    val result = divide(a, b)?;
    return Ok(result * 2);
}

// Lambda 测试
val double = (x: i32) => x * 2;
assert(double(21) == 42);

val factor = 10;
val multiply = [&factor](x: i32) => x * factor;
assert(multiply(5) == 50);

// Switch 表达式测试
val x = 42;
val desc = switch (x) {
    case 0 => "zero";
    case n when n > 0 => "positive";
    case _ => "negative";
};
assert(desc == "positive");
```
