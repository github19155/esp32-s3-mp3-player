# LONG_TERM_PLAN.md — 长期开发计划

> 适用于：ESP32-S3 多功能掌机
> 最后更新：2026-07-12

---

## 分支策略

```
master (MP3 v0.1 稳定基线，冻结)
  └── codex/core-base (核心基线: NVS + I2C + PCA9557 + LCD + LVGL + Touch + EventBus + PageMgr)
        ├── feature/mp3  (从 core-base 恢复 MP3 播放功能)
        ├── feature/wifi (WiFi 扫描 + 连接 + NTP)
        ├── feature/ble  (BLE 遥控)
        ├── feature/camera
        └── feature/attitude (姿态传感器)
```

### 分支说明

| 分支 | 用途 | 状态 |
|------|------|------|
| `master` | MP3 v0.1 稳定基线，冻结不开发 | ✅ 已冻结 |
| `codex/core-base` | 核心基线 — 后续所有功能分支的母版 | ✅ 编译通过，待实机验证 |
| `feature/mp3` | 从 core-base 恢复 MP3 播放 | ⏸ 待创建 |
| `feature/wifi` | WiFi 扫描 + 连接 + NTP | ⏸ 待创建 |
| `feature/ble` | BLE 遥控 + OTA | ⏸ 待创建 |
| `feature/camera` | 摄像头预览 | ⏸ 待创建 |
| `feature/attitude` | 姿态传感器 QMI8658 | ⏸ 待创建 |

### 合并策略

1. 所有功能分支从 `codex/core-base` 创建（不直接从 master）
2. `codex/core-base` 只保存核心能力，不合入 MP3、WiFi、BLE 等业务功能
3. 多功能组合验证时另建 integration 分支，不污染核心母版
4. 禁止直接向 `master` 提交；发布策略由用户确认后再执行

---

## 核心基线（codex/core-base）

### 保留模块
- NVS（非易失性存储）
- I2C 总线（GPIO 1/2）
- PCA9557 IO 扩展（LCD_CS / PA_EN / DVP_PWDN）
- LCD 显示（ST77922, 320×240, SPI）
- LVGL 8.3 图形库（含 Montserrat 内置字体）
- 触摸驱动（FT5x06）
- 事件总线（FreeRTOS 队列）
- 页面管理器（注册表 + 生命周期）
- 最小主页（ESP32-S3 CORE + Display + Touch Ready + Touch Test）

### 排除模块（源码保留，构建不参与）
- SD 卡管理
- 音频编解码器（ES8311 + ES7210）
- MP3/WAV 播放器
- MP3 播放器 UI
- WiFi 页面
- 图标资源（6 个 img_*_icon）
- 中文大字库（font_alipuhui20）

---

## 里程碑

### v0.1 — MP3 播放器（master，已完成）
- [x] BSP 驱动
- [x] 事件总线
- [x] SD 卡扫描
- [x] MP3/WAV 播放/暂停/切歌/音量
- [x] 中文界面 + 图标主菜单

### v0.2 — 核心基线（codex/core-base，待实机验证）
- [x] 架构解耦：分离核心与非核心模块
- [x] 干净编译通过，固件 536,608 bytes（524KB）
- [ ] Touch Test 触摸验证

### v0.3 — WiFi（feature/wifi，计划）
- [ ] WiFi 扫描 + 列表
- [ ] WPA2 连接 + 密码输入
- [ ] NTP 时间同步
- [ ] 自动重连 + 保存凭据

### v0.4 — BLE（feature/ble，计划）
- [ ] BLE 外设模式
- [ ] 遥控播放器

### v1.0 — 全功能发布
- [ ] 所有模块稳定
- [ ] 合并到 master 发布
