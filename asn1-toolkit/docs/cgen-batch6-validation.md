# CGEN 第 6 批：OER/COER、JSON/XML 家族

## 实现与边界

CGEN-16..19 增加 C11 的七个生成入口：`oer/coer/jer/jeri/xer/cxer/exer`。
已有 BER/DER/CER/UPER/APER 保留；`per` 仍是 UPER+APER 别名。
所有入口通过 `asn1::c_runtime` 消费，编译器仍只输出 C 源文件/元数据，
不链接 runtime。元数据只含 C 存储布局、字符串、数值和递归 getter，
不携带 AST/IR。三形态使用相同 schema，单头采用 static inline。

OER 的长度、整数、ENUMERATED、BOOLEAN、tag、open payload 在 `oer.c`；
结构和策略在 `schema_oer.c`。COER 选择独立调用策略，不读取生成时全局 mask。
支持 native/BigInt 精确整数、NULL、位/字节串、受限字符串、host-double REAL（含约束 IEEE binary32/binary64，拒绝精度损失）、
uint64 arc OID、SEQUENCE/SET/CHOICE/OF、扩展添加、版本组与 opaque open payload。
共享 `codegen/plan.hpp` 决定 OER SET 根顺序；untagged CHOICE 的最小标签只看 root，
不让 extension 标签改变根排序。嵌套无标签 CHOICE 按 X.696 20.1 重复选择标签。
UTF8String 长度始终按编码字节传输，不把固定字符数当作固定字节数。无约束 REAL 即使在 BASIC-OER 也校验 DER contents（X.696 12.4）；
抽象十进制 REAL 返回 Unsupported，不用 host-double 改写其抽象基数。

`text_tree.c` 实现完整输入的 JSON/XML 树解析。JSON 保留十进制 token，
不经 double 中转 INTEGER；支持 UTF-8、surrogate pair、转义和字符串内 NUL，
拒绝重复键、非法数字和不完整文档。XML 支持 UTF-8、XML 1.0 声明、实体引用、
CDATA、注释、属性、空元素和行尾规范化；DTD/外部实体不支持，绝不展开。
树节点记录字符控制元素在混合内容中的位置，恢复 `a<nul/>b` 的顺序。

`schema_json.c` 实现值域和结构解码、DEFAULT/OPTIONAL、未知扩展策略，
以及 ARRAY、BASE64、NAME、OBJECT（含 ENUMERATED key）、TEXT、UNWRAPPED。
ARRAY 根组件（包括 trailing-root）在扩展组件之前；nullable optional/extension
的 ARRAY 定义提前诊断。UNWRAPPED 解码试探备选，拒绝歧义；编码也验证可唯一还原。
JER/JERI 共用 X.697 指令语义，不声明 canonical JSON。

`schema_xml.c` 实现 BASIC-XER、CXER、E-XER。CXER 使用规范空元素、
SET/SET OF 顺序、REAL 指数形式，并按 X.693 9.5/9.6 显式输出 DEFAULT。
严格解码在值验证后复编码核对规范字节。E-XER 实现 ATTRIBUTE、BASE64、NAME、TEXT、
USE-NUMBER、LIST 和受限 USE-NIL；共享 NAME 同时作用于 JER/E-XER；BASIC-XER/CXER 忽略扩展 NAME 映射。
UNTAGGED 暂不支持，生成前明确 Unsupported；不替换成 BASIC-XER。

这批完成的是上述值域，**不是整部标准认证**。继续拒绝 named BIT STRING、
CONTAINING、BMP/Universal/时间等未进入 C schema profile 的类型图。
REAL 为 host-double，enum 为 int64，OID arc 为 uint64。
JSON/XML 的自动 typed open-type 映射尚未进入 profile，生成前拒绝；
OER opaque open payload 可用，未知 CHOICE 扩展返回 Unsupported。
未知 SEQUENCE 扩展可跳过；CXER 未保留的未知扩展明确 Unsupported，避免错误的规范性结论。
E-XER 组件 NAME 限定 ASCII XML 名，非法/带 namespace 的映射在发布前拒绝。
XML 的 namespace/DTD/任意指令组合、完整 REAL 十进制模型与全部字符集仍不支持。

## 错误与资源契约

生成入口使用 `asn1_byte_writer_t*` / `asn1_byte_reader_t*`，文本也传 UTF-8 字节。
decode 输出必须先初始化；成功替换旧值并提交 cursor，失败输出/cursor 不变。
encode 在临时 writer 中完成，成功才追加，失败不留下半个 PDU。
原语仍遵守逐 API 契约；BigInt 解码目标需为空初始化对象。
OOM、资源限制、截断、约束、非规范与 Unsupported 保留不同状态。
文本解析 API 另外提供字节错误位置；生成入口目前不提供完整字段路径。

使用既有 limits：64 KiB 字符串、1024 BigInt limbs、深度 256、
65536 节点/元素、16 MiB 累计分配。树与值的累计预算一起核算，
OER 整数按实际内容长度核算，不能按剩余 PDU 长度对每个 scalar 重复收费。
OER shadow reader 的输入副本纳入预算，OER/XML 集合排序暂存字节也设累计上限。
故障注入使用私有 allocator seam，生产库使用 libc；无第三方运行时依赖。

## 可复现门禁

- `CGenBatch6.TextSyntax`：词法、Unicode、实体、非法输入和逐字节截断。
- `CGenBatch6.Generated*`：七入口 PDU 实际线路、三形态、截断回滚、值域失败。
- `CGenBatch6.jerInstructions/exerInstructions`：实际 C 消费者，包括组合掩码下指令隔离。
- `CGenBatch6.StandardBoundaries`：版本组、扩展 CHOICE、SET 根最小标签、
  嵌套 CHOICE、CXER REAL/控制字符、非规范拒绝、uint64 tag 与整数溢出。
- `CGenBatch6.AllocationFailures`：逐次失败所有 encode/decode 分配，检查泄漏和事务性。
- `CGenBatch6.ResourceLimits`：缩小预算验证累计元素、树和字符串上限。
- `CGenBatch6.UnsupportedInstructions`：不支持的图在发布前失败，保留目录原文件。
- `CGenBatch6.EntryShapeMatrix`：39 个纯 C11 消费者，12 单入口及全组合，三形态；
  比较每一条实际线路并执行 decode，不以 syntax-only 代替运行。

独立验证脚本 `tests/codegen/cgen_batch6_oracle.py` 使用测试环境的 asn1tools，
不属于产品依赖。运行时传 `--driver <asn1_cgen_batch6_oracle_driver>`、
`--root <repo>`、`--evidence <json>`。asn1tools 0.169.0 已执行 72 组
OER/JER/XER 双向交换，包括 >128 位正负整数、Unicode、BIT/OCTET STRING、
SET OF、CHOICE、DEFAULT、OPTIONAL、REAL、OID、结构/字符串集合，
以及 BOOLEAN/ENUMERATED OF 的直接 XML 元素形式。
该版本会展开 extension addition groups，OER 位图与 JSON/XML 缺席组不能当作 oracle；
该工具也不能为含无标签 CHOICE 的 SET 排序；相关声明在独立 oracle 中隔离。
这些例外写入 evidence 的 oracle_gaps，由标准冻结向量单独覆盖。
72 组独立输入/规范化结果保存在 `tests/standards/cgen6-asn1tools-vectors.json`，
`CGenBatch6.FrozenOracleReplay` 无需安装 oracle 即可回放两种输入。
asn1tools OER/XER 不能算独立 COER/CXER 规范 oracle。
已有 asn1c 0.9.29 RRC UPER/BER 验收继续回归，不混计为新增家族验证。

最终门禁：MSVC Release **472/472**、Ubuntu GCC 13.3 ASan+UBSan **506/506**、
原 C++ 7 fixture×12 入口独立语法门禁 **84/84**。全部测试可执行消费者已重新构建；
C 7×12 最终语法矩阵仍归第 7 批，不与本批 39 个运行消费者混计。
公共头：49 个 C++17 头及 25 个 C 头（各 C11/C++17）独立严格编译通过；
分层检查与仓库 skill 校验通过。
Clang 在当前 WSL 未安装，不将其登记为通过；最终多编译器/安装门禁仍属于第 7 批。
非 Sanitizer 的 runtime-only 构建也完成 39 消费者矩阵，`ldd` 只显示 libc、libm 和系统 loader，
不依赖 libstdc++。

## 标准依据

- [ITU-T X.696 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.696-202102-I!!PDF-E&lang=e&type=items)：8、16–20、27、31。
- [ITU-T X.697 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.697-202102-I!!PDF-E&lang=e&type=items)：14、17、24、27、31。
- [ITU-T X.693 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.693-202102-I!!PDF-E&lang=e&type=items)：8、9、E-XER 指令章节。

## 生成依赖

本批 CMake 消费者共用单一生成目标；输出以 stamp 标记成功，头/源为 BYPRODUCTS，
消费者显式依赖生成目标。cache hit 不更新源码时间戳也不会导致多个目标重入同一生成目录。
并发写保护继续生效。第 6 批十个 C 消费目标在冷 stamp 的并行构建中单独复验，
其他历史 fixture 的共享生成依赖不据此宣称已完成第 7 批工程收口。
