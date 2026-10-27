# 开发指南

面向贡献者的工程手册：代码规范、测试体系、验证门禁纪律、金标准向量、文档
同步约定。架构原理见 [architecture.md](architecture.md)，当前状态见
[status.md](status.md)。

## 阅读导航

[代码规范](#代码规范) · [测试体系](#测试体系) · [验证门禁](#验证门禁纪律) · [文档同步](#文档同步纪律) · [C11 声明覆盖](#c-runtime-tests) · [REV 历史任务](changelog.md#remediation-tasks) · [功能路线](changelog.md#roadmap) · [CGEN 历史任务](changelog.md#cgen-tasks)

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

- 仅使用 C++17：`std::optional` / `std::variant` / `std::string_view` /
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
[architecture.md](architecture.md#c-runtime-contracts)。

`Asn1CPerCorrectness` / `Asn1CPerLargeValues` 使用独立冻结外部对照工具向量、位模型和
条文边界测试；大值目标不修改生产资源默认值。声明覆盖见
[development.md](development.md#c-runtime-tests)。开发时可运行
`scripts/validate_c_per_oracle.py` 重新独立差分；`--write-vectors` 仅在审查后
更新，不得使用本实现输出代替外部对照工具金标准。

### 编译器与基础

| 可执行                      | 覆盖                   |
| ------------------------ | -------------------- |
| `asn1_common_tests`      | Result / Span        |
| `asn1_lexer_tests`       | lexer / token        |
| `asn1_parser_tests`      | parser（含 parser_ioc） |
| `asn1_semantic_tests`    | 符号表、分析器              |
| `asn1_constraints_tests` | 约束归一化                |
| `asn1_bigint_tests`      | BigInteger           |

### 编码规则单元测试

| 可执行                                                      | 覆盖                                                         |
| -------------------------------------------------------- | ---------------------------------------------------------- |
| `asn1_ber_tests` / `asn1_der_tests`                      | BER / DER                                                  |
| `asn1_cer_roundtrip_tests`                               | CER（X.690，字节级金标准，10 用例）                                    |
| `asn1_per_tests`                                         | PER 原语                                                     |
| `asn1_per_golden_tests`                                  | PER/APER 金标准差分（asn1c 外部对照工具，54 向量）                         |
| `asn1_uper_tests` / `asn1_aper_tests`                    | UPER / APER                                                |
| `asn1_oer_tests` / `asn1_coer_tests`                     | OER / COER                                                 |
| `asn1_xer_tests` / `asn1_cxer_tests` / `asn1_exer_tests` | XER / CXER / E-XER                                         |
| `asn1_jer_tests` / `asn1_jeri_tests`                     | JER / JERI                                                 |
| `asn1_tlv_golden_tests`                                  | BER/DER/OER 金标准差分（asn1tools + asn1c 外部对照工具，16 用例 × 3 编码规则） |

### 集成与端到端

| 可执行                            | 覆盖                                     |
| ------------------------------ | -------------------------------------- |
| `asn1_codegen_tests`           | emit 产物文本断言（生成形态回归）                    |
| `asn1_telecom_tests`           | 3GPP 测试样例解析/分析/emit                    |
| `asn1_codegen_roundtrip_tests` | Person 跨编码规则编译往返                       |
| `asn1_telecom_roundtrip_tests` | RRC slice UPER/OER emit + 已知 hex 往返    |
| `asn1_tlv_oer_golden_tests`    | 混合标签/DEFAULT/SET/嵌套 CHOICE 字节级往返（8 用例） |
| `asn1_ioc_jer_roundtrip_tests` | 对象类开放类型 JER 往返                         |
| `asn1_tc_ie_roundtrip_tests`   | 表约束类型化 IE 往返（控制字段 id 派发）               |
| `asn1_cli_tests`               | CLI 子进程行为（退出码/错误处理/emit 产物/codec 组合）   |
| `asn1_cross_codec_tests`       | 同一语义值跨编码规则往返保真                         |
| `asn1_codec_benchmark_tests`   | 吞吐 micro-benchmark（含 `BitIoBenchmark`） |

### 编译门禁目标

| 目标                                               | 门禁内容                                                                           |
| ------------------------------------------------ | ------------------------------------------------------------------------------ |
| `asn1_<fixture>_compile`                         | 测试样例（RRC/LPP/S1AP/tlv_oer_golden/tlv_golden/cer_golden/rrc_slice）emit 产物编译为静态库 |
| `asn1_split_lpp_split` / `asn1_split_s1ap_split` | `--emit-split` 分文件产物编译门（UPER）                                                  |

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

### 3. 全编码规则组合编译门禁

共享辅助函数（`default_val_cpp` / `type_cpp` 等被全家族调用的 ~11 个成员）
的签名改动，单编码规则检查不够 —— CMake 测试样例门只链测试样例主编码规则，
其余家族的产物不能仅由该门禁证明。改共享面前必须用全编码规则位掩码 emit 各大
测试样例并 `-fsyntax-only`：

```bash
./build/asn1cxx --emit-dir /tmp/genchk_all --codec \
    uper,aper,ber,der,cer,oer,coer,jer,jeri,xer,exer,cxer --namespace ns \
    tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn
g++ -std=c++17 -fsyntax-only /tmp/genchk_all/generated.cpp \
    -I/tmp/genchk_all -Iinclude
```

注意反向陷阱：组合门禁会**掩盖单编码规则 include 缺口**（其他分支带入缺失的
头）。单编码规则场景需单独验证（见 status.md 剩余工作：XML 家族缺口即此类）。

### 4. 金标准向量零回归

自往返一致**不能**证明位布局正确（两份实现可以都错得一致）。字节级正确性
只能靠外部外部对照工具差分向量证明：

| 向量套                                       | Oracle                              | 覆盖                                                        | 生成脚本                        |
| ----------------------------------------- | ----------------------------------- | --------------------------------------------------------- | --------------------------- |
| `tests/per/golden/per_golden_vectors.txt` | asn1c                               | UPER/APER，54 向量（约束原语/长度 determinant/分片/扩展 addition/版本括号组） | `scripts/gen_per_golden.py` |
| `tests/tlv/golden/tlv_golden_vectors.txt` | asn1tools（BER/OER 源）+ asn1c（DER 仲裁） | BER/DER/OER，16 用例                                         | `scripts/gen_tlv_golden.py` |
| `cer_golden`（测试样例内断言）                     | 无外部 CER 外部对照工具                      | CER，10 用例，X.690 条文推导 + DER 值等价交叉验证                        | —                           |

外部对照工具分歧按 ITU 原版条文裁定，不按编译器声誉（如 asn1tools 不排序
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

- CI 上做金标对验（差分外部对照工具向量）必须传 `--no-emit-cache`，确保
  emit 路径是基线；
- 跨工程不要共享 `.asn1cxx-cache/`：缓存键含绝对源文件路径，路径变了
  键自然 miss，旧条目静默占用空间；
- 修改 emit 任何源/产物后必须重跑一遍带缓存与 `--no-emit-cache` 的 diff，
  确认产物字节级一致（P2-2 历史教训：flat 与 split 同一测试样例跑出的
  产物必须按编码规则 diff 出来为空）。

## 新增编解码家族流程（概要）

完整步骤（运行时库 + CMake、`CodecKind` 位与 CLI 别名、`TlvKind`/命名空间
路由、emit 派发与条件 include、测试样例 + 金标准测试、无外部对照工具验证策略）
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
2. **同步当前数字，保留历史证据**：测试总数、产品入口数、测试样例数变化时，
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

标准语义修正同时维护 [能力矩阵](status.md#standard-capabilities) 和
[数值/字符支持范围](status.md#real-and-string-profile)。新增独立对验命令及有限 fuzz 范围见
[第五批验收](changelog.md#remediation-validation--rev-20-24)：`StandardOracle.FrozenReplay`、
`StandardCapabilities.Probes` 和 `DecoderFuzz.SeedRegression` 在普通 CTest 执行，
外部工具只用于显式 live 对验/刷新，libFuzzer 由可选 `ASN1_BUILD_FUZZERS` 构建。

## 分支与提交

- 开发分支：`asn1-toolkit-phase-39-validation`（CI 在 `main`/`master` 及该
  分支的 push 与所有 PR 上运行 build-test + sanitize 双 job）；
- 提交信息使用简洁的英文祈使句（历史惯例，如 "Fix emitted-code defects
  found by all-codec 测试样例 compile gates"，即“修复全编码规则样例编译门禁发现的生成代码缺陷”）；
- 每次提交前完成文档同步纪律的第 3 条。

### 第四批新增可重复门禁

```sh
# 84 个单独选择的产物；不是把 per 当第 12 个规则
cmake --build build --target asn1_codec_matrix
# 新目录安装后，C / runtime-only / compiler-only / 宿主生成消费者真实运行
python3 scripts/validate_installed_package.py --build build --output build/package-check
```

`CodecEntry.*` 在默认测试中逐入口实际链接调用，并比较单入口与组合的三种产物
编码结果。7×12 门禁是独立目标，适合 CI 显式运行；它只证明语法，独立外部对照工具
能力仍按 [入口登记](architecture.md#codec-entries) 和后续 REV-20/24 核实。

生成器指纹失效的集成回归必须使用**独立且未在构建中的仅编译器目录**：

```sh
python3 scripts/validate_generator_revision.py --build build-compiler
```

它临时修改该构建的私有生成头并重建，验证相同 ASN.1 从 hit 变为 miss，最后
恢复原头并重建。它不修改仓库源码，不能和该目录的其他构建并发运行。

## 分层、头文件与产物门禁（REV-25..28）

- `python3 scripts/validate_layering.py --cmake cmake`：只检查实际 include 指令，
  emitter 打印的运行时 include 字符串是合法桥梁；脚本自测包含反向依赖负例。
- `cmake/Layering.cmake` 在配置时检查 compiler/runtime 传递链接；测试通过辅助函数
  间接越界也必须失败。源码检查覆盖当前直接 include 拼写，不是完整预处理器，
  引入宏 include 或新 include 根须同步扩展门禁，不能据此豁免分层铁律。
- `python3 scripts/validate_public_headers.py --cxx g++ --cc gcc`：每个 C++ 头独立
  C++17、每个 C 头独立 C11/C++17；GCC/Clang 的 CTest 默认注册，MSVC 使用其专用
  编译门禁，不向 MSVC 传 `-std`。源码分层已加入 CI。
- 产物变化运行三形态双 TU、split 模块环、12 入口线路相等和输出保护测试；
  `-fsyntax-only` 不代替链接调用。compiler-only/runtime-only 继续可独立构建。
- 后端语义修改先查 [规划边界](architecture.md#emission-planning)；目录生成失败保护、manifest
  与旧文件归属规则见 [产物契约](architecture.md#generated-artifacts)。全量门禁结果另存验收文档。

<a id="c-runtime-tests"></a>

## C11 运行时声明与验证登记

本表登记当前已声明/实现函数组，不把 init/writer 辅助函数称为完整编码规则。
同组每个声明均由对应测试或其明确调用路径执行；“负例”按函数契约选择，
void free、只读查询等无失败状态的函数不虚构错误返回。

| 声明组（头）                                                                                         | 链接/调用证据                               | 解码/线路检查                                                                   | 失败或边界检查                                  |
| ---------------------------------------------------------------------------------------------- | ------------------------------------- | ------------------------------------------------------------------------- | ---------------------------------------- |
| status_name、byte_view、byte_buf（common）                                                         | smoke/failure                         | 状态名、view 字段                                                               | 新旧错误码、未知码、NULL/空 view                    |
| BigInt init/free/sign/is_zero/cmp/from_decimal/to_int64/to_bytes                               | smoke/failure                         | 已知 2^128 limbs、幅值字节与 int64 极值                                             | 每分配点、limb 边界、重复 free、非法十进制/越界            |
| BigInt signed export/import（bigint）                                                            | correctness/failure/oracle            | 正负极值、大于 64 位、补码保护字节、Python signed int                                     | 空/冗余表示、输出不变、signed export 分配点            |
| BitWriter create/destroy/put_bit/put_bits/from_bytes/bitmap/octets/align/take/clear/queries    | smoke/failure/correctness             | 0..7 偏移 × 0..17 位独立模型、字节/bitmap                                           | 64 位宽度、短源/NULL、扩容 OOM/算术溢出、清空后复用         |
| BitReader create/destroy/get_bit/get_bits/get_bits_as_bytes/get_octet/get_octets/align/queries | smoke/failure/correctness             | 实际复制输入、UPER/APER 读位置、尾字段                                                  | OOM、截断、资源上限、失败输出不变                       |
| ByteReader create/destroy                                                                      | smoke/failure                         | 创建输入副本；尚无完整字节解码 API                                                       | 两个分配点失败、NULL/长度契约                        |
| BitString init/free/resize                                                                     | smoke/failure/correctness             | 固定/变长位值、成功替换、重复释放                                                         | 扩容失败、资源上限、截断时保留旧值                        |
| BER tag/length encode/decode                                                                   | smoke/failure                         | 高 tag、长长度、已知字节                                                            | 容量、截断、资源上限、不提前写输出；indefinite 仍为受限 API    |
| 旧 PER bool/int/unbounded_int/size/constrained_size/bitstring/octetstring                       | smoke/failure/correctness，经 UPER 包装   | 旧烟测补实际解码；兼容入口的字节/结果                                                       | 分配失败、显式 Unsupported 的片段 scalar、非法范围      |
| PER int_ex/unbounded_int_ex                                                                    | correctness/failure/live 外部对照工具       | UPER/APER、有界/无界/半约束/扩展/full int64 与大整数                                    | 非法范围、冗余 signed content、OOM、超资源           |
| PER size_ex/length_fragment                                                                    | 正确性测试                                 | 根/扩展、fixed/var/semi、继续标记与结束零                                              | 无效标记、无效 variant、scalar 不冒充片段链            |
| PER normally-small/ENUMERATED/OID ex                                                           | C 正确性测试 + C/C++ 一致性                   | 两 Variant normally-small 边界、根/扩展 ordinal、OID BER contents 与 X.691 23 长度编码 | 非法根 arc、uint64 溢出、非最短/截断 base-128、失败输出不变 |
| PER SEQUENCE/SET 前导位图                                                                          | C 正确性测试 + C/C++ parity/failure        | 扩展标志与 OPTIONAL/DEFAULT presence bitmap                                    | 截断、OOM、非法位、失败输出不变、超上限                    |
| PER octetstring_ex/bitstring_ex                                                                | correctness/large/failure/live 外部对照工具 | 短值冻结向量、16K/64K 阈值、多片、后续字段位置                                               | 截断、源容量、根约束、生产上限、后片 OOM 清理                |
| PER open_type                                                                                  | correctness/large/failure             | 前置 1 位下两 variant 对齐及内容、完整分片                                               | 继承共享片段引擎的坏长度/截断/OOM；调用者提供完整内层编码          |
| JER 写入器 init/puts/printf、读取器 init                                                              | smoke/failure                         | 固定文本输出；读取器只绑定输入                                                           | 缓冲区不足、NULL 输入；printf 未提交尾部可改写            |
| XER 写入器 init/puts、读取器 init                                                                     | smoke/failure                         | 固定文本输出；读取器只绑定输入                                                           | 缓冲区不足、NULL 输入；没有宣称 XML 解码器已实现            |

`Asn1CPerCorrectness` 和 `Asn1CPerLargeValues` 都是纯 C11，无 GoogleTest 或
Python/oracle 运行依赖。后者单独编译 128KiB 限制的运行时源副本；生产默认
仍为 64KiB。`Asn1CRuntimeFailureContracts` 的 allocator 宏只存在于测试副本。

<a id="c-runtime-tests--独立-oracle-与明确排除项"></a>

### 独立外部对照工具与明确排除项

`scripts/validate_c_per_oracle.py` 通过 ctypes 加载纯 C 共享库，与本地
asn1tools 0.166.0 做真实双方 encode/decode 和字节比较。测试 schema 的前后
BOOLEAN 能揭示对齐及片段消费错误；184 个短向量冻结在
`tests/c_lang/golden/per_oracle_vectors.h`。大型值全字节比较不写入巨大头文件。

未修改外部对照工具源码，以下边界不盲目按外部对照工具输出裁决：

1. `INTEGER(lower..MAX)`：外部对照工具丢失半约束下界（原有 PER golden 体系也已记录）；
   本次按 X.691 11.7 验证 lower=-100 时 -100 -> `01 00`、28 -> `01 80`。
2. 根外 BIT STRING：外部对照工具明确抛 NotImplementedError；用独立逐位字段构造及
   两 variant 的扩展/分片测试验证。
3. 根外 OCTET STRING 整片：外部对照工具用单 determinant 写全部内容，未写最终零；
   本次按 11.9.3.8.3 验证整片后零及多片，保留正确编码。
4. APER 有界变长零长度：外部对照工具对不存在的内容仍 align；按 11.9.3.3 及其
   Note 2 验证前置/后置 BOOLEAN 的线路位置。

本表新增的 normally-small、ENUMERATED ordinal 和 OID C 原语目前以冻结标准向量
及 C/C++ 两 Variant 字节一致性验证；尚未加入独立 asn1tools/asn1c primitive
differential。枚举映射由调用方提供排序后的根/扩展 ordinal，不能据此推断
生成器已支持 ENUMERATED 值映射或生成 PDU 编解码。

这些项在脚本输出中逐条 EXCLUDED，不计入通过的独立差分用例。条文来源：
[ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en)。
这不宣称完整 ASN.1、全部 C 编码规则或 C emitter 已完成。

<a id="c-runtime-tests--生成-c-编解码的后续覆盖"></a>

### 生成 C 编解码的后续覆盖

第 5 批 `byte_io.h`、`tlv.h`、`tlv_schema.h`、`value_content.h` 已有真实执行 API，覆盖 TLV 原语、三形态 PDU、逐分配失败、资源限制、21 消费者矩阵及整份 RRC/asn1c 对验。早期“只有骨架”的说明只针对旧接口；信封、规范值/模式检查、外部外部对照工具与 CER 条文向量分别记账。

第 6 批 `oer.h`、`text_tree.h`、`schema_codec.h` 完成实际 C11 结构编解码，区别于 jer.h/xer.h 旧 reader-init/原始写入器。39 个消费者、72 组独立冻结向量、解析负例、指令隔离、规范边界、每分配点及缩小预算测试覆盖这些接口。OER 约束 REAL 的 binary32/binary64 拒绝精度损失，无约束基本 OER 也验证 DER 内容；解析器提供字节偏移，完整生成字段路径仍待扩展。

第 7 批 BigInt 根/区间 API 继续共用 Variant，`CGenBatch7.BigintRoot` 固定十二组独立 asn1tools 的边界、根外及正负 2^128 向量，并核对截断/错误扩展分支；离散根以 X.691 RANGE 及非扩展精确成员规则处理，并有独立 asn1c 向量。

`RemainingFeatures` 覆盖命名位串、UTCTime 和已知 JSON/XML 类型选择，逐分配失败包括部分选中值；`OwningDefaults` 在十二入口/三形态/双翻译单元验证 INTEGER DEFAULT。生成解码种子与 Clang libFuzzer 到达十二个 PDU 包装入口，不宣称穷尽所有模式。详细结果和保留边界见第 7 批验收。

## 任务与完成状态

当前未完成事项统一在 [项目 TODO](todo.md)；已完成的批次、验收要求和历史路线统一在 [变更历史](changelog.md#remediation-tasks)。本页只保留代码规范、测试体系、验证纪律和可复用覆盖登记。
