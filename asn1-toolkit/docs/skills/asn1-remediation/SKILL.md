---
name: asn1-remediation
description: 执行 asn1-toolkit 的 REV/CGEN 修正任务，并同步代码、验证证据和项目文档。用于本仓库 TODO 分批修复与回归维护。
---

# ASN.1 工程修正

本文件维护可复用的操作约束；任务完成状态以
[remediation-todo.md](../../remediation-todo.md) 为准，当前能力以
[status.md](../../status.md) 为准。这里不固定测试数量，也不把历史缺陷当作当前事实。
路径均相对于本文件；执行脚本时从仓库根目录运行。

## 接手任务

读取仓库适用的 AGENTS.md（如存在）、指定 REV 或 CGEN 批次、相关接口/实现/测试和当前 diff。
按用户指定的编号或批次处理必要依赖，保留已有未提交修改。先核实调用链或复现，
再做修复；若代码已变更，记录证据，避免重复修改。

只读取与本次任务相关的契约与验收记录：

- C11 内存、状态、所有权：[C runtime 契约](../../c-runtime-contracts.md)、
  [声明覆盖登记](../../c-runtime-test-coverage.md)、
  [REV-01..03](../../rev01-03-validation.md)。
- 位操作、PER Variant、分片与独立向量：[REV-04..09](../../rev04-09-validation.md)。
- C++ Result、诊断与 E-XER 属性：[C++ 错误契约](../../cpp-error-contracts.md)、
  [REV-10..12](../../rev10-12-validation.md)。
- 缓存、并发、codec 组合与构建：[REV-13..19](../../rev13-19-validation.md)、
  [产品入口登记](../../codec-entries.md)、[安装与交叉构建](../../build-and-package.md)。
- 标准语义与独立对验：[能力矩阵](../../standard-capabilities.md)、
  [REAL/字符/时间 profile](../../real-and-string-profile.md)、
  [REV-20..24](../../rev20-24-validation.md)。
- 产物/分层/后端规划：[产物契约](../../generated-artifacts.md)、
  [规划边界](../../emission-planning.md)、[REV-25..28](../../rev25-28-validation.md)。
- 纯 C 后端：[CGEN 清单](../../c-codegen-todo.md)；若由夜间 runner 调用，另读
  [夜间契约](../../c-codegen-nightly.md)。

## 必须保留的设计

- compiler 流水线禁止链接 runtime；runtime 禁止包含 AST/IR/前端头。
  生成产物是桥梁，单头模式的桥梁在生成头中。
- C++ 业务失败使用 Result/Diagnostics，只有 std::bad_alloc 向外传播。
  检查数字转换、文件系统、线程启动及写文件的失败路径；不要机械添加 noexcept。
  C11 使用状态码与输出参数，按逐 API 契约处理失败状态、初始化和释放。
- UPER/APER 共用 Variant 参数化核心；保留 DER/BER、COER/OER、CXER/XER 策略关系。
  多入口同时生成时，各入口必须使用自己的策略，不让全局掩码覆盖同族行为。
- 保留自研 BigInt、零第三方运行时依赖、C++17、纯 C11 和三种生成产物形态。
- 12 是产品入口口径；per 是组合别名。JERI 共享 X.697 JER 线路语法，
  不称为标准 canonical JSON；未实现的 encoding instruction 必须明确诊断。

## 按改动选择回归

### 内存、线路和错误传播

分配失败必须逐层传播，检查容量加乘溢出和资源上限；遵守各 API 的输出提交、
游标及生命周期契约，不假定所有失败都会回滚游标或所有 out 都自动清空。
测试覆盖失败后释放/复用及截断输入。测试 hook 不进入生产库。

PER 改动同时验证两种 Variant，覆盖非零起始位、整数符号/补码、长度阈值、
片段与末尾零 determinant。使用适用的 asn1c/asn1tools 独立向量与交叉解码；
遇到 oracle 能力差异，记录版本、限制及标准仲裁依据。自身往返不能证明线路合规。

生成代码的解析失败应传播到调用方；必填缺失与非法值分别处理，数值完整消费并
核查目标范围。Result 的访问前置条件和源码/线路位置单位遵循错误契约。

### 缓存、并发和生成入口

- 缓存键保留生成器内容指纹、格式版本、完整输出选项、依赖内容和输入顺序。
  无 source_paths 的手工 IR 与不可读输入禁用缓存。对比缓存开/关及 miss/hit
  产物字节；修改 emitter 后覆盖相关 inline/out-of-line/split。
- 指纹集成回归使用独立、未同时构建的 compiler-only 目录：
  `python3 scripts/validate_generator_revision.py --build build-compiler`。
  此脚本会临时修改并恢复构建目录的私有生成头，不与该目录其他构建并发。
- 并发保持有界、按模块序合并诊断；0/1 串行，lower/tag 保持串行。
  验证峰值与确定性，不能仅比较运行时间。
- codec 改动核查 CLI、位掩码、include、runtime 依赖与函数名；比较单入口和组合
  的实际编码结果。用 CodecEntry 测试覆盖真实链接/调用，保留三种产物验证。

### 目录产物、分层与后端规划

目录发射使用 stage/manifest；一个目录只拥有一套当前产物，同目录并发拒绝。
切换形态/basename 只清理 hash 未变的旧归属；保留用户文件和已修改的过时文件。
不要 glob 所有历史 `.cpp` 当成本次输出，按 manifest 注册产物。
强杀遗留 `.asn1cxx-emitting` 时先确认无活动写入者并核查 backup，再恢复/重跑，
不能自动删除仍被其他生成进程使用的目录。

生成相关回归包含三形态双 TU 链接/调用、模块 SCC、名称/Windows 路径及失败归属保护；
单 TU 或语法检查不能证明 ODR。保持 `Architecture.Layering` 和 `Architecture.PublicHeaders`，
新增模块/目标或 include 根时同步两种分层门禁，不将 emitter 打印的 include 当实际依赖。
独立验证 compiler-only/runtime-only；组件开关不能只在全量构建里测试。

规划只依赖 IR，借用计划不能比 descriptor 活得久，IR 在消费期间不可修改。
C 类型后端已消费同一规划；后续 codec 继续复用，不复制 presence/标签/约束/扩展/selector 判定。
计划模型和 C++ 接入通过不等于 C emitter 通过，真实生成 C PDU 验收留给对应子任务。

### 标准语义与独立验证

先按能力矩阵区分 parse、语义/IR、生成、runtime 和外部互操作，C/C++ 分开。
基线版本、勘误是否核对、未实现指令均明确登记；不要由类型枚举或一个往返推定全标准支持。

约束不能保真进入 IR 时应诊断并阻止生成，不能只 warning 后当 unconstrained。
静态常量解析沿本模块 import 和值别名查找，检测循环；禁止从无关模块挑选同名值。
参数化值约束必须替换实参并保留调用模块来源，实例键区分值；不能只用 V0/V1
位置标签缓存不同范围。用至少两个实例的实际 PER 位宽和跨模块嵌套用例回归。
生成的 native INTEGER 在 cast 前检查宿主范围与非扩展根区间成员，跨二进制/XML/JSON
家族复用同一生成检查；不要将超界值截断为有效字段。BigInteger 的完整约束验证另行核查。
检查负 SIZE、空集合及引用类型新增约束；FROM、COMPONENTS OF、数字版本组等当前
Unsupported 的结构，只有在补语义和线路证据后才开放探针，不能直接改预期让门禁通过。

规范策略采用成对测试：基础规则的合法替代在 BER/OER 可接受，规范规则应拒绝对应非法表示。
结构级 DEFAULT/重复成员/顺序检查不能由原语策略替代；CER 片段检查计入 unused-bits
内容字节，验证非末尾片长、嵌套 EOC、最短头、递归和累计内容上限。

REAL 改动遵守已公开 profile。binary64 容量不等于 ASN.1 REAL 完整值域；
验证 mantissa 有效精度、范围、次正规和负零，禁止静默窄化或读取 JER bigint 的缓存整数。
XML/JSON 文本仍是 host-double 语义，精确十进制模型另立设计，不误称当前已实现。

新增外部向量保存工具版本/commit、schema hash、值、参数和字节。
工具副本没有独立 Git commit 时记为未知并记录包源码 hash，不能引用父工作区 commit 冒充。
使用 `scripts/validate_standard_oracles.py` 复跑 live 或冻结向量；只有显式刷新才写期望值。
XML 树/JSON 值比较与规范字节比较分开，CXER 空元素拼写等标准化转换必须记录，
不能将 XER/OER oracle 冒充独立 CXER/COER 规范编码器。

使用 `DecoderFuzz.SeedRegression` 重放截断/变异；新发现的 crash 应最小化并固定为回归。
可选 Clang `ASN1_BUILD_FUZZERS=ON` 配合 ASan+UBSan 做有限探索，记录 seed、次数、
输入上限及实际覆盖入口；无崩溃不等于安全性证明。

### 构建与验收口径

编译选项限定到 target 和适用编译器；C 目标声明 C11/REQUIRED/EXTENSIONS OFF。
维持最低 CMake 兼容、固定 GTest 下载 hash 和离线路径。
MSVC ASan 不冒充 ASan+UBSan。交叉构建区分宿主生成器和目标库，避免在宿主执行目标程序。

影响 runtime 或生成语义时，按 [开发纪律](../../development.md) 完成针对性回归、
Release 与 ASan+UBSan 全量门禁，以及 7 fixture ×12 实际入口语法矩阵：
`cmake --build build --target asn1_codec_matrix`。
语法通过不等于链接、运行或互操作通过，分别记录证据。

安装改动运行 `python3 scripts/validate_installed_package.py --build build --output build/package-check`，
使用新的输出目录，覆盖纯 C、runtime、compiler 和宿主生成消费者；
私有 GTest 不进入 SDK，纯 C 消费者不引入 C++ 标准库。
具体离线/交叉命令与支持范围查阅安装文档，不从历史通过记录推断本次已验证。

大型 fixture 构建根据资源控制并行度；中断/未跑的门禁不记为通过。
只改文档时检查表述与链接，无需重复全量编译。

## 夜间 runner 调用

由 `nightly_c_codegen.py` 调用时只执行指定 CGEN 批次，父 runner 负责 commit；
模型不得自行提交、push、切换分支或重置工作区。补真实 `CGenBatchN.*` 阶段验收，
同步契约与证据；不改 TODO 验收正文、受保护 runner/门禁或提前勾选后续任务。
runner 的自动门禁不替代每批独立 oracle、生成 C PDU 或安装验收。
其他交互任务是否提交/推送仍以用户明确授权为准，本节不提供额外 Git 授权。

## 同步收尾

每个 REV 记录实际命令、结果、环境及未验证范围；缺少必需验证时保留“验证待补”。
更新对应契约/使用/架构文档、TODO、status 与 changelog。
历史验收和分析快照保留原数字；只同步当前状态数字。

每批完成时检查本 skill：把已验证且可复用的流程变化同步到对应段落，详细证据
链接到验收文档，不复制整个任务清单。按当前验收区分原语、类型产物和 codec；
不能把 C11 smoke 或类型-only 通过写成完整 C PDU 编解码已完成。

该文件是仓库内的 skill 源文件；置于 docs 不代表已安装到 Codex 的技能发现目录。


## CGEN batch 1 reusable checks

Unsupported C graphs reject through unified dispatch before filesystem/cache I/O.
Supported codec requests are opened by their batch-4/5/6 acceptance records; explicit
type-only output remains available. Legacy CppGenerator streams reject C target options.
Build asn1cxx, asn1_emit_cache_tests and asn1_entry_combined,
asn1_entry_combined_inline, asn1_entry_combined_split. Then run
`ctest --test-dir <build> -C Release -R '^CGenBatch1\.' --output-on-failure`.
These tests execute API/CLI rejection, preserve prior artifacts/cache and compare
all twelve actual C++ entries across outline/inline/split. Do not turn Unsupported
into successful empty `.h/.c` output. Language/prefix options must join cache
identity; explicit type-only/source-shard behavior is covered by batch 3 below. Keep the ABI design and gap
audit separate from the primitive declaration register. See
[batch 1 evidence](../../cgen-batch1-validation.md),
[C ABI design](../../c-generated-abi.md) and [runtime gaps](../../c-runtime-codegen-gaps.md).

## CGEN batch 2 type-emitter checks

`emit_c_types` is type/lifecycle-only; unsupported wire profiles retain explicit
CLI rejection until actual codec acceptance exists. Preserve field-specific IR clones. Unsupported reachable layouts or
DEFAULTs must diagnose before writing streams. See [ABI profiles](../../c-generated-abi.md)
and [batch 2 evidence](../../cgen-batch2-validation.md).

Build `asn1_cgen_batch2_lifecycle`, `asn1_emit_cache_tests` and
`asn1_cgen2_cpp_{outline,inline,split}_tests`; run `CGenBatch2` and
`EmissionPlanning` tests in Release and ASan+UBSan. Strict independent C linkage,
C++17 header inclusion, compiled naming and duplicate-definition rejection stay
in the gate. Test allocation faults in generated C, with hooks only in the test
wrapper. Preserve pre-existing tests and the 7-by-12 C++ selector matrix.

Allocation-free init/free restore scalar defaults, not owning literal contents.
`reset_defaults` materializes owned literals through a temporary and must retain
live output on every allocation failure. Required nested defaults recurse;
OPTIONAL/inactive extension storage stays freeable without allocation. Keep
unsupported structured/list and unresolved non-enum defaults explicit.

Use shared field plans for tags, presence, version groups and selectors, and
shared literal normalization for C/C++ DEFAULTs. Preserve >64-bit selector keys
as decimal strings; generated type IDs are schema-local. Exercise descriptors,
lookup and group coherence in actual compiled C. REV-27 planning reuse can be
closed on this evidence; it does not open wire codecs or their PDU/oracle gates. SIZE units differ for bits/octets/elements;
character-string byte length is not ASN.1 character count. REAL host-double,
OID uint64 arcs and enum int64 are declared profiles, not full standard domains.


## CGEN batch 3 artifact checks

Type-only C CLI requests use explicit `--types-only` without a codec. Normal codec
requests use the currently declared profile. Unsupported graphs must reject; never
manufacture successful codec stubs.
`CppGenerator` remains C++ only. Follow the [C output contract](../../c-codegen-output.md)
for SCC budgets, source ownership, internal helper linkage and source lists.

Run `CGenBatch3` in Release and ASan+UBSan. Shape tests compile/link two actual C
consumers, inject allocation failures into each generated shard, include headers
under strict C++17 and compare values/primitive wire projections. The latter is
not full generated-PDU evidence. Keep the single-module threshold-change test,
whole LTE RRC fixture type compile/link, cache corruption/hit/miss checks and
C-only CMake source-list consumer. Record shape/RRC metrics from actual runs.

Inline C uses `static inline` for public and private functions. Cross-source depth
helpers instead have prefixed external declarations in the generated internal
header; no static helper may be assumed visible across TUs. Extend the measured
function count when adding emitters and retain actual definition/link checks.
C++17 header consumers need valid typed zero initialization for enum-first structs.

C and C++ publication share one transaction. Test language/shape/budget changes,
modified stale files, unowned sidecars, symlinks, lock and model/manifest failures.
Only current sources enter JSON/CMake; retained user files must not enter a glob.
A CMake source list is not a depfile or automatic ASN.1 build dependency.

Verify pure-C runtime dependencies in non-instrumented binaries. GCC UBSan itself
pulls libstdc++, so its dependency is not evidence of a product runtime leak.
Keep sanitizer execution fully instrumented. A missing compiler is unexecuted
coverage, not a passing result; preserve later toolchain checkboxes. Evidence:
[batch 3 validation](../../cgen-batch3-validation.md).

## CGEN batch 4 PER codec work

Batch 4 is closed for the declared profile in its validation record. Preserve its
limits; do not expand this into a complete PER conformance claim. The checked C UPER/APER
subset now includes BOOLEAN, NULL, constrained INTEGER/ENUMERATED, BIT/OCTET STRING,
OID/RELATIVE-OID (uint64 arcs), root CHOICE, OF types, recursive SEQUENCE OF,
extensible SEQUENCE/SET additions and version-bracket groups, opaque open-type
payloads, Numeric/IA5 strings and common REAL forms. Unsupported graphs must fail
before artifact publication; never describe this subset as complete PER. A version
group is one extension bitmap slot containing a nested SEQUENCE preamble. Open
payloads are complete-encoding octets. A known table selector now automatically
decodes to an owned typed view while preserving the raw octets; generated copy/free
and typed re-encode follow the selected type and validate its governor key.
The C PER runtime uses one `asn1_per_variant_t` core for UPER/APER. Normally-small,
ENUMERATED ordinal, and uint64-arc OID primitives are shared across both variants.
The C runtime's SEQUENCE/SET preamble and extension-addition bitmap are consumed by
generated SEQUENCE and SET code. Release and ASan+UBSan tests exercise out-of-line,
inline and split C11 consumers, unknown SEQUENCE extension skipping (unknown
CHOICE extension ordinals return `UNSUPPORTED`), recursive depth,
version-group preambles, opaque open payloads, malformed restricted characters,
REAL vectors, truncation and presence validation. Collection length fragmentation,
cumulative PDU byte/element budgets and allocation-failure injection across both
collection growth steps are implemented and gated. The broader nested-extension
allocation/cursor failure matrix, full string fragmentation profile and independent
C-PDU oracle coverage remain required before closing the batch.
Check large forms at non-octet offsets: X.691 10.6–10.9 use a length determinant and
octet-aligned semi-constrained value for normally-small values >=64. Compare the
C primitive against both a frozen standard vector and the C++ variant; keep a
regression test for aligned PER octet alignment. OID uses BER contents, strict
base-128 minimality and uint64 overflow checks; test absolute and relative arcs,
malformed tails and unchanged outputs on decode failure.

Only expose the explicitly validated C subset through the CLI/API; diagnose unsupported
REAL profiles, character alphabets or constraints before publishing
any artifact. The current generated PER subset includes recursive SEQUENCE OF,
SEQUENCE/SET extension additions and version groups, opaque open types, six restricted
string kinds, and a limited host-double REAL representation. Do not imply this means
full REAL or all character-string kinds are supported. For generated PDU decode, test
truncation, invalid enum/index, bad extension/open-type lengths, depth/aggregate
allocation caps, allocator faults, and document whether output and reader cursor can
change on failure. Keep only test-local allocator hooks. Cross-check encoded bytes
with asn1c/asn1tools where their semantics match X.691; distinguish unavailable oracle
coverage from a pass.
Progress evidence: [batch 4 validation](../../cgen-batch4-validation.md).

## CGEN batch 5 TLV work

Read [TLV/RRC acceptance and remaining telecom blockers](../../cgen-batch5-validation.md).
Use one per-call BER/DER/CER policy engine. A combined codec mask must not select
one global canonical policy. Generated metadata may describe runtime storage;
runtime must never include IR/AST or link compiler libraries.

Retain selected alternative tags inside explicitly wrapped untagged CHOICEs,
including automatically tagged CHOICE fields. For CER, sort an untagged CHOICE
SET component by its static minimum possible tag; DER uses the selected tag.
Restricted character-string fragments have OCTET STRING child tags. UTF8 SIZE
counts characters, not storage octets. Test these against explicit vectors;
a self-roundtrip can hide matching encoder/decoder mistakes.

When changing TLV allocation paths, run every-allocation failure tests on owning
DEFAULT, recursive collections and CER fragments. Verify destination/cursor and
writer commit boundaries. Re-run the 21-consumer entry/shape matrix and whole
RRC generation/build/PDU/live-asn1c oracle. RRC slice/types-only compilation alone
does not satisfy M1; whole-module PDU success does not prove every PDU. Preserve
LPP time and S1AP PER/typed-open blockers until their actual gates pass. CER
standard-derived vectors must stay distinguished from independent oracle results.

## CGEN batch 6 OER and text families

Use [batch 6 evidence and profile](../../cgen-batch6-validation.md) when changing
C OER/COER, JER/JERI or XER/CXER/E-XER. The C schema owns storage decisions only;
never pass IR to runtime. Preserve per-entry policy under combined masks and
the 39-consumer single/combined three-shape runtime matrix.

INTEGER JSON tokens remain exact decimal; ARRAY orders all roots before extensions.
OBJECT permits string/enumeration keys and must reject duplicate abstract keys.
UNWRAPPED must decode uniquely; optional nullable ARRAY graphs fail before publication.
Shared NAME/TEXT effects apply to JER and E-XER; BASIC-XER/CXER ignore those mappings. XML control elements
need mixed-content offsets; UTF-8 byte positions alone do not recover their order.

X.693 9.5/9.6 requires DEFAULT components present in CXER (unlike DER/COER).
Canonical REAL requires normalized exponent spelling; strict parsing must reject
noncanonical equivalents. OER SET minimum tags exclude CHOICE extension alternatives;
nested untagged CHOICE repeats the selected tag per X.696 20.1.

Run syntax/parser, instruction, frozen-boundary, every-allocation and constrained-
budget tests, then full Release/ASan+UBSan and the original C++ 84 syntax gates.
Use the independent asn1tools exchange separately from self-comparisons. Version
0.169.0 flattens extension addition groups and cannot validate those group wires;
retain the explicit oracle gap and standard vectors. Do not claim its OER/XER
engines are independent COER/CXER canonical oracles. Keep unimplemented UNTAGGED graphs as pre-publication Unsupported. Known table-driven
text typed views are implemented; unknown runtime selectors require explicit
Unsupported, never a hexadecimal or silent BASIC fallback.

Keep the 72 external frozen inputs and their normalized output replay independent
from self-roundtrip tests. Constrained REAL must consume the IR IEEE form, reject
lossy binary32 casts before writing, and check DER contents for unconstrained OER
even under BASIC policy. TEXT overrides on enum aliases propagate into OBJECT
keys; validate transformed field/enumeration name uniqueness before publication.

XML OF values need the actual item type name for structure/string wrappers;
BOOLEAN/ENUMERATED collections use their direct empty-element value forms.
Keep external vectors for these cases, not just INTEGER OF self-roundtrips.
Shared NAME/TEXT mappings apply in JER/E-XER; BASIC-XER/CXER retain basic names.
CGEN6 CMake consumers share a stamp-producing generation target and explicit
dependencies. Cache hits leave source timestamps unchanged, so direct duplicate
custom commands can re-enter a shared output directory under parallel builds.
Keep the publication lock; test a cold-stamp parallel build rather than disabling it.

### CGEN7 final acceptance

Use `scripts/validate_c_codec_matrix.py` for the same seven fixtures and twelve
selectors as the C++ matrix. Rejection is failure, never a successful or skipped
combination. Keep generation and C11 syntax evidence separate from linked, wire
and oracle evidence. Run the strict matrix under GCC/Clang; do not register an
old binary's partial matrix as final after changing the emitter.

Installed C consumption includes `generated-c` (host generator, multiple C TUs,
module shards, UPER+BER). `asn1_generate_c` plans during configuration, reads the
source manifest and watches schemas/generator/outputs for reconfiguration. Test
schema growth and missing-output repair. All imported schemas must be supplied;
do not hide dependencies behind a build-time glob. Cross guard probes with a
native compiler verify execution policy only, not an embedded target ABI.

Use `asn1_test_executables` to rebuild every test/consumer executable before full
CTest; the large C/C++ syntax matrices are separate mandatory gates. Generated C
decoder fuzz must call emitted wrappers, cover all twelve entries and free live
values on every outcome. Keep bounded seed regression and optional Clang libFuzzer
runs distinct. PER finite-root/range BigInt primitives share the Variant core. X.691 13.1 uses
the RANGE of the extensible root (including holes); non-extensible constraints
still require exact membership. Keep the independent asn1c sparse-root vectors;
asn1tools 0.169.0 ignores parts of some UNION roots and is not an oracle for them. Owning INTEGER DEFAULT
materialization must charge limb allocations and avoid shadowing generated locals.


CGEN7 generated/runtime ABI is 3. Rebuild/regenerate all consumers together:
private runtime storage/codec descriptors changed and empty private object-set
layouts no longer carry unusable scalar-selector views. Generated headers check
ASN1_C_RUNTIME_ABI_VERSION, without promising cross-CRT binary compatibility.

Named bits require per-family rules, not an erased preflight guard: DER/CER/CXER
remove all trailing zeros, PER/COER use the shortest effective SIZE, fixed encodings
pad as required, and decoded abstract values satisfy SIZE. Charge normalization
allocations (including geometric bitstring capacity) before allocation. Preserve
ordinary BIT STRING lengths. UTCTime validates Gregorian dates and offsets;
PER uses canonical unconstrained VisibleString (X.691 10.6.5), not octets or visible
SIZE. OER must also ignore UTCTime SIZE (X.696 8.4.4), retaining its length
determinant; test constrained time under both OER and COER. Keep GeneralizedTime
and other unsupported profiles distinct.

Typed JSON/XML open fields use generated immutable selector getters/offsets.
Defer selection until ordinary fields/governors are decoded, preserve JSON member
order independence, enforce selected type ids, and free partial selected values.
Use every-allocation tests, unknown-key/type/payload negatives, 21 independent
frozen wire equalities and asn1c sparse-root vectors; full fixture syntax success
is not a claim of whole-3GPP wire certification. Keep raw BER/OER content adapters
and known typed PER/text adapters explicit.
