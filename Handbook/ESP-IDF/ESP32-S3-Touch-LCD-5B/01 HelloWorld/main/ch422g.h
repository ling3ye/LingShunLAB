/**
 * Minimal CH422G I/O-expander driver for the Waveshare ESP32-S3-Touch-LCD-5.
 *
 * On this board the LCD reset, touch reset and LCD backlight are *not* direct
 * ESP32 GPIOs - they are EXIO outputs of the onboard CH422G, reached over I2C
 * (GPIO8=SDA, GPIO9=SCL). The panel stays dark until we drive the CH422G to
 * release reset and turn the backlight on.
 *
 * CH422G quirk: it uses the I2C 7-bit *address itself* as the register/command
 * (there is no separate register byte). Relevant commands:
 *   0x24 = mode register  (write 0x01 -> push-pull output on EXIO0..EXIO7)
 *   0x38 = EXIO0..EXIO7 output byte (bit n = level of EXIO_n, 1 = high)
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* EXIO output bit positions (push-pull outputs EXIO0..EXIO7). */
#define CH422G_EXIO0    (1u << 0)   /* DI0      */
#define CH422G_TP_RST   (1u << 1)   /* EXIO1 - touch reset   */
#define CH422G_BL       (1u << 2)   /* EXIO2 - LCD backlight (DISP) */
#define CH422G_LCD_RST  (1u << 3)   /* EXIO3 - LCD reset     */
#define CH422G_SD_CS    (1u << 4)   /* EXIO4 - SD card CS     */
#define CH422G_EXIO5    (1u << 5)   /* DI1      */

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev_mode;   /* I2C device at address 0x24 (mode)  */
    i2c_master_dev_handle_t dev_out;    /* I2C device at address 0x38 (outputs) */
    uint8_t outputs;                    /* cached EXIO0..7 state */
} ch422g_handle_t;

/**
 * Initialise the CH422G on the given I2C bus and enable push-pull output mode
 * on EXIO0..EXIO7. All outputs start low.
 */
esp_err_t ch422g_init(ch422g_handle_t *ch, i2c_master_bus_handle_t bus);

/** Set the EXIO0..EXIO7 outputs to the given bitmask (OR of CH422G_* bits). */
esp_err_t ch422g_set_outputs(ch422g_handle_t *ch, uint8_t exio_mask);
