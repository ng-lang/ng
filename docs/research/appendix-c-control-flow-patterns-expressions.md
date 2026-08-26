# Appendix C — 控制流、模式匹配与表达式能力

> Parent: [mainstream-language-comparison.md](mainstream-language-comparison.md) · Snapshot 2026-08-25 · AI-assisted.

## C1 — F5/M3：无 while/break/labeled break（严重度：中，修复复杂度：低）

**现状证据**：语法只有 `loop (binder = init)` 与 terminator `next`
（D-010；`include/syntax/ast.hpp` 无 While/Break 节点——grep 零命中）。
搜索类循环的现行写法必须引入标志位：

```ng
// 无 break 时"找到即停"只能这样写：
let mut found = -1;
let mut i = 0;
loop (i = 0) {
    if i == len(xs) { /* 只能靠条件继续 */ }
    ...
}
```

或改写为尾递归。`next` 的双重语义（循环重绑定 / 自尾递归）设计精巧
（同时保证 TCO），但 `break`/`while` 是所有对比语言的第一天标配：
Rust/C++/OCaml（例外：纯函数式惯用法则用递归）/Scala/Swift/Kotlin/Zig 全部
具备至少 while+break 之二。

**建议（复杂度低，但有一个必须先决的设计规则——Rev.2 审查补充）**：

- **前置决策（不可跳过）**：D-010 规定裸 `next` 被最近的 loop 捕获。若
  `while` 脱糖为隐藏的 loop，则 while 体内的裸 `next(args)` 会从"函数尾
  递归"静默变为"循环 continue 且 arity 必错"——语义陷阱。落地前必须在
  D-010 补一条规则（推荐：`while` 不捕获裸 `next`，体内尾递归仍指向函数；
  循环重绑定为 `continue` 形式或显式拼写）。
- `while (cond) { ... }` 纯糖：脱糖为 `loop` + 条件 Branch（FlowIR 已有）。
- `break` 需要 D-010 预留的 loop-result 决策先行（MVP 可规定 break 值类型
  为 unit、循环值为尾表达式），FlowIR 加一个 LoopBreak terminator 或复用
  Jump-to-exit-block。
- labeled break 暂不需要（嵌套场景可用命名函数+return 表达，NG 函数已是
  一等块值载体）。

注意：这与 D-010 "loop-as-expression deferred" 有交集——先落 unit-break，
表达式化随后。

## C2 — I4：switch 表达式化 + case guard（回报：高，复杂度：中）

**现状证据**：`SwitchStatement : Statement`（`include/syntax/ast.hpp:467-473`）；
穷尽性检查已实现（enum_match 全覆盖变体）；无 guard 语法。
典型样板（`example/enum_match.ng`）：

```ng
let mut total = 0;
switch (success) {
    case Ok(value) { total := value; }
    case Err(error) { total := -1; }
}
```

每个分支都要手动把结果写入外部变量——因为 switch 不能产生值。

**对比**：Rust `match` 是表达式（几乎所有枚举处理都依赖它返回值）；
OCaml match 同理；Scala match/partial function；Swift 的 if/switch 表达式
自 SE-0380（Swift 5.9，2023）起可用；C++ 的 pattern matching 提案 P2688
未进入 C++26、目标 C++29——恰好说明 NG 不应重蹈"把模式匹配当附属品"的
覆辙，而 match-as-expression 是现代语言设计的收敛方向。

**建议（复杂度中）**：
1. 允许 switch 作为尾表达式出现在块尾（block 已有 tail-expression 机制，
   checker 需要分支类型联合 + union 类型已存在，见附录 A §A2）。
2. case guard `case Ok(v) if v > 0 { }`：checker 侧把 guard 并入分支可达性
   （guard 为假视作未命中，影响穷尽性判定），lowering 复用 Branch。
3. 收益联动：I5 derive(Show/Eq) 与 M1 Result 组合子的实现都会大量受益于
   switch 表达式。

## C3 — M2：闭包缺失是最大的能力单点

**现状证据**：D-006 已接受（`|params| expr`、显式捕获列表、Fn/FnMut/FnOnce
语义族）但零实现（grep closure 仅命中 supertrait 收集变量名）。当前所有
"高阶"能力都是编译器内建特例而非库函数：

| 内建特例 | 若有闭包后的库形态 |
|---|---|
| `[f(xs)...]` map comprehension（map_comprehension.ng） | `map(xs, \|x\| ...)` |
| `[f(xs)?...]` filtered comprehension | `filterMap(xs, \|x\| ...)` |
| `f(acc, xs...)` fold 内建（folds.ng） | `fold(xs, acc, \|acc,x\| ...)` |
| `with_arena(\|arena\| {...})`（仅存在于 D-002 设计文本） | 回调即闭包 |

**对比**：一等函数方面 Rust/OCaml/Scala 自诞生即支持，Swift day-one 可用，
C++ 迟至 C++11 才有 lambda；NG 的内建特例路线在覆盖面上已经出现裂缝
（没有 filter-only、没有 zip/map2、没有 flatMap——每加一个就要动
checker+FlowIR+原生层三处）。

**建议（复杂度中–高，Rev.2 调整为两步）**：
1. **第一步独立先行：非逃逸 + copy/move 捕获的最小闭包**。该子集不需要
   loan 分析（捕获即拷贝/移动，无借用存活问题），即可解锁 map/fold 库化
   ——把用户可见的最大能力单点从 A1（最长实现杆）上解耦。
2. **第二步随 A1：borrow 捕获**（`ref`/`ref mut` 捕获）。非逃逸捕获正是
   loan 分析的最小完整测试集（捕获 = 匿名 borrow 场景）；D-006 的捕获模式
   表就是规格。
3. 库化收益立现：seq.ng 的 sum/reverse/arrayContains 全部可改为泛型
   高阶函数，内建特例转为薄糖或废弃，反而**降低**编译器复杂度。
4. Fn/FnMut/FnOnce 先以三个 auto trait 形式存在（derive 机制同源），
   不需要新类型系统机制。
5. 可行性佐证（Rev.2 probe）：泛型枚举 switch、affine payload 绑定与嵌套
   early-return 当前全部可用——闭包所需的表达式基础已就绪。

## C4 — 其他控制流观察

1. **`next` 双目标规则**（最近循环优先，函数次之）在嵌套场景会静默改变
   直觉（循环内想尾递归必须显式拼写，D-010 已预留后续设计）——建议在
   用户文档给显著警示即可，无需改语义。
2. **if 强制括号** `if (x < y)`：与 Rust/Kotlin（无括号）不同但与 C/C++ 一致；
   属风格决策，保持现状并记录即可（避免无谓的语法翻新成本）。
3. **递归默认栈行为**：非尾递归走 QBE 栈帧，深递归（如 std.list length 于
   万级链表）有栈溢出风险且无诊断。低成本缓解：libngrt 加栈水位检查或文档
   声明限制。对比：OCaml/Swift 运行时都有栈溢出 trap。
