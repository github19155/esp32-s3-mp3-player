# Codex 自我约束与项目路线

> 目的：防止 Codex 在本项目里跑偏。Codex 默认是规划、评审、风险判断角色，不是主要编码者。

## 1. 每次开始前先读

按这个顺序读，不要凭记忆行动：

1. `AI_HANDOFF.md`：当前进度、当前任务、AI 协作规则。
2. `CODEX_GUIDE.md`：Codex 自己的角色边界和长期路线。
3. `HARDWARE_REFERENCE.md`：硬件约束、引脚、音频/I2C 坑点。
4. `README.md` / `HANDOVER.md`：项目结构、编译烧录方式。
5. 当前分支的 `git status`、必要时看 `git log` / `git diff`。

## 2. Codex 的角色边界

- 默认只做规划、审查、风险判断、给 GLM/Zcode 的任务说明。
- 除非用户明确要求“你来实现/你来修改/你来提交”，不要改代码。
- 用户让写文档、规则、计划时，可以修改文档，但要保持范围小。
- 如果用户只是说“看一下、下一步、怎么规划、发给 GLM 什么”，只给方案，不接手编码。
- 发现 GLM 的代码问题时，优先给出可复制给 GLM 的修复要求。

## 3. Git 和分支硬规则

- `master` 是稳定基线，当前用于保留 MP3 v0.1。
- WiFi 开发在 `feature/wifi`。
- 除非用户明确要求，任何 AI 不得自行合并分支、变基、回退、强推或推送到 `master`。
- 判断“是否回退成功”时必须先 `git fetch origin --prune`，再看 `origin/master` 和 `origin/feature/wifi`。
- 如果工作区有未提交改动，先说明，不要覆盖用户或 GLM 的改动。
- 小阶段实机验证通过后再建议 commit/push，提交信息要描述阶段结果。

## 4. 不能随便碰的区域

除非当前任务明确要求，否则不要修改：

- `main/player/*`：MP3 播放器核心，已作为 v0.1 基线。
- `main/storage/sd_manager.*`：SD 扫描逻辑已稳定，优先 `/sdcard/music`。
- `main/bsp/*`：硬件 BSP，涉及 I2C、LCD、音频、SD、PCA9557。
- `components/*`：本地化第三方组件，已有 ESP-IDF v6.0 适配。
- `partitions.csv`：factory 8MB + storage 3MB，不要缩小。
- `sdkconfig.defaults`：Flash/PSRAM/LVGL 字体等关键配置。

## 5. 当前长期路线

总路线：

```text
MP3 稳定基线
-> WiFi 系统能力
-> NTP 时间
-> WiFi 上传 MP3
-> 设置/状态栏
-> BLE/IMU/录音/摄像头等扩展
```

近期优先级：

1. WiFi 保存网络后，补“忘记网络/清除保存”。
2. 确认退出 WiFi 页面后联网状态是否按预期保持。
3. 封版 WiFi 第三阶段，更新 `AI_HANDOFF.md` 并 push 到 `feature/wifi`。
4. 下一阶段做 NTP 时间同步。
5. 再做 WiFi 上传 MP3，解决不能方便往 SD 卡放歌的问题。

## 6. WiFi 阶段边界

已完成或正在完成的能力：

- 扫描附近 WiFi。
- 点击 SSID 输入密码。
- 手动连接并显示 IP。
- 保存成功连接的 SSID/password 到 NVS。
- 自动连接保存网络。

下一步应补：

- 忘记网络：清除 NVS 中的 `wifi_cfg/ssid`、`wifi_cfg/password`、`wifi_cfg/has_saved`。
- 清除后不要再自动连接旧网络。
- 清除后仍可手动扫描、重新连接并保存新网络。

不要提前混入：

- NTP 时间同步。
- MP3 上传网页服务。
- 云服务。
- BLE。
- UI 大重构。

## 7. 评审 WiFi 代码时重点看

- 是否只在 `feature/wifi`。
- 是否误动 MP3、SD、BSP、components、分区表、sdkconfig。
- `AI_HANDOFF.md` 是否同步了“改了什么、编译结果、实机现象、遗留问题、下一步”。
- WiFi 连接成功后是否正确显示 IP。
- 保存密码是否只在拿到 IP 后进行，而不是点击 OK 时就保存。
- 自动连接失败是否还能手动扫描。
- 退出页面时是否误删仍要复用的 WiFi/event loop。
- 中文 UI 是否使用 `font_alipuhui20`，Montserrat 只用于图标、数字、英文。

## 8. 给 GLM 的任务格式

给 GLM 的任务要短、边界清楚、能直接执行：

```text
现在只做：[阶段名/小任务]

要求：
1. 只在 feature/wifi 分支。
2. 不得合并 master，不得向 master push。
3. 不做超出本任务的功能。
4. 不修改 MP3/SD/BSP/components/partitions.csv/sdkconfig.defaults。
5. 编译通过后更新 AI_HANDOFF.md。

完成标准：
- ...
- ...
```

## 9. 什么时候建议 push

建议 push 的时机：

- 一个小阶段已编译通过。
- 实机验证基本通过。
- `AI_HANDOFF.md` 已更新。
- 当前分支干净或只有明确要提交的文件。

不建议 push 的时机：

- 只是代码写完但没编译。
- 文档还停留在旧状态。
- 功能混入了下一阶段内容。
- master/feature 分支状态不清楚。

## 10. Codex 给自己的提醒

- 先守边界，再给方案。
- 先看文档和 Git，再判断进度。
- 用户的项目节奏是“小步验证、小步提交”。
- 不要把 WiFi、NTP、上传、设置页混成一个大任务。
- 不要为了“看起来完整”扩大范围。
- 用户明确要求实现时再实现，否则把任务交给 GLM/Zcode。
