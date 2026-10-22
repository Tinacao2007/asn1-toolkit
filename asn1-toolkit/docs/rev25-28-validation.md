# REV-25..28 第六批验收

日期：2026-10-21。环境：Windows PowerShell / WSL Ubuntu 24.04，GCC 13.3，
CMake 3.28.3；Windows MSVC 19.44 的针对性结果另列。

## 修改与证据

| 项目 | 现象与修改 | 验收边界 |
|---|---|---|
| REV-25 | 旧目录发射直接截断目标文件；现改为 stage 生成、hash manifest、最后发布完成标记，失败回滚，拒绝同目录并发 | 三形态双 TU；12 入口线路相等；split SCC；名称/归属/损坏 manifest/符号链接/形态切换 |
| REV-26 | 仅凭约定无法阻止新增依赖；增加源码 include 检查、CMake 实际传递链接检查和独立公共头 CTest | 正反例包括 helper 中转越界、emitter include 字符串合法；组件独立构建 |
| REV-27 | C emitter 尚未实现；先建立只依赖 IR 的借用规划，接入 C++ PER 根位图/扩展数与整数成员约束 | 本批设计与 C++ 阶段完成；真实 C emitter 接入验收仍依赖 P2-4 sub-task 3+ |
| REV-28 | 修正仅 `.cpp` 是桥梁、过期计数/缓存设计、C++20 span/errno、C 原语即完整后端等表述 | 对应契约、README/索引/架构/状态/使用/开发/TODO/skill 同步；历史日志保留 |

详细规则见 [生成产物](generated-artifacts.md) 与 [共享规划](emission-planning.md)。
既有“换 basename 留旧文件”与“通过符号链接写 `/dev/full` 才发现失败”的测试预期
已按新契约更新：前者清理未修改旧归属，后者在覆盖前拒绝非普通文件，保留符号链接
且 CLI 不报告 `wrote`。没有删除用户文件以让门禁通过。

## 实际命令与结果

| 门禁 | 最终结果 |
|---|---|
| GCC Release 全量 CTest | **442/442** |
| GCC Debug ASan+UBSan 全量 CTest | **442/442**；leak detection/halt_on_error 开启 |
| 7 fixture ×12 实际入口语法矩阵 | **84/84**；语法检查不冒充链接或互操作 |
| manifest 独立审计 | 84 个 manifest、168 个产物 SHA256 和 84 个源依赖 SHA256 逐项相等 |
| 三种生成形态 | 各自双 TU 链接并实际 UPER 调用；12 入口跨形态线路相等测试通过 |
| split 模块依赖环 | `SplitModules.DependencyCycleMergesUnitAndCallsBothNamespaces` 通过 |
| 公共头 | **48/48 C++17**；**20 个 C 头 × C11/C++17 = 40/40**；合计 88 个独立编译 |
| 自动分层 | 123 个源码/头；字面 include 正反例及 CMake 直接/间接越界负例通过 |
| 独立组件 | compiler-only、runtime-only（含纯 C）分别配置和构建通过 |
| 安装包 | 纯 C、runtime、compiler、宿主生成四类消费者通过；纯 C 无 C++ 标准库依赖 |
| MSVC 19.44 Release | 针对性 **37/37**；含名称、空格/中文 API 目录与三形态调用 |
| 项目 skill/文档 | quick_validate 通过；本批相对文件链接检查通过 |

门禁日志在忽略目录 `build-rev1-quick/batch6-*.log`，不将临时构建产物提交。
Release 最后又在独立组件/安装检查前刷新一次，避免新增路径测试只存在于源码。
分层扫描补入 CLI 路径后，另复跑 sanitizer 的 `Architecture.Layering` 与 Windows 脚本正反例；
该变更只扩展静态检查范围，不改变编译产物。

独立组件命令：

```sh
cmake -S . -B build-rev1-quick/components/compiler -DASN1_BUILD_TESTS=OFF   -DASN1_BUILD_COMPILER=ON -DASN1_BUILD_RUNTIME=OFF -DASN1_BUILD_C_RUNTIME=OFF
cmake --build build-rev1-quick/components/compiler -j1
cmake -S . -B build-rev1-quick/components/runtime -DASN1_BUILD_TESTS=OFF   -DASN1_BUILD_COMPILER=OFF -DASN1_BUILD_RUNTIME=ON -DASN1_BUILD_C_RUNTIME=ON
cmake --build build-rev1-quick/components/runtime -j1
python3 scripts/validate_installed_package.py --build build-rev1-release   --output build-rev1-quick/batch6-package
```

MSVC：`cmake --build build-rev4-msvc --config Release --parallel 1 --target` 后接
`asn1_emit_cache_tests asn1_standard_outline_tests asn1_standard_inline_tests
asn1_standard_split_tests asn1_split_cycle_tests`；再运行
`ctest --test-dir build-rev4-msvc -C Release --output-on-failure -R
'EmitCache|EmitOutput|EmissionPlanning|GeneratedPolicy|SplitModules|Architecture.Layering'`。


```sh
cmake -S . -B build-rev1-release
cmake --build build-rev1-release -j1
ctest --test-dir build-rev1-release --output-on-failure
cmake -S . -B build-rev1-san
cmake --build build-rev1-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-rev1-san --output-on-failure
python3 scripts/validate_codec_matrix.py --compiler build-rev1-release/asn1cxx \
  --cxx g++ --output build-rev1-quick/batch6-matrix
python3 scripts/validate_layering.py --cmake cmake
```

## 未宣称的能力

- 不宣称 C emitter、`--lang c` 或生成 C PDU 已完成；REV-27 的 C 消费验收保留未完成。
- 输出事务是正常进程下的失败保护，不是全目录瞬间原子替换/断电恢复协议。
  未故障注入真实磁盘故障或断电；回滚失败会保留 stage/backup 路径供恢复。
- MSVC 只跑列出的针对性测试；不冒充 Windows 全 fixture 或 ASan+UBSan。
- include 扫描只覆盖字面 include 指令；新宏 include/特殊 include 根需扩充门禁。
  CMake 检查实际 target 依赖，但不能证明外部预编译库没有隐藏行为。
- 本批未新增独立 ASN.1 oracle。既有冻结向量与 12 入口比较属于全量 CTest，
  不将自身往返/线路相等冒充新的外部标准认证；第五批 live oracle/fuzz 记录不改写。
- 未复跑最低 CMake、AArch64 设备或长时间 fuzz；历史结果只在原记录中保留。
- 许可证/完整 ABI 政策、统一解码预算与能力矩阵 STD-01..05 仍是后续工作。
