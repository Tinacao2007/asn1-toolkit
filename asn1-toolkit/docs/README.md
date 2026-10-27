# 文档中心

文档按根目录 README 的分类维护，待办单独列入 `todo.md`，遵循“一份事实一处权威”。专题内容已合并到以下主文档，不再另设批次验收、C 后端或构建契约散页。代码、命令、API、文件名和原始日志保留原拼写，说明文字统一使用中文。

## 文档清单

| 文档                                 | 内容                                                      | 读者        |
| ---------------------------------- | ------------------------------------------------------- | --------- |
| [architecture.md](architecture.md) | 流水线、分层铁律、目录/CMake 目标、十二入口、共享规划、生成产物、C/C++ 错误/所有权和 C ABI | 架构师、核心贡献者 |
| [usage-guide.md](usage-guide.md)   | 环境、构建、CLI、三形态、C11 输出/分片、安装 SDK、离线/交叉构建、夜间执行与恢复          | 使用者、集成方   |
| [development.md](development.md)   | 代码规范、测试/门禁、声明覆盖及文档同步                 | 贡献者       |
| [todo.md](todo.md) | 仅当前未完成事项、优先级及验收条件；不含已完成任务 | 贡献者、任务规划者 |
| [status.md](status.md)             | 当前阶段、构建/矩阵/外部验证、标准能力、数值/字符/时间值域与已知限制                    | 所有人       |
| [changelog.md](changelog.md)       | 阶段里程碑、近期变更、REV-01～28 和 CGEN 第 1～7 批历史验收、命令及验证边界         | 所有人、评审者   |
| [comparison.md](comparison.md)     | asn1c、Python asn1tools 与本项目的本地实测和选型比较                   | 技术选型者     |
| [analysis/](analysis/README.md)    | 历史分析、参考项目审查、工程评审快照，仅供追溯                                 | 追溯者       |

## 阅读路径

- 使用者/集成方：根 README → [使用指南](usage-guide.md) → [能力与限制](status.md#standard-capabilities) → 按需查阅 [C ABI](architecture.md#c-abi)。
- 新贡献者：[开发指南](development.md) → [架构](architecture.md) → [状态](status.md)。
- AI 修正：[项目 skill](skills/asn1-remediation/SKILL.md) → [当前 TODO](todo.md) → 相关接口和代码；历史批次要求按需查阅变更历史。
- 评审追溯：[REV 验收](changelog.md#remediation-validation) / [CGEN 验收](changelog.md#cgen-validation) → [历史分析](analysis/README.md)。

## 文档维护约定

1. 架构规则只在架构页，用户操作只在使用指南，执行纪律只在开发指南，当前未完成任务只在 TODO，已完成任务只在变更历史，当前能力只在状态页，验收历史只在变更历史。
2. 历史数字保留原日期及验证边界，不把当时缺工具或未完成项直接当作当前状态。
3. 修改接口或行为后同步对应主文档；变更历史登记实际验证，状态页只更新当前结论。
4. 移动章节须同步跨文档锚点、项目 skill，以及读取文档的脚本/测试；不保留重复副本或跳转占位页。
5. `skills/` 保存仓库 skill 源文件，其存在不代表已安装；`analysis/` 只作为带时间语境的追溯材料。
