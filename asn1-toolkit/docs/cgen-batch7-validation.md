# CGEN 第 7 批验收（完成）

CGEN-20..24 / M2 已完成登记范围的工程验收。没有删减七个原 fixture 或任何 codec，
不将 preflight 拒绝计为通过。整部 ASN.1 标准和所有 3GPP PDU 的 wire 认证不在本结论中。

## 实现

- named BIT STRING：PER/COER 采用满足有效 SIZE 的最短长度；DER/CER/CXER 移除全部
  尾零；固定长度按规则补零。解码还原为满足 SIZE 的等价值，普通 BIT STRING 长度不变。
- C UTCTime：校验 Gregorian 日期、闰年、时分秒及 +/-hhmm，按 DER/CER/PER/COER/CXER
  要求输出 YYMMDDhhmmssZ。PER 走 unconstrained VisibleString，时间 SIZE 不作为 PER-visible。
  OER 同样忽略时间 SIZE（X.696 8.4.4），始终保留长度字段；新增 OER/COER SIZE(13) 回归。
  时间 profile 为 1950–2049；归一化跨出该窗口明确拒绝，不依赖进程 locale/时区状态。
- 不连续整数根：X.691 13.1 扩展位按根的数值 RANGE 判断，空洞不缩小范围；非扩展约束
  仍精确检查成员。不把根集合裁成第一段，也不维护第二份 APER 核心。
- 表约束：已知 JSON/XML open 字段用生成的 immutable selector/getter/offset 编解码实际
  contained value；JSON 即使 governor 最后出现也能解码。XML 使用实际类型元素包装。
  自动拥有并深拷贝/释放 selected value；未知 key/非法 payload/类型不匹配返回明确状态。
  空 PrivateIE 对象集保留 opaque PER 内容，不虚构整数 governor。
- 补齐 arbitrary-precision INTEGER DEFAULT 比较、缺省还原和资源记账，含负 2^128。
  失败时保持公共输出对象，byte/text 入口同时保持游标/writer 已提交内容。
- SDK 配置期 `asn1_generate_c` 读取受控清单；新增模块、缺失源会触发重配置。
  共用生成产物使用单一 producer 与消费者依赖，保留发布锁；新增全部测试 executable 聚合目标。
- 生成 ABI / C runtime ABI 为 **3**：私有 descriptor 扩展和空 selector 布局变化要求重新
  生成、重编译 runtime 和消费者；生成头检查 runtime header 版本。项目版本仍为 0.1.0，
  不承诺跨 CRT/平台或不同 SDK 的二进制兼容；现有专有许可声明不改变。

## 最终门禁

| 门禁 | 实际结果 |
|---|---|
| GCC 13.3 严格 C11，原 7 fixture ×12 | **84/84** |
| Clang 18.1.3 严格 C11，同一矩阵 | **84/84** |
| 原 C++17 GCC 7×12 syntax 门禁 | **84/84** |
| MSVC 19.44 Release 完整 CTest | **476/476** |
| GCC ASan+UBSan 完整 CTest | **512/512** |
| 新增 named/time/sparse/typed 功能 | 三形态各 48 个原语编解码案例 + 5 个 typed JSON/XML 案例、双 TU |
| INTEGER DEFAULT | 三形态，每形态十二入口、默认/非默认两组、双 TU |
| 新增失败门禁 | typed open / named bits 逐次分配失败；未知 key、越界 payload、错误 NULL、非规范 DER |
| Clang libFuzzer + ASan+UBSan | 十二生成入口 PDU harness，10,000 次，max_len=4096，无崩溃 |
| SDK 安装消费 | Windows 五类消费者全部通过；Linux C/runtime-only SDK 的 c 与 generated-c 通过 |
| 纯 C 链接边界 | Linux 两种 C 消费者未启用 C++，ldd 不含 libstdc++/libc++ |
| 分层与公共头 | C/C++ include/link 正反例；25 个 C 头各 C11/C++17、49 个 C++17 头通过 |
| 独立构建 | compiler-only 与纯 C runtime-only 构建通过；前者不链接任何 runtime |
| 跨构建执行保护 | Generic cross probe 编译/链接通过且不注册目标运行测试；不代表设备 ABI 验收 |

矩阵由同一生成器执行，前后 SHA-256 一致；详见本地 `build-rev7-work/final-matrices.json`。
syntax、运行、外部 oracle 与 fuzz 分别记录；测试数量不是标准覆盖率。
MSVC 本轮是 Release /std:c11 验收，没有把它称作 ASan+UBSan，也未声称执行 MSVC ASan。
Clang 执行的是严格 C11/runtime/header/feature 与插桩 fuzz，不冒充完整 Clang C++ 回归。

## 独立线路证据

- 新增 asn1tools 0.169.0 **21** 个独立相等线路及值解码，冻结在
  `tests/standards/cgen7-asn1tools-vectors.json`，由三形态 feature gate 离线回放。
- asn1c 0.9.29 独立核对 INTEGER(1..3|8|15,...) 的 UPER 根范围：
  1→00、3→10、4→18、8→38、14→68、15→70（5 bits）；16→808800（17 bits）。
  冻结根向量及非扩展空洞负例进入 `CGenBatch7.BigintRoot`。
- 原连续根的正负 2^128 等十二个 UPER/APER 独立 asn1tools 向量继续回放。
  原 72 组外部冻结 oracle 和 whole-RRC/asn1c UPER+BER 门禁均继续通过。
- asn1tools 的部分 UNION 根处理和 DER named-bit shortening 存在缺口，未用它覆盖这些
  结论；没有宣称它提供 CER/COER/CXER 规范编码 oracle。标准向量、外部验证分别记账。

## 重复执行

```sh
cmake --build build --target asn1_test_executables
ctest --test-dir build --output-on-failure
python3 scripts/validate_c_codec_matrix.py --compiler build/asn1cxx --cc gcc --output build/c-gcc
python3 scripts/validate_c_codec_matrix.py --compiler build/asn1cxx --cc clang --output build/c-clang
python3 scripts/validate_codec_matrix.py --compiler build/asn1cxx --cxx g++ --output build/cpp
python3 scripts/validate_installed_package.py --build build --output build/fresh-package-check
```

本地完整日志位于忽略的 `build-rev7-work/`。维护的 CTest/脚本与冻结向量已纳入仓库。

## 仍然明确的支持边界

GeneralizedTime、未登记字符 repertoire、E-XER UNTAGGED 等原有 profile 限制仍保留。
JSON/XML 未知 selector 或缺少 contained-type schema 的 standalone open value 明确
Unsupported，不以 hex 占位冒充 typed decode。BER/OER 仍使用 opaque 完整编码；仅有
selected view 而无适用内容时明确拒绝。全模块 syntax 成功不等于每个 LPP/S1AP/NR PDU
都完成独立 wire 认证；字段路径诊断和完整未知扩展保留也不由本批推定。

标准依据：X.680 22.6/22.7/47、X.690 11.2/11.8、X.691 10.6.5/13.1/16.2–16.3、
X.696 13/31.6/31.11、X.693 9.3/9.11/9.12、X.697 24/41。


标准原文：[X.691 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items)、
[X.696 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.696-202102-I!!PDF-E&lang=e&type=items)、
[X.693 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.693-202102-I!!PDF-E&lang=e&type=items)、
[X.697 (2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.697-202102-I!!PDF-E&lang=e&type=items)。
