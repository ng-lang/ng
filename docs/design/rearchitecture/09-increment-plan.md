# Implementation Increment Plan — 研究发现 × Post-Cutover Plan 合并路线

> **Status:** planned, not started. Converged with maintainer 2026-08-25.
> **AI-assisted document:** drafted with AI assistance.
>
> **输入源:** [docs/research/mainstream-language-comparison.md](../../research/mainstream-language-comparison.md)
> (Rev.3, F/I/M 发现) ∪ [07-post-cutover-plan.md](07-post-cutover-plan.md)
> (A/B/C 类)。排序主轴采用研究报告建议（用户可见价值早交付）；覆盖终点为
> **全部 B 类完成**；R10/R11 仅留占位。
>
> **已确认的取舍:** ① 报告排序主轴；② B1/B2/B3 细粒度入计划；
> ③ PBT 采用自研轻量 harness（Catch2 内随机生成器 + 不变量断言，无 shrink，
> 记录 seed 保证可复现）；④ 契约测试全套三层（.ng 语料断言 / 负诊断契约 /
> Const-vs-Native 差分）；⑤ 五个前置语义决策已于 2026-08-25 讨论敲定
> （见 §0b），正式 D-0xx 文档修订随对应实现增量同 commit 落盘。

## 0b. 已定语义决策记录（2026-08-25）

| ID | 决策 | 要点 | 影响增量 |
|---|---|---|---|
| **DEC-A** | **统一禁止隐式混比** | i64↔u64 与 int↔float 的**变量间**隐式比较均为编译错误；同符号性跨宽比较保留（i64 槽内已精确）；字面量上下文采用保留（`f == 1` 中字面量采纳 f64）；提供 `toFloat` / `toSigned` 显式转换 natives。对齐 Rust/Kotlin；零静默错误路径 | INC-6 |
| **DEC-B** | **while 不捕获裸 next + 新增 `continue`** | 裸 `next` 只被带绑定器的 `loop` 捕获（尾递归规则不变）；while 体内尾递归仍指向函数；新增无参 `continue` 关键字仅用于 while；`break` 在 loop/while 两处通用（unit 值） | INC-10 |
| **DEC-C** | **堆存储终态 = 侵入式引用计数 + CoW** | 聚合存储头加 refcount，Copy 赋值共享不可变存储、写时复制（Swift 验证的模式，D-015 已预留 CoW 条款）；利于持久结构（List 节点共享）与 R10 并发。exit-arena（INC-18）定位为**过渡脚手架**：仅卫生层，RC/CoW 落地后拆除，不与其长期并存 | INC-18, INC-19, R6 |
| **DEC-D** | **union 补齐为完整公民** | union 可作 switch scrutinee：case 类型模式绑定 `case (x: i64)`、成员字面量模式、otherwise；穷尽性按静态成员集检查（bindUnionBox 运行期 tag 已就绪）。与 enum 的角色分界：enum case 解构变体负载，union case 按成员类型匹配绑定值 | INC-23 |
| **DEC-E** | **字节序 + 声明序** | string Ord = 字节字典序（memcmp；UTF-8 编码保证其等价于码点序，写入规范）；枚举声明序 = 序数，derive(Ord) 先比序数再逐字段字典序；**float 字段 v1 排除**（NaN 破坏全序，IEEE totalOrder 留待未来决策），derive(Ord) 遇 float 字段报诊断 | INC-24 |

## 0. 通用约定（每个增量的验收门模板）

每个 INC 必须同时满足以下六项才算完成：

1. **端到端**: 语法/HIR/checker/lowering/QBE 原生执行全链路打通，
   有至少一个 `example/<topic>.ng` 可 `./build/ngi` 运行；
2. **测试**: 新增行为有测试覆盖——凡可生成输入处用 PBT（INC-1 harness），
   凡可观测行为处用契约语料/负诊断（INC-2/3 harness）；
3. **绿门**: `./build/ng_test` 与 ASAN 变体（INC-0 后）全绿；
4. **文档同步**: 同 commit 更新 `04-language-decisions.md` 状态、
   `05-legacy-example-migration-matrix.md` 行、本计划勾选、`README.md` 进展板；
5. **无回归**: 既有 456 用例不回退（差分契约对既有内建行为提供持续守护）；
6. **小而完整**: 单个 INC 预期 ≤ 数百行编译器改动；超出即再拆。

**决策增量（DEC-x）** 是特殊 INC：产出为 D-0xx 文档修订 + 反例测试集 +
（如适用）负契约语料；不含实现。实现 INC 引用其结论，不得绕过。

## Phase 0 — 基建（harness 预置，先行落地）

### INC-0 · ASAN/UBSAN 构建变体与泄漏基线 〔研究 I8〕 ✅ 2026-08-26
- **产出**: CMake target/preset `ng_test_asan`（`-fsanitize=address,undefined`）；
  泄漏探针语料（附录 B 的字符串拼接 probe 入 `test/`）。
- **动作**: 加构建变体；跑全量套件记录当前泄漏/UB 基线报告存档。
- **验证**: asan 变体 ctest 全绿（或已知问题清单化）；leak probe 在 asan 下
  给出确定的泄漏计数。
- **价值**: 让 F2/F4 类缺陷在 CI 可见；为 INC-18 arena 改造提供前后对照。
- **执行记录**: `NG_ASAN` CMake 选项 + `asan` preset（build-asan）；asan 变体
  458/458 全绿。平台偏差：arm64 macOS 的 Apple clang 无 LeakSanitizer，
  确定泄漏计数改由 macOS `leaks(MallocStackLogging)` 对 `--output` 产物给出
  （探针 20000 次拼接 → 19999 leaks ≈205MB；maxRSS ≈207MB）。基线存档于
  [10-baselines.md](10-baselines.md)。首跑即修复 typecheck.cpp 一处真实
  heap-use-after-free（lastUseCache_ 嵌套 clear 悬垂引用）。

### INC-1 · 自研轻量 PBT harness 〔新增基建〕
- **产出**: `test/pbt.hpp`——seeded PRNG（seed 可注入/打印）、数值域生成器
  （各宽度含 INT_MIN/u64::MAX 等边界权重）、字符串/数组生成器、NG 源片段
  生成器骨架、property 宏（失败时打印 seed 与最小复现输入）。
- **验证**: harness 自测属性 ≥5 条（如生成整数经 i64 往返恒等）随套件运行。
- **限制声明**: 无 shrink（失败时输出 seed 供人工最小化）；文档写明。
- **价值**: 后续所有 INC 的 PBT 前提。

### INC-2 · 契约语料 runner（层 1+2）〔新增基建〕
- **产出**: `test/conformance/<case>.ng` + `<case>.expected`（stdout/exit）+
  负诊断契约格式（消息模式 + span 区间断言）；Catch2 runner
  `test/contract_test.cpp`。迁移 3-5 个现有示例行为进语料作为种子。
- **验证**: 种子语料全绿；一条负契约（`use of moved value`）按码+span 断言。
- **价值**: 行为规格的唯一权威载体，后续 INC 直接追加语料。

### INC-3 · 差分契约：ConstInterpreter ↔ QBE 原生（层 3）〔新增基建〕
- **产出**: 差分 runner——同一模块双路径求值比对；受限随机程序生成器
  （const 安全子集：i64 算术/if/loop/递归/调用），N≥500/次，seed 固定可选。
- **验证**: 500 个生成程序双路径结果零分歧；人为注入一处 lowering 偏差
  能被捕获（自检）。
- **价值**: 当前架构独有的免费 oracle 对；后续任何语义改动自动受守护。

## Phase 1 — 数值正确性（研究 F1①③②）

### INC-4 · F1① 无符号排序比较指令
- **动作**: lowering 按 TypeId 符号性发射 `cuslt*` 系列；u8–u32 回归。
- **测试**: PBT「NG 比较 ≡ 宿主参考语义」逐宽度随机对（含边界加权）；
  契约语料 `numeric-ordering.ng`；probe `u64::MAX > 5` 翻转为真。
- **依赖**: INC-0/1/2。

### INC-5 · F1③ u64 算术宽度检查
- **动作**: 补 `check_w_u64` 事后检查或无符号检测路径（与 INC-4 同族同 PR 可）。
- **测试**: PBT 溢出/trap 等价性（trap ⟺ 宿主判定溢出，逐宽度）；
  契约 `numeric-wrap.ng`（期望 trap）；`u64(2^64−8)+20` 不再静默。
- **依赖**: INC-4。

### DEC-A · 混宽比较策略 — **已决策：统一禁止隐式**（见 §0b）
- 剩余工作（随 INC-6 同 commit）：D-008 修订文本 + 反例语料转正为负契约。

### INC-6 · F1② 混比禁令与显式转换落地
- **依赖**: DEC-A（已定）。
- **动作**: checker 对 i64↔u64 / int↔float 变量间隐式比较报编译错误；
  新增 `toFloat` / `toSigned` 显式转换 natives；排查并迁移受影响用法
  （预计集中在 float_test/numeric_types_test 的混比断言；unions.ng 的
  union-vs-字面量对照属成员类型匹配，不受 DEC-A 影响、无需迁移）。
  DEC-A 的 D-008 修订文本随本步同 commit 落盘。
- **测试**: 负契约（两类混比的错误码 + span）；PBT：任意生成比较表达式
  要么类型合法且语义精确、要么有确定诊断，无第三态。
- **E2E**: numeric-mixed.ng 示例展示显式转换路径。

## Phase 2 — stdlib 与工效

### INC-7 · I6 toString / parseInt（const-capable natives）
- **动作**: 经 D-012 `= native` 通道注册纯宿主（regexMatch 先例）；
  `toString(i64|i32|f64|bool)`、`parseInt(string) -> Result<…>` 形态预留
  （Result 由 INC-12 定型前先返回 Option 式或临时枚举，INC-12 迁移）。
- **测试**: PBT 往返 `parseInt(toString(n)) == n` 全宽度域含 INT_MIN；
  边界契约（空串/+/-/溢出/非数字）。

### INC-8 · derive(Eq) 〔研究 I5 第一片〕
- **动作**: checker 合成字段级结构化 ==（Copy 字段域）。
- **测试**: PBT 代律性质（自反/对称/传递）作用于生成 Eq 值域；
  与手写 == 的对照契约。

### INC-9 · I1 诊断第一阶段：错误码 + did-you-mean
- **动作**: 错误码命名空间落核心诊断集（move/borrow/type/parse）；
  未解析名 Levenshtein≤2 建议。
- **测试**: 负契约全面改用错误码断言（INC-2 格式升级）；建议命中单测。
- **价值**: 为后续所有步骤提供稳定 UI 测试锚点（越早越省）。

### DEC-B + INC-10 · M3/F5 while/break — **DEC-B 已决策：不捕获 + continue**（见 §0b）
- **DEC-B 剩余工作（随 INC-10 首 commit）**: D-010 修订文本 + 反例测试集
  （while 体内裸 `next(args)` 必须仍解析为函数尾递归调用）。
- **INC-10 动作**: `while (cond)` 脱糖 loop+Branch；无参 `continue` 关键字
  （仅 while 合法，跳到条件重估）；`break`（unit 值）在 loop/while 通用，
  FlowIR LoopBreak/Jump-to-exit。
- **测试**: 脱糖等价性 PBT（生成简单状态机，while+continue 形式 ≡ 手写
  loop 形式，经 INC-3 差分双路径验证）；负契约：①while 内 `next(args)`
  arity 错误指向函数参数（证明未被循环捕获）；②loop 内使用 `continue`
  报错（continue 不属于 loop）；③break 在函数顶层报错。
- **E2E**: 搜索循环示例去掉标志位。

### INC-11 · derive(Show) 〔研究 I5 第二片〕
- **依赖**: INC-7（数值→串）、INC-8。
- **测试**: 性质：show 输出含类型名与每个字段标签；黄金样例契约若干。

## Phase 3 — M1 错误处理主线

### INC-12 · prelude Option/Result + 组合子
- **动作**: prelude 定义两泛型枚举 + map/andThen/orElse；parseInt/std_string
  迁移到 Result。
- **测试**: PBT 函子/单元代律（map/andThen 在生成值上满足律）；
  契约 `option-result.ng`。

### INC-13 · `?` 脱糖（D-007 落地）
- **动作**: HIR 侧脱糖为 switch + early-return，挂接三张 drop 边表。
- **测试**: 差分性质：`?` 形式 ≡ 手工 switch 展开（生成调用链，双路径比对）；
  负契约（非 Result 返回函数内使用 `?`）。
- **E2E**: 解析链示例（parse→validate→compute）。

## Phase 4 — M2a 最小闭包

### INC-14 · 非逃逸闭包（copy/move 捕获）
- **范围**: `|x| expr` 语法；仅 copy/move 捕获；不可逃逸；Fn/FnOnce 最小
  auto trait 面。borrow 捕获**明确不在本步**。
- **测试**: 差分契约：`map(xs, |x| f(x)) ≡ [f(xs)...]` 内建特例（生成数组×
  纯函数，双路径）；拷贝独立性契约（捕获副本修改不影响原绑定）；
  负契约（逃逸闭包拒绝）。
- **E2E**: seq.ng 的 map/filter/fold 库化；comprehension 降级为糖。

## Phase 5 — A1 完整 loan 分析 + M2b（合并切片，plan A1 原范围）

### INC-15 · origin contracts 与返回引用（A1.1）
- `T ref(a)` / `T ref(a|b)` 解析到参数序号；返回路径核查。
- 测试：正例（first/choose 类）+ 负契约（origin 不满足）。

### INC-16 · 引用入聚合与跨调用借用（A1.2/3）
- ref 字段/全局/task payload 的放行规则；use-after-move-through-ref。
- 测试：穷举式负语料（经典 NLL problem case 1-4 逐条入契约）。

### INC-17 · M2b borrow 捕获闭包
- 依赖 INC-15/16（捕获即匿名借出场景，作为 loan 分析驱动测试集）。
- 测试：捕获存活/释放与 INC-16 同一套语料的闭包变体。

## Phase 6 — B1 堆域及连带项

### DEC-C · F2 内存路线 — **已决策：终态 RC + CoW，exit-arena 为过渡脚手架**（见 §0b）
- 剩余工作（随 INC-19 同 commit）：D-015/R6 修订——聚合头 refcount、
  CoW 写入协议、arena 拆除时点。

### INC-18 · exit-time bump arena（过渡卫生层）
- 依赖 INC-0（asan 对照）。
- 测试：asan 变体下除 arena 外零泄漏；分配吞吐 smoke（前后对比）。
- **限制如实标注**：不解决单运行内增长；RC/CoW 落地后拆除。

### INC-19 · generic `Box<T>` + `new`（B1 核心）
- 存储模型按 DEC-C：affine Box 走拥有句柄；Copy 聚合共享走 refcount+CoW。
- 测试：drop-exactly-once PBT（计数分配器句柄）；CoW 观测不变性契约
  （共享期间的读不受写方影响）；heap_box 迁移契约。

### INC-20 · F4 std.list 迁移 Box + I3 FlowIR List 指令移除
- 测试：既有 list 契约全绿不变；IR 契约断言无 List 专用 opcode；
  `lowering.cpp:612,635` 内嵌 helper 删除。

### INC-21 · A7 span\<T\> + 数组增长 + in-place pushBack
- 测试：span 越界负契约；增长序列 PBT（append N 次 ≡ 一次性构造）。

### INC-22 · M5 Cell\<T\>（handle 式内部可变性）
- 依赖 INC-14（闭包已落地，捕获共享状态场景成立）、DEC-C。
- 测试：get/set 契约；与 ref mut 排他性的交互负契约。

### DEC-D + INC-23 · union switch — **DEC-D 已决策：补齐完整公民**（见 §0b）
- **DEC-D 剩余工作（随 INC-23 首 commit）**: D-001/D-011 修订文本——
  union case 的类型模式语法 `case (x: i64)`、成员字面量模式、otherwise、
  穷尽性按静态成员集；与 enum switch 的角色分界写明。
- **INC-23 动作**: checker 接受 union scrutinee（tag 分发已就绪，
  `bindUnionBox`）；lowering 发射按成员序号分派；穷尽性复用 enum 机制。
- **测试**: 契约语料 union-match.ng（类型绑定 + 字面量 + otherwise）；
  负契约（缺成员非穷尽、未知成员类型）；PBT：union switch 结果 ≡ 生成值
  所属成员的参考分派。
- **E2E**: unions.ng 扩展分支示例。

### DEC-E + INC-24 · derive(Ord) — **DEC-E 已决策：字节序 + 声明序**（见 §0b）
- **DEC-E 剩余工作（随 INC-24 首 commit）**: D-008/D-011 修订——string
  全序 = memcmp 字节序（注明 UTF-8 ≡ 码点序）；枚举声明序 = 序数；
  float 字段排除条款。
- **INC-24 动作**: checker 合成字段级 compare（序数 → 字段字典序）；
  string `<` 等比较运算符按 memcmp 落地（现状为类型错误）。
- **测试**: 全序代律 PBT（传递/反对称/全性，生成域含字符串边界
  ——前缀串、多字节 UTF-8、空串）；枚举序数契约；负契约：float 字段
  derive(Ord) 诊断。
- **E2E**: 排序示例（struct 数组按字段排序）。

### INC-25 · I7 parse-HIR 缓存（stdlib 预解析）
- 测试：缓存命中/失效（mtime/hash）单测；sweep 总耗时前后对比。

### INC-26 · F6 f32 语义文档化 + 一致性语料
- 动作：D-008 补"per-op narrowing"条款；舍入边界契约语料
  （const 路径 vs runtime 路径经 INC-3 差分守护）。

### INC-27 · I2 双轨收敛收尾（standing rule + 专项清扫）
- 站式规则自 INC-4 起生效（新代码只读 expressionTypeIds）；
  本步做最终迁移与字符串轨道降级为打印缓存。

## Phase 6b — post-cutover A 类收尾映射（2026-08-25 对齐补全）

原 07-post-cutover-plan 的 A2/A3/A4 余项/A5/A6 此前未显式入列，现补全
（编号续接，插入点由依赖关系标注；与 07 的映射见其顶部横幅）。

### INC-30 · A2.1 模块接口工件
- **动作**: `ModuleArtifact` 导出函数描述符表（name → 签名/instance id），
  import 按类型化工件绑定而非重查源码。
- **测试**: 契约——源模块与工件导出面逐项相等（07 §7 验收口径复用）；
  负契约：签名不匹配的导入绑定拒绝。
- **依赖**: 无硬依赖；INC-25（parse 缓存）受益于同一套哈希基建。

### INC-31 · A2.2 RuntimeSession 实例生命周期 + stdlib 预加载
- **动作**: 工件（不可变）与实例（globals/init 态）分离；session 级
  resolver/工件缓存；stdlib 以预编译工件集加载。
- **测试**: 初始化恰好一次契约（含循环导入）；会话隔离负契约；
  sweep 启动耗时前后对比。
- **依赖**: INC-30。**是 INC-22（Box/堆域）的建议前置**（对齐原 plan
  "A2 与 R6 slice 合并解锁 B1"的排序意图）。

### INC-32 · A3 收尾：重载集工件与部分特化语义
- **动作**: 已解析重载按调用点编码进工件（specificity 不进运行时路径，
  call site 已携带 resolved target）；const 谓词需要处落部分特化语义。
- **测试**: 重载解析结果跨编译稳定契约；特化优先级语料
  （exact→pattern→pack→primary，D-012 既定序）。

### INC-33 · A4a 元组 case 模式（DEC-D 解除阻塞）
- 原 07 将其推迟到"union tags 有意义之后"；DEC-D 落地后即解锁。
- **动作**: `case (a, b)` 解构 tuple scrutinee（rest 绑定已有机制复用）。
- **测试**: PBT：元组模式匹配 ≡ 手工索引解构（生成元组域）；穷尽性负契约。
- **依赖**: INC-23（union switch 的 tag 分派机制同源）。

### INC-34 · A4b move-aware pattern bindings
- **动作**: case 绑定按 A1 partial-move 规则处理（字段感知、分支合并）。
- **测试**: 负语料（绑定后源值状态断言）；与 INC-16 同一套 move 语料的
  pattern 变体。
- **依赖**: INC-16（loan/partial-move 机械在位）。

### INC-35 · A5 Self-typed view methods
- **动作**: 方法签名含 `Self`（如 `clone() -> Self`）可经 `ref<Trait>`
  视图调用（per-concrete 实例化进 vtable）。
- **测试**: 契约：视图上 clone 返回独立深拷贝；负契约：返回未约束
  `Self` 的抽象调用仍拒绝。
- **依赖**: trait views（已落地）＋建议在 INC-17 后（借用检查交互）。

### INC-36 · A6 const fun 内 trait bound
- **动作**: `where T: Trait` 进 const 谓词/fun 的编译期求解
  （ConstInterpreter 扩展，fuel 预算沿用）。
- **测试**: 正例（const fun 按 bound 分派编译期实现）；负契约：
  未满足 bound 的实例化诊断；差分：const 求解结论 ≡ 运行期对应行为
  （INC-3 harness）。
- **依赖**: 无硬前置（A5 Stage 1 已落地）。

## Phase 7 — B 类收尾与占位

### INC-28 · B2 isize/usize
- TargetAbiDescriptor 驱动的目标宽度类型；ABI 边界契约。

### INC-29 · B3 C ABI 收尾（预计再拆 2-3 子步，实施前细化）
- 切片候选：a) `cstr`/原始指针解引用（unsafe 边界）；b) owning/borrowed/
  shared 所有权注解 + opaque 生命周期策略；c) varargs/callbacks/
  `export extern "C"`。
- 测试：libc fixture 扩展（ffi_extern.ng 先例）；回调往返契约。

### 占位（不入本计划细节）
- **R10 并发**（阻塞于 INC-15..17 loan 分析 + INC-19 堆域）；
- **R11 formatter/LSP/debug**（LSP 前置：错误恢复解析 SYN-06，届时立项）。

## 关键路径与并行度

```
价值线（用户可见交付，Phase 1–4）：
INC-0 → INC-1/2/3 ─┬→ INC-4 → INC-5 → INC-6（含 DEC-A 文档）─┐
                   ├→ INC-7 → INC-8 → INC-11 ────────────────┼→ INC-12 → INC-13 → INC-14
                   └→ INC-9                                  │      (M1)     (M2a)
INC-10（含 DEC-B 文档）───────────────────────────────────────┘

架构线（A1+M2b，独立并行道）：
INC-15 → INC-16 → INC-17（M2b；唯一交汇点——闭包语法面取自 INC-14）

堆域线：DEC-C → INC-18 → INC-22（建议先 INC-30/31 工件/session 基建）
        INC-19 后连带：INC-20/21/25；DEC-D → INC-23 → INC-33；INC-34；DEC-E → INC-24
插空：INC-26/27/32/35/36 相互独立；收尾：INC-28/29；占位：R10/R11
```

- **两条长杆可并行**: 价值线与架构线除 INC-17 一点外零耦合（INC-17 需要
  INC-14 的闭包语法基础设施，故排在其后；loan 分析本体不依赖 Phase 1–4），
  单人执行时按精力交替。
- **风险早验**: INC-3（差分 harness）是全计划最大的方法论赌注——若生成器
  表达力不足，Phase 1 的 PBT 价值减半，故置于 Phase 0 并带自检要求。
- **对原 plan 的覆盖**: 07-post-cutover-plan 的 A1–A7、B1–B3 已全部映射
  （见其顶部横幅）；R10/R11 两文档均为占位。

## 执行状态

| 项 | 状态 |
|---|---|
| 五项语义决策（DEC-A..E） | ✅ 2026-08-25 敲定（§0b）；正式文档修订随实现 commit |
| INC-0 ASAN/泄漏基线 | ✅ 2026-08-26（asan 变体全绿；基线见 10-baselines.md） |
| INC-1 … INC-36 | ⬜ 未开始（逐个推进，每个完成后更新本表） |
