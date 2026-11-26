# NG Production Roadmap - Version Index

## 版本规划

| 版本 | 名称 | 目标 | 状态 |
|------|------|------|------|
| v1 | Error Handling + Lambda + Switch | 语言核心改进 | 📋 计划中 |
| v2 | Standard Library Phase 1 | 基础标准库 | ⏳ 待开始 |
| v3 | Package Manager + Build System | 团队协作基础 | ⏳ 待开始 |
| v4 | Testing Framework | 质量保证 | ⏳ 待开始 |
| v5 | Concurrency | 并发支持 | ⏳ 待开始 |
| v6 | Runtime Optimization + FFI | 性能和集成 | ⏳ 待开始 |
| v7 | Tooling (LSP, Formatter, Docgen) | 开发体验 | ⏳ 待开始 |

---

## v1: Error Handling + Lambda + Switch

**目标**: 实现语言核心改进，为后续开发奠定基础

**关键交付**:
- Result/Option 内置类型
- Error trait 和 ? 操作符
- Lambda 语法和闭包捕获
- switch 表达式和 when guard
- std.result 模块

**计划文档**: `v1-error-handling-and-lambda.md`

**依赖**: 无

---

## v2: Standard Library Phase 1

**目标**: 扩展标准库，支持常见数据结构和 I/O

**关键交付**:
- `std.collections`: HashMap, HashSet
- `std.io` 改造: 返回 Result
- `std.json`: JSON 解析
- `std.string` 增强: string interpolation
- `std.math`: 基本数学函数

**依赖**: v1 (需要 Result 和 Error trait)

---

## v3: Package Manager + Build System

**目标**: 实现项目管理和依赖管理

**关键交付**:
- `ng.toml` 项目清单
- `ng build` / `ng run` 命令
- Git + path 依赖
- 增量编译

**依赖**: v1

---

## v4: Testing Framework

**目标**: 实现内置测试框架

**关键交付**:
- `test "name" { }` 语法
- 断言宏
- 测试运行器
- 覆盖率报告

**依赖**: v1 (需要 Error trait), v3 (需要构建系统)

---

## v5: Concurrency

**目标**: 实现基本并发支持

**关键交付**:
- `spawn` 和 `await` 关键字
- 线程池实现
- Task<T> 类型
- 错误传播

**依赖**: v1 (需要 Result 和 ?)

---

## v6: Runtime Optimization + FFI

**目标**: 性能优化和 C 集成

**关键交付**:
- libng 嵌入 API
- `extern fun` 语法
- `*T` 原始指针
- `unsafe` 块

**依赖**: v2, v5

---

## v7: Tooling

**目标**: 提升开发体验

**关键交付**:
- LSP 服务器
- 代码格式化
- 文档生成器
- 调试器基础

**依赖**: v3

---

## 技术债务

在实施过程中需要逐步解决的技术债务：

| 项目 | 位置 | 影响 | 建议处理版本 |
|------|------|------|--------------|
| 10 个全局静态变量 | `typecheck.cpp` | 可测试性 | v1 或 v2 |
| 800 行 VM opcode switch | `VM.cpp` | 可维护性 | v5 或 v6 |
| 39 个重复 numeral 类型模式 | 多个文件 | 代码重复 | v2 |

---

## 跨版本依赖图

```
v1 (Error + Lambda + Switch)
├── v2 (Standard Library)
│   └── v6 (Runtime + FFI)
├── v3 (Package Manager)
│   ├── v4 (Testing)
│   └── v7 (Tooling)
└── v5 (Concurrency)
```

---

## 追溯矩阵

| Req ID | 版本 | 测试 | 代码路径 |
|--------|------|------|----------|
| REQ-101 | v1 | test/typecheck/result_option_test.cpp | include/typecheck/typeinfo.hpp |
| REQ-102 | v1 | test/typecheck/error_trait_test.cpp | lib/std/prelude.ng |
| REQ-103 | v1 | test/typecheck/error_propagation_test.cpp | src/parsing/ParserImpl.cpp |
| REQ-104 | v1 | test/parsing/lambda_test.cpp | include/ast.hpp |
| REQ-105 | v1 | test/typecheck/closure_capture_test.cpp | src/typecheck/typecheck.cpp |
| REQ-106 | v1 | test/parsing/switch_expression_test.cpp | src/parsing/ParserImpl.cpp |
| ... | ... | ... | ... |
