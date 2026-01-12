

# RP2350 Serial Matrix Rain / RP2350 串口数字雨

![Python](https://img.shields.io/badge/Language-Python%20%2F%20MicroPython-blue)
![Platform](https://img.shields.io/badge/Platform-RP2350%20%2F%20Pico%202-red)
![License](https://img.shields.io/badge/License-MIT-green)

[English](#english) | [中文说明](#chinese)

---

<a name="english"></a>
## 🇬🇧 English Description

A lightweight Python script designed for the **Raspberry Pi Pico 2 (RP2350)** (and other MicroPython boards) that generates a "The Matrix" style digital rain effect via the serial port (UART/USB).

It uses ANSI escape codes to render green falling characters directly in your terminal or serial monitor (like Thonny, PuTTY, or screen).

### ✨ Features
*   **Classic Visuals:** Falling streams with bright green "heads" and darker green "tails".
*   **Highly Configurable:** Easily adjust the stream width, speed, and density.
*   **Cross-Platform:** Works on MicroPython (RP2350, ESP32) and standard PC Python.
*   **ANSI Support:** Requires a terminal that supports ANSI color codes.

### 🚀 How to Run

#### 1. Requirements
*   A Raspberry Pi Pico 2 (RP2350) or any MicroPython-compatible board.
*   **Thonny IDE** (recommended) or any serial terminal tool.

#### 2. Installation
1.  Connect your RP2350 to your computer.
2.  Open **Thonny IDE**.
3.  Create a new file and paste the code.
4.  Save the file to the device:
    *   Save as **`matrix_rain.py`** to run it manually.
    *   Save as **`main.py`** if you want it to **run automatically on boot**.

#### 3. Configuration
You can customize the effect by modifying the constants at the top of the script:

```python
# Width of the stream (columns). 
# Adjust this based on your terminal window size.
WIDTH = 40 

# Refresh speed (seconds). Lower is faster.
SPEED = 0.05 
```

### ⚠️ Note
*   **Filename Warning:** Do **NOT** name the file `time.py` or `random.py`, as this will conflict with internal libraries and crash the script.
*   **Stopping:** If running as `main.py`, press `Ctrl+C` in the console to stop the script and regain access to the REPL.

---

<a name="chinese"></a>
## 🇨🇳 中文说明

这是一个专为 **Raspberry Pi Pico 2 (RP2350)** (及其他 MicroPython 开发板) 设计的 Python 脚本。它通过串口（UART/USB）输出经典的《黑客帝国》风格数字雨特效。

程序使用 ANSI 转义序列在您的终端或串口监视器（如 Thonny, PuTTY, Xshell 等）中渲染绿色的下落字符流。

### ✨ 特性
*   **经典视觉效果**：模拟代码雨下落，具有亮绿色的“雨滴头”和深绿色的“雨滴尾”。
*   **高度可配置**：可以轻松调整瀑布流的宽度、下落速度和密度。
*   **跨平台**：既可以在 MicroPython (RP2350, ESP32) 上运行，也可以在电脑的标准 Python 环境中运行。
*   **ANSI 支持**：需要使用支持 ANSI 颜色代码的终端软件。

### 🚀 如何运行

#### 1. 准备工作
*   一块 Raspberry Pi Pico 2 (RP2350) 或其他支持 MicroPython 的开发板。
*   **Thonny IDE** (推荐) 或任意串口调试工具。

#### 2. 安装步骤
1.  将 RP2350 连接至电脑。
2.  打开 **Thonny IDE**。
3.  新建一个文件，将代码粘贴进去。
4.  将文件保存到设备中：
    *   保存为 **`matrix_rain.py`**：需要手动点击运行。
    *   保存为 **`main.py`**：开发板通电后**自动运行**此效果。

#### 3. 参数配置
你可以通过修改脚本顶部的常量来自定义效果：

```python
# 瀑布流的宽度（列数）
# 根据你的屏幕或串口窗口宽度进行调整
WIDTH = 40 

# 刷新速度（秒）。数字越小，下落越快。
SPEED = 0.05 
```

### ⚠️ 注意事项
*   **命名警告**：**千万不要**将文件命名为 `time.py` 或 `random.py`，否则会与系统内置库冲突导致报错。
*   **如何停止**：如果保存为 `main.py` 自动运行，在连接电脑后，需要在控制台按 `Ctrl+C` 强制中断程序，才能重新进入编程模式。

---

## 📜 License
MIT License
