# Appendix E — 标准库、诊断与工程基建

> Parent: [mainstream-language-comparison.md](mainstream-language-comparison.md) · Snapshot 2026-08-25 · AI-assisted.

## E1 — F4：std.list 依赖泄漏型堆单元（严重度：中，随 B1 解决）

**现状证据**

- `lib/std/list.ng`：`List<T>` 是 `Cons(head: T, tail: ref<List<T>>)` 的递归
  枚举；`append`/`reverseList`/`listFrom` 在**调用方帧内**构造 Cons 节点，
  尾部通过 scoped ref 指向其他帧的局部绑定。
- 该模式能存活是因为：被 MakeRef 触碰的 local 强制升级为永久堆单元
  （`src/native/lowering.cpp:1128`），返回后帧消失但单元不回收——
  **正确性由泄漏保证**。
- 文档自述局限："Mutating in-place builders arrive with the R6 heap domain"
  （lib/std/list.ng 注释）。

**对比**：OCaml list 节点在 GC 堆；Rust List 需 Box（所有权显式）；
Swift/Scala 引用语义天然支持。共同点：持久数据结构的节点生命周期都由
**显式机制**承载，而非实现副作用。NG 现状是"能用但不可解释"——若未来
arena/RC 落地改变了堆单元策略，std.list 会静默变悬垂。

**建议**：B1（generic `Box<T>`）验收标准中加入"list.ng 迁移至 Box 并删除
对永久堆单元行为的依赖"；同步执行 I3（移除 FlowIR 的 List 专用指令）。
在此之前在 list.ng 头部注释标注当前依赖，防止重构踩坑。

## E2 — I1：诊断基建（回报：高，复杂度：低）

**现状证据**：错误为单行消息 + 字节 span（`TypeError{message, span}`，
`include/typecheck.hpp:69-77`）；无错误码、无 notes/labels、无拼写建议；
解析错误同理（`driver.cpp:59-64` 直接打印 ParseError）。

**对比基线**：rustc 的 error code + 多标签 + did-you-mean 已成行业标杆；
clang 的 fix-it hints；甚至 OCaml 近年也在补结构化诊断（LRgrep 发布说明即
针对 Menhir 错误质量）。共识：**诊断质量决定语言可用性上限**——rustc
团队多年持续投入 borrow-checker 诊断基建的事实本身即是佐证（该投入在
Polonius/NLL 历次状态报告中均有记录；具体警句出处未定位，不作逐字引用）。

**建议（全部低成本，按序）**：
1. 错误码命名空间（如 `E-move-used`），测试断言码而非全文——同时稳定
   UI 测试。
2. did-you-mean：对未解析名做 Levenshtein ≤2 的模块内候选提示（纯字符串
   工作，checker 报错处加一次候选扫描）。
3. 主/次 span：borrow 冲突类错误给出两个位置（冲突点 + 活跃借用起点），
   数据已存在于 loan 表。
4. 多行 caret 渲染统一到 driver 层。

## E3 — I6：stdlib 基础缺口（回报：高，复杂度：低）

**现状证据**（`lib/std/string.ng` 全文仅 13 个 native 声明）：
没有 i64→string（除 print）、string→i64 解析、格式化最小集；
`charAt` 返回 string（因无 char 类型）；seq.ng 只有 len/sum/arrayContains/
reverse。写一个"读入数字文本求和"级别的程序都无法纯 NG 完成。

**对比**：任何语言的 stdlib 第一屏都是 int<->str + 格式化（C stdio 之外，
Rust fmt/parse、Java Integer.parseInt、Kotlin toInt/toString、OCaml
int_of_string/string_of_int 全部内置或零依赖可用）。

**建议**：
1. `toString(i64|i32|f64|bool) -> string` 与 `parseInt(string) -> Result<i64, ParseError>`
   作为 const-capable 纯 host 注册（D-012 `= native` 通道已就绪，
   regexMatch 已示范该路径）。
2. parseInt 返回 Result 正好成为 M1 prelude Result 的第一个真实用户，
   两项互相验证设计。
3. derive(Show)（附录 A §A4）落地后 toString 可退化为 Show 的派生结果。

## E4 — I8：ASAN/UBSAN 测试变体（回报：中，复杂度：低）

**现状证据**：单一 build 配置；F2/F4 描述的内存行为（只增不减的堆、跨帧
引用链）在现有测试下完全不可见。

**建议**：CMake 加 `-fsanitize=address,undefined` 的 ng_test 变体目标
（不要求默认开启）。预期效果：(1) 泄漏报告直接量化 F2；(2) UBSAN 捕获
潜在对齐/溢出 UB；(3) 为 B1 的 RC/CoW 改造提供回归安全网。CI 成本一条
额外 ctest 命令。

## E5 — M4：char 类型与字符串迭代语义（缺失）

**现状证据**：无 char 类型/literal（grep 语法与 TypeKind 零命中）；
`length(text)` 返回字节长还是字符长未定义（native 实现 `ngrt_str_len`
按字节）；SYN-08 登记 Unicode 策略未决。

**对比教训**：Kotlin/Java UTF-16 code unit 语义造成的历史包袱（emoji/
增补平面切分 bug 成为常见事故）；Rust 标准库明确 bytes/chars 两层 API
（grapheme 层在第三方 unicode-segmentation crate）并让 `len()` = 字节——
文档先行消歧义；Swift 把 Character 定为 grapheme cluster 且 String 非
RandomAccessCollection——语义最安全但代价是索引模型复杂。

**建议**：现在只需决策+文档，不需实现：声明 `string` 为 UTF-8 字节串、
`length`= 字节数、索引按字节且必须落在字符边界（越界/切半个字符为运行期
检查错误，与既有 bounds-check 风格一致）；char 类型推迟到有真实需求时。
成本一页文档 + 边界检查几行。

## E6 — M6：错误恢复解析（登记，R11/LSP 前置）

**现状证据**：SYN-06（gap register）："Parser loops and ad-hoc accept errors
do not provide structured recovery"。当前 driver 首错即停。

**说明**：交互式 REPL/LSP 必须能在单个语法错误后继续解析后续项；四语言
工具链均具备（rustc/clang/swiftc 的 recovery 是 IDE 体验的地基）。此项
复杂度为中且独立价值有限，维持 gap register 登记状态，作为 R11 启动前的
硬前置即可——本报告不重复展开。

## E7 — stdlib 设计正面评价

1. **prelude 导出面克制**：print/assert/not/is_ref 四个名字默认可见，
   对比 Rust prelude 的规模控制经验一致。
2. **memory.Box 用 Drop 收尾 handle**：在无堆域约束下给出了 RAII 形态的
   最小可行样本，为 B1 积累了 Drop 与资源生命周期的真实用例。
3. **const-capable host 通道**（regexMatch 先例）让"哪些 native 能进编译期"
   有显式注册协议，优于隐式白名单。
