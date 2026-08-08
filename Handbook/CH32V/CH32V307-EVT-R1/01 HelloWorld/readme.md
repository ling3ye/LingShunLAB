# 01 Hello World（闪烁 + 串口打印）

**中文** | [English](./readme.en.md)

![Build](https://img.shields.io/badge/Build-PlatformIO-orange)
![Language](https://img.shields.io/badge/Language-C-blue)

CH32V307 的最小化上电验证：以 100 ms 周期翻转 **PA0**，同时经板载 WCH-Link 的虚拟串口（USART1 PA9/PA10）打印启动信息与 LED 状态。基于 WCH NoneOS SDK 裸机开发，**不依赖任何第三方组件**。

## 现象

上电后串口（115200）应持续输出：

```
CH32V307 booted, SystemCoreClock = 144000000 Hz
LED 0
LED 1
LED 0
...
```

PA0 上的 LED 同步以约 5 Hz 闪烁（翻转间隔 100 ms）。

> ⚠️ EVT-R1 的用户 LED 默认悬空（未接 MCU）。若看不到灯亮，用一根杜邦线把 **PA0 桥接到板上的 LED1** 即可。

## 流程

1. `SystemCoreClockUpdate()` 更新系统时钟；`Delay_Init()` 初始化毫秒级延时。
2. `USART_Printf_Init(115200)` 初始化 USART1，SDK 已把 `printf` 重定向到该串口（走 WCH-Link 虚拟串口）。
3. 使能 GPIOA 时钟，把 PA0 配置为推挽输出。
4. 主循环每 100 ms 翻转 PA0 并打印当前状态。

## 编译与烧录

```bash
pio run                  # 编译
pio run -t upload        # 经板载 WCH-Link 烧录
pio device monitor       # 串口监视（115200）
```

板级硬件说明（存储布局、WCH-Link、用户 LED）见 [板级 readme](../readme.md)。
