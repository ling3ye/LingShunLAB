# 01 Hello World (Blink + Serial Print)

[中文](./readme.md) | **English**

![Build](https://img.shields.io/badge/Build-PlatformIO-orange)
![Language](https://img.shields.io/badge/Language-C-blue)

The minimal CH32V307 power-on smoke test: toggles **PA0** every 100 ms while printing boot info and LED state over the on-board WCH-Link virtual serial port (USART1 PA9/PA10). Built on the WCH NoneOS SDK bare-metal — **no third-party components**.

## Expected output

After power-on, the serial port (115200) should stream:

```
CH32V307 booted, SystemCoreClock = 144000000 Hz
LED 0
LED 1
LED 0
...
```

The LED on PA0 blinks at roughly 5 Hz (100 ms toggle interval).

> ⚠️ The EVT-R1 user LED is floating by default (not wired to the MCU). If you don't see it light up, bridge **PA0 to the board's LED1** with a jumper wire.

## Pipeline

1. `SystemCoreClockUpdate()` refreshes the system clock; `Delay_Init()` sets up millisecond delays.
2. `USART_Printf_Init(115200)` initializes USART1 — the SDK already redirects `printf` to it (routed through the WCH-Link virtual serial port).
3. Enable the GPIOA clock and configure PA0 as push-pull output.
4. The main loop toggles PA0 and prints the current state every 100 ms.

## Build & flash

```bash
pio run                  # build
pio run -t upload        # flash via the on-board WCH-Link
pio device monitor       # serial monitor (115200)
```

Shared hardware notes (memory layout, WCH-Link, user LED) are in the [board readme](../readme.en.md).
