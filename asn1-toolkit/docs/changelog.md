# 变更日志

记录约定：**追加式，最新在上**。每条含日期、变更摘要与验证口径。历史 Phase
里程碑表见下；2026-10 起的详细变更按条目记录。当前状态见 [status.md](status.md)。
未排期 / 已规划子任务见 [todo.md](todo.md)。

## 近期变更详录

### 2026-10-24 (CGEN batch 1)

- Define the future C generated ABI/ownership and twelve-entry runtime gap register.
- Add default-compatible `--lang cpp|c`, C-only prefix and conflict diagnostics,
  unified dispatch and language/prefix cache identity. C requests fail before I/O.
- Add real API/CLI/publication and linked three-shape C++ `CGenBatch1.*` tests.
  [Evidence and verification boundaries](cgen-batch1-validation.md).

### 2026-10-23（C 后端夜间 runner）

- 增加 CGEN 逐批非交互 runner 与 PowerShell 包装器，固定 Release/ASan+UBSan、原有测试、
  分层和 84 入口 C++ 矩阵门禁；验收通过后精确暂存本项目文件并本地 commit，不 push。
- 保存状态、日志与提交前 checkpoint，保护验收正文和 runner，失败保留现场，支持显式恢复。
- 临时 Git 仓库中的 runner 自测 10/10 通过；这不是 C 后端功能验收，CGEN 实现尚未开始。
- 原有文档修改单独提交；启动/恢复/预算和验证边界见 [夜间执行说明](c-codegen-nightly.md)。

### 2026-10-21（第六批）

**REV-25/26/28 完成；REV-27 完成规划与 C++ 首批接入，C 消费验收待后续**

- 目录生成采用 stage、备份回滚与输出/源依赖 manifest；同目录并发拒绝，可移植命名校验，旧文件仅按归属和 hash 清理，保护用户文件和符号链接。
- 补三形态双 TU 实际链接调用、split 模块环、名称/路径/归属/坏 manifest 回归；保持 12 入口跨形态线路相等。
- 配置阶段检查 compiler/runtime 传递链接，CI/CTest 检查实际 include，公共头独立 C++17/C11 编译；加入直接/间接越界负例。
- 最小 IR 规划复用 PER presence/扩展数与整数约束决定；C emitter 和 `--lang c` 仍未实现，真实 C 接入条件写入设计和 TODO。
- GCC Release/ASan+UBSan 各 **442/442**，语法矩阵 **84/84**，MSVC 针对性 **37/37**；独立组件和安装四类消费者通过。详见 [第六批验收](rev25-28-validation.md)。
- 同步 README、架构/使用/开发/状态、C 头、产物/规划契约和项目 skill；补 ABI/所有权/线程安全/资源限额与许可发布待办，保留历史验收原数字。

### 2026-10-21（第五批）

**REV-20..24 — 标准能力、约束保真、规范策略及独立验证**

- 建立分层标准/oracle 能力矩阵和 REAL/字符/时间 profile；合法未支持的约束在编译期失败，保留精确 REAL 模型与现代时间类型等后续项。
- 修复导入/别名常量、参数化值边界与实例键；阻止负 SIZE、空交集和未保真约束降成无约束。补三形态真实 PER 位宽回归。
- CER 复用规范原语策略，补 REAL/DEFAULT/重复 SET 与字符串片段检查；限制 CER 内层递归和内容积累，修复 REAL 精度/范围、负零、OER float 窄化和 JER bigint 取值。
- 新增带版本/commit/schema/value 的外部向量、冻结重放和能力探针；复跑 asn1tools 互操作与 C PER 234 个双向用例，Clang sanitizer fuzz 20,000 次无崩溃。完整门禁及局限见 [第五批验收](rev20-24-validation.md)。
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
- GCC/Clang Release 与 ASan+UBSan 四轮各 **392/392**，实际 7×12 语法矩阵 **84/84**；MSVC 产品构建/40 项回归、CMake 3.16.8 代表门禁 2/2、Linux AArch64 编译/链接通过。范围与复跑命令见 [第四批验收](rev13-19-validation.md)。

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
  矩阵 84/84。完整边界见 [第三批验收](rev10-12-validation.md)。

**REV-04..09 — 第二批 C11 位操作与 PER 线路修正**

- 修复 MSB-first 尾位和 >64 bit 静默截断，新增 signed BigInt 内容转换；
  完整分片、末尾零、变长/扩展 SIZE、半约束及全 int64 域处理。
- `per_core.c` 统一 UPER/APER Variant；旧 `per.c` 为 UPER 包装；新 `_ex` 明确
  描述约束和完整值，旧 known-length raw OCTET STRING decode ABI 保留。
- 新增纯 C 正确性/128KiB 大值门禁，补 smoke 实际解码和失败路径；独立
  asn1tools 差分 234 用例、184 冻结向量，对 oracle 能力缺口记录条文仲裁。
- 实测 Release / ASan+UBSan 各 **348/348**；两个 PER 门禁各 4473 检查，故障
  门禁 498 检查。详细命令与最终语法矩阵见 [第二批验收记录](rev04-09-validation.md)。

**REV-01..03 — 第一批 C11 内存安全与失败传播修正完成**

- BitWriter 返回并传播扩容失败，整次操作预留容量，检查计数/倍增溢出；
  take 失败保持有效位，clear/take 后复用不会保留旧 1 位。
- BigInt 使用临时解析后提交，传播 OOM 与 limb 限制；C 错误码 0..7 保持，
  新增 OutOfMemory=8、ResourceLimit=9；修正 PER/BER 原语失败输出与清理。
- 新增测试专用 allocator 与 `Asn1CRuntimeFailureContracts`，生产库没有测试
  hook；新增 [C11 契约](c-runtime-contracts.md) 与 [验收记录](rev01-03-validation.md)。
- 实测：GCC 13.3，Release 与 ASan+UBSan 完整 CTest 各 **346/346**；故障测试
  424 个内部检查；20 头 × C11/C++17 独立编译；历史 7×12 selector 语法矩阵
  84/84。历史列表含 per 组合别名，JERI 缺口仍属 REV-15/16；第二批 PER
  补码/分片/尾位语义未在本批完成。既有 oracle 向量已回归，未重新生成。

**同日较早 — 创建修正计划（当时仅文档变更）**

- 新增 [工程评审修正 TODO](remediation-todo.md)：REV-01..28，按 C11 安全与
  PER 正确性、错误模型、缓存/并行/构建、标准覆盖及后端扩展分批执行；
  每项包含证据性质、修改范围、依赖和验收，并提供逐项交给 AI 的提示模板。
- 该计划创建步骤的验证口径：仅文档与相对链接核查；当时没有执行实现修复，也没有重新运行
  Release、ASan/UBSan 或历史 345 项测试。原功能规划通过 todo.md 链接保留。

### 2026-10-17

**P2-4 sub-task 2 — 纯 C11 runtime 实现（`src/c_runtime/*.c` + `asn1::c_runtime` 静态库）**
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
- 子任务 2 覆盖的头声明：`asn1_c_common.h` 的 status name + byte view/buf
  构造；`bigint.h` 的 init/free/from_decimal/sign/cmp/to_int64/to_bytes；
  `bit_io.h` 全部（BitWriter + BitReader heap-owned，P2-1 bulk bitmap
  fast path 保留）；`bit_string.h` init/free/resize；`byte_io.h` opaque
  byte reader；`ber_tlv.h` tag + length 编码/解码（含 X.690 高 tag 号 +
  长形式长度）；`per.h` 的 `asn1_per_encode_bool/decode_bool/.../bitstring/
  octetstring`；`jer.h` / `xer.h` 的 writer + reader init/puts/printf。
- 门禁升级：原 sub-task 1 `-fsyntax-only` 仅验证头解析；sub-task 2 把
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
    （CER / BER constructed 后续 sub-task 处理）；
  - PER 约束 INTEGER(0..7) value 5 → `0xA0` 双向；PER OCTET STRING
    `[0x11,0x22,0x33]` → `{0x03, 0x11, 0x22, 0x33}`；PER BIT STRING 8-bit
    value `0xA5` → `{0xA5}`；
  - JER writer: 拼出 `{"name":"Alice"}`；XER writer 满 cap 时再 put
    报 `LENGTH_OVERFLOW`。
- Release ctest **345/345** 通过（+1：Asn1CRuntimeSmoke）；Sanitizer 构建
  (ASan+UBSan) **345/345** 通过，零 ASan/UBSan 报错。
- sub-task 3+ 待办：`src/codegen/emit_c/*` emitter + CLI `--lang c` +
  `EmitOptions::lang` + 全 fixture 编译门禁 + P1-3 oracle 复用（`--lang c`
  与 `--lang cpp` 同一 oracle 字节级一致）。子任务清单见
  [todo.md](todo.md#p2-4--c-语言代码生成器-lang-c)。

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
  `SymbolTable::merge(SymbolTable&&)`（按 module key 合入；duplicate
  检测由 declare 自身保证）。`Analyzer::analyze` 新增 `parallelism`
  形参（默认 0 = 串行；现有调用零变更）。
- CLI：`asn1cxx --parallel <N>`（N ∈ 0..1024；0 = 串行 = 默认；>0 启用
  worker 线程池），`--help` 文档化。
- 验证：`asn1_parallel_analyzer_tests`（10 项）— Diagnostics::merge 字节
  级序列 + error_count 聚合 + 空合并；SymbolTable::merge 合入 +
  duplicate 检测；5 个 fixture 模块（tiny single-module / multi-module
  import / undefined type / unknown field / duplicate symbol）在
  `--parallel 0` 与 `--parallel 16` 下 diagnostics **逐字节相同** +
  error_count 一致。`asn1_cli_tests` 新增
  `Cli.ParallelModeProducesByteIdenticalOutput`：LPP 14.3.0 fixture 上
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
  cli_version)`。命中条件：key 一致 **且** 每个输出文件的当前 sha256 与
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
  `--no-emit-cache` 不命中。`asn1_cli_tests` 新增 5 项 CLI smoke（默认
  二次运行 hit / `--no-emit-cache` 无 note / `--no-emit-cache` 后
  `--emit-cache` 重新启用 / 首运行状态行是 "wrote" 而非 "reused
  (cached)" / `--emit-split` 第二次运行命中 unit 缓存）。Release +
  sanitizer 全量 ctest **333/333** 通过，零 ASan/UBSan 报错。
- 编译时优化：fixture compile gate（flat / single-codec / split）的静态库
  编译选项从 `-O3` 降为 `-O1`（ABI 兼容；仅影响 fixture 门禁的墙钟时间，
  不影响运行时 / 单元测试）；CMake 全局加 `-pipe -fno-plt`（sanitizer
  构建不加以避免影响栈回溯）；`ContentCache::compute_key` 接受外部传入的
  per-source sha256 映射（避免 split 模式下对同一 .asn 文件做 N 次
  SHA-256）。

**登记 P2-3 / P3-1 待办清单**（不涉及代码改动）
- 新增 [docs/todo.md](todo.md)：把 P2-3（增量代码生成，~7 h）和 P3-1
  （前端并行分析，~15 h）从 status.md 的「未排期」一栏升级为「已规划、
  未排期」，各子任务含设计要点、估时、验证门槛、可重现性约束。
- [status.md](status.md) 增 todo.md 链接；P2-3 / P3-1 行简化为指向 todo
  的锚点。诊断输出逐字节可重现的硬要求记录在 P3-1 子任务里，作为
  强制约束。
- 没有代码 / 构建 / 测试改动。

### 2026-10-15

**P2-2 收尾 — XML 家族单 codec include 缺口修复 + 单 codec × 全家族门禁**
- 根因：`emit_includes` 的 XML 分支（xer/exer/cxer）未 include
  `<asn1/runtime/byte_io.hpp>`，而 XML 解码器在 CHOICE/ENUMERATED 等
  失败路径会调用 `asn1::make_error`（定义于 `byte_io.hpp`）。全 12-codec
  组合门禁中 PER/OER/TLV 分支都会捎带这个头，所以组合门禁一直绿；任何
  单 codec 选 XML 都必暴露。
- 修复：`emit_includes` XML 分支显式补 `#include <asn1/runtime/byte_io.hpp>`
  （一行 + 注释解释）。
- 回归门禁：`tests/CMakeLists.txt` 新增 LPP × 12 codec 各跑一次独立 emit +
  完整编译（`asn1_lpp_single_{uper,aper,per,ber,der,cer,oer,coer,jer,
  xer,exer,cxer}_compile`），覆盖单 codec 场景的 include 自包含性，
  防组合门禁再掩盖同类缺口。
- 验证：手工 3 fixture × 12 codec = 36 case 全部零错误（含 flat 单 codec
  跑 `--codec xer`/`exer`/`cxer`）；新门禁 12 个静态库编译全过；Release
  320/320 零回归。
- 状态变更：`status.md` 中"XML 家族单 codec include 缺口"由已知失败
  改为 ✅ 已修复；Phase 45 由"部分完成"改为"单 codec × 全家族门禁已就位"。

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
  16 用例 × 3 codec 字节级对验（oracle 策略：asn1tools 为 BER/OER 向量源；
  asn1c 仲裁 DER —— asn1tools 不排序 SET/SET OF，X.690 11.6 要求排序；
  asn1c 的 `oer_encode` 对 SET 值段错误（已知 bug），OER 列保留 asn1tools；
  BER 无规范 SET 序（X.690 8.14），SET 用例 BER 列记 '-'）。
- OER SET 组件改为 X.696 18.2 规范（标签）序 emit，preamble 位序与解码镜像
  同一顺序（此前按定义序，字节与 oracle 不符）。
- 验证：Release + sanitizer 319/319；tlv_golden fixture 全 12 codec
  `-fsyntax-only` 零错误；per_golden 向量零回归。

**P1-2 — CER（X.690）编解码器家族落地**（第 12 个 codec）
- `src/runtime/cer/codec.cpp`（BER 原语策略层）：9.1 构造值不定长包装、
  9.2 + 8.6.4 超 1000 内容八位组分片、11.2 末八位组未用位清零、9.3
  SET/SET OF 标签序（未打标签 CHOICE 按替代项最小标签静态定序）、canonical
  严格性；`emit_tlv.cpp` 按 `TlvKind{Ber,Der,Cer}` 路由；`asn1_cer` 静态库
  + CLI `--codec cer`；`cer_golden` fixture + 10 项字节级金标准（无外部
  CER oracle，条文推导 + DER 值等价交叉验证）。
- 顺带修复全 TLV 家族 SET 解码器缺陷：SET 内未打标签 CHOICE 组件的解码派发
  键此前为 `universal(0, true)` 占位标签（永不匹配），改为 emit 时计算该
  CHOICE 全部替代标签的并集。
- 验证：Release + sanitizer 316/316；五 fixture × 12 codec `-fsyntax-only`
  零错误。

**P2-2 — 大模块分文件/分命名空间 emit（`--emit-split`）**（2026-10-15 主体，
10-15 收尾：XML include 缺口 + 单 codec × 全家族门禁，详见上条目）
- 按 ASN.1 模块边界生成 `<Module>.hpp/.cpp` 对 + 聚合 `generated.hpp`；头内
  类型命名空间与模块名对齐，跨模块引用按 `<Module>::Name` 限定
  （`Emitter::named_ref`，flat 产物逐字节不变）；互相递归模块（依赖图 SCC）
  合并进同一 TU；每模块头自带 `detail::Null`；模块依赖 include 按结构依赖
  计算。多模块验证对象为 S1AP（6 模块，LPP 实为单模块 fixture）。
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
  fixture 静态库编译门禁实际未编译任何生成代码。
- 验证：Release 298/298（新增 2 项 CLI 测试）+ sanitizer 全量 + PER/APER
  金标准向量零回归。

**P0-2 — emit.cpp 按 codec 家族拆分**
- 5,208 行单文件拆为 7 个（`emit_internal.hpp` 共享声明 + `Emitter` 类接口；
  `emit_core` / `emit_per` / `emit_tlv` / `emit_xer` / `emit_jer` /
  `emit_oer`）。依赖图结论：共享面仅 9 个自由函数 + 11 个成员，其余 86 个
  成员全部家族内聚。函数体字节级迁移，RRC UPER 产物与拆分前逐字节一致。
- 验证：Release + sanitizer 298/298；单家族改动重编译 18.9s → 5.5s（JER）/
  6.9s（PER，-j1）；金标准向量零回归。

**P0-3 — 全 codec 产物编译门禁驱动的发射器缺陷修复**
- 首次对三大 fixture + 新 tlv_oer_golden 做 12 codec emit + `-fsyntax-only`
  门禁，暴露并修复 6 类根因（XML 运行时命名空间硬编码、解引用优先级、
  inline 列表 `.value` 误用、嵌套 CHOICE 替代未限定、解码临时变量遮蔽
  —— 新增 `Emitter::temp_counter_`、CHOICE 替代局部变量自遮蔽）及顺带修复
  （X.680 25.3 自动编号规则、default_val_cpp 按字段取默认值、X.690 8.5
  显式标签自然编码、X.696 8.7.2 OER 选择标签 constructed 位）。
- 验证：四 fixture × 12 codec `-fsyntax-only` 零错误；Release + sanitizer
  316/316，零 ASan/UBSan。

### 2026-10-08 起（文档重组前）

**表约束类型化 IE 开放类型**
- `analyzer.cpp`：entries 只挂开放类型字段；扩展枚举未显式编号时从 root
  末尾继续（X.680 惯例）；OCF fixed_type 与内联 SEQUENCE OF 纳入依赖遍历；
  去重按 C++ 名。`emit.cpp`：tc decode/encode 参数改名避免嵌套 CHOICE 冲突。
- 验证：新增 `asn1_tc_ie_roundtrip_tests`（4 项）。

**多编解码组合生成**
- `--codec` 改位掩码：逗号分隔组合与重复标志累加；未指定默认 UPER。
- 验证：新增 5 项 CLI 冒烟测试；组合产物 `-fsyntax-only` 通过。

**PER/APER 金标准字节向量（asn1c oracle）**
- `scripts/gen_per_golden.py` + `asn1_per_golden_tests`：UPER/APER 各一套，
  54 向量（约束原语/长度 determinant/分片/扩展 addition/版本括号组）。
  修复扩展 addition 编码（X.691 19.9：简单扩展 addition 的 open type 仅含
  组件编码，版本括号组 `[[...]]` 才按嵌套 SEQUENCE 编码含 preamble）——
  IR 新增 `ExtensionGroup{fields, version_bracket}` 区分两种形态。
- 验证：`PerGoldenVectors.UperMatchesOracleBytes` /
  `AperMatchesOracleBytes` 通过。

**JER 对象类开放类型编解码 / CLI 测试并行竞态修复 / 三大 3GPP fixture 编译
门禁**（均于 2026-10 上旬完成，细节见归档
[analysis/architecture-review-and-optimization.md](analysis/architecture-review-and-optimization.md)）

## Phase 里程碑表

| Phase | 内容 | 状态 |
|-------|------|------|
| 1 | 项目骨架与构建系统 | done |
| 2 | Lexer | done |
| 3 | 基础 parser | done |
| 4 | AST 与语义分析器 | done |
| 5 | 约束系统 | done |
| 6 | BER | done |
| 7 | DER | done |
| 8 | BitReader/BitWriter 与 PER 原语 | done |
| 9 | UPER | done |
| 10 | APER | done |
| 11 | 代码生成器 | done |
| 12 | 高级 ASN.1 构造（ENUMERATED / OID / RELATIVE-OID / SET / SET OF / 类型化 REAL） | done |
| 13 | 信息对象类（class fields、开放类型 `&Value`、表约束类型化 IE dispatch） | done |
| 14 | 真实电信 ASN.1 验证 | done |
| 15 | OER | done |
| 16 | COER | done |
| 17 | XER | done |
| 18 | CXER | done |
| 19 | EXTENDED-XER | done |
| 20 | JER | done |
| 21 | JER 编码指令运行时 | done |
| 22 | JER 编码指令前端 + codegen | done |
| 23 | INSTANCE OF + XER 编码指令前端/codegen | done |
| 24 | 全量 E-XER codegen 指令矩阵 | done |
| 25 | REAL 编解码（IEEE-754 二进制 ↔ ASN.1 base-2） | done |
| 26 | OER REAL WITH COMPONENTS IEEE binary32/64 分类 | done |
| 27 | EXTERNAL / EMBEDDED PDV / CHARACTER STRING | done |
| 28 | >64 位 INTEGER 编解码（`asn1::BigInteger`） | done |
| 29 | EMBEDDED PDV / CHARACTER STRING 深化 BER | done |
| 30 | BER 构造/不定长 + DER + 现代 EXTERNAL | done |
| 31 | PER 扩展 addition 序列 | done |
| 32 | ABSTRACT-SYNTAX + 现代 EXTERNAL 识别形式 IR | done |
| 33 | JER SequenceOf/SetOf 解码 codegen | done |
| 34 | Codegen BER / DER / OER / CXER | done |
| 35 | EXER LIST character-encodable 元素类型 | done |
| 36 | OER/COER 扩展 addition | done |
| 37 | COER `--codec coer` codegen | done |
| 38 | 电信 UPER 编解码往返 | done |
| 39 | 验证 —— 多 codec Person + 电信 emit 门禁 | done |
| 40 | RRC/LPP/S1AP 全量 emit —— 嵌套命名 + 参数化 IE 字段 | done |
| 41 | 评审硬化 —— OCF 重新解析、DEFAULT、PER 约束、BitString、长度上限 | done |
| 42 | 全 codec 产物硬化 —— 12 codec 编译门禁（P0-3）；`tlv_oer_golden` fixture + 8 往返测试 | done |
| 43 | CER（X.690）编解码家族（P1-2）；`cer_golden` fixture + 10 字节级金标准 | done |
| 44 | BER/DER/OER 金标准差分验证 + OER SET 规范序（P1-1） | done |
| 45 | 大模块分文件/分命名空间 emit（`--emit-split`，P2-2） | ✅ 已完成（XML include 缺口修复 + 单 codec × 全家族门禁就位，2026-10-15 收尾） |
| 46 | 增量代码生成（P2-3） | ✅ 已完成（2026-10-16，缓存就位 + 7 项单元 + 3 项 CLI 门禁） |
| 47 | 前端并行分析（P3-1） | ✅ 已完成（2026-10-17，`--parallel <N>` + declare/resolve 并行 + 11 项门禁；`lower_pass` 串行 deferred；commit `5856565`） |
| 48 | 3GPP 协议消息级金标准向量对验（P1-3） | ⏳ 已规划，未排期；子任务清单见 [todo.md](todo.md#p1-3--3gpp-协议消息级金标准向量对验) |
| 49 | C 语言代码生成器（P2-4） | 🟡 sub-task 1（`include/asn1/c_runtime/*` 头骨架 + `-fsyntax-only` 门禁，commit `c137142`）+ sub-task 2（`src/c_runtime/*.c` 纯 C11 实现 + `asn1::c_runtime` 静态库 + `Asn1CRuntimeSmoke` 链接+运行门禁）已落地；ctest **345/345**。sub-task 3+（`src/codegen/emit_c/*` + CLI `--lang c` + 全 fixture 编译门禁 + P1-3 oracle 复用）未排期；目标字节输出与 `--lang cpp` 逐字节一致（复用 P1-3 oracle）；估时 ~43 h / 5-6 工作日；子任务清单见 [todo.md](todo.md#p2-4--c-语言代码生成器-lang-c) |
