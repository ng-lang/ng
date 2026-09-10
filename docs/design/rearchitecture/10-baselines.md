# Verification Baselines — 存档

> **AI-assisted document:** drafted with AI assistance.
>
> 记录各实现增量（`09-increment-plan.md`）产出的可复现验证基线，
> 供后续增量做前后对照（如 INC-18 arena 改造）。每条基线注明日期、
> 工具链与复现命令。

## INC-0 · ASAN/UBSAN 与泄漏基线（2026-08-26）

**环境**: macOS 15.5 (arm64), Apple clang 17.0.0 (clang-1700.0.13.5),
Xcode 16.4 leaks(1)。

### 平台约束与工具选型

- **LeakSanitizer 在 arm64 macOS 不可用**（Apple clang：
  `AddressSanitizer: detect_leaks is not supported on this platform`）。
- 因此泄漏计数改用平台原生 `leaks(MallocStackLogging)`：对 `ngi
  --output` 产出的生成可执行文件运行 `/usr/bin/leaks -atExit -- <exe>`，
  输出确定的「N leaks for M total leaked bytes」。注意：其 stdout 接管道会
  挂起，必须重定向到文件。
- ASAN/UBSAN 变体照常生效于编译器管线（`ng`/`ngi`/`ng_test` 进程内）；
  `libngrt` 有意保持不插桩——它被链入由系统 cc 裸链接的生成程序，
  插桩会产生未解析的 `___asan_*` 符号。

### 构建变体

```bash
cmake -S . -B build-asan -GNinja -DNG_ASAN=ON   # 或 cmake --preset asan
cmake --build build-asan -j && ctest --test-dir build-asan -j
```

### 基线数字（2026-08-26）

| 项 | 数值 |
|---|---|
| 常规构建套件 | 458/458 通过（456 存量 + 2 个 INC-0 测试） |
| ASAN+UBSAN 变体套件 | 458/458 通过 |
| 泄漏探针（20000 次字符串拼接）maxRSS | ≈207.5 MB（`/usr/bin/time -l`） |
| 泄漏探针 leaks(1) 计数 | **19999 leaks / ≈205.2 MB**（每次拼接滞留一个缓冲） |

探针语料：`test/probes/leak_concat.ng`；runner：
`test/leak_probe_test.cpp`（阈值断言 ≥15000 处分配 / ≥100 MB）。

### 本步修复的缺陷

ASAN 变体首跑即在 `typecheck.cpp` 抓到一处真实 heap-use-after-free：
`checkBlock` 持有指向 `lastUseCache_` 元素的引用，嵌套函数检查触发
`lastUseCache_.clear()` 后引用悬垂。已改为按值持有 last-use 映射
（见 typecheck.cpp `checkBlock` 注释）。修复后 asan 变体全绿。

### 对后续增量的意义

- F2/F4 类内存缺陷自此在 CI 可见（asan 变体 + 泄漏计数双通道）。
- INC-18（exit-time bump arena）落地时，以本表 maxRSS/leak 计数作
  before 对照；RC/CoW（DEC-C）落地后，探针泄漏已归零。

---

## INC-18 · exit-time bump arena 落地对照（2026-09-10）

纯程序值分配改道 per-process mmap 匿名 bump arena（`ngrt_alloc`/`ngrt_new_string`
bump，`ngrt_free` 按指针归属调度，atexit 向 OS 归还）。生成代码 `$malloc` 全移除
（helpers + 13 处 lowering 发射点 → `$ngrt_alloc`）。保留 malloc 暂存：regex
`ngrt_c_string`、readFile/systemOutput 缓冲（随用随释放）。

| 指标 | INC-0 基线（before） | INC-18（after） | 说明 |
|---|---|---|---|
| 拼接探针 leaks(1) 计数 | 19999 leaks / ≈205.2 MB | **0 leaks / 0 bytes** | `test/leak_probe_test.cpp` 翻转契约 |
| arena_all 探针 leaks(1) | — | **0 leaks / 0 bytes** | `test/probes/arena_all.ng` 覆盖全部发射点 |
| 拼接探针峰值 footprint | ≈207.5 MB（maxRSS） | **194.5 MB**（leaks pte footprint） | arena 内增长，看 run 内总账 |
| 常规构建套件 | 458/458 → 609/609 | 全绿 | INC-18 新增 `[INC-18]` 4 例 |

### 如实标注的限制（过渡脚手架，随 RC/CoW 落地拆除）

- **不解决单运行内增长**：arena 块只增不还（进程退出统一归 OS）；长运行大分配
  峰值 footprint 高于等价及时释放方案是期望内的账，并非泄漏（leaks 计 0）。
- 主机侧（ng_test / ngi 进程内）arena 块同样只增不还、进程结束回收；测试进程
  内统计以 `ngrt_arena_bytes()`/`ngrt_arena_blocks()` 观测。
- `ngrt_release` 退化为句柄 live-table 逻辑清除，物理归还随 DEC-C（INC-19）
  的 RC/CoW 存储头一并设计。
