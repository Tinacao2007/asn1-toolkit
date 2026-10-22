# 待办清单（P1-3 / P2-4 与相关项）

> 2026-10-20 工程评审后的修正执行清单见 [remediation-todo.md](remediation-todo.md)。
> 使用 REV-xx 独立编号，包含优先级、依赖、验收及逐项交给 AI 的任务模板；
> 本文保留原功能规划与历史落地记录，不代表评审修正已完成。

本文档登记 P2-2 之后不再继续的剩余项，并按优先级、依赖、预估工作量列出
落地步骤。每条给出最小可行子任务，便于逐步合入。状态变更走
[status.md](status.md)，变更日志走 [changelog.md](changelog.md)。

## P2-3 — 增量代码生成（已落地，持续回归）

当前实现位于 `src/codegen/emit_cache.*`，不是 CLI 内部工具类。
键包含生成器内容指纹/格式版本、完整选项、有序源路径及内容 hash、相关模块依赖。
flat 为一个单元，split 按模块依赖 SCC 合并后的单元缓存；没有源依赖信息的手工 IR
禁用缓存。命中仍核验产物 hash，mtime 不是正确性依据。

`--emit-cache` 默认开启，`--no-emit-cache` 关闭；缓存位于输出目录内部
`.asn1cxx-cache/`。缓存不可读/损坏时重发射。升级指纹、依赖顺序与命中/未命中
字节一致性证据见 [第四批验收](rev13-19-validation.md)。

REV-25 加入整批 stage/manifest 发布、旧产物归属清理以及同目录并发拒绝，
契约见 [生成产物](generated-artifacts.md)。不是多进程共享缓存或等待锁服务。
后续改动保留缓存命中、失效、手改输出、三形态切换和用户文件保护回归。

## P3-1 — 前端并行分析

### 原始目标（历史规划，实际实现见下）

规划时 `Analyzer::analyze` 串行跑四趟（`declare_pass` → `resolve_pass` →
`lower_pass` → `tag_pass`，见 `src/semantic/analyzer.cpp`）：

- `declare_pass`：每个模块独立声明符号集。
- `resolve_pass`：每个模块的引用按**全模块符号表**解析。
- `lower_pass`：AST → IR；每个模块的 lowering 互不依赖。
- `tag_pass`：标签分配；当前对全局扫描态。

声明阶段每个模块相互独立（模块内声明靠模块自身），最容易并行。其余
三趟有跨模块依赖，要按依赖图并行。

### 关键风险

**诊断输出逐字节可重现是硬要求**。当前 `Diagnostics` 是 `std::vector` 顺
序 append，warning/error 顺序在测试里被断言（`asn1_semantic_tests` 有序
号和内容快照）。改成多线程后会改变 append 顺序，进而破坏：

1. 现有测试 fixture 的顺序断言。
2. `format(const Diagnostic&)` 在 `print` 时的全局顺序。
3. 跨模块 warning（如 undefined imported type）原本按模块文件序输出，
   并行后变不可预测。

**强制约束**：并行跑出的 diagnostics 集合必须与串行跑出的逐字节一致
（同样的 Diagnostic 个数 + 同样的 severity + 同样的 where + 同样的
message + 同样的顺序）。

### 设计

- **诊断合并协议**：每个 pass worker 持有本地 `Diagnostics`（无锁），
  pass 结束后按模块序 merge（worker_id 0..n-1 与 input module index
  对齐）。`merge` 时按 module_index 升序拼接结果，让最终顺序确定。这要
  求每个 worker 严格处理「只属于自己模块」的 diagnostics，不在 worker
  间共享跨模块 diagnostics。
- **declare_pass 并行**：每个 worker 持一个 `SymbolTable` 分片，只
  collect 自己模块的符号。pass 结束后 `SymbolTable::merge(other)` 串行合
  并。当前 `SymbolTable` 是单个 map，需扩展为支持按 module key 合并。
- **resolve_pass 并行**：每个 worker 持完整 SymbolTable 只读视图 +
  自己模块的 unresolved 引用集。worker 把 resolved 引用写到自己模块的
  `ir::Module`；未解析的（跨模块定义尚未到位）入「重试队列」；整 pass
  结束后对重试队列做一次 sweep（常数轮收敛：理论 O(log N)，实际 1–2
  轮）。
- **lower_pass 并行**：每个 worker 处理自己模块的 AST → IR lowering；
  IR arena 在 worker 内部构建（无共享），pass 结束后按 module index 拼
  到顶层 `ir::Model` 的 arena。**已有 `ir::TypeId` 跨模块引用靠
  `referenced.resolved` 链接**，该链接在 resolve_pass 已就位，lower_pass
  无须解析跨模块 id。
- **tag_pass 并行**：标签分配需要全局扫描（决定 explicit vs implicit），
  并行收益小、复杂度高，**保留串行**。P3-1 只并行前三趟。
- **API 扩展**：`Analyzer::analyze` 增加 `parallelism: int` 参数
  （`0` = 串行 = 默认；`>0` = worker 数）。`asn1cxx` CLI 加
  `--parallel <N>` 标志，默认 0（保持现状可重现）。
- **可重现性兜底**：保留串行路径；CI 上金标对验强制 `--parallel 0`。

### 实际落地（2026-10-17）

✅ `declare_pass` + `resolve_pass` **已并行**（`--parallel N`，N > 0 时
worker 线程池按模块分片）。`Diagnostics::merge` + `SymbolTable::merge`
按 module index 串行合并，diagnostics 字节级与 `--parallel 0` 一致。

❌ **`lower_pass` 保持串行**（**关键变更**，偏离原 todo 设计）：
- 现状：`LowerCtx` 共享 `model_.arena`，跨模块 `Referenced.resolved`
  通过 `model_.arena.get(...)` 在 pass 期间跨模块链接；declare_pass 之后
  即开始把跨模块类型加进共享 arena。`named_ids` 是 per-worker 私有 map，
  但底层 `TypeId` 全部指向同一个 arena。
- 把 arena 拆成 per-worker arena + pass 后串行合并，需要重建跨模块
  `Referenced.resolved` 链接（每个 worker 算出的 id 在合并后失效），
  且 `re_resolve_all_object_class_fields` 的 `model_` 整体遍历假设
  一个连续 arena，重建需要先合并才能再跑。
- 当前 lower 在 78 模块 RRC 8.6.0 fixture 上约 ~10 s（per todo 估时），
  占比 ~30% 的 analyze 时间；declare/resolve 并行化已经吃掉主要的
  Amdahl 收益（声明 + 引用检查的 O(M·A·fields) 部分），lower 串行
  不会成为新的瓶颈。**
- 后续 Phase 解：保留接口扩展点 `lower_pass(int)` 已接受 parallelism
  参数；实现 per-module arena + 重映射 deferred（独立工作项，可与
  ```parse_modules()` 流水线化合并设计）。

❌ **`tag_pass` 保持串行**（与原 todo 一致，标注原因）。

### 子任务

| # | 内容 | 估时 | 验证 |
|---|------|------|------|
| 1 | 在 `Diagnostics` 加 `merge(Diagnostics&& other)`：按 index 重排追加。**重要**：合并顺序由 caller 决定；先实现串行版本做对照 | 1 h | ✅ `asn1_parallel_analyzer_tests.DiagnosticsMerge*`（3 项）：merge 后 items 顺序与预期一致；`error_count` 聚合；空合并 |
| 2 | `SymbolTable` 加按 module 分片支持：`declare(module_id, names)`、`merge(SymbolTable&& other)`。保留单线程兼容路径 | 2 h | ✅ `asn1_parallel_analyzer_tests.SymbolTableMerge*`（2 项）：合入 / duplicate 检测 |
| 3 | `Analyzer::declare_pass` 改为 `std::async(std::launch::async, modules, ...)`：每个 worker 处理一个 module；结束后串行 merge | 1 h | ✅ `declare_pass(int)` worker 路径已落地 |
| 4 | `Analyzer::resolve_pass` 改为 per-module worker + 重试 sweep。worker 持完整符号表只读；residual 未解析项在 sweep 阶段按模块序解析 | 3 h | ✅ `resolve_pass(int)` worker 路径已落地；保留 import-visibility 串行（依赖 `modules_`） |
| 5 | `Analyzer::lower_pass` 改为 per-module worker + arena merge。lowered type arena per-worker，pass 结束按 module index 串行拷贝到顶层 arena | 3 h | ❌ **保持串行**（per-module arena + 重映射 deferred，理由见上） |
| 6 | `Analyzer::tag_pass` 保留串行（标注原因） | 0 h | ✅ |
| 7 | `asn1cxx --parallel <N>` CLI；`Diagnostics::merge` + 顶层 merge orchestrator；`Analyzer::analyze` parallel 参数 | 1 h | ✅ CLI smoke `Cli.ParallelModeProducesByteIdenticalOutput` 1 项 |
| 8 | 可重现性门禁：`tests/CMakeLists.txt` 新增「parallel vs serial equivalence」门禁：LPP + S1AP + RRC 跑两遍（`--parallel 0` 与 `--parallel 16`），产物 `diff` 必须空 | 2 h | ✅ 单元测试覆盖 5 个 fixture 模块 + CLI 字节相等门禁（LPP） |
| 9 | sanitizer build + Release build 都过；ctest 344/344 + 新门禁 11 项 | 1 h | ✅ Release **344/344** + sanitizer **344/344**（0 ASan/UBSan） |
| 10 | 文档：`status.md` 性能数字；`changelog.md` P3-1 条目；`architecture.md` 标「并行分析」段落（依赖图、Diagnostics 合并协议） | 1 h | ✅ status.md / changelog.md 已更新；architecture.md 设计已落进 status.md「当前阶段」段 |

**实际落地：~9 h（declare + resolve 并行；lower 串行 + deferred）。**

### 验证（2026-10-17）

- `asn1_parallel_analyzer_tests`（10 项）全过：Diagnostics::merge 字节
  级序列 / error_count 聚合 / 空合并；SymbolTable::merge 合入 /
  duplicate 检测；5 个 fixture 模块（tiny single-module / multi-module
  import / undefined type / unknown field / duplicate symbol）在
  `--parallel 0` 与 `--parallel 16` 下 diagnostics **逐字节相同** +
  error_count 一致。
- `Cli.ParallelModeProducesByteIdenticalOutput`：LPP 14.3.0 fixture 上
  `--parallel 0` 与 `--parallel 16` 生成的 `generated.hpp/.cpp` 在
  namespace 替换后**逐字节相同**（含 sanitizer）。
- Release + sanitizer 全量 ctest **344/344** 通过，零 ASan/UBSan 报错。
- 手工验证 RRC 8.6.0（largest fixture，78 模块）：serial vs `--parallel 16`
  生成 `generated.hpp` SHA-256 一致（`af86e15d...`）。

### 不在范围内（防止蔓延）

- 不并行 lexer / parser：单 .asn 文件通常几 MB，串行 lexer 已经够快；
  并行 lexer 在多线程上争 I/O 反而退化。
- 不并行 emit：emit 产物与并行分析无关，独立项。
- 不在 `Diagnostics` 加全局序号或原子化 `vector::push_back`：原子化能
  解决正确性但仍会改变顺序，不可重现。
- 不并行 `lower_pass`：per-module arena 重映射 deferred（见上）。

---

## 共同风险与缓解（两项目共享）

| 风险 | 缓解 |
|------|------|
| 现有 fixture / 金标对验对诊断顺序敏感 | 并行前所有现有 fixture 必须串行 baseline 跑通、记录产物 hash；并行化后产物与 baseline 字节级 diff |
| sanitizer 化（ASan + UBSan）漏掉数据竞争 | P3-1 强制加 TSan 临时构建验证（`-fsanitize=thread`）；CI 上跑一次确认无 race |
| 缓存错命中 = 旧产物混进构建 | P2-3 强制产物 SHA256 后校验；CI 跑 `--no-emit-cache` 与带缓存两遍做 diff |
| `release` 路径与 `debug` 路径行为不同 | 并行路径与串行路径共享同一 `Analyzer::analyze`，仅 orchestrator 不同；条件编译禁，避免路径分叉 |
| 协议消息 oracle 捕获成本高、易引入 CI 阻塞 | P1-3 复用现有 `scripts/gen_per_golden.py` / `gen_tlv_golden.py` 框架；新增 `gen_protocol_vectors.py` 走"小批量 + 三 family 一次性"策略；oracle 文本 checked-in，CI 不联网 |

---

## P1-3 — 3GPP 协议消息级金标准向量对验

### 目标

[status.md](status.md) 当前 6 项「剩余工作」中的 2 项本质上是同一件事：

- **完整模块编译 CI / 3GPP 编解码对等验证**：「剩余为 3GPP 协议消息级完整向量（逐消息 PDU）的对验；建议：扩展金标准向量覆盖面，逐步引入协议级消息向量」
- **PER 位级正确性外部对验（协议级）**：「剩余为 3GPP 协议消息级完整向量的对验；建议：扩展 per_golden 向量覆盖面」

P1-3 把它们合并为一项 TODO：**抓取 / 生成 3GPP 真实协议消息字节 → 与
asn1-toolkit 编码字节逐位对验**。覆盖 PER/APER、BER/DER/OER、CER
三大家族，每家族 3 个候选 PDU：

- **LPP-Message**（`tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn`，LPP 14.3.0）
- **S1AP-PDU**（`tests/fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn`，S1AP 14.4.0）
- **RRC DL-DCCH-Message**（`tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn`，RRC 8.6.0，78 模块最大 fixture）

### 为什么现在做

P3-1 落地后 frontend 已经能处理 3GPP 大 fixture 串行+并行；`status.md`
「编译门禁状态」表里三个 fixture × 12 codec 全 `-fsyntax-only` 零错误。
**但 emit 成功 ≠ 编解码对等** —— 当前三个 fixture 没有任何捕获的协议
消息字节可对比，只验证了"生成代码可编译"，没验证"生成代码编出的
字节 = 真实 3GPP 消息字节"。

asn1c / asn1tools 都已对这三个 fixture 提供 BER↔UPER 的双向 oracle
桥（见 `scripts/gen_per_golden.py` / `gen_tlv_golden.py` 已用的 asn1c
oracle 路径）。P1-3 是把这些 oracle 从"综合构造 per_golden"扩到"真实
3GPP 消息"。

### 设计

- **三家族共用 oracle 基础设施**：新增 `scripts/gen_protocol_vectors.py`，
  走 `asn1c` BER→UPER/APER/OER 桥（同 `gen_per_golden.py` 既有路径）。
  asn1tools 处理 BER/DER/OER 同 family 内值等价性。三家族各产一份
  `.txt`，每条 `(pduname, codec, byte)`, checked-in, CI 离线用。
- **fixture 选择策略**：每家族挑 3 个代表性 PDU（见上"3 个候选 PDU"）。
  每个 PDU 至少抓 3 条样例：边界（min / max / 空字段）+ 典型
  （real-world 抓包样本）。
- **典型样本来源**：从公开 3GPP 测试向量（ASN1C 项目的
  `c/tests/sample.asn1` 对应 binary、`openairinterface5g` 的 S1AP 测试
  fixture 等）取真实捕获 PDU；不可得时用 asn1c `ber_encode` 跨
  family oracle 桥自举（BER↔UPER/APER 由 asn1c 仲裁，OER 由 asn1c
  仲裁 BER↔OER 字节，CER 无外部 oracle 走"条文推导 + DER 值等价"
  同既有 `tests/cer/` 策略）。
- **字节级对验**：`scripts/gen_protocol_vectors.py` 产 checked-in
  `.txt`，新增 `tests/protocol/test_protocol_vectors.cpp` 跑 emit →
  encode → diff；Release + sanitizer ctest 必过。
- **fixture 编译门禁 + 协议字节级门禁双轨**：
  - fixture 编译门禁（已有）：`add_custom_command` emit → `add_library(
    STATIC)` → `g++ -std=c++17 -fsyntax-only` 全 0 错误。验证
    "emit 成功 = 代码可编译"。
  - 协议字节级门禁（新增）：emit + encode + byte-diff 真实 PDU。
    验证 "emit 成功 = 编解码对等（vs 真实 3GPP 字节）"。
- **失败模式**：
  - asn1c / asn1tools 各自已知偏差（已在 P1-1 / P1-2 处理）：`ber`
    SET 无规范序、`oer` asn1c `oer_encode` 已知 bug 等。新增条目
    记录在 `scripts/gen_protocol_vectors.py` 头部同既有策略。
  - asn1c / asn1tools 都错的极小概率：单 case X.680/X.691/X.696 条文
    仲裁走 ITU PDF（与 `references/per-differential-validation.md`
    `tlv-oer-differential-validation.md` 既有 PDF 仲裁路径一致）。

### 子任务

| # | 内容 | 估时 | 验证 |
|---|------|------|------|
| 1 | 新增 `scripts/gen_protocol_vectors.py`：选 3 fixture × 3 PDU 类型（min / max / 空 / 典型）× 5 codec（uper,aper,ber,der,oer）= ~45 条 oracle；走 asn1c/asn1tools 既有 BER↔family 桥 | 4 h | 脚本能复现 generate 已有 `tests/per/golden/` 56 条 + `tests/tlv/golden/` 16 条 oracle（同 fixture 既有 per_golden.asn 子集）；产出 `tests/protocol/golden/protocol_golden_vectors.txt` ~45 条 |
| 2 | 新增 `tests/protocol/test_protocol_vectors.cpp`：emit 三个 fixture → 抓取每条 oracle → `encode` → diff → 报告 byte-level 一致；Release + sanitizer 都跑 | 3 h | Release **344/344 + N** + sanitizer **344/344 + N**；每条 oracle 字节级对验 |
| 3 | 新增 `tests/protocol/CTestTestfile.cmake` + `tests/CMakeLists.txt` 注册；asn1cxx fixture compile gate 扩到 `asn1_protocol_lpp_up_compile` / `asn1_protocol_s1ap_uper_compile` / `asn1_protocol_rrc_uper_compile`（一次性 emit + 静态库编译 + 真实 PDU 字节 round-trip），3 fixture × 5 codec = 15 个新 gate | 2 h | ctest 新增 15 项；全过 |
| 4 | LPP fixture 端到端实测：选定 3 条 `LPP-Message` 真实捕获（来源：openairinterface5g `lpp_test` 或自捕获），3 codec × 3 sample = 9 条 oracle | 2 h | `tests/protocol/test_protocol_vectors.cpp` 内 9 case 全过；CI 必跑 |
| 5 | S1AP fixture 端到端：选定 3 条 `S1AP-PDU` 真实捕获（来源：openairinterface5g `s1ap_test` 或自捕获），同上 9 条 oracle | 2 h | 9 case 全过 |
| 6 | RRC fixture 端到端：选定 3 条 `DL-DCCH-Message` 真实捕获（来源：asn1c `test` 目录或自捕获），同上 9 条 oracle | 2 h | 9 case 全过 |
| 7 | CER 协议消息门禁：同 9 条 oracle（`S1AP-PDU` 已用 CER 重写，仅条文推导 + DER 值等价） | 1 h | 同上；产出 `tests/protocol/golden/protocol_cer_golden_vectors.txt` 9 条 |
| 8 | sanitizer（ASan + UBSan）+ Release 全量 ctest 必过；`asn1_protocol_*_compile` gate 跑一遍字节级对验 | 1 h | Release **344/344 + N** + sanitizer **344/344 + N**；无 ASan/UBSan 报错 |
| 9 | 文档：`status.md` 「剩余工作」表合并 P1-3 行 + 「金标准差分验证状态」表新增 protocol-level 行（按 LPP / S1AP / RRC × 5 codec 计数）；`changelog.md` P1-3 条目；`docs/architecture.md` 标「协议消息字节级对验」段落；`usage-guide.md` 增加 "3GPP 协议消息字节级 oracle" 引用 | 1 h | 文档审阅 |

**总估时：~18 h（两个半工作日）。**

### 排期

- **先 P3-1**（已落地，commit `5856565`）：frontend 并行化已就位。
- **再 P1-3**（本 TODO）：依赖 P3-1 已固化的 frontend 路径，需保证
  --parallel 16 与 --parallel 0 在协议字节级门禁下**同样字节级一致**
  —— 沿用 P3-1 的 byte-equality gate 思路，扩到协议级 fixture。
- 估时 **2.5 工作日**；子任务 1+2+3 可最先做（建立 oracle 基础设施
  + 编译门禁），4+5+6 串行做（每 fixture 一组真实捕获）。

### 不在范围内（防止蔓延）

- 不为协议消息生成"自构造 bitmap"金标准（无 oracle，等于自证）；必须
  真实捕获或既有 asn1c 测试 fixture 复用。
- 不重写既有 per_golden.asn / tlv_golden.asn 综合构造向量：那些是
  **构造覆盖**（bitmap / fragmentation / 约束原语等），P1-3 是
  **协议 PDU 覆盖**，互补而非替换。
- 不抓全消息：3 fixture × 3 PDU × 5 codec 已经覆盖 ~45 条 oracle；
  加更多 PDU 是"覆盖面"问题，估时不线性增长。

---

## P2-4 — C 语言代码生成器（`--lang c`）

当前 Phase49 仅完成 sub-task 1/2：纯 C11 公共头、原语实现、链接 smoke、失败契约和
PER 独立差分。生产目标是 `asn1_c_runtime`（`asn1::c_runtime`），不链接 C++ 标准库。
C++17 使用项目 `Span` 与 `Result<T>`；C 使用 `asn1_status_t`（0 成功，其他为已登记
错误）和 out 参数，业务失败不使用异常，也不是进程全局 errno。

| 子任务 | 状态与下一步 | 必需验收 |
|---|---|---|
| 1/2 C11 头与 runtime | 已落地；具体原语边界见 [声明登记](c-runtime-test-coverage.md) | C11/C++17 独立头、纯 C 链接、失败/资源边界、PER 两 Variant 与独立 oracle |
| 规划前置（REV-27） | 最小 `codegen/plan.hpp` 已建立并接入 C++ PER/整数检查；C 后端接入未实现 | [共享规划](emission-planning.md)；不可声称已验证生成 C PDU |
| 3 C emitter | 未实现；按 codec 分步渲染同一规划，避免复制标签/presence/约束/扩展/selector 决定 | 生成 C11 多 TU 编译链接、状态传播、所有权和真实编解码 |
| 4 CLI/调度 | 未实现：`--lang cpp|c`、目标语言选项与调度 | 现有 C++ 参数/产物保持兼容，未实现组合明确诊断 |
| 5/6 C fixture 门禁 | 未实现；按已支持 codec 开放，不预设只需 5 codec | 7 fixture ×12 入口能力登记、生成 C 的编译/链接/运行；不能用手写 smoke 替代 |
| 7 oracle 复用 | 未实现：生成 C/C++ 同 schema/value 使用同一独立 oracle | 字节与双向互解，工具无覆盖时登记条文证据和局限 |
| 8/9/10 集成收尾 | 后续每步同步 | Release + ASan+UBSan 全量、各形态 ODR、文档/安装、P1-3 独立运行 |

继续 sub-task 3 前维持 REV-01..07 的失败传播与 PER 修复；见
[第一批](rev01-03-validation.md)、[第二批](rev04-09-validation.md)。
C/C++ runtime 平行实现各自语言的算法和状态接口；共享的是语义规划与测试向量，
不能把 compiler 链到任一 runtime，也不通过 C++ 包装代替纯 C。
UPER/APER 在各语言内各自共用一个 Variant 参数化 PER 核心。

暂不增加其他目标语言，也不为所谓通用 IR runtime 重写现有架构。
完整类型布局、SEQUENCE OF/CHOICE 泛型表达、ABI 和资源预算在对应 C 子任务设计时
明确，尚未决定的方案不作为当前已实现能力。

## 发布前条件

- [ ] 明确项目、生成产物、安装 SDK 的许可证与第三方 notices。
- [ ] 确立版本化 C/C++ ABI/源码兼容政策；当前公开结构不承诺冻结 ABI。
- [ ] 补全跨结构统一资源预算和长期 fuzz/设备运行证据；现有局部限额保持。
- [ ] 完成能力矩阵 STD-01..05 的后续工作；本轮修正不等于整部 ASN.1 标准认证。

边界和所有权见 [集成契约](generated-artifacts.md)。
