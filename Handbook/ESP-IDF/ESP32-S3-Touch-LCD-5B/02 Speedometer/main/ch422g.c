/**
 * Minimal CH422G I/O-expander driver - see ch422g.h for the protocol notes.
 */
#include "ch422g.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "ch422g";

/* CH422G "register" = I2C 7-bit device address. */
#define CH422G_REG_MODE  0x24
#define CH422G_REG_OUT   0x38
/* Mode value: enable push-pull output on EXIO0..EXIO7. */
#define CH422G_MODE_PP_OUT 0x01

#define CH422G_I2C_FREQ_HZ 100000

esp_err_t ch422g_init(ch422g_handle_t *ch, i2c_master_bus_handle_t bus)
{
    memset(ch, 0, sizeof(*ch));
    ch->bus = bus;

    /* One I2C device handle per "register" (address-as-register quirk). */
    i2c_device_config_t mode_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = CH422G_REG_MODE,
        .scl_speed_hz    = CH422G_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &mode_cfg, &ch->dev_mode),
                        TAG, "add mode device");

    i2c_device_config_t out_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = CH422G_REG_OUT,
        .scl_speed_hz    = CH422G_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &out_cfg, &ch->dev_out),
                        TAG, "add output device");

    /* Enable push-pull output mode on EXIO0..EXIO7. */
    uint8_t mode = CH422G_MODE_PP_OUT;
    ESP_RETURN_ON_ERROR(i2c_master_transmit(ch->dev_mode, &mode, 1, -1),
                        TAG, "write mode");

    /* Start from a known state: all EXIO outputs low. */
    ch->outputs = 0;
    uint8_t zero = 0;
    ESP_RETURN_ON_ERROR(i2c_master_transmit(ch->dev_out, &zero, 1, -1),
                        TAG, "clear outputs");

    ESP_LOGI(TAG, "CH422G ready (mode=0x%02x)", mode);
    return ESP_OK;
}

esp_err_t ch422g_set_outputs(ch422g_handle_t *ch, uint8_t exio_mask)
{
    ch->outputs = exio_mask;
    /* The single data byte IS the EXIO0..7 level; the "register" is the
     * device address (0x38) baked into ch->dev_out. */
    esp_err_t err = i2c_master_transmit(ch->dev_out, &exio_mask, 1, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set outputs 0x%02x failed: %s", exio_mask, esp_err_to_name(err));
    }
    return err;
}
