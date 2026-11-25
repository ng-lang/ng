# Appendix B — 所有权、引用与错误处理

> Parent: [mainstream-language-comparison.md](mainstream-language-comparison.md) · Snapshot 2026-08-25 · AI-assisted.

## B1 — F2：深拷贝语义 + 原生层零释放（严重度：高（长期））

**现状证据（Rev.3 按全量审校修正为位点式描述）**

1. 语言层：Copy 类型的深拷贝发生在**特定位点**——聚合字面量构造、ref/cell
   写入、字符串拼接、非直通的绑定/传参/返回（`include/value.hpp:253-285`
   `deepCopy`；D-015 规则 2）。审校探针证实参数直通返回与 mut 自重绑定
   **不**克隆（SSA 中仅 `ret %p0`）——该差别直接影响未来 arena/RC 改造的
   插桩位置选择。
2. 原生层：聚合分配走 `malloc` 的 `{len,cap,elements}` 结构
   （`src/native/lowering.cpp:2100-2130,2332`），克隆 helper
   `ngrt_arr_clone_words/_strings/str_clone`（:342-394）；对整个 lowering.cpp
   检索 `free|Drop-fini` 无任何生成调用——数组/字符串的中间拷贝**从不归还**。
3. Drop impl 调用确实进入 FlowIR（`flowir.cpp:67,1291,1398` 使用
   return/fallthrough/block 三个 drop 边表），但只覆盖 affine 名义类型；
   Copy 集合无 Drop。
4. 被 MakeRef/MakeTraitView 触碰的 local 强制永久堆化
   （`lowering.cpp:1128` "locals rooted by MakeRef/MakeTraitView need heap cells"）。

**后果分级（Rev.2 定性修正，Rev.3 换用自包含可复现数字）**

- 单次运行内的长循环：已定量证实。自包含探针（字符串拼接
  `s := s + "."`，`loop (i = 0)` 计数退出）：

```ng
fun main() -> unit {
    let mut s = "";
    loop (i = 0) {
        if (i == 50000) { return; }
        s := s + ".";
        next(i + 1);
    }
}
```

  实测 maxRSS：1 万次迭代 ≈54MB，5 万次 ≈1.41GB
  （`/usr/bin/time -l ./build/ngi leak.ng`，2026-08-25；两轮独立审查
  方向一致）。增长由"每次拼接分配新串且旧串永不归还"驱动，
  总复制量随长度平方放大。
- 跨运行累积：ng_ide 通过子进程编译（libngrt `systemOutput`/popen 启动
  ngi 子进程，driver 内部再 fork qbe/cc，`driver.cpp:107-138`；
  runNgi 已随 de95fa1 移除），子进程退出即回收——原报告
  "imgui IDE 每帧执行放大泄漏"的表述不再成立。
- 未来进程内嵌入（R6 RuntimeSession 方向）：恢复为长驻风险。

**候选路线（Rev.2 改写：三条路，诚实标注各自边界）**

1. **exit-time bump arena**（复杂度低）：所有 malloc 改走 arena、退出统一
   free。收益是卫生性的（分配速度/碎片 + 让 ASAN 泄漏报告干净以配合 I8），
   **对单次运行内的增长无效**——它只是把 free 从"永不"挪到"退出时一次"。
   不要把它当作长驻方案宣传。
2. **聚合头 RC / CoW**（复杂度中，随 R6/B1 决策）：Swift 十年验证了
   "值语义观感 + 计数共享存储"；D-015 已预留 CoW 条款。这是长驻与
   大循环问题的正解方向。
3. **大 Copy 聚合参数传递的非逃逸视图**（Rev.2 新增，Hylo mutable value
   semantics 的最小片段）：现有 scoped-ref + 逃逸拒绝机制使这条路线在 NG
   异常便宜——`fun f(xs: array<i64> ref)` 已经可用且不拷贝；缺的只是
   习惯引导/语法便利（如自动借用）而非新机制。直击 O(n²) 传参拷贝，
   早于 R6 可做。

## B2 — M1：prelude Option/Result + `?`（优先级最高的缺失项）

**现状证据**：每个示例自行定义 `enum Result<T,E>`（`example/enum_match.ng`
开头即重复定义）；prelude 无 Option/Result（§3.1）；D-007 已接受 `?` 但标注
"desugared in HIR after a stable Result trait/interface is defined"，未排期。

**对比**：Rust（Result+?）、OCaml（result+bind/let*）、Scala（Try/Either+
for 推导）、Swift（Optional+try?/guard）、Kotlin（nullable 类型语法级内建）、
Zig（error union + try）——错误传播是唯一一个**所有**对比语言都在语法层
支持的横切机制。NG 当前表达"可能失败"需要手写枚举 + switch 样板。

**建议（复杂度中，但大部分是 stdlib 工作）**：
1. 第一步纯 stdlib：prelude 定义 `Option<T>`/`Result<T,E>` 枚举 +
   基础组合子。零编译器改动，立即可用（switch 已支持泛型枚举解构）。
2. 第二步语法糖：`?` 后缀在 HIR 侧表阶段脱糖为 switch-early-return
   （D-007 既定方案）。FlowIR 已有 early-return 与 drop 边处理，
   新机制面很小。
3. 与 A1 loan analysis 无依赖关系，可提前执行——这是本报告对 plan 排序的
   主要调整建议。Rev.2 对抗性审查已用 probe 实证可行性：泛型枚举 switch、
   带 Drop impl 的 affine payload 绑定、嵌套 early-return 当前全部工作
   （`flowir.cpp:67,1291,1398` 三张 drop 边表在位），`?` 脱糖所需的机械件
   已齐；D-007 的 gate（"stable Result trait/interface"）只是设计工作，
   与分析无关。

## B3 — M5：内部可变性原语 Cell 类（缺失）

**现状**：没有任何"共享可变"手段：ref mut 有排他性检查（borrow_check_test），
闭包未落地（落地后 `ref mut` 捕获会立刻撞上"多个回调共享状态"需求），
memory 模块只有 handle 式 Box。

**对比**：Rust Cell/RefCell 把别名检查从编译期挪到运行期；Swift 用引用类型
类天然共享；OCaml ref；Scala var 字段；Kotlin 无但 JVM 对象皆引用。共同点：
**都存在一个"我保证访问不重叠/愿意付运行期检查"逃生舱**。NG 目前没有，
导致图结构、缓存、观察者模式无法自然表达（std.list 只能靠重建链表绕行，
见附录 E §E1）。

**建议（复杂度中，登记至 D-006 落地时一并设计）**：Copy-first 下最轻方案是
handle 式 `Cell<T>`（复用 memory 模块 handle 表 + checked load/store），
不需要类型系统新概念；RefCell 式借用计数可后置。与 B1 的 RC 选型联动决策。

## B4 — 正面结论：值得坚持的所有权路线

1. **隐藏生命周期的 scoped-ref 路线是正确取舍**。佐证：Rust 社区十年打磨
   elision 规则与诊断基建（其投入规模本身即说明显式生命周期的可用性代价；
   Polonius 2026 目标页明确为 alpha 稳定化接受 10–20% 检查开销）；Scala
   capture checking 自 2022 实验至今（3.8 仍 experimental）才逼近稳定。
   NG 的"scoped non-escaping ref + 未来 origin 合约"用小得多的机制覆盖
   常见场景。
2. **首片 NLL 已够用**。post-cutover A1 追求的 origin contracts 对应
   Polonius 形态的位置敏感分析（10–20% 开销预算）；NG 应按同样口径评估
   该切片的 checker 成本，避免为长尾模式付出全量分析代价（早期 Polonius
   材料亦讨论过"NLL 失败时再跑精确分析"的混合策略，当前 goals 页未再
   强调，标注为待核实）。
3. **`move` 可显式可省略 + 赋值复活**比 Rust 的隐式 move 诊断更友好，
   且与 partial-move 字段恢复规则自洽（`example/move_semantics.ng` 全覆盖）。
