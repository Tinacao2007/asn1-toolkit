# 使用指南

选用类型与编码规则前，查阅 [标准能力登记](status.md#standard-capabilities) 和
[REAL/字符/时间支持范围](status.md#real-and-string-profile)。生成成功、语法通过、
链接运行和外部互操作是不同证据；不支持的约束现在会在编译期报错。
导入常量和参数化值约束解析修正后，生成的宿主字段类型可能变窄或实例名变化；
升级生成器时应重新生成并重编所有消费代码，不混用旧生成头和新对象文件。

面向使用方与集成方的操作手册：构建、CLI 参考、生成产物形态、上手工作流、
项目集成与常见问题。设计原理见 [architecture.md](architecture.md)。

## 阅读导航

[构建](#构建) · [CLI 参数](#cli-参考) · [产物形态](#生成产物形态) · [构建安装/交叉编译](#build-and-package) · [C11 输出与分片](#c-output) · [夜间执行与恢复](#c-nightly)

## 环境要求

| 项      | 要求                                       |
| ------ | ---------------------------------------- |
| CMake  | 3.16+                                    |
| 编译器    | C++17（GCC / Clang；MSVC 本地 Windows 工作可接受） |
| 网络     | 配置阶段一次（FetchContent 获取 GoogleTest）       |
| 主要目标平台 | Linux / Ubuntu 24.04（GCC / Clang）        |

## 构建

### 常规构建（Release）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # 预期 320/320 通过
```

产物：`build/asn1cxx`（编译器驱动）、各运行时静态库和测试可执行。

### 诊断构建（ASan + UBSan）

开发与 CI 使用的第二门禁，捕获内存/生命周期与未定义行为问题：

```bash
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DASN1_ENABLE_SANITIZERS=ON
cmake --build build-san
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ASAN_OPTIONS=detect_leaks=1 \
  ctest --test-dir build-san --output-on-failure
```

### 离线构建

配置时需要一次网络访问获取 GoogleTest。离线环境可将 `FetchContent` 指向本地
GoogleTest 检出：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
```

若 sanitizer 构建报 "missing: FETCHCONTENT_SOURCE_DIR_GOOGLETEST"，用 Release
构建已抓取的副本：

```bash
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DASN1_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/repo/build/_deps/googletest-src
```

### 构建目录与发布版本

开发期间保留各 `build` / `build-*` 目录可见，保存不同工具链的 CMake 缓存、测试日志及外部工具环境；当前不移动、隐藏或清理。已恢复本次整理中设置过的隐藏属性。

到 release 时再使用独立、干净的构建整理最终版本和安装 SDK，记录配置、工具链、版本和验收结果；历史目录是否归档或清理另行核对，不直接搬动包含绝对路径的 CMake 缓存或 Python 虚拟环境。具体发布待办见 [REL-02](todo.md#rel-02)。

## CLI 参考

用法：`asn1cxx [options] <file.asn>...`（支持多输入文件）。

### 选项

| 选项                 | 说明                                         |
| ------------------ | ------------------------------------------ |
| `-h`, `--help`     | 显示帮助并退出                                    |
| `-V`, `--version`  | 显示版本并退出                                    |
| `--dump-tokens`    | 对每个文件跑 lexer 并打印 token 流                   |
| `--dump-ast`       | 对每个文件解析并打印 AST                             |
| `--dump-ir`        | 解析 + 语义分析并打印 Type IR                       |
| `--emit-dir <dir>` | 将生成产物写入 `<dir>`                            |
| `--codec <list>`   | 选择编解码规则（见下），默认 `uper`                      |
| `--namespace <id>` | 生成类型的 C++ 命名空间，默认 `asn1_gen`               |
| `--emit-inline`    | 编解码函数定义 inline 进头文件（默认定义落 `generated.cpp`） |
| `--emit-split`     | 按 ASN.1 模块分文件生成（`<Module>.hpp/.cpp` + 聚合头） |

未指定任何输出动作（dump / emit）时直接报错退出。

### 编解码规则（`--codec`）

| 别名     | 编码规则                                          | 运行时库                         |
| ------ | --------------------------------------------- | ---------------------------- |
| `uper` | PER 非对齐（X.691）                                | `asn1_per`                   |
| `aper` | PER 对齐（X.691）                                 | `asn1_per`                   |
| `per`  | UPER + APER（双 Variant）                        | `asn1_per`                   |
| `ber`  | BER（X.690）                                    | `asn1_ber`                   |
| `der`  | DER（X.690）                                    | `asn1_der`                   |
| `cer`  | CER（X.690）                                    | `asn1_cer`                   |
| `oer`  | OER（X.696 BASIC-OER）                          | `asn1_oer`                   |
| `coer` | COER（X.696 CANONICAL-OER）                     | `asn1_coer`                  |
| `xer`  | XER（X.693 BASIC-XER）                          | `asn1_xer`                   |
| `cxer` | CXER（X.693 CANONICAL-XER）                     | `asn1_cxer`                  |
| `exer` | E-XER（X.693 扩展 XML + 编码指令）                    | `asn1_exer`                  |
| `jer`  | JER（X.697，兼容已有指令感知行为）                         | `asn1_jeri`（传递链接 `asn1_jer`） |
| `jeri` | 项目 JER encoding-instruction 入口；同一种 X.697 线路语法 | `asn1_jeri`                  |

组合语法（位掩码；重复项去重，列表顺序不改变所选入口）：

```bash
# 逗号分隔组合
./build/asn1cxx --emit-dir build/gen --codec uper,ber examples/person.asn

# 重复 --codec 标志效果相同（按位或累加）
./build/asn1cxx --emit-dir build/gen --codec uper --codec ber examples/person.asn

# 全量组合
./build/asn1cxx --emit-dir build/gen \
  --codec uper,aper,ber,der,cer,oer,coer,jer,jeri,xer,exer,cxer examples/person.asn
```

列表中出现空段或未知名称会报错退出。

### 快速命令清单

```bash
./build/asn1cxx --dump-tokens examples/person.asn                       # token 流
./build/asn1cxx --dump-ast    examples/person.asn                       # AST
./build/asn1cxx --dump-ir     examples/person.asn                       # Type IR
./build/asn1cxx --dump-ir     tests/fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn
./build/asn1cxx --emit-dir build/gen --codec uper examples/person.asn   # 生成 UPER
./build/asn1cxx --emit-dir build/gen --codec jer  examples/person.asn   # 生成 JER
./build/asn1cxx --emit-dir build/gen --codec uper,ber examples/person.asn  # 组合生成
./build/asn1cxx --emit-dir build/gen --emit-split examples/split_demo.asn   # 分文件生成
```

## 生成产物形态

三种形态对比：

| 形态              | 开关              | 产物                                                        | 适用场景                      |
| --------------- | --------------- | --------------------------------------------------------- | ------------------------- |
| out-of-line（默认） | 无               | `generated.hpp`（类型 + 函数声明）+ `generated.cpp`（定义，包在生成命名空间内） | 多 TU 集成（定义一次，其余 TU 只解析声明） |
| 全 inline        | `--emit-inline` | 单 `generated.hpp`（定义 inline）                              | 单 TU 小模块，零链接步骤            |
| 分文件             | `--emit-split`  | 每 ASN.1 模块一对 `<Module>.hpp/.cpp` + 聚合 `<basename>.hpp`    | 大模块增量编译、IDE 索引            |

实测收益（RRC 全模块）：out-of-line 后头文件 33,697 → 5,998 行；消费 TU
`-fsyntax-only` ~22s → ~5s；定义 TU 一次性 ~63s（盈亏平衡 N≈3–4 个 TU）。
`--emit-split` 下 S1AP 拆为 3 个 emit 单元（含合并的 IEs/Containers/
PDU-Contents 递归环），互相递归模块按 SCC 合并进同一单元。

生成代码只做薄编排：类型定义 + 对运行时原语（如
`asn1::per::encode_constrained_whole_number`）的调用；编解码算法不在产物中。
`ASN.1 OPTIONAL → std::optional`、`DEFAULT → 普通值`、`SEQUENCE OF →
std::vector`、`OBJECT IDENTIFIER → asn1::ObjectIdentifier`、非约束 INTEGER →
`asn1::BigInteger`（类型映射全表见 architecture.md）。

## 上手工作流

以 `examples/person.asn` 为例的完整流程：

```bash
# 1. 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# 2. 检查解析与语义结果
./build/asn1cxx --dump-ir examples/person.asn

# 3. 生成编解码代码
./build/asn1cxx --emit-dir build/gen --codec uper examples/person.asn
# 产物：build/gen/generated.hpp + build/gen/generated.cpp

# 4. 验证产物可编译（emit 成功 ≠ 可编译，务必自查）
g++ -std=c++17 -fsyntax-only build/gen/generated.cpp \
    -Ibuild/gen -Iinclude 2>&1 | grep -c error   # 预期 0

# 5. 最小集成示例
cat > my_app.cpp <<'EOF'
#include "generated.hpp"
#include <asn1/runtime/per/codec.hpp>
int main() {
    asn1_gen::Person p{};
    // ... 填值后 encode_uper(p) / decode_uper(bytes)
}
EOF
g++ -std=c++17 my_app.cpp build/gen/generated.cpp \
    -Ibuild/gen -Iinclude -o my_app -lasn1_per -Lbuild
```

## 项目集成指引

1. **头文件路径**：把仓库 `include/` 加入 include 路径，生成产物目录也加入；
2. **链接运行时库**：按选用的编码规则链接对应静态库 —— `uper`/`aper`/`per` →
   `asn1_per`；`ber` → `asn1_ber`；`der` → `asn1_der`；`cer` → `asn1_cer`；
   `oer` → `asn1_oer`；`coer` → `asn1_coer`；`xer`/`cxer` → `asn1_xer` +
   `asn1_cxer`；`exer` → `asn1_exer`（并视产物 include 链接 `asn1_xer`）；
   `jer` / `jeri` → `asn1_jeri`（传递链接 `asn1_jer`，涵盖生成器使用的指令辅助函数）。所有运行时库依赖
   `asn1_runtime`；
3. **产物形态选择**：多 TU 项目用默认 out-of-line（或 `--emit-split` 按模块
   拆分）；单 TU 小模块用 `--emit-inline`；
4. **命名空间**：`--namespace` 指定（默认 `asn1_gen`）；`--emit-split` 下类型
   嵌套在 `<ns>::<Module>` 中；
5. **错误模型**：编解码返回 `Result<T>`，无异常传播（`std::bad_alloc` 除外）。

## 测试执行

```bash
ctest --test-dir build --output-on-failure      # 全部 320 项
ctest --test-dir build -N                        # 仅列出测试（不运行）
ctest --test-dir build -R Cli --output-on-failure  # 按名字过滤
```

测试目录结构：

| 目录                                                                                               | 内容                                              |
| ------------------------------------------------------------------------------------------------ | ----------------------------------------------- |
| `common/` `lexer/` `parser/` `semantic/` `constraints/`                                          | 编译器前端与语义测试                                      |
| `ber/` `der/` `cer/` `per/` `uper/` `aper/` `oer/` `coer/` `xer/` `cxer/` `exer/` `jer/` `jeri/` | 各编码规则单元测试                                       |
| `per/golden/` `tlv/golden/`                                                                      | 金标准差分向量（外部外部对照工具生成的精确字节）                      |
| `runtime/`                                                                                       | 运行时原语测试（bit_io / bigint 等）                      |
| `codegen/`                                                                                       | 代码生成测试（emit 产物往返、CLI 子进程、跨编码规则一致性、benchmark） |
| `telecom/`                                                                                       | 3GPP 测试样例测试（RRC/LPP/S1AP 解析/分析/emit）        |
| `fixtures/`                                                                                      | 测试用 ASN.1 文件（telecom/ 与 cer/）                   |

完整测试二进制清单与门禁纪律见 [development.md](development.md)。

## 示例文件

| 文件               | 内容                                                                    |
| ---------------- | --------------------------------------------------------------------- |
| `person.asn`     | 简单 SEQUENCE（id INTEGER, name UTF8String, age INTEGER OPTIONAL），快速上手首选 |
| `smoke.asn`      | 基础冒烟                                                                  |
| `ioc.asn`        | 信息对象类（IOC）示例                                                          |
| `phase12.asn`    | 早期阶段构造示例                                                              |
| `split_demo.asn` | 双模块示例（Demo_Base / Demo_App），配合 `--emit-split`                         |

## 常见问题

**Q：构建时报 FetchContent / GoogleTest 网络错误？**
配置阶段需要一次网络。离线构建见上文"离线构建"。

**Q：`--codec xer`（或 `exer` / `cxer`）单独选择时 emit 产物编译失败？**
已知的既有发射器 include 缺口：`emit_includes` 的 XML 分支未 include
`<asn1/runtime/byte_io.hpp>`（`make_error` 所在头），flat 与 split 模式同样
复现（非 split 引入）；全 12-codec 组合门禁会被其他分支带入该头而掩盖。
修复方案为 XML 分支补一行 include，当前状态与排期见
[status.md](status.md#剩余工作)。

**Q：emit 成功就代表生成代码正确吗？**
不代表。exit 0 只是"发射完成"。产物必须 `-fsyntax-only` 编译通过、跑通往返
测试、并通过金标准向量差分（位布局正确性）才算验证完毕 —— 这是本项目的
验证纪律，详见 [development.md](development.md#验证门禁纪律)。

**Q：电信测试样例测试是否等于 3GPP 完整编解码对等？**
不等于。当前覆盖 parse + analyze + emit 编译门禁 + 字节级金标准差分向量，
LPP/S1AP/LTE RRC 已增加代表顶层 PDU 的独立双向对验，范围见 [状态](status.md#protocol-interoperability)；不代表所有消息或 NR 全文通过。

**Q：许可证？**
README 标注 TBD（计划开源，发布时补齐 LICENSE 文件）；源码文件头已标注
PROCODEC 版权 2026–2031。

## 缓存与并发契约（REV-13/14）

缓存键含编译器实现/头文件内容指纹、工具链标识、格式版本、输出选项和全部
`source_paths` 内容及输入顺序。输入读取失败或产物丢失/被改写时重新生成。程序化 API
若不提供源路径就关闭缓存；提供时必须列全依赖，并保证 IR 对应这些源输入。
改变手工 IR 后不能沿用旧的源依赖声明。缓存不是 IR 的完整序列化。

`--parallel 0` / `1` 串行；`2..1024` 对 declare/resolve 使用不超过 N 个任务的
有界批次，按模块索引合并结果。lower/tag 仍串行。批次屏障可能使长短任务混合时
利用率低于固定线程池，但线程上界和诊断顺序明确；线程启动资源失败回退延迟执行。

## 离线、交叉构建和安装包

参见 [构建与安装消费](usage-guide.md#build-and-package)。原有三种产物形态均保留。
新增稳定入口 `encode_exer/decode_exer`；E-XER 选择且未选择 XER 时保留旧
`encode_xer/decode_xer` 兼容别名。组合中 XER、CXER、E-XER 各自生成，
OER 与 COER 也各自生成，不再由掩码里的规范策略覆盖同族入口。

## 输出目录与 SDK 集成（REV-25/28）

`--namespace` 必须是单个非保留 C++17 标识符；API basename 限制为可移植 stem。
每套 schema/选项使用独立输出目录；同目录并发调用报错。
切换形态或 basename 会清理 hash 未变的旧归属产物，手改的过时文件和用户文件保留。
`.asn1cxx-manifest` 登记本次生成的文件与源依赖；集成构建应使用其中 `.cpp` 清单，
而非 glob 目录中所有历史 `.cpp`。失败返回非零，不输出写入成功消息。
异常终止后的 stage/backup 恢复、inline 的多 TU 用法、ABI、线程安全及发布许可
见 [产物契约](architecture.md#generated-artifacts)。C 类型-only 的三形态/分片入口见
[C 输出与集成](usage-guide.md#c-output)；C 的 12 入口已开放声明的支持范围，详见 [第 6 批验收](changelog.md#cgen-validation--batch-6)。

## 目标语言与仅类型 C 输出

默认 `--lang cpp`；`--lang c --codec ...` 为十二入口的已支持值域/指令生成真实 C 编解码，不支持的类型图在发布前拒绝。`--lang c --types-only` 明确不生成编解码函数，只生成纯 C 类型与生命周期，支持普通、单头和模块/源分片。

`--split-max-types N` 是正数软 IR 类型预算，不是字节限制，依赖 SCC 保持完整。`--c-prefix` 仅用于 C；显式 `--namespace` 与 C 冲突。完整 CLI/CMake 示例、文件归属和缓存见 [C 输出](usage-guide.md#c-output)。前端转储可独立运行；类型生成本身不证明线路支持，生成 PDU 和外部互操作分别验收。

### C BER/DER/CER 与整份 RRC

支持的 C PER/TLV 支持范围可用 `--lang c --codec uper,ber --emit-split --split-max-types 128`
生成整份 RRC 的多个 `.c`；消费 `generated-sources.cmake`
登记的当前源文件并链接 `asn1::c_runtime`，不要将 `.c` 当作 C++ 编译。
单头模式使用 `--emit-inline`，与 split 参数互斥。
示例、真实 PDU/asn1c 对验和 LPP/S1AP 阻断见
[C 产物契约](usage-guide.md#c-output) 与 [第 5 批验收](changelog.md#cgen-validation--batch-5)。

### C OER 与 XML/JSON 家族

`--lang c --codec oer,coer,jer,jeri,xer,cxer,exer` 支持同样的三形态；
编译 `.c` 并链接 `asn1::c_runtime`。七个入口均使用字节读取器/写入器，
文本按 UTF-8 字节传入。失败保留输出值/游标或已有写入器内容。
支持范围、共享 NAME/TEXT 的语义、明确 Unsupported 的指令和独立对验见
[第 6 批证据](changelog.md#cgen-validation--batch-6)。最终 C 7 测试样例×12 语法与安装验收已由
[第 7 批](changelog.md#cgen-validation--batch-7) 完成；使用 ABI 3 的 runtime/生成头并一同重编译。

安装后的纯 C 多文件消费可用 `CONSUMER_KIND=generated-c`，指定
`ASN1_HOST_COMPILER` 为已构建的宿主生成器。自动清单规划与新增模块触发重配置的
示例见 [安装集成](usage-guide.md#build-and-package)。完整 C 语法门禁可执行
`cmake --build build --target asn1_c_codec_matrix`；拒绝的组合会返回失败，不能当作通过。


<a id="build-and-package"></a>

## 构建与安装消费（REV-17/18/19）


<a id="build-and-package--原生与离线"></a>

### 原生与离线

最低声明 CMake 3.16，C++17、C11；测试门禁额外需要 Python 3 解释器。`DOWNLOAD_EXTRACT_TIMESTAMP` 仅在 CMake
3.24+ 传入。编译选项按项目 target 设置，GNU/Clang 的 UNIX 选项不传 MSVC。
MSVC 使用 `/utf-8`；C 运行时与 C 测试的 C_STANDARD=11、REQUIRED=YES、
EXTENSIONS=NO。`ASN1_ENABLE_SANITIZERS=ON` 表示 GCC/Clang 的 ASan+UBSan，
大型仅编译测试样例在 sanitizer 构建使用 -g1 保留行号并减少调试类型图内存；
生产库和实际运行测试保留正常 Debug 信息与全部 sanitizer 检查。大型测试样例
先生成到测试暂存目录，再 copy_if_different 到编译目录；生成器更新仍重新生成，
产物字节未变时由 Ninja restat 复用已有对象，改变的产物照常重新编译。
在 MSVC 明确拒绝该组合；MSVC 的 ASan 不能当作 UBSan。

```sh
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/absolute/path/to/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build -j1
ctest --test-dir build --output-on-failure
```

GTest v1.14.0 zip 固定 SHA256
`1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4`。
测试关闭时不进入 FetchContent：`-DASN1_BUILD_TESTS=OFF`。仅设置 disconnected
而未提供已有源码，不等于已经拥有离线依赖。CLI 子进程测试依赖 POSIX，Windows
暂不注册；其他单元测试和编码规则链接程序不因此关闭。

<a id="build-and-package--独立构建与安装"></a>

### 独立构建与安装

```sh
# runtime-only（C++ 和纯 C，完全不构建 compiler）
cmake -S . -B build-runtime -DASN1_BUILD_TESTS=OFF -DASN1_BUILD_COMPILER=OFF
# compiler-only（不构建任一 runtime）
cmake -S . -B build-compiler -DASN1_BUILD_TESTS=OFF \
  -DASN1_BUILD_RUNTIME=OFF -DASN1_BUILD_C_RUNTIME=OFF
cmake --build build-runtime -j2
cmake --install build-runtime --prefix /absolute/path/to/sdk
```

完整构建默认同时包含三部分。开关不改变 compiler/runtime 链接方向。
安装导出 `asn1ToolkitConfig.cmake`、版本文件及 `asn1::` targets；版本兼容范围为
相同 major/minor 的 patch 更新，不承诺跨 minor 的 ABI 兼容。例如：

```cmake
find_package(asn1Toolkit 0.1 REQUIRED COMPONENTS per)
target_link_libraries(app PRIVATE asn1::per)
```

纯 C 项目只需 `project(app LANGUAGES C)` 与组件 `c_runtime`，链接
`asn1::c_runtime`，不依赖 common 的 C++ 编译特性或 C++ 标准库。
组件名对应 target 后缀：`frontend`、`semantic`、`codegen`、`per`、`c_runtime` 等；
请求未安装组件会失败。安装包不负责复制 GTest，运行时无第三方依赖。

[独立消费示例](../examples/package-consumers/CMakeLists.txt) 的 CONSUMER_KIND 支持
`c` / `runtime` / `compiler` / `generated` / `generated-c`。最后一种始终只启用 C，
用宿主生成器生成 UPER+BER 分片，通过两个 C TU 编解码。用全新目录验证安装：

```sh
python3 scripts/validate_installed_package.py --build build --output build/package-check
```

仅运行时 SDK 可传 `--consumers c generated-c --host-compiler /host/bin/asn1cxx`，
将已单独构建的宿主工具与目标 SDK 分开。Linux 验收同时检查两种 C 消费者不链接
libstdc++/libc++，并检查 CMake 没有启用 C++。

安装包提供配置期规划函数，适用于源文件集合随模块/分片变化的 C 产物：

```cmake
find_package(asn1Toolkit REQUIRED COMPONENTS c_runtime)
asn1_generate_c(generated
  GENERATOR "${ASN1_HOST_COMPILER}"
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated"
  CODECS uper,ber SCHEMAS schema.asn
  OPTIONS --emit-split --split-max-types 128)
add_executable(app main.c ${generated})
target_include_directories(app PRIVATE "${generated_INCLUDE_DIRECTORY}")
target_link_libraries(app PRIVATE asn1::c_runtime)
```

生成器必须已经构建且能在宿主执行；函数不会构建或运行目标生成器。
schema、生成器和清单声明的源/头都登记为 `CMAKE_CONFIGURE_DEPENDS`。新增类型/模块、
删除生成文件会在下次构建时重配置并重新读取清单。改变 schema 列表或选项应修改
CMakeLists 并重新配置；外部导入的 ASN.1 文件也应全部列入 SCHEMAS。
这是配置期生成机制，不能用于配置尚未完成时才产生的 ASN.1 输入。
不使用构建期 glob；同一输出目录只能由一处规划函数拥有。

<a id="build-and-package--两阶段宿主目标流程"></a>

### 两阶段宿主/目标流程

1. 宿主原生构建仅编译器，安装宿主 `asn1cxx`。
2. 用目标 toolchain 配置仅运行时，构建并安装目标库；该阶段不执行宿主之外的
   `asn1cxx`，也不执行目标单元测试。
3. 为目标应用配置示例 generated consumer：

```sh
cmake -S examples/package-consumers -B build-target-consumer \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/target.cmake -DCONSUMER_KIND=generated \
  -DCMAKE_PREFIX_PATH=/path/to/target-sdk \
  -DASN1_HOST_COMPILER=/path/to/host/bin/asn1cxx
cmake --build build-target-consumer
```

`ASN1_HOST_COMPILER` 必须是能在当前构建机运行的宿主程序。根项目生成门禁也可
用此变量覆盖工具路径及文件依赖。交叉配置默认 ASN1_BUILD_TESTS=OFF；若显式
开启则报错，要求另建原生 gate build。应用示例只在非交叉配置注册运行测试。
目标程序的真实执行要在设备或明确配置的模拟器上完成。

本批还实际验证了 x86_64 宿主工具与 Linux AArch64 目标 C/C++ 链接；目标程序未运行。
离线、原生包消费与交叉编译/链接不等于已验证某嵌入式设备 ABI。具体可用工具链、
本次命令结果及未执行范围见 [第四批验收记录](changelog.md#remediation-validation--rev-13-19)。


<a id="c-output"></a>

## 输出与构建集成


当前同时支持显式 **仅类型/生命周期** 入口和已验证的 C 编码规则支持范围。
`--lang c --codec uper,ber` 可用于整份 RRC；UPER/APER 及 BER/DER/CER 的值域与
限制见 [第 4 批](changelog.md#cgen-validation--batch-4) 和 [第 5 批](changelog.md#cgen-validation--batch-5)。
未支持的编码规则/类型在发布产物前报 Unsupported。三形态和模块内分片同时适用于
支持的编码规则请求；生成函数是真实编码/解码，不是成功占位。

<a id="c-output--cli-与三种形态"></a>


### CLI 与三种形态

```sh
# Linux/WSL 示例；Windows 使用相应本机构建的 asn1cxx.exe。
./build-rev1-release/asn1cxx --lang c --types-only --c-prefix rrc \
  --split-max-types 128 --emit-dir build/gen-rrc-c \
  tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn
```

`--types-only` 要求 `--lang c` 和 `--emit-dir`，禁止同时指定 `--codec`，不会静默丢弃
编码请求。`--namespace` 与 C 模式冲突；`--c-prefix` 默认 `asn1_gen`，必须以 ASCII
字母开头，仅含字母、数字及单下划线，不能以下划线结尾。C++ 默认入口保持原行为。

| 选项                                 | 当前 C 产物                                            |
| ---------------------------------- | -------------------------------------------------- |
| 默认                                 | `generated.h`、`generated_internal.h`、`generated.c` |
| `--split-max-types N`              | 同一公共/内部头，`generated_0001.c` 等                      |
| `--emit-split`                     | 同一公共/内部头，按模块所有者生成 `c_m<escaped-module>.c`          |
| `--emit-split --split-max-types N` | 按模块归属再分 `c_m<escaped-module>_0001.c` 等             |
| `--emit-inline`                    | 一个包含全部定义的 `generated.h`，没有伴随 C 源                   |

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

<a id="c-output--分片预算与依赖环"></a>


### 分片预算与依赖环

`--split-max-types N` 是正整数软预算，计量的是**可达 IR 类型节点**，包括匿名类型和
字段特定 view，不能等同 ASN.1 命名类型数量。API 的 0 表示不按数量分片；CLI 显式 0
非法。按限定类型名稳定排序，以实际存储/selector 依赖图的 SCC 为不可分单元。
SCC 跨模块时整体归属原始模块名按字节序最小的模块；不会复制一份生命周期实现。

每个函数仅定义一次。非 inline 形态的跨片 `_copy_at` / `_defaults_at` 通过生成内部头
声明、使用有前缀的外部链接符号；这些是内部实现接口，不提供独立稳定 ABI。
公共类型头保证 by-value 完整类型顺序；schema 递归使用既有受控指针布局。

超过 N 的 SCC 保持完整并发出 warning。单个类型或 SCC 无法借此任意拆开，类型数预算
不是字节上限。JSON 逐单元报告字节、类型和函数数；应据此检查巨大类型/单元，不能宣传
固定大小或无限可拆。当前每个类型有 7 个公开操作和 2 个深度辅助函数；后续新增编码规则
必须同步统计和实际定义数验收。

<a id="c-output--清单cmake-与产物事务"></a>


### 清单、CMake 与产物事务

JSON format=1 记录语言、仅类型、codec_mask、形态/预算、相对源/头集合和单元指标。
仅类型时 codec_mask=0；编解码产物记录所选择的入口掩码。
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
也不解决跨编译时宿主/目标程序执行区分。自动集成可使用安装包的
[`asn1_generate_c`](usage-guide.md#build-and-package)，在配置期执行已构建的宿主工具，读取受控
清单并登记重配置依赖；不执行目标程序。

复用 REV-25 的同一 stage/backup/manifest 发布逻辑。manifest v1 兼容受限 `.h/.c`
和两个固定 sidecar 名，继续支持 C++ `.hpp/.cpp`；路径穿越和未知文件名拒绝。
同目录并发拒绝，成功前不发布半套文件；失败回滚和崩溃恢复限制见
[产物契约](architecture.md#generated-artifacts)。

减小片数、改变形态/语言会移除 hash 未变的旧归属产物。手改的过时文件、普通用户文件和
符号链接保持保护；手改旧源不进入新源清单。当前归属输出仍允许重新生成覆盖手工修改，
这与原 C++ 契约一致。清单损坏、未归属目标冲突、模型拒绝或 lock 均不能破坏旧产物。

缓存按稳定输出文件单元记录完整输入集合、输入顺序、生成器指纹、语言/type-only/codec、
前缀、basename、形态和预算。公共头覆盖整个模型，依赖集合采用保守的全部源输入。
无源路径或不可读依赖禁用复用；损坏缓存和输出 hash 不符重新生成。当前会完成规划/渲染
后验证已存产物并复用其字节，**不宣称跳过渲染 CPU 工作**。cache 不能替代 manifest。

<a id="c-output--验证边界"></a>


### 验证边界

本批验证三种形态及 flat/module source shards 的多 TU 编译、链接、运行、递归/DEFAULT/
OOM 和源集合切换。LTE RRC 8.6.0 全文已有类型-only 多片编译链接和代表类型生命周期
调用；这不是 NR TS 38.331，也不是 LTE 全部 PDU 的编码/解码互操作证明。

生成 C 产品在非 sanitizer 构建中检查无 C++ 标准库依赖。GCC UBSan 工具自身会引入
libstdc++，不得把开发工具的依赖误报成产品混入 C++。完整 C PDU/oracle 闭环留在
CGEN-10..15 等后续任务，原有 C++ 线路/84 入口门禁继续保留。

<a id="c-output--整份-rrc-的-c-uperber"></a>


### 整份 RRC 的 C UPER+BER

```sh
./build/asn1cxx --lang c --codec uper,ber --emit-split --split-max-types 128 \
  --c-prefix rrc --emit-dir build/gen-rrc-c \
  tests/fixtures/telecom/rrc_8_6_0/EUTRA-RRC-Definitions.asn
```

编译 `generated-sources.cmake`/JSON 登记的所有 `.c`，包含 `generated.h`，
只链接 `asn1::c_runtime`（Unix 下还需数学库）。不要用目录 glob 混入旧分片。
生成 public BER decoder 对初始化的目标值和字节读取器游标一起提交；成功只消费
一个 TLV。完整实际运行及 asn1c 独立对验见第 5 批记录。

<a id="c-output--oerjsonxml-入口第-6-批"></a>


### OER、JSON/XML 入口（第 6 批）

```sh
asn1cxx --lang c --codec oer,coer,jer,jeri,xer,cxer,exer \
  --c-prefix app --emit-dir build/gen-c tests/fixtures/cgen_batch6.asn
```

三种产物形态都可使用。链接 `asn1::c_runtime`；JSON/XML 同样使用字节读取器/写入器，
按 UTF-8 字节传输。先初始化 decoder 输出，失败保留原值/cursor；编码成功才追加。
JER/JERI 共用指令语义；COER/CXER 每个函数独立选择策略。
TYPE/指令限制、资源限制和独立外部对照工具证据见
[第 6 批验收](changelog.md#cgen-validation--batch-6)，不能把 12 入口理解为标准全值域完成。



<a id="c-nightly"></a>

## 夜间执行与恢复

现有 CGEN 第 1～7 批已完成，原任务要求归档在变更历史。旧 runner 只读取该历史清单，不读取当前项目 TODO。以下保留 runner 的执行与恢复操作，仅供明确授权的历史批次维护使用，不要求重新执行已完成任务，也不应取消完成标记来启动旧计划。旧运行状态保存了验收正文与受保护脚本指纹，不保证跨本次文档迁移直接恢复；保留原日志并核对当前清单，不改状态哈希绕过保护。


此 runner 执行 [C 后端清单](changelog.md#cgen-tasks) 的第 1～7 批。
每批由独立 `codex exec` 实现，父脚本执行固定门禁，全部通过后才本地 commit。
**没有 push、force push、自动合并或修改远程的步骤。** 早上由使用者审阅提交后决定推送。

<a id="c-nightly--准备与启动"></a>


### 准备与启动

从正常登录用户的 Windows PowerShell 启动；应用工具沙箱内可能无法访问用户凭据。
CLI 自动从 PATH 或 `%LOCALAPPDATA%/OpenAI/Codex/bin/*/codex.exe` 查找，也可指定
`--codex`。复用本机 CLI 的登录与默认模型，不复制 token，不修改全局配置。

启动前：保存其他编辑，现有修改单独提交，确认整个 Git 仓库干净（当前 Git 根在项目父目录）。
不要让另一个构建或另一个 AI 会话同时修改本 checkout 或共用以下构建目录。
电脑须保持开机、不休眠、终端与网络可用；CLI/账号限额、认证、策略拒绝等仍可能使执行停止。
脚本不会修改电源设置、关闭沙箱或绕过自动审批拒绝。

```powershell
cd D:\H\ACT\asn1-toolkit

# 只读预检查：登录、工具、分支、Git 身份、未提交修改、WSL/asn1c、离线 GTest
python -X utf8 .\scripts\nightly_c_codegen.py

# 开始串行执行第1～7批，预算10小时；每批只做本地提交
python -X utf8 .\scripts\nightly_c_codegen.py --run --through-batch 7 --max-hours 10
```

也可使用 PowerShell 包装器：`& .\scripts\run-c-codegen-nightly.ps1 -Run -ThroughBatch 7 -MaxHours 10`。
如系统脚本执行策略不允许 `.ps1`，直接使用上面的 Python 命令，不必修改系统执行策略。
首次可用 `--through-batch 1` 试跑一批；完成后 `--run --resume --through-batch 7` 继续。

`--max-hours` 在新批次/尝试开始前检查，预算到期不开始新的尝试；它不会强杀当前
Codex、构建或测试，因此不是精确的墙钟截止时间。复杂批次不承诺一夜完成。

<a id="c-nightly--固定推进条件"></a>


### 固定推进条件

1. 开始新运行时整个仓库无未提交修改；Git 身份存在且不是 detached HEAD。
2. 同时只允许一个 runner；每个批次最多尝试 3 轮，模型失败/报告 blocked/验收失败都计入。
3. 每轮真实实现本批任务；返回结构化 JSON，并仅勾选本批和已完成前序任务。
   TODO 验收正文不得改写，进度与证据追加到 `docs/changelog.md` 的对应批次；后续任务不得提前勾选。
4. 注册有意义的 `CGenBatchN.*` CTest（N 是批号），不能用文字匹配或恒真测试冒充。
5. 父脚本独立配置/构建 `build-rev1-release` 与 `build-rev1-san`（单编译任务，显式打开
   compiler/C++ runtime/C runtime/tests），跑全量 CTest、ASan+UBSan、原有 7×12 C++
   语法矩阵和分层检查；旧测试名称不能消失。通过后把新增测试也纳入后续基线。
   首批要求已存在的 Release 构建可列出至少 442 个既有测试；这不是新的成功计数。
6. HEAD/分支保持原值，修改路径全部在本项目内；runner/包装器/runner 测试及现有
   C++ matrix 脚本不可被本次 AI 修改。精确暂存修改文件，不对父仓库执行无范围的 `git add .`。
7. commit 写入 `CGen-Batch` 和 `CGen-Run` trailer；保存本地 commit SHA 后再开始下一批。
   父脚本负责提交，模型不得自行 commit、push、改分支、reset、clean 或 stash。

runner 检查可执行门禁与范围，不能机械证明语义覆盖完整。模型仍须执行每批清单中的
独立外部对照工具、失败契约、安装/交叉等专项验收并提供证据；不能把所有清单都降为一个
`CGenBatchN` 测试。早上仍应审阅 diff、验收文档和未验证范围，再决定 push。

<a id="c-nightly--日志暂停和恢复"></a>


### 日志、暂停和恢复

状态与日志都在被忽略的 `build-rev-cgen-nightly/`：

```text
state.json                          # 当前阶段、已完成批次、HEAD、测试基线
runner.lock                         # 运行互斥；不会自动抢占
batch-N/<UTC时间>-attempt-M/
  prompt.txt
  agent.jsonl / agent.stderr.log
  result.json
  release-build.log / release-test.log
  san-build.log / san-test.log
  matrix.log / layering.log
  commit-message.txt
```

模型事件/日志可能包含源码，不要把整个日志目录当公开报告上传。主状态用临时文件替换，
commit 前写 checkpoint；若提交成功而状态写回被中断，恢复时核对父 SHA、tree 和
trailers，识别那次提交而不重复提交。

```powershell
# 失败或预算停止后，查看状态与对应日志；只在确认无其他 runner 时恢复
Get-Content .\build-rev-cgen-nightly\state.json
python -X utf8 .\scripts\nightly_c_codegen.py --run --resume --through-batch 7 --max-hours 10
```

失败的本批保留工作区，**不提交、不清空、不回滚其他修改**；已通过批次的 commit 保留。
3 轮用完会停止，显式 `--resume` 代表使用者允许新一轮最多 3 次尝试。
发现意外 HEAD/分支或项目外修改、受保护脚本变更时停止，不能自动强行修复 Git 历史。
强杀后可能残留 `runner.lock`：先确认 CLI/构建进程确实停止，再处理锁；不要直接重开并行 runner。
Git hooks/签名如果要求人工输入也可能阻塞，首次准备提交需验证本地提交链路可用。

<a id="c-nightly--runner-自身测试"></a>


### runner 自身测试

```powershell
python -X utf8 .\tests\scripts\test_nightly_c_codegen.py
```

在临时 Git 仓库验证门禁失败不暂存/提交、父目录修改拒绝、中文/删除路径、保护脚本、
提交 checkpoint、HEAD 漂移、互斥锁和提交后状态中断恢复。它不访问网络，不调用模型，
不替代 C 后端的真实验收。普通预检查也不等于验证了模型请求额度或整夜网络连通性。

## 电信 PDU 外部对验环境

常规构建离线运行冻结向量，无需安装外部工具。需要现场重新编译完整模式、三方核验时，在独立测试环境安装固定版本：

```sh
python3 -m venv build/protocol-oracle-env
build/protocol-oracle-env/bin/python -m pip install -r tests/telecom/oracle-requirements.txt
cmake -S . -B build -DASN1_PROTOCOL_ORACLE_PYTHON="$PWD/build/protocol-oracle-env/bin/python"
cmake --build build --target asn1_test_executables -j2
ctest --test-dir build -R '^Protocol\.' --output-on-failure
```

上述为 Linux/WSL 命令；Windows Python 使用环境内 `Scripts/python.exe`。asn1c 应在 PATH 中，非 MSVC 测试工具链找到它后会自动加入 LPP/LTE UPER/BER 对验；它不提供此次 S1AP APER 金标准。不要混用 Windows 和 WSL 的 Python/编译器路径。关闭现场门禁可将缓存参数设为空字符串。

固定版本仅为测试依赖，不随运行时链接或分发。现场门禁失败时保留 `tests/protocol-live/` 的生成物与日志；不能删掉失败向量再声明通过。准确数据来源、网络规则、工具覆盖和 NR 边界见 [状态](status.md#protocol-interoperability)，执行纪律见 [开发指南](development.md#protocol-interoperability-tests)。
