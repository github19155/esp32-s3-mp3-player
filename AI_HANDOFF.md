# AI_HANDOFF.md — 项目进度中枢

> 项目：ESP32-S3 多功能掌机
> 最后更新：2026-07-12
> Git: [github19155/esp32-s3-mp3-player](https://github.com/github19155/esp32-s3-mp3-player)
> 当前分支：`master`（核心基线）

---

## AI 协作规则

当前角色分工：
- Codex：负责规划、审查、风险判断，不直接写代码，除非用户明确要求。
- GLM/Zcode：负责实际编码、编译修复、功能实现。
- 所有 AI 每次完成任务后必须更新本文件。

开发规则：
1. **写完代码后只 commit，不要 push。等用户说"测试通过"或"push"后再 push。**
2. 当前分支：`master`（核心基线）
3. `feature/mp3` 保留 MP3 v0.1 稳定版本；master 只保留核心能力
4. 除非用户明确要求，任何 AI 不得自行合并分支、变基、回退或推送到 master
5. 禁止随意修改：`main/bsp/*`、`main/player/*`、`main/storage/*`、`components/*`、`partitions.csv`、`sdkconfig.defaults`
6. 每次写代码前先读：AI_HANDOFF.md、LONG_TERM_PLAN.md、HARDWARE_REFERENCE.md、README.md
7. 每次完成后必须更新：
   - 改了哪些文件
   - 编译是否通过
   - 是否烧录测试
   - 实机现象
   - 遗留问题
   - 下一步建议

---

## 当前任务卡

任务：**ESP32-S3 核心基线（master）**
状态：**编译及实机验证通过，固件 0.51 MB（524 KB），比原 6.73 MB 减少 92.2%**
当前分支：`master`

目标：
- ✅ 核心固件只保留 NVS、I2C、PCA9557、LCD、LVGL、触摸、事件总线、页面管理框架和最小主页
- ✅ 不初始化/不编译 SD 管理器、音频 codec、播放器核心、MP3 页面、WiFi 页面
- ✅ 不删除已有 MP3/WiFi 源文件，只从核心构建清单和启动流程排除
- ✅ 图标和中文大字库 font_alipuhui20 从编译清单移除（符号未链接）
- ✅ page_main_menu 改为轻量英文核心主页：ESP32-S3 CORE + Display + Touch Ready + Touch Test 计数器
- ✅ main.c 只初始化核心能力，日志为 "Core system ready"
- ✅ idf_component.yml 移除 esp-audio-player、esp-file-iterator、esp_codec_dev
- ✅ **编译通过**，mp3_player.bin = 536,608 bytes（524 KB），ELF = 8.24 MB
- ✅ **font_alipuhui20 未链接**（map 中 0 处引用）
- ✅ **player_core / sd_manager / page_mp3 / page_wifi 未链接**（map 中 0 处引用）
- ⚠️ **esp_codec_dev / bsp_codec 被链接**（BSP `esp32_s3_szp.c` 含有 codec 函数定义和 esp_codec_dev 引用 — BSP 文件不能修改，main.c 不调用故无害）

---

## 当前状态

| 模块 | 状态 | 备注 |
|------|------|------|
| BSP 驱动 | ✅ 完成 | v6.0 新版 I2C API |
| 事件总线 | ✅ 完成 | FreeRTOS 队列，UI 统一分发 |
| 页面管理器 | ✅ 完成 | 注册表 + 生命周期 + 标题栏/返回按钮 |
| 主菜单（核心版） | ✅ 改版 | 英文轻量主页，无图标/中文大字库 |
| NVS | ✅ 完成 | 核心基线已启用 |
| I2C + PCA9557 | ✅ 完成 | 核心基线已启用 |
| LCD + LVGL + 触摸 | ✅ 完成 | 核心基线已启用 |
| SD 卡管理 | ⏸ 排除 | 源码保留，不编译 |
| 播放器核心 | ⏸ 排除 | 源码保留，不编译 |
| 音频 codec | ⏸ 排除 | BSP 源码保留，不调用 bsp_codec_init |
| MP3 播放 UI | ⏸ 排除 | 源码保留，不编译 |
| 图标资源 | ⏸ 排除 | 源码保留，不编译 |
| 中文显示 | ⏸ 排除 | font_alipuhui20.c 保留，不编译 |
| 编译 | ✅ **通过** | **mp3_player.bin = 524 KB (0.51 MB)** |
| 符号排除 | ✅ **确认** | font_alipuhui20/player/sd/mp3/wifi 未链接 |
| esp_codec_dev | ⚠️ 被动链接 | BSP 引用 codec symbol，不调用无害 |
| WiFi | ⏸ 待迁移 | 现有 feature/wifi 基于旧 MP3 版本，需从 master 重新整理 |
| BLE | ⏸ 未开发 | 从 master 开功能分支 |
| 摄像头 | ⏸ 未开发 | 从 master 开功能分支 |
| 姿态传感器 | ⏸ 未开发 | 从 master 开功能分支 |

---

## 本轮修改（2026-07-12）

### 修改 1：AI_HANDOFF.md 合并冲突解决

- **文件**：`AI_HANDOFF.md`
- **目的**：解决 codex/core-base 分支 merge 冲突，任务卡更新为核心基线
- **影响范围**：仅文档

### 修改 2：核心基线构建清单

- **文件**：`main/CMakeLists.txt`
- **目的**：只编译核心源文件 — main.c, esp32_s3_szp.c, app_event.c, page_manager.c, page_main_menu.c
- **排除**：sd_manager.c, player_core.c, page_mp3.c, 所有图标 *.c, font_alipuhui20.c
- **影响范围**：编译产物，源文件不动

### 修改 3：移除 build 依赖

- **文件**：`main/idf_component.yml`
- **目的**：移除 chmorgan/esp-audio-player, chmorgan/esp-file-iterator, espressif/esp_codec_dev
- **保留**：lvgl, esp_lvgl_port, esp_lcd_touch_ft5x06
- **影响范围**：组件下载和编译依赖

### 修改 4：核心初始化流程

- **文件**：`main/main.c`
- **目的**：只初始化 NVS → I2C → PCA9557 → LCD/LVGL/Touch → 事件总线 → 页面管理器 → 最小主页
- **移除**：sd_manager_mount, bsp_codec_init, player_core_init, page_mp3_register
- **日志**：改为 "Core system ready"
- **影响范围**：运行时初始化

### 修改 5：页面管理器移除中文大字库依赖

- **文件**：`main/pages/page_manager.c`
- **目的**：标题栏改用 LVGL 内置 Montserrat 字体，删除 font_alipuhui20 声明
- **影响范围**：所有子页面标题栏渲染

### 修改 6：核心主页改版

- **文件**：`main/pages/page_main_menu.c`
- **目的**：改为轻量英文主页，显示 "ESP32-S3 CORE" + "Display + Touch Ready"，提供 Touch Test 控件
- **移除**：所有图标引用、font_alipuhui20、中文标签
- **影响范围**：主菜单 UI

### 修改 7：长期规划

- **文件**：`LONG_TERM_PLAN.md`（新建）
- **目的**：记录核心母版与 MP3、WiFi、BLE 等独立功能分支策略
- **影响范围**：仅文档

### 修改 8：README 更新

- **文件**：`README.md`
- **目的**：更新当前状态为核心基线，说明分支策略
- **影响范围**：仅文档

### 修改 9：事件总线解耦 player_core

- **文件**：`main/event/app_event.c`
- **目的**：移除 `extern player_core_on_event` 声明和调用，核心基线不编译播放器
- **影响范围**：播放器事件不再分发，功能分支需恢复

---

## 验证结果

| 项目 | 状态 | 说明 |
|------|------|------|
| 编译 | ✅ 通过 | ESP-IDF v6.0.1，fullclean 后重新构建通过 |
| 固件体积 | ✅ **524 KB** | 原 6.73 MB → 0.51 MB，减少 92.2% |
| font_alipuhui20 未链接 | ✅ 确认 | map 文件 0 处引用 |
| MP3/WiFi 符号未链接 | ✅ 确认 | player_core/sd_manager/page_mp3/page_wifi 0 处引用 |
| 烧录 | ✅ 通过 | 2026-07-12 用户完成实机烧录 |
| 实机主菜单显示 | ✅ 通过 | ESP32-S3 CORE + Display + Touch Ready 显示正常 |
| 触摸点击反馈 | ✅ 通过 | Touch Test 计数功能正常 |

---

## 风险/坑点

1. **ESP-IDF v6.0 不支持 Git Bash**，必须用 PowerShell 编译
2. **I2C 地址**：ES8311 控制字 `0x30`、ES7210 控制字 `0x82`，由 esp_codec_dev 内部右移（核心基线不启用以避免依赖）
3. **sdkconfig.defaults**：`LV_FONT_FMT_TXT_LARGE=y` 和 `SUPPRESS_DEPRECATE_WARN=y` 不能删
4. **components/ 下的 CMakeLists.txt**：补了 v6.0 所需 `esp_driver_*` 依赖，不要回退
5. **分区表 factory 8MB** 保留，不缩减
6. **MP3/WiFi 源文件保留**：`main/player/*`、`main/storage/*`、`main/pages/page_mp3.*`、图标资源、font_alipuhui20 等源文件不删除，只从 CMakeLists.txt 排除
7. **sdkconfig.defaults 不保证覆盖旧 build**：改完 sdkconfig.defaults 后必须删除 build 目录重新 set-target
8. **BSP 文件不动**：`main/bsp/*` 包含 codec 初始化函数，源码不删，main.c 不调用即可

---

## 下一步计划（按优先级）

1. **P1** — 从核心 `master` 独立创建或迁移 WiFi、BLE 等功能分支
2. **P1** — 多功能组合验证时另建 integration 分支，不把功能合回 master

---

## 已确认的分支原则

1. `master` 长期只保存核心能力。
2. `feature/mp3` 固定保存原 MP3 v0.1 稳定版本。
3. WiFi、BLE 等新功能从 `master` 独立分叉，不形成依赖链。
