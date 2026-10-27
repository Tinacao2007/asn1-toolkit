# 工程评审修正 TODO（2026-10-20）

本清单记录逐项交给 AI 实现与验收的修正，以各 REV 的状态和验收记录为准。初始清单基于静态评审，当时未复测历史 345/345；后续执行结果分别记录，不覆盖历史证据。每项执行前必须重新核查当前代码和调用路径：已变化的问题不得机械修改，待核查项不得直接宣称缺陷。

原有功能路线及 Phase49 子任务仍见 [todo.md](todo.md)，当前实现状态见 [status.md](status.md)。本文件是评审修正执行清单，使用 `REV-xx` 编号，避免与原 P1/P2/P3 编号混淆。

## 执行约束

- compiler（lexer/parser/semantic/IR/codegen）不得链接 runtime；runtime 不得依赖 AST/IR/前端头。只有生成产物连接类型和 runtime；inline 模式的桥梁在生成头中。
- C++ 业务失败使用 Result<T>，仅 std::bad_alloc 可以向外传播；C 使用状态码和输出参数，不以 C++ 包装替代纯 C11 实现。
- UPER/APER 共用参数化核心；C 后端也使用 Variant/上下文，不复制两份算法。
- 保留自研 BigInt、零第三方运行时依赖、既定 12 个产品入口以及三种产物形态。
- 保留 DER=BER+策略、COER=OER+策略、CXER=XER+策略。
- 保留 7 fixture × 12 实际 codec 入口的产物语法门禁；per 是 UPER|APER 组合别名，不计为独立入口。
- Release、ASan+UBSan 双构建及 asn1c/asn1tools 差分纪律不降低；工具能力不足时明确记录，不伪造 oracle 结果。
- 每次只处理一个编号或明确列出的紧密依赖项；不顺手扩大重构，不覆盖用户未提交修改。
- 完成项必须记录实际命令、结果和未验证范围。缺工具只能标记“实现完成、验证待补”，不能勾选完成。

## 状态与验收规则

所有复选框初始均未完成。状态可记录：待办、进行中、实现完成/验证待补、完成、不适用（附证据）。

每项最少交付：问题复现或调用链证据、最小修复、针对性验证、相关文档同步。线路正确性不得仅用本实现自身 round-trip 证明。

影响 runtime/生成语义的修改，应运行针对性测试、相关 fixture/codec 门禁，并在合入前通过规定的 Release 与 sanitizer 全量门禁；生成产物变更覆盖相关 inline/out-of-line/split。纯文档修改检查链接与表述即可，不为文档编写实现镜像测试。

## 第一批：C11 内存安全与失败传播（优先处理）

### REV-01 【严重】BitWriter 扩容失败与容量溢出

- [x] 状态：完成（2026-10-20）；依据：已发现未检查扩容结果的复制路径。

- 完成：ensure 返回状态，整次写入预留并检查溢出；take 失败不推进有效位，clear/take 后可安全复用。验证：故障门禁 424 检查、Release/sanitizer 各 346/346；详见 [验收记录](rev01-03-validation.md)。

- 范围：`src/c_runtime/bit_io.c`，对应头及 C 测试。

- 修改：ensure 返回状态；所有调用方检查；检查位计数加法、字节换算及倍增溢出；失败不越界、不推进有效写入长度。

- 验收：逐分配点失败注入；已有容量不足、空 writer、大请求均正确失败；ASan/UBSan 无越界；成功路径字节不变。

- 依赖：无。仅增加最小内部测试注入机制，不引入全局可变的生产分配器配置。

### REV-02 【严重】BigInt 不得吞掉分配失败或 limb 上限错误

- [x] 状态：完成（2026-10-20）；依据：`bigint_mul_add_u32` 忽略 `bigint_push_limb` 失败。

- 完成：临时解析后提交，逐层传播 OOM/limb 上限；覆盖各分配点失败、9864/9865 位十进制上限相邻值、序列化失败和释放。验证及实际命令见 [验收记录](rev01-03-validation.md)。

- 范围：`src/c_runtime/bigint.c`、`include/asn1/c_runtime/bigint.h`。

- 修改：算术 helper 逐层返回失败；失败不提交错误数值；核查容量乘法与 limb 上限。

- 验收：十进制解析逐分配点失败、limb 上限、负数与零；失败对象可安全释放；不返回截断数值的成功结果。

- 依赖：无；错误码约定与 REV-03 对齐。

### REV-03 【重要】C 状态码、所有权与失败后对象状态

- [x] 状态：完成（2026-10-20）；依据：OOM 与 LengthOverflow 混用、输出契约与实现不一致。

- 完成：保留旧错误码，新增 OOM/ResourceLimit；修正原语输出提交与失败清理，核对并记录逐 API 生命周期与游标例外。见 [C 契约](c-runtime-contracts.md) 和 [验收记录](rev01-03-validation.md)。完整 CMake 双构建、公共头和历史 84 组语法矩阵通过；未宣称补齐 JERI 或第二批线路能力。

- 范围：`include/asn1/c_runtime/*`、`src/c_runtime/*`。

- 修改：区分 OOM、资源限制、长度溢出；保留既有错误码数值；明确 init/free/reuse、返回内存释放方式和 borrowed view 生命周期；逐函数核对失败时 out 是否保持不变。

- 验收：分配失败、截断、重复初始化/复用契约测试；失败可释放且无泄漏；状态名完整。禁止要求 free 任意未初始化结构。

- 依赖：可先制定契约，最终整体验收在 REV-01/02 后。

## 第二批：C11 位操作与 PER 线路正确性

### REV-04 【严重】MSB-first 尾位和短源输入

- [x] 状态：完成（2026-10-20）；原依据：快路径取低位，慢路径取高位；短源可能少写仍成功。

- 完成：高位 MSB-first 尾位、源容量/NULL 校验、超过 64 bit 明确失败；独立逐位模型覆盖 8×18 组合。实测见 [第二批验收](rev04-09-validation.md)。

- 历史进展：第一批已补短源/NULL 预检与完整预留；本批补齐尾位语义和完整参考矩阵。

- 范围：`src/c_runtime/bit_io.c`，BIT STRING 调用路径。

- 修改：统一 MSB-first 语义；验证源长度、NULL/长度组合与 BIT STRING 容量；位数超过接口能力时明确报错，不静默截断。

- 验收：`0xA0` 写 3 bit 得到高位 `101`；0..7 起始偏移 × 0..17 位长度与独立逐位参考实现一致；短源返回失败。

- 依赖：REV-01。

### REV-05 【严重】无约束 INTEGER 补码编码及符号解码

- [x] 状态：完成（2026-10-20）；原依据：C PER 使用无符号幅值，负数解码可成功得到正数。

- 完成：signed/unsigned API 分离，最短补码、符号保护及负数/大数解码，空/冗余表示拒绝。独立差分及分配失败证据见 [第二批验收](rev04-09-validation.md)。

- 范围：`src/c_runtime/{bigint,per}.c`、相关公开头。

- 修改：明确区分 unsigned magnitude 与 signed two's-complement 转换；PER 无约束整数使用有符号最短内容；拒绝非法空内容。

- 验收：0、±1、±127、±128、±255、±256、INT64_MIN/MAX、>64bit；128 内容为 `00 80`；无约束输入 `01 FF` 解得 -1；与 oracle 向量对验。

- 依赖：REV-02/03。

### REV-06 【严重】PER 长度分片链

- [x] 状态：完成（2026-10-20）；原依据：大长度仅写 C1..C4 后直接写完整 payload。

- 完成：more 显式 determinant、完整字符串/open type 片段链、多片与结束零；默认上限保持，测试副本验证 65537/131072 byte；scalar helper 不丢弃分片状态。见 [第二批验收](rev04-09-validation.md)。

- 范围：`src/c_runtime/per.c`、`include/asn1/c_runtime/per.h`。

- 修改：区分单个 determinant 与完整片段链，内容层交错处理长度/片段及结束；解码保留继续片段信息；未支持路径明确 Unsupported。

- 验收：127/128、16383/16384/16385、32768、49152、65536/65537；整片终止、连续片段、截断及非法标记；oracle 字节及交叉解码。

- 依赖：REV-01/03/04；不要只修改 encoder 而保留不匹配 decoder。

### REV-07 【重要】BIT STRING 变长范围及整数约束分类

- [x] 状态：完成（2026-10-20）；原依据：BIT STRING 解码直接使用 upper；整数上下界缺省处理不对称。

- 完成：明确长度描述符、实际 bit length、根内/根外扩展、半约束、无界及完整 int64；SIZE 上界 64K 阈值独立对验。大于 int64 的有界描述符/不连续集合不在本原语接口能力内，详见 [验证边界](rev04-09-validation.md)。

- 范围：C PER 约束描述与 encode/decode。

- 修改：区分 fixed/constrained/semi-constrained/unconstrained/extensible；变长 BIT STRING 写读实际长度；核查全 int64 域和非法 range。

- 验收：SIZE(0)、SIZE(1..16)、固定 3/8/9 bit、上下界、空范围、半约束、全域整数。尚未实现类别必须失败而非近似编码。

- 依赖：REV-04/05/06。

### REV-08 【重要】C PER 统一 Variant 接口

- [x] 状态：完成（2026-10-20）；原依据：当前 C API 没有 variant，APER 尚延期。

- 完成：`per_core.c` 单核心以 Variant 参数化，旧 ABI UPER 包装；前置 bit 的整数/长度/字符串/open type 对齐和尾字段验证；234 独立双向用例、184 冻结向量。见 [第二批验收](rev04-09-validation.md)。

- 范围：`include/asn1/c_runtime/per.h`、`src/c_runtime/per.c`。

- 修改：增加统一 Unaligned/Aligned 参数或上下文；旧接口如需兼容，作为 UPER 包装；APER shim 只转发参数，不复制算法。

- 验收：对齐前存在 1 bit 的整数、字符串、长度及 open type 向量；同核心覆盖两 variant；未支持类别明确拒绝。

- 依赖：REV-07；本项先完成接口与已支持原语，其他原语按能力矩阵排期。

### REV-09 【重要】重构 C smoke 的验证口径

- [x] 状态：完成（2026-10-20）；原依据：部分 round-trip 命名仅检查编码，reader 只初始化也被描述为解码能力。

- 完成：旧 smoke 补实际解码，新正确性/大值门禁各 4473 检查、failure 498 检查；[声明登记](c-runtime-test-coverage.md) 明确 init 与 codec 能力边界；Release/sanitizer 各 348/348、历史矩阵 84/84。见 [第二批验收](rev04-09-validation.md)。

- 范围：`tests/c_lang/*`、`tests/CMakeLists.txt`、C runtime 注释。

- 修改：保留 Asn1CRuntimeSmoke；增加独立 primitive/故障测试；逐声明登记是否链接、调用、解码及负例已覆盖；修正过度承诺。

- 验收：REV-01..08 对应回归用例进入 CTest，Release/sanitizer 均执行；用例数量不是验收替代品。

- 依赖：随前述任务补测试，最后汇总。

## 第三批：C++ 错误模型与生成代码

### REV-10 【严重】E-XER 属性解码错误传播

- [x] 状态：完成（2026-10-20）；依据：生成 `if (tmp)` 忽略失败，并生成 std::stoll。

- 完成：属性缺失与非法值分流，数值完整解析/区间与宿主检查；三种产物实际链接运行负例，错误带字段名。 Release/sanitizer 各 370/370、公共 C++17 头 47/47、历史矩阵 84/84；见 [第三批验收](rev10-12-validation.md)。

- 范围：`src/codegen/emit_xer.cpp`、E-XER 生成代码运行测试。

- 修改：区分 missing/invalid，按 mandatory/optional/default 处理；数值转换返回 Result、完整消费并检查目标范围；失败立即传播。

- 验收：缺必填属性、非法布尔、12x、溢出、非法 BigInteger；三种产物行为一致，无业务异常且不静默保留默认值。

- 依赖：无，可在 C runtime 修复后独立执行。

### REV-11 【严重】消除编译器业务异常及 bad_alloc 吞捕获

- [x] 状态：完成（2026-10-20）；依据：SourceFile/IR 显式 throw、数字转换及 filesystem 抛异常 API、catch(...)。

- 完成：SourceFile/IR BigInt 返回 Result，OID/CLI from_chars，目录 error_code、写失败检查，线程资源回退；bad_alloc 保留，容量失败入口诊断；PER restricted-string 失败传播。 Release/sanitizer 各 370/370、公共 C++17 头 47/47、历史矩阵 84/84；见 [第三批验收](rev10-12-validation.md)。

- 范围：support、IR、parser、CLI、codegen 缓存与文件操作调用链。

- 修改：用 Result、from_chars、filesystem error_code；核查 std::async/system_error、容器 length_error 等边界；业务错误不靠异常，bad_alloc 不被转换为语法错误。

- 验收：不存在文件、输出不可写、非法数字/溢出、资源失败分别产生稳定错误；既有诊断位置保留。按入口拆小提交，不机械加 noexcept。

- 依赖：与 REV-10/14 协调，不要求全局禁用异常编译选项。

### REV-12 【优化】Result 访问契约和诊断单位

- [x] 状态：完成（2026-10-20）；依据：std::get 前置条件、offset bit/byte 含义不明确。

- 完成：Result nodiscard 与 assert/终止契约，补 move-only/const/死亡测试；显式偏移单位/字段路径及源码字节列，公共头严格独立编译。 Release/sanitizer 各 370/370、公共 C++17 头 47/47、历史矩阵 84/84；见 [第三批验收](rev10-12-validation.md)。

- 范围：`include/asn1/common/result.hpp`、runtime Error、Diagnostics。

- 修改：增加 nodiscard；明确 value/error 程序员错误契约；区分 byte/bit offset、字段路径与编译期源范围；不向 runtime 引入 IR。

- 验收：公共头 C++17 独立编译；正确/失败/移动访问有针对性检查；文档区分源码位置与线路位置。

- 依赖：REV-11 的错误接口改动稳定后。

## 第四批：缓存、并行、构建与门禁

### REV-13 【重要】生成器更新必须使缓存失效

- [x] 状态：完成（2026-10-21）。

- 完成：生成器内容指纹与缓存格式键；纳入有序源路径/全部依赖，读取失败及无来源 IR 禁用缓存；11 项缓存回归与实际换指纹 miss 验证通过；见 [第四批验收](rev13-19-validation.md)。

- 范围：`src/codegen/emit_core.cpp`、`emit_cache.*`、缓存测试。

- 修改：引入可靠的生成器/缓存格式修订标识；覆盖所有输出相关选项与输入依赖；不可读输入禁止命中；程序化 API 无 source_paths 时核查模型变化是否能失效。

- 验收：相同 ASN.1 更换生成器标识后 miss；选项、依赖、模型变化 miss；同输入稳定 hit；损坏/缺失输出重建。

- 依赖：无；影响输出修复的验证显式禁用旧缓存。

### REV-14 【重要】--parallel N 真正限制并发

- [x] 状态：完成（2026-10-21）。

- 完成：declare/resolve 有界批次与模块序合并；N=1/2/16、空输入/97任务峰值及诊断回归，CLI 严格数字解析；lower/tag 保持串行；见 [第四批验收](rev13-19-validation.md)。

- 范围：`src/semantic/analyzer.cpp`、CLI 数字解析。

- 修改：固定 worker 或有界调度；0 保持串行；诊断按模块索引合并；from_chars 完整解析 N；lower/tag 保持串行。

- 验收：并发峰值不超过 N；N=1/2/16、空模块、大量模块；诊断和产物与串行一致；1x/溢出被拒绝。

- 依赖：与 REV-11 对齐资源失败处理。

### REV-15 【重要】核清 JERI 定义与真实 12 入口

- [x] 状态：完成（2026-10-21）。

- 完成：补 JERI 产品位/CLI/生成函数，共享 X.697 JER 实现；核清非 canonical JSON，补 ARRAY 控制段并明确拒绝其余未支持控制段；见 [第四批验收](rev13-19-validation.md)。

- 范围：CLI、`include/asn1/codegen/emit.hpp`、JER/JERI runtime/IR、文档。

- 修改：核查 JERI 的项目约定及 X.697 指令语义；保留 12 产品入口目标；明确与 JER 的关系并补齐选择/生成路径；per 仅为组合别名。

- 验收：12 实际入口有能力登记，JERI 不再误称标准 canonical JSON；--codec jeri 有明确行为，未实现能力清楚报错；不为凑数量引入无标准依据的线路格式。

- 依赖：先完成语义核查，再做实现；若约定不明，记录需要用户决策的具体差异。

### REV-16 【重要】修正 codec 矩阵并增加真实链接调用

- [x] 状态：完成（2026-10-21）。

- 完成：7 fixture ×12 实际入口语法门禁 84/84；修正家族依赖，12 单入口及组合三形态真实调用/编码比较；修复 OER/XML 组合策略覆盖；见 [第四批验收](rev13-19-validation.md)。

- 范围：`tests/CMakeLists.txt`、门禁脚本、能力登记表。

- 修改：保留并自动化 7 fixture × 12 实际入口 -fsyntax-only；单入口使用正确 runtime 目标；代表 fixture 建可执行程序真实调用；补组合掩码。

- 验收：单独/组合中同 codec 输出一致；per 等于 uper,aper；默认、重复项、排列行为明确；缺失符号能被链接门禁发现。

- 依赖：REV-15；可先修已有 11 入口映射，不虚报完整 12。

### REV-17 【重要】CMake 编译器隔离与最低版本

- [x] 状态：完成（2026-10-21）。

- 完成：目标级编译器条件与 C11 属性；GCC/Clang 四轮各392/392；MSVC产品构建及40项回归；CMake3.16.8离线配置和代表门禁2/2；大型fixture保留全部sanitizer检查并优化调试信息/增量复制；见 [第四批验收](rev13-19-validation.md)。

- 范围：根/测试 CMake。

- 修改：target-scoped 编译器/平台条件；C runtime 与 smoke 设置 C_STANDARD=11/REQUIRED/EXTENSIONS OFF；处理 DOWNLOAD_EXTRACT_TIMESTAMP 版本条件；sanitizer 能力明确。

- 验收：GCC/Clang Release+ASan/UBSan，MSVC 支持范围构建，声明最低 CMake 版本离线配置；不能把 MSVC ASan 称为 UBSan。

- 依赖：无；无工具链时记录待补，不声称通过。

### REV-18 【优化】依赖离线与宿主/目标构建

- [x] 状态：完成（2026-10-21）。

- 完成：固定GTest下载SHA256，测试关闭/源码覆盖离线通过；宿主工具路径及两阶段流程；x86_64宿主生成、AArch64 C/C++链接通过，目标不在宿主执行；见 [第四批验收](rev13-19-validation.md)。

- 范围：CMake、usage-guide。

- 修改：GTest URL_HASH，验证离线覆盖及关闭测试路径；提供宿主 asn1cxx 路径或两阶段流程；交叉编译不默认执行目标测试。

- 验收：断网配置；宿主工具生成、目标编译器编译最小消费程序。

- 依赖：REV-17。

### REV-19 【优化】安装包与独立消费者

- [x] 状态：完成（2026-10-21）。

- 完成：安装导出/版本/组件与独立构建；GCC和MSVC各四类安装消费者通过；纯C无C++运行库依赖，私有GTest不进入SDK；见 [第四批验收](rev13-19-validation.md)。

- 范围：CMake、最小 C/C++ 消费者。

- 修改：install/export/config/version；runtime-only 和 compiler-only 消费；核查 transitive dependencies。

- 验收：安装后新目录 find_package 并真实链接运行；纯 C 消费不引入 C++ 标准库。

- 依赖：REV-17/18。

## 第五批：标准语义、结构策略与对验

### REV-20 【重要】建立标准能力及 oracle 矩阵

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：标准版本/层次/C 与 C++/oracle 矩阵，21 项实际接受或拒绝探针；未支持项明确登记。验收见 [第五批记录](rev20-24-validation.md)。

- 范围：新建能力矩阵文档、status/README 的引用。

- 修改：按 X.680/681/682/683、X.690/691/693/696/697 登记版本/勘误、语法、分析、IR、生成、runtime、独立验证；区分 C/C++；保留原任务延期状态。

- 验收：每个“支持”有代码或测试证据；未知标待核查，合法但未支持标 Unsupported；不把历史测试数字当本次运行结果。

- 依赖：REV-15；其余可提前盘点。

### REV-21 【重要】约束保真和高频语法专项核查

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：修复 REV-21a..g：未知约束、别名约束、负 SIZE、空交集、导入/循环常量、值参数及实例键、native INTEGER 解码窄化。验收见 [第五批记录](rev20-24-validation.md)。

- 范围：parser、constraints、semantic、IR、相关 emitter。

- 修改：追踪影响线路的未知约束，必须可靠归一化或报错；核查并交差集/FROM/WITH COMPONENTS、COMPONENTS OF、自动标签、参数化、对象集、关系约束、扩展组。

- 验收：每个核实缺陷拆成子编号（如 REV-21a），附最小 schema、实际/预期线路和修复；没有证据的结构不直接判缺失。

- 依赖：REV-20；本项不是一次实现整个 X.680 系列。

### REV-22 【重要】规范策略与 BER/CER 合法替代表示

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：修复 CER 原语规范策略、REAL、DEFAULT、重复 SET、片段/unused bits、长度/递归限制，三种产物真实回归；剩余能力见 STD-02。验收见 [第五批记录](rev20-24-validation.md)。

- 范围：BER/DER/CER、OER/COER、XER/CXER runtime 与结构 emitter。

- 修改：核查 DEFAULT、SET/SET OF、时间/REAL 词法、unused bits、最短形式、未知扩展、constructed/indefinite 路径。

- 验收：合法基础规则但非法规范规则的负例；CER 999/1000/1001、BIT STRING 分片及嵌套 EOC；单独/组合行为一致。

- 依赖：REV-20/21；按家族分别提交。

### REV-23 【重要】REAL profile 与字符/时间长尾类型

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：公开准确 REAL/字符/时间 profile；修复二进制精度/范围、负零、OER float 舍入及 JER bigint 误读；精确模型按 STD-03 延期。验收见 [第五批记录](rev20-24-validation.md)。

- 范围：REAL、StringKind、parser/IR、各 codec 能力文档。

- 修改：先公开准确 profile；核查 decimal/base2、特殊值及精度/范围行为；核查 OID-IRI、TIME/DATE 等及指令覆盖，明确拒绝尚未支持能力。

- 验收：合法但不支持不静默丢精度或改语义；需精确 REAL 模型时另立设计子项，保持零第三方运行时依赖。

- 依赖：REV-20；无需在此次修复批次强行实现所有长尾类型。

### REV-24 【重要】独立 oracle、负例与 fuzz

- [x] 状态：完成本批核查与修复（2026-10-21）；后续能力保持独立排期。

- 完成：新增带独立来源元数据的向量/冻结重放，复跑 Record 与 C PER 外部对验，新增种子回归和 Clang sanitizer 20000 次 fuzz。验收见 [第五批记录](rev20-24-validation.md)。

- 范围：tests、golden、scripts、development。

- 修改：记录 oracle 版本/commit/参数/schema/value/字节；区分规范字节相等与非唯一编码交叉解码；补 CER 条文负例、COER/XML/JER 对验；添加截断、资源限制和 fuzz harness。

- 验收：明确无外部 oracle 的空白；oracle 分歧有条文仲裁；C/C++ 双向与外部验证均可重跑；fuzz 种子回归进入门禁。

- 依赖：按 REV-05/06/20/22 修复进展逐步加入。

## 后续标准能力（独立排期）

第五批核查后的能力扩展继续单独排期（当前拒绝或受限，不作为完整标准能力）：

- [ ] STD-01：继承约束交集、FROM、普通 WITH COMPONENTS、COMPONENTS OF、数字版本扩展组；匿名类型实参及其约束的多实例键专项回归；BigInteger 与各家族 encode 侧完整约束验证。
- [ ] STD-02：时间规范词法、named BIT STRING 尾零、schema 感知 CER SET 顺序、COER/XML DEFAULT/未知扩展及 BER 构造替代形式。
- [ ] STD-03：精确 REAL 数值模型及 XML/JSON 可逆文本；当前 host-double profile 不作精确十进制承诺。
- [ ] STD-04：传统字符 repertoire、现代 TIME/DATE/IRI 类型和全部 encoding instructions。
- [ ] STD-05：独立 CER/COER/CXER/E-XER 指令 oracle、长期 fuzz、C11 更多入口与完整生成 PDU 差分。

证据与边界见 [第五批验收](rev20-24-validation.md)。这些能力项不替代下面 REV-25..28 的工程任务。

## 第六批：产物、分层与后端扩展

### REV-25 【重要】三种生成形态的 ODR、命名和输出可靠性

- [x] 状态：完成（2026-10-21）；三形态双 TU/线路相等、split SCC、可移植命名、stage/manifest、并发拒绝及归属清理已验证，见 [第六批验收](rev25-28-validation.md)。

- 范围：emit_core、emit_cache、CLI、生成产物测试。

- 修改：多 TU inline 链接；out-of-line 重复/遗漏符号；split 递归模块和名称碰撞；basename/namespace 校验；产物 manifest、依赖信息、临时写入和旧文件清理。

- 验收：三种形态实际调用并线路一致；失败不留下伪成功产物；Windows 文件名/路径有测试；不删除非本生成器拥有的文件。

- 依赖：REV-13/16；同目录并发策略先明确拒绝或隔离，不默默扩成复杂锁系统。

### REV-26 【优化】分层和公共头自动门禁

- [x] 状态：完成（2026-10-21）；include/实际传递链接正反例、88 个独立公共头编译及 compiler-only/runtime-only 构建通过，见 [第六批验收](rev25-28-validation.md)。

- 范围：CI/脚本、CMake、公共头。

- 修改：compiler 禁 include/link runtime，runtime 禁 include AST/IR/前端；公共头独立 C++17/C11 编译；生成产物明确为桥梁例外。

- 验收：负例可被门禁捕获；compiler-only/runtime-only 构建通过；不得误伤 emitter 输出字符串中的 runtime include。

- 依赖：REV-17。

### REV-27 【优化】C/C++ emitter 复用语义规划

- [x] 状态：完成共享规划复用验收（2026-10-24，CGEN 第 2 批）。真实生成 C 的描述符、selector 调用及生命周期检查消费标签、presence、约束和扩展组决定；C++ 线路回归通过。该历史验收不代表当时 C codec 已实现；后续 CGEN 第 4–7 批已完成声明 profile 的
  PDU/oracle、双 C 84 矩阵与原 C++ 84 矩阵，见 [CGEN 第 7 批验收](cgen-batch7-validation.md)。见 [规划设计](emission-planning.md)、[第六批验收](rev25-28-validation.md) 和 [CGEN 第 2 批验收](cgen-batch2-validation.md)。

- 范围：IR 到 codegen 的标签、presence、约束、扩展组及 selector 规划。

- 修改：先设计最小语言无关规划层，依赖 IR/common；C/C++ 后端只表达类型和调用；禁止链接 runtime；不为抽象而整体重写现有 emitter。

- 验收：代表 fixture 的 C++ 线路无回归；C 后端复用同一决策；与原 P2-4 sub-task 3+ 分步衔接。

- 依赖：REV-08/20/21；C emitter 大规模扩展前完成设计审查。

### REV-28 【重要】文档收口与 Phase49 继续条件

- [x] 状态：完成（2026-10-21）；统一桥梁/支持口径，修正旧功能规划与 C 头注释，补输出/ABI/所有权/线程安全/资源及许可证边界，更新项目 skill；见 [第六批验收](rev25-28-validation.md)。

- 范围：README、docs/README、architecture、status、usage-guide、development、todo、changelog、C 头注释。

- 修改：修正标准归属、支持口径及端序；统一生成产物桥梁表述；清理 todo 已落地方案的误导陈述；补所有权、ABI、线程安全、资源限额及发布许可证待办。

- 验收：文档数字对应实际测试报告；未复测不更新成功计数；已完成与未来设计分开；历史 changelog 不改写成未曾发生的结果。

- 依赖：每项随修同步，末尾汇总。C emitter 扩展前至少关闭 REV-01..07 的错误成功路径。

## 推荐交给 AI 的顺序

1. REV-01 → REV-02 → REV-03 → REV-04：先保证失败传播和位操作基础。
2. REV-05 → REV-06 → REV-07 → REV-08 → REV-09：修复 C PER 并关闭 primitive 门禁。
3. REV-10 → REV-11 → REV-12：兑现 Result 错误模型。
4. REV-13 → REV-14 → REV-15 → REV-16 → REV-17 → REV-18 → REV-19：缓存、并行及构建门禁。
5. REV-20..24：能力盘点后逐个修复标准语义，不把核查清单一次扩成全标准实现。
6. REV-25..28：产物可靠性、自动分层门禁、C 后端规划与文档收口；REV-28 始终随修更新。

REV-13/17 可在首批后提前处理，以减少旧缓存和工具链问题干扰验证。以上为执行建议，不自动启动任何修复。

## 每次复制给 AI 的任务模板

```text
请执行 docs/remediation-todo.md 的 REV-XX，只处理该项和必要的最小依赖。
先读取仓库 AGENTS.md、相关接口、实现、测试及当前 git diff；保留已有用户修改。
重新核实问题，给出最小复现或调用链证据，再修复；待核查内容不要直接当缺陷。
遵守文档中的分层、Result、纯 C11、共享 PER 核心、零第三方运行时依赖约束。
完成该项验收及规定门禁，记录实际命令/结果/未验证范围；缺少工具不能宣称通过。
同步相关 docs，并检查更新 docs/skills/asn1-remediation/SKILL.md 中对应的已验证流程。
同步本项状态及相关文档，不把未完成项目勾选，不扩大功能范围。
最终说明修改文件、修复行为、测试证据、剩余限制和建议下一项。
```

## 完成记录模板

```text
REV-XX：状态 / 日期
证据：最小输入、实际结果、预期结果或调用链
修改：文件与关键行为
验证：实际命令、通过/失败/未执行、工具链及 oracle 版本
限制：未支持/未验证内容
后续：依赖项或下一编号
```
