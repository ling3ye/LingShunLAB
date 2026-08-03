# 02 Speedometer (LVGL)

![Framework](https://img.shields.io/badge/Framework-ESP--IDF%20%2B%20LVGL-orange)

An animated speedometer built with **LVGL** (via `esp_lvgl_port`): the needle sweeps continuously, rendered into the same PSRAM-backed RGB frame buffer.

使用 **LVGL**（经 `esp_lvgl_port`）实现的带指针动画的速度表，画面渲染到同一个 PSRAM RGB 帧缓冲。

## Pipeline / 流程

1. I2C + CH422G init (same as [01 HelloWorld](../01%20HelloWorld)).
2. RGB panel config (1024×600 RGB565, frame buffer in PSRAM).
3. `esp_lvgl_port` boots LVGL (own task + tick + mutex) and binds it to the RGB panel; LVGL's draw buffers also live in PSRAM.
4. Build the speedometer UI and start the needle animation (under the LVGL lock).
5. CH422G turns the backlight ON. The panel self-refreshes via DMA; LVGL keeps redrawing from its own task — **no app-side render loop needed**.

> **Tuning note:** with the frame buffer in PSRAM, **PCLK must stay ≤ 16 MHz** (21 MHz → blank white screen).
> **调参提示：** 帧缓冲在 PSRAM 时，**PCLK 需 ≤ 16 MHz**（21 MHz 会白屏）。

## Build & flash

```bash
idf.py set-target esp32s3
idf.py build          # auto-fetches lvgl + esp_lvgl_port into managed_components/
idf.py -p PORT flash monitor
```

Shared hardware notes are in the [board readme](../readme.md).
