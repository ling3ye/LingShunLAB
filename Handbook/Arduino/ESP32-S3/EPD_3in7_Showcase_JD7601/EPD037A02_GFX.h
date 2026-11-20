/*
 * ============================================================================
 *  EPD037A02_GFX — Adafruit_GFX driver class for
 *  OSPTEK 3.7" E6 e-paper, 480x720, 6 colors (JD7601 / IST7601 / IST7163)
 * ============================================================================
 *
 *  为什么是「自己写驱动类 + 套 Adafruit_GFX」而不是直接用现成库：
 *    GxEPD2 只做到 1 色 / 3 色 / 4 色 / 7 色，没有 6 色，也没有 480x720 的面板；
 *    Adafruit EPD / U8g2 / Seeed_GFX 同样没有 JD7601 的驱动。
 *  但这颗屏缺的其实只是「最底层的 172800 字节怎么排」，图形与字体完全没必要重写。
 *  所以本库照 GxEPD2 的架构做：面板层自己实现，图形层继承 Adafruit_GFX。
 *  于是 drawLine / fillCircle / setFont / print 这些全部现成可用。
 *
 *  MIT / CC BY 4.0 —— 与仓库其余资料一致，可自由用于商业项目。
 * ============================================================================
 */

#ifndef EPD037A02_GFX_H
#define EPD037A02_GFX_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

/* ------------------------------- 面板常量 -------------------------------- */

#define EPD037A02_WIDTH    480
#define EPD037A02_HEIGHT   720
#define EPD037A02_BUF_BYTES ((size_t)EPD037A02_WIDTH * EPD037A02_HEIGHT / 2) /* 172800 */

/* 一字节装同一列相邻的两像素，半字节顺序若实测错位 1~2px，把这里改成 1 */
#ifndef EPD037A02_SWAP_NIBBLE
#define EPD037A02_SWAP_NIBBLE 0
#endif

/* JD7601 4bit 色值。0x4 是非法值，规范里直接跳过 —— 不是 0~5 连续编号。 */
enum EPDColor : uint8_t {
  EPD_BLACK  = 0x0,
  EPD_WHITE  = 0x1,
  EPD_YELLOW = 0x2,
  EPD_RED    = 0x3,
  EPD_BLUE   = 0x5,
  EPD_GREEN  = 0x6,
};

/* ------------------------------- 驱动类 ---------------------------------- */
/**
 * 用法：
 *   EPD037A02_GFX display(10, 9, 8, 7);    // cs, dc, rst, busy
 *   display.begin();                       // 分配帧缓存 + 复位 + INIT
 *   display.setRotation(0);                // 0/2 = 480x720, 1/3 = 720x480
 *   display.fillScreen(EPD_WHITE);
 *   display.setFont(&FreeSansBold24pt7b);
 *   display.setCursor(20, 120);  display.print("Hello");
 *   display.display();                     // 推屏 + 全刷（15~25 s）
 *
 * 注意：颜色是 4bit 索引色（只有 6 种），不是 RGB565。传 ST77XX_RED 之类
 * 的 16 位色值不会报错，但只有低 4 位参与取色（color & 0x0F），结果没有意义。
 */
class EPD037A02_GFX : public Adafruit_GFX {
 public:
  EPD037A02_GFX(int8_t cs, int8_t dc, int8_t rst, int8_t busy,
                int8_t sck = -1, int8_t mosi = -1, SPIClass *spi = &SPI);
  ~EPD037A02_GFX();

  /* 分配帧缓存、初始化 GPIO/SPI、复位面板并下发 INIT 命令。
   * 失败（内存不够 / BUSY 无响应）返回 false，屏幕保持原样。 */
  bool begin(uint32_t spi_hz = 2000000);

  /* 把帧缓存推屏并全刷。阻塞 15~25 s，返回 false 表示 BUSY 超时。 */
  bool display();

  /* 直接发送已打包好的 480x720 原生帧，不占用第二份照片帧缓存。 */
  bool displayRawFrame(const uint8_t *frame, size_t bytes);

  /* 面板深睡眠。唤醒需要重新上电或拉 RST + begin()。 */
  bool sleep();

  /* 只清帧缓存，不刷屏。彩色屏全刷会留残影，需要时用两次 display()。 */
  void clear(uint16_t color = EPD_WHITE);

  /* Adafruit_GFX 要求的唯一纯虚函数；fillScreen 覆盖成 memset 加速。 */
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillScreen(uint16_t color) override;

  /* 把任意 RGB 量化到这块屏最接近的 6 色之一（画照片 / JPEG 时用）。 */
  static uint8_t fromRGB(uint8_t r, uint8_t g, uint8_t b);

  uint8_t *buffer() const { return _buf; }
  bool ready() const { return _buf != nullptr; }
  uint32_t lastRefreshMs() const { return _lastRefreshMs; }
  void setVerbose(bool v) { _verbose = v; }

 private:
  int8_t _cs, _dc, _rst, _busy, _sck, _mosi;
  SPIClass *_spi;
  uint32_t _spiHz;
  uint8_t *_buf;
  uint32_t _lastRefreshMs;
  bool _verbose;

  void _alloc();
  void _spiSend(const uint8_t *buf, size_t len);
  void _cmd(uint8_t c);
  void _data(uint8_t d);
  void _dataBuf(const uint8_t *buf, size_t len);
  bool _waitBusy(const char *what, uint32_t timeout_ms = 60000);
  bool _reset();
  bool _initPanel();
};

#endif /* EPD037A02_GFX_H */
