# 标准能力与独立验证登记

本表区分语法、语义/IR、生成、runtime 和外部验证。范围是已列代码及测试，
不代表对整部标准的认证。C++ 与已声明 profile 的 C11 是生成后端；C11 runtime 原语不等于完整 C codec。
本次命令和结果见 [第五批验收](rev20-24-validation.md)，入口映射见
[codec-entries.md](codec-entries.md)。

## 标准基线

采用 02/2021 版作为此次核查基线。下列链接为 ITU 原始版本/版本目录；
没有完成逐条勘误差异审计，不能把当前目录页状态当成已落实所有勘误。

| 标准 | 核查内容 | 版本及勘误口径 |
|---|---|---|
| [X.680](https://www.itu.int/rec/T-REC-X.680/en) | 基本类型、标签、组件及扩展组 | 2021；其他版本/勘误差异待逐条核对 |
| [X.681](https://www.itu.int/rec/T-REC-X.681-202102-I/en) | CLASS、对象、对象集 | 2021；非全语法覆盖 |
| [X.682](https://www.itu.int/rec/T-REC-X.682/en) | 约束和组件关系 | 2021；实现子集见下表 |
| [X.683](https://www.itu.int/rec/T-REC-X.683/en) | 参数化 | 2021；实例化路径有测试，非所有形式参数类别 |
| [X.690](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.690-202102-I!!PDF-E&lang=e&type=items) | BER/DER/CER | 2021；第 8～11 章局部核查与负例 |
| [X.691](https://www.itu.int/rec/T-REC-X.691/en) | UPER/APER | 2021；既有独立 PER 向量和 C 原语差分 |
| [X.693](https://www.itu.int/rec/T-REC-X.693/en) | XER/CXER/E-XER | 2021；不承诺全部指令或所有规范词法 |
| [X.696](https://www.itu.int/rec/T-REC-X.696/en) | OER/COER | 2021；原语及结构子集 |
| [X.697](https://www.itu.int/rec/T-REC-X.697-202102-I/en) | JER 和 encoding instructions | 2021；JERI 为项目入口，非另一种 canonical JSON 标准 |

## C++ 前端到生成代码

“有路径”表示列举的结构能进入该层；额外约束、指令和组合不能据此推定可用。
`scripts/validate_standard_capabilities.py` 生成最小 schema 并检查实际接受/拒绝，
其通过仅证明 parse/analyze/emit，没有将生成成功冒充互操作。

| 结构 | 语法/分析/IR | C++ 生成/runtime 证据 | 限制或缺口 |
|---|---|---|---|
| CLASS、对象、对象集、WITH SYNTAX | `parser_ioc.cpp`、`ast/ioc.hpp`、Analyzer 对象解析 | `Semantic.DefinedSyntaxObject`、`NestedParameterizedObjectSetInstantiation`、`ioc_jer_roundtrip_test.cpp` | 对象/对象集代数全覆盖待核查 |
| 表约束、组件关系 | `TableConstraintInfo`、`resolve_table_constraints` | `Semantic.TableConstraintSpecialization`、`table_constraint_ie_roundtrip_test.cpp` | 主要覆盖由 governor 选择的 open type；固定字段约束不等于全表成员验证 |
| 参数化类型/对象集/整数值实参 | Referenced + SubstEnv + 模板实例化；值来源模块和实参入键 | `Semantic.ParameterizedTypeInstantiation`、`NestedParameterizedObjectSetInstantiation`、`NestedValueParametersKeepCallerModule`；三形态 PER 位宽回归 | 内建/匿名类型实参及约束的多实例键、完整参数类别/递归实例化仍需专项验证 |
| 无编号 `[[ ... ]]` | VersionAdditionGroup 与 IR 扩展组 | `Parser.VersionBracketsInSequence`、`Semantic.SequenceOfSizeAndVersionBrackets`、既有 PER oracle fixture | `[[ 1: ... ]]` 当前 Unsupported（parser 报错），已加入拒绝探针 |
| INSTANCE OF、EXTERNAL、EMBEDDED PDV、CHARACTER STRING | AST 节点/内建关联结构展开 | `Semantic.ExternalEmbeddedPdvCharacterStringExpand`、`InstanceOfAndExerInstructionsOnIr`，BER/DER 原语测试 | 不由 parse/IR 测试推定每个 codec 的全部关联类型语义 |
| >64 bit INTEGER | IR BigInt，生成 BigInteger | `bigint_codec_test.cpp`、PER/BER 独立向量，C 原语独立双向验证 | 超大有界描述符及所有宿主类型选择仍需按 codec 检查 |
| INTEGER/SIZE 并、交、扩展根 | 区间集合、envelope、extensible | `constraints_test.cpp`、PER primitives/golden；native INTEGER 解码前检查 | 未解析静态值、负 SIZE、空交集现在报错；BigInteger 及所有 encoder 的完整约束检查另行核查 |
| 引用类型上的额外约束 | 能解析，当前没有保真的 IR 合并 | Unsupported，新增语义/CLI 探针 | 原先可能丢约束；现在拒绝。后续实现继承约束交集时再开放 |
| FROM permitted alphabet | 当前 parser 不支持该约束形式 | Unsupported，拒绝探针 | 不能据此承诺受限字母表 PER 位宽 |
| 普通 SEQUENCE/SET/CHOICE 的 WITH COMPONENTS | AST 可解析，IR 没有约束表示 | Unsupported，新增语义拒绝 | REAL 的已识别 IEEE profile 另见专页 |
| COMPONENTS OF | 当前 parser 不支持组件展开 | Unsupported，拒绝探针 | 另立实现项，不能算普通 SEQUENCE 已覆盖 |
| CONTAINING / ENCODED BY | 保存 containing 类型 id | `Semantic.OctetStringContaining` | 外层仍按 bit/octet string 处理，不自动执行内层编码规则或内容验证 |
| REAL、字符和时间 | double/StringKind 子集 | [准确 profile](real-and-string-profile.md) | 不等于精确 ASN.1 REAL、完整字符 repertoire 或现代时间类型 |

## 12 入口与 oracle

所有入口有生成/链接调用证据，详见第四批记录及本批三种形态测试。
“外部验证”只覆盖具体 schema/value；这里不以某个工具支持该规则推定项目已全覆盖。

| 入口 | C++ runtime/生成 | 独立证据 | 当前需要继续核查的边界 |
|---|---|---|---|
| BER | TLV primitives、结构 emitter | 历史 TLV 向量；本批 asn1tools Record 双向 | constructed string 合法替代表示未全面支持；嵌套 indefinite helper 返回 Unsupported |
| DER | BER + 规范策略 | 历史 asn1c DER 向量；本批 Record 双向、DEFAULT/REAL 负例 | 时间词法、named BIT STRING 尾零、未知扩展策略待完善 |
| CER | BER 家族 + 规范策略、indefinite 与分片 | 条文推导负例/边界；本批输出由外部 BER decoder 解码 | 本次工具无独立 CER 编码 oracle；SET schema 顺序检查仍有边界 |
| UPER | 共用 PER core/Unaligned | 历史 asn1c 向量；本批 Record + C 原语差分 | 不能推定所有约束/字符集/信息对象结构已对验 |
| APER | 同 core/Aligned | 同上；非零起始位 C 原语对验 | oracle 的零长度对齐等已知偏差见第二批记录 |
| OER | OER 原语/结构 | 历史 asn1tools 向量、本批 Record 双向 | REAL profile 与未知扩展/完整类型覆盖受限 |
| COER | OER + 规范策略 | 本批规范长度负例；Record 与外部 OER 互操作 | 无独立完整 COER oracle；OER 的相同字节不是全规则认证 |
| XER | XML parser/原语/结构 | 本批 asn1tools XML 树比较和双向 | XML 空白/空元素拼写允许不同；不是 XML 字节比较 |
| CXER | XER + 规范策略/专用 writer | 本批外部 XER 解码、规范空元素拼写后的输入 | 无独立 CXER 规范编码器；REAL/时间规范词法待补 |
| E-XER | XER + 指令子集 | 本批无指令 Record 外部双向；属性错误见第三批 | 命名空间、全部指令的独立 oracle 空白 |
| JER | JSON parser/原语/结构 | 本批 asn1tools JSON 值比较和双向 | 精确 REAL 模型、全部指令未覆盖 |
| JERI | 共享 JER 指令入口 | 无指令 Record 通过 JER oracle；ARRAY 本地条文测试 | 不是独立传输语法，不将共享 JSON 对验算成独立标准 oracle |

## C11 的独立口径

C11 没有 AST/IR/codegen 依赖。逐声明的调用/解码/负例以
[C runtime 覆盖登记](c-runtime-test-coverage.md) 为权威；
`--lang c` 已有类型/生命周期、PER/TLV 声明 profile 的生成后端；
C 独立消费者和 oracle 证据不可用 C++ 测试替代。

- C PER：整数/长度/bit/octet/open type 的共用 Variant 核心，具有独立双向差分；
  不推定完整 C 字符集、normally-small 和结构生成能力。
- C BER/DER/CER：值/结构与规范策略见 CGEN 第 5 批证据；
  C OER/COER 等其他原语仍须按声明登记查证，不能以 C++ 测试代替。
- C XER/JER reader init：绑定输入不等于完整 XML/JSON parser 或结构解码器。

架构可扩展与当前能力分开：IR 可增加约束/语义规划，后端可新增 C 表达；
扩展仍须保持 compiler/runtime 分层和零第三方运行时依赖。延期项保留在
[功能 TODO](todo.md)；本批发现的后续子项列于 [第五批验收](rev20-24-validation.md)。

CGEN batch 5: C BER/DER/CER uses one policy engine. Whole RRC UPER+BER has
1037 generated types / 9 sources and a real RRCConnectionRequest/asn1c 0.9.29
wire/decode gate. C11/C++17 single/combined output forms have 21 consumers.
Supported-value limits, CER oracle distinction, LPP time and S1AP PER/typed-open
blockers are authoritative in [batch 5 validation](cgen-batch5-validation.md).
Do not infer support for other C codec families from the twelve C++ entries.
