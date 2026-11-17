# ESP32-S3 + ST7306 4.2-inch Four-Color Reflective LCD

**English | [简体中文](README.zh-CN.md)**

An ESP-IDF example for driving an Osptek **YDP420H005-V3** 4.2-inch reflective TFT LCD with an ESP32-S3. The firmware includes the ST7306 driver, drawing primitives, and a 20-page carousel covering colors, graphics, text, UI, animation, and rotation. An independent Arduino IDE bring-up sketch is also included.

> **Compatibility:** This project targets the supplied YDP420H005-V3 module and its documented 21-pin FPC. The module datasheet uses “B/W/R” wording, while this firmware implements four observed states: white, cyan, red, and black. Do not assume that other ST7306-based panels share the same colors, pinout, addressing, or power settings.

## Hardware at a Glance

| Item | Details |
|---|---|
| Display module | Osptek YDP420H005-V3, 4.2-inch reflective TFT LCD |
| Driver IC | Sitronix ST7306 |
| Native resolution | **300 × 400 pixels** (H × V, portrait framebuffer) |
| Color states | White, cyan, red, black; **2 bits/pixel**, no intermediate grayscale |
| Host interface | 4-line SPI with D/C, CS, and RESET; optional TE output |
| Module supply | **3.3 V** on FPC pins 5 and 6; use 3.3 V logic |
| Pixel pitch | 0.212 × 0.212 mm |
| Active area | 63.6 × 84.8 mm |
| Module dimensions | 67.6 × 91.0 × 1.1 mm |
| Operating temperature | −20 to +70 °C |
| Storage temperature | −30 to +80 °C |
| Weight | 14 g |

These are module-level ratings from `docs/YDP420H005-V3.pdf`, not guaranteed ratings for a third-party adapter or carrier board. A reflective LCD uses ambient light; this project has no backlight or touch-interface wiring.

## Parts Needed

- An **ESP32-S3 development board**. The supplied ESP-IDF configuration uses an **N16R8 profile**: 16 MB flash and octal PSRAM.
- The **YDP420H005-V3** display module.
- A compatible **21-pin FPC breakout/adapter** and FPC connection. Verify pin numbering and contact orientation against the module mechanical drawing before connecting power.
- A stable **3.3 V supply**, common ground, and short jumper wires.
- Suitable supply decoupling near the display/adapter is recommended.

The exact ESP32-S3 carrier board and FPC adapter model are not specified in this repository.

## Wiring

The default wiring is shared by the ESP-IDF project and the Arduino sketch with `PINSET = 1`.

| FPC pin(s) | Module signal | Common adapter label | ESP32-S3 connection | Status |
|---:|---|---|---|---|
| 13 | `LCD_CS` | `CS` | **GPIO9** | Required; active-low chip select |
| 8 | `LCD_RES` | `RESET`, `RST` | **GPIO10** | Required; active-low hardware reset |
| 10 | `TE` | `TE` | **GPIO11** | Optional; not read by this project, may be left open |
| 9 | `LCD_SCLK` | `SCL`, `SCLK` | **GPIO12** | Required; SPI clock, **not I²C SCL** |
| 11 | `LCD_SDI` | `SDA`, `MOSI` | **GPIO13** | Required; SPI MOSI, **not I²C SDA** |
| 12 | `LCD_D/C` | `RS`, `DC`, `A0` | **GPIO14** | Required; command/data selection |
| 5, 6 | `VCC(3.3V)` | `VCC`, `VCI` | **3.3 V** | Required; connect both supply pins |
| 1, 4, 7, 14 | `GND` | `GND` | **GND** | Required; common ground |

Pin definitions for the firmware are centralized in [`main/board_config.h`](main/board_config.h).

**Important adapter note:** on the supplier's dual-FPC universal adapter, use the pad labeled **`RS`** for `LCD_D/C` → GPIO14. The adapter's separate **`DC`** label is unrelated to this module. `RESET` connects to GPIO10, and `TE` can remain disconnected.

### Power and Signal Notes

- Use **3.3 V only** for the module supply and logic. Do not apply 5 V.
- Connect both FPC supply pins (5 and 6) and a common ground. Do not drive input signals while the module logic power is off.
- ESP-IDF uses write-only SPI: **MISO is not connected**. `LCD_SDI` goes to MOSI.
- Keep SPI wires short and place suitable decoupling close to the display power input.
- The display FPC contains optional touch signals, but the module used by this example has no touch functionality; no `TP-*` connections are needed.

<details>
<summary><strong>Full 21-pin FPC definition</strong></summary>

| Pin | Module name | Description / use in this project |
|---:|---|---|
| 1 | GND | Ground |
| 2 | NC | No connection |
| 3 | NC | No connection |
| 4 | GND | Ground |
| 5 | VCC(3.3V) | Digital supply; connect to 3.3 V |
| 6 | VCC(3.3V) | Digital supply; connect to 3.3 V |
| 7 | GND | Ground |
| 8 | LCD_RES | Reset input, active low; GPIO10 |
| 9 | LCD_SCLK | SPI serial clock; GPIO12 |
| 10 | TE | Tearing-effect/frame-sync output; optional GPIO11 |
| 11 | LCD_SDI | SPI data, latched on rising SCLK; GPIO13/MOSI |
| 12 | LCD_D/C | Data/command selection; GPIO14 |
| 13 | LCD_CS | Chip select, active low; GPIO9 |
| 14 | GND | Ground |
| 15 | TP-VDD | Touch analog supply; unused |
| 16 | TP-RST | Touch reset, active low; unused |
| 17 | TP-INT | Touch interrupt output; unused |
| 18 | TP-SDA | Touch I²C data; unused |
| 19 | TP-SCL | Touch I²C clock; unused |
| 20 | GND | Ground |
| 21 | GND | Ground |

</details>

## Firmware Options

| Option | Purpose | Display setup |
|---|---|---|
| **ESP-IDF project (root)** | Full driver and 20-page demonstration | SPI2, mode 0, 40 MHz; native 300 × 400 framebuffer |
| **Arduino IDE sketch** | Compact 8-page bring-up/test suite | SPI mode 0, 20 MHz; logical 400 × 300 landscape, `ROT180 = 1` by default |

The main instructions below describe the ESP-IDF project. The Arduino alternative has its own orientation and pin-set macros; see [Arduino IDE Alternative](#arduino-ide-alternative).

## Build, Flash, and Monitor (ESP-IDF)

The development log records a successful, zero-warning build with **ESP-IDF v6.0.2**.

```bash
# Source the export script for your installed ESP-IDF version.
. "$HOME/.espressif/v6.0.2/esp-idf/export.sh"

idf.py set-target esp32s3
idf.py build

# Replace the serial port with the one on your system.
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Port examples: `/dev/cu.usbmodem*` on macOS, `/dev/ttyUSB0` on Linux, or `COM3` on Windows. Exit the ESP-IDF monitor with **Ctrl+]**.

The supplied `sdkconfig.defaults` selects ESP32-S3, 16 MB flash, octal PSRAM at 80 MHz, a 240 MHz CPU, an 8192-byte main-task stack, and FreeRTOS at 1000 Hz. Adjust the flash/PSRAM settings if your ESP32-S3 module differs.

## Display and Driver Details

- SPI host: `SPI2_HOST`; mode 0; default clock **40 MHz** in `main/main.c`. **Timing caveat:** the bundled ST7306 v0.1 datasheet specifies a 30 ns minimum write-clock period, equivalent to 33.3 MHz. The 40 MHz setting exceeds that rating; reduce it to **20 MHz or lower** for conservative operation or if communication is unreliable.
- One full-frame transfer sends **30,000 bytes** using DMA.
- Native framebuffer: **300 × 400**, 2 bits/pixel, four pixels packed into each byte as a 2 × 2 block.
- Memory organization: 150 bytes per data row × 200 data rows = 30,000 bytes.
- Column address range: `0x05`–`0x36`; row address range: `0x00`–`0xC7`.
- Pixels are drawn into RAM first; call `st7306_display()` to transfer the complete frame.
- The demo enables high-power mode for animation. The driver also exposes low-power, sleep, display-on/off, and inversion functions.

### Color Encoding

| Color | 2-bit value | Bit 1 | Bit 0 | Solid-fill byte |
|---|---:|---:|---:|---:|
| White | `0x00` | 0 | 0 | `0x00` |
| Cyan | `0x01` | 0 | 1 | `0x33` |
| Red | `0x02` | 1 | 0 | `0xCC` |
| Black | `0x03` | 1 | 1 | `0xFF` |

The two color planes are interleaved within each byte. Use the supplied driver rather than repeating a 2-bit value across a byte. There are no intermediate grayscale levels: pages derived from the monochrome project quantize their grayscale patterns to white → cyan → red → black.

### Orientation

The ESP-IDF framebuffer is portrait **300 × 400** and initializes `MADCTL (0x36)` to `0x48`. Demo page 15 demonstrates 0°, 90°, 180°, and 270° software rotation. The separate Arduino sketch instead presents a logical **400 × 300** landscape canvas and applies an additional 180° rotation by default (`ROT180 = 1`). These are different application-level mappings, not a change in panel resolution.

## Demonstration Pages

| # | Page | # | Page |
|---:|---|---:|---|
| 1 | Hello World | 11 | Game of Life |
| 2 | Color quantize | 12 | Uptime clock |
| 3 | Geometry | 13 | Retro desktop |
| 4 | Pixel grid | 14 | Inversion |
| 5 | Text | 15 | Rotation |
| 6 | Dashboard UI | 16 | Color cycle |
| 7 | Frame rate | 17 | Color card |
| 8 | Fine patterns | 18 | Quad split |
| 9 | Bounce | 19 | HMI demo |
| 10 | Starfield | 20 | Color flow |

Pages 1–15 mirror the page order, layout, dwell time, and frame-rate targets of the sibling monochrome project `420-RLCD-400x300-lekang`. Pages 16–20 are specific to the four-color panel.

To check colors quickly, set `DEMO_ONLY_COLOR_PAGES` to `1` in `main/main.c`. This shows pages 16–20 once at startup and **then enters the full 20-page carousel**; it does not loop only the color pages indefinitely.

## Arduino IDE Alternative

Open [`ArduinoIDE/esp32s3-st7306-bringup/esp32s3-st7306-bringup.ino`](ArduinoIDE/esp32s3-st7306-bringup/esp32s3-st7306-bringup.ino) in Arduino IDE with an ESP32-S3 board configuration.

Tested with an ESP32-S3 N16R8 board, Arduino IDE v2.3.8, and esp32 Arduino Core v3.3.8. The menu values below are reference only; adjust them for your own board: `ESP32S3 Dev Module` / `USB CDC On Boot = Enabled` / `CPU 240MHz` / `Flash Mode QIO 80MHz` / `Flash Size 4MB` / `PSRAM Disabled` / `Upload Speed 921600` / `USB Mode = Hardware CDC and JTAG`. `Board` must target esp32s3; `USB CDC On Boot` and `Upload Mode` depend on whether the port is native USB or a USB-serial bridge; `Flash Size` and `PSRAM` depend on the module. This does not use the full 16MB flash or 8MB PSRAM of an N16R8 board; the sketch is unaffected, but using the full capacity requires changing Flash Size, PSRAM, and the partition scheme, which is not verified here.

- `PINSET = 1` is the default and uses the wiring table above.
- `PINSET = 0` selects the alternate adapter mapping: CS → GPIO38, RESET → GPIO41, SCLK → GPIO39, MOSI → GPIO42, and RS/D/C → GPIO40.
- The sketch runs eight test pages, uses a 20 MHz SPI clock, and transmits the framebuffer in 1500-byte chunks.
- `ROT180 = 1` selects the current landscape orientation; set it to `0` for the earlier orientation.
- `DIAG_MODE = true` replaces the carousel with a 50 × 50 test square for coordinate/orientation diagnosis.

## Source Layout

```text
.
├── main/
│   ├── main.c                  # Entry point, SPI configuration, demo startup
│   ├── board_config.h          # Default ESP32-S3 pin mapping
│   ├── st7306_color4.h/.cpp    # ST7306 driver and C API
│   ├── st73xx_ui.h/.c          # Lines, rectangles, circles, triangles, polygons
│   └── color4_demo.h/.c        # 20 demo pages and scheduler
├── ArduinoIDE/
│   └── esp32s3-st7306-bringup/ # Standalone Arduino test sketch
├── docs/                       # Module/controller documents and development log
└── sdkconfig.defaults          # ESP32-S3 N16R8-oriented build defaults
```

## Troubleshooting

- **Blank display:** recheck 3.3 V on FPC pins 5 and 6, common ground, FPC orientation, and all six display signals. On the supplier adapter, confirm that **RS**, not **DC**, goes to GPIO14.
- **Unreliable image or intermittent startup:** shorten the SPI wiring, improve grounding/decoupling, and reduce the SPI clock. For ESP-IDF, change `.clock_hz` in `main/main.c`; for Arduino, change `SPI_HZ`.
- **Wrong orientation:** distinguish the ESP-IDF native 300 × 400 mapping from the Arduino 400 × 300 mapping. For Arduino, test `ROT180` and `DIAG_MODE` rather than changing the framebuffer size.
- **Unexpected colors:** verify the 2-bit color table and solid-fill bytes above. Also confirm that the replacement panel has the same color implementation as this module.
- **Build or boot configuration problems:** check that the target is `esp32s3` and that flash/PSRAM settings match the actual ESP32-S3 module.
- **Flash/monitor cannot open the port:** close any other serial monitor and verify the selected serial device.

## Validation Record

The supplied development log records the following verification; it was not rerun as part of this README update:

- ESP-IDF v6.0.2 build for ESP32-S3 completed with zero warnings.
- All 20 pages were rendered on the host with the same drawing code and visually inspected.
- Fast `fill_rect()` output matched pixel-by-pixel `write_point()` output for 300 randomized rectangles, with zero differences.
- All four rotation phases were checked for title position, arrow direction, and circular geometry.

Hardware behavior still needs to be verified with your own module, adapter, and ESP32-S3 board.

## Documentation and References

- [YDP420H005-V3 module specification](docs/YDP420H005-V3.pdf) — mechanical drawing, FPC pinout, electrical and optical characteristics.
- [ST7306 controller datasheet, v0.1](docs/ST7306_V0.1.pdf) — interfaces, registers, memory, and timing.
- [ST7306 BWR image-conversion note](<docs/HSD 4.2” 300x400 黑白红-转图说明 1.pdf>) — framebuffer organization and conversion example.
- [Project development log (Chinese)](docs/devlog/2026-10-26-1404-ST7306四色全反屏ESP-IDF点亮工程.md) — implementation notes and validation details.
- [Supplier example code on Gitee](https://gitee.com/osptek/display-rlcd-4.2inch-300x400/tree/main/examples).

Vendor documents and code remain subject to their original copyright and redistribution terms.
