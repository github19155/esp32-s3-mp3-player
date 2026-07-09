# AI_HANDOFF.md — 项目进度中枢

## AI 协作规则

当前角色分工：
- Codex：负责规划、审查、风险判断，不直接写代码，除非用户明确要求。
- GLM/Zcode：负责实际编码、编译修复、功能实现。
- 所有 AI 每次完成任务后必须更新本文件。

开发规则：
1. 当前分支：feature/wifi
2. master 保持 MP3 v0.1 稳定基线，不直接开发新功能
3. 除非用户明确要求，任何 AI 不得自行合并分支、变基、回退或推送到 master
4. 当前阶段优先完成 WiFi 扫描第一版的编译和实机验证
5. 禁止随意修改：main/player/*、main/storage/sd_manager.*、main/bsp/*、components/*、partitions.csv、sdkconfig.defaults
6. 每次写代码前先读：AI_HANDOFF.md、HARDWARE_REFERENCE.md、README.md
7. 每次完成后必须更新：
   - 改了哪些文件
   - 编译是否通过
   - 是否烧录测试
   - 实机现象
   - 遗留问题
   - 下一步建议

## 当前任务卡

~~任务：WiFi 第二阶段 — 连接功能~~ ✅ 已完成  
下一个任务：待定  
不要做：密码保存、自动连接、NTP、MP3 上传（暂未计划）  

完成标准：
- 编译通过
- 点击 SSID → 弹出密码输入（roller 滚轮 + textarea）
- 输入密码后点击 OK → 显示"连接中"
- 连接成功 → 显示 IP 地址
- 连接失败 → 显示"连接失败"
- 退出页面不崩溃
- 完成后更新 AI_HANDOFF.md

---

> 项目：ESP32-S3 MP3 多功能掌机
> 最后更新：2026-07-09
> Git: [github19155/esp32-s3-mp3-player](https://github.com/github19155/esp32-s3-mp3-player)
> 当前分支：`feature/wifi`（主分支 `master` 冻结为 v0.1 MP3 播放器）

---

## 当前状态

| 模块 | 状态 | 备注 |
|------|------|------|
| BSP 驱动 | ✅ 完成 | v6.0 新版 I2C API |
| 事件总线 | ✅ 完成 | FreeRTOS 队列，UI/BLE 统一分发 |
| SD 卡管理 | ✅ 完成 | 优先 `/sdcard/music`，回退 `/sdcard`。先 mp3 后 wav |
| 播放器核心 | ✅ 完成 | 封装 esp-audio-player，纯逻辑，无 UI 依赖 |
| 页面管理器 | ✅ 完成 | 注册表 + 生命周期 + 标题栏/返回按钮 |
| 主菜单 | ✅ 完成 | 6 图标（3×2），MP3 + WiFi 激活，其余灰显 |
| MP3 播放 UI | ✅ 完成 | 文件列表 + 播放/暂停/切歌/音量滑动条 |
| WiFi 扫描 + 连接 | ✅ 完成 | `feature/wifi`，扫描+密码输入+连接+显示IP，实机验证通过 |
| WiFi 连接 | ✅ 完成 | 连接+密码输入+显示IP |
| NTP 时间 | ⏸ 未实现 | 下阶段 |
| BLE | ⏸ 未开发 | 页面枚举已预留 |
| 摄像头 | ⏸ 未开发 | CAMERA_EN=0，BSP 有 #error 阻止 |
| 姿态传感器 | ⏸ 未开发 | qmi8658 驱动保留，页面未启用 |

---

## 本轮修改（2026-07-09）

### 修改 1：SD 卡扫描回退逻辑
- **文件**：`main/storage/sd_manager.c`
- **内容**：优先 `/sdcard/music`，目录不存在或无匹配文件时回退 `/sdcard` 根目录
- **状态**：✅ 编译通过，已烧录验证

### 修改 2：LV_FONT_FMT_TXT_LARGE 强制生效
- **文件**：无 tracked 文件变更（`build/sdkconfig` 是 generated）
- **内容**：删除 build 目录重新 set-target，使 `CONFIG_LV_FONT_FMT_TXT_LARGE=y` 真正生效
- **状态**：✅ 编译通过

### 修改 3：WiFi 扫描页面（第一阶段）
- **文件**：`main/pages/page_wifi.h`、`main/pages/page_wifi.c`、`main/main.c`、`main/pages/page_main_menu.c`、`main/CMakeLists.txt`
- **功能**：扫描附近 AP，显示 SSID + 信号强度百分比
- **状态**：✅ 编译通过，实机验证通过

### 修改 4：WiFi 连接功能（第二阶段）
- **文件**：`main/pages/page_wifi.c`
- **功能**：点击 SSID → 密码输入（textarea + 数字/大小写 roller）→ 连接 → 显示 IP 或"连接失败"
- **状态**：✅ 编译通过，实机验证通过（扫描/密码输入/连接/IP显示全流程）

---

## 待验证

| 项目 | 状态 | 说明 |
|------|------|------|
| page_wifi 全流程 | ✅ 通过 | 扫描/密码输入/连接/IP显示，退出不崩溃 |
| MP3 播放 | ⚠️ 待实物 | 需插入含 `/music/*.mp3` 的 SD 卡测试 |
| 中文显示 | ✅ 已验证 | font_alipuhui20 + LV_FONT_FMT_TXT_LARGE |

---

## 风险/坑点

1. **ESP-IDF v6.0 不支持 Git Bash**，必须用 PowerShell 编译
2. **I2C 地址**：ES8311 控制字 `0x30`、ES7210 控制字 `0x82`，不要改
3. **功放**：走 `pa_en(1)`/`pa_en(0)`（PCA9557 BIT1），不是 ES8311 的 `.pa_pin`
4. **分区表**：factory 8MB + storage 3MB，不要缩减
5. **sdkconfig.defaults**：`LV_FONT_FMT_TXT_LARGE=y` 和 `SUPPRESS_DEPRECATE_WARN=y` 不能删
6. **sdkconfig.defaults 不覆盖旧 build**：改完必须删除 build 目录重新 set-target
7. **components/ 下的 CMakeLists.txt**：补了 v6.0 所需 `esp_driver_*` 依赖，不要回退
8. **MP3 播放器冻结为 v0.1**：不新增歌词、封面、ID3、EQ、播放模式
9. **page_wifi.c 退出时释放 WiFi**：`page_wifi_on_exit()` 会 `esp_wifi_stop/deinit` 并删除 event loop。如果以后加了连接功能，退出逻辑需要改成不释放已连接的网络

---

## 下一步计划（按优先级）

1. **编译验证 page_wifi.c** — `idf.py build` 看有没有错误
2. **烧录测试 WiFi 扫描** — 实物点击 WiFi 图标，看扫描结果
3. WiFi 第二阶段：连接 + 密码输入
4. NTP 时间同步 → 主菜单显示时间

---

## 给新对话的上下文

1. 当前在 `feature/wifi` 分支，master 冻结
2. 上一轮刚写完 `main/pages/page_wifi.c`，还没编译
3. page_wifi.c 的设计：进入页面初始化 WiFi，退出页面释放 WiFi（简单干净，适合扫描阶段）
4. 如果要加 WiFi 连接功能，退出逻辑需要改：不能释放已连接的网络
5. 编译命令见 README.md 或 HANDOVER.md 的 PowerShell 脚本
6. 串口是 COM4 或 COM5

---

## 给 Codex 的问题

1. page_wifi.c 的 WiFi 初始化/释放逻辑是否合理？退出页面时释放所有 WiFi 资源，以后再连接时怎么保持？
2. 密码输入 UI 方案：继续用 14-handheld 的 roller 滚轮，还是简化为纯文本框 + 键盘？
