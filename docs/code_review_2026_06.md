# NG 编译器代码评审报告

> 评审日期：2026-06-03
> 评审范围：全仓库（解析器、AST、类型检查、运行时、ORGASM 字节码虚拟机、测试、构建系统）
> 当前状态：267/267 测试通过，1429 个断言

---

## 目录

1. [执行摘要](#1-执行摘要)
2. [严重问题（Critical）](#2-严重问题critical)
3. [高危问题（High）](#3-高危问题high)
4. [中等问题（Medium）](#4-中等问题medium)
5. [低优先级问题（Low）](#5-低优先级问题low)
6. [测试与基础设施](#6-测试与基础设施)
7. [改进建议路线图](#7-改进建议路线图)

---

## 1. 执行摘要

NG 是一个用 C++23 实现的静态类型多范式编程语言，编译器管线为经典的 Lexer → Parser → AST → TypeChecker → Interpreter/Compiler/VM 架构。代码整体结构清晰，模块划分合理，但存在以下核心问题：

| 类别 | 严重 | 高危 | 中等 | 低 |
|------|------|------|------|-----|
| 正确性 / 内存安全 | 5 | 3 | 2 | 0 |
| 运行时 / VM 安全 | 5 | 4 | 4 | 2 |
| 类型系统健全性 | 0 | 3 | 2 | 1 |
| 代码设计 / 可维护性 | 0 | 0 | 8 | 3 |
| 构建 / CI / 测试 | 0 | 0 | 6 | 4 |

**最高优先级修复项：**
1. 编译器为 PUSH_I8/PUSH_U8/PUSH_I16/PUSH_U16 发射了错误的字节数（字节码损坏）
2. VM 对字节码无任何边界检查（畸形字节码可导致任意崩溃）
3. ASTNodeType 枚举存在重复值（RangeExpression 与 TaggedValueExpression 冲突）
4. 词法分析器对未终止字符串无限循环
5. `Untyped::match()` 匹配所有类型，掩盖类型检查错误

---

## 2. 严重问题（Critical）

### 2.1 编译器发射错误字节数 — 字节码损坏

**文件：** `src/orgasm/Compiler.cpp:2645-2648`

```cpp
void Compiler::visit(ast::IntegralValue<int8_t> *intVal) {
    emit(OpCode::PUSH_I8);
    emit_i32(static_cast<int32_t>(intVal->value));  // 发射 4 字节
}
```

编译器为 `PUSH_I8`/`PUSH_U8`（VM 读取 1 字节）和 `PUSH_I16`/`PUSH_U16`（VM 读取 2 字节）发射了 4 字节的 `emit_i32`。多余字节会被 VM 静默解析为下一条指令的操作码，导致字节码损坏。

**修复：** `PUSH_I8`/`PUSH_U8` 应发射 1 字节，`PUSH_I16`/`PUSH_U16` 应发射 2 字节。

> ✅ **已修复** (2026-06-03) — `src/orgasm/Compiler.cpp:2645-2648`：将 `emit_i32` 改为 `emit_u8`（i8/u8）和 `emit_u16`（i16/u16）。新增 4 个测试用例覆盖 i8/u8/i16/u16 字面量编译和执行。全量测试 674/674 通过。

### 2.2 VM 无字节码边界检查

**文件：** `src/orgasm/VM.cpp`（贯穿全文）

VM 执行循环中 `read_le_bytes()`、`code[ip++]`、`read_u16()` 等操作均无边界检查：

- `VM.cpp:26-34` — `read_le_bytes` 在 `ip` 接近 `code.size()` 时越界读取
- `VM.cpp:588,612,682,1296` — 单字节操作数无边界检查
- `VM.cpp:677` — `LOAD_STR` 的 u16 索引不校验 `strings.size()`
- `VM.cpp:898-903` — `CALL` 的函数索引不校验 `functions.size()`
- `VM.cpp:895` — `DUP` 在空栈上执行 `stack.back()` 导致 UB
- `VM.cpp:1394-1395` — `JUMP` 的负目标地址转为巨大 `size_t`

**影响：** 任何畸形字节码（文件损坏、恶意输入、编译器 bug）都可导致越界读取、崩溃或任意行为。

**修复：** 在 `read_le_bytes` 和所有索引访问处添加边界检查，对越界情况抛出 `RuntimeException`。

> ✅ **已修复** (2026-06-03) — `src/orgasm/VM.cpp`：
> - 新增 `read_byte_checked()` 和 `read_le_bytes_checked()` 边界检查辅助函数
> - 所有 `read_le_bytes` 调用替换为 `read_le_bytes_checked` 版本
> - 所有 `code[ip++]` 单字节读取替换为 `read_byte_checked`
> - `DUP`/`SWITCH_TAG` 添加空栈检查
> - `LOAD_STR`/`LOAD_CONST`/`CALL`/`CALL_IMPORT`/`NATIVE_CALL`/`INVOKE_MEMBER`/`MAKE_TRAIT_REF`/`WRAP_NEWTYPE`/`CONSTRUCT_TAGGED` 添加索引边界校验
> - `JUMP`/`JUMP_IF_FALSE`/`SWITCH_TAG` 添加跳转目标地址校验
> - 新增 6 个边界检查测试用例（截断字节码、越界索引、空栈 DUP、负地址跳转等）
> - 全量测试 686/686 通过

### 2.3 ASTNodeType 枚举值冲突

**文件：** `include/ast.hpp:63-64,96,100`

```cpp
RANGE_EXPRESSION = 0x215,
TAGGED_VALUE_EXPRESSION = 0x215,  // 冲突！
FROM_END_INDEX_EXPRESSION = 0x216,
PACK_EXPRESSION = 0x216,          // 冲突！
```

`RangeExpression` 和 `TaggedValueExpression` 共享同一枚举值，`FromEndIndexExpression` 和 `PackExpression` 亦然。任何基于 `astNodeType()` 的 `switch` 或 `==` 比较都会混淆这些类型。

> ✅ **已修复** (2026-06-03) — `include/ast.hpp`：将 `TAGGED_VALUE_EXPRESSION` 改为 `0x219`，`PACK_EXPRESSION` 改为 `0x21A`。新增编译时测试验证枚举值唯一性。全量测试 675/675 通过。

### 2.4 未终止字符串导致无限循环

**文件：** `src/parsing/Lexer.cpp:765`

```cpp
while (state.current() != '"')
```

当源文件包含未终止的字符串字面量时，`state.current()` 在 EOF 处返回 `'\0'`（不等于 `'"'`），而 `state.next()` 在 EOF 处为 no-op，导致词法分析器永久挂起。

**修复：** 在循环体内添加 `if (state.eof()) throw LexException("Unterminated string literal");`。

> ✅ **已修复** (2026-06-03) — `src/parsing/Lexer.cpp:765`：将循环条件改为 `while (!state.eof() && state.current() != '"')`，并在循环后检查 EOF 抛出描述性异常。新增 3 个测试用例覆盖未终止字符串和空字符串场景。全量测试 676/676 通过。

### 2.5 AST 内存泄漏

**问题 A — PropertyDef 缺少析构函数：**
`include/ast.hpp:1123-1139` — `PropertyDef` 持有 `ASTRef<TypeAnnotation>` 但未定义析构函数，所有其他持有 `ASTRef<>` 的 AST 节点都在析构函数中调用 `destroyast()`。

**问题 B — NewTypeDef 泄漏 genericParams：**
`src/ast/ast.cpp:961-964` — `NewTypeDef::~NewTypeDef()` 只销毁 `wrappedType`，不销毁 `genericParams` 中的元素。对 `type Foo<T> wraps T;` 这样的泛型新类型会导致内存泄漏。

**问题 C — ReturnStatement::repr() 空指针解引用：**
`src/ast/ast.cpp:270` — `return this->expression->repr()` 对 `expression == nullptr`（裸 `return;` 语句）会崩溃。同样的问题出现在 `SimpleStatement::repr()`（行 371）和 `ValDefStatement::repr()`（行 449）。

> ✅ **已修复** (2026-06-03)
> - `include/ast.hpp`：为 `PropertyDef` 添加析构函数声明
> - `src/ast/ast.cpp`：添加 `PropertyDef::~PropertyDef()` 实现，销毁 `typeAnnotation`
> - `src/ast/ast.cpp`：修复 `NewTypeDef::~NewTypeDef()` 遍历并销毁 `genericParams`
> - `src/ast/ast.cpp`：为 `ReturnStatement::repr()`、`SimpleStatement::repr()`、`ValDefStatement::repr()` 添加 null 检查
> - 新增 3 个测试用例覆盖析构函数和 repr 空指针场景。全量测试 679/679 通过。

---

## 3. 高危问题（High）

### 3.1 整数溢出为未定义行为

**文件：** `src/runtime/runtime_numerals.hpp:132-176`

所有整数算术操作（加减乘除取模）均无溢出检查：

```cpp
return numeral_cell_from_value<T>(read_inline_cell_bytes<T>(self) + read_numeric_cell_as<T>(other));
```

`int8_t(127) + int8_t(1)` 是 C++ 未定义行为。`Negation` 对 `int8_t(-128)` 亦然（行 102）。

**修复：** 使用 `__builtin_add_overflow` 或在运算前检查边界。

### 3.2 Untyped 匹配所有类型 — 类型系统健全性漏洞

**文件：** `src/typecheck/typeinfo.cpp:101-104`

```cpp
auto Untyped::match(const TypeInfo &other) const -> bool { return true; }
```

当表达式的类型解析失败回退到 `Untyped` 时，它会静默匹配任何期望类型，掩盖真实的类型错误。

**修复：** `Untyped::match()` 应只匹配 `Untyped`，使用显式错误传播。

### 3.3 CustomizedType::match() 忽略模块 ID

**文件：** `src/typecheck/typeinfo.cpp:118-129`

```cpp
return name == otherCustom->name;  // 只比较名称，不比较 moduleId
```

不同模块中同名的类型会被视为相同类型，导致多模块程序中的假阳性类型匹配。

### 3.4 match() 语义不对称

`Untyped::match(X)` 返回 `true` 对所有 X，但 `X.match(Union<X, Y>)` 返回 `false`。类型检查结果取决于哪边是"期望"类型、哪边是"实际"类型，这种不对称性是脆弱的。

### 3.5 withStream 静默吞没所有异常

**文件：** `src/parsing/Lexer.cpp:225-240`

```cpp
catch (std::exception &) {
    state.revert(current);
    return "";  // 所有异常都被静默吞没
}
```

意图是只捕获 `LexException` 用于回溯，但实际上捕获了所有 `std::exception` 子类，包括内部 bug 产生的 `runtime_error`。

**修复：** 只捕获 `LexException`。

> ✅ **已修复** (2026-06-03) — `src/parsing/Lexer.cpp:235`：将 `catch (std::exception &)` 改为 `catch (LexException &)`，确保只有词法分析异常被捕获用于回溯，其他异常（表明 bug）会正常传播。新增 1 个测试验证回溯行为。全量测试 680/680 通过。

### 3.6 ParserImpl 按值复制整个 ParseState

**文件：** `src/parsing/ParserImpl.cpp:66`

```cpp
explicit ParserImpl(ParseState &state) : state(state) {}  // 按值存储
```

`ParseState` 包含完整的 token 向量，每次创建 `ParserImpl` 都会深拷贝整个向量。应改为引用 `ParseState &state;`。

### 3.7 异常用作控制流

**文件：** `src/runtime/runtime.hpp:47-58`

`NextIteration` 和 `StopIteration` 继承自 `std::exception` 并用于循环控制流。异常在 C++ 中用于预期控制流是反模式——应使用哨兵返回值或枚举。

---

## 4. 中等问题（Medium）

### 4.1 运行时 / VM

| 问题 | 位置 | 描述 |
|------|------|------|
| 栈推送过度克隆 | `VM.cpp:281-286` | 每次栈推送都深克隆值树，O(n) 开销 |
| `insert(begin())` O(n²) | `VM.cpp:901,975,1080` | 反向弹出时在向量头部插入，应改用 `reserve`+`push_back`+`reverse` |
| 线性函数查找 | `VM.cpp:326-335` | 按名称查找函数是 O(n) 线性扫描，应建索引 |
| GC 非线程安全 | `managed_heap.cpp:17-21` | 全局静态堆状态无同步原语 |
| GC 可重入 VM | `VM.cpp:148` | 终结器调用 `execute_slots` 重新进入 VM |
| `shared_ptr<void>` 类型不安全 | `runtime.hpp:241` | 运行时状态使用 `shared_ptr<void>`，错误转换为 UB |
| 异常类型不一致 | 多处 | 混用 `RuntimeException`、`std::logic_error`、`std::out_of_range` 等 |
| VM 错误无源码位置 | `VM.cpp:1473` | 错误消息只有字节码偏移，无源文件/行号信息 |

### 4.2 类型系统 / AST

| 问题 | 位置 | 描述 |
|------|------|------|
| TypeChecker 上帝类 | `typecheck.cpp` ~8350 行 | 应拆分为过载解析、泛型单态化、借用检查等子模块 |
| 静态状态非线程安全 | `typecheck.cpp:1046-1088` | `inline static` 映射表无同步，不支持并发类型检查 |
| `const_cast` 移除 const | `typecheck.cpp:1125` | 对 `const TypeAnnotation*` 调用非 const `accept()` |
| 类型分发链重复 | `mangling.cpp`, `typeinfo.cpp`, `typecheck.cpp` | 7+ 个函数各自用 `dynamic_cast` 链分发 15+ 种类型 |
| 234 个 `dynamic_cast` | `typecheck.cpp` | RTTI 开销在热路径中累积 |
| Visitor 模式膨胀 | `visitor.hpp` | 45+ 纯虚方法，每添加一个 AST 节点需改 6 个文件 |
| PrimitiveType 允许隐式收窄 | `PrimitiveType.cpp:189-216` | `i64.match(i8)` 返回 true，静默截断 |
| mixed ownership | `ast.hpp:271` | `genericArgs` 用 `shared_ptr` 而其他用裸指针 |

### 4.3 解析器

| 问题 | 位置 | 描述 |
|------|------|------|
| `is_operator()` O(n) | `Lexer.cpp:69-73` | 在表达式解析热路径中线性扫描 |
| `ParseState` 全量拷贝 | `ParserImpl.cpp:1605` | 泛型消歧时每次拷贝整个 token 向量 |
| `std::regex` 性能差 | `ParserImpl.cpp:21` | 简单路径验证用 `std::regex`，应手写 |
| `std::stoi` 异常未处理 | `Lexer.cpp:567` | 空字符串或超大数字抛非 `LexException` |
| 枚举范围检查脆弱 | `ParserImpl.cpp:1517,2332` | 依赖枚举数值顺序，插值即崩 |
| ~~`accept()` EOF 错误信息差~~ | `ParserImpl.cpp:390` | ✅ 2026-06-03 `EOFException` 改为继承 `ParseException`，`accept()`/`unexpected()` 检查 EOF 后给出描述性错误 |

---

## 5. 低优先级问题（Low）

### 5.1 命名问题

| 问题 | 位置 |
|------|------|
| ~~拼写错误 `moudleName`~~ | `ParserImpl.cpp:83` | ✅ 2026-06-03 |
| ~~拼写错误 `isTermintator`~~ | `Lexer.cpp:219` | ✅ 2026-06-03 |
| ~~拼写错误 `exponentalSet`~~ | `Lexer.cpp:584` | ✅ 2026-06-03 |
| ~~拼写错误测试名 `"shoud be able to"`~~ | `interpreter_test.cpp:105,115` | ✅ 2026-06-03 |
| ~~`hexoBase`/`octaBase` 应为 `hexBase`/`octalBase`~~ | `Lexer.cpp:667,720` | ✅ 2026-06-03 |
| ~~`tokenType` 局部变量遮蔽同名静态映射和枚举类型~~ | `Lexer.cpp:578` | ✅ 2026-06-03 重命名为 `numTokenType` |
| `Str`/`Vec`/`Map`/`Set` 别名遮蔽标准库类型名 | 全局 |
| Opcode 命名不一致（有/无类型后缀混用） | `opcode.hpp` |

### 5.2 设计问题

| 问题 | 位置 | 描述 |
|------|------|------|
| `NonCopyable` 同时删除移动操作 | `common.hpp:31-32` | 阻止高效对象转移 |
| ~~`revert(size_t n = 1)` 误导性默认参数~~ | `parser.hpp:61` | ✅ 2026-06-03 修正注释，移除默认值 |
| ~~`LexState::next(int n)` 参数类型不匹配~~ | `parser.hpp:54` | ✅ 2026-06-03 改为 `size_t` |
| 宏 `reserved.inc` 污染宏命名空间 | `reserved.inc` | C++23 应使用 `constexpr` 数组 |
| `TypeAnnotationType` 非 scoped enum | `ast.hpp:213` | 与 `ASTNodeType` 的 `enum class` 不一致 |
| `type_from_repr()` 暴露实现细节 | `typecheck.hpp:28` | 公共 API 允许从字符串构造类型 |
| 死代码 opcode | `opcode.hpp` | `NEG_F64`、`EQ_F64`、`ADD_I32` 等从未使用 |
| `0ZU` 非标准后缀 | `buffer_runtime.cpp:275` | 应使用 `size_t{0}` |

### 5.3 类型检查 match() 对称性

- `CustomizedType::match()` 忽略 `moduleId` — 不同模块同名类型被误判为相同
- `UnionType::match()` 允许 `any_member_matches`，但反方向 `i32.match(Union<i32,string>)` 为 false
- `PrimitiveType::match()` 允许隐式收窄转换（`i64.match(i8)` 为 true）

---

## 6. 测试与基础设施

### 6.1 测试质量问题

| 问题 | 位置 | 描述 |
|------|------|------|
| `test.hpp` 的 `parse()` 静默吞没异常 | `test/test.hpp:21-45` | 解析失败时返回 null 而非失败 |
| 多数解析器测试只检查 `ast != nullptr` | `parser_test.cpp` | 不验证 AST 结构 |
| 解释器测试用 NG 内 `assert()` 验证 | `interpreter_test.cpp` | 若 NG assert 机制有 bug 则全部测试虚过 |
| `destroyast()` 手动调用 343 次 | 全测试 | 异常时内存泄漏，应使用 RAII |
| `delete intp` 手动管理 | `interpreter_test.cpp:12-24` | 异常时泄漏，应使用 `unique_ptr` |
| 测试依赖文件系统路径 | `integration_test.cpp:92-96` | `../example/` 路径 hack |
| `SourceModuleFixture` 重复定义 | `compiler_vm_test.cpp` / `integration_test.cpp` | 违反 DRY |
| `nominal_test.cpp` 测试浅薄 | `test/orgasm/` | 用 `return 42` 断言，不验证名义类型语义 |

### 6.2 缺失的测试类别

| 类别 | 说明 |
|------|------|
| **Fuzz 测试** | 词法/解析器无 fuzz 测试（建议用 libFuzzer） |
| **压力测试** | 无深递归、大输入、内存压力测试 |
| **词法分析器负面测试** | 只有 1 个（`@`），缺少未终止字符串、非法转义、溢出字面量等 |
| **模块系统错误路径** | 无循环导入、缺失文件、损坏 .ngo 等测试 |
| **GC 压力测试** | 只测试 1 个自引用节点，无多循环、弱引用、终结器顺序测试 |
| **VM 字节码验证** | 无畸形字节码输入测试 |
| **字符串边界条件** | 无空字符串、超长字符串、null 字节、Unicode 测试 |
| **解析器错误恢复** | 无部分恢复测试 |

### 6.3 构建系统问题

| 问题 | 位置 | 描述 |
|------|------|------|
| ~~`CMAKE_BUILD_TYPE` 硬编码为 Debug~~ | `CMakeLists.txt:3` | ✅ 2026-06-03 改为 cache 变量 |
| 编译器检测脆弱 | `CMakeLists.txt:6-11` | 非 CI 路径强制 clang++，不支持 GCC |
| `feature_test` 未接入 CTest | `CMakeLists.txt:120` | 有断言但 CTest 不运行它 |
| Coverage 标志重复 | `CMakeLists.txt:131-152` | 两处近乎相同的 coverage 配置 |
| 无 Windows 支持 | `CMakeLists.txt` | 只支持 macOS + Clang + libc++ |
| ~~`.clang-format` 声明 `c++20`~~ | `.clang-format:4` | ✅ 2026-06-03 改为 `c++23` |

### 6.4 CI/CD 问题

| 问题 | 位置 | 描述 |
|------|------|------|
| 无构建缓存 | `build.yml:19` | 子模块每次完整编译 |
| 无构建矩阵 | `build.yml:11` | 只测 ubuntu-latest，无 macOS |
| 无 LLVM 安装缓存 | `build.yml:20-23` | 每次从头安装 LLVM 20 |
| 无 CI 超时 | `build.yml` | 测试挂起可无限消耗 runner 时间 |
| `feature_test` 无断言 | `build.yml:32-33` | 始终 exit 0，输出不校验 |

### 6.5 .clang-tidy 配置问题

| 问题 | 位置 | 描述 |
|------|------|------|
| `WarningsAsErrors` 为空 | `.clang-tidy:3` | 无警告会失败构建 |
| `HeaderFilterRegex` 为空 | `.clang-tidy:15` | 32 个头文件不被检查 |
| 函数大小阈值 800 | `.clang-tidy:32` | 过于宽松，应为 100-200 |
| `cert-arr39-c` 全部禁用 | `.clang-tidy:19-24` | 丢失缓冲溢出检测 |

---

## 7. 改进建议路线图

### 第一阶段：修复关键 bug（1-2 周）

- [x] **修复 PUSH 指令字节数** — `Compiler.cpp` 中 `PUSH_I8`/`U8` 发射 1 字节，`PUSH_I16`/`U16` 发射 2 字节 ✅ 2026-06-03
- [x] **添加 VM 边界检查** — 在 `read_le_bytes`、所有索引访问处添加检查 ✅ 2026-06-03
- [x] **修复 ASTNodeType 枚举冲突** — 为 `TAGGED_VALUE_EXPRESSION` 和 `PACK_EXPRESSION` 分配唯一值 ✅ 2026-06-03
- [x] **修复未终止字符串无限循环** — 在字符串词法循环中检查 EOF ✅ 2026-06-03
- [x] **修复 AST 内存泄漏** — 为 `PropertyDef` 添加析构函数，修复 `NewTypeDef` 的 `genericParams` 清理 ✅ 2026-06-03
- [x] **修复 `repr()` 空指针解引用** — 添加 null 检查 ✅ 2026-06-03
- [x] **修复 `withStream` 异常捕获** — 只捕获 `LexException` ✅ 2026-06-03

### 第二阶段：类型系统健全性（2-3 周）

- [x] **修改 `Untyped::match()`** — 调查后发现 Untyped 被广泛用作合法通配符类型（通配符导入、空数组、未解析泛型），简单修改会破坏 9 个测试。改为添加文档注释说明设计意图 ✅ 2026-06-03
- [x] **修复 `CustomizedType::match()`** — 当双方都有 moduleId 时必须匹配，防止不同模块同名类型误判 ✅ 2026-06-03
- [ ] **统一 match() 对称性语义** — 文档化并实施一致的匹配契约
- [ ] **评估隐式收窄转换** — 决定 `i64.match(i8)` 是否应为 true
- [x] **修复 `ParserImpl` 按值存储 `ParseState`** — 改为引用 `ParseState &state`，避免深拷贝整个 token 向量 ✅ 2026-06-03

### 第三阶段：运行时安全与性能（3-4 周）

- [x] **添加整数溢出检查** — 使用 `__builtin_add_overflow`/`__builtin_sub_overflow`/`__builtin_mul_overflow` ✅ 2026-06-03
- [ ] **减少栈推送克隆开销** — 对简单值（整数、布尔）使用内联存储
- [x] **替换 `insert(begin())`** — 使用 `reserve` + `push_back` + `reverse` ✅ 2026-06-03
- [ ] **构建函数/类型索引** — 替换线性扫描为 `Map<Str, size_t>`
- [ ] **统一异常层次** — 建立一致的异常类型体系
- [ ] **添加源码位置到 VM 错误** — 实现 debug info / source map

### 第四阶段：架构改进（4-6 周）

- [ ] **拆分 TypeChecker** — 提取过载解析、泛型单态化、借用检查、trait 解析为独立模块
- [ ] **减少 Visitor 膨胀** — 考虑 CRTP 默认 visitor 或 `std::variant` 方案
- [ ] **消除类型分发链重复** — 为 `TypeInfo` 添加虚方法或 visitor
- [ ] **统一 AST 所有权模型** — 统一使用 `unique_ptr` 或 `shared_ptr`
- [ ] **添加 RAII guard 用于 `destroyast`** — 消除手动内存管理

### 第五阶段：测试与基础设施（持续）

- [ ] **添加 fuzz 测试** — 对词法分析器和解析器使用 libFuzzer
- [ ] **添加 VM 字节码验证** — 在执行前验证基本正确性
- [ ] **增强测试断言** — 解析器测试应验证 AST 结构
- [ ] **引入 RAII 测试工具** — 替换手动 `destroyast`/`delete`
- [x] **修复构建系统** — `CMAKE_BUILD_TYPE` 改为 cache 变量，支持 Release ✅ 2026-06-03
- [ ] **添加 CI 构建矩阵** — macOS + Ubuntu，添加超时
- [ ] **收紧 `.clang-tidy`** — 启用 `bugprone-*` 为 error，设置 `HeaderFilterRegex`
- [ ] **添加 CI 缓存** — 缓存 LLVM 安装和构建产物
- [x] **更新 `.clang-format` 为 `c++23`** ✅ 2026-06-03

---

## 附录：评审方法论

本评审采用多维度并行审查：

1. **解析器模块** — 词法分析、递归下降解析、错误处理
2. **AST 与类型检查** — 节点层次、visitor 模式、类型匹配、泛型单态化
3. **运行时与 ORGASM** — 字节码编译、VM 执行、内存管理、GC
4. **测试与基础设施** — 测试质量、构建系统、CI/CD、代码质量工具

每个维度独立审查后交叉验证，确保发现的准确性。所有位置引用均基于实际代码行号。
