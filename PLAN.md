# NG 项目状态与实现计划

## 当前状态

**测试：267/267 全部通过（1429 断言）**

### 已完成特性

#### 1. 语言核心
- **词法分析器** — 完整的 token 化，支持关键字、标识符、运算符、字面量、元组范围
- **语法分析器** — 递归下降解析器，支持函数定义、类型定义、控制流、表达式、模块声明
- **AST** — Visitor 模式，完整的 AST 节点层次结构
- **类型检查器** — 双向类型推断，支持原始类型、函数类型、数组、元组、自定义类型

#### 2. Nominal 类型系统
- `typealias` — 透明类型别名（type A = B;）
- `newtype` — 不透明新类型（type A wraps B;）
- `cast` — 显式类型转换（cast<T>(expr)）
- 双向类型推断 — 函数参数和返回值的类型推断

#### 3. 标签联合类型（Tagged Unions）
- `type Result = Ok(value: i32) | Err(msg: string);` 语法
- `switch (expr) { case Ok(v) { ... } case Err(m) { ... } }` 模式匹配
- `otherwise { ... }` 默认分支
- **穷尽性检查**：所有变体必须被覆盖，或有 `otherwise` 分支
- 类型系统：`TaggedUnionType`、`VariantType`、`UnionType`
- 解释器和 ORGASM 后端均已支持

#### 4. 对象/结构体内存布局
- `NGStructuralObject` 扁平字段数组（`Vec<RuntimeRef<NGObject>> fields`）
- `GET_PROPERTY`/`SET_PROPERTY` 按字段索引 O(1) 访问
- `GET_PROPERTY_STR`/`SET_PROPERTY_STR` 按字符串名动态查找

#### 5. ORGASM 字节码后端
- 操作数索引从 `uint8_t` 扩展到 `uint16_t`
- 完整的编译器（AST → 字节码）和 VM（执行字节码）
- 支持：算术、比较、控制流、函数调用、对象/数组/元组、标签联合
- `NATIVE_CALL` 操作码 + `native_bridge.hpp` 自动类型转换 FFI
- `BytecodeModule::merge()` 多模块编译 + 索引重映射

#### 6. 模块系统
- `import`/`export` 声明
- 文件模块加载器
- 标准库模块（prelude）

#### 7. 泛型系统（v0.5.0）— ✅ 核心已完成
- **解析器** — `<T>` 方括号语法、`T TypeName` 后缀语法（左结合）、`<T...>` 参数包
- **类型检查器** — `GenericDefType`、`GenericParamType`、单态化（`monomorphizeGenericCall`）
- **解释器** — 泛型函数运行时实例化，参数包收集与展开
- **标准库** — `print<T...>`、`assert<T...>`、`len<T>` 均为泛型原生函数

---

## v0.5.0 — 泛型与变参函数

### 动机
- `print(...)` 类型检查失败：严格参数个数匹配阻止多参数调用
- 缺少泛型类型（`Optional<T>`、`Map<K,V>`、`array<T>`）
- 缺少参数包（parameter packs）支持变参函数
- 标准库因缺少泛型而严重受限

### 设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 主语法 | `TypeName<T>` | 无歧义，易于解析 |
| 后缀语法 | `T TypeName`（左结合嵌套） | OCaml 风格，一致性强 |
| 内建类型 | 两种：`array<i32>` 和 `i32 array` | 统一泛型系统 |
| 实例化 | 具体化（reified，单态化） | C++ 模板语义，高性能 |
| 参数包 | `T...` 配合 `args...` 展开 | 自然的 spread 扩展 |
| 后缀无需泛型声明 | 任意类型 + 标识符 = 后缀泛型应用 | 简洁统一 |
| 内建类型统一 | `array<T>` 纳入泛型系统 | 一致性 |
| `new` 中使用泛型 | 支持：`new Optional<i32> { ... }` | 完整表达力 |
| 嵌套泛型 | 从一开始就支持 | 真实使用必需 |

### 语法参考

```ng
// 泛型类型定义
type Optional<T> { property value: T; property has: bool; }
type Map<K, V> { ... }

// 两种类型注解语法
val x: Optional<i32> = ...;
val x: i32 Optional = ...;

// 内建泛型
val a: array<i32> = [1, 2, 3];
val a: i32 array = [1, 2, 3];

// 泛型函数
fun map<A, B>(f: fun(A) -> B, xs: A array) -> B array { ... }

// 参数包
fun print<T...>(args: T...) -> unit = native;
fun format<F...>(fmt: string, args: F...) -> string { ... }

// 泛型调用（类型推断）
val y = identity(42);         // T 推断为 i32
print(1, "hello", true);      // T... = (i32, string, bool)
```

### 实现阶段

#### Phase 1: AST + 解析器 — 泛型语法 ✅
- [x] `GenericParam` AST 节点（`name`、`isPack` 标志、`bound` 类型约束）
- [x] `FunctionDef`/`TypeDef`/`TypeAliasDef`/`NewTypeDef`/`TaggedUnionDef` 添加 `genericParams`
- [x] `TypeAnnotation` 添加 `genericArgs: Vec<shared_ptr<TypeAnnotation>>`
- [x] 解析 `<T, U, V...>` 语法（`fun<T>` 和 `fun name<T>` 双语法）
- [x] 解析 `<T, U>` 语法（类型定义中）
- [x] 解析 `TypeName<T>` 方括号语法（类型注解中）
- [x] 处理嵌套泛型：`Option<Option<int>>`（`>>` → 两个 `>` 的拆分）
- [x] `T: Bound` 类型约束解析
- [x] 词法分析器：`<>` 和 `>>>` 特殊处理
- [x] 54 个泛型解析器测试全部通过（458 断言）
- [x] 解析 `T TypeName` 后缀语法（左结合脱糖）
- [x] 解析 `(T1, T2) TypeName` 多参数后缀语法

#### Phase 2: 类型检查器 — 泛型 ✅
- [x] `GenericParamType`（tag: `GENERIC_PARAM = 0xC0`）
- [x] `GenericDefType`（tag: `GENERIC_DEF = 0xC1`）
- [x] 第一遍扫描注册泛型函数定义
- [x] 调用点类型参数推断（基于合一 + `extractGenericBindings`）
- [x] 单态化（`monomorphizeGenericCall`）：用具体类型参数实例化泛型定义
- [x] 处理嵌套泛型类型解析
- [x] 类型检查器泛型测试

#### Phase 3: 参数包 ✅
- [x] `GenericParam` 的 `isPack` 标志
- [x] 包匹配：收集剩余参数到包中
- [x] 解释器中的包展开

#### Phase 4: 解释器 — 泛型 ✅
- [x] 泛型函数的运行时单态化
- [x] 解释器泛型测试

#### Phase 5: 变参 + 标准库（修复 print）✅
- [x] 变参原生函数桥接（接受可变数量参数）
- [x] `print<T...>(args: T...) -> unit = native`
- [x] `assert<T...>(assertion: T...) -> unit = native`
- [x] `len<T>(xs: T array) -> u32 = native`（支持 array、string、tuple）
- [x] 254/254 测试全部通过
- [x] 258/258 测试全部通过（含泛型类型检查测试）

#### Phase 6: 文档与示例 — ✅ 完成
- [x] PLAN.md 更新为当前状态
- [x] 更新 `docs/guide/language_guide.md` 添加泛型章节
- [x] 更新 `CHANGELOG.md` 添加 v0.5.0 条目
- [x] 更新 `example/15.generics.ng`（含 len() 测试）
- [x] 更新 `README.md` 特性列表

---

## 待实现特性（长期）

### P0 — 高优先级

#### A. 语言特性完善

**A1. 结构化联合类型注解（Union Type Annotations）** — ✅ 完成
- `ParserImpl.cpp:typeAnnotation()` 已支持 `val x: i32 | string = 42;`
- `typecheck.cpp` 已将联合类型注解转换为 `UnionType`
- 测试：`test/typecheck/typecheck_union_test.cpp`

**A2. switch/case 穷尽性检查** — ✅ 完成
- `typecheck.cpp:visit(SwitchStatement*)` 检查所有变体被覆盖或有 `otherwise` 分支
- 5 个新测试（exhaustive、otherwise、non-exhaustive failure、unknown variant failure、multi-variant otherwise）
- 示例：`example/20.switch_otherwise.ng`

**A3. otherwise/default 分支** — ✅ 完成
- `otherwise { ... }` 语法在 `ParserImpl.cpp:switchStatement()` 中实现
- `CaseClause.isOtherwise` 标志在 AST 中支持
- `otherwise` 必须是最后一个分支

**A4. 标签联合值的属性访问**
- 状态：`Ok(42).value` 语法未支持
- 需要：支持 `taggedValue.fieldName` 访问载荷字段
- 修改：`Compiler.cpp`、`VM.cpp`、`stupid.cpp`

#### B. ORGASM 后端补全

**B1. 空 visitor 补全**
- `visit(TypeOfExpression*)` — 编译器中为空实现
- `visit(FunctionDef*)` — 模块级定义已在第一遍处理，但 visitor 为空
- `visit(TypeDef*)` — 同上
- `visit(TypeAliasDef*)` — 同上
- `visit(NewTypeDef*)` — 同上
- `visit(Binding*)` — 空实现

**B2. 更多运算符支持**
- 二元运算符：`MOD_F64`、`EQ_F64`、`LT_F64`、`GT_F64`、`NEG_F64` 已定义但未全部使用
- 位运算：`LSHIFT`/`RSHIFT` 已实现，但缺少 `AND`、`OR`、`XOR`
- 字符串运算：`CONCAT`、比较

**B3. 错误处理**
- 当前 VM 遇到未知 opcode 直接返回 UNIT
- 需要：更好的错误信息、栈追踪

### P1 — 中优先级

#### C. 标准库扩展

**C1. 基础 I/O**
- `print` 已有（通过 NATIVE_CALL）
- 需要：`readLine`、`readFile`、`writeFile`

**C2. 字符串操作**
- 当前 `NGString` 支持基本操作
- 需要：`split`、`join`、`trim`、`contains`、`replace`、正则表达式

**C3. 集合操作**
- 数组：`map`、`filter`、`reduce`、`sort`、`find`
- 元组：模式匹配、解构赋值增强

**C4. 数学库**
- 三角函数、对数、随机数
- 大整数支持（i128/u128）

#### D. 模块系统增强

**D1. 相对路径导入**
- 当前模块路径硬编码
- 需要：`import "./utils.ng";` 相对路径支持

**D2. 模块缓存**
- 避免重复加载同一模块
- 需要：模块实例缓存机制

**D3. 标准库完善**
- `lib/` 目录当前为空
- 需要：`std.ng`、`math.ng`、`string.ng`、`io.ng` 等

### P2 — 低优先级

#### E. 语言高级特性

**E1. 模式匹配增强**
- 嵌套模式：`case Ok(Some(x)) { ... }`
- 守卫条件：`case Ok(x) if x > 0 { ... }`
- 通配符：`case _ { ... }`

**E2. 闭包/Lambda**
- `val add = (a, b) => a + b;`
- 捕获外部变量
- 需要：AST 节点、解析器、类型检查、运行时表示

**E3. 接口/Trait**
- `trait Printable { fun print(self); }`
- `impl Printable for Point { ... }`
- 类型类约束

**E4. 异步/并发**
- `async`/`await` 语法
- 协程支持
- 并发原语

#### F. 工具链

**F1. REPL**
- 交互式解释器
- 历史记录、自动补全

**F2. 调试器**
- 断点、单步执行
- 变量检查
- 源码映射（字节码 → 源码位置）

**F3. 包管理器**
- `ngpkg` 命令
- 包注册中心
- 依赖解析

**F4. 编译优化**
- 常量折叠
- 死代码消除
- 内联优化

---

## 实现顺序建议

### 短期（当前迭代 — v0.5.0 泛型）
1. **Phase 1** — AST + 解析器泛型语法 ✅
2. **Phase 2** — 类型检查器泛型支持 ✅
3. **Phase 3** — 参数包 ✅
4. **Phase 4** — 解释器泛型 ✅
5. **Phase 5** — 变参 + 标准库（修复 print） ✅
6. **Phase 6** — 文档与示例 ✅

### 中期（v0.6.0）
7. **A1** — 结构化联合类型注解（`i32 | string`） ✅
8. **A2** — switch/case 穷尽性检查 ✅
9. **A3** — otherwise/default 分支 ✅
10. **A4** — 标签联合值属性访问
11. **C1-C3** — 标准库基础扩展

### 长期（v0.7.0+）
12. **B1-B3** — ORGASM 后端补全
13. **E1** — 模式匹配增强
14. **E2** — 闭包/Lambda
15. **D1-D2** — 模块系统增强
16. **C4** — 数学库
17. **D3** — 标准库完善

---

## 技术债务

1. **ValDefStatement → ValueBindingStatement 迁移** — `typecheck.cpp:493` 有 TODO
2. **解释器 QUERY 运算符** — `stupid.cpp:247` 抛出 NotImplementedException
3. **ORGASM Compiler 空 visitor** — 6 个空实现需要补全或移除
4. ~~**VM 错误处理**~~ — ✅ 已修复：未知 opcode 改为抛出运行时错误，异常路径会正确回收调用栈帧
5. ~~**测试覆盖**~~ — ✅ 已补充：增加 ORGASM 标签联合 `otherwise` 路径和模块合并类型索引重映射测试
6. ~~**main.cpp prelude_types 硬编码**~~ — ✅ 已修复：`build_prelude_type_index()` 自动从 prelude 模块导出类型

---

## 文件结构参考

```
ng/
├── include/
│   ├── ast.hpp              # AST 节点定义
│   ├── visitor.hpp          # Visitor 接口
│   ├── token.hpp            # Token 类型
│   ├── common.hpp           # 通用类型别名
│   ├── parser.hpp           # Parser 接口
│   ├── module.hpp           # 模块接口
│   ├── intp/
│   │   ├── runtime.hpp      # 运行时对象
│   │   └── runtime_numerals.hpp
│   ├── typecheck/
│   │   ├── typeinfo.hpp     # 类型信息
│   │   └── typecheck.hpp    # 类型检查接口
│   └── orgasm/
│       ├── opcode.hpp       # ORGASM 操作码
│       ├── compiler.hpp     # 编译器接口
│       ├── vm.hpp           # VM 接口
│       ├── module.hpp       # 字节码模块
│       └── native_bridge.hpp # FFI 桥接
├── src/
│   ├── ast/                 # AST 实现
│   ├── parsing/             # 词法/语法分析
│   ├── runtime/             # 运行时对象实现
│   ├── typecheck/           # 类型检查实现
│   ├── orgasm/              # ORGASM 编译器/VM
│   ├── intp/                # 解释器
│   ├── module/              # 模块加载
│   ├── stdlib/              # 标准库
│   └── main.cpp             # ngi 入口
├── test/                    # 测试
├── example/                 # 示例文件
├── lib/                     # NG 标准库
└── docs/                    # 文档
