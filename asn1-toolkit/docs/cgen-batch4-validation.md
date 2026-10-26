# CGEN 第 4 批进度记录

日期：2026-10-26。**第 4 批首个 C11 UPER/APER profile 已完成；CGEN-10..12 已按
本批声明范围关闭。此结论不代表实现了 X.691 全部值域或所有 ASN.1 PER 结构。**

## 已完成的增量

- C PER shared core 新增 normally-small non-negative whole number 与
  ENUMERATED root/extension ordinal 编解码接口，UPER/APER 由同一
  `asn1_per_variant_t` 参数化实现。
- 规范复核发现 C++ PER reference core 的 aligned length determinant 没有
  执行 X.691 10.9 要求的 octet alignment。已补齐，并新增 C/C++ 两变体逐字节
  parity 测试。normally-small 大值采用半约束非负整数形式；标准条文见
  [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en)，
  10.6–10.9。
- C runtime correctness test 覆盖 normally-small `0, 1, 63, 64, 127, 255,
  256, 2^32, UINT64_MAX`，root ENUMERATED、扩展 ordinal 和非法参数。
- OID/RELATIVE-OID primitives encode BER contents as base-128 subidentifiers,
  with X.691 23's semi-constrained octet count, APER alignment, arc/root
  validation, uint64 overflow rejection, non-minimal/truncated input rejection,
  and transactional output parameters. Absolute vector `1.2.840.113549` is
  frozen as `06 2A 86 48 86 F7 0D` before PER wrapping.

## 本次验证

环境：Windows 11 / Visual Studio 2022（MSVC Release）及 Ubuntu 24.04 / GCC（WSL 2）。

```powershell
cmake --build build-rev4-msvc --config Release --target asn1_per_tests asn1_c_per_correctness -- /m:1
ctest --test-dir build-rev4-msvc -C Release -R '^(PerPrimitives.CNormallySmallMatchesCppForBothVariants|PerPrimitives.CEnumeratedOrdinalsMatchCppForBothVariants|PerPrimitives.COidMatchesCppForBothVariants|Asn1CPerCorrectness|CGenBatch4.PerPrimitiveCore)$' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^(PerPrimitives\.)' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^CGenBatch4\.' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^(Asn1CPerLargeValues|Asn1CRuntimeFailureContracts|Asn1CRuntimeSmoke)$' --output-on-failure
```

结果：三项 C/C++ UPER+APER parity、C PER correctness 和 batch stage case **5/5**；
PER primitive suite **12/12**；`CGenBatch4.PerPrimitiveCore` **1/1**。最终包含 44 项
相关 PER/APER/UPER/primitive 验收的回归轮次 **44/44** 通过。C runtime smoke、
allocator failure contracts 与 128 KiB large-value target 也通过 **3/3**。

随后新增的 C SEQUENCE/SET presence preamble 原语单独在 MSVC 下通过 C/C++ parity、
C correctness 和 allocator-failure 测试 **3/3**；GCC Release 对应测试 **3/3**，
ASan+UBSan 对应测试 **3/3**。新增 API 验证扩展位、OPTIONAL/DEFAULT bitmap、非法位
预检查、截断输出保持和分配失败清理。

此前普通桌面进程启动 WSL 返回 `E_ACCESSDENIED`；提升 Windows 进程权限后，Ubuntu
24.04 可正常启动。复用 Linux 构建目录并重编译 C/C++ PER targets 后，GCC Release 的
UPER/APER 回归 **52/52**、PER primitive/oracle/C runtime 回归 **20/20** 通过。
ASan+UBSan 下相关 PER/APER、golden、codegen、C runtime 回归 **67/67** 通过。此处 sanitizer
是相关目标回归，不代表全仓库 sanitizer suite 或 Clang/MSVC 覆盖。

## 早期阶段缺口快照（已由后续章节更新）

以下记录反映 2026-10-24 当天较早阶段的状态快照；后续章节已推进其中若干项。
当前实现状态以本文件末尾按时间排序的后续实现章节为准。

CGEN-10 仍缺生成侧 REAL、restricted character-string profile 和 named-bit 语义，以及更完整的值域/约束拒绝矩阵。已支持 BIT STRING（不含 named bits）、OCTET STRING 和 uint64-arc OID/RELATIVE-OID。CGEN-11 已支持根 CHOICE、SEQUENCE OF/SET OF、非递归且无扩展的 SEQUENCE/SET；递归结构、trailing-root、扩展单字段/版本组和开放类型仍未实现。CGEN-12 覆盖截断、非法 BOOLEAN/OPTIONAL 标志、INTEGER 越界和 preamble OOM；集合解码分配故障注入、累计资源预算和完整 reader-cursor 失败契约仍缺。其他 codec/layout 诊断拒绝。未运行 asn1c/asn1tools C PDU 双向对验；自有实现互验不冒充 oracle。

因此 TODO 在该早期快照仍保持 CGEN-10..12 `[ ]`；后续章节记录了 profile 完成和
剩余标准覆盖边界。

## 2026-10-24 后续实现：递归、扩展、open type、REAL 与字符串

- C PER 递归遍历复用生成的 `*_at(..., depth)` helper，允许经 `SEQUENCE OF`
  形成的递归值；生成 decode 为间接成员分配对象，生命周期沿同一指针图释放。
- C runtime 增加 X.691 extension-addition bitmap 的 encode/decode/free；生成器消费
  普通单字段扩展和 `[[ ... ]]` 版本组。版本组占一个外层扩展位，其 open type
  内容按 X.691 19.9 带嵌套 SEQUENCE presence preamble。未知尾部扩展项被解码并丢弃。
- `CLASS.&Value` 的 C 布局保留 complete-encoding octets；生成字段以 PER
  open-type length/alignment 封装和解封。table selector 仍是调用方通过
  `*_select` 查询的接口；生成器尚未依据 selector 自动选择并递归解码具体值类型。
- REAL 新增 PER 编码：BER REAL content 形式的 binary64、±Infinity、NaN、负零；解码
  支持这些二进制值及 NR1/NR2/NR3 十进制子集。十进制值用 host `double` 表示，最多
  20 位输入有效数字；这不是精确十进制 REAL 模型，也不覆盖整个标准值域。
- restricted string 新增 NumericString、PrintableString、VisibleString、IA5String、
  BMPString、UniversalString 的 UTF-8 内存到 PER 字符码路径；另有 NumericString 和
  IA5String 生成 PDU 双变体往返。大于 16383 字符的通用分片字符串、FULL repertoire
  及全部 SIZE/字符约束组合仍未封闭。
- `asn1_per_encode_open_type` 现在接受零 octet 内容，以支持空编码值（例如 PER NULL）；
  新增 generated open-type fixture 覆盖 telecom `HandoverIE` opaque payload 与 selector
  lookup。payload 的内部类型正确性由调用方负责。

验证命令：

```powershell
cmake --build build-rev4-msvc --config Release --target asn1_cgen_batch4_per_tests asn1_cgen_open_type_tests asn1_cgen_batch4_per_inline_tests asn1_cgen_batch4_per_split_tests asn1_cgen_batch4_per_failure_tests asn1_c_per_correctness -- /m:1
ctest --test-dir build-rev4-msvc -C Release -R '^(CGenBatch4\..*|Asn1CPerCorrectness)$' --output-on-failure
```

MSVC Release 相关门禁 **7/7** 通过。Ubuntu 24.04 GCC ASan+UBSan 相关门禁 **7/7**
通过；GCC 因 `powl` 需要 `libm`，已在 C runtime 和直接编译 runtime 的测试目标中
添加条件链接。此次没有重新跑全量 sanitizer suite，也没有 Clang。`asn1c` /
`asn1tools` 未用于本次 C PDU 双向差分；这些向量和 C/C++ 以外的独立 oracle
在该阶段仍是关闭 CGEN-10..12 的阻断项；最新完成范围见本文件末尾。

当时仍保持未完成：CHOICE 扩展、trailing-root、selector 驱动的 typed open decode、完整
restricted-string 分片/字符域、完整 REAL 标准值域、extension/recursive decode 的全分配
故障矩阵和读游标契约。CHOICE 扩展、trailing-root 与 typed open decode 已在本文末尾
后续章节完成；其余全标准值域和故障矩阵限制已列为明确的 profile 边界。

## 2026-10-24 BMPString 边界修正复验

实现审查发现 BMPString 不允许以 UTF-16 surrogate pair 表示补充平面字符；现已拒绝
补充平面输入、拒绝 wire surrogate，并确认失败不覆写 decode 输出。UPER/APER
各自覆盖有效 BMP 字符和无效边界。MSVC Release 复验：`PerPrimitives.PrintableStringIncludesPeriod`、
`Uper.RealRoundTripAndVector`、`Uper.SequenceExtensionAdditions`、C runtime PER correctness、
inline/out-of-line/split PDU、opaque open type、生成解码分配失败及大型值门禁共 **10/10**。
`git diff --check` 通过。历史 GCC ASan+UBSan 7/7 是此 BMP 修正之前的结果；本环境
无法复用该目录，因为它的 CMake cache 指向 `/mnt/d/...`，当前 Windows 路径下不匹配。
本次没有新的 sanitizer 结果，也没有 `asn1c` / `asn1tools` 独立对验。

## 2026-10-24 集合分片、累计预算与故障注入

- X.691 OF 长度现在按分片 determinant 与各段元素交错编码/解码。精确 16K
  边界会读写末尾零长度 determinant；生成 `LargeFlags ::= SEQUENCE (SIZE
  (0..70000)) OF BOOLEAN` 覆盖 16384 与 16385 个元素，UPER/APER 往返，并在
  out-of-line、inline、split 三种消费者中对 16385 元素逐字节比对冻结向量。
- 已在 Ubuntu 24.04 / asn1c v0.9.29 现场用 `Frag` 与 16385 个交替 BOOLEAN
  生成 UPER；2051 字节与冻结向量完全一致。asn1c v0.9.29 提供 UPER，本次没有
  把它说成 APER oracle。
- 每个顶层生成 decode 重置 reader 预算；递归字段和嵌套 open-type reader 共用
  同一预算。默认累计上限为 16 MiB 申请字节和 65536 个元素，可用
  `ASN1_MAX_PDU_ALLOCATION_BYTES` / `ASN1_MAX_PDU_ELEMENTS` 覆盖。两份各自低于
  上限的 SEQUENCE OF 合计超 byte/element 上限时均返回 `RESOURCE_LIMIT`，且不改
  调用方输出。
- 生成代码的 allocator seam 只在 `ASN1_C_RUNTIME_TEST_ALLOCATOR` 下接入测试 hook。
  16385 个元素触发两次集合扩容，分别注入第 1、2 个分配失败；两次均返回
  `OUT_OF_MEMORY`、保留原输出并释放临时集合。成功路径确认恰好经过两个扩容点。

MSVC Release **14/14** 相关测试通过；Ubuntu 24.04 GCC ASan+UBSan **11/11** 通过；
asn1c UPER 分片向量匹配通过。`git diff --check` 通过。该阶段剩余限制包括 CHOICE 扩展、
trailing-root、自动 typed open dispatch、全部 REAL/字符值域和完整 reader-cursor
故障契约；前三项已由本文末尾后续章节补齐。该阶段证据不能单独作为批次完成结论。

## 2026-10-24 后续实现：扩展 CHOICE、trailing-root 与 typed open type

- 扩展 CHOICE 使用 X.691 extension bit、normally-small extension ordinal 和 open-type
  完整编码；已知扩展可由 UPER/APER 生成 C 自动编解码。未知 ordinal 返回
  `UNSUPPORTED`，不丢弃或伪造一个不可表示的 choice 值。
- SEQUENCE/SET 的 trailing-root 字段进入合并的 root OPTIONAL/DEFAULT presence bitmap，
  并按 PER root-component 顺序编解码。X.691 19.9 注 2 明确要求第二个扩展标记后的
  root component 按紧邻扩展标记对之前的方式编码。
- 对表约束 `&Value` 字段，包含对象集已知 key 的生成 PDU decode 自动保留 raw
  complete-encoding octets 并解出匹配的 typed value；结构提供 `decoded_type_f...` /
  `decoded_value_f...`，生成 copy/free 维护拥有关系。编码优先使用 typed view，按 type
  ID 编码并校验 governor ID；没有 typed view 时仍可转发 raw octets。单字段 decode
  helper 也可由调用方使用。子 reader 共享顶层累计预算。
- 上述类型布局更新生成 ABI 标记至版本 2。CMake 测试现对 telecom table-constraint
  fixture 同时生成 UPER/APER，覆盖自动 typed decode、copy/free、typed 修改后的重编码、
  governor 不匹配事务性、扩展 CHOICE 未知 ordinal 错误，以及 trailing-root 存在/缺失
  extension 的组合。

验证：MSVC Release **8/8**，Ubuntu 24.04 GCC ASan+UBSan **8/8**。独立 asn1c v0.9.29
UPER encoder 生成了 CHOICE extension INTEGER/IA5String、trailing-root 有/无 extension
四个向量；C 生成 encoder 必须逐字节匹配这些向量，生成 decoder 再解码同一 wire。
该 asn1c 版本不提供 APER oracle；APER 由 C/C++ 共享核心 parity 与生成 PDU 测试覆盖。
`asn1tools` 当前未安装。CGenBatch3 RRC types-only C11 生成/编译/链接验收 **1/1**。
覆盖到的 PDU 形状已有 UPER 双向对验；独立 oracle 目前覆盖受支持 profile 的分片、
CHOICE extension 和 trailing-root 代表向量，其他结构主要由 C/C++ parity 与生成运行
测试覆盖。asn1tools 未安装；未声称覆盖完整 X.691 值域。
实现依据：[ITU-T X.691 (02/2021), 19.9](https://www.itu.int/rec/T-REC-X.691-202102-I/en)。


本轮已将生成 PDU 子集扩展到 BOOLEAN、NULL、INTEGER、ENUMERATED、BIT STRING、OCTET STRING、OID/RELATIVE-OID、根 CHOICE、SEQUENCE OF/SET OF，以及无扩展非递归 SEQUENCE/SET 的 OPTIONAL/DEFAULT。out-of-line、inline、split 三种产物均由 C11 消费程序编译、链接并运行；用例检查标量和组合类型往返、BOOLEAN/OPTIONAL 非法值、INTEGER 越界、截断时输出保持及 preamble 分配失败。固定向量 B6 和新增组合值用例不是独立 oracle。MSVC Release 生成消费者 4/4；GCC Release 与 ASan+UBSan 的生成消费者各 4/4；含 primitive core 与 CLI dispatch 的筛选各 6/6。
