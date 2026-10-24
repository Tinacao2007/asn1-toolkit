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
C emitter 后续调用同一规划，不能复制 presence/标签/约束/扩展/selector 判定。
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
链接到验收文档，不复制整个任务清单。REV-27 的 C 接入及能力矩阵延期项不能写成已实现能力，
尤其不能把 C11 原语与 smoke 通过表述为 Phase49 C emitter 已完成。

该文件是仓库内的 skill 源文件；置于 docs 不代表已安装到 Codex 的技能发现目录。


## CGEN batch 1 reusable checks

C generation rejects through unified directory dispatch before filesystem/cache
I/O; legacy CppGenerator stream paths must also reject C target options.
Build asn1cxx, asn1_emit_cache_tests and asn1_entry_combined,
asn1_entry_combined_inline, asn1_entry_combined_split. Then run
`ctest --test-dir <build> -C Release -R '^CGenBatch1\.' --output-on-failure`.
These tests execute API/CLI rejection, preserve prior artifacts/cache and compare
all twelve actual C++ entries across outline/inline/split. Do not turn Unsupported
into successful empty `.h/.c` output. Language/prefix options must join cache
identity; source-shard options remain a later batch. Keep the ABI design and gap
audit separate from the primitive declaration register. See
[batch 1 evidence](../../cgen-batch1-validation.md),
[C ABI design](../../c-generated-abi.md) and [runtime gaps](../../c-runtime-codegen-gaps.md).

## CGEN batch 2 type-emitter checks

`emit_c_types` is type/lifecycle-only; keep CLI codec Unsupported tests until wire
code exists. Preserve field-specific IR clones. Unsupported reachable layouts or
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
