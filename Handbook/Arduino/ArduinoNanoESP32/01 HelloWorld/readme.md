# Arduino LED Projects / Arduino LED 实验项目

A collection of basic Arduino LED control scripts, including simple LED blinking and RGB LED sequential cycling.
一组基础的 Arduino LED 控制脚本，包含简单的 LED 闪烁和 RGB LED 顺序循环。

---

## 1. Project: Single LED Blink / 项目：单灯闪烁

### English Description

This is the "Hello World" of hardware. It controls the built-in LED on the Arduino board to blink at 1-second intervals.

* **File:** `Blink.ino` (or your chosen name)
* **Function:** Demonstrates basic digital output and delay functions.

### 中文说明

这是硬件世界的“Hello World”。它控制 Arduino 开发板上自带的 LED 灯以 1 秒为间隔循环闪烁。

* **文件：** `Blink.ino`
* **功能：** 演示基础的数字输出（Digital Output）和延时（Delay）功能。

---

## 2. Project: RGB LED Cycle / 项目：RGB LED 循环

### English Description

This project controls a Common Anode RGB LED to cycle through Red, Green, and Blue colors sequentially.

* **File:** `RGB_Blink_Cycle.ino`
* **Logic:** Since it uses a **Common Anode** LED, a `LOW` signal turns the light ON, and a `HIGH` signal turns it OFF.
* **Pins:** Uses `LED_RED`, `LED_GREEN`, and `LED_BLUE` defined pins.

### 中文说明

该项目控制一个**共阳极** RGB LED，依次循环显示红、绿、蓝三种颜色。

* **文件：** `RGB_Blink_Cycle.ino`
* **逻辑：** 由于使用**共阳极** LED，输出 `LOW`（低电平）时灯亮，输出 `HIGH`（高电平）时灯灭。
* **引脚：** 使用定义的 `LED_RED`、`LED_GREEN` 和 `LED_BLUE` 引脚。

---

## Hardware Connection / 硬件连接

| Component | Arduino Pin | Note |
| --- | --- | --- |
| **Built-in LED** | Pin 13 | Default `LED_BUILTIN` |
| **RGB Red** | Defined Pin (e.g., D9) | Use current-limiting resistors |
| **RGB Green** | Defined Pin (e.g., D10) | 使用限流电阻 |
| **RGB Blue** | Defined Pin (e.g., D11) |  |

---

## How to Use / 如何使用

1. **Install Arduino IDE**: Download and install from [arduino.cc](https://www.arduino.cc/).
2. **Open File**: Open the `.ino` file in the IDE.
3. **Select Board**: Go to `Tools > Board` and select your model (e.g., Arduino Uno).
4. **Upload**: Click the "Upload" button (arrow icon) to burn the code to your board.
5. **安装 Arduino IDE**：从官网下载并安装。
6. **打开文件**：在 IDE 中打开相应的 `.ino` 文件。
7. **选择开发板**：在“工具 > 开发板”中选择你的型号（如 Arduino Uno）。
8. **上传代码**：点击“上传”按钮（箭头图标）将程序烧录至开发板。
