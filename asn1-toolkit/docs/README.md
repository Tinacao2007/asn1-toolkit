# 文档中心

本文档目录是 asn1-toolkit 的全部项目文档入口。文档遵循"一份事实一处权威"原则：

- **架构设计**只在 [architecture.md](architecture.md)，状态与变更不混入；
- **当前状态**只在 [status.md](status.md)，历史沉淀到 [changelog.md](changelog.md)；
- **使用操作**只在 [usage-guide.md](usage-guide.md)，README 只做速览；
- **分析快照**在 [analysis/](analysis/README.md)，标注日期，不作为现状依据。

## 文档清单

| 文档                                                            | 内容                                                                            | 读者          |
| ------------------------------------------------------------- | ----------------------------------------------------------------------------- | ----------- |
| [architecture.md](architecture.md)                            | 编译流水线、分层铁律、核心架构决策、目录与 CMake 目标、编译器接口、AST/IR 语义模型、12 种编解码抽象、三种生成产物形态、错误模型、设计权衡 | 架构师、核心贡献者   |
| [usage-guide.md](usage-guide.md)                              | 环境要求、构建（含 sanitizer / 离线）、CLI 参考、生成产物形态对比、上手工作流、项目集成指引、示例文件、常见问题              | 使用者、集成方     |
| [development.md](development.md)                              | 代码规范、测试体系、四条验证门禁纪律、金标准向量体系、新增 codec 家族流程、文档同步纪律                               | 贡献者         |
| [status.md](status.md)                                        | 当前阶段、构建与测试状态、编译门禁与金标准验证结果、剩余工作、已知限制                                           | 所有人         |
| [remediation-todo.md](remediation-todo.md)                    | 工程评审修正任务、依赖、验收及逐项交给 AI 的提示模板                                                  | 贡献者         |
| [asn1-remediation/SKILL.md](skills/asn1-remediation/SKILL.md) | AI 执行 REV 修正的项目约束、回归选择与同步流程；随修正维护的仓库 skill 源文件                                | AI、贡献者      |
| [c-runtime-contracts.md](c-runtime-contracts.md)              | C11 原语的状态码、所有权、初始化/释放与失败后状态                                                   | C 后端贡献者、集成方 |
| [rev01-03-validation.md](rev01-03-validation.md)              | 第一批修复的证据、实际命令、双构建与矩阵结果及验证边界                                                   | 贡献者、评审者     |
| [rev04-09-validation.md](rev04-09-validation.md)              | 第二批 C PER 修复、独立差分及标准仲裁与验证边界                                                   | 贡献者、评审者     |
| [rev10-12-validation.md](rev10-12-validation.md)              | 第三批 E-XER 生成、编译器错误与 Result/诊断验收                                               | 贡献者、评审者     |
| [cpp-error-contracts.md](cpp-error-contracts.md)              | Result 前置条件、源/线路位置单位、资源失败与 C++ API 迁移                                         | C++ 集成方、贡献者 |
| [c-runtime-test-coverage.md](c-runtime-test-coverage.md)      | C11 声明组的链接、调用、解码与负例登记                                                         | C 后端贡献者     |
| [changelog.md](changelog.md)                                  | Phase 1–45 里程碑、近期变更详录（含每项验证口径）                                                | 所有人         |
| [comparison.md](comparison.md)                                | 与 asn1c / Python asn1tools 的本地实测横向对比与选型建议                                     | 技术选型决策者     |
| [analysis/](analysis/README.md)                               | 2026-10-08 至 10-13 的架构分析、参考项目审查、优化方案历史归档                                      | 追溯者         |
| [codec-entries.md](codec-entries.md)                          | 12 产品入口、JERI 定义与函数/依赖映射                                                       | 集成方         |
| [build-and-package.md](build-and-package.md)                  | 离线、交叉两阶段、安装包与独立消费                                                             | 集成方、CI      |
| [rev13-19-validation.md](rev13-19-validation.md)              | 第四批实际验收范围与结果                                                                  | 评审者         |
| [standard-capabilities.md](standard-capabilities.md)          | 标准基线、语法/IR/生成/runtime、C/C++ 与 oracle 能力登记                                     | 集成方、评审者     |
| [real-and-string-profile.md](real-and-string-profile.md)      | REAL 数值 profile、字符 repertoire 与时间能力边界                                         | 集成方、贡献者     |
| [rev20-24-validation.md](rev20-24-validation.md)              | 第五批约束/规范策略修复、独立对验与 fuzz 证据                                                    | 贡献者、评审者     |

| [generated-artifacts.md](generated-artifacts.md) | 生成目录、manifest、ODR、ABI、所有权及发布边界 | 集成方、贡献者 |
| [emission-planning.md](emission-planning.md) | 最小语言无关规划与 C emitter 后续接入边界 | 后端贡献者 |
| [rev25-28-validation.md](rev25-28-validation.md) | 第六批实际门禁及阶段验收记录 | 评审者 |

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
2. 数字（测试总数、行数、门禁状态）多处出现时，检索并同步当前说明，保留带日期的历史记录原值；
3. 变更完成后在 [changelog.md](changelog.md) 追加条目，并同步 [status.md](status.md) 的状态表。

## 纯 C11 后端后续计划

[纯 C11 生成后端 TODO](c-codegen-todo.md)：Phase49 sub-task 3+ 的分批实施与验收，包含单模块内拆分 `.c`。当前仍是规划。
