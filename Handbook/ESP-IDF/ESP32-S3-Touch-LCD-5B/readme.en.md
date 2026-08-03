# ESP32-S3-Touch-LCD-5B Examples

[中文](./readme.md) | **English**

![Framework](https://img.shields.io/badge/Framework-ESP--IDF-orange)
![Board](https://img.shields.io/badge/Board-Waveshare%20ESP32--S3--Touch--LCD--5B-blue)
![Display](https://img.shields.io/badge/Display-1024%C3%97600%20RGB-success)

Three ESP-IDF examples for the **Waveshare ESP32-S3-Touch-LCD-5B** — a 5" **1024×600 RGB** LCD (ST7262 direct-drive) on an ESP32-S3 WROOM-1-N16R8 module (16 MB flash + 8 MB octal PSRAM).

## Examples

| # | Example | Stack | What it shows |
| --- | --- | --- | --- |
| 01 | [HelloWorld](./01%20HelloWorld) | ESP-IDF only | Paints "Hello World" straight into the RGB framebuffer. **No third-party components.** |
| 02 | [Speedometer](./02%20Speedometer) | ESP-IDF + LVGL | An animated speedometer gauge. |
| 03 | [VehicleTelemetry](./03%20VehicleTelemetry) | ESP-IDF + LVGL | A multi-gauge vehicle telemetry dashboard. |

## Hardware notes

- **I2C bus:** SDA = **GPIO8**, SCL = **GPIO9** (shared by the CH422G expander, touch controller, RTC).
- **CH422G I/O expander:** enables the EXIO outputs, resets the LCD + touch, and **switches the backlight** on/off.
- **RGB panel:** 1024×600 RGB565; the ≈1.17 MB frame buffer lives in **PSRAM** and is self-refreshed by DMA. With the buffer in PSRAM, **PCLK must stay ≤ 16 MHz** (21 MHz → blank white screen).

## Build & flash

Requires ESP-IDF **v5.2 or newer**.

```bash
cd "01 HelloWorld"            # or 02 Speedometer / 03 VehicleTelemetry
idf.py set-target esp32s3     # first time only
idf.py build
idf.py -p PORT flash monitor  # PORT = /dev/cu.usbmodem* (macOS) / /dev/ttyUSB0 (Linux) / COMx (Windows)
```

> On first build, the LVGL examples (02, 03) download `lvgl` and `esp_lvgl_port` via the ESP-IDF Component Manager — declared in `main/idf_component.yml`, versions pinned by `dependencies.lock`. No manual setup needed.

## Articles

📖 Full write-ups are published on the companion article site: _<TODO: insert article URL>_
