# EPD_3in7_Showcase

适合新手的 Arduino IDE 示例：**Hello → 英文日程 → 一张照片**，播放一次后停在照片。

适配 OSPTEK 3.7 英寸六色电子纸 **480×720 / EPD037A02 / JD7601**，开发板为 **ESP32-S3**。

## 只需三步

1. 在 Arduino IDE 的**库管理器**搜索并安装 **Adafruit GFX Library**，同时安装其提示的依赖库。
2. 下载**整个 `EPD_3in7_Showcase` 文件夹**，用 Arduino IDE 打开 `EPD_3in7_Showcase.ino`。
3. 开发板选择 **ESP32S3 Dev Module**，按下方接线，编译上传。串口为 **115200**。

不要只复制 `.ino`：`photo_data.h`、`EPD037A02_GFX.h` 和 `EPD037A02_GFX.cpp` 也必须与主文件放在同一目录。无需制作字体、运行 Python、上传文件系统或复制库到系统目录。

## 接线

| 屏幕引脚 | ESP32-S3 GPIO |
| --- | --- |
| BUSY | 7 |
| CS | 10 |
| DC | 9 |
| RST | 8 |
| SDA / MOSI | 11 |
| CLK / SCK | 12 |
| VDD | 3.3 V |
| GND | GND |

只能使用 **3.3 V**，严禁 5 V，并确保共地。

## 新手只改主 `.ino`

- **改等待时间**：`HOLD_MS`，默认刷新结束后停留 3000 ms；全刷本身约需 15～25 秒。
- **改日程**：`subjects`、`rooms`、`times` 三张表，修改字符串即可。
- **改文字**：使用 `text("HELLO", x, y, 字号倍数, 颜色)` 或 `textCenter(...)`。
- **改图形**：使用 GFX 的 `fillRect()`、`drawRect()`、`fillRoundRect()` 等函数。
- **改引脚**：`EPD037A02_GFX epd(10, 9, 8, 7, 12, 11)` 这一行。
- **换照片**：替换 `photo_data.h`，保持数组名与尺寸不变。

## 文件结构

```text
EPD_3in7_Showcase/
├── EPD_3in7_Showcase.ino     # 新手只需要看这里：文字、日程、三屏流程
├── photo_data.h              # 已转换的照片帧，不是 JPG/PNG
├── EPD037A02_GFX.h           # 面板驱动接口
├── EPD037A02_GFX.cpp         # SPI、面板帧格式和照片发送
├── 02-driver-dog-car-480x720.png  # 裁切后的原始 PNG，便于查看和重新转换
└── README.md
```

早期拆分界面、自制字体、旧静态驱动及 `driver/` 子目录副本均已移除。新版只保留上方必要文件。

## Arduino IDE 链接错误的修复

若出现 `undefined reference to EPD037A02_GFX::begin(...)`、`display()` 等错误，说明 IDE 找到了驱动声明，却没有编译驱动的 `.cpp`。根目录下的 `EPD037A02_GFX.h` 和 `EPD037A02_GFX.cpp` 必须同时存在，不能将 `.cpp` 只放在 `driver/` 子目录中。

本版已将这两个文件放在主 `.ino` 同级目录，兼容 Arduino IDE 的示例构建方式。修改后请重新打开示例，关闭旧的构建标签页，再编译一次。

## 字体为什么不用自己创建？

新版调用 **Adafruit GFX 自带的 5×7 字体**，直接设置放大倍数：

```cpp
text("HELLO", 80, 60, 8, EPD_BLACK);
```

不需要创建字库文件，不需要修改字体数组。字体由 Adafruit GFX 提供，支持本例使用的英文字符；库同时提供现成的 FreeSans 等字体，需要更美观的字体时可进一步使用。[Adafruit 官方字体说明](https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts)

本例选择内置字体是为了减少文件和操作步骤，代价是字体的风格和精细程度有限。面板底层驱动仍需保留，因为通用 GFX 库负责文字和图形，而 JD7601 的特殊列优先帧格式需要专用驱动处理。

## 花屏问题修复记录

旧版把含 `static uint8_t *epdFb` 的 `epd037a02.h` 同时包含进 `.ino` 和 `screens.cpp`，产生两份独立的帧缓存指针：主程序初始化一份，界面代码访问另一份空指针，最后却把未初始化内存发送给屏幕，形成随机花屏。

新版只有一个 `EPD037A02_GFX epd` 对象，所有绘图和刷屏都通过它完成；分配帧缓存后也会先填白，再执行绘图和刷新。照片使用同一个对象直接发送原生帧。

## 验证

已使用 Adafruit GFX Library **1.13.0** 和 ESP32-S3 实际编译通过。未进行本次修复后的实物烧录，请在实屏上确认。
