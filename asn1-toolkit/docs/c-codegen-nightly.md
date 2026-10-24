# C 后端夜间批处理（本地提交，不推送）

此 runner 执行 [C 后端清单](c-codegen-todo.md) 的第 1～7 批。
每批由独立 `codex exec` 实现，父脚本执行固定门禁，全部通过后才本地 commit。
**没有 push、force push、自动合并或修改远程的步骤。** 早上由使用者审阅提交后决定推送。

## 准备与启动

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

## 固定推进条件

1. 开始新运行时整个仓库无未提交修改；Git 身份存在且不是 detached HEAD。
2. 同时只允许一个 runner；每个批次最多尝试 3 轮，模型失败/报告 blocked/验收失败都计入。
3. 每轮真实实现本批任务；返回结构化 JSON，并仅勾选本批和已完成前序任务。
   TODO 验收正文不得改写，进度与证据另存 docs；后续任务不得提前勾选。
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
独立 oracle、失败契约、安装/交叉等专项验收并提供证据；不能把所有清单都降为一个
`CGenBatchN` 测试。早上仍应审阅 diff、验收文档和未验证范围，再决定 push。

## 日志、暂停和恢复

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

## runner 自身测试

```powershell
python -X utf8 .\tests\scripts\test_nightly_c_codegen.py
```

在临时 Git 仓库验证门禁失败不暂存/提交、父目录修改拒绝、中文/删除路径、保护脚本、
提交 checkpoint、HEAD 漂移、互斥锁和提交后状态中断恢复。它不访问网络，不调用模型，
不替代 C 后端的真实验收。普通预检查也不等于验证了模型请求额度或整夜网络连通性。
