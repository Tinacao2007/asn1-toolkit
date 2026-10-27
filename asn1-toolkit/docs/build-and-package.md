# 构建与安装消费（REV-17/18/19）

## 原生与离线

最低声明 CMake 3.16，C++17、C11；测试门禁额外需要 Python 3 解释器。`DOWNLOAD_EXTRACT_TIMESTAMP` 仅在 CMake
3.24+ 传入。编译选项按项目 target 设置，GNU/Clang 的 UNIX 选项不传 MSVC。
MSVC 使用 `/utf-8`；C runtime 与 C 测试的 C_STANDARD=11、REQUIRED=YES、
EXTENSIONS=NO。`ASN1_ENABLE_SANITIZERS=ON` 表示 GCC/Clang 的 ASan+UBSan，
大型仅编译 fixture 在 sanitizer 构建使用 -g1 保留行号并减少调试类型图内存；
生产库和实际运行测试保留正常 Debug 信息与全部 sanitizer 检查。大型 fixture
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
暂不注册；其他单元测试和 codec 链接程序不因此关闭。

## 独立构建与安装

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

runtime-only SDK 可传 `--consumers c generated-c --host-compiler /host/bin/asn1cxx`，
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

## 两阶段宿主/目标流程

1. 宿主原生构建 compiler-only，安装宿主 `asn1cxx`。
2. 用目标 toolchain 配置 runtime-only，构建并安装目标库；该阶段不执行宿主之外的
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
本次命令结果及未执行范围见 [第四批验收记录](rev13-19-validation.md)。
