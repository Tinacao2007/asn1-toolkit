# 架构设计

本文档是 asn1-toolkit 的唯一设计权威。项目当前状态见 [status.md](status.md)，
开发历史见 [changelog.md](changelog.md)，测试与门禁操作见 [development.md](development.md)。

## 阅读导航

[架构决策](#核心架构决策) · [产品编码入口](#codec-entries) · [生成产物与发布](#generated-artifacts) · [共享发射规划](#emission-planning) · [C++ 错误契约](#cpp-error-contracts) · [C11 错误/所有权](#c-runtime-contracts) · [C11 ABI 与生命周期](#c-abi)

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
3. **生成产物是唯一桥梁（inline 模式为生成 `.hpp`）** —— 它调用运行时原语，自身不包含编解码算法。

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
  CHARACTER STRING / REAL / >64 位 INTEGER 存在实现路径，具体语法/IR/生成/runtime
  和验证边界见 [标准能力登记](status.md#standard-capabilities)；REAL、字符与时间的
  限定支持范围见 [专页](status.md#real-and-string-profile)（各 Phase 落地时间见
  [changelog.md](changelog.md)）。`OCTET STRING` / `BIT STRING (CONTAINING Type)`
  [ENCODED BY …] 降为带 `containing` 类型 id 的 IR，外层编解码按八位组/位处理。
- **C++17 only。** 用 `std::optional` / `std::variant` / `std::string_view` /
  `std::filesystem`；不用 `std::expected`、不用 `std::span`（自备 `Result<T>`
  与 `Span<T>`）。解码与约束失败返回 `Result`，不抛异常（`std::bad_alloc` 除外）。
- **无 Boost、无 multiprecision。** 非约束 INTEGER 用内部 sign-plus-limbs 类型
  （`asn1::BigInteger`）；约束整数在代码生成时按范围降到 8/16/32/64 位 C++ 类型。

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
  tests/             测试可执行与生成链接门禁 + 7 个 fixture + 金标准向量
  examples/          示例 .asn
  scripts/           版权头维护、金标准向量生成
  docs/              项目文档（入口见 docs/README.md）
```

`asn1_codegen` 的发射器按编码规则家族拆为 7 个源文件：`emit_internal.hpp`
（src-local 共享声明 + `Emitter` 类接口）、`emit_core.cpp`（驱动/命名/类型声明/
共享辅助 / split emit）、`emit_per.cpp`、`emit_tlv.cpp`（BER/DER/CER）、
`emit_xer.cpp`（XER/CXER/E-XER）、`emit_jer.cpp`、`emit_oer.cpp`（OER/COER）。

## CMake 目标

C++17，`CMAKE_CXX_STANDARD_REQUIRED ON`，扩展关闭；CMake 3.16+。每个目标有
`asn1::<name>` 别名。

### 编译器链（静态库，不链接运行时）

| 目标                       | 职责                             |
| ------------------------ | ------------------------------ |
| `asn1_common`（INTERFACE） | `Result<T>`、`Span<T>`          |
| `asn1_support`           | 源码位置、诊断、SourceFile             |
| `asn1_ast`               | AST 节点与打印                      |
| `asn1_frontend`          | lexer + parser（含 parser_ioc）   |
| `asn1_ir`                | Type IR 与打印                    |
| `asn1_constraints`       | 约束归一化                          |
| `asn1_semantic`          | 符号表、分析器                        |
| `asn1_codegen`           | C++ 发射器（按编码规则家族拆分，含缓存和输出事务） |

### 运行时（静态库）

| 目标             | 职责                                                   |
| -------------- | ---------------------------------------------------- |
| `asn1_runtime` | ByteReader/ByteWriter、BitReader/BitWriter、BigInteger |
| `asn1_ber`     | BER TLV + 原语/构造编解码                                   |
| `asn1_der`     | DER 规范编解码（依赖 ber）                                    |
| `asn1_cer`     | CER 规范策略层（X.690，依赖 der/ber）                          |
| `asn1_per`     | PER 原语 + 类型编解码（UPER/APER 共享）                         |
| `asn1_oer`     | OER（X.696 BASIC-OER）                                 |
| `asn1_coer`    | COER 规范编解码（依赖 oer）                                   |
| `asn1_xer`     | BASIC-XER XML 树 + 类型编解码                              |
| `asn1_cxer`    | CXER 规范编解码（依赖 xer）                                   |
| `asn1_exer`    | E-XER 编码指令效果                                         |
| `asn1_jer`     | BASIC-JER JSON 树 + 类型编解码                             |
| `asn1_jeri`    | JER 编码指令效果（依赖 jer）                                   |

### 可执行与门禁目标

| 目标                                               | 说明                                                                                   |
| ------------------------------------------------ | ------------------------------------------------------------------------------------ |
| `asn1cxx`                                        | 编译器驱动                                                                                |
| `asn1_<fixture>_compile`                         | 测试样例 emit 产物静态库编译门（RRC/LPP/S1AP/tlv_oer_golden/tlv_golden/cer_golden/rrc_slice 等） |
| `asn1_split_lpp_split` / `asn1_split_s1ap_split` | `--emit-split` 分文件产物编译门（UPER）                                                        |
| 测试可执行                                            | 清单见 [development.md](development.md#测试体系)                                            |

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
  `ChoiceType` 存扁平组件表 + `ExtensionMarker` 节点，语义阶段再拆根与扩展组；
- 约束是独立节点族（`ValueRange` / `SingleValue` / `SizeConstraint` /
  `UnionConstraint` / `IntersectionConstraint` / `ExtensibleConstraint`），
  AST 类上无编解码方法；
- 值在约束边界已知前保持十进制文本，宿主整数宽度只在那时决定。

## 语义模型与 IR

`SymbolTable` 将 `Module::Name` 映射到符号；IR 类型住在 `TypeArena`，以
`TypeId`（32 位索引）寻址，递归结构指向 `TypeId`。

面向编解码器的描述符：

- `IntegerDesc`：边界、可扩展、命名数、可选 `host_bits` / `is_signed`；
- `SequenceDesc`：根、扩展组（含版本括号组标记）、trailing 根；
- `Field`：presence、tag、default；
- `ConstraintDesc`：PER 使用的折叠区间（PER 直接读描述符，不重解析 `(0..255)`）；
- `SetDesc`：同时存源序与规范标签序（lowerer 计算一次）。

## 编解码抽象

共享基础：`ByteReader` / `ByteWriter`、`BitReader` / `BitWriter`。
CLI 提供 12 个产品入口：`uper` / `aper` / `ber` / `der` / `cer` /
`oer` / `coer` / `xer` / `cxer` / `exer` / `jer` / `jeri`。`per` 额外作为
`uper,aper` 组合别名，不占独立位。JERI 是项目的 JER 指令入口，不是标准 canonical JSON；
与 JER 共用 X.697 线路语法和已有后端，见 [入口登记](architecture.md#codec-entries)。

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
对象类 `&Value` 开放类型降为字节 vector，JER 编码为 hex OCTET STRING（BASE64
指令生效）；`INSTANCE OF` 从 `[type-id, value]` 数组解码。

JERI：JER 编码指令运行时 —— BASE64、ARRAY（SEQUENCE 为 JSON 数组，缺失
optional 为 null）、NAME、OBJECT（SET OF 键值为 JSON 对象）、TEXT、UNWRAPPED
（CHOICE 无包装对象）。

### 生成类型映射

| ASN.1                | C++                                         |
| -------------------- | ------------------------------------------- |
| OPTIONAL             | `std::optional`                             |
| DEFAULT              | 普通值（位清零时解码器填默认值）                            |
| SEQUENCE OF / SET OF | `std::vector`                               |
| 字符串                  | `std::string`                               |
| OBJECT IDENTIFIER    | `asn1::ObjectIdentifier`                    |
| 非约束 INTEGER          | `asn1::BigInteger`（>64 位时）；约束整数按范围定宽        |
| 对象类 `&Value` 开放类型    | 字节 vector（表约束类型化时提供 `decoded_<field>` 视图） |

表约束类型化开放类型（table-constraint typed IE）：条目只挂在开放类型字段上
（`&criticality` 这类固定类型字段保持原类型）；解码按控制字段 id 派发到
逐条目特化类型；类型化视图 `decoded_<field>` 在编码时填充开放类型字节与
控制字段 id。

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
   单入口与组合入口的 include/链接覆盖由实际 12 入口门禁验证。

## 错误模型

- 前端与语义分析追加 `Diagnostics`，可能时恢复到下一个 assignment；
  消息一律带 `file:line:column`（token 存在时）；
- 编解码错误走 `Result`，偏移单位显式标 Bit / Byte / Unknown；
- 禁止无信息失败 —— token 或类型名已知时必须出现在消息里。

Result 访问前置条件、源码 UTF-8 字节列、线路偏移与逻辑字段路径的区别、
资源失败处理及 C++ API 迁移见 [C++ 错误契约](architecture.md#cpp-error-contracts)。

## 设计权衡

- 生成编排比单一解释式 walker 代码略多，换来大 3GPP 模块的可读与可分别编译；
- `TypeId` arena 不如指针直观，换来递归类型的直接表达与稳定 IR dump；
- 识别不支持语法有解析成本，换来真实规范不被误报；
- 内部 `BigInt` 比 Boost 多写代码，换来运行时零依赖。

## 发射规划与产物事务（REV-25..28）

IR → `codegen/planning` 的借用计划 → 各语言渲染；规划不链接运行时。
本次仅接入 C++ PER presence/扩展数与 native INTEGER 约束，C emitter 后续复用
见 [设计与阶段边界](architecture.md#emission-planning)。
目录发射在私有 stage 生成，manifest 登记输出和依赖，成功发布时清理未修改的旧归属文件。
同目录并发明确拒绝；不覆盖普通用户文件或符号链接。多文件发布与崩溃恢复的限制、
三种形态的 ODR/ABI/线程安全/所有权见 [产物与集成契约](architecture.md#generated-artifacts)。
配置阶段的 `cmake/Layering.cmake` 检查实际传递链接；源码 include 与独立公共头门禁
见 [开发指南](development.md)。

## C11 与 C++17 后端边界

统一目录调度按 `TargetLanguage` 选择后端，默认 C++；`CppGenerator` 旧流接口始终只接受 C++。C 后端位于 `asn1_codegen`，只依赖 IR/common，不包含、不链接两种运行时。`render_c_type_files` 将 IR 规划/渲染为目标文件；只有生成的 `.h/.c` 包含 C 运行时头并调用其接口，不通过 C++ 包装提供所谓纯 C 实现。

共享字段与字面量计划提供有效标签、存在位、约束、扩展组、选择器及 DEFAULT；C/C++ 使用各自的语言渲染器，不复制这些语义判定。C 元数据只有偏移、获取函数、名称和数值，不携带 AST/IR。共享计划、生命周期和实际生成 PDU 的证据分别登记，不能由规划单测推断后端完成。

C UPER/APER 采用同一 Variant 核心；BER/DER/CER 共用 TLV，OER/COER 共用原语并逐调用选择策略，XML 共用解析/值引擎并选择基本/规范/扩展策略，JER/JERI 共用精确 JSON token 与指令语义。多入口掩码不改变每个函数自己的策略。C/C++ OER SET 排序读取相同根标签计划。

两种语言复用 stage/backup/manifest 发布机制；C 单头采用 `static inline`，普通/分片通过内部头连接带前缀的外部辅助函数。SCC 不拆开，缓存键包含语言、前缀、形态、预算、全部有序源和生成器身份；缓存命中仍规划/渲染并核对字节，不宣称跳过渲染工作。

文本树和值拥有有界存储，生成解码事务提交，状态逐层传播；未支持的指令/类型图在发布前失败。共享生成使用单一 stamp 生产者及显式依赖，保留并发发布锁。安装 SDK 的 `asn1_generate_c` 在配置期运行宿主工具、读取清单并监听模式/工具/产物变化，源集合变化触发重配置；纯 C 消费者不启用 C++，交叉构建不在宿主执行目标程序。

当前生成/运行时 ABI 为 3，描述符支持命名位串归一化和已知类型选择视图；JSON/XML 已知选择值由目标对象拥有，未知类型明确返回错误。原 C++ 与 GCC/Clang C 各 84 组合矩阵及生成 PDU 验收分别记录，语法成功不等于全部 3GPP 线路认证。早期 C 调度拒绝与尚未实现的叙述仅保留在变更历史。


<a id="codec-entries"></a>

## 产品编码规则入口登记（REV-15/16）


12 是产品选择入口数，不是 12 种独立标准线路格式。`per` 只等于 `uper,aper`。
CLI、C++ EmitOptions 均提供以下入口。C11 后端的 12 入口及值域限制见
[CGEN 第 6 批](changelog.md#cgen-validation--batch-6)；C 的消费目标统一为 `asn1::c_runtime`，
下表消费目标仍记录 C++ 家族依赖。

| 入口 | 规范归属/项目含义 | 生成函数后缀 | 消费目标 | 本批能力证据 |
|---|---|---|---|---|
| ber | X.690 BER | ber | asn1::ber | 单独生成、链接、往返 |
| der | X.690 DER，BER+策略 | der | asn1::der | 同上 |
| cer | X.690 CER，BER+策略 | cer | asn1::cer | 同上；不是外部 CER 外部对照工具 |
| uper | X.691 非对齐 PER | uper | asn1::per | 同上；共享 Variant 核心 |
| aper | X.691 对齐 PER | aper | asn1::per | 同上；共享 Variant 核心 |
| oer | X.696 BASIC-OER | oer | asn1::oer | 单独/组合行为比较 |
| coer | X.696 CANONICAL-OER，OER+策略 | coer | asn1::coer | 同上 |
| xer | X.693 BASIC-XER | xer | asn1::xer | 同上 |
| cxer | X.693 CANONICAL-XER，XER+策略 | cxer | asn1::cxer | 同上 |
| exer | X.693 EXTENDED-XER | exer | asn1::exer | 同上；单选保留旧 xer API 别名 |
| jer | X.697 JER，保留既有指令感知行为 | jer | asn1::jeri | 与 jeri 的同值结果比较 |
| jeri | 项目 JER instruction 入口，非独立传输语法 | jeri | asn1::jeri | 转发共享 JER 实现，不另造 canonical JSON |

消费目标列是覆盖指令辅助调用的家族依赖，非每个最小 schema 都需要其中全部符号。
CER 的共同规范原语检查复用 DER，当前 `asn1::cer` 传递依赖
`asn1::der` → `asn1::ber`；构造形式与分片策略仍由 CER 实现。
手工指定静态库时也必须包含该依赖链，推荐使用导出的 CMake 目标。
局部标准能力及后续空白见 [标准能力矩阵](status.md#standard-capabilities)。
`asn1::jeri` 传递依赖 `asn1::jer`；生成的 JER 可调用 `asn1::jeri::*`，因此统一使用前者。
JERI 选择会生成共享 `encode_jer/decode_jer` 以及 `encode_jeri/decode_jeri` 入口，
组合同时选择 JER/JERI 时共享实现只生成一次。现有 JER 不改为“忽略指令”模式，
以免改变已有 schema 的线路行为。

标准依据：[ITU-T X.697 (02/2021) 正文](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.697-202102-I!!PDF-E&lang=e&type=items)
第 5、7.5、10、11 章及附录 B；另见 [目录](https://www.itu.int/dms_pubrec/itu-t/rec/x/T-REC-X.697-202102-I!!TOC-HTM-E.htm)
列有 7.5 encoding instructions、8–19 指令语法和 ARRAY/BASE64/NAME/OBJECT/TEXT/
UNWRAPPED，以及各类型的 JER 编码。该依据不支持“Canonical JSON Encoding Rules
(JERI)”的旧名称。这里核清命名与入口，不声称已审完全部指令作用域、限制和覆盖。

运行时已有 instruction helpers 和 IR/codegen 通路；完整 X.697 语义与每条指令的
独立符合性证据仍属于 REV-20/21/24。合法但尚未覆盖的结构不能仅凭本批冒烟测试
宣称支持。所有家族的未知标准边界同样保留后续专项核查，不以自往返代替金标准。

组合回归测试样例明确声明 XER INSTRUCTIONS，并用 ENCODING-CONTROL JER 为
SEQUENCE 设置 ARRAY，使 XER ATTRIBUTE 与 JER ARRAY 的所属家族明确。
这只验证所用子集；不据此声明所有编码引用、继承、否定指令和限制已完整支持。

当前 JER 控制段支持 `[BASE64] OCTET STRING` 和 `[ARRAY] SEQUENCE`；其余解析可接受
但尚未落实的控制段组合返回编译诊断 `unsupported JER ENCODING-CONTROL instruction/target`，
不会静默生成忽略指令的 JSON。此限制也适用于只选择其他编码规则的 schema 分析；
完整控制段作用域、继承和更丰富目标仍交由 REV-21 核查/扩展。


<a id="generated-artifacts"></a>

## 生成产物与集成契约（REV-25/28）


<a id="generated-artifacts--三种形态和链接"></a>

### 三种形态和链接

out-of-line 的每个生成 `.cpp` 只加入一个链接目标一次；缺少它会出现未定义符号，
重复编译进同一程序可能出现重复定义。inline 模式可由多个 TU 包含同一个生成头，
定义必须 inline；仍需链接所选运行时。其伴随 `.cpp` 是兼容 stub，可不编译。
split 模式编译 manifest 中所有 `.cpp`，聚合头本身不提供定义。
模块依赖环合并为一个单元，不等于允许任意递归类型以值成员无限嵌套。
同一程序不要混用不同 schema/选项生成的同名命名空间与类型。

三种形态均是 compiler 与运行时的桥梁：普通/split 的调用定义在生成 `.cpp`，
inline 的调用定义在生成 `.hpp`。编译器自身不 include/link 运行时。
双 TU 的 `GeneratedPolicy.MultipleTranslationUnitsLinkAndCallSameCodecs` 与
`SplitModules.DependencyCycleMergesUnitAndCallsBothNamespaces` 覆盖实际链接调用；
12 入口跨三形态线路比较仍由 `CodecEntry.CombinationEquality` 验证。

<a id="generated-artifacts--名称与输出集"></a>

### 名称与输出集

`EmitOptions::basename` 是单个可移植文件 stem，仅字母、数字、下划线，长度至多
120；禁止路径分隔符、点和 Windows 设备名。namespace 使用同样的可移植字符/长度
与设备名限制，并且必须是单个非保留 C++17 标识符，
不接受 `a::b`、关键字、前导下划线或双下划线；非法输入报诊断，不静默改名。
split 的模块名仍经过 C++ 标识符转换，转换碰撞沿用确定性后缀；文件名大小写碰撞、
设备名或与聚合头碰撞则拒绝，避免 Linux 生成成功而 Windows 覆盖。
文件 stem 的约束不限制目录名；API 路径测试覆盖空格与中文目录。
这不等于已经验证所有 Windows CLI argv 编码场景。

**一个目录是一套产物。** 改 basename、schema 或生成形态会替换当前产物集。
多套产物使用不同目录。生成器通过创建 `.asn1cxx-emitting/` 拒绝同目录并发写入，
不等待、不自动抢占。正常退出尝试清理私有暂存区；清理失败或进程被强杀后可能遗留目录，须先
确认没有活动写入者，再检查其中 stage/backup，恢复后删除该目录并重跑。

生成器在私有 stage 内完成生成、flush、诊断检查和输出 hash，再发布。
发布期间旧文件移入私有 backup；可恢复的 I/O 失败会回滚，回滚失败保留备份并报告路径。
最后提交 `.asn1cxx-manifest`，格式首行 `asn1cxx-artifacts-v1`，后续输出名/SHA256，
以及 `dependencies` 行记录带引号的源路径和可读取内容 hash。
它是产物与依赖清单，不是 CMake depfile；调用方仍需声明全部输入依赖。
无源路径的手工 IR 不启用内容缓存。

manifest 中过时文件只有在当前字节 hash 仍匹配时才清理；已修改的过时文件和
无关用户文件保留。当前输出名由 manifest 拥有时允许重新生成覆盖手动修改。
首次迁移可认领首行为 `// Generated by asn1cxx` 的旧产物；普通用户文件、目录和
符号链接不被覆盖。manifest 损坏时拒绝修改，不凭扩展名批量清理。
`.asn1cxx-cache/` 仅用于性能，不能取代 manifest 或线路验证。

这不是整个目录的单次原子替换，也不提供断电持久性。消费者应在生成命令成功后
读取产物；不要与发布并发读文件。强杀后应核验 manifest 中每个 hash 并重新生成，
不能仅凭 manifest 文件存在就认定完整。文件系统被其他进程恶意并发修改不在此契约内。

<a id="generated-artifacts--sdk所有权和发布边界"></a>

### SDK、所有权和发布边界

公开 C++ 头不承诺跨编译器/STL/编译选项 ABI；升级编译器、运行时或生成器时，
消费者一起重新编译。C11 公开结构布局也未冻结；status 既有数值保留，不据此承诺
所有未来布局二进制兼容。C 符号使用 `asn1_` 前缀，头通过 `<asn1/c_runtime/...>`
包含，不以裸 `limits.h` 等易冲突名称安装到 include 根。

输入 Span/reader 是借用；输入存储须覆盖读取期间，输出/BigInt 的释放及错误提交
逐 API 见 [C 契约](architecture.md#c-runtime-contracts) 和 [C++ 契约](architecture.md#cpp-error-contracts)。
不同对象可独立使用；共享可变 reader/writer、Diagnostics 或同一个发射器实例需调用方
同步。不要将“库无全局业务状态”解释为所有对象可并发变更。
C 默认资源限额见 `c_runtime/limits.h`；C++ 限额按原语/profile 区分，尚无全链路统一
解码预算。嵌入式接入须显式核对输入大小、深度和内存预算，不能宣传无限值域。

标准归属为 X.680～683（语言）、X.690（BER/DER/CER）、X.691（PER）、
X.693（XML）、X.696（OER）、X.697（JER）；12 是产品入口数量。
BigInt limb 的 little-endian 是内存表示，不能推导线路端序；PER 位序、补码内容和
各编码规则按对应原语契约处理，不通过宿主结构体 memcpy 生成线路数据。

**发布前待办**：当前仓库带 PROCODEC 专有版权头；零第三方运行时依赖不等于
项目可自由分发。需由权利人明确项目/生成代码/安装 SDK 的分发许可，补许可证文件、
第三方构建依赖 notices 与版本/ABI 政策。这是发布条件，不能由本批代码修正代为授权。

<a id="generated-artifacts--c11-产物共用事务"></a>

### C11 产物共用事务

统一目录调度按目标语言选择后端，语言与 C 前缀加入形态/缓存身份；旧 CppGenerator 流接口仍只处理 C++。C/C++ 使用同一 stage/backup/manifest，登记 `.h/.c` 和固定 `generated-sources.json`、`generated-sources.cmake`；不另建绕过保护的 C 发布实现。

语言/形态/分片变化只清理 hash 未改的旧归属文件，手改旧文件和用户文件保留且不进入新源清单。C 单头为 `static inline`；普通/分片的带前缀深度辅助函数在内部头声明。预算、SCC、模块归属、保守缓存与清单/depfile 区别见 C 输出章节；历史拒绝阶段见 CGEN 验收。


<a id="emission-planning"></a>

## 语言无关发射规划（REV-27）


<a id="emission-planning--最小设计决定"></a>

### 最小设计决定

规划层位于 `include/asn1/codegen/plan.hpp`，只依赖 IR 和 C++17 标准库。
编译器本身是 C++17；“供 C 后端复用”不要求规划接口使用 C ABI。
它不链接运行时、不包含 C/C++ 目标类型拼写、不访问读写缓冲区，未引入新的解释式运行时。

| 输入/输出 | 本次决定 | 消费位置 |
|---|---|---|
| Field / FieldPlan | effective tag、presence 位索引、表约束选择器的借用视图 | 规划单测；目标语言的 tag/selector 渲染后续迁移 |
| SequenceDesc / SequencePlan | 根后尾根的统一 presence 顺序；扩展组顺序及 singleton/version-bracket 区分 | C++ PER encode/decode 前导位图的 presence 与扩展数 |
| ConstraintDesc / ConstraintPlan | 保留归一化离散区间、PER bound kind 和根区间成员检查要求 | C++ native INTEGER 约束检查 |

计划借用输入描述符；IR 必须比计划活得久且期间不可修改。presence 不将 DEFAULT
当成 optional 宿主成员，但两者均占 PER 根位图的一个位置。扩展 bitmap 的存在
不代替 version-bracket 内层 SEQUENCE 前导位图。选择器保留控制字段与完整条目，
不将无法表示的键静默压成 int64。规划不重新执行标签推断或约束归一化。

本次只抽出已经有重复风险且可直接替换的决定，不重写所有家族 emitter。
标签/selector 的既有目标语言渲染尚未全部迁移；不能声称后端现在只剩文本打印。
`EmissionPlanning.*` 测试核查 root/trailing-root、OPTIONAL/DEFAULT、显式标签、
选择器、两类扩展组和离散约束。C++ 三形态/12 入口线路门禁用于核查替换未改变线路。

<a id="emission-planning--cc-共享规划消费"></a>

### C/C++ 共享规划消费

CGEN 初期仅增加显式拒绝的语言调度；第 2 批先接入序列/CHOICE 顺序及整数约束，再加入 `storage_size(Type)` 的字节/元素 SIZE，最终通过 `planning::fields` 提供有效标签、根/尾根索引、版本组内部索引、序号、选择器及扩展归属的统一展开视图。

C 将计划渲染成实际描述符和可执行选择查找，保留任意精度有符号十进制键；生成 copy 检查活动版本组必需成员。位 SIZE 和共享字符串/二进制字面量规范化同时服务 C 拥有型 DEFAULT 及 C++ DEFAULT。计划仍只依赖 IR/common，不链接运行时。

REV-27 的真实 C 类型/生命周期消费由生成 C 测试验证，不仅是规划单测；完整 PDU 和外部对照工具分别由后续 PER/TLV/OER/文本批次验证。不能复制标签、存在、约束、扩展组或选择器的语义决定，也无需为抽象重做前端或合并运行时。历史接入顺序和当时未完成项见验收记录，不能作为当前缺陷重复修正。


<a id="cpp-error-contracts"></a>

## C++ 错误与诊断契约


编译器与运行时共享 `asn1/common/result.hpp` 的数据定义，不互相链接。
业务失败通过 `Result<T>` 或编译阶段已有的 `Diagnostics` 返回；只有
`std::bad_alloc` 可以向外传播。不要给可能分配内存的入口机械添加 `noexcept`。

<a id="cpp-error-contracts--result-访问"></a>

### Result 访问

`Result<T>` 和 `Result<void>` 为 C++17 `[[nodiscard]]` 类型。先检查成功状态，
再调用 `value()`；失败状态才能调用 `error()`。违反前置条件是程序员错误：
Debug assert，Release 也终止进程，不抛 `bad_variant_access`。
左值/const 左值访问返回引用；右值 `value()` 支持移动所有权。

`TypeArena::get/get_value` 同样要求有效且属于当前 arena 的 ID；无效 ID
属于程序员错误。它不承担验证外部输入的职责。

<a id="cpp-error-contracts--源位置与线路位置"></a>

### 源位置与线路位置

| 信息 | 表示 | 单位与范围 |
|---|---|---|
| 编译诊断 | `Diagnostic::where` / `SourceRange` | 文件名，1-based 行和 UTF-8 **字节列**；不是 Unicode 字符列 |
| 线路错误 | `Error::offset` + `offset_unit` | 0-based Byte / Bit；基于产生错误的当前读取器，不自动换算为外层完整报文偏移 |
| 未知线路位置 | `OffsetUnit::Unknown` | 旧调用点或没有可定位的线性输入；偏移=0 不足以证明“报文起点” |
| 逻辑字段 | `Error::field_path` | 可空；本批 E-XER 属性错误记录属性名，不承诺所有编码规则已构建完整嵌套路径 |

BitReader 和 ByteReader 的直接错误分别标 Bit、Byte。`make_error` 的新增
单位参数默认 Unknown，不能根据编码规则名称猜测或将旧错误默认为字节。
后续编码规则的定位增强须在知道实际单位时逐调用点补充，不能批量误标。
运行时 Error 不包含 AST、IR 或编译器源范围。

<a id="cpp-error-contracts--编译器入口与资源失败"></a>

### 编译器入口与资源失败

- `SourceFile::from_path` 返回 `Result<SourceFile>`，打开失败和读取失败显式
  返回错误；分块读取避免 streambuf iterator 的 I/O 异常路径。
- `ir::BigInt::from_decimal` 返回 `Result<BigInt>`，空值、非数字拒绝，规范化
  负零，正确识别 int64 两端；调用者必须传播失败或记录源诊断。
- CLI `--parallel` 与 OID arc 用 `from_chars`，拒绝尾随内容和溢出。OID 错误
  保留原 token 源范围。并发上限本身仍属 REV-14。
- 输出目录与缓存目录使用 `filesystem(error_code)`；输出流 flush 后检查
  失败，失败不能记录成功缓存。缓存不可用可退化为重新生成。
- OS 线程创建 `system_error` 回退 deferred 执行，按模块顺序 get/合并，结果
  保持确定。任务内部异常不由此捕获，bad_alloc 不被吞掉。
- 标准容器容量失败 `length_error` 在 Analyzer/CppGenerator/CLI 入口转换
  为诊断；这不是业务异常模型，也不是捕获所有 `std::exception` 的兜底。

<a id="cpp-error-contracts--集成迁移"></a>

### 集成迁移

`SourceFile::from_path`、`ir::BigInt::from_decimal` 的返回类型发生源兼容变化，
必须先检查 Result。Error 增加单位与字段路径会改变 C++ 对象布局；编译器、
运行时、生成代码和消费者应一起重新编译，不承诺旧静态库二进制兼容。
C11 status/out 参数 ABI 本批未修改。

本批生成器修复验证显式使用 `--no-emit-cache`。REV-13 完成前，升级生成器后
应禁用旧 emit cache 并重新生成，避免旧产物掩盖修复。


<a id="c-runtime-contracts"></a>

## C11 运行时错误与所有权契约


适用于当前 `include/asn1/c_runtime/*.h` 和生成 C 的声明接口。C emitter 与十二入口
已完成登记支持范围；标准线路范围与验收边界见 [第 7 批记录](changelog.md#cgen-validation--batch-7)。

<a id="c-runtime-contracts--状态码"></a>

### 状态码

新增 `per_*_bigint_root_ex` 支持 int64 有限根范围及任意精度扩展值；两种 Variant
共用实现。根必须同时有上下界，构造解码的输出须为新/零/已释放对象，失败保持
输出不变，读取器游标沿用原语契约，可能已推进。生成公共 decoder 仍通过临时对象
保持目标值失败不变。该原语只表示连续根区间，不能替代不连续约束的成员检查。

原有 0..7 数值保持不变，不应把 C/C++ 错误枚举直接强制转换后假设永远对应。

| 状态 | 含义 |
|---|---|
| `ASN1_ERR_OUT_OF_MEMORY` (8) | malloc/calloc/realloc 失败 |
| `ASN1_ERR_RESOURCE_LIMIT` (9) | 已配置的字符串字节数或 BigInt limb 上限被超过 |
| `ASN1_ERR_LENGTH_OVERFLOW` (3) | 长度/容量不能表示、整数转换越界，或调用者输出缓冲区不足 |
| `ASN1_ERR_TRUNCATED` (1) | 输入缺少所需数据 |
| `ASN1_ERR_INVALID_ARGUMENT` (6) | 参数不满足接口契约 |
| `ASN1_ERR_UNSUPPORTED` (7) | 当前未支持能力 |

已有 BER length 原语对 indefinite indicator 返回 LengthOverflow 的历史行为
暂保留；这不表示 indefinite 本身是非法 BER，后续实现另行处理。

`per_*_bigint_ranges_ex` 接受排序的不连续有限根区间。扩展类型的 PER 扩展位按
X.691 13.1 的根 RANGE 判断；非扩展类型仍精确检查成员，空洞非法。命名位
normalization 与 typed 选择器分配计入累计预算。C runtime/生成 ABI 3 要求
一同重编译；primitive、生成入口的游标/输出保证仍分别按各自契约处理。

<a id="c-runtime-contracts--生命周期和输出"></a>

### 生命周期和输出

所有指针必须指向有效对象/足够大的存储；不得伪造公开结构的指针、计数、容量。
输入输出不得非法重叠。NULL 数据指针只允许搭配零长度。创建函数不读取旧输出
指针，不自动释放旧对象；不能把活对象的唯一指针直接传给 create 覆盖。

| API 组 | 所有权及初始化 | 失败后状态 |
|---|---|---|
| `byte_view` / `byte_buf` | 无验证的非拥有构造；使用者维持底层内存有效 | 无状态码；后续使用函数验证参数 |
| `bigint_init` / `bitstring_init` | 在新对象或已 free 对象上置零；不要用 init 覆盖活对象 | void；不分配 |
| `bigint_init_from_decimal` | 构造函数，目标必须是新对象、零对象或已 free 对象；复用先 free | 临时构造失败时不修改目标；若希望无条件 free，请先 init 为零 |
| `per_decode_unbounded_int` | 同 BigInt 构造契约，不覆盖活对象 | 输出不变；读取器游标可能已推进 |
| `bigint_to_int64` / `bigint_to_bytes` | const 输入；返回的字节是无符号大端幅值，不是完整 BER TLV 或有符号补码 | 输出参数不变 |
| `bitstring_resize` | 目标须已 init；保留所有权并可重复使用 | 数据、指针、长度和容量不变 |
| `per_decode_bitstring` | 目标须已 init；成功释放旧存储并接管新存储 | 输出对象不变；读取器游标可能推进 |
| `bit_writer_create` | 新建拥有缓冲区的 handle | 输出指针不变 |
| `bit_writer_put_*` / `align` | 写入器须有效；每次操作先预留所需容量 | 失败不提交有效位或数据；容量不是稳定 API |
| `bit_writer_take` | 返回独立 malloc 缓冲区并清空写入器；部分尾字节零填充 | 输出与写入器的有效位不变 |
| `bit_writer_clear` | 保留容量，清空有效数据，可重新写 0 位 | void |
| `bit_reader_create` / `byte_reader_create` | 拷贝输入，调用者之后可释放/修改原输入 | 输出指针不变 |
| `bit_reader_get_*` | 返回字节缓冲区由调用者拥有 | 输出及读位置不变；截断在分配前检查 |
| BER tag/length encode/decode | 调用者提供缓冲区或值输出 | 输出缓冲区、值及 consumed 均不提交 |
| JER/XER 写入器 init/puts | 借用调用者输出缓冲区；puts 不做 JSON/XML 转义 | 失败 used/已提交前缀不变 |
| JER 写入器 printf | 借用缓冲区；格式及参数须满足标准 printf 契约 | used/已提交前缀不变；未提交尾部可能被 vsnprintf 修改 |
| JER/XER 读取器 init | 借用输入；只初始化游标，尚非完整解析器 | 参数非法时读取器不变 |
| PER scalar 解码 | 标量输出成功后提交 | 输出值不变；读取器游标可能推进 |
| PER 组合编码 | 多次调用写入器的组合操作 | 可能已经写入前缀，不承诺整次编码回滚；失败后丢弃/clear 写入器 |

`bigint_free` / `bitstring_free` 只接受已初始化或成功构造对象（或 NULL），释放
后置零，可重复 free。不得 free 任意未初始化结构。`*_destroy(NULL)` 安全，
但同一非 NULL handle 不得 destroy 两次；销毁后调用者应自行置 NULL。

所有返回的 `uint8_t*` 缓冲区使用与运行时相同 C allocator/CRT 的 `free()`
释放；空结果也可能返回非 NULL 的可释放指针。不可跨不同 Windows CRT 配置
分配/释放。JER/XER 输入、输出缓冲区始终由调用者拥有。公开结构不可浅拷贝
后分别 free；所有权移动后必须将源置为不拥有状态。

<a id="c-runtime-contracts--失败注入门禁"></a>

### 失败注入门禁

`Asn1CRuntimeFailureContracts` 单独编译 C 源码并启用私有测试 allocator，
逐分配点注入失败、检查状态/输出/存活分配数。生产 `asn1::c_runtime` 不包含
测试状态或 allocator 配置；测试替换接口不是公共 ABI。

<a id="c-runtime-contracts--rev-0409per-原语约定"></a>

### REV-04..09：PER 原语约定

`per_core.c` 是唯一 C PER 核心，使用 `asn1_per_variant_t` 选择 UPER/APER。
`per.c` 只保留旧 ABI 的 UPER 包装。`_ex` 不是另一套实现。

- INTEGER：有界单区间、仅下界、仅上界、无界、根区间可扩展，以及完整 int64
  域；超出 int64 的无界值使用 BigInt 接口。无界内容采用最短有符号补码。
  `bigint_to_bytes` 仍是无符号幅值；新增 `to_twos_complement` 与
  `init_from_twos_complement` 明确区分表示。构造输出仍须为新/零/已 free 对象。
- 长度：新描述符区分 unconstrained、constrained、semi-constrained；fixed 是
  constrained 且 lower==upper；extensible 描述根与根外。SIZE 的一般长度编码
  使用实际长度，不减去 semi-constrained 下界；有界上界 >=64K 也走一般分片。
- `encode/decode_length_fragment` 只处理一个 determinant，显式返回/接受 more；
  调用者必须交错处理片段及数据，整片后仍须最终零 determinant。标量 size
  辅助函数不能表示分片内容，遇到一般长度 >=16K 返回 Unsupported，禁止丢失
  more 信息。生成的 OF 路径通过 collection fragment API 交错处理长度和元素。
- 旧 `encode_octetstring` 编码完整无界 UPER 值；旧 `decode_octetstring(r,length)`
  保留“读取已知长度裸内容”的 ABI。新代码应配对使用 `_ex` 完整值接口；
  不要把旧解码当作能自行消费长度及片段的完整解码器。
- BIT STRING 的长度单位是位；容量验证使用 ceil(bits/8)，尾位总是从源字节高位
  开始，scalar `put_bits` 超过 64 位返回 InvalidArgument。NamedBitList 的
  合法尾零裁剪由调用者基于类型信息决定，本原语不猜测。
- APER 处理整数范围阈值、变长字符串、固定 BIT STRING >16 位、固定
  OCTET STRING >2 octet、长度与开放类型对齐；零长度不添加不存在的内容对齐。
  内部字段完整编码和开放类型的尾部零填充仍由调用者完成；空内层值的完整编码须按规则补齐；当前开放类型 API 接受零字节内容，实际封装与填充由适用入口处理。
- decoder 默认保持 64KiB 字节上限；超过返回 ResourceLimit，不提交输出。
  整个 C 运行时可一致使用 `-DASN1_MAX_STRING_BYTES=...` 重新编译，配置必须
  为正且可表示为位数。本次单独的 128KiB 测试目标不改变生产默认值。
- 生成 C PDU 的读取器以顶层解码为预算根，累计申请字节默认 16 MiB、
  集合/扩展元素默认 65536；嵌套开放类型读取器共享预算。预算统计申请总量，
  包含 realloc 的新请求，不仅是最终存活内存。

尚未支持的字符集原语、完整生成类型编解码及复杂约束
不会因这些原语完成而自动获得支持。测试证据和外部对照工具能力边界见
[development.md](development.md#c-runtime-tests)。

<a id="c-runtime-contracts--生成-per-与累计预算"></a>

### 生成 PER 与累计预算

共享 PER 核心提供 normally-small、枚举序号、uint64 弧 OID、SEQUENCE/SET 扩展标志与存在位图。解码位图由返回对象拥有，失败保持调用方输出，使用 `asn1_per_sequence_preamble_free` 释放；编码先验证位图，后续写入器失败仍可能留下前缀。APER 大值与 OID 内容按规则对齐，UPER 保持位打包；OID 采用 X.691 23 的 BER 内容，拒绝非法根、非最短 base-128、溢出和超限。

生成 C PER 支持已登记的递归集合、扩展/版本组、开放完整编码、受限字符串和宿主 double REAL。已知表约束同时保留原始字节和拥有型具体值，生成 copy/free/重编码按选中类型管理。Numeric/Printable/Visible/IA5/BMP/UniversalString 的路径不代表任意字符种类均合规，REAL 也不是精确十进制模型。

生成顶层解码重置读取器预算，扩展/open type 的子读取器共享根预算；默认累计申请 16 MiB 和 65536 个集合/扩展元素，宏为 `ASN1_MAX_PDU_ALLOCATION_BYTES`、`ASN1_MAX_PDU_ELEMENTS`。每次 realloc 的新请求也计入总量，不只统计最终存活堆。集合分片与累计预算已有门禁，更广全标准域、嵌套故障组合和每种模式的外部向量仍单独登记。

<a id="c-runtime-contracts--tlv-策略与字节流"></a>

### TLV 策略与字节流

`byte_io.h` 提供读取/视图/剩余量/跳过及拥有型写入器。失败读取/跳过不推进，追加失败不改变逻辑内容。`tlv.h` 解析一个完整 TLV 并返回借用内容/编码视图，源 reader/输入必须比视图活得久。整数原语要求空初始化 BigInt，字节串成功转移新 malloc 缓冲区，失败输出不变。

生成 BER/DER/CER 公开解码对已初始化值和读游标一起事务提交，成功恰消费一个 TLV 并释放/替换旧值。逐调用预算计入嵌套内容、集合/间接节点，输入副本另行限额。三策略共享引擎；`asn1_ber_tag_t.number` 已由 uint32 扩为 uint64，运行时与消费者须一起重编译，不混旧对象文件。


<a id="c-abi"></a>

<a id="c-abi--experimental-type-only-stream-api-batch-2"></a>
<a id="c-abi--names-artifacts-and-versioning"></a>
<a id="c-abi--value-layouts-and-capability-boundaries"></a>
<a id="c-abi--function-and-ownership-contract"></a>
<a id="c-abi--batch-2-verification"></a>
<a id="c-abi--batch-5-tlv-abi-additions"></a>
<a id="c-abi--batch-6-oertext-schema-abi"></a>
<a id="c-abi--batch-7-abi-revision-3"></a>


## C11 生成 ABI 与生命周期

当前生成 ABI 和 `ASN1_C_RUNTIME_ABI_VERSION` 均为 **3**。生成头检查要求的运行时版本；修改布局、签名或所有权必须升版并成套重新生成、重编译。项目版本为 0.1.0，不承诺跨编译器、CRT、平台、结构体打包方式或不同 SDK 的二进制兼容。第 1 批 ABI 设计及第 4～6 批 ABI 2 属于历史，不是当前接口。


### 名称与头文件

默认前缀为 `asn1_gen`，`--c-prefix` 与 C++ 命名空间相互独立。前缀以 ASCII 字母起始，仅含字母、数字和单下划线，不以下划线结尾。公共类型使用 `<prefix>_m<escaped-module>_t<escaped-type>`，字段特定克隆使用 `_view<TypeId>`，匿名类型使用 `anon_<TypeId>`；非字母数字字节（包括下划线）转义为 `_xHH`。成员以 `f`、存在标志以 `has_f` 命名。公共符号、私有辅助函数、头保护及元数据均带前缀；独立生成集必须使用不同前缀。

重复名称、保留标识符、标准头遮蔽和 Windows 设备名在发布前诊断，不静默合并。生成头仅包含标准 C 和 C 运行时头，提供 C++ `extern "C"` 保护；生成 C 不引入模板、异常、RTTI 或 C++ 标准库。三种形态、源分片和清单规则见 [C 输出](usage-guide.md#c-output)。类型 ID 是当前模式的局部标识，不能作线路判别值或跨构建 ABI 编号；函数/描述符地址不保证跨翻译单元相同。


### 值布局及限制

| ASN.1 类型 | C 存储及语义 | 明确边界 |
|---|---|---|
| INTEGER | 可表示的归一化域使用 int64 包装，否则为拥有型 BigInt | 转换前检查范围；大整数保留精确 limb，不窄化 |
| BOOLEAN / NULL | BOOLEAN 使用 0/1 整数，NULL 保留一个占位字节 | 拒绝非法布尔值；按规则省略 NULL 线路内容 |
| ENUMERATED | int64 已知值及扩展表示 | 未知扩展序号须保留或明确拒绝，不伪造已知值 |
| 字节串/字符字符串 | 拥有型字节缓冲区及 count/capacity | 不依赖 NUL；字符域、UTF-8 与字符 SIZE 只按已登记种类验证 |
| BIT STRING | `asn1_bitstring_t`，位数与容量分离 | 验证尾部填充；命名位串按规则裁零或补零 |
| OID / RELATIVE-OID | 拥有型 uint64 弧数组 | 不支持任意精度弧；验证绝对 OID 首弧 |
| REAL | 宿主 double | 已登记 binary64/部分十进制输入，不是精确 ASN.1 REAL 全值域 |
| SEQUENCE / SET | 按声明布局的结构体 | 线路顺序、有效标签和扩展组由共享计划决定，不能按内存顺序编码 |
| OPTIONAL / DEFAULT | 显式存在标志；DEFAULT 存储抽象值 | 缺席还原默认，编码按语义相等性省略；不用指针冒充存在位 |
| CHOICE | 判别枚举及 union，0 表示未选择 | 只释放活动分支，替换通过临时值提交 |
| SEQUENCE OF / SET OF | 拥有型数组及 count/capacity | 检查数量乘法、逐项清理、规范集合排序 |
| 递归类型 | 递归边使用拥有型指针 | SCC 防止无限值嵌套；调用方不得传入环或共享所有者 |
| 开放/表约束字段 | 原始完整编码及已知类型视图 | 保留编码规则身份和有效长度，不把 PER 内容当作 BER |


### 生命周期与失败提交

`init(T*)` 不分配，接受新/已释放存储和 NULL，恢复标量默认值；不得覆盖活动所有者。拥有型 DEFAULT 初始为空且可释放，通过返回状态的 `reset_defaults(T*)` 在临时对象中构造字符串、位/字节字面量、BigInt 及必需嵌套默认值。OPTIONAL 和未激活扩展不为默认值分配。未实现的结构/列表默认值、未解析非枚举引用或不支持的 REAL/OID 字面量在生成前诊断。

`free(T*)` 释放所有已初始化成员（包括存在标志为假的成员），只访问活动 CHOICE，恢复初始化状态，可重复释放。调用方图必须有限、无环、独占；数量不得超过已初始化容量。不得浅拷贝拥有型结构，不得跨 allocator/CRT 分配释放。

`copy(const T*,T*)` 和 `reset_defaults(T*)` 成功才替换已初始化输出，支持自拷贝，失败清理临时分配。检查规范 BigInt、位填充、OID 首弧、闭合枚举、精确整数/SIZE 区间及存在/版本组一致性；递归深度上限 256。测试分配 hook 不进入生产库。

公开二进制及文本编解码入口使用对应 reader/writer 与 `asn1_status_t`；JSON/XML 以 UTF-8 字节传输。生成解码成功才提交值与游标，失败保持调用方已初始化目标；生成编码先写临时写入器，成功才追加。文本解码消费完整文档，TLV 解码消费一个完整 TLV。原语有各自失败/输出前置条件，不能从生成入口的事务性推断所有原语都回滚。OOM、截断、资源超限、约束、非规范及不支持状态必须区分；编译器内部仍使用 Result/Diagnostics，不能强转为目标状态。

逐节点分配前检查深度、元素、字节、BigInt limb 和算术溢出；累计预算涵盖扩容新申请，不能只统计最终存活堆。嵌套开放类型共享顶层解码预算。默认字符串 64 KiB、BigInt 1024 个 32 位 limb、深度 256、累计申请 16 MiB、元素 65536；适用的宏配置必须在整个运行时一致。无全局 errno、longjmp 或可变全局诊断；字段路径诊断仍有单独边界。


### 共享规划与已知开放类型

`_field_count`、`_field`、`_select` 消费语言无关计划，保留有效标签、根/尾根存在序、版本组内部索引、序号、控制字段与有符号十进制键。描述符借用不可变静态存储，越界 `_field` 返回 NULL，查找失败保持输出。查找未匹配表示本次已知条目查询失败，不等于所有未知开放线路均非法。

ABI 3 增加命名位串归一化、选择视图回调和不可变选择元数据；空对象集不保留无用的标量控制字段槽。已知 PER 和 JSON/XML 表约束字段自动拥有解出的具体类型，生成 copy/free 管理其生命周期。JSON 延后选择，允许控制字段最后出现；XML 包装实际类型元素。未知选择键或独立开放值缺少内部类型模式时明确 Unsupported，不能用十六进制占位冒充类型解码。BER/OER 仍消费原始完整编码，仅有类型视图时拒绝；不同线路不能互相重解释。

大整数 DEFAULT 比较和构造保留精确负值（含负 2^128）；UTCTime 使用带长度 ASCII 存储；命名位串可解出满足 SIZE 的等价补零值。公开标签号为 uint64，不能混用旧对象文件。仓库现有专有版权声明继续有效，生成行为不授予额外许可。
