# NG — 移交说明（下一轮 agent 请从此处开始）

工作区：当前工作目录（仓库根；分支 `new-start`，**未推送**）
基线提交：`02967e4`（INC-24 part 2；其后两个提交是移交单与路径守卫）
工作区状态：**干净**（`git status --porcelain` 为 0）

本文件列出已完成的收尾状态、仍开放的缺陷（均已实测确认仍在），以及给下一轮的可直接投喂 prompt。

---

## 0. 必读约定（改动前先看）

- **AGENTS.md** 是最新的权威约定入口（已随清理更新，引用均有效）。
- **docs/ 只放 ground truth**（当前已实现、已确认的内容）。任何 planned / future / not yet / deferred / TODO 措辞禁止出现在 `docs/`。
- **不要恢复 `.agents/worklog/`、`.agents/workstate.md`、`.agents/plans/`**。
  本文件是前瞻性移交单；完成一项后从这里划掉，不要往里堆历史流水。剩余任务以下面的 prompt 为准。
- **硬约束**：`test/guard_test.cpp` 的 `kMaxCppLines = 1200`、`kMaxHeaderLines = 800`，**只拆分源文件、绝不上调上限**。当前最大：`src/native/lowering_expr.cpp` 967、`module_parser.cpp` 904、`type_interner.cpp` 895、`parser.cpp` 886。
- **提交规范**：Conventional Commits（`feat(vnext):` / `fix(checker):` / `docs(agents):` …），imperative mood；AI 生成代码须标注（见 CONTRIBUTING.md）。
- **完成前必须跑满门禁**：`ctest --test-dir build -j`（当前基线 674）+ `./build-asan/ng_test`（当前基线 673）+ guard。构建：`cmake --build build -j` / `cmake --build build-asan -j`。
  - 注意：`ninja` 在本环境常误报 "no work to do"；改动后可用 `touch <file>` 触发重编，并以**实际运行测试/探针**为准，不要相信构建输出本身。
- **已知 flaky**：`INC-18 arena ... leaks(1)` 测试依赖 `/usr/bin/leaks -atExit`，并行跑偶尔失败，单跑 4/4 通过。与改动无关，重跑即可。

---

## 1. 本轮已完成（5 个增量，全绿提交）

| Commit | 内容 |
|---|---|
| `e2bc657` | **B5** — copy 捕获仿射值要求 `Clone`，按次 clone（原为运行期 `invalid heap handle`） |
| `d622b83` | **INC-13** — `return e?` 支持 |
| `fb85e87` | **INC-14** — 空数组 `[]` 从期望类型推断 |
| `06a8f20` | **INC-24 part 1** — 声明 `Ord` + `derive(Ord)` 字段校验（DEC-E） |
| `02967e4` | **INC-24 part 2** — FlowIR 合成 `derive(Ord)` 关系比较（字典序） |

外加两个文档/计划提交：`ac812d3`（docs 大清理：plans 搬到 `.agents/plans/`、删 worklog/HANDOFF/docs/design/Internals.md、16 份 guide 重写为 truth）、`e3fd0f5`+`e9a2838`（B5 计划与归档）。

---

## 2. 仍开放的缺陷（**均已实测确认仍在**，可直接复现）

### 2.2 `?` 中部表达式仍不支持
```ng
let v = f(1)? + f(2)?;
```
→ `resolution error: ? cannot appear mid-expression`。
`return e?`、`let x = f()?`、裸语句 `f()?;` **已支持**（`d622b83`）。
仍拒绝：中部表达式、闭包体内、解构 let。中部表达式需「语句级临时变量 + 父表达式重写」，是这块里最大的一块。

### 2.3 `string` 字段的 Ord 不可用
```ng
struct P: derive(Ord) { s: string }
print(a < b);
```
→ derive 通过，但 `internal error: native lowering (M2): unsupported string binary payload 8`。
需在 native 侧为 string 加字节序（`memcmp`）比较算子 —— DEC-E 规定的字节序（`libngrt.c` 中已有 memcmp 用法可参照）。

---

## 3. 建议的下一轮顺序

按「先修已确认缺陷 → 再补半成品增量 → 再开新特性 → 最后架构类」：

1. **Prompt B** — string 字节序排序（补齐 DEC-E）
2. **Prompt C** — `?` 中部表达式（最大，单独一轮）
4. **Prompt D** — INC-33 元组 case 模式（INC-23 已解锁）
5. **Prompt E** — INC-35 Self 视图方法
6. **Prompt F** — INC-21 span 剩余
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
| **B** | string 的字节序比较（DEC-E 剩余半块） | 中 | `unsupported string binary payload` |
| **C** | `?` 中部表达式 | 大 | `? cannot appear mid-expression` |
| **D** | INC-33 元组 case 模式 | 中 | parse error |
| **E** | INC-35 Self 类型的视图方法 | 中 | 显式拒绝 |
| **F** | INC-21 剩余：field-span / span-through-ref | 中 | `span operand is not a place` |
| **G** | INC-28 `isize` / `usize` | 中 | `unknown type usize` |
| **H** | INC-30/31 模块工件 + RuntimeSession | 大（架构） | 未开始 |
| **I** | INC-29 C ABI 收尾 | 大（需先拆） | 部分实现 |
| **J** | INC-36 const fun trait bound | 小/不确定 | **基础已可用**，先探测 |

---

### Prompt B —— 补 string 的字节序比较（DEC-E 剩余半块）

```
你在当前工作目录（NG 语言）。请先读 AGENTS.md 与 HANDOFF.md（§0 约定与 §2.3）。

任务：让 `derive(Ord)` 的 string 字段真正可比较（DEC-E：字节序 = memcmp）。

现状（已确认）：
    struct P: derive(Ord) { s: string }
    print(a < b);
    → 编译通过，但 internal error: native lowering (M2): unsupported string binary payload 8
另外裸的 `"abc" < "abd"` 目前报 `comparison operand type mismatch: expected i64, got string`。

已完成的地基（不要重复做）：
- commit 06a8f20：prelude 声明 `export trait Ord { compare(self, other) -> i64 }`，checker 校验 derive(Ord) 字段（float 按 DEC-E 排除）。
- commit 02967e4：整型/bool 字段的字典序比较已端到端可用（checker 允许 Ord 类型做 ordering + FlowIR 合成）。
  example/derive_ord.ng 是可用参照。

需要做的：
1. checker 允许 string 作为 ordering 比较的操作数（src/typecheck_infer.cpp 的 ordering 分支，现允许 numericPair 或 derivedOrd）。
2. native 侧为 string 加字节序比较算子；libngrt.c 里已有 memcmp 用法可参照（如 str 的前缀/包含判断）。
3. DEC-E 规定 string 全序 = 字节字典序（UTF-8 下等价于码点序），请在 docs/ref/language-decisions.md 的 DEC-E 条目里把「string 尚未可比较」的限制句改写成已实现的 truth。

交付：修复 + 测试（含非 ASCII 的字节序用例）+ example 或契约语料 + 跑满 ctest/ASan/guard + 一个提交。
```

---

### Prompt C —— `?` 中部表达式（最大，建议单独一轮）

```
你在当前工作目录（NG 语言）。请先读 AGENTS.md 与 HANDOFF.md（§0 约定与 §2.2）。

任务：让 `?` 可用在表达式中部，例如 `let v = f(1)? + f(2)?;`。

现状（已确认）：`resolution error: ? cannot appear mid-expression`。
已支持的位置（commit d622b83，不要回退）：`let x = f()?;`、`return f()?;`、裸语句 `f()?;`。
仍拒绝：中部表达式、闭包体内、解构 let。

实现要点（我已读过代码）：
- 拒绝点在 src/hir_body.cpp 的 resolveExpression（对 TryExpression 一律抛错），
  脱糖在 Resolver::rootTryOf / Resolver::desugarTry：把 enclosing block 拆成
  scrutinee + Ok/Err switch（Ok 臂继续其余语句，Err 臂 return Result.Err($errN)）。
- 中部位置需要：为嵌套的 `?` 提取「语句级临时变量」并重写父表达式，
  本质是 desugarTry 从「只认 statement root」扩展为「遍历表达式树、按求值序提升临时值」。
- 闭包体 / 解构 let 仍可继续拒绝（decided），只需给出清晰诊断。

交付：
- 实现 + 回归测试（中部表达式的 Ok/Err 两条路径）+ example 覆盖。
- 跑满 ctest / ASan / guard（上限 1200/800 不得上调；src/hir_body.cpp 当前 693，注意拆分）。
- 同步 docs/guide/functions.md 的 `?` 章节（现写明「不允许 mid-expression」）为新的 truth。
- 建议先做最小用例（单个二元运算）跑通再扩展。一个 feat 提交。
```

---

---

### Prompt D —— INC-33 元组 case 模式

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定）。

任务：INC-33 —— 让 `case (a, b)` 解构元组 scrutinee。

现状（已确认）：
    let t = (1, 2);
    switch (t) { case (a, b) { ... } otherwise { ... } }
    → syntax error: expected `:` after binding name in type pattern
即 `(a, b)` 被当成 union 的类型模式解析了。INC-23（DEC-D union switch）已落地，
其为 union 提供 `case (name: T)` 类型模式；元组模式需与之区分（元组是结构、无需类型标注）。

实现提示：
- 解析：src/syntax/parser.cpp 的 switch case 模式解析（区分 `case (a: T)` 类型模式
  与 `case (a, b)` 元组模式 —— 后者在绑定名后是 `,` 或 `)`）。
- HIR：case 模式表示需容纳元组绑定（rest 绑定机制已存在，可复用）。
- FlowIR：src/flowir_switch.cpp 已有 enum variant / union tag / 字面量分派；
  元组模式按位置 extract 后走既有分支机制。
- 穷尽性：元组模式覆盖所有元组值；缺 `otherwise` 时的诊断请与既有穷尽性检查风格一致。

交付：实现 + PBT（元组模式匹配 ≡ 手工索引解构，用 test/pbt.hpp/INC-1 harness）
+ 穷尽性负契约 + example + 跑满 ctest/ASan/guard + 一个 feat 提交。
```

---

### Prompt E —— INC-35 Self 类型的视图方法

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定）。

任务：INC-35 —— 允许通过 `ref<Trait>` 视图调用返回 `Self` 的方法。

现状（已确认）：
    trait Dup { fun dup(self: Self ref) -> Self; }
    fun use(v: ref<Dup>) -> i64 { let d = v.dup(); ... }
    → type error: trait object methods with Self in the return type are not yet supported

目标：视图上调用 `clone()` / `dup()` 这类 `-> Self` 方法时，按 per-concrete 实例化
填入 vtable，返回与具体类型一致的独立值（深拷贝语义）。

实现提示：
- 拒绝点：搜索该错误文本（typecheck 侧，trait object / Self 返回类型检查）。
- `ref<Trait>` 的 per-concrete 视图表机制已在（traitViewTables_），可扩为携带
  Self 返回方法的实例化。
- 负契约应保留：返回未约束 `Self` 的抽象调用（无法确定具体类型）仍须拒绝。

交付：实现 + 契约（视图上 clone 返回独立深拷贝；源不受影响）+ 负契约
+ 跑满 ctest/ASan/guard + 一个 feat 提交。
```

---

### Prompt F —— INC-21 剩余：field-span / span-through-ref / span-of-span

```
你在当前工作目录（NG 语言）。先读 AGENTS.md 与本目录 HANDOFF.md（§0 约定）。

任务：INC-21 剩余切片 —— 让 span 支持 struct 字段与引用路径。

现状（已确认）：
    let mut s = S { xs: [1, 2, 3] };
    let w = span(ref mut s.xs);
    → type error: span operand is not a place
（`span(ref mut arr)` 直接对局部数组是可用的；example/span.ng 正常。）

需要扩展：field-span（`span(ref mut s.field)`）、span-through-ref（经由 `T ref` 取 span）、
span-of-span。三者同源，都是「span 的 place 路径需要支持非零步」。

实现提示：
- 拒绝点：搜索 "span operand is not a place"，当前只接受零步 place。
- FlowIR 已有 place steps 机制（MakeRef 处理 placeSteps），span 可复用。
- 注意：span 是非逃逸视图，借用/loan 规则需与既有 ref 一致（不应放宽）。

交付：实现 + 测试（字段 span 读写、经引用取 span、span of span 至少负契约明确）
+ example 扩展 + 跑满 ctest/ASan/guard + 一个 feat 提交。
```

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
