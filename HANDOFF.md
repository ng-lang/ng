# NG — 移交说明（下一轮 agent 请从此处开始）

工作区：当前工作目录（仓库根；分支 `new-start`，**未推送**）
基线：`b2a8048`（INC-35 Self 视图方法）+ 即将落地的 field-span 提交
工作区状态：完成 Prompt F 后应干净；下一轮从 Prompt G 起

本文件列出已完成的收尾状态、仍开放的缺陷（均已实测确认仍在），以及给下一轮的可直接投喂 prompt。

---

## 0. 必读约定（改动前先看）

- **AGENTS.md** 是最新的权威约定入口（已随清理更新，引用均有效）。
- **docs/ 只放 ground truth**（当前已实现、已确认的内容）。任何 planned / future / not yet / deferred / TODO 措辞禁止出现在 `docs/`。
- **不要恢复 `.agents/worklog/`、`.agents/workstate.md`、`.agents/plans/`**。
  本文件是前瞻性移交单；完成一项后从这里划掉，不要往里堆历史流水。剩余任务以下面的 prompt 为准。
- **硬约束**：`test/guard_test.cpp` 的 `kMaxCppLines = 1200`、`kMaxHeaderLines = 800`，**只拆分源文件、绝不上调上限**。当前最大：`src/native/lowering_expr.cpp` 979、`module_parser.cpp` 904、`type_interner.cpp` 895、`parser.cpp` 886。
- **提交规范**：Conventional Commits（`feat(vnext):` / `fix(checker):` / `docs(agents):` …），imperative mood；AI 生成代码须标注（见 CONTRIBUTING.md）。
- **完成前必须跑满门禁**：`ctest --test-dir build -j`（当前基线 674）+ `./build-asan/ng_test`（当前基线 673）+ guard。构建：`cmake --build build -j` / `cmake --build build-asan -j`。
  - 注意：`ninja` 在本环境常误报 "no work to do"；改动后可用 `touch <file>` 触发重编，并以**实际运行测试/探针**为准，不要相信构建输出本身。
- **已知 flaky**：`INC-18 arena ... leaks(1)` 测试依赖 `/usr/bin/leaks -atExit`，并行跑偶尔失败，单跑 4/4 通过。与改动无关，重跑即可。

---

## 1. 本轮已完成

| Commit | 内容 |
|---|---|
| `10f73da` | 去掉 drafting 仪式；HANDOFF 保留；不恢复 worklog/plans |
| `651e98c` | **Prompt A** — `Cell<T>` 深 clone |
| `238fa77` | **Prompt B** — string `Ord` 按字节 memcmp |
| `5797b9b` | **Prompt C** — 中部表达式 `?`（按求值序提升临时变量） |
| `796d85e` | **Prompt D** — INC-33 元组 `case (a, b)` |
| `b2a8048` | **Prompt E** — INC-35 视图上返回 `Self` 的方法 |
| （本提交） | **Prompt F** — INC-21 field-span / span-through-ref |

闭包体与解构 `let` 上的 `?` 仍正确拒绝。

---

## 2. 仍开放的缺陷

中部 `?` 已落地。剩余任务是半成品增量与新特性（下表），无已实测的阻塞缺陷。

---

## 3. 建议的下一轮顺序

7. **Prompt G** — INC-28 isize/usize
8. **Prompt H** — INC-30/31（架构，务必先做等价性验证）
9. **Prompt I** — INC-29 C ABI（先盘点再拆）
10. **Prompt J** — INC-36（**先探测**，可能无需改动）

注：D–G 相互独立，可并行交给不同 agent；H/I 会大范围改编译器，建议独占并在动手前先报告拆分方案。

---

## 4. 可直接投喂的 prompt

下面每段 prompt 都自包含；**一次只给新 agent 一段**，完成后再给下一段。

| Prompt | 任务 | 规模 | 现状已确认 |
|---|---|---|---|
| **G** | INC-28 `isize` / `usize` | 中 | `unknown type usize` |
| **H** | INC-30/31 模块工件 + RuntimeSession | 大（架构） | 未开始 |
| **I** | INC-29 C ABI 收尾 | 大（需先拆） | 部分实现 |
| **J** | INC-36 const fun trait bound | 小/不确定 | **基础已可用**，先探测 |

---

### Prompt G —— INC-28 isize / usize

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定）。

任务：INC-28 —— 新增 `isize` / `usize`（目标字宽整数类型）。

现状（已确认）：`let x: usize = 5;` → type error: unknown type `usize`

设计要求（见 .agents/plans/increment-plan.md 的 INC-28 与 docs/ref/language-decisions.md 的 D-008）：
- 语义应为「目标指针宽度」（当前目标 arm64/x86_64 → 64 位），而不是另起一套算术。
- 与既有整型规则一致：同类型算术、跨宽比较、DEC-A 「禁止隐式混比」。
- 需有 ABI 边界契约（TargetAbiDescriptor 驱动宽度）。

实现提示：
- 内建类型表（typecheck 的 builtin::* 与 type-interner 的内建注册）、
  字面量后缀、isIntegerBuiltin 判定、lowering 的 QBE 映射都要加。
- 常量折叠与 const 谓词（is_integer 类）需一并考虑。
- 若一次改动过大，按「先类型+算术，再 ABI/FFI 边界」拆两个提交。

交付：实现 + 测试（算术/比较/溢出 trap/与 i64 的显式转换）+ example
+ 文档同步（docs/ref/language-decisions.md 的 D-008 条目）+ 跑满门禁 + 提交。
```

---

### Prompt H —— INC-30/31 模块接口工件与 RuntimeSession

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定），
以及 .agents/plans/target-architecture-hir.md / runtime-module-ffi.md 的相关章节。

任务：INC-30（模块接口工件）→ INC-31（RuntimeSession 实例生命周期 + stdlib 预加载）。

这是**架构类**改造，不是新增语言特性，务必**行为等价**优先：
- INC-30：导出 `ModuleArtifact`（函数描述符表：name → 签名 / instance id），
  import 按类型化工件绑定而非重查源码。
- INC-31：工件（不可变）与实例（globals/init 态）分离；session 级 resolver/工件缓存；
  stdlib 以预编译工件集加载。

强建议的等价性验证手段：
- 用 `--emit=ssa` 对**全部** example 导出 SSA，改动前后 `diff -r` 比对
  （仅允许闭包编号这类等价差异）。
- ctest 全量（含 example 语料 sweep）必须保持绿。

依赖与顺序：先 INC-30，后 INC-31；两者应分为**两个提交**。
测试：初始化恰好一次契约（含循环 import）；会话隔离负契约；启动耗时前后对比。

若发现改动规模超出「数百行」，先停下来向用户汇报拆分方案，不要硬推。
```

---

### Prompt I —— INC-29 C ABI 收尾

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定），
以及 docs/ref/language-decisions.md 的 D-004 与 .agents/plans/increment-plan.md 的 INC-29。

任务：INC-29 —— C ABI 收尾。计划明确预期**再拆 2-3 个子步**，实施前先细化。

现状：`extern "C"`、`native fun`、opaque `type X = native;` 已可用
（example/ffi_extern.ng、example/opaque_types.ng 正常）；
D-004 规定的 `owning` / `borrowed` / `shared` 所有权标注与 `T?` 可空句柄尚未落实。

第一步建议：先做一次**现状盘点**（哪些 D-004 条款已实现 / 未实现 / 无法表达），
产出清单后再逐条实现，每条一个提交。不要一次性铺开。

交付：盘点清单 + 分批实现 + 每批跑满 ctest/ASan/guard + 文档同步。
```

---

### Prompt J —— INC-36 const fun 内的 trait bound（补充说明）

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定）。

注意：INC-36 的基础部分**已经可用**，接之前请先确认剩余范围，避免重复劳动。

已确认可用：
    struct S: derive(Show) { x: i64 }
    const fun hasShow<T>() -> bool where T: Show { return true; }
    hasShow<S>()   → 编译并运行，输出 ok

已确认正确拒绝（非缺陷）：
    hasShow<i64>() → type error: call to `hasShow` does not satisfy its where clause
    （i64 没有 impl Show，这是正确行为，不要"修"）

可能的剩余范围（请先探测再决定）：
- 在 const fun **体内**依据 bound 调用 trait 方法（如 `T.show()`）是否可行；
- const 谓词（D-012）与 const fun 的 bound 求解是否一致。

参考 example/generic_const_fun.ng（用 `where T: RenderText`）作为可运行基线。
若探测后发现基础已够用，请向用户汇报"无需改动"，而不是硬造改动。
```

---

## 5. 本轮过程中值得记住的两点教训

1. **大文件的字符串替换极易写坏**：对 `src/flowir_expr.cpp` 等 500+ 行文件，用一次性的大 `old_string` 替换连续造成两次损坏（都靠 `git checkout --` 完整还原）。**改用 python + 精确 anchor + 插入后立即编译验证**，本轮 part 2 因此一次成功。
2. **先验证依赖再动手**：INC-24 part 2 我原以为难点在 FlowIR 合成，实测后发现真正缺口是 checker 的 `requireType(builtin::I64, ...)`。动手前先用探针确认「到底卡在哪一层」，能显著少走弯路。
