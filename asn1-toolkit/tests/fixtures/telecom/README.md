# Telecom ASN.1 fixtures

3GPP ASN.1 模块的副本，用作 **parse / analyze / emit 编译门禁** 与 **金标准
差分向量** 的输入；另有一份 RRC 精简切片用于 **编解码往返**。它们是编译器
测试输入，**不是规范性规范**。

## Fixture 清单

| Fixture | 来源 | 用途 |
|---------|------|------|
| `rrc_8_6_0/EUTRA-RRC-Definitions.asn` | `Python_asn1tools/tests/files/3gpp/rrc_8_6_0.asn`（第一个模块） | parse + analyze + UPER emit；全 codec 编译门 |
| `lpp_14_3_0/LPP-PDU-Definitions.asn` | `Python_asn1tools/tests/files/3gpp/lpp_14_3_0.asn` | parse + analyze + UPER emit；全 codec 编译门；`--emit-split` 单模块路径验证 |
| `s1ap_14_4_0/s1ap_14_4_0.asn` | `Python_asn1tools/tests/files/3gpp/s1ap_14_4_0.asn` | 多模块 IOC；APER emit；全 codec 编译门；`--emit-split` 多模块（含递归环）验证 |
| `rrc_slice/rrc_slice.asn` | 改编自 RRC Rel-8 BCCH-BCH / PCCH / Paging | UPER/OER emit + UPER 往返对照已知 hex |
| `per_golden.asn` | 综合构造（约束原语/长度 determinant/分片/扩展 addition/版本括号组） | PER/APER 金标准差分向量输入（54 向量，asn1c oracle） |
| `tlv_oer_golden.asn` | 综合构造（混合标签/DEFAULT/SET/SET OF/嵌套 CHOICE） | BER/DER/OER/JER 字节级往返（8 用例） |
| `tlv_golden.asn` | 综合构造（IMPLICIT/EXPLICIT 标签、SET/SET OF 序、NULL/负数/REAL 等） | BER/DER/OER 金标准差分向量输入（16 用例，asn1tools + asn1c oracle） |

`cer_golden.asn` 不在本目录，位于 `tests/fixtures/cer/`（CER 金标准 fixture）。

## 金标准向量

| 向量文件 | 关联 fixture | Oracle |
|----------|--------------|--------|
| `tests/per/golden/per_golden_vectors.txt` | `per_golden.asn` | asn1c（`scripts/gen_per_golden.py`） |
| `tests/tlv/golden/tlv_golden_vectors.txt` | `tlv_golden.asn` | asn1tools + asn1c（`scripts/gen_tlv_golden.py`） |

## 版权

原始 3GPP ASN.1；此处仅作为编译器测试输入再分发，不构成规范引用。
