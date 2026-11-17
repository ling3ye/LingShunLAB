# ESP32-S3 驱动 ST7306 4.2 英寸四色全反射 LCD

**[English](README.md) | 简体中文**

这是一个使用 ESP32-S3 和 ESP-IDF 驱动 Osptek **YDP420H005-V3** 4.2 英寸全反射 TFT LCD 的示例工程，包含 ST7306 驱动、绘图基元和 20 页自动轮播，覆盖颜色、图形、文字、UI、动画与旋转测试。仓库中还附带独立的 Arduino IDE 点亮与测试程序。

> **兼容性说明：** 本工程面向随附资料中的 YDP420H005-V3 模组及其 21Pin FPC。模组规格书使用“B/W/R（黑白红）”名称，而本固件按实测实现了白、青、红、黑四种显示状态。不要默认其他采用 ST7306 的屏幕具有相同颜色、引脚、寻址方式或电源参数。

## 硬件速览

| 项目 | 参数 |
|---|---|
| 显示模组 | Osptek YDP420H005-V3，4.2 英寸全反射 TFT LCD |
| 驱动 IC | Sitronix ST7306 |
| 原生分辨率 | **300 × 400 像素**（H × V，竖屏显存） |
| 显示颜色 | 白、青、红、黑；**2 bit/像素**，无中间灰阶 |
| 主控接口 | 4 线 SPI，另有 D/C、CS、RESET 和可选 TE 输出 |
| 模组供电 | FPC 第 5、6 脚接 **3.3V**，使用 3.3V 逻辑电平 |
| 点间距 | 0.212 × 0.212 mm |
| 有效显示区域 | 63.6 × 84.8 mm |
| 模组外形尺寸 | 67.6 × 91.0 × 1.1 mm |
| 工作温度 | −20～+70 °C |
| 存储温度 | −30～+80 °C |
| 重量 | 14 g |

以上为 `docs/YDP420H005-V3.pdf` 中的模组级参数，不代表第三方转接板或载板的保证规格。全反射 LCD 利用环境光显示；本工程不涉及背光或触摸接线。

## 所需元件

- 一块 **ESP32-S3 开发板**。随附的 ESP-IDF 配置采用 **N16R8 配置档**：16MB Flash 和八线 PSRAM。
- **YDP420H005-V3** 显示模组。
- 匹配的 **21Pin FPC 转接板/转接器**及 FPC 连接。通电前请根据模组机械图核对引脚编号和触点方向。
- 稳定的 **3.3V 电源**、公共地和较短的连接线。
- 建议在靠近屏幕/转接板的电源端配置合适的去耦电容。

本仓库没有指定 ESP32-S3 载板和 FPC 转接板的具体型号。

## 接线

默认接线同时适用于 ESP-IDF 工程，以及 Arduino 程序中的 `PINSET = 1` 配置。

| FPC 脚位 | 模组信号 | 常见转接板丝印 | ESP32-S3 连接 | 说明 |
|---:|---|---|---|---|
| 13 | `LCD_CS` | `CS` | **GPIO9** | 必需；低有效片选 |
| 8 | `LCD_RES` | `RESET`、`RST` | **GPIO10** | 必需；低有效硬件复位 |
| 10 | `TE` | `TE` | **GPIO11** | 可选；本工程不读取，可悬空 |
| 9 | `LCD_SCLK` | `SCL`、`SCLK` | **GPIO12** | 必需；SPI 时钟，**不是 I²C SCL** |
| 11 | `LCD_SDI` | `SDA`、`MOSI` | **GPIO13** | 必需；SPI MOSI，**不是 I²C SDA** |
| 12 | `LCD_D/C` | `RS`、`DC`、`A0` | **GPIO14** | 必需；命令/数据选择 |
| 5、6 | `VCC(3.3V)` | `VCC`、`VCI` | **3.3V** | 必需；两路供电脚都连接 |
| 1、4、7、14 | `GND` | `GND` | **GND** | 必需；公共地 |

固件引脚统一定义在 [`main/board_config.h`](main/board_config.h)。

**转接板特别注意：** 如果使用厂家的双 FPC 通用转接板，请将丝印 **`RS`** 对应的 `LCD_D/C` 接到 GPIO14。转接板上另一个 **`DC`** 丝印与本模组无关，不要接错。`RESET` 接 GPIO10，`TE` 可以不接。

### 电源与信号注意事项

- 模组电源和逻辑均使用 **3.3V**，不要接 5V。
- FPC 第 5、6 脚均接 3.3V，并确保公共地可靠连接。模组逻辑电源关闭时不要继续向其输入信号。
- ESP-IDF 使用只写 SPI，**不连接 MISO**；`LCD_SDI` 接 MOSI。
- SPI 连接线尽量短，并在靠近屏幕电源输入处设置合适的去耦。
- FPC 保留了触摸信号，但本例使用的模组没有触摸功能，所有 `TP-*` 引脚都无需连接。

<details>
<summary><strong>展开查看完整 21Pin FPC 定义</strong></summary>

| 脚位 | 模组名称 | 功能 / 本工程用法 |
|---:|---|---|
| 1 | GND | 地 |
| 2 | NC | 不连接 |
| 3 | NC | 不连接 |
| 4 | GND | 地 |
| 5 | VCC(3.3V) | 数字电源，接 3.3V |
| 6 | VCC(3.3V) | 数字电源，接 3.3V |
| 7 | GND | 地 |
| 8 | LCD_RES | 低有效复位输入，接 GPIO10 |
| 9 | LCD_SCLK | SPI 串行时钟，接 GPIO12 |
| 10 | TE | 帧同步/撕裂效应输出，可选接 GPIO11 |
| 11 | LCD_SDI | SPI 数据，SCLK 上升沿锁存，接 GPIO13/MOSI |
| 12 | LCD_D/C | 数据/命令选择，接 GPIO14 |
| 13 | LCD_CS | 低有效片选，接 GPIO9 |
| 14 | GND | 地 |
| 15 | TP-VDD | 触摸模拟电源，不使用 |
| 16 | TP-RST | 低有效触摸复位，不使用 |
| 17 | TP-INT | 触摸中断输出，不使用 |
| 18 | TP-SDA | 触摸 I²C 数据，不使用 |
| 19 | TP-SCL | 触摸 I²C 时钟，不使用 |
| 20 | GND | 地 |
| 21 | GND | 地 |

</details>

## 两套固件

| 方案 | 用途 | 显示配置 |
|---|---|---|
| **根目录 ESP-IDF 工程** | 完整驱动和 20 页演示 | SPI2、模式 0、40MHz；原生 300 × 400 显存 |
| **Arduino IDE 程序** | 精简的 8 页点亮/测试套件 | SPI 模式 0、20MHz；逻辑 400 × 300 横屏，默认 `ROT180 = 1` |

以下主要使用说明针对 ESP-IDF 工程。Arduino 版本有独立的方向和引脚组宏，请同时参阅 [Arduino IDE 替代方案](#arduino-ide-替代方案)。

## 编译、烧录与监视（ESP-IDF）

开发记录中已使用 **ESP-IDF v6.0.2** 完成零警告编译。

```bash
# 根据本机实际安装的 ESP-IDF 版本调整 export.sh 路径。
. "$HOME/.espressif/v6.0.2/esp-idf/export.sh"

idf.py set-target esp32s3
idf.py build

# 将串口替换为本机实际设备。
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

串口示例：macOS 为 `/dev/cu.usbmodem*`，Linux 为 `/dev/ttyUSB0`，Windows 为 `COM3`。按 **Ctrl+]** 退出 ESP-IDF Monitor。

随附的 `sdkconfig.defaults` 配置为 ESP32-S3、16MB Flash、80MHz 八线 PSRAM、240MHz CPU、8192 字节主任务栈和 1000Hz FreeRTOS。若你的 ESP32-S3 模组配置不同，请调整 Flash/PSRAM 选项。

## 显示与驱动细节

- SPI 主机为 `SPI2_HOST`，模式 0，`main/main.c` 默认时钟 **40MHz**。**时序注意：** 随附 ST7306 v0.1 手册规定写时钟最小周期为 30ns，对应 33.3MHz；默认 40MHz 超出了该规格。为保守运行或遇到通信不稳定时，请降到 **20MHz 或更低**。
- 每帧通过 DMA 一次传输 **30000 字节**。
- 原生显存为 **300 × 400**、2 bit/像素，每字节打包为一个 2 × 2 像素块，共 4 个像素。
- 显存组织为每个数据行 150 字节 × 200 个数据行 = 30000 字节。
- 列地址范围 `0x05`–`0x36`，行地址范围 `0x00`–`0xC7`。
- 绘图先写入 RAM，再调用 `st7306_display()` 传输完整一帧。
- 演示为动画启用高功耗模式；驱动也提供低功耗、休眠、显示开关和反色接口。

### 颜色编码

| 颜色 | 2 bit 值 | Bit 1 | Bit 0 | 整屏同色填充字节 |
|---|---:|---:|---:|---:|
| 白 | `0x00` | 0 | 0 | `0x00` |
| 青 | `0x01` | 0 | 1 | `0x33` |
| 红 | `0x02` | 1 | 0 | `0xCC` |
| 黑 | `0x03` | 1 | 1 | `0xFF` |

两个颜色位平面在字节内交错排列，不能简单把一个 2 bit 颜色值重复填入整个字节，请使用随附驱动。屏幕没有中间灰阶：从黑白工程移植的页面会将灰阶图案量化为白 → 青 → 红 → 黑。

### 显示方向

ESP-IDF 显存采用竖屏 **300 × 400**，初始化时设置 `MADCTL (0x36) = 0x48`。第 15 页演示 0°、90°、180°、270° 软件旋转。独立的 Arduino 程序则提供逻辑 **400 × 300** 横屏画布，默认再进行 180° 旋转（`ROT180 = 1`）。两者属于不同的应用层映射，屏幕物理分辨率没有改变。

## 演示页面

| 页码 | 页面 | 页码 | 页面 |
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

第 1～15 页与同目录系列的黑白屏工程 `420-RLCD-400x300-lekang` 对齐，包括页面顺序、布局、驻留时间和目标帧率。第 16～20 页为四色屏专属测试。

如需快速检查颜色，将 `main/main.c` 中的 `DEMO_ONLY_COLOR_PAGES` 设为 `1`。上电后会先运行一次第 16～20 页，**随后仍进入完整 20 页轮播**，并不是永久只循环彩色页面。

## Arduino IDE 替代方案

使用 Arduino IDE 打开 [`ArduinoIDE/esp32s3-st7306-bringup/esp32s3-st7306-bringup.ino`](ArduinoIDE/esp32s3-st7306-bringup/esp32s3-st7306-bringup.ino)，并选择匹配的 ESP32-S3 开发板配置。

- 默认 `PINSET = 1`，使用上面的接线表。
- `PINSET = 0` 选择另一组转接板引脚：CS → GPIO38、RESET → GPIO41、SCLK → GPIO39、MOSI → GPIO42、RS/D/C → GPIO40。
- 程序运行 8 个测试页，SPI 时钟为 20MHz，显存按每块 1500 字节发送。
- `ROT180 = 1` 选择当前横屏方向；设为 `0` 可恢复较早的方向。
- `DIAG_MODE = true` 会用 50 × 50 测试方块替代轮播，用于诊断坐标和方向。

## 源码结构

```text
.
├── main/
│   ├── main.c                  # 入口、SPI 配置和演示启动
│   ├── board_config.h          # 默认 ESP32-S3 引脚映射
│   ├── st7306_color4.h/.cpp    # ST7306 驱动及 C 接口
│   ├── st73xx_ui.h/.c          # 线、矩形、圆、三角形和多边形
│   └── color4_demo.h/.c        # 20 页演示及调度
├── ArduinoIDE/
│   └── esp32s3-st7306-bringup/ # 独立 Arduino 测试程序
├── docs/                       # 模组/控制器资料和开发记录
└── sdkconfig.defaults          # 面向 ESP32-S3 N16R8 的默认构建配置
```

## 故障排查

- **屏幕不亮：** 检查 FPC 第 5、6 脚是否有 3.3V、公共地、FPC 方向和全部 6 个显示信号。使用厂家转接板时，确认 **RS**（而不是 **DC**）接 GPIO14。
- **图像不稳定或偶发无法启动：** 缩短 SPI 连线、改善接地和去耦，并降低 SPI 时钟。ESP-IDF 修改 `main/main.c` 中的 `.clock_hz`；Arduino 修改 `SPI_HZ`。
- **显示方向不对：** 区分 ESP-IDF 的原生 300 × 400 映射和 Arduino 的 400 × 300 映射。Arduino 可通过 `ROT180` 和 `DIAG_MODE` 排查，不要先改显存尺寸。
- **颜色异常：** 核对上面的 2 bit 颜色表和整屏填充字节，并确认替换屏幕与本模组采用相同颜色实现。
- **编译或启动配置问题：** 检查目标是否为 `esp32s3`，以及 Flash/PSRAM 设置是否与实际 ESP32-S3 模组一致。
- **烧录或监视打不开串口：** 关闭占用串口的其他监视器，并核对当前串口设备。

## 验证记录

随附开发记录记载了以下验证；本次 README 更新没有重新执行这些测试：

- ESP-IDF v6.0.2、ESP32-S3 目标编译零警告通过。
- 20 页全部在主机端使用同一套绘制代码渲染并逐页目视检查。
- 快速 `fill_rect()` 与逐点 `write_point()` 完成 300 组随机矩形交叉校验，结果零差异。
- 四个旋转相位均检查过标题栏位置、箭头方向和正圆度。

实际硬件表现仍需结合你自己的模组、转接板和 ESP32-S3 开发板验证。

## 文档与参考

- [YDP420H005-V3 模组规格书](docs/YDP420H005-V3.pdf)：机械图、FPC 引脚、电气与光学参数。
- [ST7306 控制器手册 v0.1](docs/ST7306_V0.1.pdf)：接口、寄存器、显存与时序。
- [ST7306 BWR 转图说明](<docs/HSD 4.2” 300x400 黑白红-转图说明 1.pdf>)：显存组织与转图示例。
- [项目开发记录](docs/devlog/2026-10-26-1404-ST7306四色全反屏ESP-IDF点亮工程.md)：实现细节与验证过程。
- [厂家 Gitee 示例代码](https://gitee.com/osptek/display-rlcd-4.2inch-300x400/tree/main/examples)。
