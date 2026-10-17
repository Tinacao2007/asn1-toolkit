# 分析归档（analysis/）

本目录是 **历史分析快照**，按日期保留，供追溯"当时为什么这样决策"。
**不作为项目现状依据** —— 现状看 [../status.md](../status.md)，变更历史看
[../changelog.md](../changelog.md)，工程纪律看 [../development.md](../development.md)。

| 文档 | 快照日期 | 内容 |
|------|----------|------|
| [analysis-report.md](analysis-report.md) | 2026-10-08（Phase 41） | 项目全面分析：身份定位、目录统计、构建系统、架构核心、阶段状态、风险 |
| [reference-project-review.md](reference-project-review.md) | 2026-10-08（Phase 41） | 父工作区参考树（pasn1 / Python_asn1tools / FFASN1dump / ASN1C / ASN1-Compiler / Wireshark_ASN.1）审查 |
| [architecture-review-and-optimization.md](architecture-review-and-optimization.md) | 2026-10-13（Phase 41+） | 架构优势量化分析与优化方案（P0/P1/P2/P3 条目，含 P0-1~P2-2 实施结果） |

归档规则（见 [development.md 文档同步纪律](../development.md#文档同步纪律)）：

- 既有快照不追改，只追加新归档；
- 快照内容中的数字（行数、测试数）反映当时状态，引用时以快照日期为准；
- `architecture-summary.md`（2026-10-08）因与 `analysis-report.md` 内容重复
  已于 2026-10-15 文档重构时合并删除，完整内容保留在 `analysis-report.md` 中。
