================================================================================
# asn1-toolkit 架构分析与优化方案

> **历史归档快照**（分析时间 2026-10-13，Phase 41+ 时点；P0-1/P0-2/P0-3/P1-1/
> P1-2/P2-1/P2-2 的实施结果已并入本文）。本文为历史记录，不作为项目现状依据：
> 各优化项的落地状态见 [../changelog.md](../changelog.md)，剩余工作见
> [../status.md](../status.md)，验证门禁纪律见 [../development.md](../development.md)。
分析时间: 2026-10-13 (Phase 41+, commit ce7d2a2)
================================================================================
本文基于本地源码实测: 模块行数、emit 产物规模、单 TU 编译耗时、
编解码器实现路径, 非设计文档转述。所有数字可复现 (见每节"实测"标注)。

--------------------------------------------------------------------------------
1. 架构总览
--------------------------------------------------------------------------------
流水线 (硬性分层, 见 docs/ARCHITECTURE.md):
  ASN.1 → Lexer → Parser → AST → Semantic(符号表/标签/约束四趟)
        → Type IR (TypeArena, TypeId 寻址) → C++ 代码生成
        → 生成值类型 + 薄编排 → 运行时原语库
        (BER/DER/PER/OER/COER/XER/CXER/E-XER/JER/JERI)

三条铁律:
  1. 编译器库从不 include 编解码算法;
  2. 运行时从不 include AST/符号表/IR 头;
  3. 生成的 .cpp 是唯一桥梁, 只调用运行时原语。

规模实测 (src + include, 83 文件):
  emit.cpp (拆分前)     5,208 行 / 103 个成员函数   (原最大单文件)
  拆分后 (2026-10-14, P0-2 完成; 行数为 2026-10-15 P2-2 后实测):
    emit_core.cpp      1,443 行 (驱动/命名/类型声明 + 共享辅助 +
                    split emit: SCC 单元/模块依赖/命名空间切换)
    emit_per.cpp       1,092 行
    emit_tlv.cpp       1,126 行
    emit_xer.cpp         959 行
    emit_jer.cpp         818 行
    emit_oer.cpp         833 行
    emit_internal.hpp    757 行 (共享声明 + Emitter 类接口, src-local)
  analyzer.cpp        2,460 行 / 45 个成员函数
  parser.cpp          2,244 行
  ber/codec.cpp       1,630 行
  per/codec.hpp       1,198 行 (header)
  ast/type.hpp        1,184 行 (header)
  其余各 500-1,200 行, 共 38,603 行 C++17
测试: 8,800+ 行 / 22 个测试域 / 320 项 CTest (tlv_oer_golden 8 项 +
  cer_golden 10 项为 2026-10-14/15 P0-3 + CER 家族新增; tlv_golden
  3 项为 2026-10-15 P1-1 新增; BitIoBenchmark 1 项为 2026-10-15
  P2-1 新增)

--------------------------------------------------------------------------------
2. 核心优势 (每项附证据)
--------------------------------------------------------------------------------
优势 1: 严格分层 — 编译期与运行期彻底解耦
  证据: 链接规则显式声明 (asn1cxx 只链编译链, asn1_codegen 不链运行时,
  generated TU 才链编解码库)。这是 pasn1 (解析器与 BER/UPER 生成器混在
  一个编译器) 被本项目文档明确列为反面教材后确立的规则。
  价值: 编解码器可独立单测 (Phases 6-10 不等 codegen); 运行时可被
  手写代码直接使用, 不必经过 asn1cxx。

优势 2: 编码规则覆盖最全, 且实现复用而非复制
  11 种: UPER/APER/BER/DER/CER/OER/COER/XER/CXER/E-XER/JER/JERI
  (对比: asn1c 6 种, Python asn1tools 8 种, 见对比文档)
  - UPER 与 APER 是同一 PER 实现的两个 Variant 参数, 对齐只发生在
    X.691 要求的原语内部 (非 "UPER+末尾填充");
  - DER = BER + 规范化策略; COER = OER + 规范化策略;
  - CXER = XER + 规范化策略; E-XER/JERI 是指令效果层。
  价值: 修复一个 PER 位布局 bug 同时惠及 UPER/APER; 策略类规则
  (如最短长度) 只写一次。

优势 3: 多编码规则组合生成 (--codec uper,ber)
  本轮新增: 位掩码 + 逗号组合/重复累加, asn1c/asn1tools 均为单选。
  5 项 CLI 行为测试 + Release/sanitizer 双构建 296/296 验证。

优势 4: 信息对象类完整支持 — 电信协议的入场券
  CLASS/对象集/表约束/参数化类型/版本括号/INSTANCE OF/EXTERNAL/
  EMBEDDED PDV/CHARACTER STRING/REAL/BigInt(>64 位) 全部落地;
  Python asn1tools 明确不支持 CLASS/参数化 (其 README 自述);
  表约束 &Value 开放类型已实现 governor id dispatch + typed 视图
  (decoded_<field>)。
  价值: RRC/LPP/S1AP 这类依赖 ProtocolIE-Field 的真实协议栈可解析。

优势 5: 字节级金标准差分验证 — 唯一做位布局证明的方案
  PER/APER: 以 asn1c 为外部 oracle 生成精确字节向量, 54 向量断言;
  边缘规则 (X.691 19.9 扩展 addition preamble、版本括号组) 按 ITU
  原版 PDF 裁定, 不信"参考编译器"声誉。
  价值: 自往返一致不能证明位布局正确 (两份实现可以都错得一致);
  金标准向量是 3GPP 互操作的前提。

优势 6: Result<T> 无异常错误模型 + 位置诊断
  全链路 Result (仅 std::bad_alloc 可传播); 前端/语义诊断一律
  file:line:column; 编解码错误带 bit/byte 偏移; 禁止无信息失败。
  价值: 嵌入式/电信代码库普遍禁用异常, 该模型与目标场景匹配。

优势 7: 电信实战 + 编译门禁
  RRC Rel-8 / LPP Rel-14 / S1AP Rel-14 全量 fixtures; emit 产物作为
  CMake 静态库编译门 (emit 成功 ≠ 可编译, 这是 skill 固化的核心
  纪律); rrc_slice UPER/OER emit + 已知 hex 往返。
  P0-3 + CER 后: 六 fixture × 12 codec 全量 emit 产物 -fsyntax-only
  门禁 (每族发射逻辑的产物首次全部经过编译验证)。

优势 8: 零外部依赖
  无 Boost、无 multiprecision (BigInt 自研)、C++17 only
  (不用 std::span/std::expected, 自备 Result/Span)。
  价值: 交叉编译到工具链受限的目标平台无障碍。

优势 9: 位写路径为吞吐设计 — bitmap 密集消息不付逐位税
  证据: BitWriter 的 put_bits/put_bitmap/put_octets 在八位对齐流上
  走整字节批量写, presence bitmap (byte-per-bit) 按 64 位打包一次
  写出; 实测 bitmap 密集型写 3.2×, UPER 往返 +12% (P2-1, 2026-10-15),
  输出字节与逐位循环逐比特一致 (三套金标准向量零回归守门)。
  价值: RRC/LPP/S1AP 这类 preamble/bitmap 密集的 PDU 编码吞吐直接
  受益, 且优化只发生在运行时原语层, 不破坏分层铁律。

--------------------------------------------------------------------------------
3. 量化现状 (实测, 2026-10-13)
--------------------------------------------------------------------------------
指标                              数值
--------------------------------- ----------------------------------------
RRC 全模块 emit 耗时              0.425s (EUTRA-RRC-Definitions.asn)
RRC 生成产物                      1 个 generated.hpp, 33,697 行, 全 inline
生成的 generated.cpp              2 行 stub (仅 #include "generated.hpp")
单 TU 全量编译 (含该头)           23.0s  ← 实测
单 TU 语法检查 (-fsyntax-only)    19.3s  ← 实测
asn1_codegen 库重编译 (改 emit.cpp 后, -j1)  18.9s  ← 实测
测试                              296 项, Release 6.0s / sanitizer 32.5s
金标准差分覆盖                    仅 PER/APER (54 向量); 其余 codec 仅自往返

--------------------------------------------------------------------------------
4. 问题与优化方案 (按优先级)
--------------------------------------------------------------------------------

[P0-1] 生成代码全 inline 于单头文件 — 集成成本数量级问题  ✅ 已完成 (2026-10-14)
  现状证据: 33,697 行的 generated.hpp, 每个类型的 encode_uper/
  decode_uper 都是 inline 自由函数; companion TU (generated.cpp)
  只 include 头、不产出任何符号。任何包含该头的 TU 支付 23s 编译;
  多 TU 工程 = N × 23s。
  方案: 生成头只放类型声明 + 编解码函数声明 (非 inline),
  函数定义全部落到 generated.cpp (一次性 23s), 后续各 TU 只付
  声明解析 (~百 ms 级)。保留开关 --emit-inline 兼容现状 (单 TU
  小模块仍可全 inline, 零链接步骤)。
  预期收益: 多 TU 电信集成编译时间数量级下降; 产物头从 33.7k 行
  缩到声明级 (约 1/5); IDE 索引同样受益。
  风险: ODR 行为变化 (当前 inline 是每 TU 一份, 改后单一定义);
  CMake 编译门需确认 generated.cpp 真正参与编译 (当前 stub 意味着
  门禁实际在编译 .hpp, 改后需保证门禁仍覆盖 .cpp)。
  验证: 以 RRC fixture 重测单 TU 编译耗时; 296 测试 + 金标准向量
  不回归。

  实施结果 (RRC Rel-8 全模块 fixture 实测):
  - 头文件 33,697 → 5,998 行 (-82%); 定义 (28,847 行) 全部落入
    generated.cpp, 包在生成命名空间内; emit 耗时不变 (0.40s)
  - 消费 TU (-fsyntax-only, 同一会话同机对比):
    22.1s → 5.2s (-76%); 每个额外 TU 省 ~17s
  - 定义 TU 一次性成本: 62.8s 全量编译 (-O0; 其中解析 21.5s);
    盈亏平衡点 N≈3-4 个 TU (旧模型 N×23s vs 新模型 62.8s+N×5.2s)
  - 开关: CLI 默认 out-of-line, --emit-inline 恢复旧行为;
    EmitOptions::emit_inline 默认 true 保证 API 调用方零回归
    (codegen_test/telecom_test 的文本断言不受影响)
  - 收益外收获: 此前 generated.cpp 是空 stub, 静态库编译门禁
    实际没编译任何生成代码; 现在门禁编译真实定义
  - 修复: codec_benchmark_tests 链接行补 asn1::der/asn1::coer
    (der/coer 定义 TU 此前无符号所以缺链不可见)
  - 验证: Release 298/298 (新增 2 项 CLI 测试断言
    默认 out-of-line / --emit-inline 两种形态) + sanitizer 全量 +
    PER/APER 金标准向量零回归

[P0-2] emit.cpp 单文件 5,208 行 / 103 成员函数 — 编译与维护瓶颈  ✅ 已完成 (2026-10-14)
  现状证据: 改动 emit.cpp 后单线程重建 asn1_codegen 需 18.9s;
  单文件承载 PER/OER/BER/TLV/JER/XER 全家族的发射逻辑。
  方案: 按 codec 家族拆为 emit_per.cpp / emit_jer.cpp / emit_xer.cpp /
  emit_tlv.cpp / emit_core.cpp (类型声明/工具函数), Emitter 类拆为
  partial class 或 改为按家族的自由函数组 + 共享上下文结构。
  预期收益: 单家族改动的重编译从 18.9s 降到 ~5s; 并行编译收益;
  评审半径缩小 (JER 改动不再动 PER 代码路径)。
  风险: 成员函数互相引用密集 (inline_type_cpp 等被全家族使用),
  拆分边界需先梳理依赖; 无功能风险。
  验证: 各 codec round-trip 测试 + fixture 编译门不回归。

  实施结果 (97 个函数全量清点 + 依赖图后):
  - 拆为 7 个文件: emit_internal.hpp (586 行: 共享自由函数声明 +
    Emitter 类完整声明 + AliasShape 嵌套枚举), emit_core.cpp (963 行),
    emit_per.cpp (1,061), emit_xer.cpp (912), emit_jer.cpp (782),
    emit_oer.cpp (769), emit_tlv.cpp (783)。emit.cpp 删除。
  - 依赖图结论: 共享面 = 9 个自由函数 (cpp_ident/resolve/is_named/
    named_type_cpp/integer_is_bigint/integer_host_type/opt_i64_expr/
    opt_size_expr) + 11 个成员 (type_cpp/inline_type_cpp/emit_header/
    emit_type_decl/emit_struct_fields/choice_all_fields/
    field_has_presence_bit/field_wraps_optional/emit_ext_group_field_present/
    default_val_cpp/type_is_constructed/collect_named/run); 其余 86 个
    成员函数全部家族内聚, 零跨家族私有调用。
  - 单家族改动重编译 (asn1_codegen, -j1): 18.9s → 5.5s (JER) /
    6.9s (PER); 多文件并行生效 (三文件同时改动 wall 6.9s, user 7.5s)。
  - 迁移方式: 函数体字节级不变 (切片脚本按闭合花括号切分, 只改
    定义行加 Emitter:: 限定); RRC UPER 产物与拆分前 diff 逐字节一致。
  - 顺带修复: 原文件 7 处被此前误操作损坏的文档注释
    (is_named/named_type_cpp/integer_is_bigint/xml_ns/xml_fn_suffix/
    oer_ns/oer_fn_suffix, 函数体文本混进 Parameters/Returns 行)
    在搬迁时重写为干净文档。
  - 验证: Release 298/298 + sanitizer 298/298 + RRC 产物字节一致 +
    PER/APER 金标准向量零回归。

[P0-3] 非主 codec 产物从未验证可编译 — 每族发射器各自藏缺陷  ✅ 已完成 (2026-10-14)
  现状证据: CMake fixture 门禁只链 fixture 的主 codec 库
  (RRC/LPP→uper, S1AP→aper), 即 XER/EXER/CXER/JER/OER/COER/TLV
  组合产物此前由任何门禁验证过。共享辅助 (default_val_cpp 等)
  的签名改动只会被单 codec 门禁部分覆盖。
  方案: 对四大 fixture (RRC/LPP/S1AP + 新 tlv_oer_golden) 用全
  codec 位掩码 emit + g++ -fsyntax-only 逐族收敛错误; 每修一个
  根因重测。
  预期收益: 每族发射逻辑各有一份概念 bug 副本 (inline .value、
  命名空间字面量、解引用优先级等), 只有该族产物被编译检查才
  暴露; 此前这些缺陷全部潜伏。
  风险: 无功能风险 (纯产物正确性); 修复面横跨 6 个发射文件。
  验证: 6 fixture × 12 codec 全量 -fsyntax-only 零错误 +
  Release/sanitizer 316/316。

  实施结果:
  - 根因 1 (XER 命名空间): emit_xer 的 Element 类型硬编码
    asn1::exer::Element, Basic-XER/CXER 产物引用 E-XER 命名空间
    却未 include 其头 (EXER 模式恰好自洽故此前不可见);
    35 处全部改走 xml_ns()
  - 根因 2 (解引用优先级): find_child 返回指针, 生成
    *child.value().children (成员访问先于解引用) →
    (*child.value()); XER 可选字段 *value.f.bits → (*value.f).bits
  - 根因 3 (JER/XER inline 列表): 匿名 SEQUENCE OF/SET OF 误用
    命名别名结构的 .value (OER/TLV 已修、JER/XER 漏网),
    统一 is_named(rt) ? expr + ".value" : expr
  - 根因 4 (嵌套 CHOICE): 替代结构体未限定 (Nested_rec_q::m_),
    JER/XER 补上 OER/TLV 已有的 choice_ty 守卫; JER 选择解码
    误发 return value; (Result<void> 函数) → Result<void>::success()
  - 根因 5 (解码临时变量遮蔽, 五族全中): 固定命名 field 的临时
    变量在嵌套可选 SEQUENCE 中遮蔽外层同名临时变量——而外层
    临时变量正是赋值目标 expr, 嵌套可选字段全部赋错对象
    (RRC 真实报错 field.X = std::move(field)); 新增
    Emitter::temp_counter_, 统一 field<N>/alt<N>/xer_field<N>/
    jer_field<N>
  - 根因 6 (CHOICE 替代局部变量自遮蔽): const auto& alt =
    std::get<N>(alt.value...) 推导前引用自身 (auto 推导错误),
    PER/OER/TLV/XER 统一 alt<N>
  - 顺带修复 (WIP 遗留): X.680 25.3/29.2 任一文本标签禁用整表
    自动编号 (IR 实测 plain 保持 UNIVERSAL 2); default_val_cpp
    改为按字段取默认值 (非约束 INTEGER 包 BigInteger::from_i64、
    字符串剥离 lexer 引号); TLV EXPLICIT 同标签编自然 universal
    标签 (X.690 8.5); OER 选择标签无 constructed 位 (X.696 8.7.2)
  - 新 fixture: tests/fixtures/telecom/tlv_oer_golden.asn
    (混合标签/DEFAULT/SET/SET OF/嵌套 CHOICE/扩展) + 8 项往返
    测试 (字节级断言 30 0F 02 01 07 81 01 08 82 02 68 69
    A3 03 01 01 FF; DEFAULT 省略/携带 83 01 63 / 84 03 62 79 65)
  - 验证: 6 fixture × 12 codec -fsyntax-only 零错误; Release +
    sanitizer 316/316, 零 ASan/UBSan

[P1-1] 金标准差分验证从 PER 扩展到其余 codec  ✅ 已完成 (2026-10-15)
  现状证据(修复前): tests/per/golden 是唯一 oracle 差分; OER/BER/DER/JER/
  XER 等只有自往返测试 (自一致 ≠ 位布局正确, 见 skill 纪律)。
  方案: `scripts/gen_tlv_golden.py` 以 asn1c (DER 仲裁) + asn1tools
  (BER/OER 向量源) 为 oracle 生成 16 用例字节向量; 每处分歧按 ITU PDF
  裁定而非按编译器名声: asn1tools 不排序 SET/SET OF (X.690 11.6 要求
  X.680 8.6 标签序/编码八位组串升序) → asn1c `der_encode` 胜出并写回;
  asn1c `oer_encode` 对 SET 值段错误 (已知 bug) → OER 列保留 asn1tools;
  BER 无规范 SET 序 (X.690 8.14) → SET 用例 BER 列记 '-'。
  收益+风险: OER/BER/DER 获得字节级正确性证明; oracle 偏差裁定工作量
  集中在 DER SET/SET OF 三例与 OER SET 段错误, 未引入 toolkit 回归。
  实施结果+验证: `asn1_tlv_golden_tests` 3 项 (BER/DER/OER) 16 用例全过;
  OER SET 组件暴露并修复定义序 emit——改为 X.696 18.2 规范(标签)序,
  preamble 位序与解码镜像同一顺序; Release + sanitizer 319/319, 七 fixture
  × 12 codec 编译门禁零错误, per_golden 向量零回归。

[P1-2] 补齐 CER (X.690)  ✅ 已完成 (2026-10-15)
  现状证据(修复前): --codec 无 cer; asn1c 支持 CER (对比文档第 1 节)。
  方案: DER 的策略层已具备最短长度/规范 SET 序, CER = BER + 规范化
  策略 (DER 的姊妹), 实现模式与 DER=BER 策略完全同构, 增量小。
  预期收益: 编码规则覆盖 10 → 11, 追平 asn1c 全集;
  价值: 某些证书/协议仍要求 CER。
  风险: CER 的 SET OF 序规则与 DER 不同 (编码值序而非标签序),
  需按 X.690 实现; 工作量中等。
  实施结果+验证: `src/runtime/cer/codec.cpp` 561 行策略层落地 (9.1
  不定长包装 / 9.2+8.6.4 分片 / 11.2 末八位组清零 / 9.3 SET/SET OF
  标签序); `cer_golden` fixture + 10 项字节级金标准 (无外部 CER
  oracle, 由 X.690 条文推导 + DER 值等价交叉验证); 顺带修复全 TLV
  SET 解码器未打标签 CHOICE 派发; Release + sanitizer 316/316。

[P2-1] bit_io 逐位写路径优化  ✅ 已完成 (2026-10-15)
  现状证据: BitWriter::put_bit 每比特做除法取字节下标 + 分支 + OR;
  encode_bitmap / put_bits(value,nbits) / put_bits(Span,nbits) 全部
  逐比特循环; UPER 的 presence bitmap/扩展 bitmap 大量逐位调用。
  方案: put_bits(value, nbits) 改为先填当前半字节、再整字节批量写;
  put_bitmap(Span<byte-per-bit>) 按 64 位打包后一次 put_bits;
  put_octets / put_bits(Span) 在八位对齐流上直接 memcpy 级批量拷;
  put_bit 下标除法改位移 (>>3 / &7); encode_bitmap 与 BIT STRING
  分片写路径全部改走新批量接口。
  预期收益: PER 编码吞吐提升 (bitmap 密集的消息类型显著);
  对 BIT STRING 内容路径 (put_bits(Span)) 无影响。
  风险: 对齐语义 (align_to_octet 时机) 不能变, 改动需金标准向量
  零回归守门。
  验证: per_golden_vectors_test 全部通过 + benchmark 对比
  (tests/codegen/codec_benchmark_test.cpp 已有框架)。

  实施结果 (同机同会话前后对比, Release -O2):
  - 新增 BitIoBenchmark.PresenceBitmapWrite (10×64-bit bitmap +
    交错 6-bit/1-bit 小字段, bitmap 密集型写形态): 196,532 →
    626,075 ops/sec (≈3.2×)
  - UPER 往返 micro-benchmark: 844,276 → ~950,000 ops/sec (+12%);
    OER/BER/JER 无显著变化 (各自走字节路径)
  - 输出字节逐比特一致: 批量路径与逐位循环同一 MSB-first 语义,
    由 per_golden (UPER/APER 54 向量) + tlv_golden + cer_golden
    金标准零回归证明
  - 边界处理: put_bits(Span) 快速路径要求 tail <8 bit 时源多一个
    字节, 否则回退逐位循环 (与旧行为一致, 不越界); put_bitmap 按
    64 位分块打包, 任意长度正确
  - 验证: Release 320/320 (新增 1 项 benchmark) + sanitizer 320/320
    + 三套金标准向量零回归; 发射器未改动, 全 codec 编译门禁不受影响

[P2-2] 大模块分文件/分命名空间 emit  ⚠️ 部分完成 (2026-10-15; 已知失败: XML 家族单 codec, 按指示暂停修复)
  现状证据: RRC 单头 33,697 行 (即便 P0-1 拆声明后仍可观);
  Phase 40 已有 synthetic nested names 基础; 计划中的 "LPP 多模块"
  验证对象实为单模块 fixture (LPP-PDU-Definitions 仅 1 个
  DEFINITIONS, 无 IMPORTS), 真正的多模块 fixture 是 S1AP (6 模块,
  含跨模块 IMPORTS 与互相递归)。
  方案: --emit-split 按 ASN.1 模块边界生成 <Module>.hpp/.cpp 对 +
  聚合头; 头内类型命名空间与模块名对齐; 跨模块类型引用按
  <Module>::Name 限定 (named_ref, 仅 split 模式生效, flat 产物不变);
  互相递归的模块 (依赖图 SCC) 合并进同一 header/.cpp 单元 —— 按值
  跨模块成员要求被引类型完整, 独立头文件无法满足环, 合并单元内按
  类型依赖序 (named_order_ 即全局依赖优先) 重开各自模块命名空间;
  每模块头内自带 detail::Null (聚合头全量 include 时避免重复定义)。
  收益+风险: 单文件可读性、增量编译 (改一个模块只重编该 .cpp)、
  IDE 性能; 风险为跨模块 include 管理 (新增面), 已按结构依赖
  (字段/元素/替代/表约束条目/别名解析目标/OCF fixed_type) 计算
  模块依赖图并生成依赖 include。
  实施结果+验证: emit_core.cpp collect_module_groups (SCC 检测 +
  模块依赖 + 依赖优先单元序) 与 run_split (单模块三趟发射 / 合并
  单元命名空间切换) 落地; EmitOptions::emit_split +
  CppGenerator::emit_split(dir) API; CLI --emit-split; 家族文件
  引用位 (type_cpp/choice_ty/static_cast/CHOICE 替代结构) 改走
  named_ref。产物逐单元 g++ -fsyntax-only 实测: LPP (单模块路径)
  uper/aper/coer/ber/der/oer/jer 零错误; S1AP (3 个 emit 单元:
  CommonDataTypes / PDU-Descriptions / 合并的
  IEs+Containers+PDU-Contents) uper/ber/der/oer/jer 零错误; RRC
  uper 零错误; flat 产物与改动前逐字节一致 (S1AP uper diff 为空);
  examples/split_demo.asn 双模块示例 (Demo_Base/Demo_App) 编译通过;
  CMake 新增 asn1_split_lpp_split / asn1_split_s1ap_split 编译门禁;
  Release 320/320 (无新增 gtest, 拆分为编译门禁 + 手工矩阵验证)。
  ⚠️ 已知失败 (暂停优化, 记录于此): --codec xer/exer/cxer 单独选择
  时 split 产物编译失败 —— LPP 182 错 / S1AP 1609 错, 统一报
  'make_error' is not a member of 'asn1'。根因不在 split: flat 模式
  --codec xer 单独跑复现同样 182 错 (已实测) —— emit_includes 的
  XML 分支未 include <asn1/runtime/byte_io.hpp> (make_error 所在),
  全 12-codec 组合门禁因 oer/tlv/jer/per 分支带入 byte_io.hpp 而
  掩盖该缺口 (单 codec 选择才暴露)。属既有发射器 include 缺口,
  非 split 引入; 修复方向为 XML 分支补 byte_io.hpp (一行), 按用户
  指示暂停。状态同步记录于 TODO.md 剩余项。

[P2-3] 增量代码生成 (src 未变则跳过)
  现状证据: 用户既有工作流偏好 (src 未变不重跑 generate/preprocess);
  当前 asn1cxx 每次全量 emit。
  方案: asn1cxx 增加内容哈希缓存 (源文件 hash → 已生成产物),
  命中则跳过; 与 --emit-split 天然配合 (按模块粒度缓存)。
  预期收益: 大模块重复 emit (0.425s) 归零; 集成构建系统收益。
  风险: 低 (纯性能优化, 产物内容不变)。
  验证: 同一输入二次运行跳过 + 修改后正确失效。

[P3-1] 前端并行分析 (多模块)
  现状证据: 语义分析四趟串行, 大规范 (S1AP 多模块) 可感知。
  方案: 第一趟符号声明按模块并行, 后三趟 (引用解析/标签/约束)
  按依赖图并行; 需线程安全的 Diagnostics (加锁聚合)。
  预期收益: 大规范分析阶段提速; 改动面大, 收益待测。
  风险: 中 (符号表并发插入顺序影响诊断可重现性)。
  验证: 诊断输出逐字节不变 (可重现性是硬要求)。

--------------------------------------------------------------------------------
5. 明确不做的优化 (守住优势)
--------------------------------------------------------------------------------
- 不动分层铁律: 不为了性能把 codec 算法塞进编译器或让运行时
  依赖 IR (优势 1 的根基);
- 不引入解释式/反射式运行时: 与 "生成薄编排 + 共享原语" 的
  架构决策冲突, 且牺牲类型安全与性能;
- 不引入 Boost/multiprecision: 优势 8 的根基;
- 不把金标准向量换成"信任 oracle": 边缘规则永远按 ITU PDF 裁定。

--------------------------------------------------------------------------------
6. 实施路线建议
--------------------------------------------------------------------------------
第 1 步 (本周): P0-1 生成代码 out-of-line (收益最大, 风险可控),
  以 RRC fixture 做前后编译耗时对比 + 全量 296 测试守门;
  同步更新 CMake 门禁确保 generated.cpp 真正被编译。✅ 已完成 2026-10-14
第 2 步: P0-2 emit.cpp 按 codec 家族拆分 (先画依赖图再动手)。✅ 已完成 2026-10-14
第 2.5 步: P0-3 全 codec 产物编译门禁 (非主 codec 产物从未验证可编译,
  每族发射器各藏缺陷; 以 12 codec 全量 -fsyntax-only 门禁驱动修复)。
  ✅ 已完成 2026-10-14
第 3 步: P1-1 OER/BER/DER 金标准扩展 (正确性投资, 服务 3GPP R17+
  OER 方向)。
第 4 步: P2-1 bit_io 批量位写 + benchmark 量化。
  ✅ 已完成 2026-10-15 (bitmap 写 3.2×, UPER 往返 +12%,
  320/320 双门禁 + 三套金标准零回归)。
后续: P1-2 CER ✅ 已完成 2026-10-15、P2-2 分文件 emit ⚠️ 部分完成
  2026-10-15 (LPP/S1AP 上 uper/aper/coer/ber/der/oer/jer 全通过;
  XML 家族单 codec 既有 include 缺口暴露, 按指示暂停修复并记录)、
  P2-3 增量生成按需排期。

每步验证口径统一: ctest 320/320 (Release + sanitizer) + 受影响
fixture 的产物 g++ -fsyntax-only 0 error (全 codec 位掩码, 见 P0-3)
+ 金标准向量零回归。
================================================================================
