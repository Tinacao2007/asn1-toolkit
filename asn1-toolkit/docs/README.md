# 文档中心

本文档目录是 asn1-toolkit 的全部项目文档入口。文档遵循"一份事实一处权威"原则：

- **架构设计**只在 [architecture.md](architecture.md)，状态与变更不混入；
- **当前状态**只在 [status.md](status.md)，历史沉淀到 [changelog.md](changelog.md)；
- **使用操作**只在 [usage-guide.md](usage-guide.md)，README 只做速览；
- **分析快照**在 [analysis/](analysis/README.md)，标注日期，不作为现状依据。

## 文档清单

| 文档 | 内容 | 读者 |
|------|------|------|
| [architecture.md](architecture.md) | 编译流水线、分层铁律、核心架构决策、目录与 CMake 目标、编译器接口、AST/IR 语义模型、12 种编解码抽象、三种生成产物形态、错误模型、设计权衡 | 架构师、核心贡献者 |
| [usage-guide.md](usage-guide.md) | 环境要求、构建（含 sanitizer / 离线）、CLI 参考、生成产物形态对比、上手工作流、项目集成指引、示例文件、常见问题 | 使用者、集成方 |
| [development.md](development.md) | 代码规范、测试体系（31 个测试可执行）、四条验证门禁纪律、金标准向量体系、新增 codec 家族流程、文档同步纪律 | 贡献者 |
| [status.md](status.md) | 当前阶段、构建与测试状态、编译门禁与金标准验证结果、剩余工作、已知限制 | 所有人 |
| [changelog.md](changelog.md) | Phase 1–45 里程碑、近期变更详录（含每项验证口径） | 所有人 |
| [comparison.md](comparison.md) | 与 asn1c / Python asn1tools 的本地实测横向对比与选型建议 | 技术选型决策者 |
| [analysis/](analysis/README.md) | 2026-10-08 至 10-13 的架构分析、参考项目审查、优化方案历史归档 | 追溯者 |

## 阅读路径

**新用户 / 集成方**
README（根目录）→ [usage-guide.md](usage-guide.md) 快速开始与 CLI → 按需查阅
[architecture.md](architecture.md) 的"生成产物形态"一节。

**新贡献者**
[development.md](development.md) → [architecture.md](architecture.md) → [status.md](status.md)
（先了解验证门禁纪律再动手改代码）。

**架构评估 / 技术选型**
[architecture.md](architecture.md) → [comparison.md](comparison.md) → [status.md](status.md)。

## 文档维护约定

文档与代码同步是每次变更的收尾动作，详见
[development.md 的"文档同步纪律"一节](development.md#文档同步纪律)。修改文档时：

1. 同一事实只在一处权威（见上表），其余位置只做链接；
2. 数字（测试总数、行数、门禁状态）多处出现时，全局检索旧数字后统一更新；
3. 变更完成后在 [changelog.md](changelog.md) 追加条目，并同步 [status.md](status.md) 的状态表。
