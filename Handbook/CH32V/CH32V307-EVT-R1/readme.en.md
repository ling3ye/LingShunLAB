# CH32V307 EVT-R1 Examples

[中文](./readme.md) | **English**

![Build](https://img.shields.io/badge/Build-PlatformIO-orange)
![Platform](https://img.shields.io/badge/Platform-WCH%20CH32V-blue)
![MCU](https://img.shields.io/badge/MCU-CH32V307%20(RISC--V)-9cf)
![Board](https://img.shields.io/badge/Board-CH32V307%20EVT--R1-success)

PlatformIO examples for the **WCH CH32V307 EVT-R1** evaluation board. The CH32V307 is a 32-bit RISC-V microcontroller running up to 144 MHz, with an on-board **WCH-Link** debugger that flashes the chip and exposes a virtual serial port.

## Examples

| # | Example | What it shows |
| --- | --- | --- |
| 01 | [HelloWorld](./01%20HelloWorld) | Blinks PA0 and prints log lines over the on-board WCH-Link virtual serial port — the minimal power-on smoke test. |

## Hardware notes

- **MCU:** CH32V307, RISC-V core, up to 144 MHz.
- **Default memory layout:** Flash 256 KB + SRAM 64 KB (the EVT-R1 factory default, matching the PlatformIO board definition — no override needed). To switch to e.g. 288 KB Flash / 32 KB SRAM, first reprogram the option bytes with a WCH tool, then uncomment the matching lines in `platformio.ini`.
- **On-board debugger:** WCH-Link — handles flashing and provides one virtual serial port (wired to USART1 PA9/PA10).
- **User LED:** the EVT-R1 user LED is floating by default (not wired to the MCU); bridge **PA0 to LED1** with a jumper wire to make it light up.

## Toolchain setup

Just install [PlatformIO](https://platformio.org/) (VSCode extension or standalone CLI). On the first build, PlatformIO auto-fetches the `ch32v` platform, the WCH NoneOS SDK, and the RISC-V toolchain — no manual setup required.

## Build & flash

```bash
cd "01 HelloWorld"
pio run                  # build
pio run -t upload        # flash via the on-board WCH-Link
pio device monitor       # serial monitor (115200; USART1 over the WCH-Link virtual port)
```

> The baud rate is **115200**, set via `monitor_speed` in `platformio.ini`.

## Articles

📖 Full write-ups are published on the companion article site: _<TODO: insert article URL>_
