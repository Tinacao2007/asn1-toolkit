# 变更日志

记录约定：**追加式，最新在上**。每条含日期、变更摘要与验证口径。历史 Phase
里程碑表见下；2026-10 起的详细变更按条目记录。当前状态见 [status.md](status.md)。
当前未完成事项见 [项目 TODO](todo.md)。

## 阅读导航

[近期变更](#近期变更详录) · [CGEN 七批验收](#cgen-validation) · [REV 六批验收](#remediation-validation)

## 近期变更详录

<a id="p1-3-completion"></a>

### 2026-10-27 P1-3：协议 PDU 独立对验与 NR 验收定义

- 完成原任务的代表值登记、独立双向线路、真实 C/C++ 消费者回归和 NR 验收定义。LPP/S1AP/LTE RRC 共 13 个真实顶层类型测试值（构造值，非抓包），最小/典型/边界及异常值均有回归；UPER/APER/BER 共 39 组顶层线路，连同 10 个 S1AP 内层值为 69 组。
- pycrate 0.8.1 从完整原模式独立编译，冻结字节及抽象值；项目双语言编码、解码、重编码和 pycrate 对项目输出反向解码共 138 组。asn1tools 0.169.0 交叉检查 LPP/LTE 27 组，asn1c 0.9.29 追加 LPP/LTE UPER/BER 54 次独立编码/反向解码比较。工具不支持的 S1AP 表约束/APER 不算通过；S1AP 由 pycrate 提供独立证据。
- 六个消费者执行 1860 个异常输入，要求正常错误返回，崩溃不算拒绝。开放内容逐层用项目实际输出组装；同时修复 C/C++ 动态标签包装、C++ 隐式命名 OF 多余包装及 BER 原始开放内容入口。不放松未知内部模式的 DER/CER 检查。
- 核对到 LTE 文件头为 TS 36.331 V8.12.0，`rrc_8_6_0` 只是历史目录名。固定来源/hash 和逐向量结果见 [目录](../tests/telecom/protocol_cases.json)、[冻结向量](../tests/telecom/protocol_vectors.json)、[验收摘要](../tests/telecom/protocol_validation.json)。
- NR TS 38.331 [验收定义](../tests/telecom/nr_38331_acceptance.json) 单独选定 V15.3.0 和官方来源，要求全文三个模块、十二入口三形态、代表 NR PDU UPER、异常资源及全量门禁。全文实际执行未完成，另列 [NR-01](todo.md#nr-01)，不宣称 NR 已支持。
- 最终门禁：GCC Release **519/519**（含现场三工具对验），GCC ASan+UBSan **518/518**，MSVC Release **482/482**；GCC/Clang C 严格矩阵各 **84/84**，原 C++ 矩阵 **84/84**。全量前重建全部消费者，未关闭插桩或出版锁；现场门禁未启用时默认离线为 518 项。
- 复现：按 [开发指南](development.md#protocol-interoperability-tests) 执行 `Protocol.*`；外部工具仅为测试依赖，运行时仍零第三方依赖。完成项从当前 TODO 移除；更广消息、标准及资源矩阵仍按未完成任务推进。


### 2026-10-27 TODO 职责修正与构建目录恢复

- `todo.md` 只保留整个项目当前未完成的实现、核查、验证和发布事项；已完成 REV/CGEN 清单和历史功能路线移入本页，执行规范仍在开发指南。
- 恢复此前设置的隐藏属性，19 个构建目录全部可见；当前不移动、清理，最终版本待 release 时独立构建整理。
- 同步索引、项目 skill、链接及旧 runner 的历史清单读取边界，避免把当前待办误当成已完成批次重新执行。

### 2026-10-27 构建显示与独立 TODO

- 本地根目录共 19 个构建目录，保留 `build` 可见，将其余 18 个设为隐藏；未移动、删除或重配构建，隐藏属性不属于 Git 版本内容。
- REV/CGEN 执行清单与功能路线从开发指南移到独立 `todo.md`；汇总标准、电信、资源/诊断、工具链/设备及发布剩余项，区分 C11 已完成范围和未验证的完整标准覆盖。
- 同步根 README、文档目录、状态、项目 skill 与夜间 runner/测试路径；后续验收仍追加本页，不重建散页。

### 2026-10-27 文档归并与中文统一

- 按根 README 的七份主文档分类：架构包含接口/ABI/共享规划，使用指南包含构建安装/C 输出/夜间操作，开发指南包含门禁/覆盖/执行清单，状态页包含标准能力与限制，变更历史包含 REV/CGEN 验收。
- 移除被合并的专题文档，保留验收数字、命令、冻结向量与明确边界；评审快照归入 `analysis/`，不再充当当前状态。
- 说明文字统一中文，代码、命令、文件名、API 和原始日志保留原拼写；同步索引、锚点、项目 skill 与脚本/测试路径。

### 2026-10-24～26：CGEN 第 1～7 批

完成 C11 后端 ABI/能力审查、CLI、共享规划的真实 C 消费、类型/生命周期/DEFAULT、三形态及源分片、PER/TLV/OER/JSON/XML 编解码，以及最终安装、矩阵、模糊测试和电信结构收口。三次第 2 批未完成尝试和后续完成证据均保留在本页下方的 CGEN 验收章节，不再重复维护英文条目。

最终生成/运行时 ABI 为 3；MSVC Release 476/476、GCC ASan+UBSan 512/512，GCC/Clang C 及原 C++ 矩阵各 84/84。历史计数、命令、缺工具情况和明确支持边界按各批记录，不以最终结果覆盖历史失败。

### 2026-10-23（C 后端夜间 runner）

- 增加 CGEN 逐批非交互 runner 与 PowerShell 包装器，固定 Release/ASan+UBSan、原有测试、
  分层和 84 入口 C++ 矩阵门禁；验收通过后精确暂存本项目文件并本地 commit，不 push。
- 保存状态、日志与提交前 checkpoint，保护验收正文和 runner，失败保留现场，支持显式恢复。
- 临时 Git 仓库中的 runner 自测 10/10 通过；这不是 C 后端功能验收，CGEN 实现尚未开始。
- 原有文档修改单独提交；启动/恢复/预算和验证边界见 [夜间执行说明](usage-guide.md#c-nightly)。

### 2026-10-21（第六批）

**REV-25/26/28 完成；REV-27 完成规划与 C++ 首批接入，C 消费验收待后续**

- 目录生成采用 stage、备份回滚与输出/源依赖 manifest；同目录并发拒绝，可移植命名校验，旧文件仅按归属和 hash 清理，保护用户文件和符号链接。
- 补三形态双 TU 实际链接调用、split 模块环、名称/路径/归属/坏 manifest 回归；保持 12 入口跨形态线路相等。
- 配置阶段检查 compiler/runtime 传递链接，CI/CTest 检查实际 include，公共头独立 C++17/C11 编译；加入直接/间接越界负例。
- 最小 IR 规划复用 PER presence/扩展数与整数约束决定；C emitter 和 `--lang c` 仍未实现，真实 C 接入条件写入设计和 TODO。
- GCC Release/ASan+UBSan 各 **442/442**，语法矩阵 **84/84**，MSVC 针对性 **37/37**；独立组件和安装四类消费者通过。详见 [第六批验收](changelog.md#remediation-validation--rev-25-28)。
- 同步 README、架构/使用/开发/状态、C 头、产物/规划契约和项目 skill；补 ABI/所有权/线程安全/资源限额与许可发布待办，保留历史验收原数字。

### 2026-10-21（第五批）

**REV-20..24 — 标准能力、约束保真、规范策略及独立验证**

- 建立分层标准/oracle 能力矩阵和 REAL/字符/时间支持范围；合法未支持的约束在编译期失败，保留精确 REAL 模型与现代时间类型等后续项。
- 修复导入/别名常量、参数化值边界与实例键；阻止负 SIZE、空交集和未保真约束降成无约束。补三形态真实 PER 位宽回归。
- CER 复用规范原语策略，补 REAL/DEFAULT/重复 SET 与字符串片段检查；限制 CER 内层递归和内容积累，修复 REAL 精度/范围、负零、OER float 窄化和 JER bigint 取值。
- 新增带版本/commit/schema/value 的外部向量、冻结重放和能力探针；复跑 asn1tools 互操作与 C PER 234 个双向用例，Clang sanitizer fuzz 20,000 次无崩溃。完整门禁及局限见 [第五批验收](changelog.md#remediation-validation--rev-20-24)。
- GCC Release/ASan+UBSan 各 **430/430**，7×12 语法矩阵 **84/84**，安装包消费者通过；MSVC 针对性 **59/59**。
- 同步项目修正 skill，不把局部证据写成完整标准符合性或 C emitter 完成。

### 2026-10-21

**项目修正 skill 同步**

- 新增 [asn1-remediation/SKILL.md](skills/asn1-remediation/SKILL.md)，沉淀前四批已验证的错误契约、PER、缓存、并发、组合入口及构建回归流程；后续每批修正同时维护对应指引。
- 文档同步规则改为更新当前数字、保留历史验收证据；TODO 提示模板加入 skill 同步步骤。本次仅改文档，检查 skill 格式、相对链接与 diff，不重复编译门禁。

**REV-13..19 — 第四批缓存、并发、产品入口与构建/安装门禁**

- 以 compiler 内容指纹替代写死版本；完整输出选项、依赖及有序源路径入键；读取失败与无来源 IR 禁用缓存，损坏产物重新生成。
- declare/resolve 限制并发并按模块序合并；JERI 作为共享 X.697 JER 的产品入口，补 ARRAY 控制段及未实现控制段错误；OER/COER、XER/CXER/E-XER 组合分别生成。
- C11/编译器条件放在 target；最低 CMake 离线验证、GTest SHA256、宿主/目标两阶段与安装导出；私有 GTest 不进入 SDK，新增独立 C/C++/compiler/宿主生成消费者。
- GCC/Clang Release 与 ASan+UBSan 四轮各 **392/392**，实际 7×12 语法矩阵 **84/84**；MSVC 产品构建/40 项回归、CMake 3.16.8 代表门禁 2/2、Linux AArch64 编译/链接通过。范围与复跑命令见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

### 2026-10-20

**REV-10..12 — 第三批 C++ 错误模型与 E-XER 生成代码修正**

- 属性区分 missing/invalid，按 mandatory/optional/default 处理；整数完整
  解析及区间/宿主范围验证，错误携带逻辑属性名；三种生成形态实际链接运行。
- SourceFile / IR BigInt 返回 Result，OID/CLI from_chars，filesystem error_code，
  flush 后检查；线程资源失败 deferred 回退，容量错误归入诊断，保留 bad_alloc；
  PER 生成器传播 restricted-string 字母表错误。
- Result nodiscard / 访问契约与死亡测试；Error 偏移单位、字段路径、迁移文档；
  36 处原测试编码调用增加成功检查；int64 两端、负零及 ENUMERATED 后继溢出保护。
- Release / ASan+UBSan 各 **370/370**；公共头 47/47 严格 C++17 编译、历史语法
  矩阵 84/84。完整边界见 [第三批验收](changelog.md#remediation-validation--rev-10-12)。

**REV-04..09 — 第二批 C11 位操作与 PER 线路修正**

- 修复 MSB-first 尾位和 >64 位静默截断，新增 signed BigInt 内容转换；
  完整分片、末尾零、变长/扩展 SIZE、半约束及全 int64 域处理。
- `per_core.c` 统一 UPER/APER Variant；旧 `per.c` 为 UPER 包装；新 `_ex` 明确
  描述约束和完整值，旧 known-length raw OCTET STRING 解码 ABI 保留。
- 新增纯 C 正确性/128KiB 大值门禁，补冒烟测试实际解码和失败路径；独立
  asn1tools 差分 234 用例、184 冻结向量，对外部对照工具能力缺口记录条文仲裁。
- 实测 Release / ASan+UBSan 各 **348/348**；两个 PER 门禁各 4473 检查，故障
  门禁 498 检查。详细命令与最终语法矩阵见 [第二批验收记录](changelog.md#remediation-validation--rev-04-09)。

**REV-01..03 — 第一批 C11 内存安全与失败传播修正完成**

- BitWriter 返回并传播扩容失败，整次操作预留容量，检查计数/倍增溢出；
  take 失败保持有效位，clear/take 后复用不会保留旧 1 位。
- BigInt 使用临时解析后提交，传播 OOM 与 limb 限制；C 错误码 0..7 保持，
  新增 OutOfMemory=8、ResourceLimit=9；修正 PER/BER 原语失败输出与清理。
- 新增测试专用 allocator 与 `Asn1CRuntimeFailureContracts`，生产库没有测试
  hook；新增 [C11 契约](architecture.md#c-runtime-contracts) 与 [验收记录](changelog.md#remediation-validation--rev-01-03)。
- 实测：GCC 13.3，Release 与 ASan+UBSan 完整 CTest 各 **346/346**；故障测试
  424 个内部检查；20 头 × C11/C++17 独立编译；历史 7×12 选择器语法矩阵
  84/84。历史列表含 per 组合别名，JERI 缺口仍属 REV-15/16；第二批 PER
  补码/分片/尾位语义未在本批完成。既有外部对照工具向量已回归，未重新生成。

**同日较早 — 创建修正计划（当时仅文档变更）**

- 新增 [工程评审修正 TODO](changelog.md#remediation-tasks)：REV-01..28，按 C11 安全与
  PER 正确性、错误模型、缓存/并行/构建、标准覆盖及后端扩展分批执行；
  每项包含证据性质、修改范围、依赖和验收，并提供逐项交给 AI 的提示模板。
- 该计划创建步骤的验证口径：仅文档与相对链接核查；当时没有执行实现修复，也没有重新运行
  Release、ASan/UBSan 或历史 345 项测试。原功能规划通过 development.md 链接保留。

### 2026-10-17

**P2-4 子任务 2 — 纯 C11 运行时实现（`src/c_runtime/*.c` + `asn1::c_runtime` 静态库）**

- 实现：`src/c_runtime/{c_common,bigint,bit_io,bit_string,byte_io,ber_tlv,per,jer,xer}.c`
  ——每个头文件 `include/asn1/c_runtime/*.h` 声明的函数/方法全部填体实现；
  与 C++ `asn1::BigInteger` / `asn1::BitWriter` / `asn1::BitReader` /
  `asn1::ber::encode_tag/decode_tag/encode_length/decode_length` /
  `asn1::per::{encode_constrained_whole_number, encode_length_determinant,
  encode_unconstrained_whole_number, encode_bitstring, encode_octetstring}`
  byte-equivalent。`asn1_per_int_range_t` + `asn1_per_size_range_t` 是 emit_c
  直接使用的 per-type 入参。命名约定 `asn1_<codec>_<primitive>`
  替代 C++ `namespace asn1::<codec>::<primitive>`（flat prefix，无嵌套）。
- `asn1::c_runtime` CMake 静态库（`-std=c11 -Wall -Wextra -Wno-unused-parameter`，
  与 C++ `add_subdirectory(tests)` 解耦：`set_source_files_properties` 把 C
  per-file flag 限定到 `src/c_runtime/*.c`，不污染 `tests/` 里其它 C++ 目标）。
- 子任务 2 覆盖的头声明：`asn1_c_common.h` 的 status name + 字节 view/buf
  构造；`bigint.h` 的 init/free/from_decimal/sign/cmp/to_int64/to_bytes；
  `bit_io.h` 全部（BitWriter + BitReader heap-owned，P2-1 bulk bitmap
  fast path 保留）；`bit_string.h` init/free/resize；`byte_io.h` 不透明
  字节读取器；`ber_tlv.h` tag + length 编码/解码（含 X.690 高 tag 号 +
  长形式长度）；`per.h` 的 `asn1_per_encode_bool/decode_bool/.../bitstring/
  octetstring`；`jer.h` / `xer.h` 的写入器 + 读取器 init/puts/printf。
- 门禁升级：原子任务 1 `-fsyntax-only` 仅验证头解析；子任务 2 把
  `Asn1CHeaderSyntax`（echo placeholder）替换为 `asn1_c_runtime_smoke` 可执行
  —— 链接 `asn1::c_runtime`，跑 `tests/c_lang/c_runtime_smoke.c`，对每个
  头声明的函数调用 + canonical 字节验证 + 边界场景（truncated input、
  length overflow、incomplete field、invalid decimal、空 buffer 等）。
- 验证：`Asn1CRuntimeSmoke` 1 项（25+ 子检查全过）：
  - 状态名：每种 `asn1_status_t` 名称稳定，超出枚举值返回 fallback；
  - BigInteger：decimal → limbs round trip；`to_int64` 对 `2^64` 报
    `LENGTH_OVERFLOW`；`to_bytes` 对 0 → `{0x00}`；`is_zero`/`sign`/`cmp`
    在 0/正/负/symmetric 配对下行为正确；空字符串 / 单 `-` / 非数字字符
    报 `INVALID_ARGUMENT`；
  - BitWriter + BitReader：4 字节 DE AD BE EF 写入 + 读回 = 字节相等；
    P2-1 8-bit bitmap → `0xB2`（10110010）；`put_bits(0x123, 12)` →
    `{0x12, 0x30}`（MSB-first，bottom-12-bits-of-value 约定）；
    `get_bits` 超尾 → `TRUNCATED`；
  - BER TLV：universal INTEGER tag `{0x02}` 编码 + 解析；高 tag 号
    (`constructed=1, number=100`) → `{0x3F, 0x64}` 双向；长形式长度
    `200` → `{0x81, 0xC8}` 双向；`0x80` 不定长 → `LENGTH_OVERFLOW`
    （CER / BER constructed 后续子任务处理）；
  - PER 约束 INTEGER(0..7) value 5 → `0xA0` 双向；PER OCTET STRING
    `[0x11,0x22,0x33]` → `{0x03, 0x11, 0x22, 0x33}`；PER BIT STRING 8-bit
    value `0xA5` → `{0xA5}`；
  - JER 写入器: 拼出 `{"name":"Alice"}`；XER 写入器满 cap 时再 put
    报 `LENGTH_OVERFLOW`。
- Release ctest **345/345** 通过（+1：Asn1CRuntimeSmoke）；Sanitizer 构建
  (ASan+UBSan) **345/345** 通过，零 ASan/UBSan 报错。
- 子任务 3+ 待办：`src/codegen/emit_c/*` emitter + CLI `--lang c` +
  `EmitOptions::lang` + 全测试样例编译门禁 + P1-3 外部对照工具复用（`--lang c`
  与 `--lang cpp` 同一外部对照工具字节级一致）。子任务清单见
  [TODO 清单](changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c)。

**P3-1 — 前端并行分析（`--parallel <N>`，declare + resolve 并行，diagnostics 字节级可重现）**

- 设计：`Analyzer::analyze(modules, parallelism = 0)` 新增 parallelism
  参数；`0`（默认）= 串行 baseline（保持现状，CI 金标对验强制），`>0`
  = worker 数。`declare_pass` 每模块独立声明符号 + 局部 `SymbolTable`，
  pass 结束后按 module index 串行 merge；`resolve_pass` 每模块持完整
  符号表只读视图 + 局部 `Diagnostics`，worker 间不共享跨模块
  diagnostics；`tag_pass` 仍串行。
- `lower_pass` **保持串行**（**关键变更**，偏离原 todo）：`LowerCtx`
  共享 `model_.arena`，跨模块 `Referenced.resolved` 链接在 pass 期间
  跨模块完成；把 arena 拆 worker + 串行重映射需要重建跨模块链接图，
  作为后续工作 deferred。`lower_pass(int)` 形参已就位，扩展点保留。
- API 扩展：`Diagnostics::merge(Diagnostics&&)`（caller 决定追加顺序，
  items 按 merge-call 序排列，与串行 baseline 字节级一致） +
  `SymbolTable::merge(SymbolTable&&)`（按 module 键合入；duplicate
  检测由 declare 自身保证）。`Analyzer::analyze` 新增 `parallelism`
  形参（默认 0 = 串行；现有调用零变更）。
- CLI：`asn1cxx --parallel <N>`（N ∈ 0..1024；0 = 串行 = 默认；>0 启用
  worker 线程池），`--help` 文档化。
- 验证：`asn1_parallel_analyzer_tests`（10 项）— Diagnostics::merge 字节
  级序列 + error_count 聚合 + 空合并；SymbolTable::merge 合入 +
  duplicate 检测；5 个测试样例模块（tiny single-module / multi-module
  import / undefined type / unknown field / duplicate symbol）在
  `--parallel 0` 与 `--parallel 16` 下 diagnostics **逐字节相同** +
  error_count 一致。`asn1_cli_tests` 新增
  `Cli.ParallelModeProducesByteIdenticalOutput`：LPP 14.3.0 测试样例上
  `--parallel 0` 与 `--parallel 16` 生成的 `generated.hpp/.cpp` 在
  namespace 替换后**逐字节相同**。手工验证 RRC 8.6.0（78 模块）：serial
  vs `--parallel 16` 生成 `generated.hpp` SHA-256 一致
  （`af86e15d...`）。Release + sanitizer 全量 ctest **344/344** 通过，
  零 ASan/UBSan 报错。

### 2026-10-16

**P2-3 — 增量代码生成（内容哈希缓存）**

- 设计：`EmitOptions::emit_cache`（默认 true）+ CLI `--emit-cache`/`--no-emit-cache`；
  缓存目录 `<emit-dir>/.asn1cxx-cache/`，每单元（flat: `default`；split: 模块
  ns ident）一个 `.cache` 文件，存 `(sha256(content_key), {output_path: sha256})`。
  缓存键 = `sha256(unit_name || sorted(source_paths + per-source sha256) ||
  codec_mask || basename || namespace || emit_inline || emit_split ||
  cli_version)`。命中条件：键一致 **且** 每个输出文件的当前 sha256 与
  记录的 sha256 一致（防止手动改产物后被错认命中）。
- 实现：`src/codegen/{sha256,emit_cache}.{hpp,cpp}` — 自包含 SHA-256 实现
  （FIPS 180-4 全部 4 个测试向量通过，无 OpenSSL 依赖）；
  `CppGenerator::emit_to_dir(model, options, diag, dir)` 新增 flat 模式入口
  （带 dir + 缓存）；`CppGenerator::emit_split` 自动启用缓存；legacy
  `emit(stream, stream)` 与 `emit_header_string()` 行为不变。
- 诊断：每次 emit 末尾追加 `note` 级 `emit cache: N/M units hit (T hit), K
  missed (T miss)`，不进错误计数。
- 验证：`asn1_emit_cache_tests`（8 项）— SHA-256 FIPS 向量；flat 二次运行
  hit；改字节强制 miss；`--no-emit-cache` 不命中；手动改产物触发 miss；
  basename 变化触发 miss；split LPP 单单元 hit；split LPP
  `--no-emit-cache` 不命中。`asn1_cli_tests` 新增 5 项 CLI 冒烟测试（默认
  二次运行 hit / `--no-emit-cache` 无 note / `--no-emit-cache` 后
  `--emit-cache` 重新启用 / 首运行状态行是 "wrote" 而非 "reused
  (cached)" / `--emit-split` 第二次运行命中 unit 缓存）。Release +
  sanitizer 全量 ctest **333/333** 通过，零 ASan/UBSan 报错。
- 编译时优化：测试样例 compile gate（flat / single-codec / split）的静态库
  编译选项从 `-O3` 降为 `-O1`（ABI 兼容；仅影响测试样例门禁的墙钟时间，
  不影响运行时 / 单元测试）；CMake 全局加 `-pipe -fno-plt`（sanitizer
  构建不加以避免影响栈回溯）；`ContentCache::compute_key` 接受外部传入的
  per-source sha256 映射（避免 split 模式下对同一 .asn 文件做 N 次
  SHA-256）。

**登记 P2-3 / P3-1 待办清单**（不涉及代码改动）

- 新增 [docs/development.md](changelog.md#roadmap)：把 P2-3（增量代码生成，~7 h）和 P3-1
  （前端并行分析，~15 h）从 status.md 的「未排期」一栏升级为「已规划、
  未排期」，各子任务含设计要点、估时、验证门槛、可重现性约束。
- [status.md](status.md) 增 development.md 链接；P2-3 / P3-1 行简化为指向 todo
  的锚点。诊断输出逐字节可重现的硬要求记录在 P3-1 子任务里，作为
  强制约束。
- 没有代码 / 构建 / 测试改动。

### 2026-10-15

**P2-2 收尾 — XML 家族单编码规则 include 缺口修复 + 单编码规则 × 全家族门禁**

- 根因：`emit_includes` 的 XML 分支（xer/exer/cxer）未 include
  `<asn1/runtime/byte_io.hpp>`，而 XML 解码器在 CHOICE/ENUMERATED 等
  失败路径会调用 `asn1::make_error`（定义于 `byte_io.hpp`）。全 12-codec
  组合门禁中 PER/OER/TLV 分支都会捎带这个头，所以组合门禁一直绿；任何
  单编码规则选 XML 都必暴露。
- 修复：`emit_includes` XML 分支显式补 `#include <asn1/runtime/byte_io.hpp>`
  （一行 + 注释解释）。
- 回归门禁：`tests/CMakeLists.txt` 新增 LPP × 12 编码规则各跑一次独立 emit +
  完整编译（`asn1_lpp_single_{uper,aper,per,ber,der,cer,oer,coer,jer,
  xer,exer,cxer}_compile`），覆盖单编码规则场景的 include 自包含性，
  防组合门禁再掩盖同类缺口。
- 验证：手工 3 测试样例 × 12 编码规则 = 36 case 全部零错误（含 flat 单编码规则
  跑 `--codec xer`/`exer`/`cxer`）；新门禁 12 个静态库编译全过；Release
  320/320 零回归。
- 状态变更：`status.md` 中"XML 家族单编码规则 include 缺口"由已知失败
  改为 ✅ 已修复；Phase 45 由"部分完成"改为"单编码规则 × 全家族门禁已就位"。

**P2-1 — bit_io 批量位写路径优化**（Phase 里程碑见下表）

- `put_bits(value,nbits)` 改先填半字节再整字节批量写；新增
  `BitWriter::put_bitmap(Span<byte-per-bit>)` 按 64 位打包一次写出；
  `put_octets` / `put_bits(Span)` 在八位对齐流上直接批量拷贝；`put_bit`
  下标除法改位移；`encode_bitmap` 与 BIT STRING 分片写路径改走批量接口。
- 验证：新增 `BitIoBenchmark.PresenceBitmapWrite`；bitmap 密集型写
  196,532 → 626,075 ops/sec（≈3.2×），UPER 往返 +12%，输出字节与逐位循环
  逐比特一致；Release + sanitizer 320/320；三套金标准向量零回归。

**P1-1 — BER/DER/OER 金标准差分向量 + OER SET 规范序**

- `scripts/gen_tlv_golden.py` + `tests/tlv/golden/` + `asn1_tlv_golden_tests`：
  16 用例 × 3 编码规则字节级对验（外部对照工具策略：asn1tools 为 BER/OER 向量源；
  asn1c 仲裁 DER —— asn1tools 不排序 SET/SET OF，X.690 11.6 要求排序；
  asn1c 的 `oer_encode` 对 SET 值段错误（已知 bug），OER 列保留 asn1tools；
  BER 无规范 SET 序（X.690 8.14），SET 用例 BER 列记 '-'）。
- OER SET 组件改为 X.696 18.2 规范（标签）序 emit，前导位图位序与解码镜像
  同一顺序（此前按定义序，字节与外部对照工具不符）。
- 验证：Release + sanitizer 319/319；tlv_golden 测试样例全 12 编码规则
  `-fsyntax-only` 零错误；per_golden 向量零回归。

**P1-2 — CER（X.690）编解码器家族落地**（第 12 个编码规则）

- `src/runtime/cer/codec.cpp`（BER 原语策略层）：9.1 构造值不定长包装、
  9.2 + 8.6.4 超 1000 内容八位组分片、11.2 末八位组未用位清零、9.3
  SET/SET OF 标签序（未打标签 CHOICE 按替代项最小标签静态定序）、canonical
  严格性；`emit_tlv.cpp` 按 `TlvKind{Ber,Der,Cer}` 路由；`asn1_cer` 静态库
  + CLI `--codec cer`；`cer_golden` 测试样例 + 10 项字节级金标准（无外部
    CER 外部对照工具，条文推导 + DER 值等价交叉验证）。
- 顺带修复全 TLV 家族 SET 解码器缺陷：SET 内未打标签 CHOICE 组件的解码派发
  键此前为 `universal(0, true)` 占位标签（永不匹配），改为 emit 时计算该
  CHOICE 全部替代标签的并集。
- 验证：Release + sanitizer 316/316；五测试样例 × 12 编码规则 `-fsyntax-only`
  零错误。

**P2-2 — 大模块分文件/分命名空间 emit（`--emit-split`）**（2026-10-15 主体，
10-15 收尾：XML include 缺口 + 单编码规则 × 全家族门禁，详见上条目）

- 按 ASN.1 模块边界生成 `<Module>.hpp/.cpp` 对 + 聚合 `generated.hpp`；头内
  类型命名空间与模块名对齐，跨模块引用按 `<Module>::Name` 限定
  （`Emitter::named_ref`，flat 产物逐字节不变）；互相递归模块（依赖图 SCC）
  合并进同一 TU；每模块头自带 `detail::Null`；模块依赖 include 按结构依赖
  计算。多模块验证对象为 S1AP（6 模块，LPP 实为单模块测试样例）。
- 验证：LPP/S1AP/RRC 逐单元 `g++ -fsyntax-only` 手工矩阵通过（uper 全过；
  LPP 另过 aper/coer/ber/der/oer/jer）；新增 `asn1_split_lpp_split` /
  `asn1_split_s1ap_split` 编译门禁；flat 产物与改动前 diff 为空；
  Release 320/320（无新增 gtest）。
- ⚠️ 已知失败（暂停修复）：`--codec xer`/`exer`/`cxer` 单独选择时 split 产物
  编译失败（`make_error` is not a member of `asn1`）—— 根因是 `emit_includes`
  XML 分支未 include `<asn1/runtime/byte_io.hpp>`，flat 单跑同样复现，属既有
  缺陷；详见 [status.md 剩余工作](status.md#剩余工作)。

### 2026-10-14

**P0-1 — 生成代码默认 out-of-line**

- CLI 默认改为 out-of-line：`generated.hpp` 只放类型声明 + 编解码函数声明，
  定义全部落 `generated.cpp`；`--emit-inline` 恢复全 inline 头。RRC 全模块
  实测：头 33,697 → 5,998 行；消费 TU 编译 ~22s → ~5s；定义 TU 一次性
  ~63s（盈亏平衡 N≈3–4 TU）。顺带修复：此前 `generated.cpp` 是空 stub，
  测试样例静态库编译门禁实际未编译任何生成代码。
- 验证：Release 298/298（新增 2 项 CLI 测试）+ sanitizer 全量 + PER/APER
  金标准向量零回归。

**P0-2 — emit.cpp 按编码规则家族拆分**

- 5,208 行单文件拆为 7 个（`emit_internal.hpp` 共享声明 + `Emitter` 类接口；
  `emit_core` / `emit_per` / `emit_tlv` / `emit_xer` / `emit_jer` /
  `emit_oer`）。依赖图结论：共享面仅 9 个自由函数 + 11 个成员，其余 86 个
  成员全部家族内聚。函数体字节级迁移，RRC UPER 产物与拆分前逐字节一致。
- 验证：Release + sanitizer 298/298；单家族改动重编译 18.9s → 5.5s（JER）/
  6.9s（PER，-j1）；金标准向量零回归。

**P0-3 — 全编码规则产物编译门禁驱动的发射器缺陷修复**

- 首次对三大测试样例 + 新 tlv_oer_golden 做 12 编码规则 emit + `-fsyntax-only`
  门禁，暴露并修复 6 类根因（XML 运行时命名空间硬编码、解引用优先级、
  inline 列表 `.value` 误用、嵌套 CHOICE 替代未限定、解码临时变量遮蔽
  —— 新增 `Emitter::temp_counter_`、CHOICE 替代局部变量自遮蔽）及顺带修复
  （X.680 25.3 自动编号规则、default_val_cpp 按字段取默认值、X.690 8.5
  显式标签自然编码、X.696 8.7.2 OER 选择标签 constructed 位）。
- 验证：四测试样例 × 12 编码规则 `-fsyntax-only` 零错误；Release + sanitizer
  316/316，零 ASan/UBSan。

### 2026-10-08 起（文档重组前）

**表约束类型化 IE 开放类型**

- `analyzer.cpp`：entries 只挂开放类型字段；扩展枚举未显式编号时从根
  末尾继续（X.680 惯例）；OCF fixed_type 与内联 SEQUENCE OF 纳入依赖遍历；
  去重按 C++ 名。`emit.cpp`：tc decode/encode 参数改名避免嵌套 CHOICE 冲突。
- 验证：新增 `asn1_tc_ie_roundtrip_tests`（4 项）。

**多编解码组合生成**

- `--codec` 改位掩码：逗号分隔组合与重复标志累加；未指定默认 UPER。
- 验证：新增 5 项 CLI 冒烟测试；组合产物 `-fsyntax-only` 通过。

**PER/APER 金标准字节向量（asn1c 外部对照工具）**

- `scripts/gen_per_golden.py` + `asn1_per_golden_tests`：UPER/APER 各一套，
  54 向量（约束原语/长度 determinant/分片/扩展 addition/版本括号组）。
  修复扩展 addition 编码（X.691 19.9：简单扩展 addition 的开放类型仅含
  组件编码，版本括号组 `[[...]]` 才按嵌套 SEQUENCE 编码含前导位图）——
  IR 新增 `ExtensionGroup{fields, version_bracket}` 区分两种形态。
- 验证：`PerGoldenVectors.UperMatchesOracleBytes` /
  `AperMatchesOracleBytes` 通过。

**JER 对象类开放类型编解码 / CLI 测试并行竞态修复 / 三大 3GPP 测试样例编译
门禁**（均于 2026-10 上旬完成，细节见归档
[analysis/architecture-review-and-optimization.md](analysis/architecture-review-and-optimization.md)）

## Phase 里程碑表

| Phase | 内容                                                                     | 状态                                                                                                                                                                                                                                                                                                                                                                                                                  |
| ----- | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1     | 项目骨架与构建系统                                                              | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 2     | Lexer                                                                  | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 3     | 基础 parser                                                              | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 4     | AST 与语义分析器                                                             | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 5     | 约束系统                                                                   | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 6     | BER                                                                    | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 7     | DER                                                                    | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 8     | BitReader/BitWriter 与 PER 原语                                           | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 9     | UPER                                                                   | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 10    | APER                                                                   | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 11    | 代码生成器                                                                  | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 12    | 高级 ASN.1 构造（ENUMERATED / OID / RELATIVE-OID / SET / SET OF / 类型化 REAL） | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 13    | 信息对象类（class fields、开放类型 `&Value`、表约束类型化 IE dispatch）                   | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 14    | 真实电信 ASN.1 验证                                                          | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 15    | OER                                                                    | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 16    | COER                                                                   | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 17    | XER                                                                    | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 18    | CXER                                                                   | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 19    | EXTENDED-XER                                                           | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 20    | JER                                                                    | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 21    | JER 编码指令运行时                                                            | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 22    | JER 编码指令前端 + 代码生成                                                      | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 23    | INSTANCE OF + XER 编码指令前端/codegen                                       | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 24    | 全量 E-XER 代码生成指令矩阵                                                      | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 25    | REAL 编解码（IEEE-754 二进制 ↔ ASN.1 base-2）                                  | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 26    | OER REAL WITH COMPONENTS IEEE binary32/64 分类                           | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 27    | EXTERNAL / EMBEDDED PDV / CHARACTER STRING                             | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 28    | >64 位 INTEGER 编解码（`asn1::BigInteger`）                                  | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 29    | EMBEDDED PDV / CHARACTER STRING 深化 BER                                 | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 30    | BER 构造/不定长 + DER + 现代 EXTERNAL                                         | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 31    | PER 扩展 addition 序列                                                     | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 32    | ABSTRACT-SYNTAX + 现代 EXTERNAL 识别形式 IR                                  | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 33    | JER SequenceOf/SetOf 解码代码生成                                            | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 34    | Codegen BER / DER / OER / CXER                                         | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 35    | EXER LIST character-encodable 元素类型                                     | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 36    | OER/COER 扩展 addition                                                   | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 37    | COER `--codec coer` 代码生成                                               | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 38    | 电信 UPER 编解码往返                                                          | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 39    | 验证 —— 多编码规则 Person + 电信 emit 门禁                                        | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 40    | RRC/LPP/S1AP 全量 emit —— 嵌套命名 + 参数化 IE 字段                               | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 41    | 评审硬化 —— OCF 重新解析、DEFAULT、PER 约束、BitString、长度上限                         | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 42    | 全编码规则产物硬化 —— 12 编码规则编译门禁（P0-3）；`tlv_oer_golden` 测试样例 + 8 往返测试          | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 43    | CER（X.690）编解码家族（P1-2）；`cer_golden` 测试样例 + 10 字节级金标准                    | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 44    | BER/DER/OER 金标准差分验证 + OER SET 规范序（P1-1）                                | 已完成                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 45    | 大模块分文件/分命名空间 emit（`--emit-split`，P2-2）                                 | ✅ 已完成（XML include 缺口修复 + 单编码规则 × 全家族门禁就位，2026-10-15 收尾）                                                                                                                                                                                                                                                                                                                                                             |
| 46    | 增量代码生成（P2-3）                                                           | ✅ 已完成（2026-10-16，缓存就位 + 7 项单元 + 3 项 CLI 门禁）                                                                                                                                                                                                                                                                                                                                                                         |
| 47    | 前端并行分析（P3-1）                                                           | ✅ 已完成（2026-10-17，`--parallel <N>` + declare/resolve 并行 + 11 项门禁；`lower_pass` 串行 deferred；commit `5856565`）                                                                                                                                                                                                                                                                                                          |
| 48    | 3GPP 协议消息级金标准向量对验（P1-3）                                                | ✅ 代表协议 PDU 对验和 NR 验收定义已完成，见 [P1-3 验收](#p1-3-completion)；NR 全文执行另列 NR-01                                                                                                                                                                                                                                                                                                                                  |
| 49    | C 语言代码生成器（P2-4）                                                        | 🟡 子任务 1（`include/asn1/c_runtime/*` 头骨架 + `-fsyntax-only` 门禁，commit `c137142`）+ 子任务 2（`src/c_runtime/*.c` 纯 C11 实现 + `asn1::c_runtime` 静态库 + `Asn1CRuntimeSmoke` 链接+运行门禁）已落地；ctest **345/345**。子任务 3+（`src/codegen/emit_c/*` + CLI `--lang c` + 全测试样例编译门禁 + P1-3 外部对照工具复用）未排期；目标字节输出与 `--lang cpp` 逐字节一致（复用 P1-3 外部对照工具）；估时 ~43 h / 5-6 工作日；子任务清单见 [TODO 清单](changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c) |

## 2026-10-26 — CGEN 第 6 批

- 增加 C OER/COER 原语与结构生成，以及 JER/JERI、XER/CXER/E-XER 解析和值编解码。
- 精确 BigInt 十进制、Unicode/转义、JSON 指令、XML 属性/受限指令、规范策略和事务性预算。
- 共享 OER SET 最小根标签规划；验证版本组、nested CHOICE 和跨家族指令隔离。
- 新增三形态、12 入口组合、故障注入/资源、独立 asn1tools 互解。
- 详细值域、未支持指令及外部对照工具限制见 [第 6 批证据](changelog.md#cgen-validation--batch-6)。

<a id="cgen-validation"></a>

## C11 生成后端验收记录

各节保留原批次的命令、结果与限制，属于历史验收记录；历史测试数量和当时的未完成项不作为当前状态。当前能力以 [status.md](status.md) 为准。

<a id="cgen-validation--目录"></a>

### 目录

- [第 1 批验收](changelog.md#cgen-validation--batch-1)
- [第 2 批验收](changelog.md#cgen-validation--batch-2)
- [第 3 批验收](changelog.md#cgen-validation--batch-3)
- [第 4 批验收](changelog.md#cgen-validation--batch-4)
- [第 5 批验收](changelog.md#cgen-validation--batch-5)
- [第 6 批验收](changelog.md#cgen-validation--batch-6)
- [第 7 批验收](changelog.md#cgen-validation--batch-7)

<a id="cgen-validation--batch-1"></a>
<a id="cgen-validation--batch-1--changes"></a>
<a id="cgen-validation--batch-1--executed-stage-checks"></a>
<a id="cgen-validation--batch-1--verification-boundaries"></a>

<a id="cgen-validation--第-1-批验收"></a>

### 第 1 批验收

日期：2026-10-24。范围仅为 CGEN-01～03：确定 C 生成 ABI、审查十二入口的运行时缺口，并增加 `--lang cpp|c`、`--c-prefix` 和参数冲突诊断。本批未实现 C 类型或编解码生成，不能将此阶段的拒绝结果当成后续 C 生成能力。

C 请求经统一目录调度和旧 `CppGenerator` 流接口时，在目录、缓存及发布操作之前诊断失败；语言、前缀加入缓存键，新选项追加到结构尾部以保留原聚合初始化。ABI 设计覆盖命名、布局、所有权、签名顺序、失败后输出/游标、预算及版本策略。缺口审查分别核对原语、容器、策略、完整解码、错误位置和资源限制，不凭头文件名假定接口已实现。

环境为 Windows 10、Visual Studio 2022 / MSVC 19.44、Windows SDK 10.0.26100.0、CMake 3.31.6-msvc6、Python 3.12.10，使用既有离线 GoogleTest，构建目录为 `build-rev4-msvc`。最终筛选 **24/24**：6 个 CGenBatch1 用例、15 个缓存/发布用例、2 个规划用例及分层检查；差异空白检查通过。

API 用例核对拒绝 C 后双流未改写、C++ 与 C 前缀冲突；CLI 用例遍历十二入口和三种形态，验证失败不创建目录、非法参数、旧输出/缓存保持，以及默认和显式 C++ 输出字节相同。三个 C++ 组合消费者实际执行十二入口并比较线路，不是文档文字检查。

当时 WSL 启动返回 `Wsl/Service/CreateInstance/E_ACCESSDENIED`，因此本批不宣称 Linux、ASan+UBSan、全量测试或 84 组合矩阵通过；MSVC Release 不等于双 Sanitizer。没有新 C 线路算法，独立 C PDU 验证属于后续批次。此阶段未修改 runner、启动器、runner 测试或矩阵脚本，没有提交或推送，也未提前勾选后续任务。

<a id="cgen-validation--复跑命令"></a>

#### 复跑命令

```powershell
cmake --build build-rev4-msvc --config Release --target asn1cxx asn1_emit_cache_tests asn1_entry_combined asn1_entry_combined_inline asn1_entry_combined_split -j 2
ctest --test-dir build-rev4-msvc -C Release -R '^CGenBatch1\.' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^(EmitCache|EmitOutput\.|EmissionPlanning\.|Architecture.Layering)' --output-on-failure
git diff --check
```

<a id="cgen-validation--batch-2"></a>
<a id="cgen-validation--batch-2--manual-completion-implementation-and-acceptance-scope"></a>
<a id="cgen-validation--batch-2--final-reproducible-gates"></a>
<a id="cgen-validation--batch-2--attempt-1--partial-implementation"></a>
<a id="cgen-validation--batch-2--implemented-scope"></a>
<a id="cgen-validation--batch-2--acceptance-actually-run"></a>
<a id="cgen-validation--batch-2--concrete-remaining-work"></a>
<a id="cgen-validation--batch-2--attempt-2--scalar-defaults-and-naming-acceptance"></a>
<a id="cgen-validation--batch-2--commands-and-results"></a>
<a id="cgen-validation--batch-2--remaining-assigned-work"></a>
<a id="cgen-validation--batch-2--attempt-3--exact-storage-size-checks-batch-still-blocked"></a>
<a id="cgen-validation--batch-2--commands-and-environment"></a>
<a id="cgen-validation--batch-2--remaining-work-required-to-finish-this-batch"></a>

<a id="cgen-validation--第-2-批验收"></a>

### 第 2 批验收

日期：2026-10-24。最终 **CGEN-04～06 在登记的存储与 DEFAULT 范围内完成**，真实编译运行的 C 消费者验证了 REV-27 的共享规划复用。以下先记录最终结果，再保留三次未完成尝试；这些尝试中的缺口不代表当前状态。

最终实现：

- 提供独立的 `emit_c_types` 双流接口，写入前检查所有可达 IR；支持 BOOLEAN、NULL、原生/BigInt INTEGER、位串/字节串/字符串存储、int64 枚举、uint64 弧 OID、宿主 double REAL、记录、CHOICE、集合、引用及开放字段布局。
- OPTIONAL/扩展使用显式存在标志；CHOICE 初始无有效分支，只释放活动分支；集合保留 data/count/capacity，递归边使用拥有型指针。释放还原初始化状态，深拷贝通过临时值提交，自拷贝有效；非法容量、判别值、OID、位填充、约束或分配失败保持旧输出，深度上限为 256。
- `init/free` 不分配，只恢复标量默认值；`reset_defaults` 事务性构造拥有型字面量及必需嵌套 DEFAULT，逐一覆盖四个字面量分配失败点。OPTIONAL 与未激活扩展不分配。结构/列表默认值、未解析非枚举引用和不支持的 REAL/OID 默认值明确诊断。
- 共享字段计划驱动有效标签、根/尾根存在位、版本组内部位、序号、控制字段及任意精度十进制选择键。实际 C 调用核对描述符、选择查找与版本组一致性；类型 ID 仅在当前模式内有效。
- SIZE 统一区分位、字节和元素，精确保留离散根区间与空的非扩展集合；扩展根允许根外值。未解析、负值和不可表示的 SIZE 在写流前失败，不能把字符串字节数当字符数。
- 命名使用带前缀、模块和类型的可逆转义，保留字段特定 `_view<TypeId>`；检查 C/C++ 关键字、前导/双下划线、大小写、连字符/下划线/转义外观碰撞、标准头遮蔽及重复类型/字段。四模块 32 记录类型和两个别名的合成 IR，经过严格 C11 双翻译单元编译、C 链接及 C++17 包含；重复实现必须链接失败。

语义 IR 保留位/字节字面量，共享字面量规范化同时服务 C/C++ DEFAULT。三种 C++ 形态验证省略默认值及修改位后的编码；asn1tools 0.166.0 独立确认 `cgen2_cpp_defaults.asn` 的 UPER 向量 `00`、`8000`，模式 SHA256 为 `a85a88236087634baa0936b1f315c39a1694aec949c7658dbe2c81aea989ec5b`。

最终 GCC 13.3.0 Release **466/466**、ASan+UBSan **466/466**、原 C++ 入口矩阵 **84/84**；保存的 448 个基线名称全部保留。安装后的纯 C、仅运行时、仅编译器和宿主生成消费者全部通过，纯 C 程序无 C++ 运行库依赖。MSVC 原生生命周期四项 **4/4**，修复 OID 容量检查顺序并显式转换 CHOICE 判别值后，最终筛选无枚举转换警告。日志位于忽略目录 `build-rev1-quick/cgen2-*`；最终文件包括 `cgen2-release-final-test.log`、`cgen2-san-final-test.log`、`cgen2-matrix.log`、`cgen2-msvc-test.log`。

<a id="cgen-validation--三次未完成尝试的历史"></a>

#### 三次未完成尝试的历史

| 尝试  | 已实现与当时缺口                                                           | 实际结果                                       |
| --- | ------------------------------------------------------------------ | ------------------------------------------ |
| 1   | 基本 C 类型、递归拥有关系、生命周期和部分共享规划；DEFAULT、其余布局及完整命名/标签/选择器验收未完成           | Release 22/22，ASan+UBSan 9/9；5 个本批用例       |
| 2   | 标量 DEFAULT、嵌套初始化、命名碰撞矩阵；CGEN-06 完成，拥有型 DEFAULT 和完整规划消费仍缺           | Release 25/25，ASan+UBSan 12/12；8 个本批用例     |
| 3   | OCTET STRING/OF 的 `storage_size(Type)` 与精确 SIZE 检查；CGEN-04/05 仍未完成 | 最终 Release 27/27，ASan+UBSan 14/14；10 个本批用例 |

第一次使用 WSL Ubuntu、GCC/G++ 13.3.0、CMake 3.28.3、Ninja、Python 3.12.3。沙箱内启动失败，授权后运行成功。测试样例为两个模块，包含递归记录、同名类型、别名、关键字/连字符字段、CHOICE、OF、字符串和离散整数；当时单头 4,652 字节、单源 16,725 字节，不据此作性能承诺。

第三次的 SIZE 用例实际复制长度 0～9 的字节串与集合，约束为 `SIZE(0..2 | 7..9)`，核对空洞拒绝、零长固定值、独立所有权及每个分配失败后的输出/存活数保持。无效模型测试验证双流未改写。第二次曾出现 shell 将含 `|` 的 CTest 正则拆开的错误，随后直接调用 CTest 重跑，错误调用不计为通过。

早期独立严格编译子进程只证明语言/链接，不冒充 Sanitizer 插桩；最终完整门禁中的子进程继承双 Sanitizer 参数。所有分配 hook 仅在测试包装中。各次尝试未删除基线、未改验收正文或受保护 runner/矩阵，也未启动后续批次。此批仅证明存储/生命周期，不证明完整字符域或 C PDU 互操作；枚举限制 int64、OID 限制 uint64、REAL 限制宿主 double。

<a id="cgen-validation--原始验收数据表"></a>

#### 原始验收数据表

| CTest 用例                                               | 实际检查                                                                                  |
| ------------------------------------------------------ | ------------------------------------------------------------------------------------- |
| CGenBatch2.UnsupportedLayoutsLeaveStreamsUntouched     | 不支持的 IR 布局拒绝生成，两个流均不改变                                                                |
| CGenBatch2.RejectsReservedPrefixAndUnresolvedReference | 拒绝非法/保留前缀和未解析类型                                                                       |
| CGenBatch2.ChoicePlanBorrowsTagsSelectorsAndOrdinals   | 实际共享计划的借用身份、标签、选择器和序号                                                                 |
| CGenBatch2.GeneratedLifecycle                          | 真实模式→生成 C→纯 C 程序：初始化/释放、深拷贝/自拷贝、CHOICE 替换、五个分配故障点、部分清理、跨模块递归、深度回滚、离散整数与非法判别值拒绝        |
| CGenBatch2.C11LinkAndDuplicateDefinition               | 以 `-std=c11 -pedantic-errors -Wall -Wextra -Werror` 独立编译生成 C；C 链接器链接运行双消费者，重复实现必须链接失败 |

<a id="cgen-validation--复跑命令-1"></a>

#### 复跑命令

```sh
cmake -S . -B build-rev1-release -DCMAKE_BUILD_TYPE=Release -DASN1_ENABLE_SANITIZERS=OFF -DASN1_BUILD_TESTS=ON -DASN1_BUILD_COMPILER=ON -DASN1_BUILD_RUNTIME=ON -DASN1_BUILD_C_RUNTIME=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-release -j1
ctest --test-dir build-rev1-release --output-on-failure --timeout 600
cmake -S . -B build-rev1-san -DCMAKE_BUILD_TYPE=Debug -DASN1_ENABLE_SANITIZERS=ON -DASN1_BUILD_TESTS=ON -DASN1_BUILD_COMPILER=ON -DASN1_BUILD_RUNTIME=ON -DASN1_BUILD_C_RUNTIME=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-rev1-san --output-on-failure --timeout 600
python3 scripts/validate_codec_matrix.py --compiler build-rev1-release/asn1cxx --cxx g++ --output build-rev1-quick/cgen2-matrix
python3 scripts/validate_installed_package.py --build build-rev1-release --output build-rev1-quick/cgen2-package
```

```sh
cmake -S . -B build-cgen2-stage -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/mnt/d/H/ACT/asn1-toolkit/build/_deps/googletest-src \
  -DASN1_BUILD_TESTS=ON
cmake -S . -B build-cgen2-san -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASN1_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/mnt/d/H/ACT/asn1-toolkit/build/_deps/googletest-src \
  -DASN1_BUILD_TESTS=ON
cmake --build build-cgen2-stage --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
cmake --build build-cgen2-san --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
ctest --test-dir build-cgen2-stage \
  -R 'CGenBatch2|EmissionPlanning|EmitCache|CGenBatch1.Api|Architecture' --output-on-failure
ctest --test-dir build-cgen2-san \
  -R 'CGenBatch2|EmissionPlanning|CGenBatch1.Api' --output-on-failure
git diff --check
```

```powershell
wsl --exec cmake --build /mnt/d/H/ACT/asn1-toolkit/build-cgen2-stage --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
wsl --exec cmake --build /mnt/d/H/ACT/asn1-toolkit/build-cgen2-san --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
wsl --exec ctest --test-dir /mnt/d/H/ACT/asn1-toolkit/build-cgen2-stage -R 'CGenBatch2|EmissionPlanning|EmitCache|CGenBatch1.Api|Architecture' --output-on-failure
wsl --exec ctest --test-dir /mnt/d/H/ACT/asn1-toolkit/build-cgen2-san -R 'CGenBatch2|EmissionPlanning|CGenBatch1.Api' --output-on-failure
git diff --check
```

<a id="cgen-validation--batch-3"></a>
<a id="cgen-validation--batch-3--实际实现"></a>
<a id="cgen-validation--batch-3--检查与当前证据"></a>
<a id="cgen-validation--batch-3--可复跑命令"></a>
<a id="cgen-validation--batch-3--实测产物资源前缀-stage3"></a>
<a id="cgen-validation--batch-3--未完成范围"></a>

<a id="cgen-validation--第-3-批验收"></a>

### 第 3 批验收

日期：2026-10-24。**CGEN-07～09 在显式仅类型/生命周期范围内完成**；普通 C 编解码请求在当时仍拒绝。最终 GCC Release **473/473**、ASan+UBSan **473/473**，原 C++ 入口语法矩阵 **84/84**；466 个基线全部保留，新增 7 项。四类安装消费者通过，纯 C 无 C++ 运行库依赖。

实现包括 `--lang c --types-only`、统一调度、稳定 SCC 分片与超预算警告、模块归属、完整公共头及带前缀的跨片深度辅助声明。单头所有函数使用 `static inline`；普通、模块和模块内分片复用同一 stage/backup/manifest 发布事务，并输出 JSON/CMake 当前源清单和保守的逐文件校验缓存。

七个 `CGenBatch3.*` 用例覆盖形态组合、单模块 40 类型预算从 8 改 13 后实际源数由 5 改 4、整份 RRC 类型产物、发布失败保护、缓存损坏/禁用/命中/失效、选项/大小写/SCC 负例及纯 C CMake 消费。五种形态验证拥有型 DEFAULT、递归独立拷贝、自拷贝、分配失败与双翻译单元；用现有 C PER 原语编码生成的标量 DEFAULT 得到 `87`，这是原语集成结果，不能称为生成 PDU 编解码。

首次 Sanitizer 运行的两个新检查将 GCC UBSan 间接引入的 libstdc++ 误判为产品泄漏；`ldd` 核实后，将依赖检查限定到非插桩产品，仍保留严格 C11 和双 Sanitizer 执行。修正前失败日志保留在 `cgen3-san-tests.log`，最终全量重跑 473/473，不通过修改生产代码掩盖失败。

MSVC Release 相关 **8/8**，额外纯 C 双翻译单元单头消费者 **1/1**；当时仍有测试测试驱动的枚举转换警告，因此不宣称零警告。当时 WSL 未安装 Clang，未执行的门禁不计为通过。

以下资源表为该主机 GNU 严格 C11 编译/链接观测值，峰值为 GNU time 的子进程最大 RSS（KiB），含测试驱动和链接器；不是目标 PDU 内存、整个构建内存或性能保证。源数/字节由生成器记录，函数数由实际定义独立核对。单头零 C 源不等于零代码；软类型预算不是硬字节上限，公共头仍未拆分，也不能拿仅类型结果与完整 C++ 编解码作性能比较。

两个构建均开启编译器、两种运行时和测试，使用离线 `build/_deps/googletest-src`；Release 不插桩，Debug 使用 ASan+UBSan。日志位于 `build-rev1-quick/cgen3-*`；`tests/cgen3-shapes`、`tests/cgen3-rrc_types` 保存 `metrics.json`。当时的 PDU 编解码、累计预算、独立外部对照工具、C 7×12 及最终 SDK/交叉工具链验收归后续批次；整份 LTE RRC 类型构建不证明所有值或 PDU 的线路行为。

<a id="cgen-validation--原始验收数据表-1"></a>

#### 原始验收数据表

| 形态            | C 源数 | 最大源字节数 | 头字节数   | 最大源函数数 | 编译链接秒数 | 子进程峰值 RSS（KiB） |
| ------------- | ----:| ------:| ------:| ------:| ------:| --------------:|
| outline       | 1    | 96040  | 19924  | 288    | 0.643  | 36224          |
| inline        | 0    | 0      | 129620 | 0      | 0.496  | 28160          |
| modules       | 2    | 91355  | 19924  | 270    | 0.701  | 35584          |
| shards        | 4    | 27572  | 19924  | 72     | 0.823  | 27472          |
| module_shards | 5    | 27572  | 19924  | 72     | 0.892  | 27188          |

| 构建         | IR 类型数 | C 源数 | 最大源字节数 | 最大源函数数 | 编译链接秒数 | 子进程峰值 RSS（KiB） |
| ---------- | ------:| ----:| ------:| ------:| ------:| --------------:|
| Release    | 1037   | 9    | 714307 | 1152   | 10.059 | 93616          |
| ASan+UBSan | 1037   | 9    | 714307 | 1152   | 19.777 | 190400         |

<a id="cgen-validation--复跑命令-2"></a>

#### 复跑命令

```sh
cmake --build build-rev1-release -j1
ctest --test-dir build-rev1-release --output-on-failure --timeout 600
cmake --build build-rev1-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-rev1-san --output-on-failure --timeout 600
python3 scripts/validate_codec_matrix.py --compiler build-rev1-release/asn1cxx --cxx g++ --output build-rev1-quick/cgen3-matrix
python3 scripts/validate_installed_package.py --build build-rev1-release --output build-rev1-quick/cgen3-package
```

<a id="cgen-validation--batch-4"></a>

<a id="cgen-validation--第-4-批验收"></a>

### 第 4 批验收

日期：2026-10-26。**第 4 批首个 C11 UPER/APER 支持范围已完成；CGEN-10..12 已按
本批声明范围关闭。此结论不代表实现了 X.691 全部值域或所有 ASN.1 PER 结构。**

<a id="cgen-validation--batch-4--已完成的增量"></a>

<a id="cgen-validation--已完成的增量"></a>

#### 已完成的增量

- C PER 共享核心新增 normally-small non-negative whole number 与
  ENUMERATED 根/扩展 ordinal 编解码接口，UPER/APER 由同一
  `asn1_per_variant_t` 参数化实现。
- 规范复核发现 C++ PER reference core 的 aligned length determinant 没有
  执行 X.691 10.9 要求的 octet alignment。已补齐，并新增 C/C++ 两变体逐字节
  一致性测试。normally-small 大值采用半约束非负整数形式；标准条文见
  [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en)，
  10.6–10.9。
- C 运行时正确性测试 test 覆盖 normally-small `0, 1, 63, 64, 127, 255,
  256, 2^32, UINT64_MAX`，根 ENUMERATED、扩展 ordinal 和非法参数。
- OID/RELATIVE-OID 原语将 BER 内容按 base-128 子标识符编码，采用 X.691 23 的半约束字节数及 APER 对齐；验证首弧/根、uint64 溢出、非最短表示和截断，输出参数事务提交。绝对 OID `1.2.840.113549` 在 PER 封装前冻结为 `06 2A 86 48 86 F7 0D`。

<a id="cgen-validation--batch-4--本次验证"></a>

<a id="cgen-validation--本次验证"></a>

#### 本次验证

环境：Windows 11 / Visual Studio 2022（MSVC Release）及 Ubuntu 24.04 / GCC（WSL 2）。

```powershell
cmake --build build-rev4-msvc --config Release --target asn1_per_tests asn1_c_per_correctness -- /m:1
ctest --test-dir build-rev4-msvc -C Release -R '^(PerPrimitives.CNormallySmallMatchesCppForBothVariants|PerPrimitives.CEnumeratedOrdinalsMatchCppForBothVariants|PerPrimitives.COidMatchesCppForBothVariants|Asn1CPerCorrectness|CGenBatch4.PerPrimitiveCore)$' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^(PerPrimitives\.)' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^CGenBatch4\.' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^(Asn1CPerLargeValues|Asn1CRuntimeFailureContracts|Asn1CRuntimeSmoke)$' --output-on-failure
```

结果：三项 C/C++ UPER+APER 一致性、C PER 正确性测试和 batch stage case **5/5**；
PER primitive suite **12/12**；`CGenBatch4.PerPrimitiveCore` **1/1**。最终包含 44 项
相关 PER/APER/UPER/primitive 验收的回归轮次 **44/44** 通过。C 运行时冒烟测试、
allocator 失败测试 contracts 与 128 KiB large-value target 也通过 **3/3**。

随后新增的 C SEQUENCE/SET presence 前导位图原语单独在 MSVC 下通过 C/C++ 一致性、
C 正确性测试和 allocator-failure 测试 **3/3**；GCC Release 对应测试 **3/3**，
ASan+UBSan 对应测试 **3/3**。新增 API 验证扩展位、OPTIONAL/DEFAULT bitmap、非法位
预检查、截断输出保持和分配失败清理。

此前普通桌面进程启动 WSL 返回 `E_ACCESSDENIED`；提升 Windows 进程权限后，Ubuntu
24.04 可正常启动。复用 Linux 构建目录并重编译 C/C++ PER targets 后，GCC Release 的
UPER/APER 回归 **52/52**、PER primitive/oracle/C 运行时回归 **20/20** 通过。
ASan+UBSan 下相关 PER/APER、golden、代码生成、C 运行时回归 **67/67** 通过。此处 sanitizer
是相关目标回归，不代表全仓库 sanitizer suite 或 Clang/MSVC 覆盖。

<a id="cgen-validation--batch-4--早期阶段缺口快照已由后续章节更新"></a>

<a id="cgen-validation--早期阶段缺口快照已由后续章节更新"></a>

#### 早期阶段缺口快照（已由后续章节更新）

以下记录反映 2026-10-24 当天较早阶段的状态快照；后续章节已推进其中若干项。
当前实现状态以本文件末尾按时间排序的后续实现章节为准。

CGEN-10 仍缺生成侧 REAL、restricted character-string 支持范围和命名位语义，以及更完整的值域/约束拒绝矩阵。已支持 BIT STRING（不含命名位串）、OCTET STRING 和 uint64-arc OID/RELATIVE-OID。CGEN-11 已支持根 CHOICE、SEQUENCE OF/SET OF、非递归且无扩展的 SEQUENCE/SET；递归结构、尾根、扩展单字段/版本组和开放类型仍未实现。CGEN-12 覆盖截断、非法 BOOLEAN/OPTIONAL 标志、INTEGER 越界和前导位图 OOM；集合解码分配故障注入、累计资源预算和完整 reader-cursor 失败契约仍缺。其他 codec/layout 诊断拒绝。未运行 asn1c/asn1tools C PDU 双向对验；自有实现互验不冒充外部对照工具。

因此 TODO 在该早期快照仍保持 CGEN-10..12 `[ ]`；后续章节记录了支持范围完成和
剩余标准覆盖边界。

<a id="cgen-validation--batch-4--2026-10-24-后续实现递归扩展open-typereal-与字符串"></a>

<a id="cgen-validation--2026-10-24-后续实现递归扩展open-typereal-与字符串"></a>

#### 2026-10-24 后续实现：递归、扩展、开放类型、REAL 与字符串

- C PER 递归遍历复用生成的 `*_at(..., depth)` 辅助函数，允许经 `SEQUENCE OF`
  形成的递归值；生成解码为间接成员分配对象，生命周期沿同一指针图释放。
- C 运行时增加 X.691 extension-addition bitmap 的 encode/decode/free；生成器消费
  普通单字段扩展和 `[[ ... ]]` 版本组。版本组占一个外层扩展位，其开放类型
  内容按 X.691 19.9 带嵌套 SEQUENCE presence 前导位图。未知尾部扩展项被解码并丢弃。
- `CLASS.&Value` 的 C 布局保留 complete-encoding octets；生成字段以 PER
  开放类型 length/alignment 封装和解封。table 选择器仍是调用方通过
  `*_select` 查询的接口；生成器尚未依据选择器自动选择并递归解码具体值类型。
- REAL 新增 PER 编码：BER REAL content 形式的 binary64、±Infinity、NaN、负零；解码
  支持这些二进制值及 NR1/NR2/NR3 十进制子集。十进制值用 host `double` 表示，最多
  20 位输入有效数字；这不是精确十进制 REAL 模型，也不覆盖整个标准值域。
- restricted string 新增 NumericString、PrintableString、VisibleString、IA5String、
  BMPString、UniversalString 的 UTF-8 内存到 PER 字符码路径；另有 NumericString 和
  IA5String 生成 PDU 双变体往返。大于 16383 字符的通用分片字符串、FULL 字符域
  及全部 SIZE/字符约束组合仍未封闭。
- `asn1_per_encode_open_type` 现在接受零 octet 内容，以支持空编码值（例如 PER NULL）；
  新增 generated 开放类型测试样例覆盖 telecom `HandoverIE` 不透明内容与选择器
  lookup。内容的内部类型正确性由调用方负责。

验证命令：

```powershell
cmake --build build-rev4-msvc --config Release --target asn1_cgen_batch4_per_tests asn1_cgen_open_type_tests asn1_cgen_batch4_per_inline_tests asn1_cgen_batch4_per_split_tests asn1_cgen_batch4_per_failure_tests asn1_c_per_correctness -- /m:1
ctest --test-dir build-rev4-msvc -C Release -R '^(CGenBatch4\..*|Asn1CPerCorrectness)$' --output-on-failure
```

MSVC Release 相关门禁 **7/7** 通过。Ubuntu 24.04 GCC ASan+UBSan 相关门禁 **7/7**
通过；GCC 因 `powl` 需要 `libm`，已在 C 运行时和直接编译运行时的测试目标中
添加条件链接。此次没有重新跑全量 sanitizer suite，也没有 Clang。`asn1c` /
`asn1tools` 未用于本次 C PDU 双向差分；这些向量和 C/C++ 以外的独立外部对照工具
在该阶段仍是关闭 CGEN-10..12 的阻断项；最新完成范围见本文件末尾。

当时仍保持未完成：CHOICE 扩展、尾根、选择器驱动的类型化开放字段解码、完整
restricted-string 分片/字符域、完整 REAL 标准值域、extension/recursive 解码的全分配
故障矩阵和读游标契约。CHOICE 扩展、尾根与类型化开放字段解码已在本文末尾
后续章节完成；其余全标准值域和故障矩阵限制已列为明确的支持范围边界。

<a id="cgen-validation--batch-4--2026-10-24-bmpstring-边界修正复验"></a>

<a id="cgen-validation--2026-10-24-bmpstring-边界修正复验"></a>

#### 2026-10-24 BMPString 边界修正复验

实现审查发现 BMPString 不允许以 UTF-16 surrogate pair 表示补充平面字符；现已拒绝
补充平面输入、拒绝线路 surrogate，并确认失败不覆写解码输出。UPER/APER
各自覆盖有效 BMP 字符和无效边界。MSVC Release 复验：`PerPrimitives.PrintableStringIncludesPeriod`、
`Uper.RealRoundTripAndVector`、`Uper.SequenceExtensionAdditions`、C 运行时 PER 正确性测试、
inline/out-of-line/split PDU、不透明开放类型、生成解码分配失败及大型值门禁共 **10/10**。
`git diff --check` 通过。历史 GCC ASan+UBSan 7/7 是此 BMP 修正之前的结果；本环境
无法复用该目录，因为它的 CMake cache 指向 `/mnt/d/...`，当前 Windows 路径下不匹配。
本次没有新的 sanitizer 结果，也没有 `asn1c` / `asn1tools` 独立对验。

<a id="cgen-validation--batch-4--2026-10-24-集合分片累计预算与故障注入"></a>

<a id="cgen-validation--2026-10-24-集合分片累计预算与故障注入"></a>

#### 2026-10-24 集合分片、累计预算与故障注入

- X.691 OF 长度现在按分片 determinant 与各段元素交错编码/解码。精确 16K
  边界会读写末尾零长度 determinant；生成 `LargeFlags ::= SEQUENCE (SIZE
  (0..70000)) OF BOOLEAN` 覆盖 16384 与 16385 个元素，UPER/APER 往返，并在
  out-of-line、inline、split 三种消费者中对 16385 元素逐字节比对冻结向量。
- 已在 Ubuntu 24.04 / asn1c v0.9.29 现场用 `Frag` 与 16385 个交替 BOOLEAN
  生成 UPER；2051 字节与冻结向量完全一致。asn1c v0.9.29 提供 UPER，本次没有
  把它说成 APER 外部对照工具。
- 每个顶层生成解码重置读取器预算；递归字段和嵌套开放类型读取器共用
  同一预算。默认累计上限为 16 MiB 申请字节和 65536 个元素，可用
  `ASN1_MAX_PDU_ALLOCATION_BYTES` / `ASN1_MAX_PDU_ELEMENTS` 覆盖。两份各自低于
  上限的 SEQUENCE OF 合计超 byte/element 上限时均返回 `RESOURCE_LIMIT`，且不改
  调用方输出。
- 生成代码的 allocator seam 只在 `ASN1_C_RUNTIME_TEST_ALLOCATOR` 下接入测试 hook。
  16385 个元素触发两次集合扩容，分别注入第 1、2 个分配失败；两次均返回
  `OUT_OF_MEMORY`、保留原输出并释放临时集合。成功路径确认恰好经过两个扩容点。

MSVC Release **14/14** 相关测试通过；Ubuntu 24.04 GCC ASan+UBSan **11/11** 通过；
asn1c UPER 分片向量匹配通过。`git diff --check` 通过。该阶段剩余限制包括 CHOICE 扩展、
尾根、自动类型化开放字段 dispatch、全部 REAL/字符值域和完整 reader-cursor
故障契约；前三项已由本文末尾后续章节补齐。该阶段证据不能单独作为批次完成结论。

<a id="cgen-validation--batch-4--2026-10-24-后续实现扩展-choicetrailing-root-与-typed-open-type"></a>

<a id="cgen-validation--2026-10-24-后续实现扩展-choicetrailing-root-与-typed-open-type"></a>

#### 2026-10-24 后续实现：扩展 CHOICE、尾根与类型化开放类型

- 扩展 CHOICE 使用 X.691 扩展位、normally-small 扩展 ordinal 和开放类型
  完整编码；已知扩展可由 UPER/APER 生成 C 自动编解码。未知 ordinal 返回
  `UNSUPPORTED`，不丢弃或伪造一个不可表示的 choice 值。
- SEQUENCE/SET 的尾根字段进入合并的根 OPTIONAL/DEFAULT presence bitmap，
  并按 PER root-component 顺序编解码。X.691 19.9 注 2 明确要求第二个扩展标记后的
  根 component 按紧邻扩展标记对之前的方式编码。
- 对表约束 `&Value` 字段，包含对象集已知键的生成 PDU 解码自动保留 raw
  complete-encoding octets 并解出匹配的 typed value；结构提供 `decoded_type_f...` /
  `decoded_value_f...`，生成 copy/free 维护拥有关系。编码优先使用类型视图，按 type
  ID 编码并校验控制字段 ID；没有类型视图时仍可转发 raw octets。单字段解码
  辅助函数也可由调用方使用。子读取器共享顶层累计预算。
- 上述类型布局更新生成 ABI 标记至版本 2。CMake 测试现对 telecom table-constraint
  测试样例同时生成 UPER/APER，覆盖自动 typed 解码、copy/free、typed 修改后的重编码、
  控制字段不匹配事务性、扩展 CHOICE 未知 ordinal 错误，以及尾根存在/缺失
  扩展的组合。

验证：MSVC Release **8/8**，Ubuntu 24.04 GCC ASan+UBSan **8/8**。独立 asn1c v0.9.29
UPER encoder 生成了 CHOICE 扩展 INTEGER/IA5String、尾根有/无扩展
四个向量；C 生成 encoder 必须逐字节匹配这些向量，生成 decoder 再解码同一线路。
该 asn1c 版本不提供 APER 外部对照工具；APER 由 C/C++ 共享核心一致性与生成 PDU 测试覆盖。
`asn1tools` 当前未安装。CGenBatch3 RRC 仅类型 C11 生成/编译/链接验收 **1/1**。
覆盖到的 PDU 形状已有 UPER 双向对验；独立外部对照工具目前覆盖受支持支持范围的分片、
CHOICE 扩展和尾根代表向量，其他结构主要由 C/C++ 一致性与生成运行
测试覆盖。asn1tools 未安装；未声称覆盖完整 X.691 值域。
实现依据：[ITU-T X.691 (02/2021), 19.9](https://www.itu.int/rec/T-REC-X.691-202102-I/en)。

本轮已将生成 PDU 子集扩展到 BOOLEAN、NULL、INTEGER、ENUMERATED、BIT STRING、OCTET STRING、OID/RELATIVE-OID、根 CHOICE、SEQUENCE OF/SET OF，以及无扩展非递归 SEQUENCE/SET 的 OPTIONAL/DEFAULT。out-of-line、inline、split 三种产物均由 C11 消费程序编译、链接并运行；用例检查标量和组合类型往返、BOOLEAN/OPTIONAL 非法值、INTEGER 越界、截断时输出保持及前导位图分配失败。固定向量 B6 和新增组合值用例不是独立外部对照工具。MSVC Release 生成消费者 4/4；GCC Release 与 ASan+UBSan 的生成消费者各 4/4；含 primitive core 与 CLI dispatch 的筛选各 6/6。

<a id="cgen-validation--batch-5"></a>
<a id="cgen-validation--batch-5--implementation-and-contracts"></a>
<a id="cgen-validation--batch-5--executable-evidence"></a>
<a id="cgen-validation--batch-5--explicit-telecom-blockers-and-unsupported-inputs"></a>
<a id="cgen-validation--batch-5--final-gates-and-environment"></a>

<a id="cgen-validation--第-5-批验收"></a>

### 第 5 批验收

日期：2026-10-26。范围为 CGEN-13～15 已登记的 C 值域，**不宣称覆盖整个 X.690**。

<a id="cgen-validation--实现与契约"></a>

#### 实现与契约

`byte_io.h` 提供有界读写，`tlv.h` 解析完整定长/不定长信封、嵌套及 EOC，`tlv_schema.h` 消费生成的 C 存储元数据。递归获取函数支持三种输出形态；编译器不链接运行时，运行时不含 AST/IR。BER/DER/CER 共用按次调用策略，PER 仅通过 `value_content.c` 共享 REAL/OID 内容，不依赖 TLV 模式引擎。

支持 BOOLEAN、NULL、int64/BigInt、枚举、位/字节串、uint64 弧 OID、宿主 double REAL、UTF8/IA5/Numeric/Printable/VisibleString，以及递归 SEQUENCE/SET/CHOICE/OF、OPTIONAL、已支持的标量/位/字节/字符串 DEFAULT、扩展/版本组和完整 TLV 开放字段。UTF8String 的 SIZE 按字符数校验。缺席时构造拥有型 DEFAULT，失败释放部分值。

规范检查包括最短头/整数、DER 定长与 CER 构造型不定长、DEFAULT 省略、DER 实际标签 SET 排序与 CER 无标签 CHOICE 的静态最小标签排序、SET OF 字典序、REAL 二进制归一化、尾部未用位及 1000 内容字节 CER 分片。受限字符串分片的子标签是 OCTET STRING（X.690 8.23.3）；自动标记无标签 CHOICE 时包裹所选完整 TLV，不能覆盖分支标签。

生成的公开解码入口成功才替换已初始化值并消费一个 TLV，失败保持值及读游标。默认输入/累计申请有效载荷上限 16 MiB、65536 元素、深度 256、字符串 64 KiB、BigInt 1024 个 32 位 limb。字符串先验证并记账，再精确分配；集合、间接节点、整数、OID 和 DEFAULT 在申请前预留预算；不将预算宣传为存活堆或 CPU 配额。

`asn1_ber_tag_t.number` 扩为 uint64，修复高于 32 位标签截断，需重编译运行时与消费者；当时生成值 ABI 为 2，当前为 3。

<a id="cgen-validation--实际执行证据"></a>

#### 实际执行证据

- `CGenBatch5.TlvCore`：UINT64_MAX 高标签、最短长度、不定长嵌套/EOC、超过 128 位有符号整数、构造字符串、CER 边界/截断、深度和游标失败。
- 三形态生成消费者：真实 PDU、显式/隐式及 CHOICE 标签、DEFAULT、省略/还原、集合排序、REAL/OID、UTF8 字符 SIZE、CER 和重复/未知扩展。
- `AllocationFailures` 逐次失败拥有型 DEFAULT、SET OF、递归集合和 CER 的所有编解码分配，检查无泄漏和输出/游标/writer 保持；`ResourceLimits` 用 96 字节、8 元素预算验证累计限制。
- `EntryShapeMatrix` 独立编译运行 21 个 C11/C++17 消费者，覆盖 UPER 单入口、BER 单入口、组合、三形态和模块内分片，并比较实际字节、执行解码。
- `WholeRrc` 先构建代表切片，再构建完整 LTE RRC 8.6.0 的 1037 类型、9 个 C 源，最大源文件 1,601,238 字节；对 RRCConnectionRequest 做 UPER/BER 实际编解码，不宣称所有 RRC PDU 已认证。

整份模式 SHA256 为 `53be8d24536eff9084361af881329fd7c49fdb658ceda84f605197c65adb5895`。Ubuntu 24.04 的 `/usr/local/bin/asn1c` v0.9.29 使用 `-gen-PER -fcompound-names` 编译同一全文，独立编码后比较字节，并解码项目输出；项目解码同一匹配字节。记录的外部对照工具源 hash 为 `037f317dc3652e7718853457fc406acebc96dc5ab9a7c4fb7875a46324146956`；脚本 `evidence.json` 保存版本、源/模式 hash、数量、向量和插桩配置。CER 只有条文推导和负例，不冒充外部差分。

<a id="cgen-validation--当时明确的阻断与边界"></a>

#### 当时明确的阻断与边界

第 5 批验收允许补齐 LPP/S1AP 全文或列出阻断；当时两者不算生成成功。LPP 的 UTCTime（含 `locationTimestamp-r13`、`measurementReferenceTime-r13`）、命名位串，以及 S1AP 的扩展离散根/表驱动类型解码尚缺；这些指定项后来由第 7 批补齐。

其余边界继续明确：未登记字符种类、CONTAINING、不支持的 DEFAULT 和未降级类型在发布前拒绝；REAL 是 binary64/部分十进制输入，DER/CER 抽象十进制不能通过 double 保真，返回 Unsupported。未知开放 TLV 缺少模式时不能推断其规范值语义；BER/OER 不默默复用 PER 类型视图，仅有类型视图时拒绝。

<a id="cgen-validation--最终环境与门禁"></a>

#### 最终环境与门禁

完整 MSVC Release **461/461**；Ubuntu 24.04 GCC 13.3 ASan+UBSan 的 80 个可执行消费者和全量测试 **494/494**；原 C++ 7×12 **84/84**；入口/形态消费者 **21/21**。整份 RRC 与现场 asn1c 对验、非插桩纯 C11 依赖检查、C++17 包含 C 单头、WSL Python/PyYAML skill 校验均通过。纯 C 仅依赖 libc/libm；全部可执行测试保持插桩，大型不执行的语法矩阵另行完整检查。

当时 MSBuild 使用 `/m:1`，避免多个历史项目重入共享生成目录而触发发布锁；后续修复调度，未禁用锁。测试执行可双作业。独立 Unix 消费者补 libm，导出目标已传递 libm。Clang、asn1tools 当时未安装，未计为通过；缺工具未阻断本批登记范围，后续第 6/7 批补齐对应验证。

日志：`build-rev4-msvc/cgen5-final-full-{build,test}.log`、`build-rev5-wsl-san/final-all-tests.log`、`build-rev5-wsl-san/matrix-84.log`、`build-rev5-wsl-san/tests/cgen5-{matrix,rrc-acceptance}/evidence.json`。标准依据为 [X.690 (2021)](https://www.itu.int/rec/T-REC-X.690-202102-I/en) 8.1、8.23、9～11。

<a id="cgen-validation--原始验收数据表-2"></a>

#### 原始验收数据表

| 编码规则                          | 冻结向量                                             |
| ----------------------------- | ------------------------------------------------ |
| UPER                          | `404080c10158`                                   |
| BER（asn1c DER 编码器输出合法 BER 表示） | `3015a013a011a008810600010203040581010382020700` |

| 门禁   | 结果                |
| ---- | ----------------- |
| 完整门禁 | 结果及环境见本批“最终环境与门禁” |

<a id="cgen-validation--batch-6"></a>

<a id="cgen-validation--第-6-批验收"></a>

### 第 6 批验收

<a id="cgen-validation--batch-6--实现与边界"></a>

<a id="cgen-validation--实现与边界"></a>

#### 实现与边界

CGEN-16..19 增加 C11 的七个生成入口：`oer/coer/jer/jeri/xer/cxer/exer`。
已有 BER/DER/CER/UPER/APER 保留；`per` 仍是 UPER+APER 别名。
所有入口通过 `asn1::c_runtime` 消费，编译器仍只输出 C 源文件/元数据，
不链接运行时。元数据只含 C 存储布局、字符串、数值和递归获取函数，
不携带 AST/IR。三形态使用相同 schema，单头采用 static inline。

OER 的长度、整数、ENUMERATED、BOOLEAN、tag、open 内容在 `oer.c`；
结构和策略在 `schema_oer.c`。COER 选择独立调用策略，不读取生成时全局 mask。
支持 native/BigInt 精确整数、NULL、位/字节串、受限字符串、host-double REAL（含约束 IEEE binary32/binary64，拒绝精度损失）、
uint64 arc OID、SEQUENCE/SET/CHOICE/OF、扩展添加、版本组与不透明 open 内容。
共享 `codegen/plan.hpp` 决定 OER SET 根顺序；untagged CHOICE 的最小标签只看根，
不让扩展标签改变根排序。嵌套无标签 CHOICE 按 X.696 20.1 重复选择标签。
UTF8String 长度始终按编码字节传输，不把固定字符数当作固定字节数。无约束 REAL 即使在 BASIC-OER 也校验 DER contents（X.696 12.4）；
抽象十进制 REAL 返回 Unsupported，不用 host-double 改写其抽象基数。

`text_tree.c` 实现完整输入的 JSON/XML 树解析。JSON 保留十进制 token，
不经 double 中转 INTEGER；支持 UTF-8、surrogate pair、转义和字符串内 NUL，
拒绝重复键、非法数字和不完整文档。XML 支持 UTF-8、XML 1.0 声明、实体引用、
CDATA、注释、属性、空元素和行尾规范化；DTD/外部实体不支持，绝不展开。
树节点记录字符控制元素在混合内容中的位置，恢复 `a<nul/>b` 的顺序。

`schema_json.c` 实现值域和结构解码、DEFAULT/OPTIONAL、未知扩展策略，
以及 ARRAY、BASE64、NAME、OBJECT（含 ENUMERATED 键）、TEXT、UNWRAPPED。
ARRAY 根组件（包括尾根）在扩展组件之前；nullable optional/extension
的 ARRAY 定义提前诊断。UNWRAPPED 解码试探备选，拒绝歧义；编码也验证可唯一还原。
JER/JERI 共用 X.697 指令语义，不声明 canonical JSON。

`schema_xml.c` 实现 BASIC-XER、CXER、E-XER。CXER 使用规范空元素、
SET/SET OF 顺序、REAL 指数形式，并按 X.693 9.5/9.6 显式输出 DEFAULT。
严格解码在值验证后复编码核对规范字节。E-XER 实现 ATTRIBUTE、BASE64、NAME、TEXT、
USE-NUMBER、LIST 和受限 USE-NIL；共享 NAME 同时作用于 JER/E-XER；BASIC-XER/CXER 忽略扩展 NAME 映射。
UNTAGGED 暂不支持，生成前明确 Unsupported；不替换成 BASIC-XER。

这批完成的是上述值域，**不是整部标准认证**。继续拒绝命名 BIT STRING、
CONTAINING、BMP/Universal/时间等未进入 C schema 支持范围的类型图。
REAL 为 host-double，enum 为 int64，OID arc 为 uint64。
JSON/XML 的自动类型化开放类型映射尚未进入支持范围，生成前拒绝；
OER 不透明 open 内容可用，未知 CHOICE 扩展返回 Unsupported。
未知 SEQUENCE 扩展可跳过；CXER 未保留的未知扩展明确 Unsupported，避免错误的规范性结论。
E-XER 组件 NAME 限定 ASCII XML 名，非法/带 namespace 的映射在发布前拒绝。
XML 的 namespace/DTD/任意指令组合、完整 REAL 十进制模型与全部字符集仍不支持。

<a id="cgen-validation--batch-6--错误与资源契约"></a>

<a id="cgen-validation--错误与资源契约"></a>

#### 错误与资源契约

生成入口使用 `asn1_byte_writer_t*` / `asn1_byte_reader_t*`，文本也传 UTF-8 字节。
解码输出必须先初始化；成功替换旧值并提交游标，失败输出/cursor 不变。
编码在临时写入器中完成，成功才追加，失败不留下半个 PDU。
原语仍遵守逐 API 契约；BigInt 解码目标需为空初始化对象。
OOM、资源限制、截断、约束、非规范与 Unsupported 保留不同状态。
文本解析 API 另外提供字节错误位置；生成入口目前不提供完整字段路径。

使用既有 limits：64 KiB 字符串、1024 BigInt limbs、深度 256、
65536 节点/元素、16 MiB 累计分配。树与值的累计预算一起核算，
OER 整数按实际内容长度核算，不能按剩余 PDU 长度对每个 scalar 重复收费。
OER shadow 读取器的输入副本纳入预算，OER/XML 集合排序暂存字节也设累计上限。
故障注入使用私有 allocator seam，生产库使用 libc；无第三方运行时依赖。

<a id="cgen-validation--batch-6--可复现门禁"></a>

<a id="cgen-validation--可复现门禁"></a>

#### 可复现门禁

- `CGenBatch6.TextSyntax`：词法、Unicode、实体、非法输入和逐字节截断。
- `CGenBatch6.Generated*`：七入口 PDU 实际线路、三形态、截断回滚、值域失败。
- `CGenBatch6.jerInstructions/exerInstructions`：实际 C 消费者，包括组合掩码下指令隔离。
- `CGenBatch6.StandardBoundaries`：版本组、扩展 CHOICE、SET 根最小标签、
  嵌套 CHOICE、CXER REAL/控制字符、非规范拒绝、uint64 tag 与整数溢出。
- `CGenBatch6.AllocationFailures`：逐次失败所有 encode/decode 分配，检查泄漏和事务性。
- `CGenBatch6.ResourceLimits`：缩小预算验证累计元素、树和字符串上限。
- `CGenBatch6.UnsupportedInstructions`：不支持的图在发布前失败，保留目录原文件。
- `CGenBatch6.EntryShapeMatrix`：39 个纯 C11 消费者，12 单入口及全组合，三形态；
  比较每一条实际线路并执行解码，不以 syntax-only 代替运行。

独立验证脚本 `tests/codegen/cgen_batch6_oracle.py` 使用测试环境的 asn1tools，
不属于产品依赖。运行时传 `--driver <asn1_cgen_batch6_oracle_driver>`、
`--root <repo>`、`--evidence <json>`。asn1tools 0.169.0 已执行 72 组
OER/JER/XER 双向交换，包括 >128 位正负整数、Unicode、BIT/OCTET STRING、
SET OF、CHOICE、DEFAULT、OPTIONAL、REAL、OID、结构/字符串集合，
以及 BOOLEAN/ENUMERATED OF 的直接 XML 元素形式。
该版本会展开扩展 addition groups，OER 位图与 JSON/XML 缺席组不能当作外部对照工具；
该工具也不能为含无标签 CHOICE 的 SET 排序；相关声明在独立外部对照工具中隔离。
这些例外写入 evidence 的 oracle_gaps，由标准冻结向量单独覆盖。
72 组独立输入/规范化结果保存在 `tests/standards/cgen6-asn1tools-vectors.json`，
`CGenBatch6.FrozenOracleReplay` 无需安装外部对照工具即可回放两种输入。
asn1tools OER/XER 不能算独立 COER/CXER 规范外部对照工具。
已有 asn1c 0.9.29 RRC UPER/BER 验收继续回归，不混计为新增家族验证。

最终门禁：MSVC Release **472/472**、Ubuntu GCC 13.3 ASan+UBSan **506/506**、
原 C++ 7 测试样例×12 入口独立语法门禁 **84/84**。全部测试可执行消费者已重新构建；
C 7×12 最终语法矩阵仍归第 7 批，不与本批 39 个运行消费者混计。
公共头：49 个 C++17 头及 25 个 C 头（各 C11/C++17）独立严格编译通过；
分层检查与仓库 skill 校验通过。
Clang 在当前 WSL 未安装，不将其登记为通过；最终多编译器/安装门禁仍属于第 7 批。
非 Sanitizer 的仅运行时构建也完成 39 消费者矩阵，`ldd` 只显示 libc、libm 和系统 loader，
不依赖 libstdc++。

<a id="cgen-validation--batch-6--标准依据"></a>

<a id="cgen-validation--标准依据"></a>

#### 标准依据

- [ITU-T X.696 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.696-202102-I!!PDF-E&lang=e&type=items)：8、16–20、27、31。
- [ITU-T X.697 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.697-202102-I!!PDF-E&lang=e&type=items)：14、17、24、27、31。
- [ITU-T X.693 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.693-202102-I!!PDF-E&lang=e&type=items)：8、9、E-XER 指令章节。

<a id="cgen-validation--batch-6--生成依赖"></a>

<a id="cgen-validation--生成依赖"></a>

#### 生成依赖

本批 CMake 消费者共用单一生成目标；输出以 stamp 标记成功，头/源为 BYPRODUCTS，
消费者显式依赖生成目标。cache hit 不更新源码时间戳也不会导致多个目标重入同一生成目录。
并发写保护继续生效。第 6 批十个 C 消费目标在冷 stamp 的并行构建中单独复验，
其他历史测试样例的共享生成依赖不据此宣称已完成第 7 批工程收口。

<a id="cgen-validation--batch-7"></a>

<a id="cgen-validation--第-7-批验收"></a>

### 第 7 批验收

CGEN-20..24 / M2 已完成登记范围的工程验收。没有删减七个原测试样例或任何编码规则，
不将启动预检查拒绝计为通过。整部 ASN.1 标准和所有 3GPP PDU 的线路认证不在本结论中。

<a id="cgen-validation--batch-7--实现"></a>

<a id="cgen-validation--实现"></a>

#### 实现

- 命名 BIT STRING：PER/COER 采用满足有效 SIZE 的最短长度；DER/CER/CXER 移除全部
  尾零；固定长度按规则补零。解码还原为满足 SIZE 的等价值，普通 BIT STRING 长度不变。
- C UTCTime：校验公历日期、闰年、时分秒及 +/-hhmm，按 DER/CER/PER/COER/CXER
  要求输出 YYMMDDhhmmssZ。PER 走 unconstrained VisibleString，时间 SIZE 不作为 PER-visible。
  OER 同样忽略时间 SIZE（X.696 8.4.4），始终保留长度字段；新增 OER/COER SIZE(13) 回归。
  时间支持范围为 1950–2049；归一化跨出该窗口明确拒绝，不依赖进程 locale/时区状态。
- 不连续整数根：X.691 13.1 扩展位按根的数值 RANGE 判断，空洞不缩小范围；非扩展约束
  仍精确检查成员。不把根集合裁成第一段，也不维护第二份 APER 核心。
- 表约束：已知 JSON/XML open 字段用生成的不可变 selector/getter/offset 编解码实际
  内部值；JSON 即使控制字段最后出现也能解码。XML 使用实际类型元素包装。
  自动拥有并深拷贝/释放选中值；未知 key/非法 payload/类型不匹配返回明确状态。
  空 PrivateIE 对象集保留不透明 PER 内容，不虚构整数控制字段。
- 补齐 arbitrary-precision INTEGER DEFAULT 比较、缺省还原和资源记账，含负 2^128。
  失败时保持公共输出对象，byte/text 入口同时保持游标/writer 已提交内容。
- SDK 配置期 `asn1_generate_c` 读取受控清单；新增模块、缺失源会触发重配置。
  共用生成产物使用单一生产者与消费者依赖，保留发布锁；新增全部测试可执行程序聚合目标。
- 生成 ABI / C 运行时 ABI 为 **3**：私有描述符扩展和空选择器布局变化要求重新
  生成、重编译运行时和消费者；生成头检查运行时 header 版本。项目版本仍为 0.1.0，
  不承诺跨 CRT/平台或不同 SDK 的二进制兼容；现有专有许可声明不改变。

<a id="cgen-validation--batch-7--最终门禁"></a>

<a id="cgen-validation--最终门禁"></a>

#### 最终门禁

| 门禁                            | 实际结果                                                            |
| ----------------------------- | --------------------------------------------------------------- |
| GCC 13.3 严格 C11，原 7 测试样例 ×12  | **84/84**                                                       |
| Clang 18.1.3 严格 C11，同一矩阵      | **84/84**                                                       |
| 原 C++17 GCC 7×12 语法门禁         | **84/84**                                                       |
| MSVC 19.44 Release 完整 CTest   | **476/476**                                                     |
| GCC ASan+UBSan 完整 CTest       | **512/512**                                                     |
| 新增 named/time/sparse/typed 功能 | 三形态各 48 个原语编解码案例 + 5 个类型化 JSON/XML 案例、双 TU                      |
| INTEGER DEFAULT               | 三形态，每形态十二入口、默认/非默认两组、双 TU                                       |
| 新增失败门禁                        | 类型化开放字段 / 命名位串逐次分配失败；未知键、越界内容、错误 NULL、非规范 DER                   |
| Clang libFuzzer + ASan+UBSan  | 十二生成入口 PDU 测试驱动，10,000 次，max_len=4096，无崩溃                       |
| SDK 安装消费                      | Windows 五类消费者全部通过；Linux C/runtime-only SDK 的 c 与 generated-c 通过 |
| 纯 C 链接边界                      | Linux 两种 C 消费者未启用 C++，ldd 不含 libstdc++/libc++                   |
| 分层与公共头                        | C/C++ include/link 正反例；25 个 C 头各 C11/C++17、49 个 C++17 头通过       |
| 独立构建                          | 仅编译器与纯 C 仅运行时构建通过；前者不链接任何运行时                                    |
| 跨构建执行保护                       | Generic cross probe 编译/链接通过且不注册目标运行测试；不代表设备 ABI 验收              |

矩阵由同一生成器执行，前后 SHA-256 一致；详见本地 `build-rev7-work/final-matrices.json`。
语法、运行、外部外部对照工具与 fuzz 分别记录；测试数量不是标准覆盖率。
MSVC 本轮是 Release /std:c11 验收，没有把它称作 ASan+UBSan，也未声称执行 MSVC ASan。
Clang 执行的是严格 C11/runtime/header/feature 与插桩 fuzz，不冒充完整 Clang C++ 回归。

<a id="cgen-validation--batch-7--独立线路证据"></a>

<a id="cgen-validation--独立线路证据"></a>

#### 独立线路证据

- 新增 asn1tools 0.169.0 **21** 个独立相等线路及值解码，冻结在
  `tests/standards/cgen7-asn1tools-vectors.json`，由三形态 feature gate 离线回放。
- asn1c 0.9.29 独立核对 INTEGER(1..3|8|15,...) 的 UPER 根范围：
  1→00、3→10、4→18、8→38、14→68、15→70（5 位）；16→808800（17 位）。
  冻结根向量及非扩展空洞负例进入 `CGenBatch7.BigintRoot`。
- 原连续根的正负 2^128 等十二个 UPER/APER 独立 asn1tools 向量继续回放。
  原 72 组外部冻结外部对照工具和整份 RRC/asn1c UPER+BER 门禁均继续通过。
- asn1tools 的部分 UNION 根处理和 DER 命名位 shortening 存在缺口，未用它覆盖这些
  结论；没有宣称它提供 CER/COER/CXER 规范编码外部对照工具。标准向量、外部验证分别记账。

<a id="cgen-validation--batch-7--重复执行"></a>

<a id="cgen-validation--重复执行"></a>

#### 重复执行

```sh
cmake --build build --target asn1_test_executables
ctest --test-dir build --output-on-failure
python3 scripts/validate_c_codec_matrix.py --compiler build/asn1cxx --cc gcc --output build/c-gcc
python3 scripts/validate_c_codec_matrix.py --compiler build/asn1cxx --cc clang --output build/c-clang
python3 scripts/validate_codec_matrix.py --compiler build/asn1cxx --cxx g++ --output build/cpp
python3 scripts/validate_installed_package.py --build build --output build/fresh-package-check
```

本地完整日志位于忽略的 `build-rev7-work/`。维护的 CTest/脚本与冻结向量已纳入仓库。

<a id="cgen-validation--batch-7--仍然明确的支持边界"></a>

<a id="cgen-validation--仍然明确的支持边界"></a>

#### 仍然明确的支持边界

GeneralizedTime、未登记字符字符域、E-XER UNTAGGED 等原有支持范围限制仍保留。
JSON/XML 未知选择器或缺少内部类型模式的独立 open value 明确
Unsupported，不以 hex 占位冒充 typed 解码。BER/OER 仍使用不透明完整编码；仅有
选中类型视图而无适用内容时明确拒绝。全模块语法成功不等于每个 LPP/S1AP/NR PDU
都完成独立线路认证；字段路径诊断和完整未知扩展保留也不由本批推定。

标准依据：X.680 22.6/22.7/47、X.690 11.2/11.8、X.691 10.6.5/13.1/16.2–16.3、
X.696 13/31.6/31.11、X.693 9.3/9.11/9.12、X.697 24/41。

标准原文：[X.691 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items)、
[X.696 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.696-202102-I!!PDF-E&lang=e&type=items)、
[X.693 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.693-202102-I!!PDF-E&lang=e&type=items)、
[X.697 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.697-202102-I!!PDF-E&lang=e&type=items)。

<a id="remediation-validation"></a>

## 工程修正 REV 验收记录

各节保留原批次的命令、结果与限制，属于历史验收记录；历史测试数量和当时的未完成项不作为当前状态。当前能力以 [status.md](status.md) 为准。

<a id="remediation-validation--目录"></a>

### 目录

- [REV-01-03 验收](changelog.md#remediation-validation--rev-01-03)
- [REV-04-09 验收](changelog.md#remediation-validation--rev-04-09)
- [REV-10-12 验收](changelog.md#remediation-validation--rev-10-12)
- [REV-13-19 验收](changelog.md#remediation-validation--rev-13-19)
- [REV-20-24 验收](changelog.md#remediation-validation--rev-20-24)
- [REV-25-28 验收](changelog.md#remediation-validation--rev-25-28)

<a id="remediation-validation--rev-01-03"></a>

<a id="remediation-validation--rev-01-03-验收"></a>

### REV-01-03 验收

<a id="remediation-validation--rev-01-03--变更与证据"></a>

<a id="remediation-validation--变更与证据"></a>

#### 变更与证据

| 任务     | 修复前调用链证据                                                      | 本批修复                                                                                              |
| ------ | ------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| REV-01 | void ensure 在 realloc 失败后返回，部分调用方仍复制或继续写入                     | ensure 返回状态；整次写入器操作预留容量；加法、位转字节及容量倍增检查；take 失败不修改有效位；clear/take 后复用不残留旧 1 位                       |
| REV-02 | decimal parse → mul_add → push_limb，最后一步失败被忽略                 | 临时 BigInt 解析、成功提交；OOM/limb 上限逐层传播；幅值序列化失败不提交输出                                                    |
| REV-03 | 分配失败混用 LengthOverflow；PER BIT STRING 解码先改输出再读；BER tag 解码提前写输出 | 保留 0..7，新增 OOM=8/ResourceLimit=9；PER 输出临时构造和失败清理；BER 输出延迟提交；读取器截断预检；文本缓冲边界安全减法及 NULL/长度验证；逐接口契约文档 |

新增 `tests/c_lang/c_runtime_failure_test.c`：逐分配点失败注入、live 分配
计数、输出不变与恢复路径、已填满/未对齐写入器、limb 上限相邻值、重复 free
以及读取器输入复制等，共 424 个内部检查。测试专用源副本带私有 allocator
宏；生产库无全局可变 allocator 或测试 hook。初始化、复用和失败游标边界见
[architecture.md](architecture.md#c-runtime-contracts)。

<a id="remediation-validation--rev-01-03--实测环境与结果"></a>

<a id="remediation-validation--实测环境与结果"></a>

#### 实测环境与结果

WSL Ubuntu 24.04，GCC/G++ 13.3.0，CMake 3.28.3，Ninja 1.11.1。
GoogleTest 使用已有本地源码，未下载新测试依赖。

| 验证                                                 | 结果                                                          |
| -------------------------------------------------- | ----------------------------------------------------------- |
| 独立 gcc C11 `-Wall -Wextra -Werror -O2` 失败测试 driver | 424 个检查通过                                                   |
| 相同编译选项、无 test allocator 的原冒烟测试                     | 通过                                                          |
| 独立 gcc C11 ASan+UBSan 失败测试 driver（含 leaks）         | 424 个检查通过，无 sanitizer 报错                                    |
| CMake Release 完整构建及 CTest                          | 346/346 通过，包含既有 345 项及新增失败测试 gate                           |
| CMake Debug ASan+UBSan 完整构建及 CTest                 | 346/346 通过，包含冒烟测试、失败测试、既有 golden 回归                         |
| 20 个公共 C 头分别独立编译                                   | C11 与 C++17 共 40 次 `-fsyntax-only -Wall -Wextra -Werror` 通过 |
| 生产库 `nm`                                           | 不含 `asn1_test_*` 符号                                         |
| 现有 7 测试样例 × 12 选择器生成语法矩阵                           | 84/84 通过，每次关闭 emit cache；口径限制见下                             |

第一次同时启动两组 `-j4` 全量构建时 WSL 响应超时，已停止本次构建并改为
`-j1` 顺序执行；最终两组完整构建及 CTest 均成功。没有修改全局 WSL 配置。

<a id="remediation-validation--rev-01-03--可复现命令"></a>

<a id="remediation-validation--可复现命令"></a>

#### 可复现命令

在仓库根目录的 Linux shell 执行；`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` 替换
为自己的 GoogleTest 1.14.0 源码目录。本次取已有 `build/_deps/googletest-src`。

```sh
cmake -S . -B build-rev1-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-release -j 1
ctest --test-dir build-rev1-release --output-on-failure

cmake -S . -B build-rev1-san -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASN1_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-san -j 1
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-rev1-san --output-on-failure
```

独立 C 故障测试（先创建 `build-rev1-quick`）：

```sh
gcc -std=c11 -Wall -Wextra -Werror -O2 \
  -DASN1_C_RUNTIME_TEST_ALLOCATOR -Iinclude \
  src/c_runtime/*.c tests/c_lang/c_runtime_failure_test.c \
  -o build-rev1-quick/failure
./build-rev1-quick/failure
```

独立 sanitizer 使用同一命令，将 `-O2` 替换为
`-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer`，
并带相同 ASAN/UBSAN 环境变量运行。

本地完整日志在忽略的 `build-rev1-quick/{config,build,ctest}-{release,san}.log`
及 `extra-validation.log`；本记录保留验收摘要，日志不加入源码仓库。

<a id="remediation-validation--rev-01-03--验证边界"></a>

<a id="remediation-validation--验证边界"></a>

#### 验证边界

- 运行了仓库保存的 asn1c/asn1tools 来源 golden 向量回归；未重新安装/运行外部
  外部对照工具生成器，未改写 golden。C PER 补码/分片等第二批问题仍待修复。
- 本批没有生成器、IR 或 C++ 运行时语义改动；既有 split 和单编码规则链接/编译
  门禁随完整 CMake 构建执行。
- 历史 12 选择器为 `uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer`。
  `per` 是组合别名，不是 JERI；此矩阵不能证明 12 个独立产品入口完整。
  JERI 与矩阵口径修正仍由 REV-15/16 处理，不作为本批已实现能力。
- Windows/MSVC ABI、完整 C 编码规则标准覆盖、C emitter 与第二批线路修复均不在
  本批验收范围内。

<a id="remediation-validation--rev-01-03--矩阵记录"></a>

<a id="remediation-validation--矩阵记录"></a>

#### 矩阵记录

最终结果：**84/84 通过**，驱动退出码 0。七个测试样例：

- `telecom/tlv_oer_golden.asn`
- `telecom/tlv_golden.asn`
- `cer/cer_golden.asn`
- `telecom/rrc_slice/rrc_slice.asn`
- `telecom/lpp_14_3_0/LPP-PDU-Definitions.asn`
- `telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn`
- `telecom/s1ap_14_4_0/s1ap_14_4_0.asn`

对每个测试样例和上述 12 选择器，执行：

```sh
./build-rev1-release/asn1cxx --no-emit-cache --emit-dir <dir> \
  --codec <selector> tests/fixtures/<fixture>
g++ -std=c++17 -fsyntax-only -Iinclude -I<dir> <dir>/generated.cpp
```

JERI 尚无独立生成入口，因此上述是历史矩阵的回归结果，不是 12 个独立入口
已完成的声明；既有缺口保留在 REV-15/16。REV-01..03 的本批验收完成。

<a id="remediation-validation--rev-04-09"></a>

<a id="remediation-validation--rev-04-09-验收"></a>

### REV-04-09 验收

<a id="remediation-validation--rev-04-09--修正范围"></a>

<a id="remediation-validation--修正范围"></a>

#### 修正范围

| 任务     | 结果                                                                                                                      |
| ------ | ----------------------------------------------------------------------------------------------------------------------- |
| REV-04 | MSB-first 尾位取高位；source/bit-count 容量验证；scalar put_bits >64 返回 InvalidArgument；独立逐位模型覆盖 8×18 组合                           |
| REV-05 | BigInt 明确区分无符号幅值与 signed minimal two's complement；负数、符号保护字节、零及 >64bit 正确编码/解码；非法空/冗余表示拒绝                                |
| REV-06 | 单 determinant 显式 more；完整字符串/open type 交错处理片段与内容，整片末尾零、多片、截断及非法 C0/C5/FF；旧 scalar 辅助函数对片段显式 Unsupported                  |
| REV-07 | 长度描述符区分 fixed/constrained/semi/unconstrained/extensible；BIT STRING 实际长度；INTEGER 根区间/半约束/上界/无界及完整 int64；64K SIZE 上界走一般分片 |
| REV-08 | 唯一 `per_core.c` 以 Variant 参数化；`per.c` 是旧 ABI UPER 包装；前置 1 位下 APER 整数/字符串/长度/open type 的独立字节和尾字段验证                       |
| REV-09 | 保留代表性冒烟测试，补真实 legacy 解码；新增纯 C11 正确性/大值门禁、冻结外部对照工具向量与后片分配失败测试；逐声明组登记测试口径                                                 |

没有将 compiler 链接运行时，没有用 C++ 包装代替纯 C11，没有增加第三方
运行时依赖。C++ 运行时、IR 和生成器语义未修改；本次不是完整 C emitter。

<a id="remediation-validation--rev-04-09--工具与实测"></a>

<a id="remediation-validation--工具与实测"></a>

#### 工具与实测

WSL Ubuntu 24.04，GCC/G++ 13.3，CMake 3.28.3。GoogleTest 使用已有本地源码。

| 检查                             | 实测结果                                                             |
| ------------------------------ | ---------------------------------------------------------------- |
| Release 全构建/CTest              | 348/348 通过                                                       |
| Debug ASan+UBSan 全构建/CTest     | 348/348 通过，开启 leaks / halt_on_error                              |
| `Asn1CRuntimeFailureContracts` | 498 个内部检查，包括 signed export 和后片拼接分配失败、输出/存活分配计数                   |
| `Asn1CPerCorrectness`          | 4473 个内部检查；生产 64KiB 限制                                           |
| `Asn1CPerLargeValues`          | 相同 4473 检查；单独 128KiB 运行时源副本，不修改生产默认限制                            |
| 独立 asn1tools 0.166.0           | 234 个双向 encode/decode 和全字节一致用例；184 个短向量冻结进入 CTest                |
| 公共头与生产符号                       | 20 头 × C11/C++17 共 40 次独立编译通过；生产库无 asn1_test_* 符号                |
| C11 严格警告检查                     | 最终运行时与三个 C 测试源 `-std=c11 -Wall -Wextra -Werror -fsyntax-only` 通过 |
| 历史 7 测试样例 × 12 选择器             | 84/84 通过；不把 per 别名当 JERI                                         |

新 primitive 测试在 CTest 中不依赖 Python、asn1tools 或 GTest；独立外部对照工具
脚本只在开发验证时使用。具体声明/负例登记见
[development.md](development.md#c-runtime-tests)，所有权与 ABI 兼容说明见
[architecture.md](architecture.md#c-runtime-contracts)。

<a id="remediation-validation--rev-04-09--重跑命令"></a>

<a id="remediation-validation--重跑命令"></a>

#### 重跑命令

本次复用第一批的本地构建目录及 GoogleTest override：

```sh
cmake --build build-rev1-release -j 1
ctest --test-dir build-rev1-release --output-on-failure
cmake --build build-rev1-san -j 1
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-rev1-san --output-on-failure
PYTHONPATH=/path/to/Python_asn1tools python3 scripts/validate_c_per_oracle.py \
  --build-dir build-rev1-quick/per-oracle
```

首次配置参数见 [第一批验收记录](changelog.md#remediation-validation--rev-01-03)。外部对照工具脚本使用
`gcc -std=c11 -Wall -Wextra -Werror -shared -fPIC` 编译纯 C 库，与独立工具对验。
`--write-vectors` 只用于明确审阅后的更新；默认重新生成预期并比较冻结头文件。
不把本实现输出回写成金标准。

<a id="remediation-validation--rev-04-09--标准仲裁和验证边界"></a>

<a id="remediation-validation--标准仲裁和验证边界"></a>

#### 标准仲裁和验证边界

- 新脚本明确排除 asn1tools 的 MAX 下界处理、根外 BIT STRING 未实现、根外
  OCTET STRING 整片缺最终零、APER 有界变长零内容仍对齐等边界；排除项不计入
  234 个通过用例。按标准独立字段构造验证；理由和代码证据见覆盖登记。
- 条文依据：[ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en)
  11.4/11.5/11.7/11.9、13、16、17。保留对外部对照工具偏差的显式记录。
- 旧 `decode_octetstring(r,length)` 是裸内容读取器；新 `_ex` 是完整值的
  对称接口。未静默改变旧函数 ABI/参数含义，文档明确推荐新接口。
- 默认 decoder 上限 64KiB；65537 字节可编码，但默认解码明确 ResourceLimit，
  128KiB 测试目标验证可配置上限下的完整解码。BIT STRING 上限按字节折算位数。
- 单区间 int64 描述符不表示不连续集合或 >64bit 的有界约束；无界大整数由
  BigInt API 支持。字符集原语、normally-small、完整 C 生成类型仍是未来项。
- 既有 asn1c/asn1tools 来源向量随全量 CTest 回归；没有重新运行 asn1c 生成器。
- 历史选择器列表包含 per 组合别名，JERI 独立入口仍属 REV-15/16；没有以
  此矩阵声称 12 个独立产品入口已经齐全。

<a id="remediation-validation--rev-04-09--最终矩阵记录"></a>

<a id="remediation-validation--最终矩阵记录"></a>

#### 最终矩阵记录

最终结果 **84/84 通过**，驱动退出码 0，逐项禁用 emit cache；七个测试样例
与十二个历史选择器同 [第一批矩阵](changelog.md#remediation-validation--rev-01-03--矩阵记录)。
公共头和生产符号检查也通过。REV-04..09 已完成并勾选；第三批 REV-10 起
仍未开始。原始日志保留在忽略目录
`build-rev1-quick/batch2-{build,ctest}-{release,san}.log`、
`batch2-extra-validation.log`、`per-oracle.log`；CTest 详细内部检查输出在各
构建目录的 `Testing/Temporary/LastTest.log`。

<a id="remediation-validation--rev-10-12"></a>

<a id="remediation-validation--rev-10-12-验收"></a>

### REV-10-12 验收

<a id="remediation-validation--rev-10-12--修正范围"></a>

<a id="remediation-validation--修正范围-1"></a>

#### 修正范围

| 任务     | 实现与证据                                                                                                                                                                      |
| ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| REV-10 | 属性先判存在；mandatory 缺失报错，optional 清空，default 恢复 ASN.1 默认；非法值立即传播；整数通过 BigInteger Result 完整解析，检查规范化根区间及宿主范围后转换                                                                 |
| REV-11 | SourceFile / IR BigInt 返回 Result；OID / CLI 数字 from_chars；目录操作 error_code；写入 flush 检查；线程创建失败 deferred 回退；容量异常在入口转换诊断；移除 catch(...) 和业务 throw；PER restricted-string 编码失败立即传播 |
| REV-12 | Result nodiscard、错误访问 assert/终止；Error 增加 offset_unit/field_path；BitReader/ByteReader 标单位；源位置明确 UTF-8 字节列；公共头 C++17 独立编译脚本（含 -Wall/-Wextra/-Werror/-pedantic-errors）        |

新增 E-XER 测试样例以 out-of-line、inline、split 三种模式各自生成、编译、
链接、运行。用例涵盖缺 mandatory、非法 bool、12x、数值溢出、约束越界、
非法 BigInteger、optional/default 缺失与非法值、逻辑字段名。全部显式禁用
emit cache；不会靠旧缓存通过生成器回归。

另有 SourceFile 不存在/目录读取、BigInt int64 两端及负零、OID 溢出源范围、
CLI 部分数字/溢出/输出为文件、`/dev/full` 写失败、线程资源失败注入及
bad_alloc 不转换、严格消费者 Error 聚合初始化、Result move-only/const/错误访问死亡测试、bit/byte 单位。
识别 int64 边界后同步消除 ENUMERATED 后继编号的有符号溢出；无法隐式表示的
后继报诊断，不悄悄变成零。

`nodiscard` 还揭示 PER 生成器丢弃 restricted-string 的 Result：已修复，新增
UPER/APER 非法字母表输入的实际生成代码测试。两个构建目录的 emit cache
在该补丁后清除，避免旧产物掩盖修正；缓存键的永久失效策略仍是 REV-13。

<a id="remediation-validation--rev-10-12--验证命令"></a>

<a id="remediation-validation--验证命令"></a>

#### 验证命令

本次使用 WSL Ubuntu 24.04、GCC/G++ 13.3、CMake 3.28.3、Ninja、本地已有
GoogleTest 源码。Windows MSVC / Clang 本批未执行。

```sh
cmake --build build-rev1-release -j 1
ctest --test-dir build-rev1-release --output-on-failure
cmake --build build-rev1-san -j 1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-rev1-san --output-on-failure
python3 scripts/validate_public_headers.py --cxx g++
```

| 门禁                           | 最终实测                                             |
| ---------------------------- | ------------------------------------------------ |
| Release 全构建 / CTest          | 370/370，0 失败                                     |
| Debug ASan+UBSan 全构建 / CTest | 370/370，leaks/halt_on_error 开启，0 失败              |
| 公共 C++17 头独立编译               | 47/47，含严格警告与 pedantic                            |
| Error 聚合初始化 / nodiscard 负向编译 | 正向严格编译通过；丢弃 Result 按预期被 -Werror=unused-result 拒绝 |
| C 运行时公共头                     | 全部 C11 与 C++17 严格语法编译通过                          |
| 历史 7 测试样例 × 12 选择器           | 84/84，显式 --no-emit-cache                         |
| 生产 C 运行时符号                   | nm 无 asn1_test_ 故障注入符号                           |

原始构建、CTest、头检查和矩阵日志位于忽略目录 `build-rev1-quick/batch3-*`；
矩阵驱动为本地 `verify3.py`，逐项命令/fixture/selector 保留在 extra-validation
日志。CTest 包含 C 冒烟测试、失败测试、PER 冻结向量与大值、所有既有 golden 回归。

<a id="remediation-validation--rev-10-12--验证边界与兼容性"></a>

<a id="remediation-validation--验证边界与兼容性"></a>

#### 验证边界与兼容性

- 历史语法矩阵仍使用 `uper aper per ber der cer oer coer jer xer exer cxer`，
  `per` 是组合别名，不等于已有 JERI 生成入口；真实 12 入口由 REV-15/16
  继续处理，不把 84 项称为 12 种独立线路规则。
- C11 与 C++ PER 原语算法本批未更改；PER 生成器补 restricted-string 失败传播；全量运行既有冻结外部对照工具门禁。第二批独立
  asn1tools 234 用例为历史证据，本批不冒充重新执行其 live 差分。
- REV-13 的缓存修订键、REV-14 的并发上限仍待办；本批仅处理相关错误路径。
- 旧编码规则未标明的偏移保留 Unknown；本批未承诺完整嵌套字段路径或 XML
  DOM 错误的原始文本字节定位。
- 新 C++ 返回类型和 Error 布局需要重编译消费者，迁移方式和访问前置条件
  见 [C++ 错误契约](architecture.md#cpp-error-contracts)。C11 ABI 与分层铁律保持。

<a id="remediation-validation--rev-13-19"></a>

<a id="remediation-validation--rev-13-19-验收"></a>

### REV-13-19 验收

本批限定缓存、并发、产品入口、构建和包消费。编译器不链接运行时；运行时
不包含 AST/IR；Result、共享 PER Variant、自研 BigInt、三种产物形态均保留。
C11 emitter 的后续子任务没有被本批宣称完成。

<a id="remediation-validation--rev-13-19--修改与证据"></a>

<a id="remediation-validation--修改与证据"></a>

#### 修改与证据

| REV | 已核实问题                                                    | 修复与可重跑证据                                                                                                                                           |
| --- | -------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| 13  | 缓存版本写死；无 source_paths 时共用 src:empty；文件 read 错误可得到部分 hash | CMake 按 compiler 源/头、构建定义和工具链生成指纹；格式版本 2；读取失败禁用缓存；无源路径 API 禁用缓存；选项/依赖/输入顺序/输出损坏/模型变化回归                                                             |
| 14  | workers 未使用，两个分析 pass 均按模块数启动 async                      | 共享 bounded_ordered 有界批次，按模块顺序合并；0/1 串行；lower/tag 保持串行；线程资源失败保留 deferred 回退                                                                         |
| 15  | 缺少 JERI CLI/掩码；误称标准 canonical JSON                       | Jeri 独立产品位及 CLI；共用 JER 实现，新增 encode_jeri/decode_jeri；落实 ARRAY 控制段并明确拒绝其余尚未支持控制段；[入口登记](architecture.md#codec-entries) 明确同一种 X.697 线路语法             |
| 16  | per 被重复计数；若干库映射错误；OER/XML 组合策略覆盖同族入口                     | 84 个实际单选语法检查；12 个单入口及组合三形态真实链接、调用和编码结果比较；OER/COER 与 XER/CXER/E-XER 各自生成；旧 E-XER-only xer 别名保留                                                      |
| 17  | 全局 GNU flags、硬编码 C11、新版下载参数                              | 项目 target 级编译器/平台条件；C_STANDARD 11/REQUIRED/EXTENSIONS OFF；MSVC utf-8；旧 CMake 条件参数；MSVC 明确拒绝 ASan+UBSan 组合                                          |
| 18  | GTest 下载无 hash；宿主/目标工具混用风险                               | 固定 zip SHA256；本地源码覆盖/测试关闭均离线；宿主工具路径与依赖；交叉默认关闭目标测试，显式开启报错；两阶段示例                                                                                     |
| 19  | 只有 INSTALL_INTERFACE，无导出包                                | install/export/config/version 与组件检查；关闭私有 GTest 安装并检查 SDK 不含 gtest/gmock 头；compiler-only/runtime-only 开关；C/C++/compiler/宿主生成四类独立消费者；纯 C 无 C++ 标准库依赖 |

代码入口包括 `src/codegen/emit_core.cpp`、`emit_{jer,xer,oer}.cpp`、
`include/asn1/support/async.hpp`、`src/semantic/analyzer.cpp`、根/测试 CMake、
`cmake/`、`scripts/validate_*.py`、`examples/package-consumers/`。

JERI 标准命名依据及局部能力边界见 [入口登记](architecture.md#codec-entries) 中的 ITU 原始来源。
本批不是完整标准语义审计，仍保留 REV-20..24；未运行新的外部外部对照工具生成任务，
全量 CTest 包含已有独立外部对照工具冻结向量回归，不以新增自往返冒充外部金标准。

明确家族后的最小输入曾触发 `CodecEntry.CombinationEquality` 失败：控制段
`[ARRAY] SEQUENCE` 被忽略，生成 object 而非 array。修复前日志保留为
`batch4-array-control-repro.log`；修复后正例实际输出 `[42,true,"ASN1"]`，
新增 TEXT/ENUMERATED 未实现控制段负例要求明确诊断，MSVC/GCC 针对性检查已通过。

<a id="remediation-validation--rev-13-19--工具与完整门禁"></a>

<a id="remediation-validation--工具与完整门禁"></a>

#### 工具与完整门禁

| 工具/门禁                                        | 本次实际结果                                                                                                                                    |
| -------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| GCC/G++ 13.3.0，CMake 3.28.3，Ubuntu 24.04 WSL | 原生工具链                                                                                                                                     |
| GCC Release 全量构建 + CTest                     | **392/392 通过**                                                                                                                            |
| GCC Debug ASan+UBSan 全量构建 + CTest            | **392/392 通过**                                                                                                                            |
| Clang 18.1.3 Release 全量构建 + CTest            | **392/392 通过**                                                                                                                            |
| Clang 18.1.3 Debug ASan+UBSan 全量构建 + CTest   | **392/392 通过**                                                                                                                            |
| CMake **3.16.8** 离线配置                        | 通过；实际编译并运行 C11 冒烟测试、12 入口组合，**2/2**                                                                                                       |
| MSVC **19.44.35214**，VS 2022 x64 Release     | compiler、全部运行时构建通过；安装后四类消费者通过                                                                                                             |
| MSVC 缓存/并发/codegen 回归                        | **11 + 13 + 16** 用例通过；含不支持控制段负例，JERI/E-XER/12 组合程序实际调用成功                                                                                  |
| MSVC ASN1_ENABLE_SANITIZERS=ON               | 按预期配置失败，明确提示 ASan 不等于 UBSan                                                                                                               |
| 公共 C++17 头独立严格编译                             | **47/47**                                                                                                                                 |
| 7 测试样例 × 12 实际产品入口单选语法矩阵                     | **84/84**；使用 JERI，未计 per 别名                                                                                                               |
| 离线仅编译器 / 仅运行时独立构建、安装与消费者                     | 通过；配置不进入 GTest 下载；C 消费者 ldd 无 libstdc++/libc++                                                                                            |
| 完整 GCC/MSVC 包独立消费                            | 各四类通过：C、运行时、compiler、host-generated                                                                                                       |
| 宿主/目标连线与真实异构链接                               | Generic 连线及拒绝交叉测试检查通过；x86_64 宿主生成 → Clang 18.1.3 + AArch64 sysroot 的 C++ 消费者链接通过；纯 C AArch64 消费者也链接通过，readelf 确认 AArch64，两者 **0** 个宿主运行测试 |
| Python -O 下门禁检查                              | 正例通过，注入错误组合输出按预期失败；入口门禁 **16/16** 重跑通过                                                                                                    |
| 实际更换生成器指纹后的缓存失效                              | 隔离仅编译器构建：同 ASN.1 hit → 改指纹重建 → miss；最后恢复指纹并重建通过                                                                                           |

Clang 与 CMake 3.16.8 的开发工具只下载/解包到忽略的构建目录，未修改系统安装，
未引入项目运行时依赖。首次同时运行多份大型构建出现 WSL 内存压力，已中断该轮
Release/sanitizer 构建；最终大型测试样例完整轮次改为串行；重新生成后采用 copy_if_different / Ninja restat 复用字节相同的编译对象；大型仅编译测试样例使用 -g1 减少调试类型图内存，
全部 sanitizer 检查仍开启。未把中断结果记为通过。

<a id="remediation-validation--rev-13-19--可重跑命令"></a>

<a id="remediation-validation--可重跑命令"></a>

#### 可重跑命令

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build-release -j1
ctest --test-dir build-release --output-on-failure

cmake -S . -B build-san -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASN1_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-san --output-on-failure

# Clang 两轮使用相同配置，并显式选择 clang / clang++
# -DCMAKE_C_COMPILER=/path/to/clang -DCMAKE_CXX_COMPILER=/path/to/clang++
cmake --build build-release --target asn1_codec_matrix
python3 scripts/validate_public_headers.py
python3 scripts/validate_installed_package.py --build build-release --output build-package-check
python3 scripts/validate_generator_revision.py --build build-compiler-only
```

最后一条只用于独立且未同时构建的仅编译器目录；它修改并恢复该构建的
私有生成头，不修改仓库源文件。独立组件及两阶段命令见
[构建与安装消费](usage-guide.md#build-and-package)。

本地日志保存在忽略目录 `build-rev1-quick/batch4-*.log`：最终 GCC 构建分别为
`batch4-build-release-final.log` / `batch4-build-san-final.log`，小型驱动收口构建为
`batch4-release-final-refresh.log`；四个完整测试日志均为 `batch4-ctest-*.log`。
矩阵 `batch4-matrix.log`、最低版本 `batch4-cmake316-final.log`、安装
`batch4-package-{gcc,msvc}-final.log`、独立组件 `batch4-components.log`、交叉连线
`batch4-cross.log` / `batch4-aarch64.log`、指纹回归 `batch4-revision-persistent.log`。

<a id="remediation-validation--rev-13-19--验证边界"></a>

<a id="remediation-validation--验证边界-1"></a>

#### 验证边界

- 实际交叉构建验证了 Linux AArch64 的编译/链接和 ELF 架构；没有设备运行证据。
  不据此承诺裸机 libc、其他 ARM ABI 或特定电信设备已通过。
- MSVC 本批不是全部测试样例的完整原生测试矩阵；POSIX CLI 子进程测试不在
  Windows 注册。最低 CMake 只做所列配置/代表构建，不称全部测试已在 3.16 重跑。
- 84 个语法检查不证明链接或互操作；链接证据另由代表 schema 的 CodecEntry
  程序提供。JER ARRAY 和 E-XER ATTRIBUTE 参与组合比较；CXER 使用自己的写入器。
- 缓存依赖调用者完整 source_paths；手工 IR 无来源时关闭缓存，不伪造 IR 序列化。
  同目录并发生成、原子发布/旧产物管理仍由 REV-25 处理。
- 12 是产品入口口径，完整语法/约束/规范策略和外部外部对照工具空白仍按后续编号核查。

<a id="remediation-validation--rev-20-24"></a>

<a id="remediation-validation--rev-20-24-验收"></a>

### REV-20-24 验收

本批是能力盘点、已确认缺陷修复、支持范围和独立验证基础设施建设，
不宣称实现整个 X.680 系列或全部规范策略。保留分层、Result、纯 C11、
共享 PER Variant、零第三方运行时依赖及三种产物形态。

<a id="remediation-validation--rev-20-24--rev-20分层能力登记"></a>

<a id="remediation-validation--rev-20分层能力登记"></a>

#### REV-20：分层能力登记

新增 [标准能力及外部对照工具矩阵](status.md#standard-capabilities)，记录 2021 基线、
未完成逐条勘误核对、语法/IR/生成/runtime/独立验证、C/C++ 区别。
条目引用实现与具体测试；21 个可重复最小 schema 探针区分接受与拒绝，
不把生成成功作为线路证据。Phase49 C emitter 的延期状态保持。

<a id="remediation-validation--rev-20-24--rev-21约束与高频结构"></a>

<a id="remediation-validation--rev-21约束与高频结构"></a>

#### REV-21：约束与高频结构

| 子项      | 最小 schema/原行为                                                                        | 预期与修复                                                                                 |
| ------- | ------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------- |
| REV-21a | `A ::= INTEGER (unknown..10)`；归一化仅 warning，可能进入 unconstrained IR                     | 无法计算约束就报错，阻止生成；不是猜测一个 PER 范围。下/上界、single value 和未知形式都改为 error                         |
| REV-21b | `Base ::= INTEGER (0..255) A ::= Base (0..7)`；引用类型路径没有保存新增约束                         | 现阶段 Unsupported，禁止生成按基类型范围编码的伪成功产物；可靠合并继承约束另立功能项                                      |
| REV-21c | `SEQUENCE { x INTEGER } (WITH COMPONENTS { x PRESENT })`；结构 IR 无该约束                  | 对普通结构及 Boolean/Null/Enumerated 上无法保存的约束报错；不是丢掉约束后继续                                   |
| REV-21d | `OCTET STRING (SIZE(-1..4))`、`INTEGER ((0..1) ^ (3..4))`                             | 负 SIZE 不转换成巨大 unsigned 上界；空交集不能按无约束可编码类型生成，改为 error                                   |
| REV-21e | S1AP `SIZE(0..maxProtocolIEs)`；旧值解析未追踪 import，错误降成 unconstrained                     | 沿模块 import 和值别名解析，检测循环；禁止挑选无关模块同名常量，补三模块回归                                            |
| REV-21f | `Param {INTEGER: upper} ::= SEQUENCE { value INTEGER (0..upper) }`；值实参没用于约束，实例键只含 V0 | 将值实参及调用模块来源带入解析与实例键；嵌套调用保留来源。Small{3}/Wide{7} 对值 3 分别产生 `C0`（2 位）/`60`（3 位），三形态真实调用验证 |
| REV-21g | `Record.count INTEGER(0..255)`；TLV/XML/JSON 等路径读取 int64 后直接 cast，300 可窄化成 44         | 五个 emitter 家族共享 native INTEGER 解码检查，转换前核查宿主范围及非扩展根区间成员；三形态验证 300 和 Small{3} 的值 4 均失败  |

这些失败输入的“预期线路”均为**不产生编码器**，而非挑选近似字节；
CLI 探针保存实际 schema、退出码、diagnostic，语义单测进入 CTest。
原有并/交区间、自动标签、参数化、对象集和组件关系测试继续回归。
严格门禁首先暴露 S1AP 导入/值实参边界错误，首次完整构建失败的证据保留在
`batch5-import-bound-repro.log`；修复原因而非恢复 warning，随后重新验收。
FROM、COMPONENTS OF、数字版本扩展组、现代时间/IRI 类型的真实拒绝已登记，
没有将它们误称为新增支持。

<a id="remediation-validation--rev-20-24--rev-22规范策略与合法替代表示"></a>

<a id="remediation-validation--rev-22规范策略与合法替代表示"></a>

#### REV-22：规范策略与合法替代表示

先新增 `StandardPolicy` 最小负例：修复前 7 项中 6 项失败，日志
`batch5-repro.log` 保留。具体修复：

| 子项      | 实际/预期线路                                                                                | 修复与证据                                                                                  |
| ------- | -------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| REV-22a | `01 01 01` 是 BER TRUE，但不满足 CER/DER 规范 TRUE                                             | CER 共享 DER 原语策略检查，保持 CER 构造/分片逻辑；新增 CER→DER→BER 的运行时目标依赖，没有 compiler/runtime 耦合        |
| REV-22b | REAL `09 03 80 FF 02` 表示 1，是 BER 的合法替代，但 mantissa 为偶数                                  | DER/CER 拒绝非规范 binary 表示；decimal 检查 NR3/最短词法。不是把所有 BER 表示强行规范化后接受                       |
| REV-22c | `Def ::= SEQUENCE {count INTEGER DEFAULT 7}`，BER `30 03 80 01 07`；CER 对应 indefinite 构造 | BER 接受显式默认值，DER/CER 拒绝；SET 同样处理。outline/inline/split 均真实运行                             |
| REV-22d | SET 两次出现同一字段，原先可能覆盖前值；短非末尾 CER 分片/非末尾 BIT STRING unused bits 未检查                       | SET 所有字段增加 seen 检查；CER 检查非末尾 1000 内容字节、末尾 unused bits 和片段策略                            |
| REV-22e | CER 手写 TLV 长度累积可绕过公共长度检查、递归无界                                                          | 使用公共 TLV header 解码和 CER 最短/构造策略校验；内层递归限 64，累计内容受 kMaxCodecBytes 限制；深嵌套、超宽长度负例和 fuzz 回归 |

补充 OER 合法长长度与 COER 非规范形式的成对测试、CER 999/1000/1001 正例，
保留既有 BIT STRING 分片和嵌套 EOC 测试。BER nested indefinite 辅助函数仍明确
Unsupported，未为通过测试而改成 CER 专用行为；COER/XML 的独立验证边界见下。

<a id="remediation-validation--rev-20-24--rev-23准确-profile-与数值失败"></a>

<a id="remediation-validation--rev-23准确-profile-与数值失败"></a>

#### REV-23：准确支持范围与数值失败

[REAL/字符/时间支持范围](status.md#real-and-string-profile) 是此次的公开边界：

- REV-23a：BER binary REAL 不再用 double 累积任意 mantissa 后静默舍入；
  去除尾零后核查 53 位有效精度、范围及次正规精确性。不支持时返回 Unsupported。
  公共 content 解码接口也显式检查 kMaxCodecBytes，不能绕过 TLV 层资源上限。
  decimal 采用明确的有界精确子集。正负零区分，encoder 的负零内容修为 `43`。
- REV-23b：JER bigint 的 `number` 字段可能不是有效值，原 decode_real 会读出
  截断/默认整数；现在拒绝该路径以及不能精确表示的 int64。**完整精确 REAL 模型仍延期**，
  XML/JSON 文本转换的 host-double 语义如实公开，不宣称精确十进制支持。
- REV-23c：OER binary32 编码对有限输入比较转换前后值，拒绝静默舍入；
  无法识别并保真的 REAL WITH COMPONENTS 不再降成 unconstrained。
- 21 项探针包含现代时间/IRI 的拒绝；传统字符串字符域、时间规范词法的
  待核查项分开登记，没有改成普通字符串假装支持。

<a id="remediation-validation--rev-20-24--rev-24外部对验负例与-fuzz"></a>

<a id="remediation-validation--rev-24外部对验负例与-fuzz"></a>

#### REV-24：外部对验、负例与 fuzz

新增 `tests/fixtures/standards.asn`、独立驱动、
`scripts/validate_standard_oracles.py` 和 `tests/standards/oracle-vectors.json`。
向量只从外部 asn1tools 产生；保存工具版本/commit、LF 归一化 schema SHA256、type、value、参数和字节。
普通 CTest 重放冻结向量，无 asn1tools 运行时依赖。刷新必须显式 `--write-vectors`。

- 本次外部对照工具：asn1tools **0.166.0**，包源码 SHA256
  `1bfc8087bedfc4071d2e0cc7452a5bab792bec25683d91febd5a4513f64b8180`。
  本地工具副本没有独立 Git commit，元数据记为 null；不把父工作区 commit 冒充外部对照工具版本。
- 12 个入口的 Record 输出均由外部 decoder 解码；11 个参考输入由项目 decoder 接受。
  CXER 输入只针对本测试样例将外部 XER 的 `<true />` 改为规范空元素拼写 `<true/>`，
  明确记录转换；不冒充独立 CXER 编码器。CER 的 definite BER 输入应拒绝。
- 7 个基础入口比较中，当前简单 Record 的 BER/DER/PER/OER 字节可比较；
  XER 比较 XML 树，JER 比较 JSON 值。无指令的 E-XER/JERI 和该 OER 子集的 COER
  外部互操作，不代表全部指令/规范策略通过。
- 复跑 C PER 外部对照工具：**234** 个外部双向 encode/decode 用例通过，**184** 个短向量；
  排除项及条文仲裁保持第二批记录。既有 C++ asn1c 冻结向量随全量回归，未重跑 asn1c 生成器。
- 新增 `LLVMFuzzerTestOneInput` 与依赖关闭的确定性驱动；13 个种子产生 **481** 次
  截断/变异调用，注册为 `DecoderFuzz.SeedRegression`。Clang libFuzzer 使用库覆盖率插桩，
  ASan+UBSan、固定 seed、最大输入 4096，实际 **20,000** 次无崩溃。
  这是受限探索，不是安全性或完整覆盖证明。

<a id="remediation-validation--rev-20-24--门禁结果"></a>

<a id="remediation-validation--门禁结果"></a>

#### 门禁结果

| 门禁                                   | 本次结果                                          |
| ------------------------------------ | --------------------------------------------- |
| 修复前最小策略复现                            | 7 项中 6 项失败，已记录                                |
| 21 项能力探针                             | 通过（含预期拒绝）                                     |
| 新增策略/生成三形态/约束回归                      | 已运行针对性检查；最终以全量结果为准                            |
| GCC 13.3 Release 全量构建/CTest          | **430/430 通过**                                |
| GCC 13.3 Debug ASan+UBSan 全量构建/CTest | **430/430 通过**；leak detection / halt_on_error |
| 7 测试样例 ×12 实际入口 -fsyntax-only        | **84/84 通过**；不等于互操作                           |
| GCC 安装包四类消费者                         | 全部通过；CER→DER 的传递目标正确导出                        |
| Clang 18.1.3 sanitizer libFuzzer     | 20,000 次通过，481 次确定性种子调用通过                     |
| MSVC 19.44 Release 本批目标构建/针对性 CTest  | 构建通过；**59/59** 回归通过，不称全测试样例原生矩阵已运行            |
| 本次外部 asn1tools Record + C PER        | 通过，范围如上                                       |

完整日志在忽略目录 `build-rev1-quick/batch5-*.log`。中断的旧轮次不记为通过；最终门禁使用全部修复后的源码。
本批未重跑完整 Clang 测试样例 CTest；Clang 证据限于所列运行时 fuzz，MSVC 限于 59 项针对性门禁。

<a id="remediation-validation--rev-20-24--复跑"></a>

<a id="remediation-validation--复跑"></a>

#### 复跑

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build-release -j1
ctest --test-dir build-release --output-on-failure
# Debug + ASN1_ENABLE_SANITIZERS=ON 独立构建目录重复上述门禁，启用 halt_on_error/leak detection。
cmake --build build-release --target asn1_codec_matrix
python3 scripts/validate_installed_package.py --build build-release --output build-package-check
python3 scripts/validate_standard_capabilities.py --compiler build-release/asn1cxx --output build-probes
python3 scripts/validate_standard_oracles.py --driver build-release/tests/asn1_standard_oracle_driver \
  --oracle-root /absolute/path/to/Python_asn1tools
PYTHONPATH=/absolute/path/to/Python_asn1tools python3 scripts/validate_c_per_oracle.py --build-dir build-c-per-oracle
```

Fuzz 使用独立 Clang Debug 构建，开启 `ASN1_ENABLE_SANITIZERS=ON`、
`ASN1_BUILD_FUZZERS=ON`，构建 `asn1_decoder_fuzzer`。把 `tests/fuzz/seeds.txt`
的非注释 hex 行用 `bytes.fromhex` 各写入 corpus 文件后运行：

```sh
build-fuzz/tests/asn1_decoder_fuzzer /path/to/corpus -runs=20000 -max_len=4096 -seed=20261021
```

<a id="remediation-validation--rev-20-24--后续功能与验证空白"></a>

<a id="remediation-validation--后续功能与验证空白"></a>

#### 后续功能与验证空白

以下是本批核查后的明确后续项，不把它们算成现有完整能力；无需为本批一次实现整个标准：

1. 继承约束交集、FROM、普通 WITH COMPONENTS、COMPONENTS OF、数字版本扩展组；
   添加可靠 IR/语义规划后逐项开放，当前失败比错误线路更可控。
2. DER/CER 时间规范词法、命名 BIT STRING 尾零、schema 感知的 CER SET 顺序，
   COER/XER DEFAULT/未知扩展策略及全部构造替代形式需继续补独立正反例。
3. 精确 REAL 模型与文本可逆性设计（REV-23b 后续），字符字符域、现代时间/IRI；
   当前 double/profile 不得表述为任意精度或全标准覆盖。
4. 独立 CER/COER/CXER/E-XER 指令外部对照工具仍有空白；本批 fuzz 只覆盖所列 C++ 解码入口，
   未执行长期 fuzz，未覆盖 C11 全部原语及完整生成 PDU。

新增证据与后续改动需同步 [项目 skill](skills/asn1-remediation/SKILL.md)，
历史第一至四批数字保持原样。

<a id="remediation-validation--rev-25-28"></a>

<a id="remediation-validation--rev-25-28-验收"></a>

### REV-25-28 验收

日期：2026-10-21。环境：Windows PowerShell / WSL Ubuntu 24.04，GCC 13.3，
CMake 3.28.3；Windows MSVC 19.44 的针对性结果另列。

<a id="remediation-validation--rev-25-28--修改与证据"></a>

<a id="remediation-validation--修改与证据-1"></a>

#### 修改与证据

| 项目     | 现象与修改                                                          | 验收边界                                                    |
| ------ | -------------------------------------------------------------- | ------------------------------------------------------- |
| REV-25 | 旧目录发射直接截断目标文件；现改为 stage 生成、hash manifest、最后发布完成标记，失败回滚，拒绝同目录并发 | 三形态双 TU；12 入口线路相等；split SCC；名称/归属/损坏 manifest/符号链接/形态切换 |
| REV-26 | 仅凭约定无法阻止新增依赖；增加源码 include 检查、CMake 实际传递链接检查和独立公共头 CTest        | 正反例包括辅助函数中转越界、emitter include 字符串合法；组件独立构建              |
| REV-27 | C emitter 尚未实现；先建立只依赖 IR 的借用规划，接入 C++ PER 根位图/扩展数与整数成员约束       | 本批设计与 C++ 阶段完成；真实 C emitter 接入验收仍依赖 P2-4 子任务 3+         |
| REV-28 | 修正仅 `.cpp` 是桥梁、过期计数/缓存设计、C++20 span/errno、C 原语即完整后端等表述         | 对应契约、README/索引/架构/状态/使用/开发/TODO/skill 同步；历史日志保留         |

详细规则见 [生成产物](architecture.md#generated-artifacts) 与 [共享规划](architecture.md#emission-planning)。
既有“换 basename 留旧文件”与“通过符号链接写 `/dev/full` 才发现失败”的测试预期
已按新契约更新：前者清理未修改旧归属，后者在覆盖前拒绝非普通文件，保留符号链接
且 CLI 不报告 `wrote`。没有删除用户文件以让门禁通过。

<a id="remediation-validation--rev-25-28--实际命令与结果"></a>

<a id="remediation-validation--实际命令与结果"></a>

#### 实际命令与结果

| 门禁                            | 最终结果                                                              |
| ----------------------------- | ----------------------------------------------------------------- |
| GCC Release 全量 CTest          | **442/442**                                                       |
| GCC Debug ASan+UBSan 全量 CTest | **442/442**；leak detection/halt_on_error 开启                       |
| 7 测试样例 ×12 实际入口语法矩阵           | **84/84**；语法检查不冒充链接或互操作                                           |
| manifest 独立审计                 | 84 个 manifest、168 个产物 SHA256 和 84 个源依赖 SHA256 逐项相等                |
| 三种生成形态                        | 各自双 TU 链接并实际 UPER 调用；12 入口跨形态线路相等测试通过                             |
| split 模块依赖环                   | `SplitModules.DependencyCycleMergesUnitAndCallsBothNamespaces` 通过 |
| 公共头                           | **48/48 C++17**；**20 个 C 头 × C11/C++17 = 40/40**；合计 88 个独立编译      |
| 自动分层                          | 123 个源码/头；字面 include 正反例及 CMake 直接/间接越界负例通过                       |
| 独立组件                          | 仅编译器、仅运行时（含纯 C）分别配置和构建通过                                          |
| 安装包                           | 纯 C、运行时、compiler、宿主生成四类消费者通过；纯 C 无 C++ 标准库依赖                      |
| MSVC 19.44 Release            | 针对性 **37/37**；含名称、空格/中文 API 目录与三形态调用                              |
| 项目 skill/文档                   | quick_validate 通过；本批相对文件链接检查通过                                    |

门禁日志在忽略目录 `build-rev1-quick/batch6-*.log`，不将临时构建产物提交。
Release 最后又在独立组件/安装检查前刷新一次，避免新增路径测试只存在于源码。
分层扫描补入 CLI 路径后，另复跑 sanitizer 的 `Architecture.Layering` 与 Windows 脚本正反例；
该变更只扩展静态检查范围，不改变编译产物。

独立组件命令：

```sh
cmake -S . -B build-rev1-quick/components/compiler -DASN1_BUILD_TESTS=OFF   -DASN1_BUILD_COMPILER=ON -DASN1_BUILD_RUNTIME=OFF -DASN1_BUILD_C_RUNTIME=OFF
cmake --build build-rev1-quick/components/compiler -j1
cmake -S . -B build-rev1-quick/components/runtime -DASN1_BUILD_TESTS=OFF   -DASN1_BUILD_COMPILER=OFF -DASN1_BUILD_RUNTIME=ON -DASN1_BUILD_C_RUNTIME=ON
cmake --build build-rev1-quick/components/runtime -j1
python3 scripts/validate_installed_package.py --build build-rev1-release   --output build-rev1-quick/batch6-package
```

MSVC：`cmake --build build-rev4-msvc --config Release --parallel 1 --target` 后接
`asn1_emit_cache_tests asn1_standard_outline_tests asn1_standard_inline_tests
asn1_standard_split_tests asn1_split_cycle_tests`；再运行
`ctest --test-dir build-rev4-msvc -C Release --output-on-failure -R
'EmitCache|EmitOutput|EmissionPlanning|GeneratedPolicy|SplitModules|Architecture.Layering'`。

```sh
cmake -S . -B build-rev1-release
cmake --build build-rev1-release -j1
ctest --test-dir build-rev1-release --output-on-failure
cmake -S . -B build-rev1-san
cmake --build build-rev1-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-rev1-san --output-on-failure
python3 scripts/validate_codec_matrix.py --compiler build-rev1-release/asn1cxx \
  --cxx g++ --output build-rev1-quick/batch6-matrix
python3 scripts/validate_layering.py --cmake cmake
```

<a id="remediation-validation--rev-25-28--未宣称的能力"></a>

<a id="remediation-validation--未宣称的能力"></a>

#### 未宣称的能力

- 不宣称 C emitter、`--lang c` 或生成 C PDU 已完成；REV-27 的 C 消费验收保留未完成。
- 输出事务是正常进程下的失败保护，不是全目录瞬间原子替换/断电恢复协议。
  未故障注入真实磁盘故障或断电；回滚失败会保留 stage/backup 路径供恢复。
- MSVC 只跑列出的针对性测试；不冒充 Windows 全测试样例或 ASan+UBSan。
- include 扫描只覆盖字面 include 指令；新宏 include/特殊 include 根需扩充门禁。
  CMake 检查实际 target 依赖，但不能证明外部预编译库没有隐藏行为。
- 本批未新增独立 ASN.1 外部对照工具。既有冻结向量与 12 入口比较属于全量 CTest，
  不将自身往返/线路相等冒充新的外部标准认证；第五批 live oracle/fuzz 记录不改写。
- 未复跑最低 CMake、AArch64 设备或长时间 fuzz；历史结果只在原记录中保留。
- 许可证/完整 ABI 政策、统一解码预算与能力矩阵 STD-01..05 仍是后续工作。

## 历史状态补充记录（2026-10-16～21）

以下由旧状态页迁入，保留原验证数字和当时待办，仅供追溯；其中“当前”“未排期”均指记录时点，最新结论见状态页。

- **2026-10-21，第六批**：REV-25/26/28 完成产物事务、ODR/命名/归属回归、自动分层/公共头门禁及文档收口。REV-27 完成最小共享规划和 C++ 首批接入；真实 C emitter 消费验收仍待 P2-4 子任务 3+。GCC Release/ASan+UBSan 各 **442/442**，实际矩阵 **84/84**，MSVC 针对性 **37/37**；组件独立构建与安装消费者通过。见 [第六批验收](changelog.md#remediation-validation--rev-25-28)、[产物契约](architecture.md#generated-artifacts)、[规划边界](architecture.md#emission-planning)。

- **2026-10-21，REV-20..24**：补标准能力与 REAL/字符/时间支持范围；修复约束保真、常量/值参数解析、整数窄化及规范策略缺口。GCC Release/ASan+UBSan 各 **430/430**，实际矩阵 **84/84**，MSVC 针对性 **59/59**；外部对验、Clang sanitizer fuzz 与后续能力项见 [第五批验收](changelog.md#remediation-validation--rev-20-24) 和 [能力矩阵](status.md#standard-capabilities)。

- **2026-10-21，REV-13..19**：修复缓存生成器指纹/输入顺序及有界并发；补 JERI 产品入口与控制段错误诊断，隔离 OER/XML 组合策略；落地离线、宿主/目标构建和安装包。GCC/Clang Release、ASan+UBSan 四轮各 **392/392**，实际入口矩阵 **84/84**；MSVC、最低 CMake 3.16.8 与 AArch64 链接证据见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。设备运行与完整标准语义审计仍未宣称完成。

- **2026-10-20，REV-10..12**：修复 E-XER 属性错误传播与三种生成形态回归；
  SourceFile / IR BigInt 改为 Result，数字/文件/线程资源失败规范化；补 Result
  nodiscard 与访问前置条件、线路偏移单位和字段路径。Release / ASan+UBSan
  各 **370/370**；公共 C++17 头 47/47 严格编译、历史语法矩阵 84/84。
  C++ API/布局变化及验证边界见 [第三批验收](changelog.md#remediation-validation--rev-10-12) 和
  [C++ 错误契约](architecture.md#cpp-error-contracts)。当时 REV-13/14 与 JERI 入口待后续，现已在第四批落地。

- **2026-10-20，REV-04..09**：C PER 原语修复 MSB-first 尾位、有符号大整数、
  长度片段链与变长/扩展约束；统一 `per_core.c` 以 Variant 参数化，旧 ABI
  保留为 UPER 包装。默认 64KiB decoder 限制保持；大值门禁单独编译 128KiB
  源副本。Release 与 ASan+UBSan 均 **348/348**；故障检查 498，两个 PER
  门禁各 4473 检查，独立 asn1tools 双向差分 234 用例、184 冻结短向量。
  具体支持范围、外部对照工具排除项和旧 raw 内容 API 语义见
  [第二批验收记录](changelog.md#remediation-validation--rev-04-09) 与
  [声明/测试登记](development.md#c-runtime-tests)。这不代表完整 C emitter 已完成。

- **2026-10-20，REV-01..03**：C11 BitWriter 整次预留容量并传播 OOM/溢出；
  BigInt 临时解析成功后提交，传播分配失败与 limb 限制；新增 C 状态码 8/9
  （原 0..7 不变），核对并修正原语输出/所有权契约及失败清理。
  `Asn1CRuntimeFailureContracts` 含 424 个内部检查；Release 与 ASan+UBSan
  全量 CTest 均 **346/346** 通过。生命周期说明见
  [architecture.md](architecture.md#c-runtime-contracts)，命令与门禁证据见
  [REV-01..03 验收记录](changelog.md#remediation-validation--rev-01-03)。该批不含 PER 线路修复；
  后续结果见上方 REV-04..09，不据此推定全部 C 编码规则或 C emitter 已完成。

- **P2-4 子任务 1 + 2（已完成）**：`include/asn1/c_runtime/*` 头骨架

  + `src/c_runtime/*.c` 纯 C11 实现 + `asn1::c_runtime` 静态库 +
    `Asn1CRuntimeSmoke` 链接+运行门禁。当前声明实现和解码/负例范围以
    [测试登记](development.md#c-runtime-tests) 为准；JER/XER 读取器 init 只绑定输入，
    不等于完整解析器。命名约定 `asn1_<codec>_<primitive>`
    替代 C++ `namespace asn1::<codec>::<primitive>`（flat prefix，无嵌套）。
    Byte-equivalent 的设计为后续子任务 3+（`emit_c/*` + `--lang c`）做
    外部对照工具复用留下基础。

- **P2-4 子任务 3+（未排期）**：`src/codegen/emit_c/*` emitter + CLI
  `--lang c` + `EmitOptions::lang` + 全测试样例编译门禁 + P1-3 外部对照工具
  复用。子任务清单见 [TODO 清单](changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c)。

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
  SymbolTable&&)`（按 module 键合入，worker 间 duplicate 检测由 declare
  自身保证）。`Analyzer::analyze` 形参 `parallelism` 不破坏现有调用（默认
  0 = 串行）。

- CLI：`asn1cxx --parallel <N>`（N ∈ 0..1024，0 = 串行 = 默认；>1 使用
  有界任务批次（1 保持串行））；`--help` 已文档化。

- 验证：`asn1_parallel_analyzer_tests`（10 项）：`Diagnostics::merge` 序列
  / 空合并 / `error_count` 聚合；`SymbolTable::merge` 合入 + duplicate
  检测；模块测试样例（tiny / multi-module import / undefined type / unknown
  field / duplicate symbol）下 `--parallel 0` 与 `--parallel 16` diagnostics
  **逐字节相同** + error_count 一致。`asn1_cli_tests` 新增
  `Cli.ParallelModeProducesByteIdenticalOutput`：LPP 14.3.0 测试样例上
  `--parallel 0` 与 `--parallel 16` 生成的 `generated.hpp/.cpp` 在
  namespace 替换后**逐字节相同**。Release + sanitizer（ASan+UBSan）全量
  ctest **344/344** 通过，零 ASan/UBSan 报错。

- 文档：`docs/development.md` P3-1 条目增「`lower_pass` 保持串行」说明 + 「后续
  工作」段落；`docs/changelog.md` P3-1 落地条目；`status.md` 当前阶段 +
  构建与测试状态 + 剩余工作更新。

- `--emit-cache`（默认）/ `--no-emit-cache` 已落地：缓存目录
  `<emit-dir>/.asn1cxx-cache/`，每单元（flat: `default`；split: 模块 ns
  ident）一个 `.cache` 文件，存 `(sha256(content_key), {output_path: sha256})`；
  命中条件 = 键一致 **且** 每个输出文件的当前 sha256 与记录的 sha256
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
  `--no-emit-cache` 不命中。CLI 冒烟测试 4 项（默认二次运行 hit /
  `--no-emit-cache` 无 note / flag 顺序最后胜出 / 首运行状态行是 "wrote"
  而非 "reused (cached)"）。

- 算法 / 编译时优化：`ContentCache::compute_key` 接受外部传入的 per-source
  sha256 映射，Emitter 一次性算好后通过 `cache_source_hashes()` 复用，
  split 模式 N 个单元只对每个 .asn 源读 + SHA-256 一次；测试样例 compile
  gate（flat / single-codec / split）从 `-O3` 降为 `-O1`，仍 ABI 兼容，
  大测试样例编译时间显著下降；CMake 全局加 `-pipe -fno-plt`（sanitizer
  构建不加以避免影响 sanitizer 栈回溯）。


## 历史任务与完成登记

以下保留原 REV/CGEN 验收要求、完成标记及旧功能路线，仅用于回归与追溯；其中历史规划和估时不构成当前待办。当前所有未完成事项统一在 [todo.md](todo.md)。CGEN runner 只读取本节中有明确边界的旧任务清单，不把整份变更历史作为待办。

<a id="remediation-tasks"></a>

## 工程评审修正 TODO（2026-10-20）

本清单记录逐项交给 AI 实现与验收的修正，以各 REV 的状态和验收记录为准。初始清单基于静态评审，当时未复测历史 345/345；后续执行结果分别记录，不覆盖历史证据。每项执行前必须重新核查当前代码和调用路径：已变化的问题不得机械修改，待核查项不得直接宣称缺陷。

原有功能路线及 Phase49 子任务仍见 [TODO 清单](changelog.md#roadmap)，当前实现状态见 [status.md](status.md)。本文件是评审修正执行清单，使用 `REV-xx` 编号，避免与原 P1/P2/P3 编号混淆。

<a id="remediation-tasks--执行约束"></a>

### 执行约束

- compiler（lexer/parser/semantic/IR/codegen）不得链接运行时；运行时不得依赖 AST/IR/前端头。只有生成产物连接类型和运行时；inline 模式的桥梁在生成头中。
- C++ 业务失败使用 Result<T>，仅 std::bad_alloc 可以向外传播；C 使用状态码和输出参数，不以 C++ 包装替代纯 C11 实现。
- UPER/APER 共用参数化核心；C 后端也使用 Variant/上下文，不复制两份算法。
- 保留自研 BigInt、零第三方运行时依赖、既定 12 个产品入口以及三种产物形态。
- 保留 DER=BER+策略、COER=OER+策略、CXER=XER+策略。
- 保留 7 测试样例 × 12 实际编码规则入口的产物语法门禁；per 是 UPER|APER 组合别名，不计为独立入口。
- Release、ASan+UBSan 双构建及 asn1c/asn1tools 差分纪律不降低；工具能力不足时明确记录，不伪造外部对照工具结果。
- 每次只处理一个编号或明确列出的紧密依赖项；不顺手扩大重构，不覆盖用户未提交修改。
- 完成项必须记录实际命令、结果和未验证范围。缺工具只能标记“实现完成、验证待补”，不能勾选完成。

<a id="remediation-tasks--状态与验收规则"></a>

### 状态与验收规则

所有复选框初始均未完成。状态可记录：待办、进行中、实现完成/验证待补、完成、不适用（附证据）。

每项最少交付：问题复现或调用链证据、最小修复、针对性验证、相关文档同步。线路正确性不得仅用本实现自身 round-trip 证明。

影响 runtime/生成语义的修改，应运行针对性测试、相关 fixture/codec 门禁，并在合入前通过规定的 Release 与 sanitizer 全量门禁；生成产物变更覆盖相关 inline/out-of-line/split。纯文档修改检查链接与表述即可，不为文档编写实现镜像测试。

<a id="remediation-tasks--第一批c11-内存安全与失败传播优先处理"></a>

### 第一批：C11 内存安全与失败传播（优先处理）

<a id="remediation-tasks--rev-01-严重bitwriter-扩容失败与容量溢出"></a>

#### REV-01 【严重】BitWriter 扩容失败与容量溢出

- [x] 状态：完成（2026-10-20）；依据：已发现未检查扩容结果的复制路径。

- 完成：ensure 返回状态，整次写入预留并检查溢出；take 失败不推进有效位，clear/take 后可安全复用。验证：故障门禁 424 检查、Release/sanitizer 各 346/346；详见 [验收记录](changelog.md#remediation-validation--rev-01-03)。

- 范围：`src/c_runtime/bit_io.c`，对应头及 C 测试。

- 修改：ensure 返回状态；所有调用方检查；检查位计数加法、字节换算及倍增溢出；失败不越界、不推进有效写入长度。

- 验收：逐分配点失败注入；已有容量不足、空写入器、大请求均正确失败；ASan/UBSan 无越界；成功路径字节不变。

- 依赖：无。仅增加最小内部测试注入机制，不引入全局可变的生产分配器配置。

<a id="remediation-tasks--rev-02-严重bigint-不得吞掉分配失败或-limb-上限错误"></a>

#### REV-02 【严重】BigInt 不得吞掉分配失败或 limb 上限错误

- [x] 状态：完成（2026-10-20）；依据：`bigint_mul_add_u32` 忽略 `bigint_push_limb` 失败。

- 完成：临时解析后提交，逐层传播 OOM/limb 上限；覆盖各分配点失败、9864/9865 位十进制上限相邻值、序列化失败和释放。验证及实际命令见 [验收记录](changelog.md#remediation-validation--rev-01-03)。

- 范围：`src/c_runtime/bigint.c`、`include/asn1/c_runtime/bigint.h`。

- 修改：算术辅助函数逐层返回失败；失败不提交错误数值；核查容量乘法与 limb 上限。

- 验收：十进制解析逐分配点失败、limb 上限、负数与零；失败对象可安全释放；不返回截断数值的成功结果。

- 依赖：无；错误码约定与 REV-03 对齐。

<a id="remediation-tasks--rev-03-重要c-状态码所有权与失败后对象状态"></a>

#### REV-03 【重要】C 状态码、所有权与失败后对象状态

- [x] 状态：完成（2026-10-20）；依据：OOM 与 LengthOverflow 混用、输出契约与实现不一致。

- 完成：保留旧错误码，新增 OOM/ResourceLimit；修正原语输出提交与失败清理，核对并记录逐 API 生命周期与游标例外。见 [C 契约](architecture.md#c-runtime-contracts) 和 [验收记录](changelog.md#remediation-validation--rev-01-03)。完整 CMake 双构建、公共头和历史 84 组语法矩阵通过；未宣称补齐 JERI 或第二批线路能力。

- 范围：`include/asn1/c_runtime/*`、`src/c_runtime/*`。

- 修改：区分 OOM、资源限制、长度溢出；保留既有错误码数值；明确 init/free/reuse、返回内存释放方式和 borrowed view 生命周期；逐函数核对失败时 out 是否保持不变。

- 验收：分配失败、截断、重复初始化/复用契约测试；失败可释放且无泄漏；状态名完整。禁止要求 free 任意未初始化结构。

- 依赖：可先制定契约，最终整体验收在 REV-01/02 后。

<a id="remediation-tasks--第二批c11-位操作与-per-线路正确性"></a>

### 第二批：C11 位操作与 PER 线路正确性

<a id="remediation-tasks--rev-04-严重msb-first-尾位和短源输入"></a>

#### REV-04 【严重】MSB-first 尾位和短源输入

- [x] 状态：完成（2026-10-20）；原依据：快路径取低位，慢路径取高位；短源可能少写仍成功。

- 完成：高位 MSB-first 尾位、源容量/NULL 校验、超过 64 位明确失败；独立逐位模型覆盖 8×18 组合。实测见 [第二批验收](changelog.md#remediation-validation--rev-04-09)。

- 历史进展：第一批已补短源/NULL 预检与完整预留；本批补齐尾位语义和完整参考矩阵。

- 范围：`src/c_runtime/bit_io.c`，BIT STRING 调用路径。

- 修改：统一 MSB-first 语义；验证源长度、NULL/长度组合与 BIT STRING 容量；位数超过接口能力时明确报错，不静默截断。

- 验收：`0xA0` 写 3 位得到高位 `101`；0..7 起始偏移 × 0..17 位长度与独立逐位参考实现一致；短源返回失败。

- 依赖：REV-01。

<a id="remediation-tasks--rev-05-严重无约束-integer-补码编码及符号解码"></a>

#### REV-05 【严重】无约束 INTEGER 补码编码及符号解码

- [x] 状态：完成（2026-10-20）；原依据：C PER 使用无符号幅值，负数解码可成功得到正数。

- 完成：signed/unsigned API 分离，最短补码、符号保护及负数/大数解码，空/冗余表示拒绝。独立差分及分配失败证据见 [第二批验收](changelog.md#remediation-validation--rev-04-09)。

- 范围：`src/c_runtime/{bigint,per}.c`、相关公开头。

- 修改：明确区分 unsigned magnitude 与 signed two's-complement 转换；PER 无约束整数使用有符号最短内容；拒绝非法空内容。

- 验收：0、±1、±127、±128、±255、±256、INT64_MIN/MAX、>64bit；128 内容为 `00 80`；无约束输入 `01 FF` 解得 -1；与外部对照工具向量对验。

- 依赖：REV-02/03。

<a id="remediation-tasks--rev-06-严重per-长度分片链"></a>

#### REV-06 【严重】PER 长度分片链

- [x] 状态：完成（2026-10-20）；原依据：大长度仅写 C1..C4 后直接写完整内容。

- 完成：more 显式 determinant、完整字符串/open type 片段链、多片与结束零；默认上限保持，测试副本验证 65537/131072 字节；scalar 辅助函数不丢弃分片状态。见 [第二批验收](changelog.md#remediation-validation--rev-04-09)。

- 范围：`src/c_runtime/per.c`、`include/asn1/c_runtime/per.h`。

- 修改：区分单个 determinant 与完整片段链，内容层交错处理长度/片段及结束；解码保留继续片段信息；未支持路径明确 Unsupported。

- 验收：127/128、16383/16384/16385、32768、49152、65536/65537；整片终止、连续片段、截断及非法标记；外部对照工具字节及交叉解码。

- 依赖：REV-01/03/04；不要只修改 encoder 而保留不匹配 decoder。

<a id="remediation-tasks--rev-07-重要bit-string-变长范围及整数约束分类"></a>

#### REV-07 【重要】BIT STRING 变长范围及整数约束分类

- [x] 状态：完成（2026-10-20）；原依据：BIT STRING 解码直接使用 upper；整数上下界缺省处理不对称。

- 完成：明确长度描述符、实际位 length、根内/根外扩展、半约束、无界及完整 int64；SIZE 上界 64K 阈值独立对验。大于 int64 的有界描述符/不连续集合不在本原语接口能力内，详见 [验证边界](changelog.md#remediation-validation--rev-04-09)。

- 范围：C PER 约束描述与 encode/decode。

- 修改：区分 fixed/constrained/semi-constrained/unconstrained/extensible；变长 BIT STRING 写读实际长度；核查全 int64 域和非法 range。

- 验收：SIZE(0)、SIZE(1..16)、固定 3/8/9 位、上下界、空范围、半约束、全域整数。尚未实现类别必须失败而非近似编码。

- 依赖：REV-04/05/06。

<a id="remediation-tasks--rev-08-重要c-per-统一-variant-接口"></a>

#### REV-08 【重要】C PER 统一 Variant 接口

- [x] 状态：完成（2026-10-20）；原依据：当前 C API 没有 variant，APER 尚延期。

- 完成：`per_core.c` 单核心以 Variant 参数化，旧 ABI UPER 包装；前置位的整数/长度/字符串/open type 对齐和尾字段验证；234 独立双向用例、184 冻结向量。见 [第二批验收](changelog.md#remediation-validation--rev-04-09)。

- 范围：`include/asn1/c_runtime/per.h`、`src/c_runtime/per.c`。

- 修改：增加统一 Unaligned/Aligned 参数或上下文；旧接口如需兼容，作为 UPER 包装；APER shim 只转发参数，不复制算法。

- 验收：对齐前存在 1 位的整数、字符串、长度及开放类型向量；同核心覆盖两 variant；未支持类别明确拒绝。

- 依赖：REV-07；本项先完成接口与已支持原语，其他原语按能力矩阵排期。

<a id="remediation-tasks--rev-09-重要重构-c-smoke-的验证口径"></a>

#### REV-09 【重要】重构 C 冒烟测试的验证口径

- [x] 状态：完成（2026-10-20）；原依据：部分 round-trip 命名仅检查编码，读取器只初始化也被描述为解码能力。

- 完成：旧冒烟测试补实际解码，新正确性/大值门禁各 4473 检查、失败测试 498 检查；[声明登记](development.md#c-runtime-tests) 明确 init 与编码规则能力边界；Release/sanitizer 各 348/348、历史矩阵 84/84。见 [第二批验收](changelog.md#remediation-validation--rev-04-09)。

- 范围：`tests/c_lang/*`、`tests/CMakeLists.txt`、C 运行时注释。

- 修改：保留 Asn1CRuntimeSmoke；增加独立 primitive/故障测试；逐声明登记是否链接、调用、解码及负例已覆盖；修正过度承诺。

- 验收：REV-01..08 对应回归用例进入 CTest，Release/sanitizer 均执行；用例数量不是验收替代品。

- 依赖：随前述任务补测试，最后汇总。

<a id="remediation-tasks--第三批c-错误模型与生成代码"></a>

### 第三批：C++ 错误模型与生成代码

<a id="remediation-tasks--rev-10-严重e-xer-属性解码错误传播"></a>

#### REV-10 【严重】E-XER 属性解码错误传播

- [x] 状态：完成（2026-10-20）；依据：生成 `if (tmp)` 忽略失败，并生成 std::stoll。

- 完成：属性缺失与非法值分流，数值完整解析/区间与宿主检查；三种产物实际链接运行负例，错误带字段名。 Release/sanitizer 各 370/370、公共 C++17 头 47/47、历史矩阵 84/84；见 [第三批验收](changelog.md#remediation-validation--rev-10-12)。

- 范围：`src/codegen/emit_xer.cpp`、E-XER 生成代码运行测试。

- 修改：区分 missing/invalid，按 mandatory/optional/default 处理；数值转换返回 Result、完整消费并检查目标范围；失败立即传播。

- 验收：缺必填属性、非法布尔、12x、溢出、非法 BigInteger；三种产物行为一致，无业务异常且不静默保留默认值。

- 依赖：无，可在 C 运行时修复后独立执行。

<a id="remediation-tasks--rev-11-严重消除编译器业务异常及-badalloc-吞捕获"></a>

#### REV-11 【严重】消除编译器业务异常及 bad_alloc 吞捕获

- [x] 状态：完成（2026-10-20）；依据：SourceFile/IR 显式 throw、数字转换及 filesystem 抛异常 API、catch(...)。

- 完成：SourceFile/IR BigInt 返回 Result，OID/CLI from_chars，目录 error_code、写失败检查，线程资源回退；bad_alloc 保留，容量失败入口诊断；PER restricted-string 失败传播。 Release/sanitizer 各 370/370、公共 C++17 头 47/47、历史矩阵 84/84；见 [第三批验收](changelog.md#remediation-validation--rev-10-12)。

- 范围：support、IR、parser、CLI、代码生成缓存与文件操作调用链。

- 修改：用 Result、from_chars、filesystem error_code；核查 std::async/system_error、容器 length_error 等边界；业务错误不靠异常，bad_alloc 不被转换为语法错误。

- 验收：不存在文件、输出不可写、非法数字/溢出、资源失败分别产生稳定错误；既有诊断位置保留。按入口拆小提交，不机械加 noexcept。

- 依赖：与 REV-10/14 协调，不要求全局禁用异常编译选项。

<a id="remediation-tasks--rev-12-优化result-访问契约和诊断单位"></a>

#### REV-12 【优化】Result 访问契约和诊断单位

- [x] 状态：完成（2026-10-20）；依据：std::get 前置条件、偏移 bit/byte 含义不明确。

- 完成：Result nodiscard 与 assert/终止契约，补 move-only/const/死亡测试；显式偏移单位/字段路径及源码字节列，公共头严格独立编译。 Release/sanitizer 各 370/370、公共 C++17 头 47/47、历史矩阵 84/84；见 [第三批验收](changelog.md#remediation-validation--rev-10-12)。

- 范围：`include/asn1/common/result.hpp`、运行时 Error、Diagnostics。

- 修改：增加 nodiscard；明确 value/error 程序员错误契约；区分 byte/bit 偏移、字段路径与编译期源范围；不向运行时引入 IR。

- 验收：公共头 C++17 独立编译；正确/失败/移动访问有针对性检查；文档区分源码位置与线路位置。

- 依赖：REV-11 的错误接口改动稳定后。

<a id="remediation-tasks--第四批缓存并行构建与门禁"></a>

### 第四批：缓存、并行、构建与门禁

<a id="remediation-tasks--rev-13-重要生成器更新必须使缓存失效"></a>

#### REV-13 【重要】生成器更新必须使缓存失效

- [x] 状态：完成（2026-10-21）。

- 完成：生成器内容指纹与缓存格式键；纳入有序源路径/全部依赖，读取失败及无来源 IR 禁用缓存；11 项缓存回归与实际换指纹 miss 验证通过；见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

- 范围：`src/codegen/emit_core.cpp`、`emit_cache.*`、缓存测试。

- 修改：引入可靠的生成器/缓存格式修订标识；覆盖所有输出相关选项与输入依赖；不可读输入禁止命中；程序化 API 无 source_paths 时核查模型变化是否能失效。

- 验收：相同 ASN.1 更换生成器标识后 miss；选项、依赖、模型变化 miss；同输入稳定 hit；损坏/缺失输出重建。

- 依赖：无；影响输出修复的验证显式禁用旧缓存。

<a id="remediation-tasks--rev-14-重要--parallel-n-真正限制并发"></a>

#### REV-14 【重要】--parallel N 真正限制并发

- [x] 状态：完成（2026-10-21）。

- 完成：declare/resolve 有界批次与模块序合并；N=1/2/16、空输入/97任务峰值及诊断回归，CLI 严格数字解析；lower/tag 保持串行；见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

- 范围：`src/semantic/analyzer.cpp`、CLI 数字解析。

- 修改：固定 worker 或有界调度；0 保持串行；诊断按模块索引合并；from_chars 完整解析 N；lower/tag 保持串行。

- 验收：并发峰值不超过 N；N=1/2/16、空模块、大量模块；诊断和产物与串行一致；1x/溢出被拒绝。

- 依赖：与 REV-11 对齐资源失败处理。

<a id="remediation-tasks--rev-15-重要核清-jeri-定义与真实-12-入口"></a>

#### REV-15 【重要】核清 JERI 定义与真实 12 入口

- [x] 状态：完成（2026-10-21）。

- 完成：补 JERI 产品位/CLI/生成函数，共享 X.697 JER 实现；核清非 canonical JSON，补 ARRAY 控制段并明确拒绝其余未支持控制段；见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

- 范围：CLI、`include/asn1/codegen/emit.hpp`、JER/JERI runtime/IR、文档。

- 修改：核查 JERI 的项目约定及 X.697 指令语义；保留 12 产品入口目标；明确与 JER 的关系并补齐选择/生成路径；per 仅为组合别名。

- 验收：12 实际入口有能力登记，JERI 不再误称标准 canonical JSON；--codec jeri 有明确行为，未实现能力清楚报错；不为凑数量引入无标准依据的线路格式。

- 依赖：先完成语义核查，再做实现；若约定不明，记录需要用户决策的具体差异。

<a id="remediation-tasks--rev-16-重要修正-codec-矩阵并增加真实链接调用"></a>

#### REV-16 【重要】修正编码规则矩阵并增加真实链接调用

- [x] 状态：完成（2026-10-21）。

- 完成：7 测试样例 ×12 实际入口语法门禁 84/84；修正家族依赖，12 单入口及组合三形态真实调用/编码比较；修复 OER/XML 组合策略覆盖；见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

- 范围：`tests/CMakeLists.txt`、门禁脚本、能力登记表。

- 修改：保留并自动化 7 测试样例 × 12 实际入口 -fsyntax-only；单入口使用正确运行时目标；代表测试样例建可执行程序真实调用；补组合掩码。

- 验收：单独/组合中同编码规则输出一致；per 等于 uper,aper；默认、重复项、排列行为明确；缺失符号能被链接门禁发现。

- 依赖：REV-15；可先修已有 11 入口映射，不虚报完整 12。

<a id="remediation-tasks--rev-17-重要cmake-编译器隔离与最低版本"></a>

#### REV-17 【重要】CMake 编译器隔离与最低版本

- [x] 状态：完成（2026-10-21）。

- 完成：目标级编译器条件与 C11 属性；GCC/Clang 四轮各392/392；MSVC产品构建及40项回归；CMake3.16.8离线配置和代表门禁2/2；大型fixture保留全部sanitizer检查并优化调试信息/增量复制；见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

- 范围：根/测试 CMake。

- 修改：target-scoped 编译器/平台条件；C 运行时与冒烟测试设置 C_STANDARD=11/REQUIRED/EXTENSIONS OFF；处理 DOWNLOAD_EXTRACT_TIMESTAMP 版本条件；sanitizer 能力明确。

- 验收：GCC/Clang Release+ASan/UBSan，MSVC 支持范围构建，声明最低 CMake 版本离线配置；不能把 MSVC ASan 称为 UBSan。

- 依赖：无；无工具链时记录待补，不声称通过。

<a id="remediation-tasks--rev-18-优化依赖离线与宿主目标构建"></a>

#### REV-18 【优化】依赖离线与宿主/目标构建

- [x] 状态：完成（2026-10-21）。

- 完成：固定GTest下载SHA256，测试关闭/源码覆盖离线通过；宿主工具路径及两阶段流程；x86_64宿主生成、AArch64 C/C++链接通过，目标不在宿主执行；见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

- 范围：CMake、usage-guide。

- 修改：GTest URL_HASH，验证离线覆盖及关闭测试路径；提供宿主 asn1cxx 路径或两阶段流程；交叉编译不默认执行目标测试。

- 验收：断网配置；宿主工具生成、目标编译器编译最小消费程序。

- 依赖：REV-17。

<a id="remediation-tasks--rev-19-优化安装包与独立消费者"></a>

#### REV-19 【优化】安装包与独立消费者

- [x] 状态：完成（2026-10-21）。

- 完成：安装导出/版本/组件与独立构建；GCC和MSVC各四类安装消费者通过；纯C无C++运行库依赖，私有GTest不进入SDK；见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

- 范围：CMake、最小 C/C++ 消费者。

- 修改：install/export/config/version；仅运行时和仅编译器消费；核查 transitive dependencies。

- 验收：安装后新目录 find_package 并真实链接运行；纯 C 消费不引入 C++ 标准库。

- 依赖：REV-17/18。

<a id="remediation-tasks--第五批标准语义结构策略与对验"></a>

### 第五批：标准语义、结构策略与对验

<a id="remediation-tasks--rev-20-重要建立标准能力及-oracle-矩阵"></a>

#### REV-20 【重要】建立标准能力及外部对照工具矩阵

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：标准版本/层次/C 与 C++/oracle 矩阵，21 项实际接受或拒绝探针；未支持项明确登记。验收见 [第五批记录](changelog.md#remediation-validation--rev-20-24)。

- 范围：新建能力矩阵文档、status/README 的引用。

- 修改：按 X.680/681/682/683、X.690/691/693/696/697 登记版本/勘误、语法、分析、IR、生成、运行时、独立验证；区分 C/C++；保留原任务延期状态。

- 验收：每个“支持”有代码或测试证据；未知标待核查，合法但未支持标 Unsupported；不把历史测试数字当本次运行结果。

- 依赖：REV-15；其余可提前盘点。

<a id="remediation-tasks--rev-21-重要约束保真和高频语法专项核查"></a>

#### REV-21 【重要】约束保真和高频语法专项核查

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：修复 REV-21a..g：未知约束、别名约束、负 SIZE、空交集、导入/循环常量、值参数及实例键、native INTEGER 解码窄化。验收见 [第五批记录](changelog.md#remediation-validation--rev-20-24)。

- 范围：parser、constraints、semantic、IR、相关 emitter。

- 修改：追踪影响线路的未知约束，必须可靠归一化或报错；核查并交差集/FROM/WITH COMPONENTS、COMPONENTS OF、自动标签、参数化、对象集、关系约束、扩展组。

- 验收：每个核实缺陷拆成子编号（如 REV-21a），附最小 schema、实际/预期线路和修复；没有证据的结构不直接判缺失。

- 依赖：REV-20；本项不是一次实现整个 X.680 系列。

<a id="remediation-tasks--rev-22-重要规范策略与-bercer-合法替代表示"></a>

#### REV-22 【重要】规范策略与 BER/CER 合法替代表示

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：修复 CER 原语规范策略、REAL、DEFAULT、重复 SET、片段/unused bits、长度/递归限制，三种产物真实回归；剩余能力见 STD-02。验收见 [第五批记录](changelog.md#remediation-validation--rev-20-24)。

- 范围：BER/DER/CER、OER/COER、XER/CXER 运行时与结构 emitter。

- 修改：核查 DEFAULT、SET/SET OF、时间/REAL 词法、unused bits、最短形式、未知扩展、constructed/indefinite 路径。

- 验收：合法基础规则但非法规范规则的负例；CER 999/1000/1001、BIT STRING 分片及嵌套 EOC；单独/组合行为一致。

- 依赖：REV-20/21；按家族分别提交。

<a id="remediation-tasks--rev-23-重要real-profile-与字符时间长尾类型"></a>

#### REV-23 【重要】REAL 支持范围与字符/时间长尾类型

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：公开准确 REAL/字符/时间支持范围；修复二进制精度/范围、负零、OER float 舍入及 JER bigint 误读；精确模型按 STD-03 延期。验收见 [第五批记录](changelog.md#remediation-validation--rev-20-24)。

- 范围：REAL、StringKind、parser/IR、各编码规则能力文档。

- 修改：先公开准确支持范围；核查 decimal/base2、特殊值及精度/范围行为；核查 OID-IRI、TIME/DATE 等及指令覆盖，明确拒绝尚未支持能力。

- 验收：合法但不支持不静默丢精度或改语义；需精确 REAL 模型时另立设计子项，保持零第三方运行时依赖。

- 依赖：REV-20；无需在此次修复批次强行实现所有长尾类型。

<a id="remediation-tasks--rev-24-重要独立-oracle负例与-fuzz"></a>

#### REV-24 【重要】独立外部对照工具、负例与 fuzz

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：新增带独立来源元数据的向量/冻结重放，复跑 Record 与 C PER 外部对验，新增种子回归和 Clang sanitizer 20000 次 fuzz。验收见 [第五批记录](changelog.md#remediation-validation--rev-20-24)。

- 范围：tests、golden、scripts、development。

- 修改：记录外部对照工具版本/commit/参数/schema/value/字节；区分规范字节相等与非唯一编码交叉解码；补 CER 条文负例、COER/XML/JER 对验；添加截断、资源限制和 fuzz 测试驱动。

- 验收：明确无外部外部对照工具的空白；外部对照工具分歧有条文仲裁；C/C++ 双向与外部验证均可重跑；fuzz 种子回归进入门禁。

- 依赖：按 REV-05/06/20/22 修复进展逐步加入。

### 标准能力后续工作

当前未完成的 STD-01～05 已单独登记在 [项目 TODO](todo.md#standard-work)，不属于已完成 REV 批次的验收结论。

<a id="remediation-tasks--第六批产物分层与后端扩展"></a>

### 第六批：产物、分层与后端扩展

<a id="remediation-tasks--rev-25-重要三种生成形态的-odr命名和输出可靠性"></a>

#### REV-25 【重要】三种生成形态的 ODR、命名和输出可靠性

- [x] 状态：完成（2026-10-21）；三形态双 TU/线路相等、split SCC、可移植命名、stage/manifest、并发拒绝及归属清理已验证，见 [第六批验收](changelog.md#remediation-validation--rev-25-28)。

- 范围：emit_core、emit_cache、CLI、生成产物测试。

- 修改：多 TU inline 链接；out-of-line 重复/遗漏符号；split 递归模块和名称碰撞；basename/namespace 校验；产物 manifest、依赖信息、临时写入和旧文件清理。

- 验收：三种形态实际调用并线路一致；失败不留下伪成功产物；Windows 文件名/路径有测试；不删除非本生成器拥有的文件。

- 依赖：REV-13/16；同目录并发策略先明确拒绝或隔离，不默默扩成复杂锁系统。

<a id="remediation-tasks--rev-26-优化分层和公共头自动门禁"></a>

#### REV-26 【优化】分层和公共头自动门禁

- [x] 状态：完成（2026-10-21）；include/实际传递链接正反例、88 个独立公共头编译及 compiler-only/runtime-only 构建通过，见 [第六批验收](changelog.md#remediation-validation--rev-25-28)。

- 范围：CI/脚本、CMake、公共头。

- 修改：compiler 禁 include/link 运行时，运行时禁 include AST/IR/前端；公共头独立 C++17/C11 编译；生成产物明确为桥梁例外。

- 验收：负例可被门禁捕获；compiler-only/runtime-only 构建通过；不得误伤 emitter 输出字符串中的运行时 include。

- 依赖：REV-17。

<a id="remediation-tasks--rev-27-优化cc-emitter-复用语义规划"></a>

#### REV-27 【优化】C/C++ emitter 复用语义规划

- [x] 状态：完成共享规划复用验收（2026-10-24，CGEN 第 2 批）。真实生成 C 的描述符、选择器调用及生命周期检查消费标签、presence、约束和扩展组决定；C++ 线路回归通过。该历史验收不代表当时 C 编码规则已实现；后续 CGEN 第 4–7 批已完成声明支持范围的
  PDU/oracle、双 C 84 矩阵与原 C++ 84 矩阵，见 [CGEN 第 7 批验收](changelog.md#cgen-validation--batch-7)。见 [规划设计](architecture.md#emission-planning)、[第六批验收](changelog.md#remediation-validation--rev-25-28) 和 [CGEN 第 2 批验收](changelog.md#cgen-validation--batch-2)。

- 范围：IR 到代码生成的标签、presence、约束、扩展组及选择器规划。

- 修改：先设计最小语言无关规划层，依赖 IR/common；C/C++ 后端只表达类型和调用；禁止链接运行时；不为抽象而整体重写现有 emitter。

- 验收：代表测试样例的 C++ 线路无回归；C 后端复用同一决策；与原 P2-4 子任务 3+ 分步衔接。

- 依赖：REV-08/20/21；C emitter 大规模扩展前完成设计审查。

<a id="remediation-tasks--rev-28-重要文档收口与-phase49-继续条件"></a>

#### REV-28 【重要】文档收口与 Phase49 继续条件

- [x] 状态：完成（2026-10-21）；统一桥梁/支持口径，修正旧功能规划与 C 头注释，补输出/ABI/所有权/线程安全/资源及许可证边界，更新项目 skill；见 [第六批验收](changelog.md#remediation-validation--rev-25-28)。

- 范围：README、docs/README、architecture、status、usage-guide、development、todo、changelog、C 头注释。

- 修改：修正标准归属、支持口径及端序；统一生成产物桥梁表述；清理 todo 已落地方案的误导陈述；补所有权、ABI、线程安全、资源限额及发布许可证待办。

- 验收：文档数字对应实际测试报告；未复测不更新成功计数；已完成与未来设计分开；历史 changelog 不改写成未曾发生的结果。

- 依赖：每项随修同步，末尾汇总。C emitter 扩展前至少关闭 REV-01..07 的错误成功路径。

<a id="remediation-tasks--推荐交给-ai-的顺序"></a>

### 推荐交给 AI 的顺序

1. REV-01 → REV-02 → REV-03 → REV-04：先保证失败传播和位操作基础。
2. REV-05 → REV-06 → REV-07 → REV-08 → REV-09：修复 C PER 并关闭 primitive 门禁。
3. REV-10 → REV-11 → REV-12：兑现 Result 错误模型。
4. REV-13 → REV-14 → REV-15 → REV-16 → REV-17 → REV-18 → REV-19：缓存、并行及构建门禁。
5. REV-20..24：能力盘点后逐个修复标准语义，不把核查清单一次扩成全标准实现。
6. REV-25..28：产物可靠性、自动分层门禁、C 后端规划与文档收口；REV-28 始终随修更新。

REV-13/17 可在首批后提前处理，以减少旧缓存和工具链问题干扰验证。以上为执行建议，不自动启动任何修复。

<a id="remediation-tasks--每次复制给-ai-的任务模板"></a>

### 每次复制给 AI 的任务模板

```text
请执行 docs/development.md 的 REV-XX，只处理该项和必要的最小依赖。
先读取仓库 AGENTS.md、相关接口、实现、测试及当前 git diff；保留已有用户修改。
重新核实问题，给出最小复现或调用链证据，再修复；待核查内容不要直接当缺陷。
遵守文档中的分层、Result、纯 C11、共享 PER 核心、零第三方运行时依赖约束。
完成该项验收及规定门禁，记录实际命令/结果/未验证范围；缺少工具不能宣称通过。
同步相关 docs，并检查更新 docs/skills/asn1-remediation/SKILL.md 中对应的已验证流程。
同步本项状态及相关文档，不把未完成项目勾选，不扩大功能范围。
最终说明修改文件、修复行为、测试证据、剩余限制和建议下一项。
```

<a id="remediation-tasks--完成记录模板"></a>

### 完成记录模板

```text
REV-XX：状态 / 日期
证据：最小输入、实际结果、预期结果或调用链
修改：文件与关键行为
验证：实际命令、通过/失败/未执行、工具链及 oracle 版本
限制：未支持/未验证内容
后续：依赖项或下一编号
```

<a id="roadmap"></a>

## 待办清单（P1-3 / P2-4 与相关项）

> 2026-10-20 工程评审后的修正执行清单见 [TODO 清单](changelog.md#remediation-tasks)。
> 使用 REV-xx 独立编号，包含优先级、依赖、验收及逐项交给 AI 的任务模板；
> 本文保留原功能规划与历史落地记录，不代表评审修正已完成。

本文档登记 P2-2 之后不再继续的剩余项，并按优先级、依赖、预估工作量列出
落地步骤。每条给出最小可行子任务，便于逐步合入。状态变更走
[status.md](status.md)，变更日志走 [changelog.md](changelog.md)。

<a id="roadmap--p2-3--增量代码生成已落地持续回归"></a>

### P2-3 — 增量代码生成（已落地，持续回归）

当前实现位于 `src/codegen/emit_cache.*`，不是 CLI 内部工具类。
键包含生成器内容指纹/格式版本、完整选项、有序源路径及内容 hash、相关模块依赖。
flat 为一个单元，split 按模块依赖 SCC 合并后的单元缓存；没有源依赖信息的手工 IR
禁用缓存。命中仍核验产物 hash，mtime 不是正确性依据。

`--emit-cache` 默认开启，`--no-emit-cache` 关闭；缓存位于输出目录内部
`.asn1cxx-cache/`。缓存不可读/损坏时重发射。升级指纹、依赖顺序与命中/未命中
字节一致性证据见 [第四批验收](changelog.md#remediation-validation--rev-13-19)。

REV-25 加入整批 stage/manifest 发布、旧产物归属清理以及同目录并发拒绝，
契约见 [生成产物](architecture.md#generated-artifacts)。不是多进程共享缓存或等待锁服务。
后续改动保留缓存命中、失效、手改输出、三形态切换和用户文件保护回归。

<a id="roadmap--p3-1--前端并行分析"></a>

### P3-1 — 前端并行分析

<a id="roadmap--原始目标历史规划实际实现见下"></a>

#### 原始目标（历史规划，实际实现见下）

规划时 `Analyzer::analyze` 串行跑四趟（`declare_pass` → `resolve_pass` →
`lower_pass` → `tag_pass`，见 `src/semantic/analyzer.cpp`）：

- `declare_pass`：每个模块独立声明符号集。
- `resolve_pass`：每个模块的引用按**全模块符号表**解析。
- `lower_pass`：AST → IR；每个模块的 lowering 互不依赖。
- `tag_pass`：标签分配；当前对全局扫描态。

声明阶段每个模块相互独立（模块内声明靠模块自身），最容易并行。其余
三趟有跨模块依赖，要按依赖图并行。

<a id="roadmap--关键风险"></a>

#### 关键风险

**诊断输出逐字节可重现是硬要求**。当前 `Diagnostics` 是 `std::vector` 顺
序 append，warning/error 顺序在测试里被断言（`asn1_semantic_tests` 有序
号和内容快照）。改成多线程后会改变 append 顺序，进而破坏：

1. 现有测试测试样例的顺序断言。
2. `format(const Diagnostic&)` 在 `print` 时的全局顺序。
3. 跨模块 warning（如 undefined imported type）原本按模块文件序输出，
   并行后变不可预测。

**强制约束**：并行跑出的 diagnostics 集合必须与串行跑出的逐字节一致
（同样的 Diagnostic 个数 + 同样的 severity + 同样的 where + 同样的
message + 同样的顺序）。

<a id="roadmap--设计"></a>

#### 设计

- **诊断合并协议**：每个 pass worker 持有本地 `Diagnostics`（无锁），
  pass 结束后按模块序 merge（worker_id 0..n-1 与 input module index
  对齐）。`merge` 时按 module_index 升序拼接结果，让最终顺序确定。这要
  求每个 worker 严格处理「只属于自己模块」的 diagnostics，不在 worker
  间共享跨模块 diagnostics。
- **declare_pass 并行**：每个 worker 持一个 `SymbolTable` 分片，只
  collect 自己模块的符号。pass 结束后 `SymbolTable::merge(other)` 串行合
  并。当前 `SymbolTable` 是单个 map，需扩展为支持按 module 键合并。
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

<a id="roadmap--实际落地2026-10-17"></a>

#### 实际落地（2026-10-17）

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
- 当前 lower 在 78 模块 RRC 8.6.0 测试样例上约 ~10 s（per todo 估时），
  占比 ~30% 的 analyze 时间；declare/resolve 并行化已经吃掉主要的
  Amdahl 收益（声明 + 引用检查的 O(M·A·fields) 部分），lower 串行
  不会成为新的瓶颈。**
- 后续 Phase 解：保留接口扩展点 `lower_pass(int)` 已接受 parallelism
  参数；实现 per-module arena + 重映射 deferred（独立工作项，可与
  `parse_modules()` 流水线化合并设计）。

❌ **`tag_pass` 保持串行**（与原 todo 一致，标注原因）。

### 子任务

| #   | 内容                                                                                                                                      | 估时  | 验证                                                                                                 |
| --- | --------------------------------------------------------------------------------------------------------------------------------------- | --- | -------------------------------------------------------------------------------------------------- |
| 1   | 在 `Diagnostics` 加 `merge(Diagnostics&& other)`：按 index 重排追加。**重要**：合并顺序由 caller 决定；先实现串行版本做对照                                           | 1 h | ✅ `asn1_parallel_analyzer_tests.DiagnosticsMerge*`（3 项）：merge 后 items 顺序与预期一致；`error_count` 聚合；空合并 |
| 2   | `SymbolTable` 加按 module 分片支持：`declare(module_id, names)`、`merge(SymbolTable&& other)`。保留单线程兼容路径                                         | 2 h | ✅ `asn1_parallel_analyzer_tests.SymbolTableMerge*`（2 项）：合入 / duplicate 检测                          |
| 3   | `Analyzer::declare_pass` 改为 `std::async(std::launch::async, modules, ...)`：每个 worker 处理一个 module；结束后串行 merge                            | 1 h | ✅ `declare_pass(int)` worker 路径已落地                                                                 |
| 4   | `Analyzer::resolve_pass` 改为 per-module worker + 重试 sweep。worker 持完整符号表只读；residual 未解析项在 sweep 阶段按模块序解析                                  | 3 h | ✅ `resolve_pass(int)` worker 路径已落地；保留 import-visibility 串行（依赖 `modules_`）                          |
| 5   | `Analyzer::lower_pass` 改为 per-module worker + arena merge。lowered type arena per-worker，pass 结束按 module index 串行拷贝到顶层 arena             | 3 h | ❌ **保持串行**（per-module arena + 重映射 deferred，理由见上）                                                   |
| 6   | `Analyzer::tag_pass` 保留串行（标注原因）                                                                                                         | 0 h | ✅                                                                                                  |
| 7   | `asn1cxx --parallel <N>` CLI；`Diagnostics::merge` + 顶层 merge orchestrator；`Analyzer::analyze` parallel 参数                               | 1 h | ✅ CLI 冒烟测试 `Cli.ParallelModeProducesByteIdenticalOutput` 1 项                                       |
| 8   | 可重现性门禁：`tests/CMakeLists.txt` 新增「parallel vs serial equivalence」门禁：LPP + S1AP + RRC 跑两遍（`--parallel 0` 与 `--parallel 16`），产物 `diff` 必须空 | 2 h | ✅ 单元测试覆盖 5 个测试样例模块 + CLI 字节相等门禁（LPP）                                                               |
| 9   | sanitizer build + Release build 都过；ctest 344/344 + 新门禁 11 项                                                                             | 1 h | ✅ Release **344/344** + sanitizer **344/344**（0 ASan/UBSan）                                        |
| 10  | 文档：`status.md` 性能数字；`changelog.md` P3-1 条目；`architecture.md` 标「并行分析」段落（依赖图、Diagnostics 合并协议）                                            | 1 h | ✅ status.md / changelog.md 已更新；architecture.md 设计已落进 status.md「当前阶段」段                              |

**实际落地：~9 h（declare + resolve 并行；lower 串行 + deferred）。**

### 验证（2026-10-17）

- `asn1_parallel_analyzer_tests`（10 项）全过：Diagnostics::merge 字节
  级序列 / error_count 聚合 / 空合并；SymbolTable::merge 合入 /
  duplicate 检测；5 个测试样例模块（tiny single-module / multi-module
  import / undefined type / unknown field / duplicate symbol）在
  `--parallel 0` 与 `--parallel 16` 下 diagnostics **逐字节相同** +
  error_count 一致。
- `Cli.ParallelModeProducesByteIdenticalOutput`：LPP 14.3.0 测试样例上
  `--parallel 0` 与 `--parallel 16` 生成的 `generated.hpp/.cpp` 在
  namespace 替换后**逐字节相同**（含 sanitizer）。
- Release + sanitizer 全量 ctest **344/344** 通过，零 ASan/UBSan 报错。
- 手工验证 RRC 8.6.0（largest 测试样例，78 模块）：serial vs `--parallel 16`
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

| 风险                              | 缓解                                                                                                                                             |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| 现有测试样例 / 金标对验对诊断顺序敏感            | 并行前所有现有测试样例必须串行 baseline 跑通、记录产物 hash；并行化后产物与 baseline 字节级 diff                                                                                |
| sanitizer 化（ASan + UBSan）漏掉数据竞争 | P3-1 强制加 TSan 临时构建验证（`-fsanitize=thread`）；CI 上跑一次确认无 race                                                                                      |
| 缓存错命中 = 旧产物混进构建                 | P2-3 强制产物 SHA256 后校验；CI 跑 `--no-emit-cache` 与带缓存两遍做 diff                                                                                       |
| `release` 路径与 `debug` 路径行为不同    | 并行路径与串行路径共享同一 `Analyzer::analyze`，仅 orchestrator 不同；条件编译禁，避免路径分叉                                                                               |
| 协议消息外部对照工具捕获成本高、易引入 CI 阻塞       | P1-3 复用现有 `scripts/gen_per_golden.py` / `gen_tlv_golden.py` 框架；新增 `gen_protocol_vectors.py` 走"小批量 + 三 family 一次性"策略；外部对照工具文本 checked-in，CI 不联网 |

---

<a id="roadmap--p1-3--3gpp-协议消息级金标准向量对验"></a>

### P1-3 — 3GPP 协议消息级金标准向量对验

### 目标

[status.md](status.md) 当前 6 项「剩余工作」中的 2 项本质上是同一件事：

- **完整模块编译 CI / 3GPP 编解码对等验证**：「剩余为 3GPP 协议消息级完整向量（逐消息 PDU）的对验；建议：扩展金标准向量覆盖面，逐步引入协议级消息向量」
- **PER 位级正确性外部对验（协议级）**：「剩余为 3GPP 协议消息级完整向量的对验；建议：扩展 per_golden 向量覆盖面」

P1-3 把它们合并为一项 TODO：**抓取 / 生成 3GPP 真实协议消息字节 → 与
asn1-toolkit 编码字节逐位对验**。覆盖 PER/APER、BER/DER/OER、CER
三大家族，每家族 3 个候选 PDU：

- **LPP-Message**（`tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn`，LPP 14.3.0）
- **S1AP-PDU**（`tests/fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn`，S1AP 14.4.0）
- **RRC DL-DCCH-Message**（`tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn`，RRC 8.6.0，78 模块最大测试样例）

### 为什么现在做

P3-1 落地后 frontend 已经能处理 3GPP 大测试样例串行+并行；`status.md`
「编译门禁状态」表里三个测试样例 × 12 编码规则全 `-fsyntax-only` 零错误。
**但 emit 成功 ≠ 编解码对等** —— 当前三个测试样例没有任何捕获的协议
消息字节可对比，只验证了"生成代码可编译"，没验证"生成代码编出的
字节 = 真实 3GPP 消息字节"。

asn1c / asn1tools 都已对这三个测试样例提供 BER↔UPER 的双向外部对照工具
桥（见 `scripts/gen_per_golden.py` / `gen_tlv_golden.py` 已用的 asn1c
外部对照工具路径）。P1-3 是把这些外部对照工具从"综合构造 per_golden"扩到"真实
3GPP 消息"。

### 设计

- **三家族共用外部对照工具基础设施**：新增 `scripts/gen_protocol_vectors.py`，
  走 `asn1c` BER→UPER/APER/OER 桥（同 `gen_per_golden.py` 既有路径）。
  asn1tools 处理 BER/DER/OER 同 family 内值等价性。三家族各产一份
  `.txt`，每条 `(pduname, codec, byte)`, checked-in, CI 离线用。
- **测试样例选择策略**：每家族挑 3 个代表性 PDU（见上"3 个候选 PDU"）。
  每个 PDU 至少抓 3 条样例：边界（min / max / 空字段）+ 典型
  （real-world 抓包样本）。
- **典型样本来源**：从公开 3GPP 测试向量（ASN1C 项目的
  `c/tests/sample.asn1` 对应 binary、`openairinterface5g` 的 S1AP 测试
  测试样例等）取真实捕获 PDU；不可得时用 asn1c `ber_encode` 跨
  family 外部对照工具桥自举（BER↔UPER/APER 由 asn1c 仲裁，OER 由 asn1c
  仲裁 BER↔OER 字节，CER 无外部外部对照工具走"条文推导 + DER 值等价"
  同既有 `tests/cer/` 策略）。
- **字节级对验**：`scripts/gen_protocol_vectors.py` 产 checked-in
  `.txt`，新增 `tests/protocol/test_protocol_vectors.cpp` 跑 emit →
  编码 → diff；Release + sanitizer ctest 必过。
- **测试样例编译门禁 + 协议字节级门禁双轨**：
  - 测试样例编译门禁（已有）：`add_custom_command` emit → `add_library(
    STATIC)` → `g++ -std=c++17 -fsyntax-only` 全 0 错误。验证
    "emit 成功 = 代码可编译"。
  - 协议字节级门禁（新增）：emit + 编码 + byte-diff 真实 PDU。
    验证 "emit 成功 = 编解码对等（vs 真实 3GPP 字节）"。
- **失败模式**：
  - asn1c / asn1tools 各自已知偏差（已在 P1-1 / P1-2 处理）：`ber`
    SET 无规范序、`oer` asn1c `oer_encode` 已知 bug 等。新增条目
    记录在 `scripts/gen_protocol_vectors.py` 头部同既有策略。
  - asn1c / asn1tools 都错的极小概率：单 case X.680/X.691/X.696 条文
    仲裁走 ITU PDF（与 `references/per-differential-validation.md`
    `tlv-oer-differential-validation.md` 既有 PDF 仲裁路径一致）。

### 子任务

| #   | 内容                                                                                                                                                                                                                                                                           | 估时  | 验证                                                                                                                                                                  |
| --- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | 新增 `scripts/gen_protocol_vectors.py`：选 3 测试样例 × 3 PDU 类型（min / max / 空 / 典型）× 5 编码规则（uper,aper,ber,der,oer）= ~45 条外部对照工具；走 asn1c/asn1tools 既有 BER↔family 桥                                                                                                                   | 4 h | 脚本能复现 generate 已有 `tests/per/golden/` 56 条 + `tests/tlv/golden/` 16 条外部对照工具（同测试样例既有 per_golden.asn 子集）；产出 `tests/protocol/golden/protocol_golden_vectors.txt` ~45 条 |
| 2   | 新增 `tests/protocol/test_protocol_vectors.cpp`：emit 三个测试样例 → 抓取每条外部对照工具 → `encode` → diff → 报告 byte-level 一致；Release + sanitizer 都跑                                                                                                                                           | 3 h | Release **344/344 + N** + sanitizer **344/344 + N**；每条外部对照工具字节级对验                                                                                                   |
| 3   | 新增 `tests/protocol/CTestTestfile.cmake` + `tests/CMakeLists.txt` 注册；asn1cxx 测试样例 compile gate 扩到 `asn1_protocol_lpp_up_compile` / `asn1_protocol_s1ap_uper_compile` / `asn1_protocol_rrc_uper_compile`（一次性 emit + 静态库编译 + 真实 PDU 字节 round-trip），3 测试样例 × 5 编码规则 = 15 个新 gate | 2 h | ctest 新增 15 项；全过                                                                                                                                                    |
| 4   | LPP 测试样例端到端实测：选定 3 条 `LPP-Message` 真实捕获（来源：openairinterface5g `lpp_test` 或自捕获），3 编码规则 × 3 sample = 9 条外部对照工具                                                                                                                                                                 | 2 h | `tests/protocol/test_protocol_vectors.cpp` 内 9 case 全过；CI 必跑                                                                                                        |
| 5   | S1AP 测试样例端到端：选定 3 条 `S1AP-PDU` 真实捕获（来源：openairinterface5g `s1ap_test` 或自捕获），同上 9 条外部对照工具                                                                                                                                                                                     | 2 h | 9 case 全过                                                                                                                                                           |
| 6   | RRC 测试样例端到端：选定 3 条 `DL-DCCH-Message` 真实捕获（来源：asn1c `test` 目录或自捕获），同上 9 条外部对照工具                                                                                                                                                                                               | 2 h | 9 case 全过                                                                                                                                                           |
| 7   | CER 协议消息门禁：同 9 条外部对照工具（`S1AP-PDU` 已用 CER 重写，仅条文推导 + DER 值等价）                                                                                                                                                                                                                 | 1 h | 同上；产出 `tests/protocol/golden/protocol_cer_golden_vectors.txt` 9 条                                                                                                   |
| 8   | sanitizer（ASan + UBSan）+ Release 全量 ctest 必过；`asn1_protocol_*_compile` gate 跑一遍字节级对验                                                                                                                                                                                         | 1 h | Release **344/344 + N** + sanitizer **344/344 + N**；无 ASan/UBSan 报错                                                                                                 |
| 9   | 文档：`status.md` 「剩余工作」表合并 P1-3 行 + 「金标准差分验证状态」表新增 protocol-level 行（按 LPP / S1AP / RRC × 5 编码规则计数）；`changelog.md` P1-3 条目；`docs/architecture.md` 标「协议消息字节级对验」段落；`usage-guide.md` 增加 "3GPP 协议消息字节级外部对照工具" 引用                                                                    | 1 h | 文档审阅                                                                                                                                                                |

**总估时：~18 h（两个半工作日）。**

### 排期

- **先 P3-1**（已落地，commit `5856565`）：frontend 并行化已就位。
- **再 P1-3**（本 TODO）：依赖 P3-1 已固化的 frontend 路径，需保证
  --parallel 16 与 --parallel 0 在协议字节级门禁下**同样字节级一致**
  —— 沿用 P3-1 的 byte-equality gate 思路，扩到协议级测试样例。
- 估时 **2.5 工作日**；子任务 1+2+3 可最先做（建立外部对照工具基础设施
  + 编译门禁），4+5+6 串行做（每测试样例一组真实捕获）。

### 不在范围内（防止蔓延）

- 不为协议消息生成"自构造 bitmap"金标准（无外部对照工具，等于自证）；必须
  真实捕获或既有 asn1c 测试测试样例复用。
- 不重写既有 per_golden.asn / tlv_golden.asn 综合构造向量：那些是
  **构造覆盖**（bitmap / 分片 / 约束原语等），P1-3 是
  **协议 PDU 覆盖**，互补而非替换。
- 不抓全消息：3 测试样例 × 3 PDU × 5 编码规则已经覆盖 ~45 条外部对照工具；
  加更多 PDU 是"覆盖面"问题，估时不线性增长。

---

<a id="roadmap--p2-4--c-语言代码生成器-lang-c"></a>

### P2-4 — C 语言代码生成器（`--lang c`）

详细执行清单见 [纯 C11 后端 TODO](changelog.md#cgen-tasks)：7 批、CGEN-01～24，包含模块内源文件分片。各批状态以该清单及对应验收记录为准，未勾选项不代表已实现。

当前 Phase49 已有纯 C11 公共头、原语实现、生成类型/生命周期和 12 入口的已声明
PDU 支持范围，并有真实 RRC UPER+BER 与 OER/JER/XER 独立差分。生产目标是 `asn1_c_runtime`（`asn1::c_runtime`），不链接 C++ 标准库。
C++17 使用项目 `Span` 与 `Result<T>`；C 使用 `asn1_status_t`（0 成功，其他为已登记
错误）和 out 参数，业务失败不使用异常，也不是进程全局 errno。

| 子任务           | 当前状态            | 证据与持续回归                                             |
| ------------- | --------------- | --------------------------------------------------- |
| 1/2 C11 头与运行时 | 已完成登记原语范围       | 独立 C11/C++17 头、纯 C 链接、失败/资源、双 PER Variant 和原语外部对验   |
| 3/4 类型与 CLI   | 第 1～3 批完成       | 默认 C++、C 前缀/冲突诊断、布局/生命周期/DEFAULT、共享规划和命名负例          |
| 5/6 编解码       | 第 4～7 批完成登记支持范围 | 十二入口、三形态、模块内分片、累计资源/错误及电信结构；未实现值域仍明确拒绝              |
| 7 外部对验        | 已完成适用工具的登记范围    | RRC/asn1c、asn1tools 独立与冻结向量；CER/COER/CXER 等工具缺口单独记录 |
| 8/9/10 工程集成   | 第 7 批完成         | 全量双构建、三套 84 矩阵、安装消费者、分层/头文件、文档及 ABI；设备运行与全标准认证不由此推定 |

继续后续 C 家族时维持 REV-01..07 的失败传播与 PER 修复；见
[第一批](changelog.md#remediation-validation--rev-01-03)、[第二批](changelog.md#remediation-validation--rev-04-09)。
C/C++ 运行时平行实现各自语言的算法和状态接口；共享的是语义规划与测试向量，
不能把 compiler 链到任一运行时，也不通过 C++ 包装代替纯 C。
UPER/APER 在各语言内各自共用一个 Variant 参数化 PER 核心。

暂不增加其他目标语言，也不为所谓通用 IR 运行时重写现有架构。
完整类型布局、SEQUENCE OF/CHOICE 泛型表达、ABI 和资源预算在对应 C 子任务设计时
明确，尚未决定的方案不作为当前已实现能力。

<a id="cgen-tasks"></a>

## CGEN 执行清单

创建于 2026-10-23，更新于 2026-10-26。第 1～7 批已在登记范围内完成，第 6 批开放十二入口，第 7 批补齐最终工程门禁，见 [验收记录](changelog.md#cgen-validation--batch-7)。仅类型/生命周期和已支持编解码共用三种形态、分片、清单和发布事务。
本清单细化 [P2-4](changelog.md#roadmap--p2-4--c-语言代码生成器-lang-c) 子任务 3+，
承接 [共享规划](architecture.md#emission-planning) 与 REV-27，不另建一套前端或运行时。
不支持的 C 编码规则/值域在发布前诊断；初始调度的历史证据见 [第 1 批](changelog.md#cgen-validation--batch-1)。

<a id="cgen-tasks--目标与现状"></a>

### 目标与现状

目标：同一个 C++17 编译器选择输出 C++17 或纯 C11；C 产物为 `.h/.c`，只链接
`asn1::c_runtime`，用 C 编译器独立编译/链接/运行，不带 C++ 标准库或异常运行库。
保留原有 C++ 后端及默认行为，不通过修改文件扩展名或包装 C++ 运行时冒充 C 后端。

当前已有 C11 原语、失败契约、PER 两 Variant 与部分独立外部对照工具；
[声明覆盖登记](development.md#c-runtime-tests) 已登记 PER/TLV 值、结构与字节解码；
JER/XER 的 legacy raw 读取器辅助函数只绑定输入；完整解析与结构解码使用
`text_tree.h` 和 `schema_codec.h`，证据见 [第 6 批](changelog.md#cgen-validation--batch-6)。
因此不能直接把 C++ emitter 的函数名替换为 C 前缀，也不能假定 C 运行时已支持全部类型。

C++ `--emit-split` 仍按模块；显式 C `--types-only` 可叠加 `--split-max-types N`，
已解决类型及支持编码规则产物的模块内源分片。12 入口已进入声明的支持支持范围；最终 C 7×12 测试样例矩阵及安装收尾仍属第 7 批。
既有 RRC 是 LTE EUTRA 测试样例，不是 TS 38.331；NR 全文测试须另行引入明确版本的输入。

<a id="cgen-tasks--必须保留的约束"></a>

### 必须保留的约束

- compiler 不 include/link C/C++ 运行时；运行时不依赖 AST/IR/前端；生成产物是桥梁。
- 编译器内部保持 Result/Diagnostics，只有 std::bad_alloc 可向外传播；生成 C 使用
  `asn1_status_t` 与 out 参数，不用异常、全局 errno 或 longjmp 处理业务错误。
- 每种语言内 UPER/APER 共用一套 Variant 参数化 PER 核心；DER/BER、COER/OER、
  CXER/XER 保留基础规则加策略模型，不复制完整算法。
- 自研 BigInt、零第三方运行时依赖、C11/C++17 保持。
- 最终覆盖固定 12 个产品入口；`per` 是 UPER|APER 别名，JERI 不是另一种规范 JSON。
- 保留 out-of-line、单头 inline、按模块 split 三形态；模块内分片是附加能力。
- 按阶段明确 Unsupported，不能生成成功占位函数、忽略约束或把读取器 init 当成解码。

<a id="cgen-tasks--分批执行"></a>

### 分批执行

<a id="cgen-tasks--第-1-批契约能力审计与-cli-调度"></a>

#### 第 1 批：契约、能力审计与 CLI 调度

- [x] **CGEN-01【重要】C 产物与接口设计。** 明确类型/函数前缀、类型布局、参数顺序、
  init/free、借用/拥有、状态传播、输出提交、资源上限及 ABI 版本策略。
  覆盖 INTEGER/BigInt、BOOLEAN/NULL、字符串/位串、OID/REAL、SEQUENCE/SET、CHOICE、
  OF、OPTIONAL/DEFAULT、递归与开放类型；无法完整支持的值域写入能力表。
- [x] **CGEN-02【重要】建立生成需求→C 运行时 API 缺口表。** 对 12 入口逐项登记
  原语、容器、规范策略、完整解码、错误定位和资源限制，不凭头文件存在判定可用。
  对照 C++ 实际调用链，区分可复用原语、必须补实现与当前 Unsupported。
- [x] **CGEN-03【重要】增加 `--lang cpp|c` 与统一调度。** 默认 cpp；C 模式 `.h/.c`；
  C 前缀采用单独 `--c-prefix`，明确 C 模式 `--namespace` 的处理并报冲突，不能静默忽略。
  语言/前缀/拆分选项进入缓存键，未知值或不支持的组合在发布前诊断。
  CLI 骨架未具备编码能力时必须明确失败，不能输出假实现。

验收：设计文档和能力表落地；非法选项负例；原 C++ 三形态/组合入口回归。
依赖：读取 C 运行时契约、REV-01..09、REV-25..28；不重做已完成原语。

<a id="cgen-tasks--第-2-批c-类型生命周期与共享规划"></a>

#### 第 2 批：C 类型、生命周期与共享规划

- [x] **CGEN-04【重要】生成 C 类型和生命周期。** 用有前缀的 struct/enum/union；
  OPTIONAL 有显式存在标志，DEFAULT 保留语义；CHOICE 有 discriminator 与无效初始态；
  OF 保存 data/count/capacity；递归依赖用受控间接引用，不能无限按值展开。
  生成可重复 free、失败后可释放/复用的接口，按需要提供深拷贝，禁止浅拷贝拥有指针。
- [x] **CGEN-05【重要】C emitter 实际消费 `codegen/plan.hpp`。** 复用标签、presence
  顺序、离散约束、扩展组和选择器决定；不足的规划补在语言无关层。
  C 后端只渲染 C 布局和调用，不重新执行语义分析、不复制 C++ 的语义分支。
  有真实生成 C 的调用证据后再关闭 REV-27，不能仅凭规划单测勾选。
- [x] **CGEN-06【重要】命名与公开头。** 覆盖 C11 关键字、保留标识符、模块/类型/字段
  转换碰撞、大小写文件冲突、跨模块限定、重复类型别名；前缀一致应用到辅助函数。
  公共 C 头独立 C11/C++17 可包含；可用 extern C 保护 C 函数声明，但实现仍是纯 C。

验收：小 schema 与跨模块递归 schema 的真实生成 `.h/.c` 编译、链接、init/free/深拷贝；
OOM/部分初始化/CHOICE 切换/多 TU 负例；生成头不包含任何 C++ 专用类型或运行时头。

<a id="cgen-tasks--第-3-批模块内分片与可靠产物管理"></a>

#### 第 3 批：模块内分片与可靠产物管理

- [x] **CGEN-07【重要】按类型依赖规划 `.c` 分片。** 在现有模块 split 外增加可配置的
  单模块源分片；暂定参数 `--split-max-types N`，实施时在设计中固定语义。
  按稳定顺序/依赖 SCC 分组；每个函数只定义一次，跨片调用有声明，不依赖 static 私有
  函数跨 TU 可见。公共类型头先保证正确，测量后再决定是否按依赖拆头。
- [x] **CGEN-08【重要】明确三种形态与分片组合。** 默认 out-of-line 为 `.h/.c`；
  C 单头使用 `static inline`，避免 C99/C11 外部 inline 规则造成遗漏/重复符号。
  `--emit-split` 仍按模块，叠加模块内分片；inline 与源分片冲突明确报错。
  可选按编码规则分开源文件，但不能复制类型声明或公共生命周期定义。
- [x] **CGEN-09【重要】扩展 REV-25 输出事务。** manifest/归属验证接受 `.h/.c` 和分片；
  缓存按稳定单元记录语言/codec/分片参数及依赖；暂存成功才发布，旧文件按归属和 hash
  清理，拒绝同目录并发。切换 C/C++、形态、片数不残留参与链接的旧产物，不删除用户文件。
  提供机器可读源清单与可选 CMake include 清单，明确它们与 depfile 的区别。

验收：单模块含多个类型时确实生成多个 `.c`；改变分片阈值后文件集合正确且线路相等；
片数减少/改语言/失败路径/用户文件保护通过。记录最大文件字节、函数数、编译时间及内存。
按类型数分片不保证严格字节上限，单个巨大类型/SCC 超预算要报告，不能宣称无限可拆。

<a id="cgen-tasks--第-4-批首个完整-uperaper-生成闭环"></a>

#### 第 4 批：首个完整 UPER/APER 生成闭环

进度（2026-10-26）：C11 首个 UPER/APER 生成支持范围已闭环：normally-small/ENUMERATED/OID，生成 C 增加递归
SEQUENCE OF、普通扩展添加、版本括号组、不透明开放类型、Numeric/IA5 字符串及
常见 REAL 线路路径，并通过 Release 与 ASan+UBSan 的三种产物消费者。现已补 OF
长度分片、顶层共享的累计字节/元素预算，以及覆盖两次集合扩容的生成分配失败注入；
asn1c UPER 16K 分片及扩展 CHOICE/trailing-root 向量均逐字节对验；三形态消费者通过
Release 与 ASan+UBSan。更广的标准值域和故障矩阵仍作为后续扩展，不属于当前已声明
支持范围的未实现行为；详见[阶段证据](changelog.md#cgen-validation--batch-4)。

已补齐 C11 生成的扩展 CHOICE、第二个 ellipsis 后的尾根，以及表约束
开放类型的自动 typed decode/encode 和拥有型 copy/free；三种产物形态均有运行测试。

- [x] **CGEN-10【严重】补齐首批 PER 所需 C 原语。** 对照 CGEN-02 补 ENUMERATED、
  normally-small、OID/字符/REAL 等所需能力；已有整数/位串/分片原语继续复用。
  每项明确值域与约束支持，无法支持时诊断，不窄化、不丢扩展位。
- [x] **CGEN-11【严重】生成首个完整 PER 支持范围的编解码编排。** 从小 schema 开始覆盖 SEQUENCE、
  SET/CHOICE、OF、OPTIONAL/DEFAULT、root/trailing-root、扩展单字段/版本组、开放类型。
  同一规划生成 UPER/APER；校验位长度、填充、后续字段位置、片段链与内层开放类型。
- [x] **CGEN-12【重要】生成 PDU 的错误/资源门禁。** 截断、坏长度、非法值、OOM、
  深度/累计字节限制；传播 status 并清理部分对象。逐接口规定 out 与游标失败状态，
  不笼统承诺所有失败均回滚。分配故障 hook 仅进测试副本。

验收：out-of-line/inline/split 均用 C 编译器链接运行；C/C++ primitive 一致性、asn1c
UPER 冻结向量（含新增扩展 CHOICE 与尾根的 C 编码/解码）通过。asn1c 0.9.29
不提供 APER，故 APER 由共享 C/C++ Variant 一致性和生成 PDU 门禁验证。此处关闭的是
已声明 C UPER/APER 首个支持范围；完整 X.691 字符/REAL 值域、未知 CHOICE 扩展保留、
更广 allocation/cursor 矩阵及全类型外部外部对照工具仍明确不支持/未覆盖，必须诊断或作为
后续扩展，不能宣传为完整 ASN.1 PER 实现。

<a id="cgen-tasks--第-5-批berdercer形成用户需要的-uperber-版本"></a>

#### 第 5 批：BER/DER/CER，形成用户需要的 UPER+BER 版本

- [x] **CGEN-13【严重】补齐字节读取器、BER 值与构造值 API。** 不能只使用 tag/length
  辅助函数；覆盖高 tag、长/不定长、EOC、符号整数、字符串、SET/CHOICE、显式/隐式标签、
  未知成员与资源边界；共享底层表示和错误处理。
- [x] **CGEN-14【严重】生成 BER/DER/CER 并保留规范策略。** DER/CER 的 DEFAULT、
  排序、长度、REAL、字符串构造/分片等按 schema 与策略共同处理，不把基础 BER 成功
  当作规范规则合规；原语与结构级负例分开。
- [x] **CGEN-15【重要】UPER+BER 组合与电信接入。** 小 PDU 先验证单 codec/组合、
  C/C++、三形态/分片线路一致，再推进 RRC slice、RRC 全模块、LPP/S1AP。
  全模块遇到的类型/runtime 缺口必须补齐或列为阻断，不能生成未实现 stub。

里程碑 M1：`--lang c --codec uper,ber` 真正生成纯 C 并链接运行；
**RRC 全模块可生成、多 `.c` 编译链接，且代表 PDU 通过真实编解码与独立对验**。
先用 RRC slice 通过不能声称全模块已通过；全模块可编译也不能声称所有 PDU 已验证。

完成记录（2026-10-26）：CGEN-13..15 按上述验收完成，M1 的整份 RRC
1037 类型/9 个 C 源文件已编译链接，代表 PDU 的 UPER/BER 与 live asn1c 0.9.29
字节一致并互解。三形态/分片及单 codec/组合 C/C++ 有 21 个实际消费者。
LPP/S1AP 按 CGEN-15 允许的口径登记明确阻断，不计为通过；完整值域不据此扩大。
Release 461/461、ASan+UBSan 494/494、原 C++ 7×12 语法矩阵 84/84。
详见 [第 5 批验收和限制](changelog.md#cgen-validation--batch-5)。第 6 批已按其声明支持范围完成；第 7 批已完成登记支持范围，见 [最终验收](changelog.md#cgen-validation--batch-7)。

<a id="cgen-tasks--第-6-批oercoer-与-xmljson-家族"></a>

#### 第 6 批：OER/COER 与 XML/JSON 家族

- [x] **CGEN-16【重要】C OER/COER 原语与结构生成。** 数值、长度、presence/扩展、
  开放类型和规范表示；拒绝精度丢失；两入口策略独立，组合掩码不污染单入口。
- [x] **CGEN-17【重要】完整 C JER/JERI。** 实现词法/解析与结构解码、字符串转义、
  精确整数、重复键和未知字段策略、资源限制及已支持指令；读取器 init 不算解析实现。
- [x] **CGEN-18【重要】完整 C XER/CXER/E-XER。** XML 输入解析、字符转义、结构解码、
  标签/属性与指令、规范输出及错误传播；不引入第三方运行时 XML 库。
  未实现指令明确 Unsupported，不用 BASIC-XER 替代其他入口。
- [x] **CGEN-19【重要】12 入口能力登记与组合验证。** 补全 enum/CLI/函数/include/
  运行时依赖/策略/诊断映射；单入口与组合实际编码比较，避免共享全局掩码串策略。

验收：每家族先通过原语与生成 PDU 测试，再开放相应能力；外部对照工具缺口按条文和冻结向量
明确登记，不把 OER/XER 基础编码器冒充独立 COER/CXER 规范外部对照工具。

第 6 批验收：Release 472/472、ASan+UBSan 506/506、原 C++ 7×12 语法 84/84；
39 个 C11 单/组合/三形态消费者、72 组独立 asn1tools 互解及离线双输入回放，
原整份 RRC/asn1c 回归保留。已同步 docs 和项目 skill。
第 6 批时 E-XER UNTAGGED、文本 typed-open 等边界明确 Unsupported；后者的已知表选择
路径由第 7 批补齐，前者继续明确拒绝。
本批历史验收不声明全标准值域；后续第 7 批已通过 Clang 门禁。详见 [第 6 批证据](changelog.md#cgen-validation--batch-6)。

<a id="cgen-tasks--第-7-批工程门禁安装与完成验收"></a>

#### 第 7 批：工程门禁、安装与完成验收

- [x] **CGEN-20【重要】最终 C 生成矩阵。** 7 测试样例 ×12 入口全部生成并独立
  `-std=c11 -fsyntax-only`；同时保留现有 C++ 7×12 门禁。阶段中未支持的组合登记拒绝，
  不算最终 84/84 成功，也不通过删 fixture/codec 缩减目标。
- [x] **CGEN-21【重要】链接/运行/差分矩阵。** 三形态和模块内分片的多 TU；纯 C
  可执行不依赖 C++ 标准库；代表电信 PDU、原语边界、外部外部对照工具、冻结向量、生成
  decoder fuzz；自有 C/C++ 对比与外部验证分别计数。
- [x] **CGEN-22【重要】CMake 与安装消费。** 生成文件名集合变化时采用配置期规划或
  受控清单机制，明确新增类型/新模块时如何重配置，不能只依赖构建期 glob。
  安装后纯 C 工程用 SDK 和宿主生成器编译；交叉构建不在宿主执行目标程序。
  扩展分层门禁识别 C 后端，保持 compiler-only/runtime-only 独立构建。
- [x] **CGEN-23【重要】最终工具链门禁。** Release 与 ASan+UBSan 全量；GCC/Clang
  严格 C11；MSVC 按真实 C11 支持执行并登记边界。区分 MSVC ASan 与 ASan+UBSan。
  不以构建耗时或测试数量替代覆盖证据。
- [x] **CGEN-24【重要】文档与收口。** 同步 README、status、usage、架构、C 契约、
  生成/runtime 能力表、安装示例、changelog 和项目 skill；记录版本与 ABI/许可边界。
  只有真实 C 生成消费共享规划的验收完成才勾选 REV-27；逐项保留未验证范围。

第 7 批执行记录见 [changelog.md](changelog.md#cgen-validation--batch-7)。新增门禁会完整
记录所有失败组合；LPP 命名位串/UTCTime、S1AP 非连续 PER 根与已知表选择的 JSON/XML 开放类型已补齐；
三套原 84 组合门禁、Release/Sanitizer 全量和安装消费均通过。未知动态类型等范围仍明确登记。

里程碑 M2：12 入口与三形态/模块内分片达到所登记支持范围，最终 C/C++ 门禁和安装
消费全部通过；仍不等于整部 ASN.1 标准或全部 3GPP 消息得到认证。

<a id="cgen-tasks--当前可运行的-c-type-only-示例"></a>

### 当前可运行的 C 仅类型示例

以下命令已经由 CGEN-03/07 验收；`128` 是软类型数预算，不是性能承诺或字节上限：

```sh
asn1cxx --lang c --types-only --c-prefix rrc \
  --emit-split --split-max-types 128 --emit-dir build/gen-c \
  tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn
```

当前产物包含一个完整公共 `generated.h`、内部声明头、模块归属的 `.c` 分片、
`generated-sources.json`、`generated-sources.cmake` 和 `.asn1cxx-manifest`。
RRC 测试样例已通过整个类型产物集的 C11 编译、链接和代表生命周期调用；这不表示
C 编码规则可用，也不表示 LTE/NR 全部 PDU 或 TS 38.331 线路行为已验证。文件命名与组合规则
见 [C 输出契约](usage-guide.md#c-output)，批次证据见 [CGEN 第 3 批记录](changelog.md#cgen-validation--batch-3)。

<a id="cgen-tasks--每批执行与完成记录"></a>

### 每批执行与完成记录

按第 1～7 批顺序实施；任一批发现新的必要运行时缺口，登记到对应 CGEN，
补实现/失败契约/线路证据后再开放。每批同步项目 skill 的可复用流程与对应 docs，
不修改已经存在的历史验证数字；本清单不是一次性授权执行所有实现任务。

```text
请按明确授权查阅 docs/changelog.md 中归档的“CGEN 执行清单”第 N 批及必要依赖。
先读当前 diff、项目 skill、C runtime 契约和共享规划，保留用户修改。
不要合并 compiler/runtime，不用 C++ 包装替代纯 C，不拆成两套 UPER/APER。
验证真实生成 C 的编译、链接、运行、错误传播和适用 oracle，不能只检查扩展名。
同步本批状态、验证记录、相关 docs 和 docs/skills/asn1-remediation/SKILL.md。
```

完成记录包含：实际修改、命令/工具版本、通过和未执行范围、产物大小/分片数、
失败路径、外部对照工具来源、剩余阻断。禁止预先填写成功计数或全部勾选。

<a id="cgen-tasks-end"></a>
