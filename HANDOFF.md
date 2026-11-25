# NG — 移交说明（下一轮 agent 请从此处开始）

工作区：当前工作目录（仓库根；分支 `new-start`，**未推送**）
基线：INC-28/30/31 + INC-29 全部落地（含具名函数回调）
工作区状态：INC-29 全部落地、INC-36 探测无需改动；本轮任务已全部完成，无待办增量

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
| （本提交） | **Prompt G** — INC-28 `isize` / `usize`（目标字宽整数，D-016） |
| （本提交） | **Prompt H** — INC-30 模块工件（D-017）+ INC-31 CompilationSession（D-018） |
| （本提交） | **Prompt I 第 1 步** — INC-29 owning/borrowed/shared 注解 |
| （本提交） | **Prompt I 第 2 步** — INC-29 `T?` 可空句柄（含修复静默丢弃） |
| （本提交） | **Prompt I 第 3a 步** — INC-29 `export extern "C"` 定义 + ABI 包装 |
| （本提交） | **Prompt I 第 3b 步** — INC-29 extern C varargs |
| （本提交） | **Prompt K** — INC-29 callback：具名函数 → C 函数指针 thunk |
| （本探测） | **Prompt J** — INC-36 探测：基础完备，**无需改动** |

闭包体与解构 `let` 上的 `?` 仍正确拒绝。

---

## 2. 仍开放的缺陷

中部 `?` 已落地。剩余任务是半成品增量与新特性（下表），无已实测的阻塞缺陷。

---

## 3. 建议的下一轮顺序

本移交单列出的增量（Prompt A–K）已全部完成或探测确认无需改动，当前无待办增量。

未来若继续，候选方向（均需先出方案再动手）：

- 闭包捕获跨 C ABI 的 userdata / trampoline 约定（INC-29 的剩余边界）；
- const eval 支持 Member / struct 值（让 const fun 折叠字段访问与 trait 方法调用）——见 §4a；
- 既有 backlog（heap 域、并发、工具链）见 git 历史。

---

## 4. 可直接投喂的 prompt

下面每段 prompt 都自包含；**一次只给新 agent 一段**，完成后再给下一段。

| Prompt | 任务 | 规模 | 现状已确认 |
|---|---|---|---|

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

## 4a. INC-36 探测结论（2026-11-25） — 无需改动

基础已完备，实测确认：

- `const fun hasShow<T>() -> bool where T: Show { return true; }` + `hasShow<S>()`（S derive(Show)）→ 编译输出 ok；`hasShow<i64>()` → 正确报 "does not satisfy its where clause"。
- `const if` 中调用带 bound 的 const fun：正例折叠通过、负例正确拒绝，bound 求解一致。
- `T: Show` 声明式 bound 下 `value.show()` 在普通函数与 const fun 中**运行时**均可用（输出 `S{x: 7}`）。

探索到但**不属于 INC-36、不修**的既有通用限制：

- `where T: Show` 形式不提供方法解析（普通函数同样如此），`T: Show` 声明式 bound 才可以；见 D-011 verified caveat 一脉。
- const fun 体内方法调用及普通结构体字段访问 `value.x` 在**编译期折叠**时报 "expression is not supported during const evaluation"——ConstValue 只有 Unit/Bool/Integer/String/Tuple，无 struct/Member 表示，与 bound 无关；属 const-eval 能力边界。
- `const` 声明体（D-012）语法本身不接受调用表达式（`const isShow<T>: bool = hasShow<T>();` 解析失败），设计如此。

---

## 5. 本轮过程中值得记住的两点教训

1. **大文件的字符串替换极易写坏**：对 `src/flowir_expr.cpp` 等 500+ 行文件，用一次性的大 `old_string` 替换连续造成两次损坏（都靠 `git checkout --` 完整还原）。**改用 python + 精确 anchor + 插入后立即编译验证**，本轮 part 2 因此一次成功。
2. **先验证依赖再动手**：INC-24 part 2 我原以为难点在 FlowIR 合成，实测后发现真正缺口是 checker 的 `requireType(builtin::I64, ...)`。动手前先用探针确认「到底卡在哪一层」，能显著少走弯路。
