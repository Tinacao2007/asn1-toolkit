# 架构设计

本文档是 asn1-toolkit 的唯一设计权威。项目当前状态见 [status.md](status.md)，
开发历史见 [changelog.md](changelog.md)，测试与门禁操作见 [development.md](development.md)。

## 设计目标

1. **编译产物必须是可编译、可链接的 C++17** —— emit 成功不等于产物可编译，
   这是贯穿全项目的验证纪律（见 [development.md](development.md#验证门禁纪律)）。
2. **电信协议栈可用** —— RRC/LPP/S1AP 级别的 ASN.1 大模块（信息对象类、
   表约束、参数化类型）必须能解析并生成可编译代码。
3. **零第三方运行时依赖** —— 交叉编译到工具链受限的目标平台无障碍。

## 编译流水线

```text
ASN.1 源码
  → Lexer → Parser → AST
  → 语义分析（符号表、标签、约束，共四趟）
  → Type IR（TypeArena，TypeId 寻址）
  → C++ 代码生成器
  → 生成值类型 + 薄编排（encode/decode 调用）
  → 运行时原语库（BER / DER / CER / PER / OER / COER / XER / CXER / E-XER / JER / JERI）
```

## 分层铁律

以下三条是硬性架构约束，任何"为了性能"的例外都不被接受：

1. **编译器库从不 include 编解码算法**（`asn1cxx` / `asn1_codegen` 不链接任何运行时库）；
2. **运行时从不 include AST / 符号表 / IR 头**（`src/runtime/**` 对前端头零依赖）；
3. **生成的 `.cpp` 是唯一桥梁** —— 它调用运行时原语，自身不包含编解码算法。

反面教材：父工作区的 `pasn1/tools/act`（解析器 `parser.h` 与 BER/UPER 生成器
`ber-gen-enc.h` / `uper-gen-enc.h` 混在同一编译器内），本项目的严格分离即针对此模式确立。

## 核心架构决策

- **手写 lexer 与递归下降解析器。** ASN.1 以首字母大小写区分 `TypeReference`
  与 `identifier`，类型/值赋值的区分是词法层面的；手写解析器让源码位置与诊断
  完全可控，语法按阶段渐进扩展。明确弃用 Flex/Bison 与 ANTLR。
- **AST 是语法，IR 是语义。** Parser 不解析 IMPORTS、不计算自动标签、不折叠
  `INTEGER (0..255)`；编解码器永不遍历 AST，只读取语义阶段降低后的 IR 描述符。
- **编解码算法在运行时只存在一份。** 代码生成器只产出调用共享原语的可读编排
  （如 `asn1::per::encode_constrained_whole_number`），不为每个类型生成私有实现。
  运行时可被手写代码直接使用，不必经过 asn1cxx。
- **UPER 与 APER 共享同一 PER 实现。** `per::Variant::{Unaligned,Aligned}` 是
  参数；对齐只发生在 X.691 要求的原语内部，APER 不是"UPER + 末尾填充"。
- **DER = BER + 规范策略**（最短定长、禁止构造字符串、规范 SET 序）；
  **COER = OER + 规范策略**；**CXER = XER + 规范策略**；E-XER / JERI 是指令
  效果层。策略类规则只写一次。
- **不支持的语法"识别并明确拒绝"。** 真实规范中出现的、本阶段不支持的构造
  必须被识别后给出明确诊断，而不是误报为语法错误。
- **电信语法全量落地。** CLASS / 对象与对象集 / `Class.&field` / 表约束与
  组件关系约束 / 参数化类型 / `IMPORTS Name{}` / WITH SYNTAX 定义语法 /
  版本括号 `[[...]]` / INSTANCE OF / EXTERNAL / EMBEDDED PDV /
  CHARACTER STRING / REAL / >64 位 INTEGER 均已实现（各 Phase 落地时间见
  [changelog.md](changelog.md)）。`OCTET STRING` / `BIT STRING (CONTAINING Type)`
  [ENCODED BY …] 降为带 `containing` 类型 id 的 IR，外层编解码按八位组/位处理。
- **C++17 only。** 用 `std::optional` / `std::variant` / `std::string_view` /
  `std::filesystem`；不用 `std::expected`、不用 `std::span`（自备 `Result<T>`
  与 `Span<T>`）。解码与约束失败返回 `Result`，不抛异常（`std::bad_alloc` 除外）。
- **无 Boost、无 multiprecision。** 非约束 INTEGER 用内部 sign-plus-limbs 类型
  （`asn1::BigInteger`）；约束整数在 codegen 时按范围降到 8/16/32/64 位 C++ 类型。

## 目录布局

```text
asn1-toolkit/
  CMakeLists.txt
  README.md
  include/asn1/
    common/       result.hpp, span.hpp, float_conv.hpp（Result / Span / REAL 转换）
    support/      SourceLocation, Diagnostics, SourceFile
    ast/          AST 节点族 + printer + visitor + ioc.hpp + encoding.hpp
    frontend/     Token, Lexer, Parser, parser_ioc
    ir/           Type IR（TypeArena, TypeId）+ printer
    semantic/     SymbolTable, Analyzer
    constraints/  约束归一化
    codegen/      emit.hpp（CppGenerator / EmitOptions / CodecKind）
    runtime/
      基础:       byte_io, bit_io, bigint, bit_string, aper.hpp
      ber/ der/ cer/ per/ oer/ coer/ xer/ cxer/ exer/ jer/ jeri/
  src/               与 include 镜像的实现；ast/ir 下不出现编解码代码
  tools/asn1cxx/     编译器驱动 main.cpp
  tests/             31 个测试可执行 + 7 个 fixture + 金标准向量
  examples/          示例 .asn
  scripts/           版权头维护、金标准向量生成
  docs/              项目文档（入口见 docs/README.md）
```

`asn1_codegen` 的发射器按 codec 家族拆为 7 个源文件：`emit_internal.hpp`
（src-local 共享声明 + `Emitter` 类接口）、`emit_core.cpp`（驱动/命名/类型声明/
共享辅助 / split emit）、`emit_per.cpp`、`emit_tlv.cpp`（BER/DER/CER）、
`emit_xer.cpp`（XER/CXER/E-XER）、`emit_jer.cpp`、`emit_oer.cpp`（OER/COER）。

## CMake 目标

C++17，`CMAKE_CXX_STANDARD_REQUIRED ON`，扩展关闭；CMake 3.16+。每个目标有
`asn1::<name>` 别名。

### 编译器链（静态库，不链接运行时）

| 目标 | 职责 |
|------|------|
| `asn1_common`（INTERFACE） | `Result<T>`、`Span<T>` |
| `asn1_support` | 源码位置、诊断、SourceFile |
| `asn1_ast` | AST 节点与打印 |
| `asn1_frontend` | lexer + parser（含 parser_ioc） |
| `asn1_ir` | Type IR 与打印 |
| `asn1_constraints` | 约束归一化 |
| `asn1_semantic` | 符号表、分析器 |
| `asn1_codegen` | C++ 发射器（按 codec 家族拆分的 7 个源文件，见上） |

### 运行时（静态库）

| 目标 | 职责 |
|------|------|
| `asn1_runtime` | ByteReader/ByteWriter、BitReader/BitWriter、BigInteger |
| `asn1_ber` | BER TLV + 原语/构造编解码 |
| `asn1_der` | DER 规范编解码（依赖 ber） |
| `asn1_cer` | CER 规范策略层（X.690，依赖 ber） |
| `asn1_per` | PER 原语 + 类型编解码（UPER/APER 共享） |
| `asn1_oer` | OER（X.696 BASIC-OER） |
| `asn1_coer` | COER 规范编解码（依赖 oer） |
| `asn1_xer` | BASIC-XER XML 树 + 类型编解码 |
| `asn1_cxer` | CXER 规范编解码（依赖 xer） |
| `asn1_exer` | E-XER 编码指令效果 |
| `asn1_jer` | BASIC-JER JSON 树 + 类型编解码 |
| `asn1_jeri` | JER 编码指令效果（依赖 jer） |

### 可执行与门禁目标

| 目标 | 说明 |
|------|------|
| `asn1cxx` | 编译器驱动 |
| `asn1_<fixture>_compile` | fixture emit 产物静态库编译门（RRC/LPP/S1AP/tlv_oer_golden/tlv_golden/cer_golden/rrc_slice 等） |
| `asn1_split_lpp_split` / `asn1_split_s1ap_split` | `--emit-split` 分文件产物编译门（UPER） |
| 测试可执行（31 个） | 清单见 [development.md](development.md#测试体系) |

链接规则：`asn1cxx` 只链编译链；`asn1_codegen` 不链运行时（只打印
`asn1::per::...` 调用）；生成的 TU 与编解码测试才链 `asn1_ber` / `asn1_der` /
`asn1_per` 等；`asn1_uper` / `asn1_aper` 不是独立库，是 `asn1_per` 中以
`Variant::Unaligned` / `Variant::Aligned` 进入的入口。

## 编译器接口

```cpp
struct SourceLocation { std::string file; int line; int column; };
struct SourceRange { SourceLocation begin, end; };

enum class Severity { Note, Warning, Error };
struct Diagnostic { Severity severity; SourceRange where; std::string message; };

class Diagnostics {
 public:
  void error(SourceRange, std::string message);
  bool ok() const;
  const std::vector<Diagnostic>& items() const;
};

class SourceFile;  // 拥有缓冲区；lexer 的 string_view 指向它

class Lexer {
 public:
  explicit Lexer(const SourceFile&, Diagnostics&);
  Token next();
};

class Parser {
 public:
  explicit Parser(Lexer&, Diagnostics&);
  std::unique_ptr<ast::Module> parse_module();
};

class Analyzer {
 public:
  explicit Analyzer(Diagnostics&);
  ir::Model analyze(std::vector<std::unique_ptr<ast::Module>> modules);
};

class CppGenerator {
 public:
  void emit(const ir::Model&, const EmitOptions&, Diagnostics&);
};
```

语义分析是四趟显式流程：

1. 声明符号并诊断重复；
2. 解析 IMPORTS 与类型/值引用（递归 SEQUENCE 合法；经值表达式或约束边界的环是错误）；
3. 赋予有效标签（显式/隐式/自动），标签落在 IR 组件上，编解码器不重算；
4. 归一化约束，然后降低到 IR。

## AST

多态节点、`std::unique_ptr` 所有权、每个节点带 `SourceRange`、单一 `Visitor`。

- `Module`：名称、`TagDefault`（Explicit/Implicit/Automatic）、
  `EXTENSIBILITY IMPLIED`、imports/exports、assignments；
- 赋值分 `TypeAssignment` 与 `ValueAssignment`；
- `Type` 基类带可选 `Tag` 与 `Constraint`；`SequenceType` / `SetType` /
  `ChoiceType` 存扁平组件表 + `ExtensionMarker` 节点，语义阶段再拆 root 与扩展组；
- 约束是独立节点族（`ValueRange` / `SingleValue` / `SizeConstraint` /
  `UnionConstraint` / `IntersectionConstraint` / `ExtensibleConstraint`），
  AST 类上无编解码方法；
- 值在约束边界已知前保持十进制文本，宿主整数宽度只在那时决定。

## 语义模型与 IR

`SymbolTable` 将 `Module::Name` 映射到符号；IR 类型住在 `TypeArena`，以
`TypeId`（32 位索引）寻址，递归结构指向 `TypeId`。

面向编解码器的描述符：

- `IntegerDesc`：边界、可扩展、命名数、可选 `host_bits` / `is_signed`；
- `SequenceDesc`：root、扩展组（含版本括号组标记）、trailing root；
- `Field`：presence、tag、default；
- `ConstraintDesc`：PER 使用的折叠区间（PER 直接读描述符，不重解析 `(0..255)`）；
- `SetDesc`：同时存源序与规范标签序（lowerer 计算一次）。

## 编解码抽象

共享基础：`ByteReader` / `ByteWriter`、`BitReader` / `BitWriter`。
CLI 层可选 12 个 codec 别名：`uper` / `aper` / `per` / `ber` / `der` / `cer` /
`oer` / `coer` / `xer` / `cxer` / `exer` / `jer`（`jeri` 无 CLI 别名，是指令层）。

### TLV 家族（BER / DER / CER）

BER：TLV 标签/长度/值，短与长定长、高 tag number 形式、原语与构造值；不定长
可被 BER 解码，在 DER 中是硬错误。

DER：同一写路径 + 最小长度与规范 SET 序；解码时拒绝非规范输入。

CER（X.690，BER 的策略层姊妹）：9.1 构造值一律不定长包装；9.2 + 8.6.4 超
1000 内容八位组的字符串分片（BIT STRING 分片各自携带初始未用位八位组）；
11.2 末八位组未用位清零；9.3 SET/SET OF 规范序（SET 按标签序，未打标签
CHOICE 按替代项最小标签静态定序；SET OF 按编码八位组串升序）；拒绝规范形式
之外的不定长/定长混用。

### PER 家族（UPER / APER）

约束/半约束/非约束 whole number、normally small non-negative whole number、
长度 determinant、choice index、optional 与扩展 bitmap、开放类型。
`align_to_octet()` 仅在 X.691 要求的 APER 分支内调用。

### OER 家族（OER / COER）

OER（X.696 BASIC-OER）：长度 determinant；范围落在 1/2/4/8 八位组的整数用定宽，
否则长度 + 二进制补码内容；BOOLEAN 编码 `0x00`/`0xFF`（解码接受任意非零）；
SEQUENCE presence bitmap 补齐到八位组；扩展 addition 序列（长度 + 未用位 +
presence bitmap + 开放类型）；CHOICE 上下文标签（扩展替代为标签 + 开放类型）；
定长 OCTET/BIT STRING 省略长度；SET 组件按 X.696 18.2 规范（标签）序。

COER：同一写路径 + 最短长度 determinant、最小 INTEGER/ENUMERATED 内容、TRUE
必须 `0xFF`、未用位与扩展 bitmap 填充位必须为零、SET OF 按升序；解码拒绝
非规范输入。

### XML 家族（XER / CXER / E-XER）

XER（X.693 BASIC-XER）：UTF-8 XML 元素树（`asn1::xer::Element`）；BOOLEAN 为
`<true/>`/`<false/>` 子元素；NULL 为空元素；INTEGER 为十进制文本；OCTET STRING
为 hex；BIT STRING 为二进制位；UTF8String 非 ASCII 用十进制 NCR；ENUMERATED 为
命名空子元素；OID 为点分弧；SEQUENCE/CHOICE/SEQUENCE OF 为嵌套元素。

CXER：空序言；标签间无空白；空元素写 `<name/>`；文本只转义 `&`/`<`；INTEGER
为规范 SignedNumber；OCTET STRING 为大写等长 hex；SET OF 按 CXER 字符串序；
`parse_document` 拒绝非规范 XML。

E-XER（`asn1::exer`）：编码指令在 Element 树上的效果 —— ATTRIBUTE、BASE64、
TEXT、USE-NUMBER、LIST（空格分隔的 character-encodable 项）、UNTAGGED
（并入父元素）、NAME、USE-NIL（`xsi:nil`）。

### JSON 家族（JER / JERI）

JER（X.697 BASIC-JER）：JSON 值树（`asn1::jer::Value`）；BOOLEAN 为
`true`/`false`；NULL 为 `null`；INTEGER 为 JSON number；OCTET STRING 为 hex
字符串；非约束 BIT STRING 为 `{"value","length"}`；ENUMERATED/OID/UTF8String
为 JSON 字符串；SEQUENCE 为对象；SEQUENCE OF 为数组；CHOICE 为单属性对象；
对象类 `&Value` 开放类型降为 byte vector，JER 编码为 hex OCTET STRING（BASE64
指令生效）；`INSTANCE OF` 从 `[type-id, value]` 数组解码。

JERI：JER 编码指令运行时 —— BASE64、ARRAY（SEQUENCE 为 JSON 数组，缺失
optional 为 null）、NAME、OBJECT（SET OF 键值为 JSON 对象）、TEXT、UNWRAPPED
（CHOICE 无包装对象）。

### 生成类型映射

| ASN.1 | C++ |
|-------|-----|
| OPTIONAL | `std::optional` |
| DEFAULT | 普通值（位清零时解码器填默认值） |
| SEQUENCE OF / SET OF | `std::vector` |
| 字符串 | `std::string` |
| OBJECT IDENTIFIER | `asn1::ObjectIdentifier` |
| 非约束 INTEGER | `asn1::BigInteger`（>64 位时）；约束整数按范围定宽 |
| 对象类 `&Value` 开放类型 | byte vector（表约束类型化时提供 `decoded_<field>` 视图） |

表约束类型化开放类型（table-constraint typed IE）：条目只挂在开放类型字段上
（`&criticality` 这类固定类型字段保持原类型）；解码按 governor id 派发到
逐条目特化类型；类型化视图 `decoded_<field>` 在编码时填充开放类型字节与
governor id。

## 生成产物形态

`asn1cxx` 的 emit 有三种形态（CLI 细节见 [usage-guide.md](usage-guide.md)）：

1. **默认 out-of-line（flat）** —— `generated.hpp` 只含类型声明与编解码函数
   声明，定义全部落在 `generated.cpp`（一个 TU 付一次，其余 TU 只解析声明）。
   RRC 全模块实测：头 33,697 → 5,998 行，消费 TU 编译 ~22s → ~5s；
2. **`--emit-inline`** —— 函数定义 inline 进头文件（单 TU 小模块零链接步骤）；
3. **`--emit-split`** —— 按 ASN.1 模块边界生成 `<Module>.hpp/.cpp` 对 + 聚合
   头：类型命名空间与模块名对齐，跨模块引用按 `<Module>::Name` 限定
   （`Emitter::named_ref`，flat 模式为 no-op）；互相递归的模块（模块依赖图
   SCC）合并进同一 emit 单元，按类型依赖序重开各自模块命名空间；每模块头
   自带 `detail::Null`（聚合头全量 include 时避免重复定义）；模块依赖 include
   按结构依赖（字段/元素/替代/表约束条目/别名目标/OCF fixed_type）计算。
   S1AP（6 模块，含 IEs/Containers/PDU-Contents 三模块递归环）是主要验证对象。
   已知限制：单 codec 选择 XML 家族（`--codec xer` 等单独）会暴露既有
   include 缺口，见 [status.md](status.md#剩余工作)。

## 错误模型

- 前端与语义分析追加 `Diagnostics`，可能时恢复到下一个 assignment；
  消息一律带 `file:line:column`（token 存在时）；
- 编解码错误走 `Result`，带 bit 或 byte 偏移；
- 禁止无信息失败 —— token 或类型名已知时必须出现在消息里。

## 设计权衡

- 生成编排比单一解释式 walker 代码略多，换来大 3GPP 模块的可读与可分别编译；
- `TypeId` arena 不如指针直观，换来递归类型的直接表达与稳定 IR dump；
- 识别不支持语法有解析成本，换来真实规范不被误报；
- 内部 `BigInt` 比 Boost 多写代码，换来运行时零依赖。
