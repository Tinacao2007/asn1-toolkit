# 项目状态

当前状态的唯一权威。状态截至 **2026-10-16**。历史变更见
[changelog.md](changelog.md)。已排期 / 未排期待办见
[todo.md](todo.md)。

## 当前阶段

**Phase 47 — 前端并行分析（`--parallel <N>`，`declare_pass` + `resolve_pass`
按模块并行，diagnostics 字节级可重现）。**

- `Analyzer::analyze(modules, parallelism = 0)` 新增 `parallelism` 参数；
  `0`（默认）= 串行基线（保持现状，CI 金标准对验强制），`>0` = worker 数。
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
- CLI：`asn1cxx --parallel <N>`（N ∈ 0..1024，0 = 串行 = 默认；>0 启用
  worker 线程池）；`--help` 已文档化。
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
| Release 构建 ctest | **344/344 通过**，0 失败（含 P3-1 前端并行 10 项单元测试 + 1 项 CLI 字节相等门禁；P2-3 内容哈希缓存 8 项单元测试 + 5 项 CLI 冒烟） |
| Sanitizer 构建 ctest（ASan+UBSan，Debug） | **344/344 通过**，0 失败，零 ASan/UBSan 报错 |
| `-Wall -Wextra` | 无新增警告 |

## 编译门禁状态

| 门禁 | 结果 |
|------|------|
| 全 codec 产物门禁 | 7 fixture（tlv_oer_golden / tlv_golden / cer_golden / RRC 8.6.0 / LPP 14.3.0 / S1AP 14.4.0 / rrc_slice）× 12 codec（uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer）`g++ -std=c++17 -fsyntax-only` 全部零错误 |
| 单 codec × 全家族门禁（新增） | LPP × 12 codec 各跑一次独立 emit + 完整编译（`asn1_lpp_single_{uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer}_compile`）：每族单独选择时产物的 runtime header 依赖自包含。该门禁捕获单 codec 场景的 include 缺口（组合门禁会把其他分支的 header 链入而掩盖） |
| 分文件 emit 门禁（UPER） | `asn1_split_lpp_split`（LPP 单模块路径）、`asn1_split_s1ap_split`（S1AP 三 emit 单元，含合并的 IEs+Containers+PDU-Contents 递归环）静态库编译通过 |
| 分文件 emit 手工矩阵 | LPP uper/aper/coer/ber/der/oer/jer 零错误；S1AP uper/ber/der/oer/jer 零错误；RRC uper 零错误；xer/exer/cxer 单 codec 零错误（XML 缺口已修复，见上）；flat 产物与改动前逐字节一致 |
| 三大 3GPP fixture emit 产物编译 | RRC/LPP/S1AP 全量 emit 产物作为 CMake 静态库编译通过（编译的是真实定义，非 stub） |

## 金标准差分验证状态

| 向量套 | Oracle | 状态 |
|--------|--------|------|
| PER/APER（`tests/per/golden/`，54 向量） | asn1c | 字节级对验通过 |
| BER/DER/OER（`tests/tlv/golden/`，16 用例 × 3 codec） | asn1tools + asn1c（DER 仲裁） | 字节级对验通过 |
| CER（`cer_golden`，10 用例） | 无外部 oracle；X.690 条文推导 + DER 值等价交叉验证 | 通过 |
| 运行时优化回归 | 三套金标准向量零回归（bit_io 批量位写前后输出逐比特一致） | 通过 |

## 剩余工作

| 项目 | 阻塞原因 / 说明 | 建议后续 |
|------|----------------|----------|
| XML 家族单 codec include 缺口（flat + split 均中招，P2-2 暴露） | ✅ 已修复：XML 分支 `emit_includes` 增补 `#include <asn1/runtime/byte_io.hpp>`（`make_error` 所在头）；补单 codec × 全家族发射矩阵的回归（每族单独选择跑一次编译门禁，LPP × 12 codec 全部零错误） | — |
| 完整模块编译 CI / 3GPP 编解码对等验证 + PER 位级正确性外部对验（协议级） | ✅ 已规划 P1-3（详见 [todo.md](todo.md#p1-3--3gpp-协议消息级金标准向量对验)）：抓取 / 生成 3GPP 真实协议消息字节 → 与 asn1-toolkit 编码字节逐位对验；3 fixture（LPP / S1AP / RRC）× 5 codec（uper,aper,ber,der,oer）= ~45 条 oracle | — |
| 许可证文件 | README 标注 TBD（开放源码，发布时添加）；源码头已标注 PROCODEC 版权 2026–2031 | 发布前补 LICENSE |
| 增量代码生成（P2-3，已落地；详见 [todo.md](todo.md#p2-3--增量代码生成内容哈希缓存)） | ✅ P2-3 自身验证：`--emit-cache`/`--no-emit-cache` + `.asn1cxx-cache/` 内容哈希 + split 模式按单元粒度 + CLI `--help` 文档化 + 8 项单元测试 + 5 项 CLI 冒烟；P2-3 落地当时 ctest **333/333**（Release + sanitizer）。P3-1 累计 → **344/344** | — |
| 前端并行分析（P3-1，已落地；详见 [todo.md](todo.md#p3-1--前端并行分析)） | ✅ 已完成 `declare_pass` + `resolve_pass` 并行；`lower_pass` 保留串行是有意决定（per-module arena + `Referenced.resolved` 重映射 deferred，理由见 todo.md P3-1 「实际落地」段）；`asn1cxx --parallel <N>` + `Analyzer::analyze(modules, N)` + `Diagnostics::merge` + `SymbolTable::merge`；CLI/单元/diagnostics 字节级一致性 + Release/sanitizer 344/344 | — |
| C 语言代码生成器（P2-4，已规划；详见 [todo.md](todo.md#p2-4--c-语言代码生成器-lang-c)） | ⏳ 已规划，未排期：`asn1cxx --lang c` 输出纯 C 兼容代码（`asn1/c_runtime/*` 平行 runtime + `src/codegen/emit_c/*`）；目标字节输出与 `--lang cpp` 逐字节一致（复用 P1-3 oracle）；估时 ~43 h / 5-6 工作日，Phase 49 | — |

## 已知限制

1. **emit 成功 ≠ 可编译 / 正确** —— 全部门禁定义见
   [development.md](development.md#验证门禁纪律)；
2. **组合门禁掩盖单 codec 缺口** —— 全 codec 位掩码单次发射会把其他分支的
   include 带进产物；单 codec 场景必须单独验证（XML 家族缺口即由此暴露）；
3. **电信 fixture 测试不等于 3GPP 完整对等** —— 当前覆盖 parse + analyze +
   emit 编译门禁 + 字节级金标准差分，协议级逐消息 PDU 对验在路线图（见上表）；
4. **CER 无外部 oracle** —— asn1c v0.9.29 与 asn1tools 均无 CER 实现，
   金标准由 X.690 条文推导 + DER 值等价交叉验证；
5. **许可证未定** —— 源码头已标 PROCODEC 版权，正式许可证文件发布时补齐。
