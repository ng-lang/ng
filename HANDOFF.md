# NG — 移交说明（下一轮 agent 从此处开始）

工作区：仓库根，分支 `new-start`（PR #47）。
本文件只保留三项待办、其证据，以及执行所需约定。完成一项后删除该项，不要回填历史流水；历史见 git log 与 PR #47。

---

## 0. 必读约定

- **AGENTS.md** 是权威约定入口；`docs/` 只放 ground truth，禁止 planned / future / TODO 措辞。
- 不要恢复 `.agents/worklog/`、`.agents/workstate.md`、`.agents/plans/`。本文件是前瞻性移交单。
- **硬约束**：`test/guard_test.cpp` 的 `kMaxCppLines = 1200`、`kMaxHeaderLines = 800`；只拆源文件，绝不上调上限。当前最大：`src/native/lowering_expr.cpp` 985、`module_parser.cpp` ~910、`type_interner.cpp` 899、`parser.cpp` 886。
- **提交规范**：Conventional Commits，imperative mood；AI 生成代码须标注（CONTRIBUTING.md）。
- **门禁**（每条待办完成前必须跑满）：`ctest --test-dir build -j`、`./build-asan/ng_test`、guard；构建 `cmake --build build -j` / `cmake --build build-asan -j`。`ninja` 常误报 "no work to do"，改动后 `touch <file>` 触发重编，以实际运行测试/探针为准。
- 已知 flaky：`INC-18 arena ... leaks(1)` 依赖 `/usr/bin/leaks -atExit`，并行偶发失败，单跑通过，重跑即可。
- 经验：大文件改动用 python + 精确 anchor，插入后立即编译验证；先探针确认「缺口在哪一层」再动手。

---

## 1. 待办（一次只给新 agent 一段）

| Prompt | 任务 | 规模 | 现状证据 |
|---|---|---|---|
| **L** | const eval 支持 Member / struct 值 | 中 | `const fun` 里 `value.x` 与 trait 方法调用折叠均报 "expression is not supported during const evaluation"（运行时正常） |
| **M** | where 子句 bound 的方法解析 | 小 | `where T: Show` 下 `value.show()` 报 "no trait bound provides method"，声明式 `T: Show` 可用 |
| **N** | C ABI 闭包捕获的 userdata / trampoline | 大（先出方案） | 回调当前明确拒绝闭包值："a C callback must be a named function" |

### Prompt L — const eval 支持 Member / struct 值

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本文件 §0。

任务：让 const fun 在编译期折叠结构体字段访问，然后再支持 trait 方法调用。

现状证据（已实测）：
- const fun getX(value: S) -> i64 { return value.x; } 在 const if 中折叠 →
  "expression is not supported during const evaluation"（探针 j13）；
- const fun callShow<T: Show>(value: T) -> string { return value.show(); }
  编译期折叠同样失败（j7）；但两者在运行时均正常（j6/j12 输出正确）。
- 根因：ConstValue 只有 Unit/Bool/Integer/String/Tuple，无 struct / Member 求值；
  ConstValueId 是 const 泛型实例键，扩展时须保持规范化去重语义。

实现提示（拆成两个提交）：
1) 先加 struct 值与字段访问的 const 求值（const_eval / const_interp / typecheck_const）；
2) 再处理 trait 方法调用折叠——需要为实例解析 call target，可能复用 callTargets 边表。

交付：每步测试（正/负契约）、文档同步（D-013 相关 Verified 注记）、跑满 §0 门禁。
```

### Prompt M — where 子句 bound 的方法解析

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本文件 §0。

任务：让 where 子句中的 trait bound 与声明式 bound 一样提供方法解析。

现状证据（已实测）：
- fun callShow<T>(value: T) -> string where T: Show { return value.show(); }
  → "no trait bound provides method `show` for type parameter `T`"（j2/j14）；
- fun callShow<T: Show>(value: T) -> string { return value.show(); }
  → 编译运行正常（j6）。普通函数同样受影响，并非 const fun 专属；
  这是两种 bound 写法的不一致，不是 D-011 supertrait caveat 那一条。

实现提示：checker 注册 where 子句 trait bound 时，使其进入声明式 bound 所用的
方法解析表；保留现有 supertrait caveat 的行为（不要顺手「修」它）。

交付：both spellings 的正负契约测试、文档同步（D-011 / D-014 相关注记）、跑满 §0 门禁。
```

### Prompt N — C ABI 闭包捕获的 userdata / trampoline

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本文件 §0。

任务：INC-29 的剩余边界——让捕获闭包能作为 C 回调跨 ABI。

现状证据（已实测）：
- 当前回调只接受具名非捕获函数，闭包值传入报
  "a C callback must be a named function; closures cannot capture state across the C ABI"。
- 具名函数经内部 C ABI thunk 传递已可用（test/cabi_callback_test.cpp）。

此任务规模最大，**先出方案再动手**：定义 (fnptr, userdata) 的 NG 表示、
生命周期/所有权约束、每闭包 trampoline 的生成与 C fixture 往返契约；
向用户报告方案并得到确认后再实现，分提交落地。

交付：方案 + 分批实现 + 每批测试与文档同步 + 跑满 §0 门禁。
```

---

## 2. 相关线索（探测已确认，不属于上述三项的交付）

- INC-36 基础完备、无需改动：`hasShow<S>()`（derive(Show)）→ ok；`hasShow<i64>()` → 正确拒绝；`const if` 中带 bound 的 const fun 正负例一致。
- `const` 声明体（D-012）语法不接受调用表达式（`const isShow<T>: bool = hasShow<T>();` 解析失败），设计如此，不要当成缺陷。
- 回调相关的既有实现锚点：`test/cabi_callback_test.cpp`、`src/native/lowering.cpp` 的 `emitCAbiThunk`、`TypeCheckResult::callbackTargets`。
