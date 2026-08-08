# CH32V307 EVT-R1 示例

**中文** | [English](./readme.en.md)

![Build](https://img.shields.io/badge/Build-PlatformIO-orange)
![Platform](https://img.shields.io/badge/Platform-WCH%20CH32V-blue)
![MCU](https://img.shields.io/badge/MCU-CH32V307%20(RISC--V)-9cf)
![Board](https://img.shields.io/badge/Board-CH32V307%20EVT--R1-success)

面向 **沁恒（WCH）CH32V307 EVT-R1 评估板** 的 PlatformIO 示例。主控 CH32V307 是一颗 32 位 RISC-V 内核单片机，最高 144 MHz，板载 **WCH-Link** 调试器，可直接烧录并提供虚拟串口。

## 示例列表

| 编号 | 示例 | 内容 |
| --- | --- | --- |
| 01 | [HelloWorld](./01%20HelloWorld) | 闪烁 PA0 + 经板载 WCH-Link 虚拟串口打印日志，最小化上电验证。 |

## 硬件要点

- **MCU：** CH32V307，RISC-V，最高 144 MHz。
- **默认存储布局：** Flash 256 KB + SRAM 64 KB（EVT-R1 出厂默认，与 PlatformIO 板卡定义一致，无需覆盖）。如需切换为 288 KB Flash / 32 KB SRAM 等其它布局，需先用 WCH 工具改 option bytes，并在 `platformio.ini` 中同步取消注释。
- **板载调试器：** WCH-Link，负责烧录并提供一路虚拟串口（接 USART1 的 PA9/PA10）。
- **用户 LED：** EVT-R1 的用户 LED 默认悬空（未接 MCU），需用一根杜邦线把 **PA0 桥接到 LED1** 才会亮。

## 工具链准备

仅需安装 [PlatformIO](https://platformio.org/)（VSCode 插件或独立 CLI 均可）。首次编译时，PlatformIO 会自动拉取 `ch32v` 平台、WCH NoneOS SDK 与 RISC-V 工具链，无需手动配置。

## 编译与烧录

```bash
cd "01 HelloWorld"
pio run                  # 编译
pio run -t upload        # 经板载 WCH-Link 烧录
pio device monitor       # 串口监视（115200，USART1 走 WCH-Link 虚拟串口）
```

> 串口波特率为 **115200**，在 `platformio.ini` 的 `monitor_speed` 中配置。

## 配套讲解

📖 完整图文讲解发布在配套网站：_<TODO: 待补充文章链接>_
