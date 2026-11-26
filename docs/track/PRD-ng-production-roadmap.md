# PRD: NG Language Production Roadmap

## 1. 背景与问题

NG 是一个用 C++23 实现的静态类型、多范式编程语言。当前已完成：
- 类型系统（泛型、trait、标签联合、HKT）
- 所有权模型（ref、move、partial move）
- 编译时元编程（const if、const fun、类型特化）
- 双后端（AST 解释器 + 字节码 VM）
- 原生 FFI（C++ 绑定）
- ImGui 集成
- 695+ 测试，3,327+ 断言

**当前痛点**：
- 无错误处理机制（只有 assert 终止）
- 标准库过于简单（~450 行 NG）
- 无包管理和构建系统
- 无并发支持
- 无调试器和 IDE 支持
- 缺少语法便利性（lambda、string interpolation 等）

## 2. 目标

**最终目标**：生产部署可用（C 级别）

**阶段目标**：
- **Phase 1**：团队协作可用（错误处理 + 标准库 + 包管理 + 测试）
- **Phase 2**：生产就绪（并发 + 调试器 + 性能优化）
- **Phase 3**：生态系统成熟（LSP + 文档 + 社区工具）

## 3. 非目标

以下功能**不包含**在本计划中：
- LLVM AOT 编译（GC stack maps 复杂度高）
- JIT 编译（无明确使用场景）
- 直接 WASM 编译（需要 LLVM 后端）
- GAT（需要生命周期系统，12+ 个月）
- Async/await 状态机（需要 VM 暂停支持）

## 4. 术语

| 术语 | 定义 |
|------|------|
| ORGASM | 字节码 VM 后端 |
| STUPID | AST 解释器后端 |
| libng | 嵌入式 C API 库 |
| Result | `Result<T, E>` 标签联合 |
| Option | `Option<T>` 标签联合 |

## 5. 已确认的语法设计

### 5.1 错误处理
```ng
// 内置类型
type Result<T, E> = Ok(value: T) | Err(error: E);
type Option<T> = Some(value: T) | None;
type NoneError = NoneError;

// Error trait
export trait Error {
    fun message(self: ref<Self>) -> string;
    fun causedBy(cause: ref<Error>) -> Self;
}

// ? 操作符（后缀）
val content = readFile("data.txt")?;  // Result: 拿到 T 或抛出 E
val item = findItem()?;               // Option: 拿到 T 或抛出 NoneError
```

### 5.2 循环
- 只保留 `loop` 语法，不添加 `for`/`while`

### 5.3 Lambda 与闭包
```ng
// Kotlin/Scala 风格
val double = (x: i32) -> i32 => x * 2;
val process = (x: i32) -> i32 {
    val tmp = x * 2;
    return tmp + 1;
};

// C++ 风格捕获
val factor = 10;
val multiply = (x: i32) -> i32 => x * factor;           // 默认 by value
val multiplyRef = [&factor](x: i32) -> i32 => x * factor; // by ref
val captureAll = [&](x: i32) -> i32 => x * factor;       // 全部 by ref
val captureNone = [](x: i32) -> i32 => x * factor;       // 全部 by value
val mixed = [&a, b](x: i32) -> i32 => a + b + x;        // 混合

// (T) -> U 是引用类型，无法解引用
```

### 5.4 模式匹配
```ng
// switch 作为表达式
val result = switch (x) {
    case 0 => "zero";
    case n when n > 0 => "positive";
    case _ => "negative";
};

// switch 作为语句
switch (x) {
    case 0 { print("zero"); }
    case n when n > 0 { print("positive"); }
    otherwise { print("negative"); }
}
```

### 5.5 运算符重载
```ng
// Rust 风格 trait
trait Add<Rhs = Self> {
    fun add(self: ref<Self>, rhs: ref<Rhs>) -> Self;
}

impl Add for Complex {
    fun add(self: ref<Self>, rhs: ref<Self>) -> Complex {
        return Complex { real: self.real + rhs.real, imag: self.imag + rhs.imag };
    }
}

val c = a + b;  // 自动调用 Add::add
```

### 5.6 并发
```ng
// spawn/await 关键字
val task = spawn heavyComputation(data);
val result: Result<T, E> = await(task);
val value = (await task)?;
```

## 6. 需求清单

### Phase 1: 团队协作可用

| Req ID | 需求 | 验收口径 | 优先级 |
|--------|------|----------|--------|
| REQ-101 | Result/Option 内置类型 | `Result<T,E>` 和 `Option<T>` 可用，支持模式匹配 | P0 |
| REQ-102 | Error trait | `Error` trait 可用，`message()` 和 `causedBy()` 方法 | P0 |
| REQ-103 | `?` 操作符 | 后缀 `?` 可用于 Result 和 Option，自动传播错误 | P0 |
| REQ-104 | Lambda 语法 | `(args) -> Type => expr` 和 `(args) -> Type { body }` 可用 | P0 |
| REQ-105 | 闭包捕获 | `[&]`, `[]`, `[&a, b]` 捕获语法可用 | P0 |
| REQ-106 | switch 表达式 | `switch` 可返回值，支持 `when` guard | P0 |
| REQ-107 | 运算符重载 | `Add`, `Sub`, `Mul`, `Div`, `Eq`, `Ord`, `Index` trait 可用 | P0 |
| REQ-108 | std.result 模块 | `Result` 和 `Option` 的辅助函数（map, unwrap 等） | P0 |
| REQ-109 | std.collections | `HashMap<K,V>` 和 `HashSet<T>` 可用 | P0 |
| REQ-110 | std.io 改造 | `readFile`/`writeFile` 返回 `Result` | P0 |
| REQ-111 | std.json | JSON 解析和序列化 | P1 |
| REQ-112 | std.string 增强 | string interpolation `f"Hello, {name}!"` | P1 |
| REQ-113 | std.math | 基本数学函数 | P1 |
| REQ-114 | 包管理器 | `ng.toml` + `ng add`/`ng remove` | P0 |
| REQ-115 | 测试框架 | `test "name" { }` 语法，断言宏 | P0 |
| REQ-116 | 构建系统 | `ng build`/`ng run`，增量编译 | P1 |
| REQ-117 | 格式化工具 | `ng fmt`，`--check` 模式 | P1 |

### Phase 2: 生产就绪

| Req ID | 需求 | 验收口径 | 优先级 |
|--------|------|----------|--------|
| REQ-201 | spawn/await | `spawn` 和 `await` 关键字可用，线程池实现 | P0 |
| REQ-202 | never 类型 | `never` 类型可用，`exit()` 返回 `never` | P1 |
| REQ-203 | 编译时借用检查 | use-after-move 编译时检测 | P1 |
| REQ-204 | libng 嵌入 API | C API 可用：`ng_vm_create()`, `ng_vm_run_file()` | P1 |
| REQ-205 | 调试器 Phase 1 | source maps + DAP skeleton | P2 |
| REQ-206 | C FFI Phase 1 | `extern fun`, `*T` 指针, `unsafe` 块 | P2 |

### Phase 3: 生态系统成熟

| Req ID | 需求 | 验收口径 | 优先级 |
|--------|------|----------|--------|
| REQ-301 | LSP 服务器 | 基本补全、诊断、跳转 | P1 |
| REQ-302 | 文档生成器 | `///` 注释 → HTML/Markdown | P1 |
| REQ-303 | 网站和 Playground | Hugo 站点 + Docker 化 ngi | P2 |
| REQ-304 | RFC 流程 | 公开 RFC 模板和流程 | P2 |

## 7. 约束

- **兼容性**：不破坏现有 695+ 测试
- **性能**：新功能不应显著降低编译/运行速度
- **架构**：遵循现有 Visitor 模式和依赖方向
- **依赖**：vendored 目录中的第三方库不可修改

## 8. 追溯矩阵

（实施时维护）

| Req ID | 计划 | 测试 | 代码路径 |
|--------|------|------|----------|
| REQ-101 | v1-001 | test/typecheck/result_test.cpp | include/typecheck/, src/typecheck/ |
| ... | ... | ... | ... |
