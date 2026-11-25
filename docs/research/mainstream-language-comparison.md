# NG 与主流语言设计与实现对比研究

> **Status:** research snapshot — 2026-08-25；**Rev.2** 同日并入对抗性审查修正
> （F1 扩为三处、F2 建议改写、I5 拆分、M3 前置条件）；**Rev.3** 并入全量审校
> 修正（泄漏数字换为自包含可复现探针、深拷贝表述改为位点式、SE 编号勘误、
> 引用归属修正）；**Rev.4** 同日 doc-alignment（AGENTS.md/07-plan 过期描述
> 已修复，见 §3.2 注记与 §8 缺口项）。
> **AI-assisted document:** drafted with AI assistance; all claims carry local
> evidence (`file:line`) or external citations with retrieval dates. See the
> confidence statement in §8.

## 1. 研究问题与边界

**问题：** 与 Rust、C++、OCaml、Scala（辅证：Swift、Kotlin、Zig、Haskell）相比，
NG vNext 在**语言设计**与**实现架构**两个层面，有哪些：

1. **有缺陷的设计**（F 类）——当前语义会产生错误结果、资源失控或语义双轨；
2. **可改进的设计**（I 类）——方向正确但存在更优做法或实现债务；
3. **缺失的设计**（M 类)——对比语言视为"第一天标配"、且 NG 未规划或仅登记未排期的能力。

**硬约束：** 所有建议必须满足"不显著提升实现复杂度"。本报告的判断基准是：
单人维护规模（当前 src+include ≈ 18.9k 行）、现有架构（syntax → HIR → side-table
checker → FlowIR CFG → QBE 原生层）、不引入新 IR 层/全局类型推断/effect 系统/
生命周期语法/GC。每条建议标注复杂度增量（低 = 数百行内、无新机制；中 = 有新机制
但落在既有扩展点；高 = 超出基准，**只登记不推荐**）。

**边界外：** 并发运行时（R10，设计上明确不稳定）、LSP/formatter（R11）、性能调优。
这些只在影响上述判断时提及。

## 2. 方法与证据

1. **本地材料：** `docs/design/rearchitecture/` 全部决策文档（D-001~D-015、gap
   register、post-cutover plan）、`example/*.ng` 46 个示例、`lib/std/*.ng` 8 个
   标准库模块、关键头文件与实现（`value.hpp`、`typecheck.hpp`、`flowir.hpp`、
   `typecheck.cpp`、`flowir.cpp`、`driver.cpp`、`native/lowering.cpp`、
   `native/libngrt.c`）。
2. **基线验证：** `ctest --test-dir build -j 8` 于 2026-08-25 运行，456/456 通过；
   `./build/ngi example/heap_box.ng` 端到端原生编译执行正常。
3. **外部来源：** 官方文档与一手工程资料优先；检索日期均为 **2026-08-25**。
   关键外部事实（用于交叉验证，非 NG 结论的依据）：
   - Scala 3.8 capture checking 仍为 experimental，官方文档自述 "still highly experimental and unstable"（`docs.scala-lang.org/scala3/reference/experimental/cc.html`）；
   - OCaml 5 effect handlers 官方标注 experimental 且 unchecked（`ocaml.org/manual/5.2/effects.html`）；OCaml 5.5 发布 modular explicits（CWN 2026-06-30）；
   - C++ pattern matching 提案 P2688 未进入 C++26，目标改为 C++29（wg21.link/p2688；cppreference C++26 feature list 不含该项）；
   - Rust NLL 自 1.63 起为唯一 borrow checker；Polonius alpha 已上 nightly，
     2026 年项目目标为稳定化，接受 10–20% 检查开销（`rust-lang.github.io/rust-project-goals/2026/polonius.html`）。
4. 对比语言的行为描述基于各自官方参考（Rust Reference / cppreference /
   OCaml manual / Scala 3 Reference / Swift Book / Kotlin Docs / Zig LangRef）。

## 3. NG 现状快照（事实基线）

### 3.1 设计面

- 类型：内建数值塔 i8–i64/u8–u64/f32/f64 + bool/unit/string；结构体、带负载枚举、
  别名、newtype、opaque、trait/impl（含泛型 impl、supertrait、default method、
  auto trait、derive）、匿名 union `A | B`、HKT 参数 `F<_>`、变参包 `T...`、
  const 泛型与 const 谓词特化（`= delete`、pattern specialization）、where 子句
  （D-001/D-011/D-012/D-014）。
- 所有权：Copy-first（标量/string/tuple Copy，集合按值深拷贝；名义类型 affine，
  `move`/`clone`，字段感知 partial move，Drop 恰好一次）；scoped `ref`/`ref mut`
  不可返回、不可存聚合；首片 NLL 释放分析；生命周期语法永久禁止（D-002/D-015）。
- 错误模型：`Result<T,E>` 用户自定义 + 计划中的 `?`（D-007 **未落地**）；panic
  trap 分离。prelude 中无 Option/Result。
- 控制流：`loop (binder = init)` + `next` 双重语义（循环重绑定 / 自尾递归）
  （D-010）；switch 仅语句（enum 变量解构 + 标量 literal-or + otherwise，
  语句地位是实现现状而非决策记录）；**无 while、无 break、无 guard、
  switch 不能作表达式**。
- 表达式能力：fold 内建 `f(acc, xs...)`、map comprehension `[f(xs)...]`、过滤
  `[f(xs)?...]`、数组 spread/slice/range——全部是**内建特例而非库函数**
  （`example/folds.ng`、`map_comprehension.ng`）。闭包 D-006 已接受**未实现**。
- 模块：文件即模块，transitive import 合并为单一编译单元，可见名集合控制隐私；
  无接口工件、无独立编译（A2 未做）。

### 3.2 实现面（AGENTS.md / 07-plan 的流水线描述已于 2026-08-25 对齐修订）

> **Rev.4 对齐注记**：本节最初记录了 AGENTS.md 与 07-post-cutover-plan §1
> 的过期流水线描述；该问题已在 doc-alignment pass 中修复（AGENTS.md 改为
> QBE-only 流水线与现行命令，07 加历史保留横幅 + INC 映射）。以下为对齐后
> 仍然成立的事实基线。

- 当前唯一执行路径是 **QBE 原生编译**："There is no bytecode/VM tier; every
  program is lowered to QBE IL, assembled, linked against libngrt, and run as a
  native executable."（`src/driver.cpp:3-5`）。`src/bytecode/` 为空目录，
  `src/vm.cpp` 不存在（AGENTS.md 与 07-plan 的对应过期描述已在 Rev.4 对齐中
  修复/加横幅）。
- checker 是 side-table 风格（`TypeCheckResult`，`include/typecheck.hpp:248-300`），
  但同时维护 `expressionTypes`（字符串）与 `expressionTypeIds`（TypeId）双轨；
  FlowIR/原生层仍在多处依赖字符串名解析。
- 单体文件：`typecheck.cpp` 4537 行、`native/lowering.cpp` 2924 行、
  `flowir.cpp` 1882 行。
- 原生层值表示：标量走 QBE word/double；聚合 malloc `{len,cap,elements}` 结构；
  **按值语义在绑定/传参/返回等位点深拷贝**（部分直通路径可省略克隆，见 F2
  Rev.3 修正），字符串拼接等构造位点逐次分配且不释放（clone helpers
  `ngrt_arr_clone_words/_strings` 等，`src/native/lowering.cpp:342-394`）；
  checked 算术 helper（同文件 :136-340）。
- **生成代码不含任何 free/Drop-fini 调用**（对 `lowering.cpp` grep `free|drop`
  仅命中注释与 Drop 相关的普通方法调用边）：Drop impl 的调用作为普通函数调用
  进入 FlowIR（`flowir.cpp:67,1291,1398` 使用三个 drop 边表），但数组/字符串的
  中间深拷贝从不释放。
- 被 `MakeRef`/`MakeTraitView` 触碰过的 local 强制升级为**永久堆单元**
  （`lowering.cpp:1128` "locals rooted by MakeRef/MakeTraitView need heap cells"）。
- union 在运行期装箱为 {成员序号, 负载}（`bindUnionBox`，`lowering.cpp:818`），
  即已有动态 tag——补 union switch 不需改表示。
- 测试：49 个 Catch2 套件 + 示例全量 sweep，456 用例全绿（2026-08-25）。

## 4. 定位矩阵（一行一维度）

| 维度 | NG | Rust | C++ | OCaml | Scala |
|---|---|---|---|---|---|
| 多态形式 | 单态化 clone+recheck；HKT/变参/const 泛型 | 单态化 + trait bound | 模板 + C++20 concepts | 参数多态 + functor（无 HKT） | JVM erased + given/using |
| 动态分发 | 仅 `ref<Trait>` 视图 | `dyn Trait` | 虚函数 | 对象方法动态分派（`obj#m`），惯用面有限 | 一切皆引用 |
| 所有权/内存 | Copy-first 深拷贝 + affine 名义类型；无 GC/RC | affine + borrow check | 手工 RAII | GC | GC |
| 生命周期 | 隐藏 origin，禁语法 | 显式 `'a` + elision | 引用计数/约定 | 无概念 | 无概念（cc 实验中） |
| 错误处理 | Result 枚举 + `?`（未落地）+ panic trap | Result/panic + `?` | 异常/expected | exception + result | Try/Either + exception |
| 编译期求值 | const 谓词特化 + `const fun` HIR 解释器 | const fn + 泛型 | constexpr/模板特化 | 无（白盒折叠极浅） | inline/宏/match type |
| 模式匹配 | enum 解构 + 标量 or；语句非表达式；无 guard | match 表达式全功能 | 无（P2688 目标 C++29） | match 全功能 | match 全功能 |
| 迭代抽象 | 内建 fold/comprehension 特例 | Iterator trait + assoc type | range + concepts | fold/Stream（stdlib） | Collection + for 推导 |
| 模块/编译 | 整程序合并编译，源码级 import | crate + rlib 元数据 | 头文件/C++20 modules | .cmi/.mli 独立编译 | TASTy + classpath |
| 数值安全 | 默认逐宽 checked（含 const）※u64 路径存在 F1①③ 缺陷待修 | debug panic/release wrap | UB | 环绕（int）/NaN | 环绕/溢出静默 |
| 尾调用 | `next` 保证 TCO | 无保证 | 无保证 | 保证 | JVM 无保证 |
| 字符串 | 不可变共享 Copy、字节串、无 char 类型 | UTF-8 + char 明确 | 可变 std::string | 不可变字节串 | UTF-16（JVM） |

补充语言的旁证在附录中随维度给出（Swift 的 ARC/CoW 与 trap 算术、Kotlin 的
null 安全与 UTF-16 教训、Zig 的 comptime、Haskell 的 HM 推断代价）。

## 5. 发现总表

严重度：高=正确性/可用性根基；中=显著摩擦或债务；低=打磨。
复杂度：低/中/高（见 §1 判断基准）。"已规划"指 post-cutover plan 已有对应项。

### F — 有缺陷的设计

| ID | 发现 | 严重度 | 修复复杂度 | 详情 |
|---|---|---|---|---|
| F1 | 数值三处缺陷：①u64 排序比较编译为有符号指令（`u64::MAX > 5` 为假）；②混宽 int/float 比较隐式经 double（`u64(2^53+1) == f64(2^53)` 为真）；③u64 算术溢出静默回绕（`u64(2^64−8)+20` 得 12 无 trap，违反 D-008）——①③同根同源应同 PR 修复 | 高 | 低 | 附录 A §A1 |
| F2 | Copy-first 深拷贝 + 原生层零释放：堆单调增长；`ref` 触碰的 local 永久堆化 | 高（长期） | 低→中（arena 先行） | 附录 B §B1 |
| F3 | 匿名 union 与 enum 双轨制能力不对等：union 有运行期 tag 却不能 switch/穷尽检查 | 中 | 低–中 | 附录 A §A2 |
| F4 | std.list 的跨帧引用链正确性依赖"泄漏型永久堆单元"这一实现巧合 | 中 | 随 B1 解决 | 附录 E §E1 |
| F5 | 无 while/break/label；搜索类循环被迫用标志位或递归；loop 无表达式形式 | 中 | 低 | 附录 C §C1 |
| F6 | f32 以 double 计算再收窄：单运算结果安全但未写入规范、无常量路径一致性测试 | 低 | 低 | 附录 A §A3 |

### I — 可改进的设计

| ID | 发现 | 回报 | 复杂度 | 详情 |
|---|---|---|---|---|
| I1 | 诊断基建：错误码/notes/did-you-mean/多行 span 缺位 | 高 | 低 | 附录 E §E2 |
| I2 | TypeCheckResult 字符串/TypeId 双轨收敛（TYP-07 残留） | 中 | 低–中 | 附录 D §D1 |
| I3 | FlowIR 含 std.list 专用指令 `EnumListLength/EnumListGet`（泄漏抽象） | 中 | 中 | 附录 D §D2 |
| I4 | switch 表达式化 + case guard（穷尽性检查已具备，复用即可） | 高 | 中 | 附录 C §C2 |
| I5 | derive 扩容：Eq/Show 可立即（Show 排在 I6 之后）；**derive(Ord) 受阻于两个语义决策**——NG 字符串无 Ord（`"apple" < "banana"` 为类型错误）、枚举变体无序数语义 | 高 | Eq/Show 低；Ord 中 | 附录 A §A4 |
| I6 | stdlib 基础缺口：i64↔string 转换、格式化最小集 | 高 | 低 | 附录 E §E3 |
| I7 | 编译缓存：stdlib/未变更模块的 parse-HIR 缓存（A2 的前置简化版） | 中 | 低–中 | 附录 D §D3 |
| I8 | ASAN/UBSAN 测试变体；让 F2/F4 类问题在 CI 可见 | 中 | 低 | 附录 E §E4 |

### M — 缺失的设计（对比语言"第一天标配"）

| ID | 能力 | 状态 | 优先级论证 | 详情 |
|---|---|---|---|---|
| M1 | prelude Option/Result + `?` | D-007 已接受未落地；plan 外提前建议 | 四语言皆以可选/结果类型为一等公民；当前每个示例手写 Result 枚举，stdlib 可用性最大单点 | 附录 B §B2 |
| M2 | 闭包 + Fn/FnMut/FnOnce 族 | D-006 已接受未实现 | 没有闭包则 map/filter/fold 无法库化，只能停留在内建特例；这是与四语言差距最大的单点 | 附录 C §C3 |
| M3 | while/break/labeled break | plan 外新增（F5 修复面）；**前置条件**：必须先定义 while 与裸 `next` 的捕获规则（D-010 最近循环优先会让 while 体内的尾递归静默变 continue） | FlowIR 已有 Branch/Jump，纯语法糖成本 | 附录 C §C1 |
| M4 | char 类型与字符串迭代语义（bytes vs chars） | plan 外新增 | Kotlin UTF-16 与 Rust bytes/chars 双 API 是正反教材；先定语义再写文档 | 附录 E §E5 |
| M5 | 内部可变性原语（Cell 类） | plan 外新增 | D-006 ref mut 捕获闭包落地后立即需要；Copy-first 下可用轻量方案 | 附录 B §B3 |
| M6 | 错误恢复解析 | SYN-06 已登记，R11 前置 | 只登记；LSP 启动前必须 | 附录 E §E6 |
| M7 | associated types / Iterator 故事 | 远期登记，**明确现在不做** | 无 assoc type 则 trait 无法表达 `Item` 迭代协议；等 B1/R10 后再议 | 附录 A §A5 |

## 6. 关键论证摘要

**F1（数值）** 含三个缺陷，均经可复现实验证实（2026-08-25，
`./build/ngi <probe>.ng`）：
①整数排序比较一律发射**有符号** QBE 指令（`csltl/cslel/csgtl/csgel`，
`src/native/lowering.cpp:2670-2675`；全文件无 `cu*` 无符号变体），因此
`let big: u64 = 18446744073709551615; if (big > 5)` 判定为假（实测返回 0；
u8/u16/u32 因槽内值恒非负而不受害，仅 u64 受害）；②任一侧为浮点的比较走
double 路径（lowering 的 `sltof` 转换 + 浮点比较指令，
`src/native/lowering.cpp:2537,2632-2656`），`u64(9007199254740993) ==
f64(9007199254740992.0)` 静默判等——
精度损失隐式且不可拒绝（对抗性审查补充：混宽整数排序同样错误，
`u64(2^63+1000) > i64(5)` 为假，单一 QBE 指令对混合符号无正确解，
需归一化策略）；③**u64 算术溢出静默回绕**：`u64(18446744073709551608) +
20` 返回 12 无 trap——checked_add 的符号检测遇"读为负"的操作数失效，
且 u64 不发射任何宽度检查（对照 u8 `200+200` 被 `ngrt_check_w_u8` 正确
trap）。③违反 D-008 "Ordinary integer arithmetic is checked" 承诺。
修复：①按符号性选无符号指令、③为 u64 补宽度检查或改用无符号检测——
同根同源同 PR；②checker 禁止 int/float 隐式混比 + 显式转换 API +
混宽整数归一化策略。均为局部改动。

**F2（内存）** 语言层承诺"deep-copy 语义 + Drop 恰好一次"，但原生层只对
Drop-impl 类型发调用；聚合/字符串的中间拷贝永不归还（§3.2 证据）。泄漏已
定量证实且可自包含复现——字符串拼接循环 `s := s + "."`：1 万次迭代
maxRSS≈54MB、5 万次≈1.41GB（2026-08-25 `/usr/bin/time -l` 实测，
探针全文见附录 B）。注意两点边界：(a) 克隆发生在特定位点（聚合构造、
ref/cell 写入、字符串拼接等），参数直通返回与 mut 自重绑定**不**克隆
（Rev.3 全量审校 SSA 实证）——这影响未来 arena/RC 改造的插桩位置；
(b) ng_ide 通过子进程编译（libngrt `systemOutput`/popen 启动 ngi 子进程，
driver 内部再 fork qbe/cc，`driver.cpp:107-138`），跨运行累积被进程回收
缓解——风险集中在**单次运行内的长循环**与未来的进程内嵌入。
修正后的方案分层：exit-arena 仅是卫生改进（分配速度/碎片 + 让 ASAN 泄漏
报告干净，配合 I8）；长驻/大循环问题的真解是 B1 的 RC/CoW 提前评估，以及
一条此前遗漏的路——利用现有 scoped-ref 机制把大 Copy 聚合的参数传递改为
非逃逸视图（Hylo mutable value semantics 的最小片段，直击传参拷贝）。

**M1/M2 优先级（路线图输入的核心结论）** post-cutover plan 把 A1（完整 loan
analysis）排在首位。从跨语言证据看，**闭包（M2）与 Result/?（M1）才是用户可见
生产力的最大瓶颈**：四门主对比语言无一例外第一天就有一等函数与错误传播；NG 连
`map` 都是内建特例（`[f(xs)...]`），fold 是内建语法而非高阶函数。且 M1 不依赖
loan analysis（Result 是普通枚举 + 语法糖；Rev.2 审查已用 probe 实证：
泛型枚举 switch + affine payload + 嵌套 early-return 今天全部工作，
flowir 三张 drop 边表足以承载 `?` 脱糖），可以先行；M2 按 Rev.2 意见拆为
两步——copy/move 捕获的**最小闭包独立先行**（不依赖 loan 分析即可解锁
map/fold 库化），borrow 捕获再作为 A1 的驱动测试集，避免把最大能力单点
绑死在最长实现杆上。

**方向性正面结论（防止过度修正）** 以下 NG 选择相对主流是**更简而非更差**，
建议坚持：隐藏生命周期的 scoped-ref 路线（Scala capture checking 实验 4 年仍未
稳定、Polonius 需 10–20% 开销预算，均佐证显式生命周期/区域系统的代价）；`next`
保证尾调用（C++/Rust 均无此保证）；默认 checked 算术（对齐 Swift trap，优于
C++ UB 与 Rust release wrap；u64 缺陷见 F1①③，属实现缺口而非方向错误）；
monomorphization clone+recheck（规避 Scala
specialization 的工程泥潭）；QBE 小后端（工具链复杂度比 LLVM 系低一个数量级）。

## 7. 建议行动排序（与 post-cutover plan 合并视角)

以下为**调整建议**而非替代 plan；每条标注与 plan 的关系：

1. **立即（低复杂度、独立于一切）**：F1①③ 数值正确性修复（同 PR）+
   F1② 混比诊断与归一化策略；F5/M3 while/break 语法糖
   （先落 M3 的 next 捕获规则决策）；I6 stdlib 数值转换；
   I5 收窄为 derive(Eq) + derive(Show)（Show 排在 I6 后）；I8 ASAN CI 变体。
2. **提前于 plan**：M1 prelude Result/Option + `?`（D-007 本就不阻塞 A1；
   建议提到 A1 之前或并行）。
3. **合并执行 → 两步走（Rev.2 调整）**：M2 最小闭包（copy/move 捕获）
   独立先行；borrow 捕获与 A1 loan analysis 作为同一垂直切片
   （捕获规则就是 loan analysis 的最佳测试集）。
4. **B1 前置决策（Rev.2 改写）**：exit-arena 降格为卫生改进，与 I8 同批；
   长驻/大循环内存问题的候选路线按序评估——(a) 聚合头 RC/CoW 提前评估，
   (b) 大 Copy 聚合参数传递的非逃逸视图切片（Hylo 式，早于 R6 可做）。
5. **随 B1 解决**：F4 std.list 迁移到 `Box<T>`；I3 移除 FlowIR List 专用指令。
6. **登记不动**：M6、M7、union switch（F3，等 enum/union 角色讨论后统一处理）。

## 8. 置信度、矛盾与缺口

> **Rev.2（2026-08-25）**：并入一轮独立对抗性审查的结果。审查复现了
> 全部 F1 probe、逐字核对了抽查行号与文件行数、定量证实 F2 泄漏、实证 M1
> 机械件齐备，并新发现 F1③（u64 算术回绕）、推翻了 F2 原 arena 表述对
> 长驻场景的有效性、发现 I5(Ord) 的地基依赖与 M3 的 next 捕获陷阱、确认
> runNgi 已移除（de95fa1）。
> **Rev.3（同日）**：并入一轮**全量覆盖式审校**（35 处引用逐一核对、10 组
> 运行期声明全部重验、21 条建议逐条合规裁决）——判定 pass-with-edits，
> 无 must-fix；6 处 should-fix 已折入：泄漏数字换为自包含探针（附录 B）、
> "每次深拷贝"改为位点式表述、SE-0380/SE-0185 勘误、libngrt helper 归属
> 修正、asNumber 证据链标注过期、矩阵数值安全格加 F1 脚注。

- **高置信**（本地证据直接支撑）：F1（三处均含可复现 probe）/F3/F5/F6/
  I1-I4/I6-I8/M3-M5 的事实陈述与行号证据；§3.2 的流水线现状。
  Rev.3 全量审校：35 处引用零虚构、计数精确到个位、三条错误消息文本逐字命中。
- **研究过程中的修正**：F1 的初始假设是"所有数值比较经 double"——实验证明
  同宽整数比较是精确的，真实缺陷为"无符号排序用有符号指令"+"int/float 隐式
  转换"两项；报告已按实验结果修正。这提示：涉及运行期语义的结论应以
  probe 实验为准，静态阅读代码可能误判路径。Rev.2 审查再次印证该教训
  （u64 算术回绕即静态阅读漏掉的第三处）。
- **中置信**：F2 的长期影响判断（单次运行内风险已定量，进程内嵌入属前瞻）；
  M1/M2 的优先级排序（价值判断，取决于项目目标偏工具链还是偏语言完成度）；
  F2 三条候选路线的取舍（需 R6 设计期定夺）。
- **矛盾点**：D-015 文档声称 Drop "runs exactly once on every exit edge"，
  实现上仅覆盖 Drop-impl 类型且不释放聚合存储——两者并不矛盾（语义 vs 内存
  管理），但文档未声明"过渡期程序内存只增不减"，建议补记。
- **缺口**：(1) 未做大规模程序的性能/内存实测，F2 严重度缺少定量数据；
  (2) union 的语义边界（能否 switch、tag 如何暴露）官方决策文档完全空白，
  本报告只能给出现状描述；(3) 补充语言（Swift/Kotlin/Zig/Haskell）仅作旁证,
  未逐项深挖；(4) ~~AGENTS.md 过期问题~~ 已解决——2026-08-25 doc-alignment
  修订了 AGENTS.md（QBE-only 流水线、现行命令、目录表）并为
  07-post-cutover-plan 加历史保留横幅与 INC 映射。

## 9. 附录索引

- [Appendix A — 类型系统、泛型与 const](appendix-a-type-system-generics-const.md)
- [Appendix B — 所有权、引用与错误处理](appendix-b-ownership-references-errors.md)
- [Appendix C — 控制流、模式匹配与表达式能力](appendix-c-control-flow-patterns-expressions.md)
- [Appendix D — 模块系统与实现架构](appendix-d-modules-runtime-implementation.md)
- [Appendix E — 标准库、诊断与工程基建](appendix-e-stdlib-diagnostics-tooling.md)

### 主要外部参考（检索日期 2026-08-25）

- Rust Project Goals 2026 — Polonius: https://rust-lang.github.io/rust-project-goals/2026/polonius.html
- Rust RFC 2094 (NLL): https://rust-lang.github.io/rfcs/2094-nll.html
- rustc dev guide — Borrow checking: https://rustc-dev-guide.rust-lang.org/borrow_check.html
- Scala 3 Reference — Capture Checking (experimental): https://docs.scala-lang.org/scala3/reference/experimental/cc.html
- OCaml manual — Effect handlers: https://ocaml.org/manual/5.2/effects.html
- OCaml Weekly News 2026-06-30（OCaml 5.5 modular explicits）: https://alan.petitepomme.net/cwn/2026.06.30.html
- cppreference — Constraints and concepts (C++20): https://en.cppreference.com/w/cpp/language/constraints
- Swift Programming Language — Automatic Reference Counting: https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/
- Kotlin Docs — Null safety: https://kotlinlang.org/docs/null-safety.html
- Zig Language Reference — comptime: https://ziglang.org/documentation/master/#comptime
