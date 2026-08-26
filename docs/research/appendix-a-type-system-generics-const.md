# Appendix A — 类型系统、泛型与 const

> Parent: [mainstream-language-comparison.md](mainstream-language-comparison.md) · Snapshot 2026-08-25 · AI-assisted.

## A1 — F1：数值三处缺陷（严重度：高，均经实验证实；③为 Rev.2 审查新发现）

### A1.① 无符号排序比较编译为有符号指令（正确性 bug）

**证据**：整数比较统一发射有符号 QBE 指令（`src/native/lowering.cpp:2670-2675`
仅 `ceql/cnel/csltl/cslel/csgtl/csgel`），全文件无 `cuslt*` 等无符号变体。
可复现实验（2026-08-25）：

```ng
fun main() -> i64 {
    let big: u64 = 18446744073709551615;
    if (big > 5) { return 1; }
    return 0;   // 实测走到这里：u64::MAX > 5 为假
}
```

`./build/ngi` 输出 `native main exited with code 0`。所有含高位的 u64
排序/区间判断（二分查找、容量检查、哈希桶比较等）都会静默出错。
同宽整数相等比较不受影响（按位比较）。

**修复**（复杂度低）：lowering 按操作数类型符号性选择 `cneql` vs
QBE 的无符号比较指令；checker 已知每个 TypeId 的符号性，信息齐全。

### A1.② 混宽 int/float 比较隐式经 double

**证据**：lowering 对浮点参与的双边发射 `sltof` 转换后用 `ceqd/cled/...`
比较（`lowering.cpp:2537,2632-2656`；`include/value.hpp:159-162` 的
`asNumber()` 为同语义的早期实现，Rev.3 审校确认其已无调用方，运行期机制
以下列 lowering 路径为准）。可复现实验：

```ng
let big: u64 = 9007199254740993;        // = 2^53 + 1
let f: f64 = 9007199254740992.0;        // = 2^53，double 可精确表示
if (big == f) { return 1; }             // 实测返回 1：判为相等
```

数学上两值不同，但 u64→double 舍入后相同。语义上这等价于 C 的
usual arithmetic conversions + IEEE 比较（结果"符合 IEEE"），问题在于
转换是**隐式且不可拒绝**的：程序员写的精确整数在无提示下参与了有损比较。

**对比语言做法**

| 语言 | 规则 |
|---|---|
| Rust | 不同类型比较直接编译错；整数间无隐式转换 |
| C++ | 整型提升层级保序；int→float 隐式转换存在但被 cppreference/CORE Guidelines 明示为已知陷阱 |
| OCaml | int 与 float 不同运算符（`=`,`+` vs `=.`,`+.`），混比不可能 |
| Kotlin | 官方决策无隐式数值转换，比较需显式 `toDouble()` |

**建议**（复杂度低）：短期在 checker 对 int↔float 混比给出 warning 级诊断 +
提供显式 `toFloat()/toSigned()` 转换 natives；长期（与 D-008 修订合并）
禁止隐式混比。同宽整数路径保持现状（已精确）。

**混宽整数排序连带问题（Rev.2 补充）**：实测 `u64(2^63+1000) > i64(5)`
为假——混合符号比较在单一 QBE 指令下无正确解，而 D-008 明文允许跨宽比较。
需一条明确语义规则：或禁止 i64↔u64 隐式混比，或定义归一化提升路径
（如双方经 i128 比较）。这是语义决策而非纯实现改动。

### A1.③ u64 算术溢出静默回绕（正确性 bug，Rev.2 审查新发现）

**证据**：可复现实验——`let a: u64 = 18446744073709551608; let b = a + 20;`
结果为 12，无任何诊断（`--emit=ssa` 确认仅调用 `ngrt_checked_add`，
其后无宽度检查）。机理：u64 值存于 i64 槽，≥2^63 的操作数读为负，
`checked_add` 的溢出检测失效；且与 u8 不同（`200+200` 被
`ngrt_check_w_u8` 正确 trap），**u64 不发射任何事后宽度检查**。
这直接违反 D-008 "Ordinary integer arithmetic is checked in both runtime
and const evaluation" 的承诺。

**修复**（复杂度低）：为 u64 补 `check_w_u64` 类检查或改用无符号检测路径；
与 A1.① 同根同源（"有符号槽位 vs 无符号语义"错配），同一 PR 修复，
并补齐 u8–u64 全宽的算术/比较 probe 语料进测试套件。

## A2 — F3：匿名 union 与 enum 双轨制能力不对等（严重度：中）

**现状证据**

- union 类型有独立 TypeKind 与运行期装箱表示 {成员序号, 负载}
  （`include/typecheck.hpp:91` Union kind；`src/native/lowering.cpp:818`
  `bindUnionBox`）——即**动态 tag 已经存在**。
- 但 union 只支持与成员类型的相等/排序比较（`example/unions.ng` 全部内容
  即 == / >），没有 switch 支持：switch 仅接受 enum 变量解构或标量
  literal-or（`example/enum_match.ng`、`switch_patterns.ng`）。
- 决策文档 D-001/D-011/D-012 均未提及匿名 union 的角色定位——它是 legacy
  迁移残留（gap register SYN-05 一脉）。

**缺陷**：同一语言里两种 sum type：
- enum：穷尽 switch + 负载 + 泛型，但构造需限定名 `Result.Ok(42)`;
- union：字面量即成员（`let x: i64|string = 1`）却不能分支判断，
  运行期带着 tag 却无处使用。

对比：Scala 3 的 union type `A | B` 是完整公民（match 穷尽检查可推导）；
TypeScript 用 discriminated union 支撑穷尽性；OCaml/Rust/C++17 variant 均
要求显式 tag 访问。NG 处于"有 tag 的 TypeScript 窄联合"这一无人区。

**建议**（复杂度：低–中，二选一，先做决策再动手）：
1. **补齐**：switch 接受 union scrutinee，case 绑定成员值；表示已就绪
   （bindUnionBox），只需 checker/lowering 分支。风险是与 enum 语法重叠。
2. **收缩**：union 定位为"约束性参数类型"（多态参数的接受范围），文档明确
   不支持分支；等 B1 后评估是否废弃。
推荐方案 1——已有运行期 tag，放弃它反而是浪费；且 Scala 证明 union+match
的组合价值。

## A3 — F6：f32 以 double 计算再收窄（严重度：低）

**现状**：QBE 层标量统一 word/double 寄存器；checked helper 全部以 `l`
（i64）运算后逐宽校验（`lowering.cpp:136-340`）。f32 运算实际发生在 double
再舍入。对单次 +,-,*,/ 而言，double(≥2×24 位有效位) 保证双重舍入安全，
结果与 IEEE f32 直接运算一致——**但这一点没有任何文档或测试固化**；
若未来加入 fused 运算或常量折叠跨运算合并，将静默破坏。

**对比**：Rust/C++ 要求 f32 运算按 IEEE 单精度执行（允许 F 合同收缩但有
`#pragma STDC FP_CONTRACT` 类控制）；Java 严格模式历史教训（strictfp）
说明"平台浮点语义必须写死在规范里"。

**建议**（复杂度：低）：在 D-008 补一段"f32 per-op narrowing"语义说明 +
const/runtime 一致性测试语料（边界舍入值）；禁止未来优化跨运算合并除非引入
显式契约。无需改实现。

## A4 — I5：derive 扩容 Eq/Ord/Show（回报：高；Eq/Show 复杂度低，Ord 受阻）

**现状**：`derive(Copy + Clone)` 已合成 marker impl + 深拷贝 clone
（`example/derive.ng`）；auto trait Send 已存在。但 Eq/Ord/Show 需要手写——
而结构体相等/序/展示是完全机械的字段级推导。

**地基依赖（Rev.2 审查实证）**：derive(Ord) 并非纯机械合成，前置两个
语义决策：(1) **NG 字符串当前无 Ord**——实测 `"apple" < "banana"` 是类型
错误（string 只有 Eq），全序定义（字节序 vs 码点序）未决；(2) 枚举变体
的序数语义（声明序 = 序数？）无决策记录。derive(Show) 依赖 I6 的数值→
字符串转换。因此落地顺序应为：I6 → derive(Eq) → derive(Show) →
string Ord 决策 + variant 序数决策 → derive(Ord)。

**对比**：Rust `#[derive(PartialEq, Eq, PartialOrd, Ord, Debug)]` 是使用率
最高的宏；Scala case class 自动 equals/hashCode/toString；OCaml
`[@@deriving eq, ord, show]`（ppx）；Kotlin data class。四语言生态一致证明
这是"第一天标配"。Swift 的 SE-0185 只为 Equatable/Hashable 提供成员级
自动合成（**不含 Comparable**——Ord 无 Swift 先例可援引，更说明 NG 需要
自己的序语义决策）。

**建议**：checker 内合成（与现有 derive 同一扩展点）：Eq 生成结构化 ==；
Show 生成 name{field: value,...} 字符串；Ord 在两个语义决策落地后按同法
生成字段级 compare。不需要宏系统。

## A5 — M7：associated types / Iterator 故事（远期登记）

**现状**：D-011 Stage 3 明确 "Associated types remain a non-goal per legacy
#35"。当前迭代协议只能靠内建 fold/comprehension 特例表达（附录 C §C3）。

**对比**：Rust 的 Iterator trait（`type Item; fn next(&mut self) -> Option<Self::Item>`）
是 associated types 的教科书动机；Scala 用 path-dependent types 或高kinded
抽象达成同类目标；C++20 ranges 用 concepts + 关联类型萃取；OCaml 用 functor
签名 `S with type t` 表达——**四种机制殊途同归：集合抽象需要"元素类型"
这个关联输出**。NG 现在没有表达手段，导致任何用户自定义容器都无法接入
统一的 map/fold 协议。

**建议**：维持"现在不做"（复杂度高：涉及 trait solver 与单态化键），但把
"最小关联类型"登记进 R4/R10 之间的远期议题。一个低成本过渡方案：约定俗成的
命名约定 trait（如 `fun next(self: Self ref mut) -> Option<T>` 中 T 由 impl
上下文推断），验证 API 形态后再决定是否引入真正的 assoc type 语法。
