# 语言无关发射规划（REV-27）

## 最小设计决定

规划层位于 `include/asn1/codegen/plan.hpp`，只依赖 IR 和 C++17 标准库。
编译器本身是 C++17；“供 C 后端复用”不要求规划接口使用 C ABI。
它不链接 runtime、不包含 C/C++ 目标类型拼写、不访问读写缓冲区，未引入新的解释式 runtime。

| 输入/输出 | 本次决定 | 消费位置 |
|---|---|---|
| Field / FieldPlan | effective tag、presence 位索引、表约束 selector 的借用视图 | 规划单测；目标语言的 tag/selector 渲染后续迁移 |
| SequenceDesc / SequencePlan | root 后 trailing-root 的统一 presence 顺序；扩展组顺序及 singleton/version-bracket 区分 | C++ PER encode/decode preamble 的 presence 与扩展数 |
| ConstraintDesc / ConstraintPlan | 保留归一化离散区间、PER bound kind 和根区间 membership 要求 | C++ native INTEGER 约束检查 |

计划借用输入 descriptor；IR 必须比计划活得久且期间不可修改。presence 不将 DEFAULT
当成 optional 宿主成员，但两者均占 PER 根位图的一个位置。扩展 bitmap 的存在
不代替 version-bracket 内层 SEQUENCE preamble。selector 保留 governor 与完整条目，
不将无法表示的键静默压成 int64。规划不重新执行标签推断或约束归一化。

本次只抽出已经有重复风险且可直接替换的决定，不重写所有家族 emitter。
标签/selector 的既有目标语言渲染尚未全部迁移；不能声称后端现在只剩文本打印。
`EmissionPlanning.*` 测试核查 root/trailing-root、OPTIONAL/DEFAULT、显式标签、
selector、两类扩展组和离散约束。C++ 三形态/12 入口线路门禁用于核查替换未改变线路。

## Phase49 接续与验收边界

REV-01..07 的错误成功路径已经有独立验收记录；扩展 C emitter 前继续保留这些门禁。
本批完成最小规划设计与 C++ 首批接入，**没有实现 `--lang c` 或 C emitter**。
REV-27 的“真实 C 后端消费同一决定”需要 P2-4 sub-task 3+，现阶段不能运行该验收。

后续顺序：

1. C emitter 在同一 `asn1_codegen` 边界或仅依赖 IR/common 的私有目标内调用该规划，
   把 tag、presence、约束、扩展组、selector 渲染为 C 描述符和状态码检查；不能复制语义判定。
2. 增加同一 schema、值、变体的 C/C++ 生成驱动，并复用独立 oracle；手写 C 原语驱动
   或 smoke 通过不能替代生成 PDU 的证据。
3. 按 codec/结构逐项开放 C 能力，未知类型/约束明确 Unsupported；保留 C11 独立编译、
   纯 C 链接、多 TU、所有权/资源失败、Release 和 ASan+UBSan 门禁。
4. 每次迁移缩小目标语言 emitter 的语义分支，但无需为抽象重做前端或合并 runtime。

此文是当前代码和待接入边界的设计审查记录，不代表另一个外部评审者已批准完整 C 后端。


CGEN-03 adds `--lang c` dispatch with an explicit Unsupported failure, without
implementing a C emitter or closing the real C plan-consumption boundary above.
