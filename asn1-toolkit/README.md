# asn1-toolkit

C++17 ASN.1 编译器与编解码运行时：将 ASN.1 规范（X.680 系列）编译为 C++17 值类型与
编解码代码，并提供 12 个产品 codec 入口及运行时原语库。JERI 是项目的 JER 指令入口，
与 JER 共享 X.697 线路语法；12 个入口不表示 12 种独立标准线路格式。

| 项目    | 值                                                                                                                                                                                     |
| ----- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 当前阶段 | Phase 49 — C11 PER/TLV profile、三形态/模块内分片可用；整份 RRC 的 C UPER+BER PDU 已通过 asn1c 对验，完整标准值域仍有明确限制。见 [状态](docs/status.md) |
| 测试状态 | MSVC Release **461/461**；GCC ASan+UBSan **494/494**；7×12 实际入口语法矩阵 **84/84**。见 [CGEN 第 5 批验收](docs/cgen-batch5-validation.md) |
| 编码入口  | BER / DER / CER / UPER / APER / OER / COER / XER / CXER / E-XER / JER / JERI                                                                                                          |
| 语言/构建 | C++17（GCC / Clang；MSVC 本地可用）+ CMake 3.16+                                                                                                                                             |
| 依赖    | 零第三方运行时依赖（BigInt 自研，无 Boost、无 multiprecision）                                                                                                                                         |

显式 `--lang c --types-only` 可生成纯 C11 类型/生命周期及模块内分片；C PER/TLV 的已声明 profile 可生成真实编解码，其他家族仍待续。
用法见 [C 输出与集成](docs/c-codegen-output.md)。

## 特性

| 能力         | 说明                                                                                                                         |
| ---------- | -------------------------------------------------------------------------------------------------------------------------- |
| 编码入口    | 12 个产品入口（能力边界见 [入口登记](docs/codec-entries.md)）；UPER/APER 共享同一 PER 实现（`Variant::{Unaligned,Aligned}` 参数化，非两份代码）；DER=BER+规范策略、COER=OER+规范策略、CXER=XER+规范策略      |
| 语法与类型子集    | CLASS / 信息对象 / 表约束开放类型 / 参数化 / 无编号 `[[...]]` / INSTANCE OF / EXTERNAL / EMBEDDED PDV / REAL / >64 位 INTEGER；各层证据、Unsupported 与 oracle 空白见 [标准能力登记](docs/standard-capabilities.md) |
| 多编码规则组合生成  | `--codec uper,ber` 位掩码组合一次生成多套编解码代码                                                                                        |
| 字节级金标准差分验证 | PER/APER（asn1c oracle，54 向量）、BER/DER/OER（asn1tools + asn1c oracle，16 用例）、CER（X.690 条文推导 + DER 值等价，10 用例）                   |
| 三种生成产物形态   | 默认 out-of-line（声明在头、定义在 `.cpp`）、`--emit-inline` 全 inline 单头、`--emit-split` 按 ASN.1 模块分文件                                   |
| 编译门禁       | 7 个 fixture × 12 codec 全量 emit 产物 `-fsyntax-only` 零错误（emit 成功 ≠ 可编译）                                                       |
| 无异常错误模型    | 业务错误用 `Result<T>` / 编译诊断（仅 `std::bad_alloc` 可传播）；源范围与线路偏移单位见 [错误契约](docs/cpp-error-contracts.md)                                                           |
| 严格分层       | 编译器链（lexer/parser/IR/codegen）不链接运行时；运行时不包含 AST/IR 头；生成产物是唯一桥梁（inline 模式为生成 `.hpp`）                                                       |

## 环境要求

- CMake 3.16+
- C++17 编译器（GCC 或 Clang；MSVC 本地 Windows 工作可接受）
- 配置阶段需要一次网络访问（FetchContent 获取 GoogleTest）；离线构建方式见
  [docs/usage-guide.md](docs/usage-guide.md#离线构建)

主要目标平台：Linux / Ubuntu 24.04（GCC / Clang）。

## 快速开始

```bash
# 1. 构建（Release）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. 运行全部测试（当前结果见 docs/status.md）
ctest --test-dir build --output-on-failure

# 3. 解析一个 ASN.1 文件并查看 IR
./build/asn1cxx --dump-ir examples/person.asn

# 4. 生成 UPER 编解码 C++ 代码
./build/asn1cxx --emit-dir build/gen --codec uper examples/person.asn
# 产物：build/gen/generated.hpp（类型声明）+ build/gen/generated.cpp（编解码定义）
```

诊断构建（ASan + UBSan，CI 同款门禁）：

```bash
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DASN1_ENABLE_SANITIZERS=ON
cmake --build build-san
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ASAN_OPTIONS=detect_leaks=1 \
  ctest --test-dir build-san --output-on-failure
```

完整的 CLI 参考、产物形态对比与集成指引见 [docs/usage-guide.md](docs/usage-guide.md)。

## 仓库结构

```text
asn1-toolkit/
  CMakeLists.txt          构建系统（默认 20 个静态库 + asn1cxx 可执行 + 测试/门禁目标）
  tools/asn1cxx/          编译器驱动（CLI）
  include/asn1/           公共接口（common / support / ast / frontend / semantic /
                           constraints / ir / codegen / runtime/*）
  src/                    实现（与 include 镜像；编解码算法只在 runtime/ 下）
  tests/                  分领域测试可执行 + 7 个 fixture + 金标准向量
  examples/               示例 .asn 文件
  scripts/                版权头维护、金标准向量生成脚本
  docs/                   项目文档（入口见 docs/README.md）
```

## 文档

| 文档                                           | 内容                                  |
| -------------------------------------------- | ----------------------------------- |
| [docs/README.md](docs/README.md)             | 文档中心：索引与按角色阅读路径                     |
| [docs/architecture.md](docs/architecture.md) | 架构设计：流水线、分层铁律、目录与 CMake 目标、编解码抽象    |
| [docs/usage-guide.md](docs/usage-guide.md)   | 使用指南：构建、CLI 参考、产物形态、集成指引            |
| [docs/development.md](docs/development.md)   | 开发指南：代码规范、测试体系、验证门禁纪律、文档同步约定        |
| [docs/status.md](docs/status.md)             | 项目状态：构建/门禁/金标准验证结果、剩余工作、已知限制        |
| [docs/changelog.md](docs/changelog.md)       | 变更历史：Phase 1–45 里程碑与近期变更详录          |
| [docs/comparison.md](docs/comparison.md)     | 横向对比：asn1c / Python asn1tools / 本方案 |
| [docs/analysis/](docs/analysis/README.md)    | 历史分析归档（快照，仅供追溯）                     |

## 参考树

父工作区 `/mnt/f/ASN_1/` 下的 `pasn1/`、`ASN1C/`、`Python_asn1tools/`、`FFASN1dump/`
等为参考材料，不属于本构建（`pasn1` 的"解析器与生成器混合"是本项目刻意避免的反面模式，
详见 [docs/architecture.md](docs/architecture.md#核心架构决策)）。

## 许可证

TBD —— 计划开源，许可证文件将在项目发布时补齐（源码文件头已标注 PROCODEC 版权
2026–2031，由 `scripts/add_file_headers.py` 自动维护）。

CGEN batch 1 adds `--lang cpp|c` (default cpp) and C-only `--c-prefix`.
C emission currently fails explicitly with Unsupported and publishes no artifacts.
See [C ABI design](docs/c-generated-abi.md), [runtime gap audit](docs/c-runtime-codegen-gaps.md)
and [stage evidence](docs/cgen-batch1-validation.md).
