# 纯 C11 代码生成后端执行清单

Created 2026-10-23; updated 2026-10-26. Batches 1–6 complete for their declared
profiles; batch 6 opens all twelve C entries and records final gates in
[batch 6 evidence](cgen-batch6-validation.md). Batch 7 remains pending. C type/lifecycle-only and
supported codec profiles share all output forms, shards, manifests and publication.
本清单细化 [P2-4](todo.md#p2-4--c-语言代码生成器-lang-c) sub-task 3+，
承接 [共享规划](emission-planning.md) 与 REV-27，不另建一套前端或 runtime。
Unsupported C codec/profile requests diagnose before publication; historical
initial-dispatch evidence: [batch 1](cgen-batch1-validation.md).

## 目标与现状

目标：同一个 C++17 编译器选择输出 C++17 或纯 C11；C 产物为 `.h/.c`，只链接
`asn1::c_runtime`，用 C 编译器独立编译/链接/运行，不带 C++ 标准库或异常运行库。
保留原有 C++ 后端及默认行为，不通过修改文件扩展名或包装 C++ runtime 冒充 C 后端。

当前已有 C11 原语、失败契约、PER 两 Variant 与部分独立 oracle；
[声明覆盖登记](c-runtime-test-coverage.md) 已登记 PER/TLV 值、结构与 byte decode；
JER/XER 的 legacy raw reader helper 只绑定输入；完整解析与结构解码使用
`text_tree.h` 和 `schema_codec.h`，证据见 [第 6 批](cgen-batch6-validation.md)。
因此不能直接把 C++ emitter 的函数名替换为 C 前缀，也不能假定 C runtime 已支持全部类型。

C++ `--emit-split` 仍按模块；显式 C `--types-only` 可叠加 `--split-max-types N`，
已解决类型及支持 codec 产物的模块内源分片。12 入口已进入声明的支持 profile；最终 C 7×12 fixture 矩阵及安装收尾仍属第 7 批。
既有 RRC 是 LTE EUTRA fixture，不是 TS 38.331；NR 全文测试须另行引入明确版本的输入。

## 必须保留的约束

- compiler 不 include/link C/C++ runtime；runtime 不依赖 AST/IR/前端；生成产物是桥梁。
- 编译器内部保持 Result/Diagnostics，只有 std::bad_alloc 可向外传播；生成 C 使用
  `asn1_status_t` 与 out 参数，不用异常、全局 errno 或 longjmp 处理业务错误。
- 每种语言内 UPER/APER 共用一套 Variant 参数化 PER 核心；DER/BER、COER/OER、
  CXER/XER 保留基础规则加策略模型，不复制完整算法。
- 自研 BigInt、零第三方 runtime 依赖、C11/C++17 保持。
- 最终覆盖固定 12 个产品入口；`per` 是 UPER|APER 别名，JERI 不是另一种规范 JSON。
- 保留 out-of-line、单头 inline、按模块 split 三形态；模块内分片是附加能力。
- 按阶段明确 Unsupported，不能生成成功占位函数、忽略约束或把 reader init 当成解码。

## 分批执行

### 第 1 批：契约、能力审计与 CLI 调度

- [x] **CGEN-01【重要】C 产物与接口设计。** 明确类型/函数前缀、类型布局、参数顺序、
  init/free、借用/拥有、状态传播、输出提交、资源上限及 ABI 版本策略。
  覆盖 INTEGER/BigInt、BOOLEAN/NULL、字符串/位串、OID/REAL、SEQUENCE/SET、CHOICE、
  OF、OPTIONAL/DEFAULT、递归与开放类型；无法完整支持的值域写入能力表。
- [x] **CGEN-02【重要】建立生成需求→C runtime API 缺口表。** 对 12 入口逐项登记
  原语、容器、规范策略、完整 decode、错误定位和资源限制，不凭头文件存在判定可用。
  对照 C++ 实际调用链，区分可复用原语、必须补实现与当前 Unsupported。
- [x] **CGEN-03【重要】增加 `--lang cpp|c` 与统一调度。** 默认 cpp；C 模式 `.h/.c`；
  C 前缀采用单独 `--c-prefix`，明确 C 模式 `--namespace` 的处理并报冲突，不能静默忽略。
  语言/前缀/拆分选项进入缓存键，未知值或不支持的组合在发布前诊断。
  CLI 骨架未具备编码能力时必须明确失败，不能输出假实现。

验收：设计文档和能力表落地；非法选项负例；原 C++ 三形态/组合入口回归。
依赖：读取 C runtime 契约、REV-01..09、REV-25..28；不重做已完成原语。

### 第 2 批：C 类型、生命周期与共享规划

- [x] **CGEN-04【重要】生成 C 类型和生命周期。** 用有前缀的 struct/enum/union；
  OPTIONAL 有显式存在标志，DEFAULT 保留语义；CHOICE 有 discriminator 与无效初始态；
  OF 保存 data/count/capacity；递归依赖用受控间接引用，不能无限按值展开。
  生成可重复 free、失败后可释放/复用的接口，按需要提供深拷贝，禁止浅拷贝拥有指针。
- [x] **CGEN-05【重要】C emitter 实际消费 `codegen/plan.hpp`。** 复用标签、presence
  顺序、离散约束、扩展组和 selector 决定；不足的规划补在语言无关层。
  C 后端只渲染 C 布局和调用，不重新执行语义分析、不复制 C++ 的语义分支。
  有真实生成 C 的调用证据后再关闭 REV-27，不能仅凭规划单测勾选。
- [x] **CGEN-06【重要】命名与公开头。** 覆盖 C11 关键字、保留标识符、模块/类型/字段
  转换碰撞、大小写文件冲突、跨模块限定、重复类型别名；前缀一致应用到辅助函数。
  公共 C 头独立 C11/C++17 可包含；可用 extern C 保护 C 函数声明，但实现仍是纯 C。

验收：小 schema 与跨模块递归 schema 的真实生成 `.h/.c` 编译、链接、init/free/深拷贝；
OOM/部分初始化/CHOICE 切换/多 TU 负例；生成头不包含任何 C++ 专用类型或 runtime 头。

### 第 3 批：模块内分片与可靠产物管理

- [x] **CGEN-07【重要】按类型依赖规划 `.c` 分片。** 在现有模块 split 外增加可配置的
  单模块源分片；暂定参数 `--split-max-types N`，实施时在设计中固定语义。
  按稳定顺序/依赖 SCC 分组；每个函数只定义一次，跨片调用有声明，不依赖 static 私有
  函数跨 TU 可见。公共类型头先保证正确，测量后再决定是否按依赖拆头。
- [x] **CGEN-08【重要】明确三种形态与分片组合。** 默认 out-of-line 为 `.h/.c`；
  C 单头使用 `static inline`，避免 C99/C11 外部 inline 规则造成遗漏/重复符号。
  `--emit-split` 仍按模块，叠加模块内分片；inline 与源分片冲突明确报错。
  可选按 codec 分开源文件，但不能复制类型声明或公共生命周期定义。
- [x] **CGEN-09【重要】扩展 REV-25 输出事务。** manifest/归属验证接受 `.h/.c` 和分片；
  缓存按稳定单元记录语言/codec/分片参数及依赖；暂存成功才发布，旧文件按归属和 hash
  清理，拒绝同目录并发。切换 C/C++、形态、片数不残留参与链接的旧产物，不删除用户文件。
  提供机器可读源清单与可选 CMake include 清单，明确它们与 depfile 的区别。

验收：单模块含多个类型时确实生成多个 `.c`；改变分片阈值后文件集合正确且线路相等；
片数减少/改语言/失败路径/用户文件保护通过。记录最大文件字节、函数数、编译时间及内存。
按类型数分片不保证严格字节上限，单个巨大类型/SCC 超预算要报告，不能宣称无限可拆。

### 第 4 批：首个完整 UPER/APER 生成闭环

进度（2026-10-26）：C11 首个 UPER/APER 生成 profile 已闭环：normally-small/ENUMERATED/OID，生成 C 增加递归
SEQUENCE OF、普通扩展添加、版本括号组、opaque open-type、Numeric/IA5 字符串及
常见 REAL 线路路径，并通过 Release 与 ASan+UBSan 的三种产物消费者。现已补 OF
长度分片、顶层共享的累计字节/元素预算，以及覆盖两次集合扩容的生成分配失败注入；
asn1c UPER 16K 分片及扩展 CHOICE/trailing-root 向量均逐字节对验；三形态消费者通过
Release 与 ASan+UBSan。更广的标准值域和故障矩阵仍作为后续扩展，不属于当前已声明
profile 的未实现行为；详见[阶段证据](cgen-batch4-validation.md)。

已补齐 C11 生成的扩展 CHOICE、第二个 ellipsis 后的 trailing-root，以及表约束
open-type 的自动 typed decode/encode 和拥有型 copy/free；三种产物形态均有运行测试。

- [x] **CGEN-10【严重】补齐首批 PER 所需 C 原语。** 对照 CGEN-02 补 ENUMERATED、
  normally-small、OID/字符/REAL 等所需能力；已有整数/位串/分片原语继续复用。
  每项明确值域与约束支持，无法支持时诊断，不窄化、不丢扩展位。
- [x] **CGEN-11【严重】生成首个完整 PER profile 的编解码编排。** 从小 schema 开始覆盖 SEQUENCE、
  SET/CHOICE、OF、OPTIONAL/DEFAULT、root/trailing-root、扩展单字段/版本组、开放类型。
  同一规划生成 UPER/APER；校验位长度、填充、后续字段位置、片段链与内层 open type。
- [x] **CGEN-12【重要】生成 PDU 的错误/资源门禁。** 截断、坏长度、非法值、OOM、
  深度/累计字节限制；传播 status 并清理部分对象。逐接口规定 out 与游标失败状态，
  不笼统承诺所有失败均回滚。分配故障 hook 仅进测试副本。

验收：out-of-line/inline/split 均用 C 编译器链接运行；C/C++ primitive parity、asn1c
UPER 冻结向量（含新增扩展 CHOICE 与 trailing-root 的 C 编码/解码）通过。asn1c 0.9.29
不提供 APER，故 APER 由共享 C/C++ Variant parity 和生成 PDU 门禁验证。此处关闭的是
已声明 C UPER/APER 首个 profile；完整 X.691 字符/REAL 值域、未知 CHOICE 扩展保留、
更广 allocation/cursor 矩阵及全类型外部 oracle 仍明确不支持/未覆盖，必须诊断或作为
后续扩展，不能宣传为完整 ASN.1 PER 实现。

### 第 5 批：BER/DER/CER，形成用户需要的 UPER+BER 版本

- [x] **CGEN-13【严重】补齐 byte reader、BER 值与构造值 API。** 不能只使用 tag/length
  helper；覆盖高 tag、长/不定长、EOC、符号整数、字符串、SET/CHOICE、显式/隐式标签、
  未知成员与资源边界；共享底层表示和错误处理。
- [x] **CGEN-14【严重】生成 BER/DER/CER 并保留规范策略。** DER/CER 的 DEFAULT、
  排序、长度、REAL、字符串构造/分片等按 schema 与策略共同处理，不把基础 BER 成功
  当作规范规则合规；原语与结构级负例分开。
- [x] **CGEN-15【重要】UPER+BER 组合与电信接入。** 小 PDU 先验证单 codec/组合、
  C/C++、三形态/分片线路一致，再推进 RRC slice、RRC 全模块、LPP/S1AP。
  全模块遇到的类型/runtime 缺口必须补齐或列为阻断，不能生成未实现 stub。

里程碑 M1：`--lang c --codec uper,ber` 真正生成纯 C 并链接运行；
**RRC 全模块可生成、多 `.c` 编译链接，且代表 PDU 通过真实编解码与独立对验**。
先用 RRC slice 通过不能声称全模块已通过；全模块可编译也不能声称所有 PDU 已验证。

完成记录（2026-10-26）：CGEN-13..15 按上述验收完成，M1 的整份 RRC
1037 类型/9 个 C 源文件已编译链接，代表 PDU 的 UPER/BER 与 live asn1c 0.9.29
字节一致并互解。三形态/分片及单 codec/组合 C/C++ 有 21 个实际消费者。
LPP/S1AP 按 CGEN-15 允许的口径登记明确阻断，不计为通过；完整值域不据此扩大。
Release 461/461、ASan+UBSan 494/494、原 C++ 7×12 语法矩阵 84/84。
详见 [第 5 批验收和限制](cgen-batch5-validation.md)。第 6 批已按其声明 profile 完成；第 7 批尚未完成。

### 第 6 批：OER/COER 与 XML/JSON 家族

- [x] **CGEN-16【重要】C OER/COER 原语与结构生成。** 数值、长度、presence/扩展、
  开放类型和规范表示；拒绝精度丢失；两入口策略独立，组合掩码不污染单入口。
- [x] **CGEN-17【重要】完整 C JER/JERI。** 实现词法/解析与结构解码、字符串转义、
  精确整数、重复键和未知字段策略、资源限制及已支持指令；reader init 不算解析实现。
- [x] **CGEN-18【重要】完整 C XER/CXER/E-XER。** XML 输入解析、字符转义、结构解码、
  标签/属性与指令、规范输出及错误传播；不引入第三方 runtime XML 库。
  未实现指令明确 Unsupported，不用 BASIC-XER 替代其他入口。
- [x] **CGEN-19【重要】12 入口能力登记与组合验证。** 补全 enum/CLI/函数/include/
  runtime 依赖/策略/诊断映射；单入口与组合实际编码比较，避免共享全局掩码串策略。

验收：每家族先通过原语与生成 PDU 测试，再开放相应能力；oracle 缺口按条文和冻结向量
明确登记，不把 OER/XER 基础编码器冒充独立 COER/CXER 规范 oracle。

第 6 批验收：Release 472/472、ASan+UBSan 506/506、原 C++ 7×12 语法 84/84；
39 个 C11 单/组合/三形态消费者、72 组独立 asn1tools 互解及离线双输入回放，
原 whole-RRC/asn1c 回归保留。已同步 docs 和项目 skill。
第 6 批时 E-XER UNTAGGED、文本 typed-open 等边界明确 Unsupported；后者的已知表选择
路径由第 7 批补齐，前者继续明确拒绝。
不声明全标准值域或 Clang 门禁通过。详见 [第 6 批证据](cgen-batch6-validation.md)。

### 第 7 批：工程门禁、安装与完成验收

- [x] **CGEN-20【重要】最终 C 生成矩阵。** 7 fixture ×12 入口全部生成并独立
  `-std=c11 -fsyntax-only`；同时保留现有 C++ 7×12 门禁。阶段中未支持的组合登记拒绝，
  不算最终 84/84 成功，也不通过删 fixture/codec 缩减目标。
- [x] **CGEN-21【重要】链接/运行/差分矩阵。** 三形态和模块内分片的多 TU；纯 C
  可执行不依赖 C++ 标准库；代表电信 PDU、原语边界、外部 oracle、冻结向量、生成
  decoder fuzz；自有 C/C++ 对比与外部验证分别计数。
- [x] **CGEN-22【重要】CMake 与安装消费。** 生成文件名集合变化时采用配置期规划或
  受控清单机制，明确新增类型/新模块时如何重配置，不能只依赖构建期 glob。
  安装后 C-only 工程用 SDK 和宿主生成器编译；交叉构建不在宿主执行目标程序。
  扩展分层门禁识别 C 后端，保持 compiler-only/runtime-only 独立构建。
- [x] **CGEN-23【重要】最终工具链门禁。** Release 与 ASan+UBSan 全量；GCC/Clang
  严格 C11；MSVC 按真实 C11 支持执行并登记边界。区分 MSVC ASan 与 ASan+UBSan。
  不以构建耗时或测试数量替代覆盖证据。
- [x] **CGEN-24【重要】文档与收口。** 同步 README、status、usage、架构、C 契约、
  生成/runtime 能力表、安装示例、changelog 和项目 skill；记录版本与 ABI/许可边界。
  只有真实 C 生成消费共享规划的验收完成才勾选 REV-27；逐项保留未验证范围。

第 7 批执行记录见 [cgen-batch7-validation.md](cgen-batch7-validation.md)。新增门禁会完整
记录所有失败组合；LPP named bits/UTCTime、S1AP 非连续 PER 根与已知表选择的 JSON/XML open type 已补齐；
三套原 84 组合门禁、Release/Sanitizer 全量和安装消费均通过。未知动态类型等范围仍明确登记。

里程碑 M2：12 入口与三形态/模块内分片达到所登记支持范围，最终 C/C++ 门禁和安装
消费全部通过；仍不等于整部 ASN.1 标准或全部 3GPP 消息得到认证。

## 当前可运行的 C type-only 示例

以下命令已经由 CGEN-03/07 验收；`128` 是软类型数预算，不是性能承诺或字节上限：

```sh
asn1cxx --lang c --types-only --c-prefix rrc \
  --emit-split --split-max-types 128 --emit-dir build/gen-c \
  tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn
```

当前产物包含一个完整公共 `generated.h`、内部声明头、模块归属的 `.c` 分片、
`generated-sources.json`、`generated-sources.cmake` 和 `.asn1cxx-manifest`。
RRC fixture 已通过整个类型产物集的 C11 编译、链接和代表生命周期调用；这不表示
C codec 可用，也不表示 LTE/NR 全部 PDU 或 TS 38.331 wire 行为已验证。文件命名与组合规则
见 [C 输出契约](c-codegen-output.md)，批次证据见 [CGEN 第 3 批记录](cgen-batch3-validation.md)。

## 每批执行与完成记录

按第 1～7 批顺序实施；任一批发现新的必要 runtime 缺口，登记到对应 CGEN，
补实现/失败契约/线路证据后再开放。每批同步项目 skill 的可复用流程与对应 docs，
不修改已经存在的历史验证数字；本清单不是一次性授权执行所有实现任务。

```text
请执行 docs/c-codegen-todo.md 的第 N 批及必要依赖。
先读当前 diff、项目 skill、C runtime 契约和共享规划，保留用户修改。
不要合并 compiler/runtime，不用 C++ 包装替代纯 C，不拆成两套 UPER/APER。
验证真实生成 C 的编译、链接、运行、错误传播和适用 oracle，不能只检查扩展名。
同步本批状态、验证记录、相关 docs 和 docs/skills/asn1-remediation/SKILL.md。
```

完成记录包含：实际修改、命令/工具版本、通过和未执行范围、产物大小/分片数、
失败路径、oracle 来源、剩余阻断。禁止预先填写成功计数或全部勾选。
