# ESP32-S3-Touch-LCD-5B 示例

**中文** | [English](./readme.en.md)

![Framework](https://img.shields.io/badge/Framework-ESP--IDF-orange)
![Board](https://img.shields.io/badge/Board-Waveshare%20ESP32--S3--Touch--LCD--5B-blue)
![Display](https://img.shields.io/badge/Display-1024%C3%97600%20RGB-success)

三个面向 **微雪 ESP32-S3-Touch-LCD-5B** 的 ESP-IDF 示例 —— 5 寸 **1024×600 RGB** 屏（ST7262 直驱），主控 ESP32-S3 WROOM-1-N16R8（16 MB Flash + 8 MB 八线 PSRAM）。

## 示例列表

| 编号 | 示例 | 技术栈 | 内容 |
| --- | --- | --- | --- |
| 01 | [HelloWorld](./01%20HelloWorld) | 纯 ESP-IDF | 直接往 RGB 帧缓冲画文字，**无第三方组件**。 |
| 02 | [Speedometer](./02%20Speedometer) | ESP-IDF + LVGL | 带指针动画的速度表。 |
| 03 | [VehicleTelemetry](./03%20VehicleTelemetry) | ESP-IDF + LVGL | 多仪表的车辆遥测仪表盘。 |

## 硬件要点

- **I2C 总线：** SDA = **GPIO8**，SCL = **GPIO9**（CH422G 扩展芯片、触摸、RTC 共用）。
- **CH422G I/O 扩展芯片：** 使能 EXIO 输出、复位屏幕与触摸、并**开关背光**。
- **RGB 面板：** 1024×600 RGB565；约 1.17 MB 的帧缓冲放在 **PSRAM** 里，由 DMA 自刷新。帧缓冲在 PSRAM 时，**PCLK 必须 ≤ 16 MHz**（21 MHz 会白屏）。

## 编译与烧录

所需 ESP-IDF **v5.2 及以上**。

```bash
cd "01 HelloWorld"            # 或 02 Speedometer / 03 VehicleTelemetry
idf.py set-target esp32s3     # 仅首次
idf.py build
idf.py -p PORT flash monitor  # PORT = /dev/cu.usbmodem* (macOS) / /dev/ttyUSB0 (Linux) / COMx (Windows)
```

> 首次编译时，LVGL 示例（02、03）会经 ESP-IDF Component Manager 自动拉取 `lvgl` 与 `esp_lvgl_port`（依赖在 `main/idf_component.yml` 声明，版本由 `dependencies.lock` 固定），无需手动配置。

## 配套讲解

📖 完整图文讲解发布在配套网站：_<TODO: 待补充文章链接>_
