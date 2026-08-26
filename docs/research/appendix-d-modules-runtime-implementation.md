# Appendix D — 模块系统与实现架构

> Parent: [mainstream-language-comparison.md](mainstream-language-comparison.md) · Snapshot 2026-08-25 · AI-assisted.

## D1 — I2：TypeCheckResult 字符串/TypeId 双轨（回报：中，复杂度：低–中）

**现状证据**（`include/typecheck.hpp:250-255`）：

```cpp
std::unordered_map<const hir::Expression *, std::string> expressionTypes;   // 显示名轨道
std::unordered_map<const hir::Expression *, TypeId> expressionTypeIds;      // 语义轨道
std::unordered_map<uint32_t, FunctionType> functionTypes;                   // 字符串签名
std::unordered_map<uint32_t, FunctionTypeIds> functionTypeIds;              // 语义签名
```

FlowIR/原生层仍多处用字符串名查类型/方法（TYP-07 的遗留形态：编译器从
字符串再推断类型）。双轨意味着每个消费者都要选择轨道，且字符串轨道无法
表达泛型实例、引用、trait view 等结构事实。

**对比**：Rust rustc 用 `Ty<'tcx>` 单一语义表示 + 惰性打印；OCaml 编译器
内部 types 与打印分离（Types.t vs Out_type）；Scala 3 TASTy 以语义树为唯一
真源。共识：**字符串只是视图，不是存储**。

**建议**：不做大重构——以"新代码只读 expressionTypeIds"为纪律，逐季度把
lowering.cpp 的字符串查询点迁到 TypeId 轨道，最后把字符串轨道降级为纯
诊断打印缓存。与 A3（instance artifact）共享同一批触点，顺路完成。

## D2 — I3：FlowIR 内建 std.list 专用指令（回报：中，复杂度：中）

**现状证据**（`include/flowir.hpp:49-52`）：

```cpp
/// Walks a recursive-list enum and reports its Cons-chain length.
EnumListLength,
/// Walks a recursive-list enum and extracts the head at an index.
EnumListGet,
```

通用 CFG IR 里内嵌了 `List<T>` 这一具体 stdlib 类型的遍历指令，
配套 helper `ngrt_enum_list_len/_get` 以内嵌 QBE IL 形式生成于
lowering 层（`src/native/lowering.cpp:612,635`，非 libngrt.c 常驻函数）。
这是分层泄漏：stdlib 数据结构的性能特例进入了 IR 契约，任何 IR 消费者
（QBE 后端、未来解释器、验证器）都要理解 List 形状。

**对比**：Rust 对 Vec/str 的优化在 MIR 之后的标准库 intrinsics 层；
C++ 在 std 实现层；OCaml 在 runtime primitives（如 String.length 的
原语化）——共同点是**特例都在稳定 ABI/runtime 边界之下，不在中间 IR 语义里**。

**建议**：短期保留（它们让无堆域下的 std.list 可行）；B1 落地后随附录 E §E1
的 list 迁移一并移除，或降级为"enum 迭代泛指令"。登记为 B1 验收项的一部分，
避免成为永久债务。

## D3 — I7：整程序编译与缓存缺失（回报：中，复杂度：低–中）

**现状证据**：`import` 把传递闭包内全部源文件合并为一个编译单元重新走完整
流水线（module_loader.cpp；A2 计划文档自述 "Stdlib preload as a session-owned
artifact set, not per-run source parse" 为未完成项）。当前每跑一个示例都要
重新 parse+check 全部依赖源码。

**对比**：四门语言的独立编译机制各不相同但目标一致——OCaml .cmi 接口哈希、
Rust crate metadata fingerprint、Scala TASTy、C++20 BMI。NG 整程序模式在
正确性上最简单（无跨单元一致性难题），代价是 O(全部源码) 的每次启动成本。
imgui IDE 场景（runNgi 重入编译）会放大该成本。

**建议（不等于 A2 全量，先做简化版）**：
1. mtime+内容哈希键控的 **parse 缓存**（HIR 树内存复用），命中则跳过
   lex/parse——实现最小，收益立现（stdlib 是最大的不变源码集合）。
2. check 缓存暂缓：checker 有全局 interner 状态，缓存失效面大，等 A2 的
   session 化一起做。
3. 该简化版与 A2 不冲突，是它的第一块脚手架。

> **Rev.2 更正**：本节原稿以"imgui IDE 场景 runNgi 重入编译放大成本"为动机
> ——该机制已随 de95fa1 移除，现 ng_ide 通过 fork 子进程编译
> （`driver.cpp:107-138`），编译内存随子进程退出回收。I7 的真实收益场景是
> **每次 ngi 启动重复 parse 全部 stdlib 源码**（含每个测试用例 × 456 次全量
> sweep），此收益仍然真实且可测量。

## D4 — 实现架构正面评价

1. **QBE 选型**：以 ~19k 行自有代码获得原生代码生成，工具链复杂度比
   LLVM 系（rustc/swiftc/zig 均背负 LLVM 版本矩阵）低一个数量级；
   QBE 的优化上限（无向量化等）对当前阶段无关紧要。Zig 自研后端的教训
   （多年才达可用）反证该选择的务实性。
2. **side-table checker**：语法/HIR 不可变 + 侧表存放检查产物，与 rustc 的
   "query/无 AST 变异"方向一致，且比 gap register 中记录的 legacy
   （AST 直接变异，TYP-02 stop-the-line）有本质改善。4537 行的单体
   typecheck.cpp 是主要维护风险（对齐 TYP-01），建议按决策域切分为
   traits/generics/ownership/const 四个子模块——纯移动重构，风险低。
3. **clone+recheck 单态化**：以 Rust 同款策略规避了 Scala specialization
   的工程泥潭（其编译器团队多年修补 erasure 与特化交互）；instance 表去重
   已就位（post-cutover A3 "delivered by construction"）。
4. **测试文化**：49 个测试套件 + 示例全量 sweep + 负诊断断言，形态上接近 rustc
   UI 测试实践，是同类个人项目中少见的严谨度。
