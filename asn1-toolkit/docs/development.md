# 开发指南

面向贡献者的工程手册：代码规范、测试体系、验证门禁纪律、金标准向量、文档
同步约定。架构原理见 [architecture.md](architecture.md)，当前状态见
[status.md](status.md)。

## 开发环境

与 [usage-guide.md](usage-guide.md#环境要求) 相同：CMake 3.16+、C++17
（GCC/Clang）、配置阶段一次网络（或离线 FETCHCONTENT 指向本地 GoogleTest）。
开发期建议同时维护 Release 与 sanitizer 两个构建目录（`build/` 与
`build-san/`），两者都是门禁。

## 代码规范

### 分层规则（硬性）

- 编译器链（`include/asn1/{ast,frontend,semantic,ir,constraints,codegen}`、
  `src/` 对应目录）不链接、不 include 任何运行时编解码库；
- 运行时（`include/asn1/runtime/`、`src/runtime/`）不 include AST / IR /
  符号表头；
- 编解码算法只在 `src/runtime/<family>/` 存在一份；发射器（`src/codegen/`）
  只产出调用原语的编排。

### 语言与依赖约束

- C++17 only：`std::optional` / `std::variant` / `std::string_view` /
  `std::filesystem`；不用 `std::expected`、`std::span`（用自备 `Result<T>` /
  `Span<T>`）；
- 无 Boost、无 multiprecision（`asn1::BigInteger` 自研）；
- 解码与约束失败返回 `Result`，不抛异常（`std::bad_alloc` 可传播）；
- 诊断一律带 `file:line:column`；编解码错误带 bit/byte 偏移；禁止无信息失败。

### 文件头与注释

- 所有 `.cpp` / `.hpp` 由 `scripts/add_file_headers.py` 维护标准 PROCODEC
  版权头（版本 0.1、日期、作者），含每文件与 ITU-T 编码规则（X.680/X.690/
  X.691/X.696/X.693/X.697）的映射说明；
- 公开接口使用 tabular 风格函数注释（`scripts/add_tabular_function_comments.py`
  维护）：Function / Description / Parameters / Returns 分块。

## 测试体系

GoogleTest 按领域组织，纯 C 门禁使用独立驱动，全部注册进 CTest。
当前数量与双构建结果以 [status.md](status.md) 为准。

C11 的 `Asn1CRuntimeSmoke` 验证原语链接运行；
`Asn1CRuntimeFailureContracts` 单独编译 C 源码并注入分配失败，验证输出、
生命周期、资源上限及恢复路径。测试 hook 不进入生产库，契约见
[c-runtime-contracts.md](c-runtime-contracts.md)。

`Asn1CPerCorrectness` / `Asn1CPerLargeValues` 使用独立冻结 oracle 向量、位模型和
条文边界测试；大值目标不修改生产资源默认值。声明覆盖见
[c-runtime-test-coverage.md](c-runtime-test-coverage.md)。开发时可运行
`scripts/validate_c_per_oracle.py` 重新独立差分；`--write-vectors` 仅在审查后
更新，不得使用本实现输出代替 oracle 金标准。

### 编译器与基础

| 可执行 | 覆盖 |
|--------|------|
| `asn1_common_tests` | Result / Span |
| `asn1_lexer_tests` | lexer / token |
| `asn1_parser_tests` | parser（含 parser_ioc） |
| `asn1_semantic_tests` | 符号表、分析器 |
| `asn1_constraints_tests` | 约束归一化 |
| `asn1_bigint_tests` | BigInteger |

### 编码规则单元测试

| 可执行 | 覆盖 |
|--------|------|
| `asn1_ber_tests` / `asn1_der_tests` | BER / DER |
| `asn1_cer_roundtrip_tests` | CER（X.690，字节级金标准，10 用例） |
| `asn1_per_tests` | PER 原语 |
| `asn1_per_golden_tests` | PER/APER 金标准差分（asn1c oracle，54 向量） |
| `asn1_uper_tests` / `asn1_aper_tests` | UPER / APER |
| `asn1_oer_tests` / `asn1_coer_tests` | OER / COER |
| `asn1_xer_tests` / `asn1_cxer_tests` / `asn1_exer_tests` | XER / CXER / E-XER |
| `asn1_jer_tests` / `asn1_jeri_tests` | JER / JERI |
| `asn1_tlv_golden_tests` | BER/DER/OER 金标准差分（asn1tools + asn1c oracle，16 用例 × 3 codec） |

### 集成与端到端

| 可执行 | 覆盖 |
|--------|------|
| `asn1_codegen_tests` | emit 产物文本断言（生成形态回归） |
| `asn1_telecom_tests` | 3GPP fixture 解析/分析/emit |
| `asn1_codegen_roundtrip_tests` | Person 跨 codec 编译往返 |
| `asn1_telecom_roundtrip_tests` | RRC slice UPER/OER emit + 已知 hex 往返 |
| `asn1_tlv_oer_golden_tests` | 混合标签/DEFAULT/SET/嵌套 CHOICE 字节级往返（8 用例） |
| `asn1_ioc_jer_roundtrip_tests` | 对象类开放类型 JER 往返 |
| `asn1_tc_ie_roundtrip_tests` | 表约束类型化 IE 往返（governor id 派发） |
| `asn1_cli_tests` | CLI 子进程行为（退出码/错误处理/emit 产物/codec 组合） |
| `asn1_cross_codec_tests` | 同一语义值跨 codec 往返保真 |
| `asn1_codec_benchmark_tests` | 吞吐 micro-benchmark（含 `BitIoBenchmark`） |

### 编译门禁目标

| 目标 | 门禁内容 |
|------|----------|
| `asn1_<fixture>_compile` | fixture（RRC/LPP/S1AP/tlv_oer_golden/tlv_golden/cer_golden/rrc_slice）emit 产物编译为静态库 |
| `asn1_split_lpp_split` / `asn1_split_s1ap_split` | `--emit-split` 分文件产物编译门（UPER） |

## 验证门禁纪律

本项目对"完成"的定义是四条门禁全部通过，缺一不可：

### 1. 双构建 ctest

Release 与 ASan/UBSan（`-DASN1_ENABLE_SANITIZERS=ON`）各跑一遍全量 ctest。
只跑 Release 不算验证。

### 2. emit 成功 ≠ 可编译

`asn1cxx` exit 0 只代表"发射完成"，不代表产物可编译。每次改动发射器/分析器后：

```bash
./build/asn1cxx --emit-dir /tmp/genchk --codec uper --namespace ns \
    tests/fixtures/telecom/<fixture>.asn
g++ -std=c++17 -fsyntax-only /tmp/genchk/generated.cpp \
    -I/tmp/genchk -Iinclude 2>&1 | grep -c error
```

以**错误数**为进度指标；报错按消息形态（而非行号）分组收敛 —— 数百个报错
通常归并为 handful 个根因，修完根因重新测量。

### 3. 全 codec 组合编译门禁

共享辅助函数（`default_val_cpp` / `type_cpp` 等被全家族调用的 ~11 个成员）
的签名改动，单 codec 检查不够 —— CMake fixture 门只链 fixture 主 codec，
其余家族的产物不能仅由该门禁证明。改共享面前必须用全 codec 位掩码 emit 各大
fixture 并 `-fsyntax-only`：

```bash
./build/asn1cxx --emit-dir /tmp/genchk_all --codec \
    uper,aper,ber,der,cer,oer,coer,jer,jeri,xer,exer,cxer --namespace ns \
    tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn
g++ -std=c++17 -fsyntax-only /tmp/genchk_all/generated.cpp \
    -I/tmp/genchk_all -Iinclude
```

注意反向陷阱：组合门禁会**掩盖单 codec include 缺口**（其他分支带入缺失的
头）。单 codec 场景需单独验证（见 status.md 剩余工作：XML 家族缺口即此类）。

### 4. 金标准向量零回归

自往返一致**不能**证明位布局正确（两份实现可以都错得一致）。字节级正确性
只能靠外部 oracle 差分向量证明：

| 向量套 | Oracle | 覆盖 | 生成脚本 |
|--------|--------|------|----------|
| `tests/per/golden/per_golden_vectors.txt` | asn1c | UPER/APER，54 向量（约束原语/长度 determinant/分片/扩展 addition/版本括号组） | `scripts/gen_per_golden.py` |
| `tests/tlv/golden/tlv_golden_vectors.txt` | asn1tools（BER/OER 源）+ asn1c（DER 仲裁） | BER/DER/OER，16 用例 | `scripts/gen_tlv_golden.py` |
| `cer_golden`（fixture 内断言） | 无外部 CER oracle | CER，10 用例，X.690 条文推导 + DER 值等价交叉验证 | — |

oracle 分歧按 ITU 原版条文裁定，不按编译器声誉（如 asn1tools 不排序
SET/SET OF，X.690 11.6 要求排序，asn1c 的 `der_encode` 胜出并写回向量）。
运行时改动后**必须重新生成产物再 diff** —— 对着旧产物 diff 会得出"修复无效"
的错误结论。

### 5. 内容哈希缓存（增量 emit，P2-3）

`asn1cxx` 默认开启 `<dir>/.asn1cxx-cache/` 内容哈希缓存：源文件 sha256 + 发射
参数不变即跳过该单元的 emit + write。命中时 stderr 末尾输出一行 `note`
（`emit cache: N/M units hit (...)`），不进错误计数。**缓存命中不能绕过产物
哈希校验** —— 写入磁盘后立刻对每个输出文件做一次 sha256 并写入缓存条目，
下一次命中时会重新计算并比较，手动改产物后任何命中都自动失效。

门禁纪律：
- CI 上做金标对验（差分 oracle 向量）必须传 `--no-emit-cache`，确保
  emit 路径是基线；
- 跨工程不要共享 `.asn1cxx-cache/`：缓存键含绝对源文件路径，路径变了
  key 自然 miss，旧条目静默占用空间；
- 修改 emit 任何源/产物后必须重跑一遍带缓存与 `--no-emit-cache` 的 diff，
  确认产物字节级一致（P2-2 历史教训：flat 与 split 同一 fixture 跑出的
  产物必须按 codec diff 出来为空）。

## 新增编解码家族流程（概要）

完整步骤（运行时库 + CMake、`CodecKind` 位与 CLI 别名、`TlvKind`/命名空间
路由、emit 派发与条件 include、fixture + 金标准测试、无 oracle 验证策略）
见历史归档 [analysis/architecture-review-and-optimization.md](analysis/architecture-review-and-optimization.md)
与 CER 家族（Phase 43，2026-10-15 P1-2）的落地记录
[changelog.md](changelog.md#近期变更详录)。

要点：CLI 别名删除/重命名会破坏 `tests/CMakeLists.txt` 中按名调用的门禁，
先全仓 grep 别名字符串；每个家族文件都各自携带"同一类概念缺陷"的副本，
一个家族暴露的 bug 要 grep 其余家族再宣布修复。

## 文档同步纪律

文档与代码同一次变更内同步。当前文档结构与权威分工见
[docs/README.md](README.md)。同步规则：

1. **一份事实一处权威**：设计 → architecture.md；操作 → usage-guide.md；
   状态 → status.md；历史 → changelog.md；对比 → comparison.md；
   工程纪律 → development.md；README 只做速览与链接；
2. **同步当前数字，保留历史证据**：测试总数、产品入口数、fixture 数变化时，
   检索 README / status / comparison 等当前说明并统一口径；带日期的验收记录、
   changelog 历史条目和分析快照保留当时数字，不做全局替换；
3. **变更收尾动作**：changelog.md 追加条目（含验证口径），status.md 同步
   状态表，若涉及 CLI/产物形态同步 usage-guide.md；
4. **历史归档不追改**：`docs/analysis/` 是带日期的快照，只在新归档时新增，
   不修改既有快照内容。
5. **项目 skill 随修更新**：每批 REV 修正同时检查
   [asn1-remediation/SKILL.md](skills/asn1-remediation/SKILL.md)，同步已验证的
   操作约束、回归选择和文档入口；任务状态及测试数量仍留在对应权威文档中。

## 标准语义与独立验证

标准语义修正同时维护 [能力矩阵](standard-capabilities.md) 和
[数值/字符 profile](real-and-string-profile.md)。新增独立对验命令及有限 fuzz 范围见
[第五批验收](rev20-24-validation.md)：`StandardOracle.FrozenReplay`、
`StandardCapabilities.Probes` 和 `DecoderFuzz.SeedRegression` 在普通 CTest 执行，
外部工具只用于显式 live 对验/刷新，libFuzzer 由可选 `ASN1_BUILD_FUZZERS` 构建。

## 分支与提交

- 开发分支：`asn1-toolkit-phase-39-validation`（CI 在 `main`/`master` 及该
  分支的 push 与所有 PR 上运行 build-test + sanitize 双 job）；
- 提交信息使用简洁的英文祈使句（历史惯例，如 "Fix emitted-code defects
  found by all-codec fixture compile gates"）；
- 每次提交前完成文档同步纪律的第 3 条。


### 第四批新增可重复门禁

```sh
# 84 个单独选择的产物；不是把 per 当第 12 个规则
cmake --build build --target asn1_codec_matrix
# 新目录安装后，C / runtime-only / compiler-only / 宿主生成消费者真实运行
python3 scripts/validate_installed_package.py --build build --output build/package-check
```

`CodecEntry.*` 在默认测试中逐入口实际链接调用，并比较单入口与组合的三种产物
编码结果。7×12 门禁是独立目标，适合 CI 显式运行；它只证明语法，独立 oracle
能力仍按 [入口登记](codec-entries.md) 和后续 REV-20/24 核实。

生成器指纹失效的集成回归必须使用**独立且未在构建中的 compiler-only 目录**：

```sh
python3 scripts/validate_generator_revision.py --build build-compiler
```

它临时修改该构建的私有生成头并重建，验证相同 ASN.1 从 hit 变为 miss，最后
恢复原头并重建。它不修改仓库源码，不能和该目录的其他构建并发运行。


## 分层、头文件与产物门禁（REV-25..28）

- `python3 scripts/validate_layering.py --cmake cmake`：只检查实际 include 指令，
  emitter 打印的 runtime include 字符串是合法桥梁；脚本自测包含反向依赖负例。
- `cmake/Layering.cmake` 在配置时检查 compiler/runtime 传递链接；测试通过 helper
  间接越界也必须失败。源码检查覆盖当前直接 include 拼写，不是完整预处理器，
  引入宏 include 或新 include 根须同步扩展门禁，不能据此豁免分层铁律。
- `python3 scripts/validate_public_headers.py --cxx g++ --cc gcc`：每个 C++ 头独立
  C++17、每个 C 头独立 C11/C++17；GCC/Clang 的 CTest 默认注册，MSVC 使用其专用
  编译门禁，不向 MSVC 传 `-std`。源码分层已加入 CI。
- 产物变化运行三形态双 TU、split 模块环、12 入口线路相等和输出保护测试；
  `-fsyntax-only` 不代替链接调用。compiler-only/runtime-only 继续可独立构建。
- 后端语义修改先查 [规划边界](emission-planning.md)；目录生成失败保护、manifest
  与旧文件归属规则见 [产物契约](generated-artifacts.md)。全量门禁结果另存验收文档。
