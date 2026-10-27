---
name: asn1-remediation
description: 执行 asn1-toolkit 当前 TODO 的实现、核查和验证任务，同步代码、证据及文档；按明确要求维护历史 REV/CGEN 批次。
---

# ASN.1 工程修正

本文件维护可复用的操作约束；当前未完成事项以
[当前 TODO](../../todo.md) 为准；已完成任务和验收要求查阅 [变更历史](../../changelog.md)，当前能力以
[status.md](../../status.md) 为准。这里不固定测试数量，也不把历史缺陷当作当前事实。
路径均相对于本文件；执行脚本时从仓库根目录运行。

## 接手任务

读取仓库适用的 AGENTS.md（如存在）、指定 REV 或 CGEN 批次、相关接口/实现/测试和当前 diff。
按用户指定的当前 TODO 编号处理必要依赖；只有明确要求维护历史批次时才查原 REV/CGEN 清单。保留已有未提交修改。先核实调用链或复现，
再做修复；若代码已变更，记录证据，避免重复修改。

只读取与本次任务相关的契约与验收记录：

- C11 内存、状态、所有权：[C 运行时契约](../../architecture.md#c-runtime-contracts)、
  [声明覆盖登记](../../development.md#c-runtime-tests)、
  [REV-01..03](../../changelog.md#remediation-validation--rev-01-03)。
- 位操作、PER Variant、分片与独立向量：[REV-04..09](../../changelog.md#remediation-validation--rev-04-09)。
- C++ Result、诊断与 E-XER 属性：[C++ 错误契约](../../architecture.md#cpp-error-contracts)、
  [REV-10..12](../../changelog.md#remediation-validation--rev-10-12)。
- 缓存、并发、编码规则组合与构建：[REV-13..19](../../changelog.md#remediation-validation--rev-13-19)、
  [产品入口登记](../../architecture.md#codec-entries)、[安装与交叉构建](../../usage-guide.md#build-and-package)。
- 标准语义与独立对验：[能力矩阵](../../status.md#standard-capabilities)、
  [REAL/字符/时间支持范围](../../status.md#real-and-string-profile)、
  [REV-20..24](../../changelog.md#remediation-validation--rev-20-24)。
- 产物/分层/后端规划：[产物契约](../../architecture.md#generated-artifacts)、
  [规划边界](../../architecture.md#emission-planning)、[REV-25..28](../../changelog.md#remediation-validation--rev-25-28)。
- 纯 C 后端：[历史 CGEN 要求](../../changelog.md#cgen-tasks)；若由夜间 runner 调用，另读
  [夜间契约](../../usage-guide.md#c-nightly)。

## 必须保留的设计

- compiler 流水线禁止链接运行时；运行时禁止包含 AST/IR/前端头。
  生成产物是桥梁，单头模式的桥梁在生成头中。
- C++ 业务失败使用 Result/Diagnostics，只有 std::bad_alloc 向外传播。
  检查数字转换、文件系统、线程启动及写文件的失败路径；不要机械添加 noexcept。
  C11 使用状态码与输出参数，按逐 API 契约处理失败状态、初始化和释放。
- UPER/APER 共用 Variant 参数化核心；保留 DER/BER、COER/OER、CXER/XER 策略关系。
  多入口同时生成时，各入口必须使用自己的策略，不让全局掩码覆盖同族行为。
- 保留自研 BigInt、零第三方运行时依赖、C++17、纯 C11 和三种生成产物形态。
- 12 是产品入口口径；per 是组合别名。JERI 共享 X.697 JER 线路语法，
  不称为标准 canonical JSON；未实现的 encoding instruction 必须明确诊断。

## 按改动选择回归

### 内存、线路和错误传播

分配失败必须逐层传播，检查容量加乘溢出和资源上限；遵守各 API 的输出提交、
游标及生命周期契约，不假定所有失败都会回滚游标或所有 out 都自动清空。
测试覆盖失败后释放/复用及截断输入。测试 hook 不进入生产库。

PER 改动同时验证两种 Variant，覆盖非零起始位、整数符号/补码、长度阈值、
片段与末尾零 determinant。使用适用的 asn1c/asn1tools 独立向量与交叉解码；
遇到外部对照工具能力差异，记录版本、限制及标准仲裁依据。自身往返不能证明线路合规。

生成代码的解析失败应传播到调用方；必填缺失与非法值分别处理，数值完整消费并
核查目标范围。Result 的访问前置条件和源码/线路位置单位遵循错误契约。

### 缓存、并发和生成入口

- 缓存键保留生成器内容指纹、格式版本、完整输出选项、依赖内容和输入顺序。
  无 source_paths 的手工 IR 与不可读输入禁用缓存。对比缓存开/关及 miss/hit
  产物字节；修改 emitter 后覆盖相关 inline/out-of-line/split。
- 指纹集成回归使用独立、未同时构建的仅编译器目录：
  `python3 scripts/validate_generator_revision.py --build build-compiler`。
  此脚本会临时修改并恢复构建目录的私有生成头，不与该目录其他构建并发。
- 并发保持有界、按模块序合并诊断；0/1 串行，lower/tag 保持串行。
  验证峰值与确定性，不能仅比较运行时间。
- 编码规则改动核查 CLI、位掩码、include、运行时依赖与函数名；比较单入口和组合
  的实际编码结果。用 CodecEntry 测试覆盖真实链接/调用，保留三种产物验证。

### 目录产物、分层与后端规划

目录发射使用 stage/manifest；一个目录只拥有一套当前产物，同目录并发拒绝。
切换形态/basename 只清理 hash 未变的旧归属；保留用户文件和已修改的过时文件。
不要 glob 所有历史 `.cpp` 当成本次输出，按 manifest 注册产物。
强杀遗留 `.asn1cxx-emitting` 时先确认无活动写入者并核查 backup，再恢复/重跑，
不能自动删除仍被其他生成进程使用的目录。

生成相关回归包含三形态双 TU 链接/调用、模块 SCC、名称/Windows 路径及失败归属保护；
单 TU 或语法检查不能证明 ODR。保持 `Architecture.Layering` 和 `Architecture.PublicHeaders`，
新增模块/目标或 include 根时同步两种分层门禁，不将 emitter 打印的 include 当实际依赖。
独立验证 compiler-only/runtime-only；组件开关不能只在全量构建里测试。

规划只依赖 IR，借用计划不能比描述符活得久，IR 在消费期间不可修改。
C 类型后端已消费同一规划；后续编码规则继续复用，不复制 presence/标签/约束/扩展/selector 判定。
计划模型和 C++ 接入通过不等于 C emitter 通过，真实生成 C PDU 验收留给对应子任务。

### 标准语义与独立验证

先按能力矩阵区分 parse、语义/IR、生成、运行时和外部互操作，C/C++ 分开。
基线版本、勘误是否核对、未实现指令均明确登记；不要由类型枚举或一个往返推定全标准支持。

约束不能保真进入 IR 时应诊断并阻止生成，不能只 warning 后当 unconstrained。
静态常量解析沿本模块 import 和值别名查找，检测循环；禁止从无关模块挑选同名值。
参数化值约束必须替换实参并保留调用模块来源，实例键区分值；不能只用 V0/V1
位置标签缓存不同范围。用至少两个实例的实际 PER 位宽和跨模块嵌套用例回归。
生成的 native INTEGER 在 cast 前检查宿主范围与非扩展根区间成员，跨二进制/XML/JSON
家族复用同一生成检查；不要将超界值截断为有效字段。BigInteger 的完整约束验证另行核查。
检查负 SIZE、空集合及引用类型新增约束；FROM、COMPONENTS OF、数字版本组等当前
Unsupported 的结构，只有在补语义和线路证据后才开放探针，不能直接改预期让门禁通过。

规范策略采用成对测试：基础规则的合法替代在 BER/OER 可接受，规范规则应拒绝对应非法表示。
结构级 DEFAULT/重复成员/顺序检查不能由原语策略替代；CER 片段检查计入 unused-bits
内容字节，验证非末尾片长、嵌套 EOC、最短头、递归和累计内容上限。

REAL 改动遵守已公开支持范围。binary64 容量不等于 ASN.1 REAL 完整值域；
验证 mantissa 有效精度、范围、次正规和负零，禁止静默窄化或读取 JER bigint 的缓存整数。
XML/JSON 文本仍是 host-double 语义，精确十进制模型另立设计，不误称当前已实现。

新增外部向量保存工具版本/commit、schema hash、值、参数和字节。
工具副本没有独立 Git commit 时记为未知并记录包源码 hash，不能引用父工作区 commit 冒充。
使用 `scripts/validate_standard_oracles.py` 复跑 live 或冻结向量；只有显式刷新才写期望值。
XML 树/JSON 值比较与规范字节比较分开，CXER 空元素拼写等标准化转换必须记录，
不能将 XER/OER 外部对照工具冒充独立 CXER/COER 规范编码器。

使用 `DecoderFuzz.SeedRegression` 重放截断/变异；新发现的 crash 应最小化并固定为回归。
可选 Clang `ASN1_BUILD_FUZZERS=ON` 配合 ASan+UBSan 做有限探索，记录 seed、次数、
输入上限及实际覆盖入口；无崩溃不等于安全性证明。

### 构建与验收口径

编译选项限定到 target 和适用编译器；C 目标声明 C11/REQUIRED/EXTENSIONS OFF。
维持最低 CMake 兼容、固定 GTest 下载 hash 和离线路径。
MSVC ASan 不冒充 ASan+UBSan。交叉构建区分宿主生成器和目标库，避免在宿主执行目标程序。

影响运行时或生成语义时，按 [开发纪律](../../development.md) 完成针对性回归、
Release 与 ASan+UBSan 全量门禁，以及 7 测试样例 ×12 实际入口语法矩阵：
`cmake --build build --target asn1_codec_matrix`。
语法通过不等于链接、运行或互操作通过，分别记录证据。

安装改动运行 `python3 scripts/validate_installed_package.py --build build --output build/package-check`，
使用新的输出目录，覆盖纯 C、运行时、compiler 和宿主生成消费者；
私有 GTest 不进入 SDK，纯 C 消费者不引入 C++ 标准库。
具体离线/交叉命令与支持范围查阅安装文档，不从历史通过记录推断本次已验证。

大型测试样例构建根据资源控制并行度；中断/未跑的门禁不记为通过。
只改文档时检查表述与链接，无需重复全量编译。

## 夜间 runner 调用

由 `nightly_c_codegen.py` 调用时只执行指定 CGEN 批次，父 runner 负责 commit；
模型不得自行提交、push、切换分支或重置工作区。补真实 `CGenBatchN.*` 阶段验收，
同步契约与证据；不改 TODO 验收正文、受保护 runner/门禁或提前勾选后续任务。
runner 的自动门禁不替代每批独立外部对照工具、生成 C PDU 或安装验收。
其他交互任务是否提交/推送仍以用户明确授权为准，本节不提供额外 Git 授权。

## 同步收尾

每个 REV 记录实际命令、结果、环境及未验证范围；缺少必需验证时保留“验证待补”。
更新对应契约/使用/架构文档、TODO、status 与 changelog。
历史验收和分析快照保留原数字；只同步当前状态数字。

每批完成时检查本 skill：把已验证且可复用的流程变化同步到对应段落，详细证据
链接到验收文档，不复制整个任务清单。按当前验收区分原语、类型产物和编码规则；
不能把 C11 冒烟测试或类型-only 通过写成完整 C PDU 编解码已完成。

当前 TODO 完成后，将实现、验证及原任务记录转入 changelog，并从 TODO 移除或缩减已完成范围；不要在 TODO 累积已完成批次、历史估时和 Codex 执行日志。构建目录保持可见，最终发布版本只在 release 时单独整理。

该文件是仓库内的 skill 源文件；置于 docs 不代表已安装到 Codex 的技能发现目录。


## C11 后端专项回归

### 调度与类型生命周期

不支持的可达类型、DEFAULT 或选项组合在写流及目录/缓存 I/O 前诊断；旧 CppGenerator 流接口不能接收 C 目标。不把 Unsupported 改成成功的空头/源。语言、前缀必须进入缓存身份；保留字段特定 IR 克隆和可逆命名。

构建 `asn1_cgen_batch2_lifecycle`、`asn1_emit_cache_tests`、`asn1_cgen2_cpp_{outline,inline,split}_tests`，按改动运行 `CGenBatch1`、`CGenBatch2`、`EmissionPlanning`。C11 独立链接、C++17 包含、命名碰撞、重复定义负例和原 7×12 矩阵不能省略。分配 hook 仅位于测试包装。

init/free 不分配，只恢复标量 DEFAULT；拥有型字面量通过事务性 `reset_defaults` 构造。必需嵌套默认值递归还原，OPTIONAL/未激活扩展保持可释放空状态。逐分配点失败必须保持原输出，不支持的结构/列表和未解析非枚举 DEFAULT 明确拒绝。

字段计划共同决定有效标签、存在位、版本组和选择器；字面量规范化服务两种语言。超过 64 位的选择键保持有符号十进制，类型 ID 仅在当前模式有效。实际生成 C 运行描述符、查找及组一致性；规划单测不能替代 C 后端消费。SIZE 分别以位、字节、元素计量，不把字符串存储字节当字符数；double REAL、uint64 OID 弧、int64 枚举不是完整标准域。

### 产物、分片与安装

显式仅类型请求使用 `--types-only` 且不带编码规则；普通编码规则仅开放已验收值域。三形态/模块内分片复用同一发布事务，禁止重复实现绕过保护。单头所有公有/私有函数为 `static inline`；跨源深度辅助函数在内部头带前缀外部声明，不假定 static 符号能跨翻译单元访问。

运行 `CGenBatch3` 的双 C 翻译单元编译/链接/调用、每片故障注入、严格 C++17 包含、单模块预算切换、整份 LTE RRC 类型构建、缓存损坏/命中/失效及纯 C CMake 清单消费。原语编码投影不能称为生成 PDU 互操作。新增函数同步实际定义数和链接检查；C++17 包含时枚举首成员须正确类型初始化。

语言/形态/预算切换，手改旧文件、未归属清单、符号链接、锁及模型/manifest 失败均保持发布保护。JSON/CMake 只登记本次源，不 glob 历史文件；清单不是 depfile。非插桩程序检查无 C++ 运行时依赖；GCC UBSan 自身引入 libstdc++ 不等于产品混入 C++，不能为依赖检查关闭 Sanitizer。

### PER 与 TLV

UPER/APER 使用同一 `asn1_per_variant_t` 核心，验证 normally-small、枚举序号、uint64 弧 OID、非字节起点和长度/内容对齐。>=64 的 normally-small 大形式按 X.691 10.6～10.9 的半约束非负整数和对齐规则处理。OID 使用 BER 内容及最短 base-128，检查首弧、溢出、截断和输出保持；保留 C/C++ 一致性及独立冻结向量。

递归集合、扩展添加、版本组、CHOICE 扩展、尾根和开放内容均需生成 C PDU 测试。版本组占一个外层扩展位，内部保留 SEQUENCE 前导位图；未知 SEQUENCE 扩展可跳过，未知 CHOICE 序号明确 Unsupported。已知表约束解码同时保留原始完整编码并拥有具体类型；copy/free、重编码及控制键匹配逐项验证，子读取器共享顶层预算。

保留长度与元素交错分片、16K 精确边界末尾零 determinant、累计字节/元素预算和集合两次扩容故障注入。更广字符分片、完整 REAL/字符域和全部嵌套失败组合仍按状态页登记，不把局部完成写成全 PER 合规。验证坏长度/索引、截断、深度、预算、分配失败和游标/输出契约。

BER/DER/CER 采用逐入口策略，不用组合掩码选一套全局规范行为。显式包装无标签 CHOICE 时保留实际分支标签；CER SET 使用静态最小可能标签，DER 使用所选标签。受限字符构造分片的子标签为 OCTET STRING；UTF8 SIZE 按字符计数，均需显式向量，避免编码器和解码器共同出错。

TLV 分配路径改动运行拥有型 DEFAULT、递归集合和 CER 的逐分配点测试，核对 writer/输出/游标提交边界，复跑 21 消费者入口/形态矩阵及整份 RRC 生成、编译、PDU 与现场 asn1c 对验。切片或仅类型构建不满足整份 PDU 验收；代表 PDU 不证明每个 PDU。CER 条文向量与外部外部对照工具分别记账。

### OER、JSON/XML 与类型开放字段

保持共享 C 存储元数据边界和逐入口策略，复跑 39 个单/组合入口三形态消费者。JSON INTEGER 保留精确十进制 token，ARRAY 根（含尾根）排在扩展前；OBJECT 检查抽象键重复；UNWRAPPED 必须唯一可解，nullable optional ARRAY 图在发布前拒绝。

NAME/TEXT 共享映射作用于 JER/E-XER，基本 XER/CXER 忽略该映射；枚举别名 TEXT 需传到 OBJECT 键，变换后的字段/枚举名须唯一。XML 控制元素需要混合内容位置，不能仅靠 UTF-8 字节偏移恢复顺序。OF 使用实际元素类型包装，BOOLEAN/ENUMERATED 集合保留直接空元素值形式，并用独立外部向量验证。

CXER 按 X.693 9.5/9.6 显式输出 DEFAULT，与 DER/COER 不同；规范 REAL 指数拼写及严格解码拒绝非规范等价值。OER SET 最小标签排除 CHOICE 扩展分支，嵌套无标签 CHOICE 按 X.696 20.1 重复选择标签。约束 REAL 消费 IR 的 IEEE 形式，写出前拒绝有损 binary32 转换；无约束 OER 即便基本策略也校验 DER REAL 内容。

运行解析/词法、指令、规范边界、每分配点和缩小预算测试，再做全量双构建及原 C++ 84 语法门禁。72 组外部输入/规范化回放必须独立于自往返；asn1tools 0.169.0 展平扩展添加组，不能验证相关线路，也不能充当 COER/CXER 规范外部对照工具。未支持 UNTAGGED 明确拒绝，不降级为基本规则。

已知 JSON/XML 开放字段使用生成的不可变选择元数据，普通字段解码完再选型，保持 JSON 成员顺序无关；验证类型 ID 并释放部分选中值。未知键、无内部类型模式、非法内容均明确报错，不用十六进制占位。逐分配点测试、未知键/类型/内容负例、21 组独立冻结向量与 asn1c 离散根向量分别保留。

### 最终工程门禁与标准边界

`scripts/validate_c_codec_matrix.py` 与 C++ 矩阵使用同七个 fixture/十二入口，拒绝即失败，不能计为跳过或通过。GCC/Clang 严格 C11 矩阵在最后一次 emitter 修改后执行；旧二进制结果不能充当最终结果。语法、链接运行、外部外部对照工具和模糊测试各自登记。

安装消费包括 `generated-c`：宿主工具、多 C 翻译单元、模块分片及 UPER+BER。`asn1_generate_c` 配置期读取受控清单，登记模式/生成器/产物依赖；验证新增模块重配置、缺源修复，显式供应所有导入模式。用原生编译器的交叉保护探针仅证明执行策略，不证明嵌入式 ABI。

全量 CTest 前用 `asn1_test_executables` 重建全部消费者/测试，可执行门禁与大型 C/C++ 语法矩阵分开运行。生成解码模糊测试直接调用十二个生成入口，所有结果都释放活动值；种子回归与可选 Clang libFuzzer 分开登记。生成共享 stamp 及显式依赖需经过冷 stamp 并行构建验证，不能关闭发布锁。

有限根/区间 BigInt 继续共用 PER 核心。X.691 13.1 的可扩展根按 RANGE（含空洞）判定，非扩展约束仍精确检查成员；保留独立 asn1c 离散根向量，asn1tools 的 UNION 缺口不可冒充金标准。拥有型 INTEGER DEFAULT 计入 limb 分配，生成局部变量避免遮蔽。

当前生成/运行时 ABI 为 3，空私有对象集不含不可用的标量选择槽；私有描述符变更须成套重新生成/编译，生成头检查运行时 ABI，不能承诺跨 CRT。

命名位串：DER/CER/CXER 全裁尾零，PER/COER 取最短有效 SIZE，固定编码补零，解码等价值满足 SIZE；归一化前记入含几何容量的分配预算，普通位串保持长度。UTCTime 校验公历日期/偏移；PER 用规范无约束 VisibleString（X.691 10.6.5），OER 同样忽略时间 SIZE（X.696 8.4.4）并保留长度，OER/COER 都测试约束时间。GeneralizedTime 等未支持种类单独登记。全测试样例语法通过不是全 3GPP 线路认证。


## 电信 PDU 与 NR 独立验收

协议修改复跑 `Protocol.{lpp,s1ap,rrc}.{c,cpp}`，并在固定测试环境启用 `ASN1_PROTOCOL_ORACLE_PYTHON` 执行现场独立编译。原七样例十二入口、全部消费者重建及 Release/ASan+UBSan 不省略。检查模式原文版本和 SHA-256，保留 `.gitattributes` 的 LF 检出约定，避免平台换行改变证据；目录名不能替代文件头；`rrc_8_6_0` 实际文件头是 LTE V8.12.0。

独立工具从完整同源模式编译，不用项目 IR、生成物或手裁切片充当独立来源。固定目录/向量包含值、编码参数、工具版本和源码/生成物 hash，现场先复跑消费者避免过期报告。冻结变化必须明确评审；测试工具仅为开发依赖。构造的顶层 PDU 不能称现场抓包，代表消息不能推定全部协议。

S1AP 开放内容从项目内层编码结果逐层构造，禁止直接转发 oracle octets；同时检查独立字节、项目解码抽象值、外部解码项目输出和错误正常返回。pycrate 覆盖表约束，asn1tools 不承担 S1AP 表约束，当前 asn1c 无 APER；BER/非网络 PER 变体单独作为库能力登记。测试中规范 XER 值传递不等于项目 CXER 外部验证。

动态 CHOICE/open type 的自动标签保留完整内层 TLV，命名 OF 隐式字段只能替换外层包装。C++ 原始未知模式 BER 开放内容目前只支持单个定长 TLV；不定长和无法校验模式的 DER/CER 明确拒绝，不通过放松检查伪装完成。

NR 以 `tests/telecom/nr_38331_acceptance.json` 固定的 V15.3.0 和 NR-0～NR-6 为基线。验收定义完成与全文实际执行分开；尚无全文/hash 时不得宣布 NR 支持，不以 LTE、NR 精选切片或其他 Release 代替。已完成 P1-3 记录在 changelog，当前 TODO 只留 NR-01 等尚未执行工作。
