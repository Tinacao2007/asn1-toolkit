# 横向对比：asn1c / Python asn1tools / asn1-toolkit

对比时间：2026-10-13（首版），2026-10-15 更新（asn1-toolkit CER 家族落地、
测试总数 296 → 320、Phase 45 部分完成），2026-10-16 更新（Phase 46
增量代码生成 / 内容哈希缓存就位，测试总数 320 → 332）。

数据来源（均为本地实测，非仅引用宣传资料）：

- asn1c：`/mnt/f/ASN_1/ASN1C/asn1c.zip`（1017 文件，解包实测）
- Python asn1tools：`/mnt/f/ASN_1/Python_asn1tools/`（v0.166.0，本地树）
- asn1-toolkit：本仓库（Phase 45，部分完成）

另参考：ITU 官方工具页、asn1tools 官方文档（readthedocs）。

## 三方概览

| 维度 | asn1c | Python asn1tools | asn1-toolkit |
|------|-------|------------------|--------------|
| 身份 | 经典开源 ASN.1→C 编译器 | Python 编解码库 | C++17 ASN.1 编译器 + 编解码运行时 |
| 作者/维护 | Lev Walkin (lionet.info) | Erik Moqvist (eerimoq) | PROCODEC |
| 许可证 | BSD-2-Clause (2003–2017) | MIT | TBD（计划开源） |
| 目标语言产物 | C 代码（C++ 兼容） | Python 对象（解释执行） | C++17 代码 |
| 代码规模 | 1017 文件 / 7.4 万行 C | 230 py 文件 / 22.0 万行 | ~83 文件 / ~3.9 万行 C++17 |
| 解析器 | 手写 libasn1parser | 手写 Python 解析器 | 手写 lexer + 递归下降（弃 flex/bison） |
| 代码生成器 | 展开式：每类型独立 enc/dec | 无（运行时按类型分派） | 编排式：薄函数调用共享运行时原语 |
| 运行形态 | 编译型，零运行时依赖 | 解释型，需 Python 运行时 | 编译型，C++ 静态库 |

## 编解码规则覆盖（核心维度）

| 编码规则 | asn1c | asn1tools | asn1-toolkit |
|----------|-------|-----------|--------------|
| BER（基本） | 支持 | 支持 | 支持 |
| DER（区分） | 支持 | 支持 | 支持 |
| CER（规范） | 支持 | 不支持 | 支持（Phase 43 落地） |
| PER / UPER（非对齐） | 支持 | 支持 | 支持 |
| PER / APER（对齐） | 不支持* | 支持 | 支持 |
| OER（基本八位组） | 支持 | 支持 | 支持 |
| COER（规范八位组） | 不支持 | 不支持 | 支持 |
| XER（基本 XML） | 支持 | 支持 | 支持 |
| CXER（规范 XML） | 不支持 | 不支持 | 支持 |
| E-XER（扩展 XML） | 不支持 | 不支持 | 支持（编码指令矩阵） |
| JER（基本 JSON） | 不支持 | 支持 | 支持 |
| JERI（JER 编码指令） | 不支持 | 不支持 | 支持 |
| GSER（通用字符串） | 不支持 | 支持 | 不支持 |
| **合计** | **6** | **8** | **12**（另有 `per` = UPER+APER 组合别名） |

\* asn1c：解包实测 `skeletons/` 下仅 ber/oer/per/xer/der 编解码器，未见
APER 相关实现；官方描述为 "BER/DER/CER/PER/XER"。

要点：

- 三者在"常见三件套"（BER/DER/PER/XER/OER）上重叠；
- asn1-toolkit 独有：COER / CXER / E-XER / JERI，且 UPER/APER 共享一个 PER
  实现（Variant 参数化，非两份代码）；CER 于 Phase 43 补齐后追平 asn1c 的
  编码规则全集；
- 多编码规则组合生成：asn1-toolkit 的 `--codec` 支持逗号组合（如
  `uper,ber`），asn1c / asn1tools 均为单次单选；
- 2020 年后新增的 OER 系（OER/COER）+ JSON 系（JER）是 asn1tools 与
  asn1-toolkit 的分水岭；3GPP R17+ 部分新 IE 已转向 OER/JSON。

## ASN.1 语法与高级特性覆盖

| 特性 | asn1c | asn1tools（官方 README 声明） | asn1-toolkit |
|------|-------|------------------------------|--------------|
| CLASS (X.681) | 解析支持（asn1p_class.c） | 不支持 | 支持（对象类/对象集/表约束） |
| 参数化 (X.683) | 部分支持 | 不支持 | 支持（实例化 + 依赖遍历） |
| 表约束/组件关系 | 支持 | 不支持 | 支持（typed IE dispatch） |
| INSTANCE OF | 支持 | 不支持 | 支持 |
| EMBEDDED PDV | 支持 | 不支持 | 支持（含 modern EXTERNAL） |
| ANY / DEFINED BY | 支持（1994 前） | 不支持（已从标准移除） | 识别并拒绝 / 按现代 EXTERNAL 处理 |
| EXTERNAL | 支持 | 未明确 | 支持（含 modern 识别形式） |
| REAL | 支持 | 部分 | 支持（IEEE-754 二进制，OER 分类） |
| 约束子集语法 | 完整 | "subset" 语法 | 完整 |
| WITH COMPONENT(S) | 支持 | 忽略（OER REAL 除外） | 支持（WITH COMPONENTS 分类 OER） |
| 版本括号 `[[...]]` | 支持 | 未明确 | 支持（IR 级 ExtensionGroup） |
| 编码指令 | 不支持 | 不支持 | 支持（XER/JER 指令矩阵） |

要点：

- 信息对象类（CLASS）是电信协议栈（RRC/LPP/S1AP 的 ProtocolIE-Field）的
  必备语法。asn1tools 在此直接出局；asn1-toolkit 把 `&Value` 开放类型 +
  governor id dispatch 做成了类型化视图（`decoded_<field>`）；
- asn1-toolkit 对"不支持的语法"采取"识别并明确拒绝"策略，避免把真实规范
  误报为语法错误 —— 这是对电信大模块的硬性要求。

## 编译器架构与流水线

**asn1c**：手写解析器（libasn1parser）→ 语法树 → 直接展开为每类型独立 C
函数。解析与生成同属一个编译器（但内部有独立 parser 库，分层好于 pasn1 的
lex/yacc 混合方案）。约束检查代码随类型自动生成。

**asn1tools**：Python 解析 → Python 内部编译结构 → 运行时按类型分派编码。
无独立代码生成阶段（仅 OER/UPER 提供有限 C 生成器）。

**asn1-toolkit**：ASN.1 → Lexer → Parser → AST → Semantic（符号表/标签/
约束四趟）→ Type IR（TypeArena）→ C++ 代码生成 → 生成值类型 + 薄编排 →
运行时原语（12 种编码规则）。硬性分层规则：编译器库从不 include 编解码
算法；运行时从不 include AST/符号表/IR 头；生成的 `.cpp` 是唯一桥梁；
UPER 与 APER 是同一 PER 实现的两个 Variant 参数；DER=BER+规范化策略、
COER=OER+规范化策略（不复制算法）。

评价：asn1c 的"展开式生成"性能最优但产物体积大；asn1-toolkit 的"编排式
生成"产物更小、大模块保持可读可分文件编译，代价是每类型多一层函数调用。

## 约束（Constraint）处理

- **asn1c**：自动生成约束检查代码（`skeletons/constraints.c`、
  `constr_TYPE.c` 等），编码时可拒绝越界值。
- **asn1tools**：有 `constraints_checker` / `permitted_alphabet` 模块，但
  README 声明 WITH COMPONENT(S) 被忽略（OER REAL 除外）。
- **asn1-toolkit**：约束作为独立 AST 节点族（ValueRange / SingleValue /
  SizeConstraint / Union / Intersection / Extensible），语义阶段规范化折叠
  为 `ConstraintDesc`（PER 直接读描述符，不重解析 `(0..255)`）；PER 约束
  INTEGER 越界直接拒绝（不 clamp）；OER REAL 按 IEEE binary32/64 分类
  （X.696）。

## 性能特征

- **asn1c**：生成原生 C，单文件自包含 skeletons，无运行时依赖。电信嵌入式
  领域事实标准，吞吐最高。
- **asn1tools**：Python 解释执行，吞吐最低。本地树带 fuzzer/rust 实验
  （rust_source.rs、boolean_uper.c 手写热路径替换等），说明官方也在探索
  性能优化路径。
- **asn1-toolkit**：生成 C++17 薄编排 + 共享运行时原语，编译型零解释开销，
  量级与 asn1c 同档（未做正式跨项目 benchmark，不做速度断言）；P2-1 后
  bitmap 密集型写路径有 3.2× 实测提升（见 changelog.md）。

## 错误处理与诊断

- **asn1c**：约束违例生成检查代码，返回错误码；解析错误有位置信息。
- **asn1tools**：Python 异常（DecodeError 等），适合脚本交互。
- **asn1-toolkit**：全链路 `Result<T>` 无异常（仅 `std::bad_alloc` 可传播）；
  前端/语义诊断一律带 `file:line:column`；编解码错误带 bit/byte 偏移；
  "无法解析"必须给出 token 或类型名，不允许无信息失败。

## 测试与正确性验证方法

- **asn1c**：自带测试套件（zip 内含 1017 文件，含 test 用例）。
- **asn1tools**：pytest 全套（test_ber/der/gser/jer/oer/per/uper/xer +
  codecs 一致性 + C 生成测试，20+ 文件）。
- **asn1-toolkit**：GoogleTest + CTest，当前 **344 项测试**；Release +
  sanitizer（ASan/UBSan）双构建门禁；三套字节级金标准差分向量 —— PER/APER
  以 asn1c 为外部 oracle（54 向量）、BER/DER/OER 以 asn1tools + asn1c 为
  oracle（16 用例 × 3 codec）、CER 由 X.690 条文推导 + DER 值等价交叉验证
  （10 用例）；电信 fixture（RRC/LPP/S1AP + 4 个专用 fixture）全量 emit
  产物作为 CMake 静态库编译门（emit 成功 ≠ 可编译）。

要点：asn1-toolkit 是三者中唯一做多套"字节级差分验证"的 —— 自往返一致
不能证明位布局正确，金标准向量才能。

## 电信领域实战

- **asn1c**：3GPP 参考实现生态的事实标准（大量协议栈用它）。
- **asn1tools**：小型测试文件，无电信大模块验证。
- **asn1-toolkit**：RRC Rel-8 / LPP Rel-14 / S1AP Rel-14 完整 fixtures；
  全量 emit 产物编译门禁；rrc_slice UPER/OER emit + UPER 往返对已知 hex
  向量；S1AP 按 ProtocolIE-Field（表约束 typed IE）验证；`--emit-split`
  按模块分文件生成（S1AP 6 模块验证）。

## 依赖与可移植性

- **asn1c**：零运行时依赖（生成代码自带 skeletons）；裸机/嵌入式友好。
- **asn1tools**：Python 3 + bitstruct + parsing；无编译步骤。
- **asn1-toolkit**：C++17 + CMake 3.16+；零 Boost / 零 multiprecision
  （BigInt 自研）；目标平台 Linux/GCC/Clang（MSVC 本地可用）。

## 许可证与商用

- **asn1c**：BSD-2-Clause，可商用（保留版权声明）。
- **asn1tools**：MIT，可商用。
- **asn1-toolkit**：TBD，计划开源（README 明确 license 文件发布时补齐；
  源码头已标注 PROCODEC 版权 2026–2031）。

## 选型建议（按场景）

| 场景 | 推荐方案 |
|------|----------|
| 遗留 C 项目 / 裸机 / 嵌入式无 OS | asn1c（零依赖，电信事实标准） |
| 快速原型 / 脚本 / 测试向量生成 / 教学 | Python asn1tools（MIT，即装即用） |
| 生产级 C++ 项目 | asn1-toolkit：需 COER/CXER/E-XER/JER/JERI（12 种编码规则全覆盖）；含 CLASS/表约束/参数化（电信 IE，唯一支持信息对象类）；需 Result 无异常错误模型 / 强诊断；需金标准字节级正确性证明；需多编码规则一次组合生成 |

不适用 asn1-toolkit 的场景：

- 目标语言是 C 而非 C++（先用 asn1c）；
- 无 C++17 工具链的环境；
- 只需要 BER 编码且追求最小产物（asn1c 展开式更直接）。

## 备注

- 本地 `/mnt/f/ASN_1/Python_asn1tools/` 是 asn1tools 的带实验改动 fork
  （含 rust_source.rs、fuzzer、c_source-minus 等非官方文件），对比以
  upstream 0.166.0 官方 README 声明的能力为准；
- 本对比基于 2026-10-13 本地实测，2026-10-15 更新 asn1-toolkit 侧状态
  （Phase 45 部分完成；XML 家族单 codec include 缺口已知，见
  [status.md](status.md#剩余工作)）；
- ITU 官方工具页（itu.int/en/ITU-T/asn1/pages/tools.aspx）可作第三方交叉
  参考；商业方案（OSS Nokalva / Objective Systems）不在本对比范围。
