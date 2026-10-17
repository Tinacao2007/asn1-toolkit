# 待办清单（P1-3 / P2-4 与相关项）

本文档登记 P2-2 之后不再继续的剩余项，并按优先级、依赖、预估工作量列出
落地步骤。每条给出最小可行子任务，便于逐步合入。状态变更走
[status.md](status.md)，变更日志走 [changelog.md](changelog.md)。

## P2-3 — 增量代码生成（内容哈希缓存）

### 目标

`asn1cxx` 当前每次 `--emit-dir` 都全量生成 + 覆盖。C/C++ 工程在标准接入流程
里需要把 asn1cxx 作为构建依赖（CMake `add_custom_command(OUTPUT ... DEPENDS
asn1cxx <asn>)`），全量生成在大 fixture 上是数秒到分钟量级（per_golden 0.5 s、
RRC 8.6.0 10 s、S1AP 14.4.0 30 s+），CI 反复构建和 IDE 内保存触发都吃这个
成本。

`--emit-split` 已经把粒度降到 ASN.1 模块，再加一个**内容哈希缓存**：源文
件 hash 不变就跳过该单元的 emit + write；源文件 hash 变了只重写其对应单
元。

### 设计

- **缓存键**：`sha256(module_name || source_path_mtime || source_sha256)`
  三元组。`source_path_mtime` 仅用于触发重算 `source_sha256`（直接读文件
  hash 比解析 .asn 快得多，避免把 lexer/parser 拖进缓存键计算）。
- **缓存形态**：`.asn1cxx-cache/` 放在 `--emit-dir` 旁边，
  `<module-name>.cache` 文件存 `key: emit_output_sha256`。emit 后把输出
  文件做一次 hash 写入缓存条目；下一次命中时直接复用旧文件，并校验实际
  写出的 hash 与缓存一致（防止手动改产物后被错认命中）。
- **粒度**：
  - flat 模式：缓存键 = 单个 key（全量；命中即跳过整 emit）。
  - split 模式：每个 ASN.1 模块一个 key（与 `--emit-split` 单元粒度对
    齐）。依赖模块 hash 变更只重算当前单元。
- **失效链**：split 模式下单元 A 依赖 B 的类型 → B 失效时 A 也失效。
  用拓扑序逐单元判定（A 缓存键附加所有依赖 B 的 hash），保证正确性。
- **缓存关闭**：`--no-cache` 强制全量（CI 上做金标对验时不希望走缓存）；
  默认开。
- **诊断**：`Diagnostics` 加一条 `note`：「emit cache: 11/12 modules
  hit (0.04s) / 1 miss (1.2s)」（按 `note` 走，不进错误计数）。

### 子任务

| # | 内容 | 估时 | 验证 |
|---|------|------|------|
| 1 | 在 `EmitOptions` 加 `emit_cache: bool = true`；`asn1cxx` 加 `--emit-cache` / `--no-emit-cache`；`--help` 文档化 | 0.5 h | CLI 用例：flag-as-is 下命中缓存第二次跑时间归零；`--no-emit-cache` 不命中 |
| 2 | `tools/asn1cxx/main.cpp` 增加 `ContentCache` 工具类（读 `.asn1cxx-cache/*.cache` 文本、按 `(module, key) -> output_sha256` 查询、写回） | 1 h | 单元测试 `asn1_cache_tests`：写盘/读盘/命中/未命中/损坏恢复 |
| 3 | `CppGenerator::emit` 入口改为先调 cache：命中且 hash 一致则跳过 emit + write；未命中则走原路径并写缓存 | 2 h | 重复调用同 fixture 二次运行时间记录（flat + split）；改一字节后正确失效 |
| 4 | `CppGenerator::emit_split` 改为按单元走 cache：解析阶段产 module → key 依赖图（已有 `collect_module_groups`），拓扑序决定每个单元 emit 与否 | 2 h | LPP 单模块 split + S1AP 三单元 split 重复跑测时间；故意改一个 .asn 行仅影响一个单元 |
| 5 | 加 `asn1_cache_tests`：金标准用例（Person.asn 二次运行时间记录 + 改字段后失效链）；sanitizer build 也跑 | 1 h | sanitizer 0 失败；ctest 多一项 |
| 6 | 文档：`status.md` 「构建与测试状态」补缓存命中率；`changelog.md` 增条目；`development.md` 「验证门禁纪律」补"缓存命中不能绕过产物哈希校验" | 0.5 h | 文档审阅 |

**总估时：~7 h（一个工作日）。**

### 不在范围内（防止蔓延）

- 不做产物 SHA256 之外的校验（不签名）。
- 不做多进程锁：同一目录被并发 asn1cxx 调用由调用方负责；通常一个工程
  一个 asn1cxx 实例。
- 不做跨工程共享缓存：每个 `--emit-dir` 自带 `.asn1cxx-cache/`。

---

## P3-1 — 前端并行分析

### 目标

`Analyzer::analyze` 当前串行跑四趟（`declare_pass` → `resolve_pass` →
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

### 目标

当前 `asn1cxx` 只输出 C++ 代码（`namespace asn1::per` + `std::optional`
+ `std::span` + 自研 `BigInt` 等）。P2-4 引入**第二目标语言**：

- `asn1cxx --lang c` 输出**纯 C 兼容**代码（`asn1/codec.h` runtime
  + `Foo.h` / `Foo.c` 类型 stub）。
- 生成的 C 代码可被任意 C 项目（无 C++ 工具链）直接 `#include` + 编译。
- 字节输出与 `--lang cpp` 逐字节一致（P1-3 oracle 同样适用）。

### 为什么放在 P2 项目

P2 = **emit 系统强化**（P2-1 bit_io、P2-2 split emit、P2-3 增量缓存）。
新增"目标语言维度"是 emit 输出的进一步强化，与 P2-3 同级（同样改
emit 端、动 codec 输出形态、不动 runtime 语义）。归 P2-4。

### 关键挑战

1. **runtime 子集抽取**：`asn1/runtime/*` 用了 `std::optional`（Result
   错误码载体）、`std::span`（解码缓冲）、自研 `BigInt`（INTEGER 大数）。
   C 兼容子集 `asn1/c_runtime/*` 需要：
   - `Result<T>` → 整数错误码 + out 参数（`-1` / `0` / `1`）；
   - `std::span` → `const uint8_t*` + `size_t`；
   - `BigInt` → 同名结构体（C 风格）；
   - 命名空间 `asn1::per` → 前缀 `asn1_per_`。
   所有现有 C++ runtime **保留不动**，C runtime 是平行的纯 C 实现
   （`extern "C"` 包装不可行因为 C++ runtime 内部用了 C++ 习惯）。
2. **类型擦除 / 泛型替代**：C 无模板 → `SEQUENCE OF` / `SET OF` / `CHOICE`
   需要走 `void*` + size 或 per-element 函数指针表。最务实是
   **per-codec 单一 SEQUENCE OF 实现 + void\***（参考 libasn1 / asn1c）。
3. **错误码返回**：C++ 用 `Result<T>` + 抛出或 carry；C 用 errno + out
   参数。
4. **无 RTTI**：CHOICE 的 alternative 派发用 union + 显式 tag。
5. **零依赖**：C runtime 不许引 `<optional>`/`<span>`/`<vector>`，纯
   `<stdint.h>`/`<stddef.h>`/`<string.h>`。

### 设计

- **`src/codegen/emit_c/` 子目录**：模仿 `src/codegen/emit_per.cpp` 等
  按 codec 拆 C 风格 emit（`emit_c_per.cpp`、`emit_c_tlv.cpp`、
  `emit_c_jer.cpp` 等）。
- **`include/asn1/c_runtime/` 平行的纯 C 头** + `src/c_runtime/` 实现
  文件。**不允许** `#include <asn1/runtime/*>`（避免 C++ 习惯污染）。
- **CLI**：`--lang cpp|c`（默认 `cpp`，向后兼容零变更）。
- **CMake**：新增 `asn1c_c_runtime` 静态库（`asn1_c_per.o` 等）与
  `asn1_codegen_c` emitter 库；`asn1cxx` target 沿用，新增
  `--lang c` 路径调用 C emitter。
- **可重现性**：C 产物的字节序列必须与 C++ 产物**逐字节一致**
  （同 per_golden oracle）。P1-3 oracle 既覆盖 C++，扩到 `--lang c`
  后同一份 oracle 必须两种语言产出对得上。
- **测试**：复用 `tests/per/golden/`、`tests/tlv/golden/` 既有 oracle
  （56 + 16 条）。新增 `tests/c_lang/`：
  - `c_emit_test.cpp` 用 gtest + fork `cc` 编译生成的 C 代码 + 跑
    encode/decode round-trip + 字节对 oracle；
  - fixture compile gate：`asn1_c_lpp_up_compile` 等（N 个 fixture ×
    5 codec）；
  - sanitizer / ctest 必过。

### 子任务

| # | 内容 | 估时 | 验证 |
|---|------|------|------|
| 1 | 设计 `include/asn1/c_runtime/` 接口：从 `runtime/*` 头提取 C 兼容 API 清单（Result → errno；span → ptr+size；BigInt → C struct；命名空间 → 前缀）；`asn1/c_runtime/per.h` + `tlv.h` + `jer.h` 等骨架 | 6 h | 头文件接口 review 通过；编译 `gcc -std=c11 -c -fsyntax-only` 零错误 |
| 2 | `src/c_runtime/` 纯 C 实现（剥离 std::optional / span / 自研 BigInt 转 C struct）：`asn1_per_bit_reader_t`、`asn1_per_bit_writer_t`、`asn1_ber_tlv_*` 等 | 10 h | 5 fixture × 5 codec 既有 per_golden + tlv_golden oracle 在 C runtime 下 byte-equal；ctest 新增 N 项 |
| 3 | `src/codegen/emit_c/` emitter（仿 `emit_per.cpp` 拆分）：每 codec 一个 emit_c_*.cpp，输出 C 函数（C linkage，不带 `extern "C++"`） | 8 h | emit 输出 `gcc -std=c11 -Wall -Wextra -c -fsyntax-only` 零错误 |
| 4 | `asn1cxx --lang c` CLI + `EmitOptions::lang` 字段；`emit_c_toplevel.cpp` 调度 | 2 h | `--lang c` 在 LPP / S1AP / RRC 三 fixture × 5 codec 跑通；无 C++ 串味 |
| 5 | `tests/c_lang/c_emit_test.cpp`：fork `cc` 编译生成的 C 代码 + 跑 encode/decode round-trip + 比对 `tests/per/golden/` `tests/tlv/golden/` 既有 oracle；LPP / S1AP / RRC × 5 codec 字节级对验 | 6 h | Release **344/344 + N** + sanitizer **344/344 + N**；零 ASan/UBSan |
| 6 | fixture compile gate：`asn1_c_lpp_{uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer}_compile` 12 个 + 同 split 模式 — N 个新 gate | 4 h | ctest 全过 |
| 7 | P1-3 oracle 复用：`-lang c` 与 `-lang cpp` 在同一 oracle 下 byte-equal（per_golden + tlv_golden + P1-3 协议级 oracle）；合并进 P1-3 验证清单 | 2 h | oracle byte-equal 通过；避免两套 oracle 分叉 |
| 8 | sanitizer（ASan + UBSan）+ Release 全量 ctest 必过；新增 C runtime 走 `-fsanitize=address,undefined` 仍零报错 | 2 h | Release **344/344 + N** + sanitizer **344/344 + N** |
| 9 | 文档：`status.md` 「剩余工作」表新增 P2-4 行（指向 todo.md）；`changelog.md` P2-4 条目 + Phase 里程碑表新增 Phase 49；`docs/architecture.md` 新增「目标语言维度」段落（runtime C/C++ 平行设计）；`usage-guide.md` 加 C 集成示例 | 2 h | 文档审阅 |
| 10 | `tests/c_lang/` 与 `tests/protocol/`（P1-3）独立 fixture 编译互不依赖；CMake `add_subdirectory(tests)` 内分两套 fixture compile gate | 1 h | ctest 注册无冲突 |

**总估时：~43 h（5-6 个工作日）。** 估时高的根因 = C runtime 子集抽取 +
emit_c 拆分是大头；其余项与 P1-3 模式相似。

### 排期

- **先 P1-3**（commit `83fd175` 已规划）：协议级 oracle 字节对验
  就位后，C 语言产出可直接用同一 oracle，无需新建 oracle。
- **再 P2-4**（本 TODO）：C runtime 是新代码基线，**与 P1-3 协议消息
  oracle 互补**——P2-4 让 `--lang c` 走 P1-3 同一 oracle，避免双 oracle
  分叉。Phase 49。
- 估时 **5-6 工作日**；建议拆两个 PR：
  - PR1（子任务 1+2 + 头/实现骨架）：C runtime 子集就位，可独立跑
    现有 per_golden oracle（C 手写 driver，~10 测试用例）；
  - PR2（子任务 3+4+5+6+7）：emit_c + CLI + 自动 fixture 编译门禁 +
    P1-3 oracle 复用。

### 不在范围内（防止蔓延）

- 不输出其它语言（Rust / Go / Python）：每个语言一个 runtime + emitter，
  估时翻倍；当前只 C 一个新目标。
- 不抽 `asn1c_c_runtime` 之外的"通用 IR runtime"——所有 codec 各自抽
  C 子集；走 C 习惯的前缀 + errno，避免大一统抽象。
- 不支持 C++/C 互操作（C++ emit 产物调用 C runtime 或反向）——两套
  runtime 各自完整，不交叉。
- 不做 C99 之前兼容（C11 起是合理基线，与 P1-3 oracle 时窗匹配）。