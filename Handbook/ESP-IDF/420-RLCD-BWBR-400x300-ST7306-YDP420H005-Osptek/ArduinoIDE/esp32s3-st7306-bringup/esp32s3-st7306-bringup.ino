/*
 * ESP32-S3 + OSPTEK 4.2" 300x400 黑白红 LCD（ST7306 · SPI）测试套件 v7.5
 *
 * v7.5 改动（删除无效的状态回读）：
 *   用 esp32s3-st7306-readback-test 实测：SPI.begin(...,-1,MOSI,-1) 时 MISO 未接，
 *   transfer() 恒返回 0xFF，与寄存器地址、CS 是否选中都无关；把数据脚切成输入后
 *   用上拉/下拉各读一遍，结果是全 1 / 全 0，说明屏幕在读命令期间没有驱动数据线。
 *   结论：这块模组在当前接法下读不回寄存器，原来的 [DIAG] 0x09 日志只会误报 OK，
 *   已整段删除。判断链路好坏请看屏幕是否正常显示，不要看串口。
 *
 * v7.4 改动（修复 PAGE_CYAN 全屏填充）：
 *   实机对比 0x33 / 0x55 / 逐点 writePoint(C_CYAN)：
 *   0x33 与逐点写入都显示整屏青（视觉偏蓝），0x55 显示间隔条纹。
 *   逐点写入后的 30000 字节显存也全部为 0x33，故将 PAGE_CYAN 改为 0x33。
 *
 * v7.3 改动（整屏再旋转 180°）：
 *   现场演示时画面方向与原映射相反，需要整屏转 180°。
 *   在 writePoint 的软件映射前加一次逻辑坐标中心对称：
 *     x -> LCD_W-1-x, y -> LCD_H-1-y
 *   等价于 rx = 299 - y; ry = x;  用 ROT180 宏开关，置 0 可回到 v7.1 方向。
 *
 * v7.1 改动（横版适配 - 软件旋转，方向修正）：
 *   面板物理排线是竖版 300x400，用户横着摆放，视角为 400x300。
 *   用软件旋转：writePoint 把逻辑横版坐标映射到竖版显存。
 *   映射公式（v7.1 修正）：rx = y; ry = 399 - x;
 *   v7 曾用 rx=299-y, ry=x（顺时针），诊断方块出现在右下方，方向反了。
 *   v7.1 改为逆时针：方块应出现在左上方。
 * v7.2：关闭 DIAG_MODE，恢复完整 8 页测试循环。
 *
 * v4 关键修复：
 *   1) 补上 pinMode(LCD_CS/RS, OUTPUT)：digitalWrite 底层只是 gpio_set_level()，
 *      不会自动切输出模式，ESP32 引脚默认是输入，CS/D-C 悬空导致屏幕收不到
 *      任何有效命令/数据时序。这是之前几版无显示的真正根因。
 *   2) 引脚定义与 board_config.h（BOARD_PINSET 1）保持一致，接线无需改动：
 *      CS=9  RST=10  CLK=12  MOSI=13  RS(D/C)=14
 *   3) SPI 时钟 20MHz
 *   4) 补上 IDF 工程 app_main 中 initialize() 之后的步骤：
 *      High_Power_Mode / display_on / display_Inversion(false) / clearDisplay
 *   5) 颜色映射与 IDF board_color.h 一致：
 *      C_WHITE=0x00  C_CYAN=0x01  C_RED=0x02  C_BLACK=0x03
 *      （bit0=青, bit1=红, 两bit都开=黑）
 *
 * v5 新增 8 个测试页面（每页 3 秒，刷新率页 6 秒）：
 *   0 COLOR_BARS  四色横条（白/青/红/黑）
 *   1 HELLO       HELLO WORLD + 四角角标 + 边框
 *   2 COUNTER     计时 / 帧数 / FPS
 *   3 DIAGONAL    对角线 + 横竖线
 *   4 REFRESH     刷新率测试（移动方块）
 *   5 CHECKER     棋盘格
 *   6 STRIPES     横条纹
 *   7 CYAN        全屏青色
 */

#include <SPI.h>
#include <esp_heap_caps.h>

// ---------------- 引脚配置 ----------------
#define PINSET 1  // 1=新板  0=转接板

#if PINSET == 1
  // 引脚与 IDF 工程 board_config.h（BOARD_PINSET 1）完全一致，接线不需要改动
  #define LCD_CS   9
  #define LCD_RST  10
  #define LCD_CLK  12
  #define LCD_MOSI 13
  #define LCD_RS   14   // D/C，丝印 RS
#else
  #define LCD_CS   38
  #define LCD_RST  41
  #define LCD_CLK  39
  #define LCD_MOSI 42
  #define LCD_RS   40
#endif

// ---------------- 屏幕参数 ----------------
// 逻辑分辨率：横版 400x300（面板物理排线是竖版 300x400，用硬件 MV 位旋转）
#define LCD_W      400
#define LCD_H      300
// 竖版显存参数（不变）
#define LCD_PIVOT_W  300   // 物理宽
#define LCD_PIVOT_H  400   // 物理高
#define LCD_ROW_B    (LCD_PIVOT_W / 2)          // 每行 150 字节
#define BUF_LEN      (LCD_ROW_B * LCD_PIVOT_H / 2)  // 30000 字节
#define CHUNK        1500

// 颜色映射与 IDF board_color.h 一致
#define C_WHITE  0x00
#define C_CYAN   0x01
#define C_RED    0x02
#define C_BLACK  0x03

// 若仍然点不亮，可把 20000000 降到 10000000 再试
#define SPI_HZ  20000000

static uint8_t *fb = nullptr;

// ---------------- 底层 SPI（CS 全手动）----------------
static void spi_begin(void) {
  SPI.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
}

static void lcd_cmd(uint8_t cmd) {
  digitalWrite(LCD_CS, LOW);
  digitalWrite(LCD_RS, LOW);
  SPI.transfer(cmd);
  digitalWrite(LCD_CS, HIGH);
}

static void lcd_param(uint8_t p) {
  digitalWrite(LCD_CS, LOW);
  digitalWrite(LCD_RS, HIGH);
  SPI.transfer(p);
  digitalWrite(LCD_CS, HIGH);
}

// 这里原来有个 lcd_read_reg()，用 SPI.transfer() 读 0x09 状态寄存器。
// v7.5 删掉的原因（实测，不是推测）：
//   1) SPI.begin(LCD_CLK, -1, LCD_MOSI, -1) 的第二个参数是 MISO，传 -1 表示没接，
//      外设没有输入脚可采，transfer() 恒返回 0xFF。
//   2) 换成 0x04 / 0x0A / 0xDA / 0xA5 任意寄存器，或者把 CS 一直拉高根本
//      不选中屏幕，读回的还是 0xFF —— 说明这个值和屏幕没有任何关系。
//   3) 改成半双工（把 MOSI 脚切成输入、手动打时钟）再读，内部上拉得到全 1、
//      内部下拉得到全 0，说明读命令期间屏幕没有驱动数据线，读不回来。
// 所以这块模组在当前接法下没有可用的链路自检。判断接没接好，就看屏幕显不显示。

// ---------------- 初始化序列（与 IDF 逐条一致）----------------
static void st7306_init(void) {
  // 关键修复：必须显式把 CS / RS / RST 配成输出。
  // digitalWrite 底层只是 gpio_set_level()，不会自动切输出模式；
  // ESP32 引脚默认是输入，若不 pinMode，CS 和 D/C 全程悬空，屏幕收不到任何有效时序。
  pinMode(LCD_CS,  OUTPUT);
  pinMode(LCD_RS,  OUTPUT);
  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_CS, HIGH);   // 空闲时 CS 拉高
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_RST, HIGH); delay(10);
  digitalWrite(LCD_RST, LOW);  delay(10);
  digitalWrite(LCD_RST, HIGH); delay(10);

  // 最后一个参数传 -1，CS 完全由手动 digitalWrite 控制
  SPI.begin(LCD_CLK, -1, LCD_MOSI, -1);
  spi_begin();

  lcd_cmd(0xD6); lcd_param(0x17); lcd_param(0x02);
  lcd_cmd(0xD1); lcd_param(0x01);

  lcd_cmd(0xC0); lcd_param(0x12); lcd_param(0x0A);
  lcd_cmd(0xC1); lcd_param(115); lcd_param(0x3E); lcd_param(0x3C); lcd_param(0x3C);
  lcd_cmd(0xC2); lcd_param(0);   lcd_param(0x21); lcd_param(0x23); lcd_param(0x23);
  lcd_cmd(0xC4); lcd_param(50);  lcd_param(0x5C); lcd_param(0x5A); lcd_param(0x5A);
  lcd_cmd(0xC5); lcd_param(50);  lcd_param(0x35); lcd_param(0x37); lcd_param(0x37);

  lcd_cmd(0xD8); lcd_param(0xA6); lcd_param(0xE9);
  lcd_cmd(0xB2); lcd_param(0x12);

  lcd_cmd(0xB3); lcd_param(0xE5); lcd_param(0xF6); lcd_param(0x17);
                 lcd_param(0x77); lcd_param(0x77); lcd_param(0x77); lcd_param(0x77);
                 lcd_param(0x77); lcd_param(0x77); lcd_param(0x71);
  lcd_cmd(0xB4); lcd_param(0x05); lcd_param(0x46); lcd_param(0x77); lcd_param(0x77);
                 lcd_param(0x77); lcd_param(0x77); lcd_param(0x76); lcd_param(0x45);

  lcd_cmd(0x62); lcd_param(0x32); lcd_param(0x03); lcd_param(0x1F);

  lcd_cmd(0xB7); lcd_param(0x13);
  lcd_cmd(0xB0); lcd_param(0x64);

  lcd_cmd(0x11); delay(120);                         // Sleep Out

  lcd_cmd(0xC9); lcd_param(0x00);
  lcd_cmd(0x36); lcd_param(0x48);                    // MX=1, DO=1
  lcd_cmd(0x3A); lcd_param(0x11);
  lcd_cmd(0xB9); lcd_param(0x20);
  lcd_cmd(0xB8); lcd_param(0x29);

  lcd_cmd(0x2A); lcd_param(0x05); lcd_param(0x36);
  lcd_cmd(0x2B); lcd_param(0x00); lcd_param(0xC7);

  lcd_cmd(0x35); lcd_param(0x00);
  lcd_cmd(0xD0); lcd_param(0xFF);
  lcd_cmd(0x38);                                     // HPM

  lcd_cmd(0x29);                                     // DISPLAY ON
  lcd_cmd(0x20);                                     // Inversion Off

  lcd_cmd(0xBB); lcd_param(0x4F);                    // Enable Clear RAM

  // ---- IDF app_main 在 initialize() 之后还会调用的步骤 ----
  lcd_cmd(0x38); delay(300);                         // High_Power_Mode() 的 HPM 序列
  lcd_cmd(0xC1); lcd_param(115); lcd_param(0x3E); lcd_param(0x3C); lcd_param(0x3C);
  lcd_cmd(0xC2); lcd_param(0);   lcd_param(0x21); lcd_param(0x23); lcd_param(0x23);
  lcd_cmd(0xC4); lcd_param(50);  lcd_param(0x5C); lcd_param(0x5A); lcd_param(0x5A);
  lcd_cmd(0xC5); lcd_param(50);  lcd_param(0x35); lcd_param(0x37); lcd_param(0x37);
  lcd_cmd(0xC9); lcd_param(0x00);
  delay(20);
  // display_on(true) / display_Inversion(false) 已在上面发过 0x29/0x20
  // clearDisplay() 等价于显存清零，由刷屏前的 memset 完成

  SPI.endTransaction();
}

// ---------------- 刷屏（分块发送）----------------
static void st7306_flush(void) {
  spi_begin();
  lcd_cmd(0x2A); lcd_param(0x05); lcd_param(0x36);
  lcd_cmd(0x2B); lcd_param(0x00); lcd_param(0xC7);
  lcd_cmd(0x2C);

  digitalWrite(LCD_CS, LOW);
  digitalWrite(LCD_RS, HIGH);
  for (size_t off = 0; off < BUF_LEN; off += CHUNK) {
    size_t n = BUF_LEN - off;
    if (n > CHUNK) n = CHUNK;
    SPI.writeBytes(fb + off, n);
  }
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

// ---------------- 像素操作 ----------------
// 逻辑坐标：横版 400x300
// 竖版显存 300(列) x 400(行)，软件旋转映射：
//   横版 x (0~399, 左到右) -> 竖版行 ry = 399 - x（左右翻转）
//   横版 y (0~299, 上到下) -> 竖版列 rx = y
// bit 位段用竖版物理坐标 rx/ry 的奇偶计算。
//
// ROT180：把整屏再旋转 180°（面板装反 / 演示摆放方向相反时用）。
//   做法是先把逻辑坐标做中心对称 x -> 399-x, y -> 299-y，再套用上面的映射，
//   化简后等价于：rx = 299 - y; ry = x;
#define ROT180 1   // 1 = 旋转 180°（当前）；0 = 恢复 v7.1 原方向

static void writePoint(uint16_t x, uint16_t y, uint16_t data) {
  if (x >= LCD_W || y >= LCD_H) return;
#if ROT180
  x = LCD_W - 1 - x;       // 逻辑 x 镜像
  y = LCD_H - 1 - y;       // 逻辑 y 镜像
#endif
  uint16_t rx = y;         // 竖版列
  uint16_t ry = 399 - x;   // 竖版行（翻转）
  uint16_t real_x = rx / 2;
  uint16_t real_y = ry / 2;
  uint32_t idx    = (uint32_t)real_y * LCD_ROW_B + real_x;
  uint8_t one_two    = (ry % 2) ? 1 : 0;
  uint8_t line_bit_1 = (rx % 2) * 4;
  uint8_t line_bit_0 = (rx % 2) * 4 + 2;
  uint8_t bit1       = 7 - (line_bit_1 + one_two);
  uint8_t bit0       = 7 - (line_bit_0 + one_two);
  if (data & 0x02) fb[idx] |=  (1 << bit1);
  else             fb[idx] &= ~(1 << bit1);
  if (data & 0x01) fb[idx] |=  (1 << bit0);
  else             fb[idx] &= ~(1 << bit0);
}

static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  for (uint16_t j = 0; j < h; j++)
    for (uint16_t i = 0; i < w; i++)
      writePoint(x + i, y + j, color);
}

// ---------------- 5x7 字模（移植自 IDF board_ui_draw.cc）----------------
static const uint8_t s_font_5x7[][7] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /*   */
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}, /* ! */
    {0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00}, /* " */
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}, /* # */
    {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04}, /* $ */
    {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}, /* % */
    {0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D}, /* & */
    {0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}, /* ( */
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}, /* ) */
    {0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00}, /* * */
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, /* + */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08}, /* , */
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, /* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00}, /* . */
    {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00}, /* / */
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}, /* 5 */
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}, /* 9 */
    {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00}, /* : */
};

// 大写字母 A-Z（5x7）
static const uint8_t s_font_upper[][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E}, /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}, /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
};

static void draw_char(int x, int y, char c, int scale, uint16_t color) {
    const uint8_t *rows = nullptr;
    if (c >= ' ' && c <= ':') {
        rows = s_font_5x7[c - ' '];
    } else if (c >= 'A' && c <= 'Z') {
        rows = s_font_upper[c - 'A'];
    } else if (c >= 'a' && c <= 'z') {
        rows = s_font_upper[c - 'a'];   // 复用大写字模
    } else {
        return;
    }
    for (int ry = 0; ry < 7; ++ry) {
        for (int rx = 0; rx < 5; ++rx) {
            if (rows[ry] & (1U << (4 - rx))) {
                fillRect(x + rx * scale, y + ry * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text(int x, int y, const char *s, int scale, uint16_t color) {
    const int char_w = 5 * scale + scale;
    int pen_x = x;
    for (const char *p = s; *p; ++p) {
        draw_char(pen_x, y, *p, scale, color);
        pen_x += char_w;
    }
}

// ---------------- 测试页面 ----------------
enum TestPage {
    PAGE_COLOR_BARS = 0,   // 四色横条（白/青/红/黑）
    PAGE_HELLO,            // Hello World + 四角角标 + 边框
    PAGE_COUNTER,          // 秒数 / 帧数计数
    PAGE_DIAGONAL,         // 对角线 + 横竖线
    PAGE_REFRESH,          // 刷新率测试（移动方块）
    PAGE_CHECKER,          // 棋盘格
    PAGE_STRIPES,          // 横条纹
    PAGE_CYAN,             // 全屏青色
    kTotalPages
};

static void draw_corner_marks(uint16_t color) {
    const int L = 30, T = 4;  // 臂长、臂粗
    // 左上
    fillRect(4, 4, L, T, color);  fillRect(4, 4, T, L, color);
    // 右上
    fillRect(LCD_W - 4 - L, 4, L, T, color);  fillRect(LCD_W - 4 - T, 4, T, L, color);
    // 左下
    fillRect(4, LCD_H - 4 - T, L, T, color);  fillRect(4, LCD_H - 4 - L, T, L, color);
    // 右下
    fillRect(LCD_W - 4 - L, LCD_H - 4 - T, L, T, color);
    fillRect(LCD_W - 4 - T, LCD_H - 4 - L, T, L, color);
}

static void draw_border(uint16_t color) {
    fillRect(0, 0, LCD_W, 2, color);
    fillRect(0, LCD_H - 2, LCD_W, 2, color);
    fillRect(0, 0, 2, LCD_H, color);
    fillRect(LCD_W - 2, 0, 2, LCD_H, color);
}

// 画一条任意方向直线（Bresenham）
static void draw_line(int x0, int y0, int x1, int y1, int thick, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        fillRect(x0, y0, thick, thick, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void render_page(int page, uint32_t frame, uint32_t elapsed_ms) {
    memset(fb, 0x00, BUF_LEN);   // 默认全白

    switch (page) {
    case PAGE_COLOR_BARS: {
        const uint16_t colors[4] = { C_WHITE, C_CYAN, C_RED, C_BLACK };
        const int bar_h = LCD_H / 4;
        for (int i = 0; i < 4; ++i) {
            int y0 = i * bar_h;
            int y1 = (i == 3) ? LCD_H : (y0 + bar_h);
            fillRect(0, y0, LCD_W, y1 - y0, colors[i]);
        }
        draw_text(10, 10, "1: COLOR BARS", 2, C_BLACK);
        break;
    }
    case PAGE_HELLO:
        // "HELLO WORLD" 11 字符 * scale5 * 6px = 330px 宽，居中 x=(400-330)/2=35
        draw_text(35, 110, "HELLO WORLD", 5, C_BLACK);
        draw_text(110, 175, "ST7306 400X300", 2, C_RED);
        draw_text(110, 200, "ESP32-S3 ARDUINO", 2, C_CYAN);
        draw_corner_marks(C_BLACK);
        draw_border(C_RED);
        break;
    case PAGE_COUNTER: {
        char buf[24];
        uint32_t sec = elapsed_ms / 1000;
        snprintf(buf, sizeof(buf), "T: %02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
        draw_text(115, 80, buf, 5, C_BLACK);        // 8*5*6=240 宽，x=(400-240)/2
        snprintf(buf, sizeof(buf), "FRAME: %lu", (unsigned long)frame);
        draw_text(140, 160, buf, 3, C_RED);         // 11*3*6=198 宽
        snprintf(buf, sizeof(buf), "FPS: %lu", (unsigned long)(frame * 1000 / max((uint32_t)1, elapsed_ms)));
        draw_text(152, 200, buf, 3, C_CYAN);        // 9*3*6=162 宽
        break;
    }
    case PAGE_DIAGONAL:
        // 两条对角线
        draw_line(0, 0, LCD_W - 1, LCD_H - 1, 3, C_BLACK);
        draw_line(LCD_W - 1, 0, 0, LCD_H - 1, 3, C_RED);
        // 横线
        for (int y = 30; y < LCD_H; y += 60)
            draw_line(0, y, LCD_W - 1, y, 2, C_CYAN);
        // 竖线
        for (int x = 40; x < LCD_W; x += 80)
            draw_line(x, 0, x, LCD_H - 1, 2, C_BLACK);
        draw_text(10, 10, "3: DIAGONAL", 2, C_BLACK);
        break;
    case PAGE_REFRESH: {
        // 方块左右往返，观察运动平滑度
        int period = LCD_W - 60;
        int x = (int)((frame * 8) % (period * 2));
        if (x > period) x = period * 2 - x;
        fillRect(x + 10, 130, 40, 40, C_BLACK);     // 垂直居中 y=(300-40)/2
        char buf[24];
        snprintf(buf, sizeof(buf), "FRAME %lu", (unsigned long)frame);
        draw_text(10, 10, buf, 2, C_BLACK);
        break;
    }
    case PAGE_CHECKER: {
        const int cell = 20;
        for (int y = 0; y < LCD_H; y += cell)
            for (int x = 0; x < LCD_W; x += cell)
                fillRect(x, y, cell, cell, ((x / cell + y / cell) & 1) ? C_BLACK : C_WHITE);
        break;
    }
    case PAGE_STRIPES: {
        const int stripe_h = 16;
        for (int y = 0; y < LCD_H; y += stripe_h * 2) {
            fillRect(0, y, LCD_W, stripe_h, C_BLACK);
            fillRect(0, y + stripe_h, LCD_W, stripe_h, C_WHITE);
        }
        break;
    }
    case PAGE_CYAN:
        memset(fb, 0x33, BUF_LEN);   // 0x33 = 每个合并像素 01 -> 全屏青
        draw_text(85, 130, "CYAN FULL", 5, C_BLACK);   // 9*5*6=270 宽，x=(400-270)/2
        break;
    default:
        break;
    }

    st7306_flush();
}

// ---------------- Arduino 入口 ----------------

// 诊断模式：true 时只画一个 50x50 黑方块在逻辑 (50,50)，
// 用于判断 writePoint 映射 + MV 位组合是否正确。
// 观察要点：
//   1. 方块是正方形（不是矩形/碎片）-> 像素映射比例正确
//   2. 方块在横版视角的左上 1/4 区域 -> 方向正确
//   3. 若整体反了 -> 改 MADCTL(0x36) 的 MX/MY 位
#define DIAG_MODE  false

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("=== ST7306 test suite v7.5 (ROT180) ===");

    fb = (uint8_t *)heap_caps_malloc(BUF_LEN, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!fb) fb = (uint8_t *)malloc(BUF_LEN);
    if (!fb) { Serial.println("[ERR] 显存分配失败"); while (1) delay(100); }
    Serial.printf("[OK] 显存 %d 字节 @%p\n", BUF_LEN, fb);
    memset(fb, 0x00, BUF_LEN);

    Serial.println("[..] 初始化 ST7306 ...");
    st7306_init();

    // 这里原来会读一次 0x09 状态寄存器并打印 OK / 链路不通。
    // v7.5 删除：MISO 没接，读回的值恒为 0xFF，和屏幕状态无关，只会误报。
    // 链路是否正常，看屏幕有没有正常出画面。

    if (DIAG_MODE) {
        Serial.println("[DIAG] 画 50x50 方块 at (50,50)，观察屏幕...");
        fillRect(50, 50, 50, 50, C_BLACK);
        st7306_flush();
    } else {
        Serial.println("[OK] 进入测试页面循环");
    }
}

void loop() {
    if (DIAG_MODE) { delay(1000); return; }

    static int page = 0;
    static uint32_t page_start = 0;
    static uint32_t frame = 0;
    static uint32_t t_start = 0;

    if (t_start == 0) t_start = millis();
    if (page_start == 0) page_start = millis();

    uint32_t dwell = (page == PAGE_REFRESH) ? 6000 : 3000;
    uint32_t elapsed = millis() - page_start;

    render_page(page, frame, millis() - t_start);
    frame++;

    if (elapsed >= dwell) {
        page = (page + 1) % kTotalPages;
        page_start = millis();
        Serial.printf("[PAGE] %d / %d, frame %lu\n", page, kTotalPages - 1, (unsigned long)frame);
    }
}
