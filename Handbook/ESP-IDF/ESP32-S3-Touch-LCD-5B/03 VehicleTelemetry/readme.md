# 03 Vehicle Telemetry (LVGL)

![Framework](https://img.shields.io/badge/Framework-ESP--IDF%20%2B%20LVGL-orange)

A more complete vehicle telemetry dashboard built with **LVGL** (via `esp_lvgl_port`) on the same PSRAM-backed RGB panel — multiple gauges/widgets updating from LVGL's own task.

在同一个 PSRAM RGB 面板上，用 **LVGL**（经 `esp_lvgl_port`）实现的更完整的车辆遥测仪表盘 —— 多个仪表 / 控件由 LVGL 自身任务持续刷新。

## Pipeline / 流程

Same display bring-up as [02 Speedometer](../02%20Speedometer) — I2C → CH422G → RGB panel in PSRAM → `esp_lvgl_port` — then builds the telemetry dashboard UI and turns the backlight on. The panel self-refreshes via DMA; LVGL redraws from its own task.

显示初始化流程与 [02 Speedometer](../02%20Speedometer) 相同（I2C → CH422G → PSRAM RGB 面板 → `esp_lvgl_port`），随后构建遥测仪表盘 UI 并点亮背光。面板靠 DMA 自刷新，LVGL 在自己的任务里持续重绘。

## Build & flash

```bash
idf.py set-target esp32s3
idf.py build          # auto-fetches lvgl + esp_lvgl_port into managed_components/
idf.py -p PORT flash monitor
```

Shared hardware notes are in the [board readme](../readme.md).
