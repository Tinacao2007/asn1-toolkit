# 项目状态

当前状态的唯一权威。状态截至 **2026-10-24**。历史变更见
[changelog.md](changelog.md)。已排期 / 未排期待办见
[todo.md](todo.md)。

## 当前阶段

**Phase 49: C runtime primitives and CGEN batch 1 design/dispatch complete; C emitter pending.**

- **2026-10-21，第六批**：REV-25/26/28 完成产物事务、ODR/命名/归属回归、自动分层/公共头门禁及文档收口。REV-27 完成最小共享规划和 C++ 首批接入；真实 C emitter 消费验收仍待 P2-4 sub-task 3+。GCC Release/ASan+UBSan 各 **442/442**，实际矩阵 **84/84**，MSVC 针对性 **37/37**；组件独立构建与安装消费者通过。见 [第六批验收](rev25-28-validation.md)、[产物契约](generated-artifacts.md)、[规划边界](emission-planning.md)。

- **2026-10-21，REV-20..24**：补标准能力与 REAL/字符/时间 profile；修复约束保真、常量/值参数解析、整数窄化及规范策略缺口。GCC Release/ASan+UBSan 各 **430/430**，实际矩阵 **84/84**，MSVC 针对性 **59/59**；外部对验、Clang sanitizer fuzz 与后续能力项见 [第五批验收](rev20-24-validation.md) 和 [能力矩阵](standard-capabilities.md)。

- **2026-10-21，REV-13..19**：修复缓存生成器指纹/输入顺序及有界并发；补 JERI 产品入口与控制段错误诊断，隔离 OER/XML 组合策略；落地离线、宿主/目标构建和安装包。GCC/Clang Release、ASan+UBSan 四轮各 **392/392**，实际入口矩阵 **84/84**；MSVC、最低 CMake 3.16.8 与 AArch64 链接证据见 [第四批验收](rev13-19-validation.md)。设备运行与完整标准语义审计仍未宣称完成。

- **2026-10-20，REV-10..12**：修复 E-XER 属性错误传播与三种生成形态回归；
  SourceFile / IR BigInt 改为 Result，数字/文件/线程资源失败规范化；补 Result
  nodiscard 与访问前置条件、线路偏移单位和字段路径。Release / ASan+UBSan
  各 **370/370**；公共 C++17 头 47/47 严格编译、历史语法矩阵 84/84。
  C++ API/布局变化及验证边界见 [第三批验收](rev10-12-validation.md) 和
  [C++ 错误契约](cpp-error-contracts.md)。当时 REV-13/14 与 JERI 入口待后续，现已在第四批落地。

- **2026-10-20，REV-04..09**：C PER 原语修复 MSB-first 尾位、有符号大整数、
  长度片段链与变长/扩展约束；统一 `per_core.c` 以 Variant 参数化，旧 ABI
  保留为 UPER 包装。默认 64KiB decoder 限制保持；大值门禁单独编译 128KiB
  源副本。Release 与 ASan+UBSan 均 **348/348**；故障检查 498，两个 PER
  门禁各 4473 检查，独立 asn1tools 双向差分 234 用例、184 冻结短向量。
  具体支持范围、oracle 排除项和旧 raw payload API 语义见
  [第二批验收记录](rev04-09-validation.md) 与
  [声明/测试登记](c-runtime-test-coverage.md)。这不代表完整 C emitter 已完成。

- **2026-10-20，REV-01..03**：C11 BitWriter 整次预留容量并传播 OOM/溢出；
  BigInt 临时解析成功后提交，传播分配失败与 limb 限制；新增 C 状态码 8/9
  （原 0..7 不变），核对并修正原语输出/所有权契约及失败清理。
  `Asn1CRuntimeFailureContracts` 含 424 个内部检查；Release 与 ASan+UBSan
  全量 CTest 均 **346/346** 通过。生命周期说明见
  [c-runtime-contracts.md](c-runtime-contracts.md)，命令与门禁证据见
  [REV-01..03 验收记录](rev01-03-validation.md)。该批不含 PER 线路修复；
  后续结果见上方 REV-04..09，不据此推定全部 C codec 或 C emitter 已完成。

- **P2-4 sub-task 1 + 2（已完成）**：`include/asn1/c_runtime/*` 头骨架
  + `src/c_runtime/*.c` 纯 C11 实现 + `asn1::c_runtime` 静态库 +
  `Asn1CRuntimeSmoke` 链接+运行门禁。当前声明实现和解码/负例范围以
  [测试登记](c-runtime-test-coverage.md) 为准；JER/XER reader init 只绑定输入，
  不等于完整解析器。命名约定 `asn1_<codec>_<primitive>`
  替代 C++ `namespace asn1::<codec>::<primitive>`（flat prefix，无嵌套）。
  Byte-equivalent 的设计为后续 sub-task 3+（`emit_c/*` + `--lang c`）做
  oracle 复用留下基础。
- **P2-4 sub-task 3+（未排期）**：`src/codegen/emit_c/*` emitter + CLI
  `--lang c` + `EmitOptions::lang` + 全 fixture 编译门禁 + P1-3 oracle
  复用。子任务清单见 [todo.md](todo.md#p2-4--c-语言代码生成器-lang-c)。

- `Analyzer::analyze(modules, parallelism = 0)` 新增 `parallelism` 参数；
  `0`（默认）= 串行基线（保持现状，CI 金标准对验强制），`1` 仍串行，`>1` 为并发上界。
- 并行范围：`declare_pass` 每模块独立声明符号 + 局部 `SymbolTable`，
  pass 结束后按 module index 串行合并；`resolve_pass` 每模块持完整符号表
  只读视图 + 局部 `Diagnostics`，worker 间不共享跨模块 diagnostics；
  `tag_pass` 仍串行。
- `lower_pass` **保持串行**：per-module `LowerCtx` 共享 `model_.arena`
  + 跨模块 `Referenced.resolved` 链接，把 arena 拆 worker + 串行重映射
  需要重建跨模块链接图，**作为后续工作 deferred**。P3-1 仅并行前两趟。
- API 扩展：`Diagnostics::merge(Diagnostics&&)`（caller 决定追加顺序，items
  按 merge-call 序排列，与串行 baseline 字节级一致）；`SymbolTable::merge(
  SymbolTable&&)`（按 module key 合入，worker 间 duplicate 检测由 declare
  自身保证）。`Analyzer::analyze` 形参 `parallelism` 不破坏现有调用（默认
  0 = 串行）。
- CLI：`asn1cxx --parallel <N>`（N ∈ 0..1024，0 = 串行 = 默认；>1 使用
  有界任务批次（1 保持串行））；`--help` 已文档化。
- 验证：`asn1_parallel_analyzer_tests`（10 项）：`Diagnostics::merge` 序列
  / 空合并 / `error_count` 聚合；`SymbolTable::merge` 合入 + duplicate
  检测；模块 fixture（tiny / multi-module import / undefined type / unknown
  field / duplicate symbol）下 `--parallel 0` 与 `--parallel 16` diagnostics
  **逐字节相同** + error_count 一致。`asn1_cli_tests` 新增
  `Cli.ParallelModeProducesByteIdenticalOutput`：LPP 14.3.0 fixture 上
  `--parallel 0` 与 `--parallel 16` 生成的 `generated.hpp/.cpp` 在
  namespace 替换后**逐字节相同**。Release + sanitizer（ASan+UBSan）全量
  ctest **344/344** 通过，零 ASan/UBSan 报错。
- 文档：`docs/todo.md` P3-1 条目增「`lower_pass` 保持串行」说明 + 「后续
  工作」段落；`docs/changelog.md` P3-1 落地条目；`status.md` 当前阶段 +
  构建与测试状态 + 剩余工作更新。

- `--emit-cache`（默认）/ `--no-emit-cache` 已落地：缓存目录
  `<emit-dir>/.asn1cxx-cache/`，每单元（flat: `default`；split: 模块 ns
  ident）一个 `.cache` 文件，存 `(sha256(content_key), {output_path: sha256})`；
  命中条件 = key 一致 **且** 每个输出文件的当前 sha256 与记录的 sha256
  一致（防止手动改产物后被错认命中）。
- 实现：`src/codegen/{sha256,emit_cache}.{hpp,cpp}` — 自包含 SHA-256
  （FIPS 180-4 全部 4 个测试向量通过，无 OpenSSL 依赖）；
  `CppGenerator::emit_to_dir(model, options, diag, dir)` 新增 flat 模式入口
  （带 dir + 缓存）；`CppGenerator::emit_split` 自动启用缓存；legacy
  `emit(stream, stream)` 与 `emit_header_string()` 行为不变。
- 诊断：每次 emit 末尾追加 `note` 级 `emit cache: N/M units hit (T hit),
  K missed (T miss)`，不进错误计数。
- 验证：`asn1_emit_cache_tests`（8 项）— SHA-256 FIPS 向量；flat 二次
  运行 hit；改字节强制 miss；`--no-emit-cache` 不命中；手动改产物触发
  miss；basename 变化触发 miss；split LPP 单单元 hit；split LPP
  `--no-emit-cache` 不命中。CLI smoke 4 项（默认二次运行 hit /
  `--no-emit-cache` 无 note / flag 顺序最后胜出 / 首运行状态行是 "wrote"
  而非 "reused (cached)"）。
- 算法 / 编译时优化：`ContentCache::compute_key` 接受外部传入的 per-source
  sha256 映射，Emitter 一次性算好后通过 `cache_source_hashes()` 复用，
  split 模式 N 个单元只对每个 .asn 源读 + SHA-256 一次；fixture compile
  gate（flat / single-codec / split）从 `-O3` 降为 `-O1`，仍 ABI 兼容，
  大 fixture 编译时间显著下降；CMake 全局加 `-pipe -fno-plt`（sanitizer
  构建不加以避免影响 sanitizer 栈回溯）。

## 构建与测试状态

| 门禁 | 结果 |
|------|------|
| Release 构建 ctest | GCC 13.3 **442/442**（2026-10-21，WSL）；完整 Clang 历史轮次见第四批记录 |
| Sanitizer 构建 ctest（ASan+UBSan，Debug） | GCC **442/442**；启用 leak detection / halt_on_error；第五批 Clang 为独立受限 fuzz 验证，本批未重跑 |
| `-Wall -Wextra` | 无新增警告 |

## 编译门禁状态

| 门禁 | 结果 |
|------|------|
| 全 codec 产物门禁 | 7 fixture（tlv_oer_golden / tlv_golden / cer_golden / RRC 8.6.0 / LPP 14.3.0 / S1AP 14.4.0 / rrc_slice）× 12 codec（uper,aper,ber,der,cer,oer,coer,jer,jeri,xer,exer,cxer）`g++ -std=c++17 -fsyntax-only` 全部零错误 |
| 单 codec × 全家族门禁（新增） | LPP × 12 codec 各跑一次独立 emit + 完整编译（`asn1_lpp_single_{uper,aper,ber,der,cer,oer,coer,jer,jeri,xer,exer,cxer}_compile`）：每族单独选择时产物的 runtime header 依赖自包含。该门禁捕获单 codec 场景的 include 缺口（组合门禁会把其他分支的 header 链入而掩盖） |
| 分文件 emit 门禁（UPER） | `asn1_split_lpp_split`（LPP 单模块路径）、`asn1_split_s1ap_split`（S1AP 三 emit 单元，含合并的 IEs+Containers+PDU-Contents 递归环）静态库编译通过 |
| 历史分文件 emit 手工矩阵 | LPP uper/aper/coer/ber/der/oer/jer 零错误；S1AP uper/ber/der/oer/jer 零错误；RRC uper 零错误；xer/exer/cxer 单 codec 零错误（XML 缺口已修复，见上）；flat 产物与改动前逐字节一致 |
| 三大 3GPP fixture emit 产物编译 | RRC/LPP/S1AP 全量 emit 产物作为 CMake 静态库编译通过（编译的是真实定义，非 stub） |

## 金标准差分验证状态

2026-10-20 复测说明：历史 7 fixture × 12 selector 语法矩阵 **84/84** 通过。
该列表中的 `per` 是 UPER|APER 组合别名，不能计为第十二个独立入口；JERI
独立生成与矩阵口径仍待 REV-15/16 修正。以上历史矩阵不等于 12 个独立入口
完整合规。既有 oracle 来源向量随本次双构建回归通过，未重新生成外部向量。

| 向量套 | Oracle | 状态 |
|--------|--------|------|
| PER/APER（`tests/per/golden/`，54 向量） | asn1c | 字节级对验通过 |
| BER/DER/OER（`tests/tlv/golden/`，16 用例 × 3 codec） | asn1tools + asn1c（DER 仲裁） | 字节级对验通过 |
| CER（`cer_golden`，10 用例） | 无外部 oracle；X.690 条文推导 + DER 值等价交叉验证 | 通过 |
| 运行时优化回归 | 三套金标准向量零回归（bit_io 批量位写前后输出逐比特一致） | 通过 |

## 已知限制

1. **emit 成功 ≠ 可编译 / 正确** —— 全部门禁定义见
   [development.md](development.md#验证门禁纪律)；
2. **组合门禁掩盖单 codec 缺口** —— 全 codec 位掩码单次发射会把其他分支的
   include 带进产物；历史 XML 家族 include 缺口曾由此暴露，当前保持 12 个实际入口逐项检查，不能仅跑组合；
3. **电信 fixture 测试不等于 3GPP 完整对等** —— 当前覆盖 parse + analyze +
   emit 编译门禁 + 字节级金标准差分，协议级逐消息 PDU 对验见
   [todo.md](todo.md#p1-3--3gpp-协议消息级金标准向量对验)；
4. **CER 无外部 oracle** —— asn1c v0.9.29 与 asn1tools 均无 CER 实现，
   金标准由 X.690 条文推导 + DER 值等价交叉验证；
5. **许可证未定** —— 源码头已标 PROCODEC 版权，正式许可证文件发布时补齐。

## CGEN batch 1 (2026-10-24)

CGEN-01..03 completes the C ABI design, twelve-entry runtime gap audit and
`--lang cpp|c` dispatch. C emission explicitly fails without publishing artifacts.
No C emitter or generated PDU is claimed; batches 2 onward and REV-27 real C
plan consumption remain open. [Evidence](cgen-batch1-validation.md).
