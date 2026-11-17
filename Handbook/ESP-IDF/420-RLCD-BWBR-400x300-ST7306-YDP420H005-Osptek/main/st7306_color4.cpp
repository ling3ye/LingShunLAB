/**
 * @file st7306_color4.cpp
 * @brief ST7306 4 色驱动实现。
 *
 * 类本体与 bringup 工程一致；这里额外提供 extern "C" 的 handle 接口，
 * 让上面的 demo 全部用 C 写，便于和黑白版 demo 逐页对照。
 */
#include "st7306_color4.h"

#include <cstring>
#include <cstdlib>
#include <new>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ST7306";

namespace {

class St7306Color4 {
public:
    St7306Color4(const st7306_config_t &cfg) : cfg_(cfg)
    {
        buffer_ = static_cast<uint8_t *>(
            heap_caps_malloc(ST7306_BUFFER_LENGTH, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
        if (buffer_) {
            memset(buffer_, 0x00, ST7306_BUFFER_LENGTH);
        }
    }

    ~St7306Color4()
    {
        if (spi_dev_) {
            spi_bus_remove_device(spi_dev_);
            spi_dev_ = nullptr;
        }
        if (buffer_) {
            heap_caps_free(buffer_);
            buffer_ = nullptr;
        }
    }

    bool init()
    {
        if (!buffer_) {
            ESP_LOGE(TAG, "framebuffer alloc failed (%d bytes)", ST7306_BUFFER_LENGTH);
            return false;
        }

        gpio_config_t io = {};
        io.mode = GPIO_MODE_OUTPUT;
        io.pin_bit_mask = (1ULL << cfg_.gpio_dc) | (1ULL << cfg_.gpio_res);
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&io));

        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = cfg_.gpio_sclk;
        buscfg.mosi_io_num = cfg_.gpio_mosi;
        buscfg.miso_io_num = -1;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = ST7306_BUFFER_LENGTH + 16;
        ESP_ERROR_CHECK(spi_bus_initialize(static_cast<spi_host_device_t>(cfg_.spi_host),
                                           &buscfg, SPI_DMA_CH_AUTO));

        spi_device_interface_config_t dev = {};
        dev.clock_speed_hz = cfg_.clock_hz;
        dev.mode = 0;
        dev.spics_io_num = cfg_.gpio_cs;
        dev.queue_size = 4;
        dev.flags = SPI_DEVICE_NO_DUMMY;
        ESP_ERROR_CHECK(spi_bus_add_device(static_cast<spi_host_device_t>(cfg_.spi_host),
                                           &dev, &spi_dev_));

        gpio_set_level(static_cast<gpio_num_t>(cfg_.gpio_res), 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(static_cast<gpio_num_t>(cfg_.gpio_res), 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(static_cast<gpio_num_t>(cfg_.gpio_res), 1);
        vTaskDelay(pdMS_TO_TICKS(10));

        init_panel();
        clear();
        return true;
    }

    void clear() { memset(buffer_, 0x00, ST7306_BUFFER_LENGTH); }

    void fill(uint8_t value)
    {
        /* value 是 2bit 颜色值。一个字节打包 4 个像素，但两个 bit 平面是
         * 交错排布的（bit7/6/3/2 = bit1 平面，bit5/4/1/0 = bit0 平面），
         * 所以不能把 2bit 颜色简单左移拼接，必须按平面掩码展开：
         *   青(0x01) -> 0x33，红(0x02) -> 0xCC，黑(0x03) -> 0xFF */
        uint8_t packed = 0x00;
        if (value & 0x2) {
            packed |= 0xCC;
        }
        if (value & 0x1) {
            packed |= 0x33;
        }
        memset(buffer_, packed, ST7306_BUFFER_LENGTH);
    }

    inline void write_point(int x, int y, uint16_t color)
    {
        if (static_cast<unsigned>(x) >= ST7306_LCD_WIDTH ||
            static_cast<unsigned>(y) >= ST7306_LCD_HEIGHT) {
            return;
        }
        const int real_x = x / 2;         /* 2 个像素共用一个字节 */
        const int real_y = y / 2;         /* 上下两行共用一行数据 */
        const int byte_index = real_y * ST7306_DATA_WIDTH + real_x;
        const int row_half = y & 1;       /* 0=上行, 1=下行 */
        const int bit1 = (x & 1) * 4;     /* bit1 平面偏移 */
        const int bit0 = bit1 + 2;        /* bit0 平面偏移 */
        const uint8_t mask1 = static_cast<uint8_t>(1u << (7 - (bit1 + row_half)));
        const uint8_t mask0 = static_cast<uint8_t>(1u << (7 - (bit0 + row_half)));

        if (color & 0x2) {
            buffer_[byte_index] |= mask1;
        } else {
            buffer_[byte_index] &= static_cast<uint8_t>(~mask1);
        }
        if (color & 0x1) {
            buffer_[byte_index] |= mask0;
        } else {
            buffer_[byte_index] &= static_cast<uint8_t>(~mask0);
        }
    }

    /* 矩形填充：按 2x2 像素块处理，整块同色时一次写 2 个 bit 平面。
     * 边界上的奇数列/奇数行再退回逐点写，保证与 write_point 结果完全一致。 */
    void fill_rect(int x0, int y0, int x1, int y1, uint16_t color)
    {
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= ST7306_LCD_WIDTH) x1 = ST7306_LCD_WIDTH - 1;
        if (y1 >= ST7306_LCD_HEIGHT) y1 = ST7306_LCD_HEIGHT - 1;
        if (x0 > x1 || y0 > y1) {
            return;
        }

        const bool bit0 = (color & 0x1) != 0;
        const bool bit1 = (color & 0x2) != 0;

        /* 先把上/下/左/右的奇数边界用逐点补掉，中间留出完整 2x2 块区域 */
        int bx0 = x0, by0 = y0, bx1 = x1, by1 = y1;
        if (bx0 & 1) {
            for (int y = y0; y <= y1; y++) write_point(bx0, y, color);
            bx0++;
        }
        if (!(bx1 & 1)) {
            for (int y = y0; y <= y1; y++) write_point(bx1, y, color);
            bx1--;
        }
        if (by0 & 1) {
            for (int x = bx0; x <= bx1; x++) write_point(x, by0, color);
            by0++;
        }
        if (!(by1 & 1)) {
            for (int x = bx0; x <= bx1; x++) write_point(x, by1, color);
            by1--;
        }

        if (bx0 > bx1 || by0 > by1) {
            return;
        }

        /* 一个字节含 4 个像素（2 列 x 2 行）：
         *   bit7/6 = 偶数列上/下行的 bit1 平面，bit3/2 = 奇数列上/下行的 bit1 平面
         *   bit5/4 = 偶数列上/下行的 bit0 平面，bit1/0 = 奇数列上/下行的 bit0 平面
         * 所以整块同色时，只需按平面掩码置位/清位。 */
        const uint8_t mask_bit1 = 0xCC;      /* 每个半字节的高 2 位 = bit1 平面 */
        const uint8_t mask_bit0 = 0x33;      /* 每个半字节的低 2 位 = bit0 平面 */

        for (int y = by0; y <= by1; y += 2) {
            const int real_y = y / 2;
            uint8_t *row = buffer_ + real_y * ST7306_DATA_WIDTH;
            for (int x = bx0; x <= bx1; x += 2) {
                const int real_x = x / 2;
                uint8_t value = row[real_x];
                if (bit1) {
                    value |= mask_bit1;
                } else {
                    value &= static_cast<uint8_t>(~mask_bit1);
                }
                if (bit0) {
                    value |= mask_bit0;
                } else {
                    value &= static_cast<uint8_t>(~mask_bit0);
                }
                row[real_x] = value;
            }
        }
    }

    esp_err_t display()
    {
        write_cmd(0x2A);
        write_param(0x05);
        write_param(0x36);
        write_cmd(0x2B);
        write_param(0x00);
        write_param(0xC7);
        write_cmd(0x2C);

        gpio_set_level(static_cast<gpio_num_t>(cfg_.gpio_dc), 1);
        spi_transaction_t t = {};
        t.length = ST7306_BUFFER_LENGTH * 8;
        t.tx_buffer = buffer_;
        return spi_device_polling_transmit(spi_dev_, &t);
    }

    void high_power_mode()
    {
        if (hpm_) {
            return;
        }
        hpm_ = true;
        write_cmd(0x38);
        vTaskDelay(pdMS_TO_TICKS(300));
        voltage_setting();
        write_cmd(0xC9);
        write_param(0x00);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    void low_power_mode()
    {
        if (!hpm_) {
            return;
        }
        hpm_ = false;
        voltage_setting();
        write_cmd(0xC9);
        write_param(0x00);
        vTaskDelay(pdMS_TO_TICKS(20));
        write_cmd(0x39);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    void display_on(bool enabled) { write_cmd(enabled ? 0x29 : 0x28); }
    void display_inversion(bool enabled) { write_cmd(enabled ? 0x21 : 0x20); }

    void display_sleep(bool enabled)
    {
        if (enabled) {
            if (!hpm_) {
                write_cmd(0x38);
                vTaskDelay(pdMS_TO_TICKS(300));
                hpm_ = true;
            }
            write_cmd(0x10);
        } else {
            write_cmd(0x11);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    uint8_t *buffer() { return buffer_; }

private:
    esp_err_t write_cmd(uint8_t cmd)
    {
        gpio_set_level(static_cast<gpio_num_t>(cfg_.gpio_dc), 0);
        spi_transaction_t t = {};
        t.length = 8;
        t.tx_buffer = &cmd;
        return spi_device_polling_transmit(spi_dev_, &t);
    }

    esp_err_t write_param(uint8_t param)
    {
        gpio_set_level(static_cast<gpio_num_t>(cfg_.gpio_dc), 1);
        spi_transaction_t t = {};
        t.length = 8;
        t.tx_buffer = &param;
        return spi_device_polling_transmit(spi_dev_, &t);
    }

    void voltage_setting()
    {
        write_cmd(0xC1);   /* VSHP */
        write_param(115);
        write_param(0x3E);
        write_param(0x3C);
        write_param(0x3C);

        write_cmd(0xC2);   /* VSLP */
        write_param(0);
        write_param(0x21);
        write_param(0x23);
        write_param(0x23);

        write_cmd(0xC4);   /* VSHN */
        write_param(50);
        write_param(0x5C);
        write_param(0x5A);
        write_param(0x5A);

        write_cmd(0xC5);   /* VSLN */
        write_param(50);
        write_param(0x35);
        write_param(0x37);
        write_param(0x37);
    }

    void init_panel()
    {
        write_cmd(0xD6);
        write_param(0x17);
        write_param(0x02);

        write_cmd(0xD1);
        write_param(0x01);

        write_cmd(0xC0);   /* Gate Voltage */
        write_param(0x12); /* VGH 17V */
        write_param(0x0A); /* VGL -10V */

        voltage_setting();

        write_cmd(0xD8);   /* OSC */
        write_param(0xA6);
        write_param(0xE9);

        write_cmd(0xB2);   /* Frame Rate Control: HPM 32Hz / LPM 1Hz */
        write_param(0x12);

        write_cmd(0xB3);   /* HPM Gate EQ */
        write_param(0xE5);
        write_param(0xF6);
        write_param(0x17);
        write_param(0x77);
        write_param(0x77);
        write_param(0x77);
        write_param(0x77);
        write_param(0x77);
        write_param(0x77);
        write_param(0x71);

        write_cmd(0xB4);   /* LPM Gate EQ */
        write_param(0x05);
        write_param(0x46);
        write_param(0x77);
        write_param(0x77);
        write_param(0x77);
        write_param(0x77);
        write_param(0x76);
        write_param(0x45);

        write_cmd(0x62);   /* Gate Timing */
        write_param(0x32);
        write_param(0x03);
        write_param(0x1F);

        write_cmd(0xB7);   /* Source EQ */
        write_param(0x13);

        write_cmd(0xB0);   /* Gate Line = 400 */
        write_param(0x64);

        write_cmd(0x11);   /* Sleep out */
        vTaskDelay(pdMS_TO_TICKS(120));

        write_cmd(0xC9);   /* Source Voltage Select */
        write_param(0x00);

        write_cmd(0x36);   /* Memory Data Access Control: MX=1, DO=1 */
        write_param(0x48);

        write_cmd(0x3A);   /* Data Format: 3 write for 24bit */
        write_param(0x11);

        write_cmd(0xB9);   /* Gamma Mode */
        write_param(0x20);

        write_cmd(0xB8);   /* Panel Setting */
        write_param(0x29);

        write_cmd(0x2A);   /* Column address 5..54 */
        write_param(0x05);
        write_param(0x36);

        write_cmd(0x2B);   /* Row address 0..199 */
        write_param(0x00);
        write_param(0xC7);

        write_cmd(0x35);   /* TE */
        write_param(0x00);

        write_cmd(0xD0);   /* Auto power down */
        write_param(0xFF);

        write_cmd(0x38);   /* HPM */
        hpm_ = true;

        write_cmd(0x29);   /* Display ON */
        write_cmd(0x20);   /* Inversion OFF */

        write_cmd(0xBB);   /* Clear RAM */
        write_param(0x4F);
    }

    st7306_config_t cfg_;
    spi_device_handle_t spi_dev_ = nullptr;
    uint8_t *buffer_ = nullptr;
    bool hpm_ = true;
};

}  // namespace

/* ------------------------------------------------------------------ */
/* C 接口                                                             */
/* ------------------------------------------------------------------ */

extern "C" {

st7306_handle_t st7306_init(const st7306_config_t *cfg)
{
    if (!cfg) {
        return nullptr;
    }
    St7306Color4 *dev = new (std::nothrow) St7306Color4(*cfg);
    if (!dev) {
        return nullptr;
    }
    if (!dev->init()) {
        delete dev;
        return nullptr;
    }
    ESP_LOGI(TAG, "ST7306 4-color panel ready (%dx%d, SPI %d Hz)",
             ST7306_LCD_WIDTH, ST7306_LCD_HEIGHT, cfg->clock_hz);
    return reinterpret_cast<st7306_handle_t>(dev);
}

void st7306_deinit(st7306_handle_t dev)
{
    delete reinterpret_cast<St7306Color4 *>(dev);
}

void st7306_clear(st7306_handle_t dev)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->clear();
}

void st7306_fill(st7306_handle_t dev, uint8_t color)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->fill(color);
}

void st7306_write_point(st7306_handle_t dev, int x, int y, uint16_t color)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->write_point(x, y, color);
}

void st7306_fill_rect(st7306_handle_t dev, int x0, int y0, int x1, int y1, uint16_t color)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->fill_rect(x0, y0, x1, y1, color);
}

esp_err_t st7306_display(st7306_handle_t dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    return reinterpret_cast<St7306Color4 *>(dev)->display();
}

void st7306_high_power_mode(st7306_handle_t dev)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->high_power_mode();
}

void st7306_low_power_mode(st7306_handle_t dev)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->low_power_mode();
}

void st7306_display_on(st7306_handle_t dev, bool enabled)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->display_on(enabled);
}

void st7306_display_sleep(st7306_handle_t dev, bool enabled)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->display_sleep(enabled);
}

void st7306_display_inversion(st7306_handle_t dev, bool enabled)
{
    if (!dev) return;
    reinterpret_cast<St7306Color4 *>(dev)->display_inversion(enabled);
}

uint8_t *st7306_get_buffer(st7306_handle_t dev)
{
    if (!dev) return nullptr;
    return reinterpret_cast<St7306Color4 *>(dev)->buffer();
}

int st7306_get_buffer_length(st7306_handle_t dev)
{
    (void)dev;
    return ST7306_BUFFER_LENGTH;
}

}  // extern "C"
