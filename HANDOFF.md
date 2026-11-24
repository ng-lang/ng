# NG — 移交说明（下一轮 agent 请从此处开始）

工作区：当前工作目录（仓库根；分支 `new-start`，**未推送**）
基线：isize/usize 提交（Prompt G）+ 本轮 INC-30 模块工件提交（Prompt H 第一步）
工作区状态：完成 INC-30 后应干净；下一轮从 INC-31（Prompt H 第二步）起

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
| （本提交） | **Prompt H 第一步** — INC-30 模块接口工件（D-017）；INC-31 待做 |

闭包体与解构 `let` 上的 `?` 仍正确拒绝。

---

## 2. 仍开放的缺陷

中部 `?` 已落地。剩余任务是半成品增量与新特性（下表），无已实测的阻塞缺陷。

---

## 3. 建议的下一轮顺序

7. **Prompt H** — INC-30/31（架构，务必先做等价性验证）
8. **Prompt I** — INC-29 C ABI（先盘点再拆）
9. **Prompt J** — INC-36（**先探测**，可能无需改动）

注：H/I 会大范围改编译器，建议独占并在动手前先报告拆分方案。

---

## 4. 可直接投喂的 prompt

下面每段 prompt 都自包含；**一次只给新 agent 一段**，完成后再给下一段。

| Prompt | 任务 | 规模 | 现状已确认 |
|---|---|---|---|
| **H** | INC-31 RuntimeSession（INC-30 工件已完成） | 大（架构） | INC-30 已落地，见 D-017 |
| **I** | INC-29 C ABI 收尾 | 大（需先拆） | 部分实现 |
| **J** | INC-36 const fun trait bound | 小/不确定 | **基础已可用**，先探测 |

---

### Prompt H（第二步）—— INC-31 RuntimeSession 实例生命周期

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定），
以及 .git 历史里上一轮的 INC-30 提交（模块接口工件，D-017）。

INC-30 已完成：每模块导出 `ModuleArtifact`（导出函数 + 类型声明 + 私有函数），
`ngi --emit=artifact` 打印接口摘要并校验 import 边界；默认 lowering 与
`--emit=ssa` 逐字节不变（全部 example 已验证）。工件是「接口层」，见 D-017。

任务：INC-31 —— RuntimeSession 实例生命周期 + stdlib 预加载。注意**现状边界**：
- 当前语言**没有模块级 globals / init 语句**（D-009：无模块级可执行语句、
  模块级 `let`），因此「每 artifact 一个实例、初始化恰好一次」的运行时语义
  目前**无可观测载体**。不要硬造 globals 机制来满足它——那是新增语言特性，
  超出本增量范围。
- 可行的等价性路径：在 **编译期/解析期**建立 `CompilationSession` 抽象
  （resolver + 源/工件缓存的会话级所有），把 `hir::Resolver` 的每模块状态
  （functions_/structs_/enums_/traits_/opaqueTypes_/visibleNames_ 等）收拢成
  「会话内可复用、跨会话隔离」的形态；stdlib 模块的 parse+HIR 结果做会话级
  缓存（即 I7 的雏形），并以「初始化恰好一次」的**等价契约**落到工件构建
  （每个 module 的 artifact 只构建一次、循环 import 不重复构建）。
- 负契约：两个 CompilationSession 之间不得共享可变状态（构建两份 → 结果
  等价但对象独立）。

强建议：改动前后对全部 example `--emit=ssa` + `--emit=artifact` 双导出做
`diff -r` 等价性比对；ctest/ASan/guard 必须全绿。
若发现必须新增语言级 globals 才能有意义地实现，先停下向用户汇报，不要硬推。
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
