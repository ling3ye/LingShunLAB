# 01 Hello World (RGB Framebuffer)

![Framework](https://img.shields.io/badge/Framework-ESP--IDF-orange)

The minimal display bring-up: paint a dark-blue background and a centred white **"Hello World!"** directly into the 1024×600 RGB frame buffer. **No LVGL, no third-party components** — only built-in ESP-IDF APIs plus a tiny CH422G driver.

最小化的屏幕点亮示例：直接在 1024×600 RGB 帧缓冲里绘制深蓝背景 + 居中的白色 **"Hello World!"**。**不依赖 LVGL、不依赖任何第三方组件**，仅使用 ESP-IDF 内置 API 和一个小型 CH422G 驱动。

## Pipeline / 流程

1. Init the I2C bus (GPIO8/GPIO9) and the **CH422G** I/O expander.
2. CH422G: enable EXIO outputs, reset the LCD + touch, keep the **backlight OFF** for now.
3. Configure the RGB LCD panel (1024×600 RGB565, frame buffer in PSRAM).
4. Paint the background + text into the frame buffer.
5. CH422G turns the **backlight ON** → the image becomes visible.
6. Idle — the RGB panel self-refreshes from the PSRAM frame buffer via DMA.

## Build & flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Shared hardware notes (I2C pins, CH422G, PSRAM) are in the [board readme](../readme.md).
