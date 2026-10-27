# 项目状态

当前状态的唯一权威。状态截至 **2026-10-27**。历史变更见
[changelog.md](changelog.md)。已排期 / 未排期待办见
[TODO 清单](todo.md)。

文档按架构、使用、开发、状态和历史分类维护；各批验收统一见变更历史，历史验证边界保留。

## 阅读导航

[当前阶段](#当前阶段) · [构建与测试](#构建与测试状态) · [已知限制](#已知限制) · [标准能力](#standard-capabilities) · [数值/字符/时间](#real-and-string-profile) · [C11 编解码范围](#c-capabilities)

## 当前阶段

**Phase 49：CGEN 第 1～7 批已在登记值域和指令范围内完成。**

当前 C11 和 C++17 共用前端/IR/生成规划，分别使用目标语言后端及运行时。C 的十二入口、仅类型模式、普通/单头/模块拆分和模块内分片均可用；支持范围不能等同完整 ASN.1 标准。

第 7 批补齐 LPP 命名位串/UTCTime、S1AP 离散 PER 根及已知 JSON/XML 类型开放字段。GCC 和 Clang 严格 C11 矩阵各 **84/84**，原 C++ 矩阵 **84/84**；完整 MSVC Release **476/476**，GCC ASan+UBSan **512/512**。安装 SDK 的纯 C 多翻译单元/分片消费、独立编译器/runtime 构建、配置期源规划、生成解码模糊测试与外部冻结向量均通过。生成/C 运行时 ABI 为 **3**，必须一起重新生成和编译，见 [最终验收](changelog.md#cgen-validation--batch-7)。

C OER/COER、JER/JERI、XER/CXER/E-XER 提供字节/文本事务读写、精确 INTEGER、完整解析及结构解码、已支持指令、规范策略与资源/失败门禁；39 个单/组合入口三形态消费者及 72 组独立 asn1tools 交换/冻结回放通过。C BER/DER/CER 共用策略引擎，完整 LTE RRC（1037 类型、9 个 C 源）编译通过，代表 PDU RRCConnectionRequest 的 UPER/BER 与现场 asn1c 0.9.29 字节及独立解码一致；这不是 NR TS 38.331 全文支持或所有 LTE PDU 互操作证明。

C PER 支持登记的标量、集合/递归、扩展 CHOICE、尾根、扩展添加/版本组、开放内容与已知类型选择；长度分片、累计预算和分配故障已纳入门禁。完整字符/REAL 值域、任意模式形状的独立外部向量、所有嵌套失败/游标组合和未知 CHOICE 扩展保留仍不能由现有通过结果推定。

各批历史测试数与当时的缺工具情况见变更历史，不以旧计数描述当前门禁。所有剩余事项及优先级汇总见 [TODO](todo.md)，本页保留支持范围与限制证据。

## 构建与测试状态

| 门禁                  | 结果                                                                     |
| ------------------- | ---------------------------------------------------------------------- |
| Release 构建 CTest    | MSVC 19.44 **476/476**（第 7 批最终验收）；GCC/Clang 历史完整轮次见验收记录                |
| ASan+UBSan 完整 CTest | GCC 13.3 **512/512**；Clang 严格 C11/公共头/功能及插桩模糊测试另行登记，不冒充完整 Clang C++ 回归 |
| `-Wall -Wextra`     | 无新增警告                                                                  |

## 编译门禁状态

| 门禁                     | 结果                                                                                                                                                                                                      |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 全编码规则产物门禁              | 7 测试样例（tlv_oer_golden / tlv_golden / cer_golden / RRC 8.6.0 / LPP 14.3.0 / S1AP 14.4.0 / rrc_slice）× 12 编码规则（uper,aper,ber,der,cer,oer,coer,jer,jeri,xer,exer,cxer）`g++ -std=c++17 -fsyntax-only` 全部零错误 |
| 单编码规则 × 全家族门禁（新增）      | LPP × 12 编码规则各跑一次独立 emit + 完整编译（`asn1_lpp_single_{uper,aper,ber,der,cer,oer,coer,jer,jeri,xer,exer,cxer}_compile`）：每族单独选择时产物的运行时 header 依赖自包含。该门禁捕获单编码规则场景的 include 缺口（组合门禁会把其他分支的 header 链入而掩盖）        |
| 分文件 emit 门禁（UPER）      | `asn1_split_lpp_split`（LPP 单模块路径）、`asn1_split_s1ap_split`（S1AP 三 emit 单元，含合并的 IEs+Containers+PDU-Contents 递归环）静态库编译通过                                                                                   |
| 历史分文件 emit 手工矩阵        | LPP uper/aper/coer/ber/der/oer/jer 零错误；S1AP uper/ber/der/oer/jer 零错误；RRC uper 零错误；xer/exer/cxer 单编码规则零错误（XML 缺口已修复，见上）；flat 产物与改动前逐字节一致                                                                 |
| 三大 3GPP 测试样例 emit 产物编译 | RRC/LPP/S1AP 全量 emit 产物作为 CMake 静态库编译通过（编译的是真实定义，非 stub）                                                                                                                                                |

## 金标准差分验证状态

2026-10-20 复测说明：历史 7 测试样例 × 12 选择器语法矩阵 **84/84** 通过。
该列表中的 `per` 是 UPER|APER 组合别名，不能计为第十二个独立入口；JERI
独立生成与矩阵口径仍待 REV-15/16 修正。以上历史矩阵不等于 12 个独立入口
完整合规。既有外部对照工具来源向量随本次双构建回归通过，未重新生成外部向量。

| 向量套                                             | Oracle                             | 状态      |
| ----------------------------------------------- | ---------------------------------- | ------- |
| PER/APER（`tests/per/golden/`，54 向量）             | asn1c                              | 字节级对验通过 |
| BER/DER/OER（`tests/tlv/golden/`，16 用例 × 3 编码规则） | asn1tools + asn1c（DER 仲裁）          | 字节级对验通过 |
| CER（`cer_golden`，10 用例）                         | 无外部外部对照工具；X.690 条文推导 + DER 值等价交叉验证 | 通过      |
| 运行时优化回归                                         | 三套金标准向量零回归（bit_io 批量位写前后输出逐比特一致）   | 通过      |

<a id="剩余工作"></a>

## 已知限制

1. **emit 成功 ≠ 可编译 / 正确** —— 全部门禁定义见
   [development.md](development.md#验证门禁纪律)；
2. **组合门禁掩盖单编码规则缺口** —— 全编码规则位掩码单次发射会把其他分支的
   include 带进产物；历史 XML 家族 include 缺口曾由此暴露，当前保持 12 个实际入口逐项检查，不能仅跑组合；
3. **电信测试样例测试不等于 3GPP 完整对等** —— 当前覆盖 parse + analyze +
   emit 编译门禁 + 字节级金标准差分，协议级逐消息 PDU 对验见
   [TODO 清单](todo.md#p1-3)；
4. **CER 无外部外部对照工具** —— asn1c v0.9.29 与 asn1tools 均无 CER 实现，
   金标准由 X.690 条文推导 + DER 值等价交叉验证；
5. **许可证未定** —— 源码头已标 PROCODEC 版权，正式许可证文件发布时补齐。

<a id="standard-capabilities"></a>

## 标准能力与独立验证登记

本表区分语法、语义/IR、生成、运行时和外部验证。范围是已列代码及测试，
不代表对整部标准的认证。C++ 与已声明支持范围的 C11 是生成后端；C11 运行时原语不等于完整 C 编码规则。
本次命令和结果见 [第五批验收](changelog.md#remediation-validation--rev-20-24)，入口映射见
[architecture.md](architecture.md#codec-entries)。

<a id="standard-capabilities--标准基线"></a>

### 标准基线

采用 02/2021 版作为此次核查基线。下列链接为 ITU 原始版本/版本目录；
没有完成逐条勘误差异审计，不能把当前目录页状态当成已落实所有勘误。

| 标准                                                                                                | 核查内容                        | 版本及勘误口径                                |
| ------------------------------------------------------------------------------------------------- | --------------------------- | -------------------------------------- |
| [X.680](https://www.itu.int/rec/T-REC-X.680/en)                                                   | 基本类型、标签、组件及扩展组              | 2021；其他版本/勘误差异待逐条核对                    |
| [X.681](https://www.itu.int/rec/T-REC-X.681-202102-I/en)                                          | CLASS、对象、对象集                | 2021；非全语法覆盖                            |
| [X.682](https://www.itu.int/rec/T-REC-X.682/en)                                                   | 约束和组件关系                     | 2021；实现子集见下表                           |
| [X.683](https://www.itu.int/rec/T-REC-X.683/en)                                                   | 参数化                         | 2021；实例化路径有测试，非所有形式参数类别                |
| [X.690](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.690-202102-I!!PDF-E&lang=e&type=items) | BER/DER/CER                 | 2021；第 8～11 章局部核查与负例                   |
| [X.691](https://www.itu.int/rec/T-REC-X.691/en)                                                   | UPER/APER                   | 2021；既有独立 PER 向量和 C 原语差分               |
| [X.693](https://www.itu.int/rec/T-REC-X.693/en)                                                   | XER/CXER/E-XER              | 2021；不承诺全部指令或所有规范词法                    |
| [X.696](https://www.itu.int/rec/T-REC-X.696/en)                                                   | OER/COER                    | 2021；原语及结构子集                           |
| [X.697](https://www.itu.int/rec/T-REC-X.697-202102-I/en)                                          | JER 和 encoding instructions | 2021；JERI 为项目入口，非另一种 canonical JSON 标准 |

<a id="standard-capabilities--c-前端到生成代码"></a>

### C++ 前端到生成代码

“有路径”表示列举的结构能进入该层；额外约束、指令和组合不能据此推定可用。
`scripts/validate_standard_capabilities.py` 生成最小 schema 并检查实际接受/拒绝，
其通过仅证明 parse/analyze/emit，没有将生成成功冒充互操作。

| 结构                                                 | 语法/分析/IR                                          | C++ 生成/runtime 证据                                                                                                                          | 限制或缺口                                                    |
| -------------------------------------------------- | ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------- |
| CLASS、对象、对象集、WITH SYNTAX                           | `parser_ioc.cpp`、`ast/ioc.hpp`、Analyzer 对象解析      | `Semantic.DefinedSyntaxObject`、`NestedParameterizedObjectSetInstantiation`、`ioc_jer_roundtrip_test.cpp`                                    | 对象/对象集代数全覆盖待核查                                           |
| 表约束、组件关系                                           | `TableConstraintInfo`、`resolve_table_constraints` | `Semantic.TableConstraintSpecialization`、`table_constraint_ie_roundtrip_test.cpp`                                                          | 主要覆盖由控制字段选择的开放类型；固定字段约束不等于全表成员验证                         |
| 参数化类型/对象集/整数值实参                                    | Referenced + SubstEnv + 模板实例化；值来源模块和实参入键          | `Semantic.ParameterizedTypeInstantiation`、`NestedParameterizedObjectSetInstantiation`、`NestedValueParametersKeepCallerModule`；三形态 PER 位宽回归 | 内建/匿名类型实参及约束的多实例键、完整参数类别/递归实例化仍需专项验证                     |
| 无编号 `[[ ... ]]`                                    | VersionAdditionGroup 与 IR 扩展组                     | `Parser.VersionBracketsInSequence`、`Semantic.SequenceOfSizeAndVersionBrackets`、既有 PER 外部对照工具测试样例                                           | `[[ 1: ... ]]` 当前 Unsupported（parser 报错），已加入拒绝探针         |
| INSTANCE OF、EXTERNAL、EMBEDDED PDV、CHARACTER STRING | AST 节点/内建关联结构展开                                   | `Semantic.ExternalEmbeddedPdvCharacterStringExpand`、`InstanceOfAndExerInstructionsOnIr`，BER/DER 原语测试                                       | 不由 parse/IR 测试推定每个编码规则的全部关联类型语义                          |
| >64 位 INTEGER                                      | IR BigInt，生成 BigInteger                           | `bigint_codec_test.cpp`、PER/BER 独立向量，C 原语独立双向验证                                                                                            | 超大有界描述符及所有宿主类型选择仍需按编码规则检查                                |
| INTEGER/SIZE 并、交、扩展根                               | 区间集合、envelope、extensible                          | `constraints_test.cpp`、PER primitives/golden；native INTEGER 解码前检查                                                                          | 未解析静态值、负 SIZE、空交集现在报错；BigInteger 及所有 encoder 的完整约束检查另行核查 |
| 引用类型上的额外约束                                         | 能解析，当前没有保真的 IR 合并                                 | Unsupported，新增语义/CLI 探针                                                                                                                    | 原先可能丢约束；现在拒绝。后续实现继承约束交集时再开放                              |
| FROM permitted alphabet                            | 当前 parser 不支持该约束形式                                | Unsupported，拒绝探针                                                                                                                           | 不能据此承诺受限字母表 PER 位宽                                       |
| 普通 SEQUENCE/SET/CHOICE 的 WITH COMPONENTS           | AST 可解析，IR 没有约束表示                                 | Unsupported，新增语义拒绝                                                                                                                         | REAL 的已识别 IEEE 支持范围另见专页                                  |
| COMPONENTS OF                                      | 当前 parser 不支持组件展开                                 | Unsupported，拒绝探针                                                                                                                           | 另立实现项，不能算普通 SEQUENCE 已覆盖                                 |
| CONTAINING / ENCODED BY                            | 保存 containing 类型 id                               | `Semantic.OctetStringContaining`                                                                                                           | 外层仍按 bit/octet string 处理，不自动执行内层编码规则或内容验证                |
| REAL、字符和时间                                         | double/StringKind 子集                              | [准确支持范围](status.md#real-and-string-profile)                                                                                                | 不等于精确 ASN.1 REAL、完整字符字符域或现代时间类型                          |

<a id="standard-capabilities--12-入口与-oracle"></a>

### 12 入口与外部对照工具

所有入口有生成/链接调用证据，详见第四批记录及本批三种形态测试。
“外部验证”只覆盖具体 schema/value；这里不以某个工具支持该规则推定项目已全覆盖。

| 入口    | C++ runtime/生成               | 独立证据                                         | 当前需要继续核查的边界                                                     |
| ----- | ---------------------------- | -------------------------------------------- | --------------------------------------------------------------- |
| BER   | TLV primitives、结构 emitter    | 历史 TLV 向量；本批 asn1tools Record 双向             | constructed string 合法替代表示未全面支持；嵌套 indefinite 辅助函数返回 Unsupported |
| DER   | BER + 规范策略                   | 历史 asn1c DER 向量；本批 Record 双向、DEFAULT/REAL 负例 | 时间词法、命名 BIT STRING 尾零、未知扩展策略待完善                                 |
| CER   | BER 家族 + 规范策略、indefinite 与分片 | 条文推导负例/边界；本批输出由外部 BER decoder 解码             | 本次工具无独立 CER 编码外部对照工具；SET schema 顺序检查仍有边界                        |
| UPER  | 共用 PER core/Unaligned        | 历史 asn1c 向量；本批 Record + C 原语差分               | 不能推定所有约束/字符集/信息对象结构已对验                                          |
| APER  | 同 core/Aligned               | 同上；非零起始位 C 原语对验                              | 外部对照工具的零长度对齐等已知偏差见第二批记录                                         |
| OER   | OER 原语/结构                    | 历史 asn1tools 向量、本批 Record 双向                 | REAL 支持范围与未知扩展/完整类型覆盖受限                                         |
| COER  | OER + 规范策略                   | 本批规范长度负例；Record 与外部 OER 互操作                  | 无独立完整 COER 外部对照工具；OER 的相同字节不是全规则认证                              |
| XER   | XML parser/原语/结构             | 本批 asn1tools XML 树比较和双向                      | XML 空白/空元素拼写允许不同；不是 XML 字节比较                                    |
| CXER  | XER + 规范策略/专用写入器             | 本批外部 XER 解码、规范空元素拼写后的输入                      | 无独立 CXER 规范编码器；REAL/时间规范词法待补                                    |
| E-XER | XER + 指令子集                   | 本批无指令 Record 外部双向；属性错误见第三批                   | 命名空间、全部指令的独立外部对照工具空白                                            |
| JER   | JSON parser/原语/结构            | 本批 asn1tools JSON 值比较和双向                     | 精确 REAL 模型、全部指令未覆盖                                              |
| JERI  | 共享 JER 指令入口                  | 无指令 Record 通过 JER 外部对照工具；ARRAY 本地条文测试        | 不是独立传输语法，不将共享 JSON 对验算成独立标准外部对照工具                               |

<a id="standard-capabilities--c11-的独立口径"></a>

### C11 的独立口径

C11 没有 AST/IR/codegen 依赖。逐声明的调用/解码/负例以
[C 运行时覆盖登记](development.md#c-runtime-tests) 为权威；
`--lang c` 已有类型/生命周期、PER/TLV 声明支持范围的生成后端；
C 独立消费者和外部对照工具证据不可用 C++ 测试替代。

- C PER：整数/长度/bit/octet/open type 的共用 Variant 核心，具有独立双向差分；
  不推定完整 C 字符集、normally-small 和结构生成能力。
- C BER/DER/CER：值/结构与规范策略见 CGEN 第 5 批证据；
  C OER/COER 等其他原语仍须按声明登记查证，不能以 C++ 测试代替。
- C XER/JER 读取器 init：绑定输入不等于完整 XML/JSON parser 或结构解码器。

架构可扩展与当前能力分开：IR 可增加约束/语义规划，后端可新增 C 表达；
扩展仍须保持 compiler/runtime 分层和零第三方运行时依赖。延期项保留在
[功能 TODO](changelog.md#roadmap)；本批发现的后续子项列于 [第五批验收](changelog.md#remediation-validation--rev-20-24)。

CGEN 第 5 批实现 C BER/DER/CER 策略引擎和整份 RRC 代表 PDU 的 UPER/BER 独立对验，1037 个类型、9 个源文件及 21 个单/组合入口消费者通过。第 6 批增加 OER/文本结构编解码及 72 组外部交换/冻结回放；这些入口不能与旧读取器初始化或原始写入器混为一谈。

第 7 批补齐精确拥有型 INTEGER DEFAULT、有限根 PER BigInt 扩展、实际安装的纯 C 消费者和生成解码模糊测试。未删减的七测试样例×十二入口矩阵在 GCC/Clang C 各 **84/84**，原 C++ **84/84**。LPP 命名位串/UTCTime、S1AP 离散根及已知文本选择类型已实现；未知动态内部类型和未支持指令仍明确报错。历史阻断及各规则外部对照工具差异详见 [CGEN 验收](changelog.md#cgen-validation)。

<a id="real-and-string-profile"></a>

## REAL、字符与时间的当前支持范围

这是接口能力边界，不是完整标准符合性声明。标准/层次及外部对照工具对照见
[标准能力登记](status.md#standard-capabilities)，实际验证见 [第五批验收](changelog.md#remediation-validation--rev-20-24)。

<a id="real-and-string-profile--real"></a>

### REAL

C++ 生成类型为 host `double`。BER 二进制编码依赖 IEEE-754 binary64 布局，
不表示任意精度 ASN.1 REAL；精确的 mantissa/base/exponent 模型另立后续设计项，
继续保持自研数值实现和零第三方运行时依赖。
当前 double 只保留可表示的数学数值，不保留输入的 base、scale 和十进制词法来源；
依赖这些属性的约束、或要求按原 base 重编码的业务，应视为此模型不支持。

| 路径                     | 当前接受/产生范围                                              | 失败及精度约定                                                                                        |
| ---------------------- | ------------------------------------------------------ | ---------------------------------------------------------------------------------------------- |
| BER 编码                 | binary64 的有限值、±INF、NaN、正零/负零                           | 负零产生 `43` 特殊内容，正零为空；NaN 内容不保留                                                                  |
| BER binary 解码          | base 2，1～3 字节 exponent；精确可表示的 binary64 值               | 超过 53 位有效 mantissa、溢出/下溢或次正规舍入返回 Unsupported；base 8/16、长 exponent 返回 Unsupported；零 mantissa 非法 |
| BER decimal 解码         | NR1/2/3 的局部精确子集，uint64 significand、十进制 exponent ±400 内 | 十进制分母的 5 因子无法约去、精度/范围不合适时明确失败；例如 1.E2、1.5 可接受，0.1 不按精确十进制 REAL 接受；更宽合法形式可能 Unsupported         |
| DER/CER REAL 解码        | 上述范围再检查 X.690 11.3 规范表示                                | binary 最短 mantissa/exponent、奇 mantissa、零 scale；decimal NR3 及最短词法。有限值不按 BER 合法替代形式放行            |
| UPER/APER REAL         | 复用 BER REAL 内容与统一 Variant                              | 解码受到同一二进制/十进制支持范围限制                                                                            |
| OER/COER unconstrained | BER REAL 内容 + OER 长度                                   | 当前 OER/COER REAL 规范性仍需进一步逐条核对，不能用 DER 的检查结果替代                                                  |
| OER/COER binary32      | 识别到相应 REAL WITH COMPONENTS 支持范围后采用固定 4 字节              | 编码拒绝溢出和有限值舍入，不再将 1.1 静默变成 float；1.5 可用                                                         |
| OER/COER binary64      | 固定 8 字节 double                                         | 宿主 binary64；不承诺保留 NaN payload/signaling 状态                                                     |
| XML/JSON REAL          | 当前文本映射到 host double；特殊值依家族映射                           | 文本小数仍采用 binary64 转换语义，可能舍入；不是精确十进制 REAL。JER 整数不得使用溢出/截断的 number 字段；超出精确整数支持范围返回 Unsupported    |

XML/JSON 的精确十进制输入与可逆文本输出需要连同数值模型共同设计；
不能在不改模型和写入器的情况下，仅拒绝某些输入就声称获得精确 REAL。
接口使用者要求精确十进制语义时，应将此路径视为不支持并等待后续模型，
不要以当前 double 接口处理该业务。相关子项见第五批验收的 REV-23b。

REAL WITH COMPONENTS 只识别既有 IEEE32/64 支持范围；其他无法保真的约束现在报错，
不再默认为 unconstrained。已识别支持范围主要用于 OER 形式选择，
不代表所有编码规则都执行完整 X.682 REAL 值约束检查。

<a id="real-and-string-profile--字符与时间"></a>

### 字符与时间

`ir::StringKind` 含 UTF8String、IA5String、PrintableString、VisibleString、NumericString、
TeletexString、VideotexString、GraphicString、GeneralString、BMPString、UniversalString、
UTCTime、GeneralizedTime。枚举存在说明模型可区分类型，不是字符域、长度单位、
每种规则的线路形式及独立互操作都已验证。

- UTF8/IA5 等常用路径有运行时和测试样例测试；FROM permitted alphabet 当前 parser
  拒绝，不能承诺其 PER alphabet compaction。
- BMP/Universal 及传统多字符集类型需要继续核查字符宽度、非法码点、注册字符集切换和
  SIZE 的字符/字节计数。没有外部证据的组合在能力登记中保持待核查。
- UTCTime/GeneralizedTime 当前有 StringKind、标签和字符串生成路径，未完成日期有效性、
  时区、秒/小数及 DER/CER/CXER 的完整规范词法校验；使用者不能据此依赖规范时间验证。
- OID-IRI、RELATIVE-OID-IRI、TIME、DATE、TIME-OF-DAY、DATE-TIME、DURATION 当前没有
  对应 IR/生成路径；最小 schema 在编译期报错，已纳入拒绝探针，不替换成普通字符串。
- E-XER/JER 指令覆盖按 [入口登记](architecture.md#codec-entries) 查证；未支持指令应报错，
  不能由前端读入一个指令推定 writer/reader 已实现其语义。

需要字符域与规范词法保证时，先为具体类型/规则补独立正反例，再开放相应承诺。

<a id="real-and-string-profile--c11-时间与命名位串"></a>

### C11 时间与命名位串

此前时间未完成的说明，仅对 C11 UTCTime 已被第 7 批实现取代。校验公历日期、闰年、时分秒及 +/-hhmm 偏移，规范形式为 `YYMMDDhhmmssZ`。世纪窗口为 1950～2049；偏移转换跨出窗口拒绝。PER 使用无约束 VisibleString 和规范 UTC 词法（X.691 10.6.5），不按普通字节串或可见 SIZE 约束编码。OER 同样忽略时间 SIZE（X.696 8.4.4）并保留长度字段，OER/COER 均有 SIZE(13) 回归。BER、基本 OER、基本 XER、JSON 可保留合法偏移/分钟精度写法。GeneralizedTime 和其他未支持字符/时间种类维持明确限制。

命名位串在 PER/COER 使用最短有效 SIZE，DER/CER/CXER 移除全部尾零，固定长度按规则补零。解码返回满足 SIZE 的等价值，不保证还原原填充。NamedBitList 未列出的置位仍合法（X.680 22.6），不能一概当作未知枚举拒绝。

<a id="c-capabilities"></a>

<a id="c-capabilities--c-call-paths-used-as-the-comparison-baseline"></a>
<a id="c-capabilities--twelve-entry-register"></a>
<a id="c-capabilities--dependency-allocation-and-opening-gates"></a>
<a id="c-capabilities--cgen7-closure-of-the-telecom-generation-blockers"></a>

## C11 编解码能力范围

本节区分已实现的原语、实际生成的结构编解码、规范策略和外部互操作；仅有头文件、初始化函数或写文本辅助函数不算完整编码规则。第 1～7 批的历史缺口和完成过程见 [CGEN 验收](changelog.md#cgen-validation)，当前能力以本节和标准能力登记为准。

| 入口    | 当前实现与结构能力                                                                                     | 策略、解码与边界                                   |
| ----- | --------------------------------------------------------------------------------------------- | ------------------------------------------ |
| UPER  | 布尔、精确整数/BigInt、长度分片、位/字节串、枚举、uint64 弧 OID、已登记 REAL/字符串；递归结构、CHOICE、集合、根/尾根、扩展及版本组、已知类型/原始开放内容 | 未对齐 Variant；位偏移及累计预算；更广互操作和完整字符域仍有边界       |
| APER  | 与 UPER 共用值域和结构编排                                                                              | 同一 PER 核心的对齐 Variant；不能复制第二份算法             |
| BER   | 原生/大整数、位/字节串、已登记字符/REAL/OID/UTCTime，递归结构、显式/隐式标签、不定长/EOC、扩展及原始完整 TLV                          | 基础 TLV 策略，成功消费一个值并提交游标；不推断未知内容的规范语义        |
| DER   | 共用 BER 的值和结构                                                                                  | 最短表示、DEFAULT 省略、规范排序及时间/命名位语义；严格拒绝非规范表示    |
| CER   | 共用 TLV 引擎                                                                                     | 构造型不定长、1000 内容字节分片及规范排序；标准推导向量不冒充外部外部对照工具  |
| OER   | 精确整数、位/字节串、已登记字符/REAL/OID，结构/集合/扩展、原始开放内容                                                     | 基础 OER 策略，完整生成解码及累计预算；SET 根排序来自共享计划        |
| COER  | 与 OER 共用结构和值                                                                                  | 每入口独立规范长度、整数、布尔、数量、DEFAULT 及 SET OF 排序     |
| JER   | 精确整数 token、UTF-8、结构/数组/CHOICE/集合、已支持指令及已知类型开放字段                                               | 完整 JSON 树解析与类型解码，字节错误位置；不声称 canonical JSON |
| JERI  | 共用 JER 线路和值域                                                                                  | 项目指令入口，共享 X.697 语义，不是第十三种线路规则              |
| XER   | XML 元素/属性、已登记值/结构/集合、已知类型开放字段                                                                 | 完整文档解析及基本策略；树和值共用预算                        |
| CXER  | 与 XER 共用值引擎                                                                                   | 规范 XML、DEFAULT 显式输出、集合排序和严格复编码核对           |
| E-XER | 已支持 ATTRIBUTE、BASE64、NAME、TEXT、USE-NUMBER、LIST、受限 USE-NIL                                     | 未支持 UNTAGGED/命名空间及任意指令组合，发布前诊断，不退回基本 XER   |

`per` 是 UPER/APER 组合别名。C++ 参考路径为 CLI→统一目录发射→`CppGenerator`→各家族 emitter；C 后端同样消费 IR/公共计划，但生成不同目标语言代码。C/C++ 的 PER 各自用 Variant 共享核心；TLV、OER、XML 规范入口均采用基础机制加逐调用策略，多编码规则掩码不能覆盖每个函数自己的策略。

### 类型与资源边界

REAL 采用宿主 double，枚举已知值为 int64，OID 弧为 uint64；不能从存储宽度推导完整标准域。GeneralizedTime、未登记字符集、CONTAINING、未知类型开放文本及未实现指令必须明确拒绝。类型接受、IR 保留、代码可编译、真实编解码和外部认证分别登记。

命名位串按规则选择最短有效 SIZE（PER/COER）或移除全部尾零（DER/CER/CXER），固定长度按需补零；普通位串不裁剪。UTCTime 校验公历日期、闰年和时区偏移，规范形式为 `YYMMDDhhmmssZ`，窗口为 1950～2049，跨出窗口拒绝；PER 使用无约束 VisibleString（X.691 10.6.5），OER 同样忽略时间 SIZE 并保留长度字段（X.696 8.4.4）。

有限离散整数根必须保留区间集合。X.691 13.1 的可扩展根分支按数值 RANGE（包含空洞）选择；非扩展约束仍精确检查成员。asn1tools 0.169.0 对部分 UNION 根处理不足，用独立 asn1c 向量和条款裁决，不能裁成第一个区间。

顶层预算包括临时缓冲区、集合扩容和嵌套开放值；默认累计申请 16 MiB、65536 元素/节点、深度 256，字符串/整数还有原语限制。文本解析保留错误字节偏移，生成入口完整字段路径和任意未知扩展保留仍未全面实现。全模块 84 组合语法通过不代表所有 LPP/S1AP/NR PDU 均完成线路认证。

### 入口开放条件

每个入口须有纯 C11 编译、链接、运行，结构特定负例、每分配点失败与适用独立向量；自往返和原始写入器输出不足以开放。CGEN-04～06 验证类型/生命周期和真实共享规划消费，07～09 验证分片/发布，10～12 验证 PER，13～15 验证 TLV/RRC，16～19 验证 OER/文本，20～24 完成工程、安装和最终矩阵。原 C++ 门禁不得被 C 门禁替代。
