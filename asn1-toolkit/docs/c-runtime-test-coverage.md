# C11 runtime 声明与验证登记

本表登记当前已声明/实现函数组，不把 init/writer helper 称为完整 codec。
同组每个声明均由对应测试或其明确调用路径执行；“负例”按函数契约选择，
void free、只读查询等无失败状态的函数不虚构错误返回。

| 声明组（头） | 链接/调用证据 | 解码/线路检查 | 失败或边界检查 |
|---|---|---|---|
| status_name、byte_view、byte_buf（common） | smoke/failure | 状态名、view 字段 | 新旧错误码、未知码、NULL/空 view |
| BigInt init/free/sign/is_zero/cmp/from_decimal/to_int64/to_bytes | smoke/failure | 已知 2^128 limbs、幅值字节与 int64 极值 | 每分配点、limb 边界、重复 free、非法十进制/越界 |
| BigInt signed export/import（bigint） | correctness/failure/oracle | 正负极值、大于 64 bit、补码保护字节、Python signed int | 空/冗余表示、输出不变、signed export 分配点 |
| BitWriter create/destroy/put_bit/put_bits/from_bytes/bitmap/octets/align/take/clear/queries | smoke/failure/correctness | 0..7 偏移 × 0..17 位独立模型、字节/bitmap | 64 bit 宽度、短源/NULL、扩容 OOM/算术溢出、清空后复用 |
| BitReader create/destroy/get_bit/get_bits/get_bits_as_bytes/get_octet/get_octets/align/queries | smoke/failure/correctness | 实际复制输入、UPER/APER 读位置、尾字段 | OOM、截断、资源上限、失败输出不变 |
| ByteReader create/destroy | smoke/failure | 创建输入副本；尚无完整 byte decode API | 两个分配点失败、NULL/长度契约 |
| BitString init/free/resize | smoke/failure/correctness | 固定/变长位值、成功替换、重复释放 | 扩容失败、资源上限、截断时保留旧值 |
| BER tag/length encode/decode | smoke/failure | 高 tag、长长度、已知字节 | 容量、截断、资源上限、不提前写输出；indefinite 仍为受限 API |
| 旧 PER bool/int/unbounded_int/size/constrained_size/bitstring/octetstring | smoke/failure/correctness，经 UPER 包装 | 旧烟测补实际解码；兼容入口的字节/结果 | 分配失败、显式 Unsupported 的片段 scalar、非法范围 |
| PER int_ex/unbounded_int_ex | correctness/failure/live oracle | UPER/APER、有界/无界/半约束/扩展/full int64 与大整数 | 非法范围、冗余 signed content、OOM、超资源 |
| PER size_ex/length_fragment | correctness | root/extension、fixed/var/semi、继续标记与结束零 | 无效标记、无效 variant、scalar 不冒充片段链 |
| PER normally-small/ENUMERATED/OID ex | C correctness + C/C++ parity | 两 Variant normally-small 边界、root/extension ordinal、OID BER contents 与 X.691 23 长度编码 | 非法根 arc、uint64 溢出、非最短/截断 base-128、失败输出不变 |
| PER SEQUENCE/SET preamble | C correctness + C/C++ parity/failure | 扩展标志与 OPTIONAL/DEFAULT presence bitmap | 截断、OOM、非法位、失败输出不变、超上限 |
| PER octetstring_ex/bitstring_ex | correctness/large/failure/live oracle | 短值冻结向量、16K/64K 阈值、多片、后续字段位置 | 截断、源容量、根约束、生产上限、后片 OOM 清理 |
| PER open_type | correctness/large/failure | 前置 1 bit 下两 variant 对齐及内容、完整分片 | 继承共享片段引擎的坏长度/截断/OOM；调用者提供完整内层编码 |
| JER writer init/puts/printf、reader init | smoke/failure | 固定文本输出；reader 只绑定输入 | 缓冲区不足、NULL 输入；printf 未提交尾部可改写 |
| XER writer init/puts、reader init | smoke/failure | 固定文本输出；reader 只绑定输入 | 缓冲区不足、NULL 输入；没有宣称 XML 解码器已实现 |

`Asn1CPerCorrectness` 和 `Asn1CPerLargeValues` 都是纯 C11，无 GoogleTest 或
Python/oracle 运行依赖。后者单独编译 128KiB 限制的 runtime 源副本；生产默认
仍为 64KiB。`Asn1CRuntimeFailureContracts` 的 allocator 宏只存在于测试副本。

## 独立 oracle 与明确排除项

`scripts/validate_c_per_oracle.py` 通过 ctypes 加载纯 C 共享库，与本地
asn1tools 0.166.0 做真实双方 encode/decode 和字节比较。测试 schema 的前后
BOOLEAN 能揭示对齐及片段消费错误；184 个短向量冻结在
`tests/c_lang/golden/per_oracle_vectors.h`。大型值全字节比较不写入巨大头文件。

未修改 oracle 源码，以下边界不盲目按 oracle 输出裁决：

1. `INTEGER(lower..MAX)`：oracle 丢失半约束下界（原有 PER golden 体系也已记录）；
   本次按 X.691 11.7 验证 lower=-100 时 -100 -> `01 00`、28 -> `01 80`。
2. 根外 BIT STRING：oracle 明确抛 NotImplementedError；用独立逐位字段构造及
   两 variant 的扩展/分片测试验证。
3. 根外 OCTET STRING 整片：oracle 用单 determinant 写全部内容，未写最终零；
   本次按 11.9.3.8.3 验证整片后零及多片，保留正确编码。
4. APER 有界变长零长度：oracle 对不存在的内容仍 align；按 11.9.3.3 及其
   Note 2 验证前置/后置 BOOLEAN 的线路位置。

本表新增的 normally-small、ENUMERATED ordinal 和 OID C 原语目前以冻结标准向量
及 C/C++ 两 Variant 字节一致性验证；尚未加入独立 asn1tools/asn1c primitive
differential。枚举映射由调用方提供排序后的 root/extension ordinal，不能据此推断
生成器已支持 ENUMERATED 值映射或生成 PDU 编解码。

这些项在脚本输出中逐条 EXCLUDED，不计入通过的独立差分用例。条文来源：
[ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en)。
这不宣称完整 ASN.1、全部 C codec 或 C emitter 已完成。

## CGEN batch 1 boundary

Unified directory dispatch now selects `TargetLanguage` (default Cpp). C requests
fail through Diagnostics before stage/cache/publication; CppGenerator also rejects
C options on legacy stream paths. Language and C prefix join the existing shape
fields in cache keys. The future `.h/.c` layout/ownership/ABI contract is in
[c-generated-abi.md](c-generated-abi.md); the twelve-entry implementation audit is
[c-runtime-codegen-gaps.md](c-runtime-codegen-gaps.md). No generated C value codec
or additional runtime implementation is enabled. Evidence:
[cgen-batch1-validation.md](cgen-batch1-validation.md).

## CGEN batch 5 additions

`byte_io.h`, `tlv.h`, `tlv_schema.h`, and `value_content.h` are executable APIs,
covered by `CGenBatch5.TlvCore`, generated three-form consumers, allocation faults,
resource limits, entry/shape matrix and whole RRC live asn1c comparison. Primitive
headers are also independently included under C11/C++17 by PublicHeaders.
[Acceptance evidence and explicit unsupported profiles](cgen-batch5-validation.md)
separate envelope checks from canonical value/schema checks and independent oracle
from standard-derived CER vectors. Older skeleton descriptions above are historical
for these new APIs; CGEN batch 6 supersedes the historical OER/text skeleton status below.

## CGEN batch 6 OER and text declarations

`oer.h` primitives and the new `text_tree.h` / `schema_codec.h` are implemented
C11 APIs. The full generated text entry is not the legacy reader-init/raw writer
API in jer.h/xer.h. See [batch 6 validation](cgen-batch6-validation.md): actual
three-shape PDU consumers, 39-consumer twelve-entry matrix, 72 independent frozen
oracle vectors, parser negatives, instruction isolation, canonical boundaries,
every-allocation failure and constrained-budget tests. OER constraints include
IEEE binary32/binary64 with precision-loss rejection and mandatory DER REAL
contents validation even for BASIC-OER. Parser byte offsets are available; full
generated field paths remain a separate diagnostic extension.
