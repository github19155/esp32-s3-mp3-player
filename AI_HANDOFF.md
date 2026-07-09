# AI_HANDOFF.md — 项目进度中枢

> 项目：ESP32-S3 MP3 多功能掌机
> 最后更新：2026-07-09
> Git: [github19155/esp32-s3-mp3-player](https://github.com/github19155/esp32-s3-mp3-player)
> 当前版本：v0.1（MP3 播放器冻结）

---

## 当前状态

| 模块 | 状态 | 备注 |
|------|------|------|
| BSP 驱动 | ✅ 完成 | v6.0 新版 I2C API |
| 事件总线 | ✅ 完成 | FreeRTOS 队列，UI/BLE 统一分发 |
| SD 卡管理 | ✅ 完成 | 优先 `/sdcard/music`，不存在则回退 `/sdcard`。先扫 mp3，没找到再扫 wav（按扩展名先后，非混合列表） |
| 播放器核心 | ✅ 完成 | 封装 esp-audio-player，纯逻辑，无 UI 依赖 |
| 页面管理器 | ✅ 完成 | 注册表 + 生命周期 + 标题栏/返回按钮 |
| 主菜单 | ✅ 完成 | 6 图标（3×2），仅 MP3 激活 |
| MP3 播放 UI | ✅ 完成 | 文件列表 + 播放/暂停/切歌/音量滑动条 |
| 中文显示 | ✅ 完成 | font_alipuhui20，LV_FONT_FMT_TXT_LARGE |
| 编译适配 | ✅ 完成 | ESP-IDF v6.0.1 |
| 分区表 | ✅ 完成 | factory 8MB + storage 3MB（对齐官方 14-handheld） |
| WiFi | ⏸ 未开发 | 页面枚举已预留 |
| BLE | ⏸ 未开发 | 页面枚举已预留 |
| 摄像头 | ⏸ 未开发 | CAMERA_EN=0，BSP 有 #error 阻止 |
| 姿态传感器 | ⏸ 未开发 | qmi8658 驱动保留，页面未启用 |
| AI_HANDOFF.md 维护 | ✅ 已建立规则 | 见本文档 |

---

## 本轮修改（2026-07-09）

### 修改 1：AI_HANDOFF.md 重构

- **文件**：`AI_HANDOFF.md`
- **目的**：按协作规则重写，固定章节格式
- **影响范围**：仅文档

### 修改 2：SD 卡扫描回退逻辑

- **文件**：`main/storage/sd_manager.c`
- **目的**：优先扫描 `/sdcard/music`，目录不存在或无匹配文件时回退到 `/sdcard` 根目录。提取 `scan_dir()` 内部函数消除重复代码。
- **影响范围**：MP3 播放器文件列表数据源

### 修改 3：LV_FONT_FMT_TXT_LARGE 强制生效

- **文件**：无 tracked 文件变更（`build/sdkconfig` 是 generated）
- **目的**：`sdkconfig.defaults` 里配置了 `CONFIG_LV_FONT_FMT_TXT_LARGE=y`，但旧 build 目录的 sdkconfig 没被覆盖。删除 build 目录重新 set-target + build + flash 后生效。
- **影响范围**：中文字体 font_alipuhui20 渲染

---

## 待验证

| 项目 | 状态 | 说明 |
|------|------|------|
| 编译 | ✅ 通过 | `idf.py build`，零错误零警告 |
| 烧录 | ✅ 通过 | COM4，ESP32-S3 正常运行 |
| 系统稳定性 | ✅ 通过 | 无重启，内存 DRAM 188K / PSRAM 8377K |
| 主菜单显示 | ✅ 通过 | 6 图标正常，中文正常 |
| MP3 播放 | ⚠️ 待实物 | 需插入含 `/music/*.mp3` 的 SD 卡测试 |
| 触摸点击 | ⚠️ 待实物 | 需触摸点击图标验证页面切换 |

---

## 风险/坑点

1. **ESP-IDF v6.0 不支持 Git Bash**，必须用 PowerShell 编译
2. **I2C 地址**：ES8311 控制字 `0x30`、ES7210 控制字 `0x82`，由 esp_codec_dev 内部右移，不要改
3. **功放**：走 `pa_en(1)`/`pa_en(0)`（PCA9557 BIT1），不是 ES8311 的 `.pa_pin`
4. **摄像头**：CAMERA_EN=0，BSP 有 `#error`，不能简单改宏启用
5. **分区表**：factory 8MB + storage 3MB，不要缩减
6. **sdkconfig.defaults**：`LV_FONT_FMT_TXT_LARGE=y` 和 `SUPPRESS_DEPRECATE_WARN=y` 不能删
7. **components/ 下的 CMakeLists.txt**：补了 v6.0 所需 `esp_driver_*` 依赖，不要回退
8. **MP3 播放器冻结为 v0.1**：不新增歌词、封面、ID3、EQ、播放模式等
9. **sdkconfig.defaults 不保证覆盖旧 build**：改完 sdkconfig.defaults 后必须删除 build 目录重新 set-target，否则旧 sdkconfig 会残留

---

## 下一步计划（按优先级）

1. **P0** — 插 SD 卡（含 `/music/*.mp3`）实物测试 MP3 播放
2. **P0** — WiFi 扫描 + 连接页面（参考 14-handheld，适配 page_manager）
3. **P0** — NTP 时间同步（主菜单显示时间）
4. **P1** — BLE 遥控（通过事件总线控制播放器）
5. **P1** — BOOT 按键接入事件总线

---

## 给 Codex 的问题

1. **WiFi 页面架构**：14-handheld 的 WiFi 代码（~800 行）全部塞在 `app_ui.c` 里，密码输入用 roller 滚轮。我们的架构里页面逻辑应该放在 `pages/page_wifi.c`，WiFi 连接管理单独放 `services/wifi_manager.c`。这个拆分方向对吗？还是先快速集成再重构？
2. **NTP 时间刷新**：官方例程用 `lv_timer` 每秒刷时间标签。我们的主菜单目前没有定时器基础设施。是在 `page_main_menu.c` 里直接创建 lv_timer，还是走事件总线用 FreeRTOS 定时器？
3. **BLE 遥控和播放器解耦**：当前 `player_core` 已经通过 `app_event` 接收事件。BLE 收到指令后直接 `app_event_post(APP_EVENT_NEXT, 0)` 即可，对吗？还是需要额外的线程安全保护？
