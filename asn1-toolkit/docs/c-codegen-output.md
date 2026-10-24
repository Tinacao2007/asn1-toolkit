# C11 类型产物、分片与构建集成（CGEN 第 3 批）

当前开放的是显式 **type/lifecycle-only** 入口；不生成任何 codec 函数或成功占位。
普通 `--lang c --codec uper,ber` 仍报 Unsupported。类型/值域与 DEFAULT 限制见
[C ABI](c-generated-abi.md)，验证范围见 [第 3 批验收](cgen-batch3-validation.md)。

## CLI 与三种形态

```sh
# Linux/WSL 示例；Windows 使用相应本机构建的 asn1cxx.exe。
./build-rev1-release/asn1cxx --lang c --types-only --c-prefix rrc \
  --split-max-types 128 --emit-dir build/gen-rrc-c \
  tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn
```

`--types-only` 要求 `--lang c` 和 `--emit-dir`，禁止同时指定 `--codec`，不会静默丢弃
编码请求。`--namespace` 与 C 模式冲突；`--c-prefix` 默认 `asn1_gen`，必须以 ASCII
字母开头，仅含字母、数字及单下划线，不能以下划线结尾。C++ 默认入口保持原行为。

| 选项 | 当前 C 产物 |
|---|---|
| 默认 | `generated.h`、`generated_internal.h`、`generated.c` |
| `--split-max-types N` | 同一公共/内部头，`generated_0001.c` 等 |
| `--emit-split` | 同一公共/内部头，按模块所有者生成 `c_m<escaped-module>.c` |
| `--emit-split --split-max-types N` | 按模块归属再分 `c_m<escaped-module>_0001.c` 等 |
| `--emit-inline` | 一个包含全部定义的 `generated.h`，没有伴随 C 源 |

每种形态还生成 `generated-sources.json`、`generated-sources.cmake` 和归属 manifest。
API 自定义 basename 会替换 `generated` 主文件 stem；固定 sidecar 名不改变。
公共头目前包含整个 schema 的完整类型，先保证跨模块递归/引用正确；没有宣称按模块拆头。

C 单头的所有函数（含深度辅助函数）均为 `static inline`，每个 TU 各自拥有内部定义；
不依赖 C99/C11 外部 inline 链接规则。C11 与 C++17 均可包含。不能依赖不同 TU/形态的
函数或描述符地址相等。inline 与模块 split 或非零 source budget 冲突并报错。
不同独立生成集使用不同前缀；同一生成集不混用不同形态或版本。

API 使用 `emit_generated_to_dir`，设置 `language=C`、`c_types_only=true`、`codec=None`；
按需求设置 `emit_inline`、`emit_split`、`split_max_types`。保留的 `emit_c_types` 双 stream
API 仍输出一个普通头/源，不负责目录事务。`CppGenerator` facade 始终只接受 C++。
注意继承的 `EmitOptions.emit_inline` API 默认值为 true，CLI 默认值为 false。

## 分片预算与依赖环

`--split-max-types N` 是正整数软预算，计量的是**可达 IR 类型节点**，包括匿名类型和
字段特定 view，不能等同 ASN.1 命名类型数量。API 的 0 表示不按数量分片；CLI 显式 0
非法。按限定类型名稳定排序，以实际存储/selector 依赖图的 SCC 为不可分单元。
SCC 跨模块时整体归属原始模块名按字节序最小的模块；不会复制一份生命周期实现。

每个函数仅定义一次。非 inline 形态的跨片 `_copy_at` / `_defaults_at` 通过生成内部头
声明、使用有前缀的外部链接符号；这些是内部实现接口，不提供独立稳定 ABI。
公共类型头保证 by-value 完整类型顺序；schema 递归使用既有受控指针布局。

超过 N 的 SCC 保持完整并发出 warning。单个类型或 SCC 无法借此任意拆开，类型数预算
不是字节上限。JSON 逐单元报告字节、类型和函数数；应据此检查巨大类型/单元，不能宣传
固定大小或无限可拆。当前每个类型有 7 个公开操作和 2 个深度辅助函数；后续新增 codec
必须同步统计和实际定义数验收。

## 清单、CMake 与产物事务

JSON format=1 记录语言、type-only、codec_mask=0、形态/预算、相对源/头集合和单元指标。
它是机器可读**当前源集合**。CMake sidecar 声明 `ASN1_GENERATED_SOURCES` 和
`ASN1_GENERATED_HEADERS`，路径相对 `CMAKE_CURRENT_LIST_DIR` 展开，可用于纯 C 工程：

```cmake
cmake_minimum_required(VERSION 3.16)
project(consumer LANGUAGES C)
find_package(asn1Toolkit REQUIRED COMPONENTS c_runtime)
set(gen "${CMAKE_CURRENT_SOURCE_DIR}/gen")
include("${gen}/generated-sources.cmake")
add_executable(app main.c ${ASN1_GENERATED_SOURCES})
set_target_properties(app PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_include_directories(app PRIVATE "${gen}")
target_link_libraries(app PRIVATE asn1::c_runtime)
```

先运行宿主生成器，再配置消费者；schema、模块、形态或预算改变后，重新生成并重新配置。
不能只 glob 目录内历史 `.c`。此清单不是 depfile，不自动为构建系统声明 ASN.1 输入依赖，
也不解决跨编译时宿主/目标程序执行区分；完整自动安装/交叉构建集成仍属 CGEN-22。

复用 REV-25 的同一 stage/backup/manifest 发布逻辑。manifest v1 兼容受限 `.h/.c`
和两个固定 sidecar 名，继续支持 C++ `.hpp/.cpp`；路径穿越和未知文件名拒绝。
同目录并发拒绝，成功前不发布半套文件；失败回滚和崩溃恢复限制见
[产物契约](generated-artifacts.md)。

减小片数、改变形态/语言会移除 hash 未变的旧归属产物。手改的过时文件、普通用户文件和
符号链接保持保护；手改旧源不进入新源清单。当前归属输出仍允许重新生成覆盖手工修改，
这与原 C++ 契约一致。清单损坏、未归属目标冲突、模型拒绝或 lock 均不能破坏旧产物。

缓存按稳定输出文件单元记录完整输入集合、输入顺序、生成器指纹、语言/type-only/codec、
前缀、basename、形态和预算。公共头覆盖整个模型，依赖集合采用保守的全部源输入。
无源路径或不可读依赖禁用复用；损坏缓存和输出 hash 不符重新生成。当前会完成规划/渲染
后验证已存产物并复用其字节，**不宣称跳过渲染 CPU 工作**。cache 不能替代 manifest。

## 验证边界

本批验证三种形态及 flat/module source shards 的多 TU 编译、链接、运行、递归/DEFAULT/
OOM 和源集合切换。LTE RRC 8.6.0 全文已有类型-only 多片编译链接和代表类型生命周期
调用；这不是 NR TS 38.331，也不是 LTE 全部 PDU 的编码/解码互操作证明。

生成 C 产品在非 sanitizer 构建中检查无 C++ 标准库依赖。GCC UBSan 工具自身会引入
libstdc++，不得把开发工具的依赖误报成产品混入 C++。完整 C PDU/oracle 闭环留在
CGEN-10..15 等后续任务，原有 C++ 线路/84 入口门禁继续保留。
