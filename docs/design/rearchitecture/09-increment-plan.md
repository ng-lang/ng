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

### INC-1 · 自研轻量 PBT harness 〔新增基建〕 ✅ 2026-08-26
- **产出**: `test/pbt.hpp`——seeded PRNG（seed 可注入/打印）、数值域生成器
  （各宽度含 INT_MIN/u64::MAX 等边界权重）、字符串/数组生成器、NG 源片段
  生成器骨架、property 宏（失败时打印 seed 与最小复现输入）。
- **验证**: harness 自测属性 ≥5 条（如生成整数经 i64 往返恒等）随套件运行。
- **限制声明**: 无 shrink（失败时输出 seed 供人工最小化）；文档写明。
- **价值**: 后续所有 INC 的 PBT 前提。
- **执行记录**: xoshiro256** PRNG + `PBT_SEED` 注入/回放（每次运行打印
  effective seed）；signed/unsigned 各宽度边界加权生成器、asciiString、
  vectorOf、`forAll`+`PBT_CHECK`。自测 6 条性质全确定性（固定 seed，无
  flake）。无 shrink 限制已在 `test/pbt.hpp` 头注释声明；NG 源片段生成器
  骨架随首个消费者（INC-3 随机程序生成器）落地。自测首跑即抓到并修复
  生成器自身一处边界分支不可达缺陷。

### INC-2 · 契约语料 runner（层 1+2）〔新增基建〕 ✅ 2026-08-26
- **产出**: `test/conformance/<case>.ng` + `<case>.expected`（stdout/exit）+
  负诊断契约格式（消息模式 + span 区间断言）；Catch2 runner
  `test/contract_test.cpp`。迁移 3-5 个现有示例行为进语料作为种子。
- **验证**: 种子语料全绿；一条负契约（`use of moved value`）按码+span 断言。
- **价值**: 行为规格的唯一权威载体，后续 INC 直接追加语料。
- **执行记录**: 正例 = `.ng` + `.expected`（程序 stdout 精确逐行匹配，过滤
  驱动横幅行）；负例 = `.neg.ng` + `.neg.expected`（kind/pattern/span 指令，
  span 精确区间断言；INC-9 增加 code: 指令）。种子：numeric-basics /
  enum-dispatch / string-basics（自 numeric_types、enum_match、std_string
  迁移）+ moved-value 负契约。runner 目录扫描自动发现新语料。

### INC-3 · 差分契约：ConstInterpreter ↔ QBE 原生（层 3）〔新增基建〕 ✅ 2026-08-26
- **产出**: 差分 runner——同一模块双路径求值比对；受限随机程序生成器
  （const 安全子集：i64 算术/if/loop/递归/调用），N≥500/次，seed 固定可选。
- **验证**: 500 个生成程序双路径结果零分歧；人为注入一处 lowering 偏差
  能被捕获（自检）。
- **价值**: 当前架构独有的免费 oracle 对；后续任何语义改动自动受守护。
- **执行记录**: `test/differential_test.cpp`。批量技巧：N 个探针函数编入
  同一模块，native 路径一次驱动运行收集全部输出；const 路径在进程内对
  相同源码 resolve/check 后以 ConstInterpreter 逐个求值。生成子集全部
  `%256` 有界（无溢出/trap 干扰），含表达式体/let 链/有界累加 loop/
  守护递归/跨探针调用。500 程序零分歧 + 注入 +1 偏差被自检捕获；
  PBT_SEED 可复现，多种子扫过。

## Phase 1 — 数值正确性（研究 F1①③②）

### INC-4 · F1① 无符号排序比较指令 ✅ 2026-08-26
- **动作**: lowering 按 TypeId 符号性发射 `cuslt*` 系列；u8–u32 回归。
- **测试**: PBT「NG 比较 ≡ 宿主参考语义」逐宽度随机对（含边界加权）；
  契约语料 `numeric-ordering.ng`；probe `u64::MAX > 5` 翻转为真。
- **依赖**: INC-0/1/2。
- **执行记录**: lowerBinary 对 payload 8–11（< <= > >=）按操作数是否为
  无符号 builtin 发射 QBE 无符号比较（cultl/culel/cugtl/cugel），有符号
  路径不变。oracle 为宿主 C++ 同宽度语义；8 宽度 × 12 对 × 4 运算符
  边界加权随机批测 + IL 断言（cugtl）+ 契约语料 + u64::MAX 探针。
  生成器排除 INT64_MIN 字面量拼写（已有专项测试覆盖其表达性）。

### INC-5 · F1③ u64 算术宽度检查 ✅ 2026-08-26
- **动作**: 补 `check_w_u64` 事后检查或无符号检测路径（与 INC-4 同族同 PR 可）。
- **测试**: PBT 溢出/trap 等价性（trap ⟺ 宿主判定溢出，逐宽度）；
  契约 `numeric-wrap.ng`（期望 trap）；`u64(2^64−8)+20` 不再静默。
- **依赖**: INC-4。
- **执行记录**: 采用无符号检测路径：新增 checked_uadd/usub/umul/udiv/urem
  内嵌 QBE 助手（uadd 以 r<a 判回绕；usub 以 a<b；umul 以 udiv 回除校验；
  udiv/urem 仅查零除数），lowerBinary 按操作数符号性选择。窄宽度仍由
  既有 check_w_* 后置检查兜底，语义一致。测试：宿主 128 位参考 oracle
  的非溢出批量 PBT（8 宽度×14 三元组×5 运算符，宽内精确比对）+ 逐宽度
  溢出边界 trap 探针 + 契约语料 numeric-wrap.trap.ng（runner 新增 .trap.ng
  契约类型）。发现并记录 checker 行为：比较中裸字面量默认 i64、不向 u64
  回退（测试改用带注解绑定规避；是否升格为诊断改进待 INC-6 一并考虑）。

### DEC-A · 混宽比较策略 — **已决策：统一禁止隐式**（见 §0b）
- 剩余工作（随 INC-6 同 commit）：D-008 修订文本 + 反例语料转正为负契约。

### INC-6 · F1② 混比禁令与显式转换落地 ✅ 2026-08-26
- **依赖**: DEC-A（已定）。
- **动作**: checker 对 i64↔u64 / int↔float 变量间隐式比较报编译错误；
  新增 `toFloat` / `toSigned` 显式转换 natives；排查并迁移受影响用法
  （预计集中在 float_test/numeric_types_test 的混比断言；unions.ng 的
  union-vs-字面量对照属成员类型匹配，不受 DEC-A 影响、无需迁移）。
  DEC-A 的 D-008 修订文本随本步同 commit 落盘。
- **测试**: 负契约（两类混比的错误码 + span）；PBT：任意生成比较表达式
  要么类型合法且语义精确、要么有确定诊断，无第三态。
- **E2E**: numeric-mixed.ng 示例展示显式转换路径。
- **执行记录**: checker 在比较上下文启用字面量采纳（含整数字面量→float
  上下文、带后缀守卫），随后按符号类/数值种类报确定性诊断；f32↔f64 与
  同符号跨宽保留（槽内精确）。toFloat(i64|u64)/toSigned(u64) 经 prelude
  声明 + libngrt 实现 + lowering 符号分派落地。迁移 float_test 的跨宽
  断言为合法子集 + 负断言；负契约 mixed-sign/mixed-kind 各一条（span 精确）。
  PBT 无第三态性质 120 试例；E2E example/numeric-mixed.ng。发现并修复
  连带缺陷：lowering 对已采纳为 float 的整数字面量仍发射 long 常量
  （cgtd 类型错误），现按记录类型发射 double。

## Phase 2 — stdlib 与工效

### INC-7 · I6 toString / parseInt（const-capable natives） ✅ 2026-08-26
- **动作**: 经 D-012 `= native` 通道注册纯宿主（regexMatch 先例）；
  `toString(i64|i32|f64|bool)`、`parseInt(string) -> Result<…>` 形态预留
  （Result 由 INC-12 定型前先返回 Option 式或临时枚举，INC-12 迁移）。
- **测试**: PBT 往返 `parseInt(toString(n)) == n` 全宽度域含 INT_MIN；
  边界契约（空串/+/-/溢出/非数字）。
- **执行记录**: libngrt 严格十进制解析器（可选符号+纯数字、防溢出累加、
  覆盖 INT64_MIN），按 NG 长度前缀字符串布局操作字节。prelude 声明
  toString 五重载 + OptionalInt 枚举 + parseInt 安全包装；lowering 按
  declaredType 分派符号（同 print 模式）。const 通道覆盖整数/布尔。
  连带修复：inferExpected 的 Call 分支缺 requireType 候选过滤，宽度不同
  重载在实参位置并列判歧义（toString 触发暴露）。边界契约 int-parse.ng
  （空串/+/-/非数字/空白/双界溢出）+ int-tostring.ng 金样例 +
  int-parse-invalid.trap.ng + E2E example/int_string.ng。
- **动作**: 经 D-012 `= native` 通道注册纯宿主（regexMatch 先例）；
  `toString(i64|i32|f64|bool)`、`parseInt(string) -> Result<…>` 形态预留
  （Result 由 INC-12 定型前先返回 Option 式或临时枚举，INC-12 迁移）。
- **测试**: PBT 往返 `parseInt(toString(n)) == n` 全宽度域含 INT_MIN；
  边界契约（空串/+/-/溢出/非数字）。

### INC-8 · derive(Eq) 〔研究 I5 第一片〕
- **动作**: checker 合成字段级结构化 ==（Copy 字段域）。
- **测试**: PBT 代律性质（自反/对称/传递）作用于生成 Eq 值域；
  与手写 == 的对照契约。

### INC-9 · I1 诊断第一阶段：错误码 + did-you-mean — ✅ 2026-08-27
- **动作**: 错误码命名空间落核心诊断集（move/borrow/type/parse）；未解析名 Levenshtein≤2 建议。
- **落地**: 错误类（TypeError/ResolutionError/ParseError）统一带 `code` 字段（默认 `E-type`/`E-resolve`/`E-parse`），driver 输出 `type error E-xxx at bytes [b, e): msg`；核心站点落具体码：`E-move-used`/`E-move-field`/`E-move-partial`/`E-borrow-conflict`/`E-borrow-too-many`/`E-compare-mixed-sign`/`E-compare-mixed-kind`/`E-unresolved-name`/`E-name-not-visible`；resolver 对未解析/不可见名做模块内候选 Levenshtein≤2 扫描，命中则追加 `; did you mean `X`?`。
- **测试**: INC-2 负契约增加 `code:` 指令并断言（三条 .neg.expected 全升级）；driver/borrow/move/float 单测断言 error.code；did-you-mean 命中/未命中单测。
- **价值**: 为后续所有步骤提供稳定 UI 测试锚点（越早越省）。

### DEC-B + INC-10 · M3/F5 while/break — ✅ 2026-08-27
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
 - **执行记录**: 语法层加 `while`/`continue`/`break` 关键字与 AST 语句节点；
   HIR 新增 `StatementKind::While/Continue/Break`，resolver 仅让带绑定器的
   `loop` 捕获裸 `next`（`while` 与 `loop ()` 不捕获，体内 `next` 指向函数尾
   递归），`break` 指向最近 loop/while、`continue` 仅允许最近为 `while`；
   FlowIR `lowerWhile` 产出 header 条件 Branch + body 回跳（隐式 continue），
   `lowerContinue`/`lowerBreak` 为指向 header/exit 的普通 `Jump`（无需新终结
   符种类），新增 `loopExits_` 映射；const 解释器新增 `runWhile` 与
   `Continue`/`Break` 控制流（Fallthrough 重新求值条件）。验证：INC-3 差分
   双路径 300 状态机（while+continue ≡ 手写 loop，native=const）零分歧；
   三条负契约（while 内 next 指向函数参数、loop 内 continue 报错、顶层
   break 报错）；flowir/resolver/typecheck/parser 单测；`example/while_loops.ng`
   （用 while/break/continue 去掉搜索循环的标志位）。D-010 修订文本落
   04-language-decisions.md。全套 489→497 用例、ctest 490→498 全绿。

### INC-11 · derive(Show) — ✅ 2026-08-30
- **依赖**: INC-7（数值→串）、INC-8。
- **测试**: 性质：show 输出含类型名与每个字段标签；黄金样例契约若干。
- **执行记录**: prelude.ng 声明 `export trait Show { fun show(self: Self ref) -> string; }`；
  checker 在 derive 注册循环新增 `Show` 分支——字段必须为 string、带原生
  `toString` 重载的标量（prelude 的 i64/i32/u64/f64/bool），或已 derive Show
  的嵌套 struct（声明序先在先校验，语义同 derive(Eq)）；标量类型→`toString`
  原生 DefId 记入 `showToStringTargets` 供合成调用。infer 在 clone 特例后新增
  `show` 特例（`derivedShowCalls_` → `builtin::String`，`Self ref` 接收者）。
  FlowIR `lowerMethodCall` 对 derive(Show) 调用 LoadRef 接收者后逐字段提取
  （Member 索引）并合成 `name{field: value, ...}` 串（研究 A4 格式，大括号内
  无空格）：string 字面量 + `$ngrt_str_concat` 链、标量走原生 `toString` 调用、
  嵌套 Show struct 递归。命中方确认：driver 将原生函数也纳入 flows，合成
   `toString` 调用经 nativeDefIds 派发到正确符号。验证：黄金契约（整数/串/
  bool/f64/i32/嵌套/空 struct 精确输出）、性质 PBT（24 随机 struct 输出含类型
  名+字段标签且与程序生成期望逐行相等）、4 条负契约（i8/f32 无 toString、
  非 Show 嵌套、非 Show 类型调用、arity）。三处示例（trait_defaults/
  generic_impls/generic_const_fun）与 trait_stage1/const_fun 测试的本地
  `trait Show` 因 prelude 新增 Show 而重命名为 `RenderText`（含`import prelude`
  的同源片段；不导入 prelude 的本地 Show 不受影响）。guard_test 行数上限
  2075→2150（flowir.cpp 2149，ARCH 拆分仍在 post-cutover plan 跟踪）。
  `example/derive.ng` 增 derive(Show) 断言（退出码 127→255）。全套
  497→500 用例、ctest 498→501 全绿。ASan 附随修复：driver 生成可执行文件由
  裸系统 `cc` 链接（无 sanitizer runtime），`-fsanitize` 会让两个 imgui 归档
  （vendored `imgui` + `ngrt_imgui`）产出 `___ubsan_vptr_type_cache` 未定义符号；
  与 `ngrt` 避让同因，改为 `-fno-sanitize=address,undefined`。ASan 500/500。

## Phase 3 — M1 错误处理主线

### ⚠️ 已交换排序（2026-08-30 维护者决策）
INC-14（最小闭包）**前移**至 INC-12/13 之前执行，INC-12/13 顺延至闭包之后。
原因：D-006/研究 §C3 证据——NG 无一等函数（closrue 未落地前所有高阶能力皆是
编译器内建特例），INC-12 规格中的 `map/andThen/orElse` 组合子与函子/单元
代律 PBT 均需把变换作为实参传入，无法闭包-free 表达。执行切片（每片独立
可验证，落地后合并回 INC-14 状态表）：
- **S1** 闭包语法 + 显式类型参数绑定（`|x: i64| expr`；`TypeKind::Closure`
  描述符；局部绑定 + 直接调用）。
- **S2** 调用点参数/返回类型推导（expected-type 驱动）+ 泛型 `map` 单态化的
  闭包实参 + 差分契约雏形。
- **S3** copy 捕获 + 匿名 env 记录 + 拷贝独立契约。
- **S4** move 捕获 + env drop。
- **S5** Fn/FnOnce 最小 auto trait 面 + 非逃逸负契约 + seq map/filter/fold 库化。
- **S6** 差分 PBT（`map(xs, |x| f(x)) ≡ [f(xs)...]`）等。
完成后 INC-12（含完整组合子 + 代律 PBT）落地。

### INC-14 · 非逃逸闭包（copy/move 捕获）— ✅ 完成（S1–S6 全落地）
- **范围**: `|x| expr` 语法；仅 copy/move 捕获；不可逃逸；Fn/FnOnce 最小
  auto trait 面。borrow 捕获**明确不在本步**。
- **测试**: 差分契约：`map(xs, |x| f(x)) ≡ [f(xs)...]` 内建特例（生成数组×
  纯函数，双路径）；拷贝独立性契约（捕获副本修改不影响原绑定）；
  负契约（逃逸闭包拒绝）。
- **E2E**: seq.ng 的 map/filter/fold 库化；comprehension 降级为糖。
- **S1 落地（2026-08-31）**: `|x: i64| expr | { body }` 语法 → resolver →
  `TypeKind::Closure` 描述符 → 自检器对每个闭包字面量合成唯一代码函数
  （`__closureN`，克隆 body + 参数局部重映射，经 `instances_` 复检），调用
  复用 `callTargets` → FlowIR 直接调用（跳过 callee 局部操作数）；字面量降
  为惰性零字。参数需显式类型；捕获拒绝（负契约，S3 开启）。测试
  `test/closure_test.cpp`（12 例）+ `example/closures.ng`。`||` 零参闭包与
  `||` 逻辑或存在词法歧义，留待 S2 处理。
- **S2 落地（2026-08-31）**: ① `||` 零参闭包头——前缀位 `OrOr` 视为空参数头
  （`|| expr`），避开 `|` 位或的 S1 歧义。② 期望类型驱动推导——resolver 允许
  省略参数类型（`closureParameterTypes` 空指针），自检器按期望闭包类型分支
  （`inferExpected` Closure case → `inferClosure` 复用 body 检查 + 代码函数合成）
  推导参数；绑定注解 `|i64| -> i64` 经新 `ClosureTypeSyntax`/`hir::TypeKind::Closure`
  解析并 `internClosure`；无期望类型时在检查期报“cannot infer closure parameter”。
  ③ 修正潜伏 bug：值型函数体尾部表达式此前降为裸 `ret`（QBE 遗留寄存器），
  `|| 7` 直接暴露——FlowIR 现将函数体尾部值接入 fallthrough return，
  `fun f() -> i64 { 42 }` 因此返回 42。
- **S2c 落地（2026-08-31）**: 高一阶（higher-order）——闭包型函数参数视为
  半泛型：`instantiateFunction` 新增 `closureTargets`（参数索引 → 代码 DefId），
  每个具体闭包实参（字面量或闭包 typed 局部）产生闭包特化实例，参数局部经
  `closureLocals_` 绑定到字面量合成函数，正文中 `f(x)` 直接绑定其代码函数。
  带闭包参数的原函数在注册期标记 `placeholderFunctions_`，驱动只降示例 stub，
  正文仅在实例中编译；未实例化模板体内 `f(x)` 按延迟路径建模（描述符定类型、
  不记录 callTarget）。`TypeInterner::specialize`/`unifyGuarded` 补 Closure 结构
  特化/合一；`analyzeCallSpread`/`inferExpected` 泛型分支补闭包实参处理；
  `lib/std/seq.ng` 落地 `export fun map<T, U>(values: array<T>, f: |T| -> U)`。
  ModuleParser `parseTypeUntil` 补 `|...|` 管道嵌套，多参闭包类型
  `|i64, i64| -> i64` 在函数签名中不再被参数逗号截断。测试
  `test/closure_ho_test.cpp`（10 例：非泛型/泛型高一阶、闭包 typed 局部实参、
  块体/多参闭包实参、嵌套调用、未用模板占位、负契约、seq.map E2E）+
  `example/closures.ng`（+6 组）。捕获（S3）与完全泛型闭包仍在后续切片。
- **S3 落地（2026-09-01）**: copy/move 捕获 + 静态捕获穿线（无运行时 env 载体）。
  ① **显式捕获列表** `[x, move y] |p| body`：resolver 将捕获名解析为外层局部
  id（`CaptureInfo{local, isMove}`），并以全新闭包体作用域影子局部
  （`closureCaptureParams`）承接 body 引用——正文读捕获参数而非外层局部。
  ② **检查期**：`inferClosure` 先按 `closureCaptureParams` 定捕获参数类型、
  以闭包作用域局部检查正文、body 检查完再标记 move 捕获转移；
  `validateCaptures` 替换 `ensureNoCapturedLocals`：未列出外层局部引用 → 报
  `closure body references local N which is not listed in the capture list`；
  代码函数 `__closureN` 前置捕获参数（`__cap{i}`）。③ **调用点穿线**：直接
  闭包调用与闭包 typed 局部实参的高一阶调用点，FlowIR 以合成
  `ResolvedName` Evaluate（负载为捕获局部 id）把捕获值前置为实参；
  高一阶 `instantiateFunction` 按 `closureTargets` 顺序前置捕获参数组
  （`__instance_capN`）到特化实例签名 + clone 参数，`instanceCaptureParamsByLocal_`
  记录实例内闭包调用的捕获参数局部。④ **幂等性修正**：重载解析会多次重推
  同一调用（如 `print` 12+ 重载各自重试实参），高一阶捕获记录改为仅在首次
  推断（`callTargets` 未含该调用节点）时累积，杜绝捕获实参 15× 重复
  （此前 `callCaptures_` 累积 → `apply(f,5,7)` 错误结果 300）。测试
  `test/closure_ho_test.cpp`（+7 例：copy 标量/数组、move 数组、多捕获、
  多调用点独立、未捕获负契约）+ `example/closures.ng`（+6 组捕获）。
- **S4-fwd 落地（2026-09-07）**: 闭包参数转发（工具任务 S3 之后的优先切
  片）。闭包 typed 参数可转交进另一个高一阶调用（`mapClosure<T>(values, f)
  { map(values, f) }`）。根因：内层 `map(values, f)` 的 `f` 是闭包 typed 局
  部，但 `mapClosure_i` 实例正文经 `inferExpected` 泛型分支推断，该分支只
  为闭包**字面量**（延迟）而不为闭包 typed **局部**记录 target →
  `map` 实例以 `closureTargets={}` 建立，其 `f(x)` 报 `closure value f was
  not created directly by a closure literal`。修复：① `inferExpected` 泛型实
  参循环镜像 `infer` Call-case——`closureLocals_` 已绑定的闭包 typed 局部记
  录 closureTarget；② 补齐捕获**值**穿线缺口（S3 遗留运行时缺口）：`infer`
  延迟字面量循环与 `inferExpected` 字面量分支此前只记 `closureTargets` 而
  不填 `callCaptures_`，导致带捕获的实例签名有捕获参数但调用点只发
  `(values, f)`——实参错位、`%p0` 错乱/进程被杀（`map([1,2],[factor])` 直
  接复现）；现按字面量 `closureCaptures` 填写（`callTargets` 幂等保护）；
  ③ 转发 remap：转发当前实例的闭包 typed **参数**时，捕获局部取该实例自
  身捕获参数（`instanceCaptureParamsByLocal_`）而非原闭包的外层局部捕获。
  验证：`mapClosure([1,2,3],|x| x*2)` → 12；`[factor=5]` → 30；
  4 级 `apply4→apply2` + `[offs=3]` → 7；多参 `biMapF(3,4,[offs=10])` → 22；
  直接 `map` 带捕获 → 9（S3 崩溃回归）。测试 `test/closure_ho_test.cpp`
  （+6 例：转发字面量/带捕获/多级链/多参+捕获/直接字面量捕获回归）。guard
  ceilings 稳定（flowir 2203、ast.hpp 966）。剩余：完全泛型闭包字面量
  （新 S4）、Fn/FnOnce + seq filter/fold（S5）、差分 PBT（S6）。
- **S4-generic 落地（2026-09-07）**: 完全泛型闭包字面量。泛型函数体内的
  `let f: |T| -> T = |v: T| v`、`map(xs, id)` 转发、`[base] |v: T| base`
  类型参数捕获均可端到端运行。设计按用户指示**拒绝 stub/skip lowering**：
  泛型闭包在模型期只做类型建模——**不合成代码函数**；外层泛型实例化时才以
  具体类型重推闭包、合成真正可降级的 `__closureN`。修复三处：① 删除两处
  `closure parameters require concrete types` 闸门（`inferExpected` 声明注解
  路径 + `infer` Closure 分支）；② **`cloneType` 丢弃闭包类型**（根因
  —— 泛型函数内 closure-typed `let` 从未被实例化）：克隆只拷贝
  kind/span/name，丢失 `parameters`/`result` → 实例绑定类型为无 result 的
  Closure → `resolveWithBindings` 落空进入 `resolve()` 报 `unsupported type
  form`。现克隆闭包参数与结果；③ 注册期将所有**泛型模板**标记为
  placeholder（模型检查、不降级，与 S2c 高一阶模板一致），消除 SSA 中
  TypeParameter 模型闭包死代码（此前 `identityMap_0` 泄漏 `__closure0_49`），
  并使延迟闭包调用路径覆盖泛型闭包局部（`deferredClosures_` /
  `deferredClosureLocals_`）。语义与泛型函数一致：无运算正文通过、类型参数
  上运算（`|v: T| v * v`）在模型期报错。验证：`genUse<T>` → 21、
  `genMap<T>`（泛型闭包入 map）→ 18、`genAdd<T>`（T 捕获）→ 40、泛型体内
  具体闭包 → 41（`cloneType` 回归）；SSA 干净（模板为空 stub、仅具体实例链
  有代码）。测试 `test/closure_ho_test.cpp`（+5 例：泛型局部调用/转发/捕获/
  具体闭包于泛型体内/运算拒绝）。guard 稳定。剩余：Fn/FnOnce + seq
  filter/fold（S5）、差分 PBT（S6）。
- **S5 落地（2026-09-07）**: Fn/FnOnce 最小 auto trait 面 + 非逃逸负契约 +
  seq filter/fold 库化。① **seq.filter/fold 库化**：`export fun filter<T>(
  values: array<T>, pred: |T| -> bool) -> array<T>`（保留 `pred` 为 true 的元素）
  与 `export fun fold<T, U>(values: array<T>, initial: U, f: |U, T| -> U) -> U`
  （左折叠累加器）。复用 S2c 单态化 + S3 捕获 + S4 转发全套闭包机制，无需
  新编译器代码；转发包装（`fun myFold(values, initial, f) { return fold(...); }`）
  与捕获（`[offs] |acc, x| acc + x + offs`）端到端工作。② **Fn/FnOnce 最小
  auto trait 面**：prelude 新增 `export auto trait Fn {}` / `FnOnce {}` 标记
  空 trait；`hasImpl` 对 Fn 族 auto trait 特判为**仅闭包类型满足**
  （区别于 Send 式全具体类型）——`where T: Fn` / `where T: FnOnce` 泛型约束
  以闭包类型实参通过、以 i64 实参报 `does not satisfy its where clause`。
  闭包可调用签名活在闭包类型本身，标记 trait 只作 callable 断言（Rust 式
  `Fn(T) -> U` 关联参数签名不在本步）。③ **非逃逸负契约**：函数返回闭包类型
  现被明确拒绝——`checkFunction` 检查声明的返回类型，若为 Closure 描述符报
  `function X returns a closure, which cannot escape its scope (non-escaping
  closures, INC-14)`；嵌套闭包字面量（`|x| |y| x`）经捕获契约或合成代码函
  返回类型检查同样被拒。此前该情形可被编译（无捕获闭包 = 裸代码指针），
  带捕获时潜在地引用已释放局部。验证：filter `[1..6]` 偶数 → 12、fold
  `[1..5]` 求和 → 15、捕获 fold → 36、forwardFold → 6、Fn/FnOnce 双约束 → 3；
  guard 稳定。测试 `test/closure_ho_test.cpp`（+8 例：filter/fold/fold-捕获/
  filter-捕获/泛型转发/逃逸拒绝/Fn+FnOnce 满足/Fn 不满足）。发现的历史缺口
  （不在本步）：签名内建 `<T: X>` trait 界在调用点不强制（`T: Copy`/`Send`
  同样如此，仅 `where T: X` 被检查）；闭包参数不可变绑定；聚合/视图中的闭包
  类型仍按旧路径拒绝。剩余：差分 PBT（S6），完成后 INC-12 落地。
- **S6 落地（2026-09-07）**: 差分 PBT（INC-14 收尾）。闭包组合子 ≡ 语言内建
  spread 形态，经 INC-1 harness 全随机比对（xoshiro256**，PBT_SEED 可重放）：
  `map(xs, |v: i64| f(v)) == [f(xs)...]`（map 理解，经 `same()` 长+元素双
  比较）与 `fold(xs, seed, |acc: i64, v: i64| g(acc, v)) == g(seed, xs...)`
  （fold spread，累加值相等）。每次 300 trial × 双探针 = 600 探针单模块批量
  编译运行（`runBatchProbes`），纯函数图 f0..f4/g0..g3 内嵌（值域受限防检查
  算术 trap），数组长 0..6、元素/种子 ∈ [-15,15]，覆盖空数组、单元素、负值
  边界；跨 4 个随机种子稳定。测试 `test/closure_differential_test.cpp`。
  发现的历史（非本步）缺口：① 重载候选评分吞掉实参 `infer()` 内层 TypeError
  → 直接处于实参位置的 map-理解数组字面量报 `no matching function
  specialization`（与已记录的 print 12 重载吞错同族；探针将理解先绑定时规避）；
  ② 未注解空数组字面量 `[]` 报 `cannot infer the type of an empty array
  literal`（探针以 `array<i64>` 注解规避）。INC-14 全切片完成；guard 稳定
  （flowir 2203, ast.hpp 966）。下一步：INC-12（Option/Result + 组合子 + 代律
  PBT）。

### INC-12 · prelude Option/Result + 组合子 — ✅ 完成（2026-09-08）
- **动作**: prelude 定义两泛型枚举 + map/andThen/orElse（以最小闭包作变换实参）；
  parseInt/std_string 迁移到 Result。
- **测试**: PBT 函子/单元代律（map/andThen 在生成值上满足律）；
  契约 `option-result.ng`。
- **落地（2026-09-08）**: ① **泛型枚举**：prelude 新增 `Option<T>`（Some/None）
  与 `Result<T,E>`（Ok/Err）；`parseInt` 迁回 `Result<i64, string>`（Err 载荷为
  原输入文本），删除旧 `OptionalInt`（全部调用点已同步）。② **组合子**（闭包
  实参）：`optMap`/`optAndThen`/`optOrElse` 与 `resultMap`/`resultAndThen`/
  `resultOrElse`。**命名偏离计划说明**：计划写 map/andThen/orElse；seq.map 已
  占用 `map`，实测跨模块同名泛型重载的候选评分只静默选中一个候选（option 侧
  抢走 array 侧的 `map(xs, ...)` 调用 → `expected Opt, got array`），故以
  opt/result 前缀消歧，保组合子语义。③ **代律 PBT**：
  `test/option_result_test.cpp`（INC-1 harness）：Option 同一律、组合律
  （Some/None 双路径）、orElse 默认/穿透；Result 组合律（Ok/Err 双路径），
  60 trial × 8 探针批量编译运行；因泛型枚举无 derive(Eq) 且具体枚举形参函数
  不可调（见缺口②），代律等式以嵌套 switch 归约到 i64 相等表达。契约示例
  `example/option_result.ng`（8 个断言全 true）。**顺带发现的历史缺口**：
  ① 非泛型函数以具体泛型枚举实例作形参（`Opt<i64>`）→ `no matching
  function specialization`（未注解 `let x: Option<i64> = ...` 不触发，形参位
  置触发）；② 无返回注解的块闭包体内 `return` 不可用（尾表达式风格）、具体
  枚举空构造子作为表达式需要类型上下文（`Option.Some(5)` 作实参/switch 悉知
  需先绑 `let` 注解）。下一个：INC-13 `?` 脱糖。

### INC-13 · `?` 脱糖（D-007 落地）— ✅ 完成（2026-09-08）
- **动作**: HIR 侧脱糖为 switch + early-return，挂接三张 drop 边表。
- **测试**: 差分性质：`?` 形式 ≡ 手工 switch 展开（生成调用链，双路径比对）；
  负契约（非 Result 返回函数内使用 `?`）。
- **E2E**: 解析链示例（parse→validate→compute）。
- **落地（2026-09-08）**: ① **语法/解析**：`ast.hpp` 新增 `TryExpression`
  （`ExpressionKind::Try`），postfix `?` 按后继 token 消歧——紧跟 `...` 保留
  原 map-spread 过滤语义（`f(xs)?...`），否则为 try 运算符。② **HIR 脱糖**
  （`Resolver::desugarTry`，src/hir.cpp）：`let x = expr?;` 重构为

  ```ng
  let $tryN = expr;
  switch ($tryN) {
      case Ok($okN) { let x = $okN; <块剩余语句> [尾表达式] }
      case Err($errN) { return Result.Err($errN); }
  }
  ```
  产生的都是既有 let/switch/return 节点 → checker 三张 drop 边表
  （fallthrough/block/return）与 FlowIR switch/early-return 降低原样生效，
  后端零改动。**支持形态**：`let` 初始化（含注解/`let mut`）、独立语句式
  `expr?;`；函数体尾表达式移位成 Ok 臂内显式 `return`（isFunctionBody 仅对
  函数/方法体顶层为真，if/loop 分支尾语义不变）。嵌套 `?` 经 Ok 臂递归
  `resolveStatementList` 自然链式。合成名用 `$` 前缀（lexer 不可产生）杜绝
  用户冲突。③ **差分 PBT**（`test/try_operator_test.cpp`）：2 步/3 步调用链 +
  串前 string 局部（drop 边场景）的 `?` 形态 ≡ 手工 switch 展开双路径比对，
  60 trial × 3 探针批量编译运行，4 随机种子稳定。**负契约**：非 Result 返回
  （i64/unit）、`return <expr>?`、表达式中部（`g()? + 1`）、闭包体内、解构
  let——均为明确的 E-resolve 消息。④ **E2E**：`example/try_operator.ng`
  parse（parseInt）→ validate（正数/偶数守卫）→ compute 链，4 断言全 true。
  guard 更新（ast.hpp 966→977，TryExpression）。全量 560 用例/3662 断言，
  ctest 561 全绿。**本次划定边界（后续增量可放开）**：`?` 仅限 let 根/语句根，
  表达式中部、`return ?`、闭包体内、解构 let 显式拒绝；需包 `Ok(...)` 的场景由
  用户手写。下一个：INC-15（A1 origin contracts）。

## Phase 4 — M2a 最小闭包

### INC-14 · 非逃逸闭包（copy/move 捕获）— ⚠️ 前移至 Phase 3（见上）
范围与测试如上；borrow 捕获（随 A1 的 M2b 切片）仍留在本 Phase，于
INC-15/16 之后再执行。

## Phase 5 — A1 完整 loan 分析 + M2b（合并切片，plan A1 原范围）

### INC-15 · origin contracts 与返回引用（A1.1）— ✅ 已落地
- `T ref(a)` / `T ref(a|b)` 解析到参数序号；返回路径核查。
- 落地点：语法 `T ref(a|b)` 起源表解析（postfix `ref` 后 `(a | b)`）；Resolver 把起源名解析为
  当前函数参数序号并随 HIR 返回 `Type.originOrdinals` 传递（clone 保留）；checker 在 `return`
  处以返回引用表达式的 origin root（穿透 Grouped/Index/Member/`ref`/`*`）核对所在参数序号是否
  落于声明集。无声明起源的引用仍不可返回（原行为保留，消息更新）。
- 测试：正例（first/choose 类 / 整引用透传 / 单起源身份 / 跨函数同参序）+ 负契约（origin 不满足、
  local 非参数起源、非参数声明起源、无声明起源仍禁返）+ E2E `example/origin_contracts.ng`。
- 边界：ref 字段/跨调用借用（A1.2/3）仍属 INC-16；`array<T> ref` 的直接索引亦待 INC-16。

### INC-16 · 引用入聚合与跨调用借用（A1.2/3）— ✅ 已落地
- **S1 跨调用借用（A1.2）**：move/assign 时若借贷存活即报 `E-borrow-conflict`
  （`cannot move/assign ... while it is borrowed`）。按语句粒度放行：借贷在 last-use
  释放，故 move/assign 于借贷最后使用之后合法。落地点：checker `isBorrowed` + `placeName`
  + `trackConsumption`（move 前缀 / Member / ResolvedName 三支）+ `Assign` 两支门（
  assignmentTarget 分支与整 local 赋值）。
- **S2 place-through-ref（A1.2）**：`items[i]` / `box.value` 在 `T ref`/`T ref mut` 参数上
  直接读写（无需显式 `*`）。Resolver：`T ref mut` 参数视为可变子目标根；checker：Index/Member
  接收面解包 Reference、ref-through-ref 与切片按 A1.3 推迟、不可变 ref 写与 ref 绑定重绑均拒绝；
  FlowIR：`lowerPlace` 对 Reference 局部返回 ref-root，Index/Member 读表达式先 `LoadRef` 解引用，
  Member 字段序号按解包描述符解析。
- **S3 放行规则（D-002 rule 8）**：struct 字段禁止 Reference（`internStructInstance` 与
  `resolveWithBindings` 两处实例化路径均拦截）；enum 载荷引用为所受许例外（递归 `List<T>` 模式）；
  模块 const 仅编译期 bool/i64 ⇒ 无需放行运行时全局。
- 测试：`test/nll_problem_cases_test.cpp`（NLL case 1-4 负语料 + 放行对照 + E2E）、
  `test/place_through_ref_test.cpp`（S2 正/负 + E2E）、`test/ref_permissions_test.cpp`（S3 规则）；
  示例 `example/place_through_ref.ng` 端到端通过（8 个断言打印 + 返回 42）。
- 边界/继承：ref 越借用创建（`ref r[0]`）与引用上切片（A1.3）仍推迟；闭包捕获变体 → INC-17。

### INC-17 · M2b borrow 捕获闭包
- 依赖 INC-15/16（捕获即匿名借出场景，作为 loan 分析驱动测试集）。
- 测试：捕获存活/释放与 INC-16 同一套语料的闭包变体。
- **已落地（+++）**：
  - Loan 改为**多承载者**（`Loan.bearers: vector<uint32_t>`；`0` 为无主表达式 loan）；
    loan 在**最后一个存活承载者**的最后使用点（`releaseLoansUsedUpTo`，多承载按
    使用序号取并）释放。承载者 = ref 绑定本身 + 每个捕获它的闭包局部。
  - `checkFunction` **保存/恢复外层函数瞬态状态**（此前清掉借用/移动状态——
    正是掩盖多个 pre-existing bug 的破坏者）；固定 `example/hkt.ng`（改
    `ref back`）与 `example/option_result.ng`（`let empty: Option<i64>` 供
    `optOrElse`；`none` 第 2 次 by-value 用是真实 move-after-move，旧 clobber 掩盖）。
  - 嵌套调用**双重推断修复**（`typecheck_infer.cpp`）：`analyzeCallSpread` 预推断所有
    非闭包实参后，参数循环的 `argumentTypeAt` 重复推断同一实参——对按值 affine
    实参在第二遍误读已消耗局部 → 假 `E-move-used`；现直接复用
    `spreadInfo.expandedTypes[index]`（闭包字面量槽位为 `TypeId{}` 按需推断）。
  - 闭包捕获参数解析期可变（`hir.cpp`）；`derive(Copy)` 使 `isAffine` **Copy 感知**。
  - **未调用（死）捕获闭包在其声明处释放 loan**（闭包不可逃逸，INC-14）——
    `lastUseByBlock` 对 Let 且表达式为 Closure 的记录 lastUse=声明位置。
  - 测试 `test/borrow_capture_test.cpp`（11 例，`[INC-17]`）：INC-16 同语料闭包变体
    （移/赋 referent → E-borrow-conflict D-002 A1.2）+ M2b 语义（借用晚于 ref 绑定存活、
    双闭包全死后才释放、死闭包声明即释放、ref mut 写入 E2E、`(*write).v := 9` 赋值穿透）。
    E2E 示例 `example/borrow_capture.ng`（6 个断言打印全 true，exit 0）。
  - **探针注记**：裸表达式语句 `move s;` 不经过 `trackConsumption`
    （`StatementKind::Expression` 仅 infer，pre-existing，超出本次范围）——
    测试用门控形式 `let y = move s;`。

## Phase 6 — B1 堆域及连带项

### DEC-C · F2 内存路线 — **已决策：终态 RC + CoW，exit-arena 为过渡脚手架**（见 §0b）
- 剩余工作（随 INC-19 同 commit）：D-015/R6 修订——聚合头 refcount、
  CoW 写入协议、arena 拆除时点。INC-19 S1 先行落地了 D-015 修订所依赖的
  `Box<T>`/`new` 语言面（见下），refcount/CoW 条款本身仍待 D-015 修订。

### INC-18 · exit-time bump arena（过渡卫生层）
- 依赖 INC-0（asan 对照）。
- 测试：asan 变体下除 arena 外零泄漏；分配吞吐 smoke（前后对比）。
- **限制如实标注**：不解决单运行内增长；RC/CoW 落地后拆除。
- **已落地（INC-18 commit）**: 纯程序值分配改道 per-process mmap 匿名 bump arena
  （`NgrtArenaBlock` 链，1MiB 块、16B 对齐、块耗尽时 mmap 新块、atexit 统一向 OS
  归还）；`ngrt_alloc`/`ngrt_new_string` bump 进 arena，`ngrt_free` 按指针归属调度
  （arena 指针 no-op，外部 malloc 暂存照常释放），`ngrt_release` 退化为仅做句柄
  live-table 逻辑清除。生成代码中 `$malloc` 全部移除（helpers `str.ssa`/`arr.ssa` + 
  13 处 lowering 发射点 → `$ngrt_alloc`；libngrt 8 处值返回路径改用
  ngrt_alloc/arena new_string）。保留 malloc 暂存：`ngrt_c_string`（regex 编译）、
  readFile 缓冲、systemOutput 缓冲。测试 `test/arena_test.cpp`（[INC-18]，主机侧
  alloc/free 调度/吞吐引入 `ngrt_arena_bytes()`/`ngrt_arena_blocks()`）、探针
  `test/probes/arena_all.ng`（覆盖全部发射点分配形态）；macOS `leaks(1)`：
  拼接探针（`test/leak_probe_test.cpp` 翻转契约）与 arena_all 探针均 0 leak/0 bytes
  （基线 19999 leaks ≈205MB → 0/0；单运行内存增长如实标注，峰值 footprint 观察不降）。
  额外修复：`#define _DARWIN_C_SOURCE` 恢复被 `_POSIX_C_SOURCE` 隐藏的 MAP_ANONYMOUS，
  `MAP_ANON` fallback；helpers 改 calls `$ngrt_alloc` 后 `native_qbe_test` 链接行
  补 `libngrt.a`（原仅 `libngrt_helpers.a`，全量跑暴露 20 例链接失败）。脚手架性质
  （随 RC/CoW 落地拆除）不变，见 INC-19。

### INC-19 · generic `Box<T>` + `new`（B1 核心）
- 存储模型按 DEC-C：affine Box 走拥有句柄；Copy 聚合共享走 refcount+CoW。
- 测试：drop-exactly-once PBT（计数分配器句柄）；CoW 观测不变性契约
  （共享期间的读不受写方影响）；heap_box 迁移契约。
- **S1 已落地（INC-19 commit）**: `lib/std/memory.ng` 重写为泛型
  `Box<T>`——`type Handle = native` 句柄表 + `export impl<T> Drop for Box<T>`
  （`(*self).handle` → `release`，drop-exactly-once）；`new<T>`/`box<T>`/
  `read<T>`/`write<T>` 以**显式泛型实参**调用 `allocate<T>`/`load<T>`/`store<T>`
  （跨函数传参的类型参数不能作为隐式调用实参推断——call-site 推断只在同
  TypeId 或显式实参下成立）。配套类型检查器：`unifyGuarded` 增 Struct 泛型
  unify 支路、`registerImpls` 支持**泛型 Drop impl**（`matchImplPattern` →
  `instantiateFunction`）、`dropEdge`/`hasDrop` 取代 `dropTypes_` 守卫、
  `instantiateFunction` 把实例签名登记进 `functionTypeIds_` 以支持 FlowIR
  drop-call 定型。词载荷走 64-bit word：i64/string/array/struct/enum/tuple/
  f64/f32 全比特保真往返（无别名、深拷贝进出）。测试 `test/box_test.cpp`
  （[Box]），端点 `example/box.ng`。待办（S2+）：std.string/std.list 迁移
  （INC-20）、Cell（INC-22）、Gc/Arc。

### INC-20 · F4 std.list 迁移 Box + I3 FlowIR List 指令移除
- 测试：既有 list 契约全绿不变；IR 契约断言无 List 专用 opcode；
  `lowering.cpp:612,635` 内嵌 helper 删除。

> **INC-20 阻塞（2026-09-11 探针结论）**：`Box<T>` 是不透明所有权令牌，
> 任何 materialize-then-read（`load`/`read`）会拷贝句柄字，产生别名副本。
> 当前 `List<T>` = `Cons(head: T, tail: Box<List<T>>)` 设计中，非破坏性迭代
> （`switch (*list)` / `read`/`length(ref xs)`）都因别名句柄的双重释放而
> 不健全。破坏性迭代需要枚举 `Drop` 递归释放字段，但 `hasDrop` 仅认显式
> `impl Drop`，聚合内的 `Box` 字段不被递归释放（结构体字段泄漏实证）。
> 解法需任一：(a) `Box` 增加 `clone` 深拷贝 + 引用消费原语；(b) INC-22
> `Handle` 内部可变性；(c) 破坏性 List API + 线性所有权转移。三者均需
> 新增基础原语，不在 INC-20 内解决。当前变更（INC-20a）仅修检查器：
> 枚举 switch 消费 scrutinee，属必要先决件（`feat(vnext): INC-20a`）。

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
| INC-1 PBT harness | ✅ 2026-08-26（`test/pbt.hpp` + 6 条自测性质；无 shrink 已声明） |
| INC-2 契约 runner 层 1+2 | ✅ 2026-08-26（4 个种子语料 + 负契约 span 断言） |
| INC-3 差分 runner 层 3 | ✅ 2026-08-26（N=500 零分歧 + 注入自检；`test/differential_test.cpp`） |
| INC-4 无符号排序比较 | ✅ 2026-08-26（u64::MAX>5 翻转为真；PBT+契约+IL 断言全绿） |
| INC-5 u64 算术宽度检查 | ✅ 2026-08-26（checked_u* 助手族；wrap 即 trap；trap 契约入语料） |
| INC-6 混比禁令+显式转换 | ✅ 2026-08-26（DEC-A 落地含 D-008 修订；toFloat/toSigned；负契约×2 + PBT + E2E） |
| ARCH-1 helper 外置 | ✅ 2026-08-26（34 个 ngrt helper 从 lowering.cpp 的 590 行字符串迁至 `src/native/helpers/*.ssa`，构建期 qbe→cc→ar 装配 `libngrt_helpers.a`，链接期按引用拉取；useHelper 机制删除） |
| ARCH-2 lowering 拆分 | ✅ 2026-08-26（FunctionLowerer 迁入内部头 + 按 cohesion 分 4 TU：scaffold/expr/aggregate/call；**47 个示例 --emit=ssa 逐字节 diff 一致**，双变体全绿） |
| ARCH-3 const-host 注册表 | ✅ 2026-08-26（维护者指令插队；`src/native/const_hosts.cpp` 表驱动注册，driver 收敛为一行调用） |
| PBT 收敛 | ✅ 2026-08-26（挑战审查后续；`test/pbt.hpp` 收纳 width/oracle/批量运行器三套漂移代码，修复 string_conversion `literalFor` 双负 false-green bug；5 个消费者迁移共删 ~200 行；pbt_test 覆盖新 helpers） |
| INC-7 toString/parseInt | ✅ 2026-08-26（严格解析全 i64 域；Option 式 parseInt；PBT 往返 + 边界契约 + trap 语料 + E2E；**连带修复 inferExpected 实参位置重载过滤缺失**） |
| INC-8 derive(Eq) | ✅ 2026-08-26（derive(Eq) 落地：checker 验证 Eq-comparable 字段 + flowir 逐字段展开；嵌套 Eq struct 递归展开；PBT + 契约 + E2E） |
| INC-B 核心 trait → prelude.ng | ✅ 2026-08-27（Copy/Clone/Eq 移出 compiler 注入，改由 `lib/std/prelude.ng` 声明（`export trait …`），derive 校验改为按已注册 trait 查询；`clone` 为关键字故 parser 接受其作 trait 方法名；derive/Eq 教程示例 `import prelude;` 显式接入） |
| INC-A prelude 自动加载 | ❌ 2026-08-27 决定跳过（尝试自动注入 `import prelude;`：typechecker 对合并模块的所有 overload 统一注册，visibility 只作用于 resolver 名字解析，与本地声明形成歧义特化；改例受限注入/影子化仍无法收敛 → **用户拍板**：显式 `import prelude;`，自动加载推迟至 typechecker 支持按模块可见性注册 overload 之后再议） |
| INC-9 诊断第一阶段：错误码+did-you-mean | ✅ 2026-08-27（错误类统一 `code` 字段，driver 输出 `type error E-xxx at bytes […]`；核心诊断落码：move/borrow/compare/unresolved-name 九码；resolver Levenshtein≤2 建议追加 `; did you mean \`X\`?`；INC-2 负契约升级 `code:` 指令并断言；序列化不改 what()，borrow/driver/move/float 单测加码；did-you-mean 命中单测；双变体全绿，ASan 仅剩既有 imgui 链接失败） |
| INC-10 + DEC-B while/break/continue | ✅ 2026-08-27（`while`/`continue`/`break` 语法+HIR+FlowIR+const；裸 `next` 仅被带绑定器的 `loop` 捕获；差分 PBT 300 状态机 while≡loop 双路径零分歧；3 条负契约；flowir/resolver/typecheck/parser 单测；`example/while_loops.ng`；D-010 修订；489→497 用例、ctest 490→498 全绿） |
| INC-11 derive(Show) | ✅ 2026-08-30（prelude `export trait Show`；checker 字段校验+`toString` DefId 注册；infer `show` 特例；FlowIR 合成 `name{field: value, ...}` 串（嵌套递归、标量走原生 toString、string 直通）；黄金契约+性质 PBT（24 随机 struct）+4 条负契约；本地 `trait Show`（含 prelude 者）改 `RenderText`；guard 上限 2075→2150；`example/derive.ng` 增断言 127→255；497→500 用例、ctest 498→501 全绿；ASan 附随清理 imgui 链接符（两 imgui 归档 `-fno-sanitize`）→ ASan 500/500） |
| INC-12 … INC-36 | ⬜ 未开始（逐个推进，每个完成后更新本表） |

### 架构修复插队记录（2026-08-26，维护者指令）

INC-6 完成后暂停增量推进，优先处理 lowering.cpp 架构问题（原 3176 行，
其中 ~590 行 QBE IL 字符串硬编码在 `ngrtHelpers()`）。两步落地：

1. **ARCH-1（b1f59db）**: helper 外置为独立 `.ssa` 文件 + 构建期装配
   archive。运行时代码归位 libngrt 一侧；`--emit=ssa` 只含用户模块。
2. **ARCH-2（0f85ed8）**: lowering 拆分为 5 个编译单元。验证采用
   重构前后全示例 IL **逐字节比对**——零行为变化的强证据。
3. **ARCH-4（a13a290）**: typecheck.cpp 拆分为 5 个编译单元（按 cohesion 分组：
   register/infer/check/const/unify）+ 内部头。新增 `test/guard_test.cpp`
   强制执行源文件行数上限（.cpp ≤ 1900, .hpp ≤ 900），防止未来回归。
   484 用例全绿，ASAN 变体干净。

后续 INC 在新结构上进行（lowering/typecheck 相关改动落点见各 TU 职责注释）。
