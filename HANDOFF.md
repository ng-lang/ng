# HANDOFF

工作区：`当前工作区`（分支：`new-start`，未推送）
更新时间：2026-09-29

## 当前状态

已提交，工作区干净（除本文件与 `.agents/tmp/`，均不入库）：

- `867fad9` feat(vnext): INC-23 union switch contracts, dispatch PBT, coercion boxing
- `e2dc666` docs(vnext): DEC-D union switch rules in D-001/D-011; mark INC-23 landed
- `35894cb` chore(agents): workstate + worklog for INC-23 acceptance and union coercion fix

验证：ng_test 656/656、ctest 657/657、ASan 656/656。INC-23 已 ✅。

## 硬约束（仍然有效）

`test/guard_test.cpp`：`kMaxCppLines = 1200`、`kMaxHeaderLines = 800`，**只拆分、绝不上调**。
当前最大文件：`src/native/lowering_expr.cpp` 958、`src/syntax/module_parser.cpp` 904、
`src/type_interner.cpp` 895、`src/syntax/parser.cpp` 886（接近上限，改动前先规划拆分点）。

拆分/重构的行为等价验证方法：`.agents/tmp/golden.sh <dir>` 导出全部 example 的
`--emit=ssa`，改动前后各导一份后 `diff -r`。

## 后续待办

按「先补完已开增量 → 修已确认缺陷 → 推进新增量」排序。每项已标来源与现状证据。

### A. INC-23 验收项 — ✅ 已完成（2026-09-29）

DEC-D 文本、契约语料、PBT 均已补齐。补齐时修复：非字面量成员值强制进 union 槽不装箱
（checker `unionCoercions` → FlowIR `WrapUnion`）。新发现的未修限制：`case -2.5`
（负浮点字面量模式）不解析；`array<i64 | string>` 等 union 元素类型不解析。

### B. 已确认的既有缺陷（本次拆分中发现或复测仍在）

进度（2026-09-30）：B1 `927aec0`、B2 `826da1c`、B3 `1177256`、B4 `56b50fc` 已修复；
B5 待语义决策（见 worklog 2026-09-30）；B6–B8 未动。

1. **嵌套调用二次推断误报 move**：`id(take(r))`（r 为仿射）报 `E-move-used`。
   根因在 `typecheck_call.cpp` 调用路径对实参重复 `infer`（INC-17 只修了 `argumentTypeAt`）。
2. **无注解 `let h = Holder.Keep(r)` 不消耗 `r`**：之后可再次使用 `r`（带注解时正确报错）。
   合成模式 enum 构造器不做 payload consumption（`inferEnumLiteral` 传 `consumePayloads=false`，
   沿用原行为），需在绑定/调用侧补消耗并复核 drop 边。
3. **非泛型形参位置的泛型 enum 字面量无法推断**：`get(Result.Ok(3))`（形参 `Result<i64,string>`）
   报 `cannot infer generic arguments`。修法：非泛型调用对 EnumLiteral 实参走 `inferExpected`。
4. **期望类型下 map spread 不支持**：`let ys: array<i64> = [0, f(xs)...]` 报参数类型不匹配
   （`inferExpected` 数组分支逐元素检查，未识别 spread）。
5. **闭包 copy 捕获仿射 `Cell` 双重释放**：`let f = [c] |x: i64| x + 1; f(1)` → `invalid heap handle`
   （INC-22 记录，复测仍在）。
6. **重载评分吞掉实参内层 TypeError**：`print([twice(xs)...])` 报 `no matching function specialization`
   （INC-14 S6 记录，复测仍在）。
7. **签名内 `<T: Trait>` 界在调用点不强制**：`pick<T: Marker>(3)` 通过（仅 `where T: X` 被检查；
   INC-14 S5 记录，复测仍在）。
8. **裸表达式语句 `move s;` 不消耗**：之后 `let t = s` 通过（INC-17 探针注记，复测仍在）。

### C. 已开增量的剩余切片 / 明示推迟项

- INC-12：泛型枚举无 derive(Eq)；无返回注解的块闭包体内不可 `return`。
- INC-13：`?` 仅限 let 根 / 语句根；表达式中部、`return e?`、闭包体、解构 let 仍拒绝。
- INC-14：空数组字面量 `[]` 无法推断类型；闭包参数不可变绑定；聚合/视图中的闭包类型仍拒绝。
- INC-16/A1.3：通过引用再创建引用（`ref r[0]`）、引用上切片仍推迟。
- INC-20c S1：std.list 构建器 O(n²) 分配（待 R6 可变构建器）。
- INC-21：field-span、span-through-ref、span-of-span 推迟；std.list `pushBack` O(n)/次。
- DEC-C / INC-19：D-015 的 refcount + CoW 条款修订未写；exit-arena 仍是过渡脚手架。
- INC-A：prelude 自动加载按用户决定跳过，待 typechecker 按模块可见性注册 overload 后再议。

### D. 未开始的增量（计划顺序，详见 09-increment-plan.md）

- INC-24 derive(Ord)（含 DEC-E 文本修订、string 比较运算）
- INC-25 parse-HIR 缓存；INC-26 f32 语义文档 + 一致性语料；INC-27 双轨收敛收尾
- INC-30/31 模块接口工件、RuntimeSession；INC-32 重载集工件
- INC-33 元组 case 模式（INC-23 已完成，已解锁）；INC-34 move-aware pattern bindings
- INC-35 Self-typed view methods；INC-36 const fun 内 trait bound
- INC-28 isize/usize；INC-29 C ABI 收尾（再拆 2–3 子步）
- 占位：R10 并发、R11 formatter/LSP/debug

## 附：本地临时文件

`.agents/tmp/golden.sh`：SSA 基线导出脚本（未跟踪，供拆分/重构的等价性比对）。
