# 使用指南

面向使用方与集成方的操作手册：构建、CLI 参考、生成产物形态、上手工作流、
项目集成与常见问题。设计原理见 [architecture.md](architecture.md)。

## 环境要求

| 项 | 要求 |
|----|------|
| CMake | 3.16+ |
| 编译器 | C++17（GCC / Clang；MSVC 本地 Windows 工作可接受） |
| 网络 | 配置阶段一次（FetchContent 获取 GoogleTest） |
| 主要目标平台 | Linux / Ubuntu 24.04（GCC / Clang） |

## 构建

### 常规构建（Release）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # 预期 320/320 通过
```

产物：`build/asn1cxx`（编译器驱动）、各运行时静态库、31 个测试可执行。

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

## CLI 参考

用法：`asn1cxx [options] <file.asn>...`（支持多输入文件）。

### 选项

| 选项 | 说明 |
|------|------|
| `-h`, `--help` | 显示帮助并退出 |
| `-V`, `--version` | 显示版本并退出 |
| `--dump-tokens` | 对每个文件跑 lexer 并打印 token 流 |
| `--dump-ast` | 对每个文件解析并打印 AST |
| `--dump-ir` | 解析 + 语义分析并打印 Type IR |
| `--emit-dir <dir>` | 将生成产物写入 `<dir>` |
| `--codec <list>` | 选择编解码规则（见下），默认 `uper` |
| `--namespace <id>` | 生成类型的 C++ 命名空间，默认 `asn1_gen` |
| `--emit-inline` | 编解码函数定义 inline 进头文件（默认定义落 `generated.cpp`） |
| `--emit-split` | 按 ASN.1 模块分文件生成（`<Module>.hpp/.cpp` + 聚合头） |

未指定任何输出动作（dump / emit）时直接报错退出。

### 编解码规则（`--codec`）

| 别名 | 编码规则 | 运行时库 |
|------|----------|----------|
| `uper` | PER 非对齐（X.691） | `asn1_per` |
| `aper` | PER 对齐（X.691） | `asn1_per` |
| `per` | UPER + APER（双 Variant） | `asn1_per` |
| `ber` | BER（X.690） | `asn1_ber` |
| `der` | DER（X.690） | `asn1_der` |
| `cer` | CER（X.690） | `asn1_cer` |
| `oer` | OER（X.696 BASIC-OER） | `asn1_oer` |
| `coer` | COER（X.696 CANONICAL-OER） | `asn1_coer` |
| `xer` | XER（X.693 BASIC-XER） | `asn1_xer` |
| `cxer` | CXER（X.693 CANONICAL-XER） | `asn1_cxer` |
| `exer` | E-XER（X.693 扩展 XML + 编码指令） | `asn1_exer` |
| `jer` | JER（X.697 BASIC-JER） | `asn1_jer` |

组合语法（位掩码，asn1c / asn1tools 均为单选，本工具独有）：

```bash
# 逗号分隔组合
./build/asn1cxx --emit-dir build/gen --codec uper,ber examples/person.asn

# 重复 --codec 标志效果相同（按位或累加）
./build/asn1cxx --emit-dir build/gen --codec uper --codec ber examples/person.asn

# 全量组合
./build/asn1cxx --emit-dir build/gen \
  --codec uper,aper,per,ber,der,cer,oer,coer,jer,xer,exer,cxer examples/person.asn
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

| 形态 | 开关 | 产物 | 适用场景 |
|------|------|------|----------|
| out-of-line（默认） | 无 | `generated.hpp`（类型 + 函数声明）+ `generated.cpp`（定义，包在生成命名空间内） | 多 TU 集成（定义一次，其余 TU 只解析声明） |
| 全 inline | `--emit-inline` | 单 `generated.hpp`（定义 inline） | 单 TU 小模块，零链接步骤 |
| 分文件 | `--emit-split` | 每 ASN.1 模块一对 `<Module>.hpp/.cpp` + 聚合 `<basename>.hpp` | 大模块增量编译、IDE 索引 |

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
2. **链接运行时库**：按选用的 codec 链接对应静态库 —— `uper`/`aper`/`per` →
   `asn1_per`；`ber` → `asn1_ber`；`der` → `asn1_der`；`cer` → `asn1_cer`；
   `oer` → `asn1_oer`；`coer` → `asn1_coer`；`xer`/`cxer` → `asn1_xer` +
   `asn1_cxer`；`exer` → `asn1_exer`（并视产物 include 链接 `asn1_xer`）；
   `jer` → `asn1_jer`（JERI 指令产物加链 `asn1_jeri`）。所有运行时库依赖
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

| 目录 | 内容 |
|------|------|
| `common/` `lexer/` `parser/` `semantic/` `constraints/` | 编译器前端与语义测试 |
| `ber/` `der/` `cer/` `per/` `uper/` `aper/` `oer/` `coer/` `xer/` `cxer/` `exer/` `jer/` `jeri/` | 各编码规则单元测试 |
| `per/golden/` `tlv/golden/` | 金标准差分向量（外部 oracle 生成的精确字节） |
| `runtime/` | 运行时原语测试（bit_io / bigint 等） |
| `codegen/` | 代码生成测试（emit 产物往返、CLI 子进程、跨 codec 一致性、benchmark） |
| `telecom/` | 3GPP fixture 测试（RRC/LPP/S1AP 解析/分析/emit） |
| `fixtures/` | 测试用 ASN.1 文件（telecom/ 与 cer/） |

完整测试二进制清单与门禁纪律见 [development.md](development.md)。

## 示例文件

| 文件 | 内容 |
|------|------|
| `person.asn` | 简单 SEQUENCE（id INTEGER, name UTF8String, age INTEGER OPTIONAL），快速上手首选 |
| `smoke.asn` | 基础冒烟 |
| `ioc.asn` | 信息对象类（IOC）示例 |
| `phase12.asn` | 早期阶段构造示例 |
| `split_demo.asn` | 双模块示例（Demo_Base / Demo_App），配合 `--emit-split` |

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

**Q：电信 fixture 测试是否等于 3GPP 完整编解码对等？**
不等于。当前覆盖 parse + analyze + emit 编译门禁 + 字节级金标准差分向量，
协议级逐消息 PDU 的完整对验仍在路线图上（见 [status.md](status.md#剩余工作)）。

**Q：许可证？**
README 标注 TBD（计划开源，发布时补齐 LICENSE 文件）；源码文件头已标注
PROCODEC 版权 2026–2031。
