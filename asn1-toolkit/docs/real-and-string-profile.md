# REAL、字符与时间的当前 profile

这是接口能力边界，不是完整标准符合性声明。标准/层次及 oracle 对照见
[标准能力登记](standard-capabilities.md)，实际验证见 [第五批验收](rev20-24-validation.md)。

## REAL

C++ 生成类型为 host `double`。BER 二进制编码依赖 IEEE-754 binary64 布局，
不表示任意精度 ASN.1 REAL；精确的 mantissa/base/exponent 模型另立后续设计项，
继续保持自研数值实现和零第三方运行时依赖。
当前 double 只保留可表示的数学数值，不保留输入的 base、scale 和十进制词法来源；
依赖这些属性的约束、或要求按原 base 重编码的业务，应视为此模型不支持。

| 路径 | 当前接受/产生范围 | 失败及精度约定 |
|---|---|---|
| BER encode | binary64 的有限值、±INF、NaN、正零/负零 | 负零产生 `43` 特殊内容，正零为空；NaN payload 不保留 |
| BER binary decode | base 2，1～3 字节 exponent；精确可表示的 binary64 值 | 超过 53 位有效 mantissa、溢出/下溢或次正规舍入返回 Unsupported；base 8/16、长 exponent 返回 Unsupported；零 mantissa 非法 |
| BER decimal decode | NR1/2/3 的局部精确子集，uint64 significand、十进制 exponent ±400 内 | 十进制分母的 5 因子无法约去、精度/范围不合适时明确失败；例如 1.E2、1.5 可接受，0.1 不按精确十进制 REAL 接受；更宽合法形式可能 Unsupported |
| DER/CER REAL decode | 上述范围再检查 X.690 11.3 规范表示 | binary 最短 mantissa/exponent、奇 mantissa、零 scale；decimal NR3 及最短词法。有限值不按 BER 合法替代形式放行 |
| UPER/APER REAL | 复用 BER REAL 内容与统一 Variant | 解码受到同一二进制/十进制 profile 限制 |
| OER/COER unconstrained | BER REAL 内容 + OER 长度 | 当前 OER/COER REAL 规范性仍需进一步逐条核对，不能用 DER 的检查结果替代 |
| OER/COER binary32 | 识别到相应 REAL WITH COMPONENTS profile 后采用固定 4 字节 | encode 拒绝溢出和有限值舍入，不再将 1.1 静默变成 float；1.5 可用 |
| OER/COER binary64 | 固定 8 字节 double | 宿主 binary64；不承诺保留 NaN payload/signaling 状态 |
| XML/JSON REAL | 当前文本映射到 host double；特殊值依家族映射 | 文本小数仍采用 binary64 转换语义，可能舍入；不是精确十进制 REAL。JER 整数不得使用溢出/截断的 number 字段；超出精确整数 profile 返回 Unsupported |

XML/JSON 的精确十进制输入与可逆文本输出需要连同数值模型共同设计；
不能在不改模型和 writer 的情况下，仅拒绝某些输入就声称获得精确 REAL。
接口使用者要求精确十进制语义时，应将此路径视为不支持并等待后续模型，
不要以当前 double 接口处理该业务。相关子项见第五批验收的 REV-23b。

REAL WITH COMPONENTS 只识别既有 IEEE32/64 profile；其他无法保真的约束现在报错，
不再默认为 unconstrained。已识别 profile 主要用于 OER 形式选择，
不代表所有 codec 都执行完整 X.682 REAL 值约束检查。

## 字符与时间

`ir::StringKind` 含 UTF8String、IA5String、PrintableString、VisibleString、NumericString、
TeletexString、VideotexString、GraphicString、GeneralString、BMPString、UniversalString、
UTCTime、GeneralizedTime。枚举存在说明模型可区分类型，不是 repertoire、长度单位、
每种规则的线路形式及独立互操作都已验证。

- UTF8/IA5 等常用路径有 runtime 和 fixture 测试；FROM permitted alphabet 当前 parser
  拒绝，不能承诺其 PER alphabet compaction。
- BMP/Universal 及传统多字符集类型需要继续核查字符宽度、非法码点、注册字符集切换和
  SIZE 的字符/字节计数。没有外部证据的组合在能力登记中保持待核查。
- UTCTime/GeneralizedTime 当前有 StringKind、标签和字符串生成路径，未完成日期有效性、
  时区、秒/小数及 DER/CER/CXER 的完整规范词法校验；使用者不能据此依赖规范时间验证。
- OID-IRI、RELATIVE-OID-IRI、TIME、DATE、TIME-OF-DAY、DATE-TIME、DURATION 当前没有
  对应 IR/生成路径；最小 schema 在编译期报错，已纳入拒绝探针，不替换成普通字符串。
- E-XER/JER 指令覆盖按 [入口登记](codec-entries.md) 查证；未支持指令应报错，
  不能由前端读入一个指令推定 writer/reader 已实现其语义。

需要 repertoire 与规范词法保证时，先为具体类型/规则补独立正反例，再开放相应承诺。

## C11 CGEN7 UTCTime and named bits

The earlier unfinished-time statement is superseded for C11 UTCTime only.
C11 validates Gregorian days/leap years, hh/mm/ss and +/-hhmm offsets; canonical
rules emit YYMMDDhhmmssZ. The declared century window is 1950–2049; an offset
conversion outside that window is rejected. PER uses unconstrained VisibleString
and canonical UTC lexical form (X.691 10.6.5), not generic octets or visible SIZE.
OER also ignores UTCTime SIZE constraints (X.696 8.4.4) and retains the length
determinant; both OER and COER have a SIZE(13) regression case.
BER/BASIC-OER/BASIC-XER/JSON can preserve valid offset/minute precision spellings.
GeneralizedTime and the other unsupported character/time profiles are unchanged.

Named-bit encoders select the shortest valid effective SIZE for PER and COER,
trim all trailing zeros for DER/CER/CXER, and pad fixed-size values where required.
Decoders return an equivalent value satisfying SIZE, not the original padding.
Unknown set bits are legal even when not in the NamedBitList (X.680 22.6).
