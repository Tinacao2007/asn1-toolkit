# C11 runtime 错误与所有权契约

适用于当前 `include/asn1/c_runtime/*.h` 和生成 C 的声明接口。C emitter 与十二入口
已完成登记 profile；标准线路范围与验收边界见 [第 7 批记录](cgen-batch7-validation.md)。

## 状态码

新增 `per_*_bigint_root_ex` 支持 int64 有限根范围及任意精度扩展值；两种 Variant
共用实现。根必须同时有上下界，构造解码的输出须为新/零/已释放对象，失败保持
输出不变，reader 游标沿用原语契约，可能已推进。生成公共 decoder 仍通过临时对象
保持目标值失败不变。该原语只表示连续根区间，不能替代不连续约束的成员检查。

原有 0..7 数值保持不变，不应把 C/C++ 错误枚举直接强制转换后假设永远对应。

| 状态 | 含义 |
|---|---|
| `ASN1_ERR_OUT_OF_MEMORY` (8) | malloc/calloc/realloc 失败 |
| `ASN1_ERR_RESOURCE_LIMIT` (9) | 已配置的字符串字节数或 BigInt limb 上限被超过 |
| `ASN1_ERR_LENGTH_OVERFLOW` (3) | 长度/容量不能表示、整数转换越界，或调用者输出缓冲区不足 |
| `ASN1_ERR_TRUNCATED` (1) | 输入缺少所需数据 |
| `ASN1_ERR_INVALID_ARGUMENT` (6) | 参数不满足接口契约 |
| `ASN1_ERR_UNSUPPORTED` (7) | 当前未支持能力 |

已有 BER length 原语对 indefinite indicator 返回 LengthOverflow 的历史行为
暂保留；这不表示 indefinite 本身是非法 BER，后续实现另行处理。

`per_*_bigint_ranges_ex` 接受排序的不连续有限根区间。扩展类型的 PER 扩展位按
X.691 13.1 的根 RANGE 判断；非扩展类型仍精确检查成员，空洞非法。named-bit
normalization 与 typed selector 分配计入累计预算。C runtime/生成 ABI 3 要求
一同重编译；primitive、生成入口的游标/输出保证仍分别按各自契约处理。

## 生命周期和输出

所有指针必须指向有效对象/足够大的存储；不得伪造公开结构的指针、计数、容量。
输入输出不得非法重叠。NULL 数据指针只允许搭配零长度。创建函数不读取旧输出
指针，不自动释放旧对象；不能把活对象的唯一指针直接传给 create 覆盖。

| API 组 | 所有权及初始化 | 失败后状态 |
|---|---|---|
| `byte_view` / `byte_buf` | 无验证的非拥有构造；使用者维持底层内存有效 | 无状态码；后续使用函数验证参数 |
| `bigint_init` / `bitstring_init` | 在新对象或已 free 对象上置零；不要用 init 覆盖活对象 | void；不分配 |
| `bigint_init_from_decimal` | 构造函数，目标必须是新对象、零对象或已 free 对象；复用先 free | 临时构造失败时不修改目标；若希望无条件 free，请先 init 为零 |
| `per_decode_unbounded_int` | 同 BigInt 构造契约，不覆盖活对象 | 输出不变；reader cursor 可能已推进 |
| `bigint_to_int64` / `bigint_to_bytes` | const 输入；返回的字节是无符号大端幅值，不是完整 BER TLV 或有符号补码 | 输出参数不变 |
| `bitstring_resize` | 目标须已 init；保留所有权并可重复使用 | 数据、指针、长度和容量不变 |
| `per_decode_bitstring` | 目标须已 init；成功释放旧存储并接管新存储 | 输出对象不变；reader cursor 可能推进 |
| `bit_writer_create` | 新建拥有缓冲区的 handle | 输出指针不变 |
| `bit_writer_put_*` / `align` | writer 须有效；每次操作先预留所需容量 | 失败不提交有效位或数据；容量不是稳定 API |
| `bit_writer_take` | 返回独立 malloc 缓冲区并清空 writer；部分尾字节零填充 | 输出与 writer 的有效位不变 |
| `bit_writer_clear` | 保留容量，清空有效数据，可重新写 0 bit | void |
| `bit_reader_create` / `byte_reader_create` | 拷贝输入，调用者之后可释放/修改原输入 | 输出指针不变 |
| `bit_reader_get_*` | 返回字节缓冲区由调用者拥有 | 输出及读位置不变；截断在分配前检查 |
| BER tag/length encode/decode | 调用者提供缓冲区或值输出 | 输出缓冲区、值及 consumed 均不提交 |
| JER/XER writer init/puts | 借用调用者输出缓冲区；puts 不做 JSON/XML 转义 | 失败 used/已提交前缀不变 |
| JER writer printf | 借用缓冲区；格式及参数须满足标准 printf 契约 | used/已提交前缀不变；未提交尾部可能被 vsnprintf 修改 |
| JER/XER reader init | 借用输入；只初始化游标，尚非完整解析器 | 参数非法时 reader 不变 |
| PER scalar decode | 标量输出成功后提交 | 输出值不变；reader cursor 可能推进 |
| PER 组合 encode | 多次调用 writer 的组合操作 | 可能已经写入前缀，不承诺整次编码回滚；失败后丢弃/clear writer |

`bigint_free` / `bitstring_free` 只接受已初始化或成功构造对象（或 NULL），释放
后置零，可重复 free。不得 free 任意未初始化结构。`*_destroy(NULL)` 安全，
但同一非 NULL handle 不得 destroy 两次；销毁后调用者应自行置 NULL。

所有返回的 `uint8_t*` 缓冲区使用与 runtime 相同 C allocator/CRT 的 `free()`
释放；空结果也可能返回非 NULL 的可释放指针。不可跨不同 Windows CRT 配置
分配/释放。JER/XER 输入、输出缓冲区始终由调用者拥有。公开结构不可浅拷贝
后分别 free；所有权移动后必须将源置为不拥有状态。

## 失败注入门禁

`Asn1CRuntimeFailureContracts` 单独编译 C 源码并启用私有测试 allocator，
逐分配点注入失败、检查状态/输出/存活分配数。生产 `asn1::c_runtime` 不包含
测试状态或 allocator 配置；测试替换接口不是公共 ABI。

## REV-04..09：PER 原语约定

`per_core.c` 是唯一 C PER 核心，使用 `asn1_per_variant_t` 选择 UPER/APER。
`per.c` 只保留旧 ABI 的 UPER 包装。`_ex` 不是另一套实现。

- INTEGER：有界单区间、仅下界、仅上界、无界、根区间可扩展，以及完整 int64
  域；超出 int64 的无界值使用 BigInt 接口。无界内容采用最短有符号补码。
  `bigint_to_bytes` 仍是无符号幅值；新增 `to_twos_complement` 与
  `init_from_twos_complement` 明确区分表示。构造输出仍须为新/零/已 free 对象。
- 长度：新描述符区分 unconstrained、constrained、semi-constrained；fixed 是
  constrained 且 lower==upper；extensible 描述根与根外。SIZE 的一般长度编码
  使用实际长度，不减去 semi-constrained 下界；有界上界 >=64K 也走一般分片。
- `encode/decode_length_fragment` 只处理一个 determinant，显式返回/接受 more；
  调用者必须交错处理片段及数据，整片后仍须最终零 determinant。标量 size
  helper 不能表示分片内容，遇到一般长度 >=16K 返回 Unsupported，禁止丢失
  more 信息。生成的 OF 路径通过 collection fragment API 交错处理长度和元素。
- 旧 `encode_octetstring` 编码完整无界 UPER 值；旧 `decode_octetstring(r,length)`
  保留“读取已知长度裸 payload”的 ABI。新代码应配对使用 `_ex` 完整值接口；
  不要把旧 decode 当作能自行消费长度及片段的完整解码器。
- BIT STRING 的长度单位是位；容量验证使用 ceil(bits/8)，尾位总是从源字节高位
  开始，scalar `put_bits` 超过 64 bit 返回 InvalidArgument。NamedBitList 的
  合法尾零裁剪由调用者基于类型信息决定，本原语不猜测。
- APER 处理整数范围阈值、变长字符串、固定 BIT STRING >16 bit、固定
  OCTET STRING >2 octet、长度与 open-type 对齐；零长度不添加不存在的内容对齐。
  内部字段完整编码和 open type 的尾部零填充仍由调用者完成；空内层编码也须
  提供一个零 octet，不能传入零字节的 open-type 内容。
- decoder 默认保持 64KiB 字节上限；超过返回 ResourceLimit，不提交输出。
  整个 C runtime 可一致使用 `-DASN1_MAX_STRING_BYTES=...` 重新编译，配置必须
  为正且可表示为位数。本次单独的 128KiB 测试目标不改变生产默认值。
- 生成 C PDU 的 reader 以顶层 decode 为预算根，累计申请字节默认 16 MiB、
  集合/扩展元素默认 65536；嵌套 open-type reader 共享预算。预算统计申请总量，
  包含 realloc 的新请求，不仅是最终存活内存。

尚未支持的字符集原语、完整生成类型编解码及复杂约束
不会因这些原语完成而自动获得支持。测试证据和 oracle 能力边界见
[c-runtime-test-coverage.md](c-runtime-test-coverage.md)。

## CGEN batch 4 primitive additions

The C runtime exposes the X.691 SEQUENCE/SET preamble as an extension-present
bit and presence bitmap. The C emitter consumes it for supported non-extensible
SEQUENCE layouts. Decode owns the returned bitmap and preserves the caller result
on truncation or allocation failure; release it with
`asn1_per_sequence_preamble_free`. Encoding validates the bitmap before writing,
while a later writer failure may leave a prefix.

The shared `asn1_per_variant_t` core provides normally-small non-negative whole
numbers, ENUMERATED ordinals, and OBJECT IDENTIFIER/RELATIVE-OID contents for
uint64 arcs. APER large normally-small and OID payloads apply octet alignment;
UPER remains bit-packed. OID values use BER contents as required by X.691 23 and
reject invalid roots, malformed base-128 encodings, overflow, and values beyond
the configured string cap. OID decode preserves caller outputs on failure.

The generated C UPER/APER profile additionally covers recursive SEQUENCE OF graphs,
extensible SEQUENCE/SET additions (including version-bracket groups), opaque PER
open-type payloads, selected restricted character strings, and a host-`double` REAL
profile. Out-of-line, inline and split consumers are covered for the current fixture.
The supported string profile is NumericString, PrintableString, VisibleString,
IA5String, BMPString and UniversalString; other string kinds still use the existing
octet representation and are not claimed as character-set compliant. REAL supports
binary64 and selected decimal forms, not exact decimal REAL semantics. Known
table-constrained open fields automatically decode into an owned typed value while
retaining raw complete-encoding octets. Generated APIs expose its type ID and pointer;
the matching free helper, copy path, and selected-type re-encoding preserve ownership.
Beyond the closed CGEN-10..12 declared profile, full standard value domains,
the broader extension/recursive fault matrix and independent oracle checks
remain future coverage. Collection fragment handling
and cumulative PDU allocation/element budgets are implemented. A generated public
decode resets its reader budget; nested readers made for extension open types share
the root budget. The defaults are 16 MiB of cumulative requested allocation bytes
and 65536 collection/addition elements. Override with
`ASN1_MAX_PDU_ALLOCATION_BYTES` and `ASN1_MAX_PDU_ELEMENTS`. The budget counts
allocation requests over the entire decode, including resized buffers, rather than
only the final live heap size. See [batch 4 progress](cgen-batch4-validation.md).

## C TLV policy and byte stream APIs (CGEN batch 5)

`byte_io.h` now supplies read/view/remaining/skip and an owning byte writer.
Failed reads/skips do not advance; failed append does not change logical bytes.
`tlv.h` parses one complete TLV and returns borrowed content/encoding views.
The source reader/input must outlive those views. Primitive integer decode
requires an empty initialized BigInt destination; octet decode transfers a new
malloc buffer on success. Outputs remain unchanged on failure.

Generated BER/DER/CER public decode calls are transactional for both initialized
value and byte reader cursor. Success consumes exactly one TLV and replaces/frees
the previous value. The per-call decode budget includes nested payload allocation
and collection/indirect nodes; reader input copies are separately bounded.
Policies share one engine. See [batch 5 contracts, tested limits and exceptions](cgen-batch5-validation.md).
`asn1_ber_tag_t.number` widened from uint32_t to uint64_t: rebuild runtime and all
consumers together; do not mix old object files with the new header.
