# 产品 codec 入口登记（REV-15/16）

12 是产品选择入口数，不是 12 种独立标准线路格式。`per` 只等于 `uper,aper`。
CLI、C++ EmitOptions 均提供以下入口。C11 后端的 12 入口及值域限制见
[CGEN 第 6 批](cgen-batch6-validation.md)；C 的消费目标统一为 `asn1::c_runtime`，
下表消费目标仍记录 C++ 家族依赖。

| 入口 | 规范归属/项目含义 | 生成函数后缀 | 消费目标 | 本批能力证据 |
|---|---|---|---|---|
| ber | X.690 BER | ber | asn1::ber | 单独生成、链接、往返 |
| der | X.690 DER，BER+策略 | der | asn1::der | 同上 |
| cer | X.690 CER，BER+策略 | cer | asn1::cer | 同上；不是外部 CER oracle |
| uper | X.691 非对齐 PER | uper | asn1::per | 同上；共享 Variant 核心 |
| aper | X.691 对齐 PER | aper | asn1::per | 同上；共享 Variant 核心 |
| oer | X.696 BASIC-OER | oer | asn1::oer | 单独/组合行为比较 |
| coer | X.696 CANONICAL-OER，OER+策略 | coer | asn1::coer | 同上 |
| xer | X.693 BASIC-XER | xer | asn1::xer | 同上 |
| cxer | X.693 CANONICAL-XER，XER+策略 | cxer | asn1::cxer | 同上 |
| exer | X.693 EXTENDED-XER | exer | asn1::exer | 同上；单选保留旧 xer API 别名 |
| jer | X.697 JER，保留既有指令感知行为 | jer | asn1::jeri | 与 jeri 的同值结果比较 |
| jeri | 项目 JER instruction 入口，非独立传输语法 | jeri | asn1::jeri | 转发共享 JER 实现，不另造 canonical JSON |

消费目标列是覆盖指令辅助调用的家族依赖，非每个最小 schema 都需要其中全部符号。
CER 的共同规范原语检查复用 DER，当前 `asn1::cer` 传递依赖
`asn1::der` → `asn1::ber`；构造形式与分片策略仍由 CER 实现。
手工指定静态库时也必须包含该依赖链，推荐使用导出的 CMake 目标。
局部标准能力及后续空白见 [标准能力矩阵](standard-capabilities.md)。
`asn1::jeri` 传递依赖 `asn1::jer`；生成的 JER 可调用 `asn1::jeri::*`，因此统一使用前者。
JERI 选择会生成共享 `encode_jer/decode_jer` 以及 `encode_jeri/decode_jeri` 入口，
组合同时选择 JER/JERI 时共享实现只生成一次。现有 JER 不改为“忽略指令”模式，
以免改变已有 schema 的线路行为。

标准依据：[ITU-T X.697 (02/2021) 正文](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.697-202102-I!!PDF-E&lang=e&type=items)
第 5、7.5、10、11 章及附录 B；另见 [目录](https://www.itu.int/dms_pubrec/itu-t/rec/x/T-REC-X.697-202102-I!!TOC-HTM-E.htm)
列有 7.5 encoding instructions、8–19 指令语法和 ARRAY/BASE64/NAME/OBJECT/TEXT/
UNWRAPPED，以及各类型的 JER 编码。该依据不支持“Canonical JSON Encoding Rules
(JERI)”的旧名称。这里核清命名与入口，不声称已审完全部指令作用域、限制和覆盖。

运行时已有 instruction helpers 和 IR/codegen 通路；完整 X.697 语义与每条指令的
独立符合性证据仍属于 REV-20/21/24。合法但尚未覆盖的结构不能仅凭本批 smoke
宣称支持。所有家族的未知标准边界同样保留后续专项核查，不以自往返代替金标准。

组合回归 fixture 明确声明 XER INSTRUCTIONS，并用 ENCODING-CONTROL JER 为
SEQUENCE 设置 ARRAY，使 XER ATTRIBUTE 与 JER ARRAY 的所属家族明确。
这只验证所用子集；不据此声明所有编码引用、继承、否定指令和限制已完整支持。

当前 JER 控制段支持 `[BASE64] OCTET STRING` 和 `[ARRAY] SEQUENCE`；其余解析可接受
但尚未落实的控制段组合返回编译诊断 `unsupported JER ENCODING-CONTROL instruction/target`，
不会静默生成忽略指令的 JSON。此限制也适用于只选择其他 codec 的 schema 分析；
完整控制段作用域、继承和更丰富目标仍交由 REV-21 核查/扩展。
