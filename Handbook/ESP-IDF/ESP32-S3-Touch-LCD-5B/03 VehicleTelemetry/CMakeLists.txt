# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Bridge an ESP-IDF macro rename so esp_lvgl_port 2.8's RGB-panel path compiles
# on IDF 5.2.7. esp_lvgl_port gates its RGB code on SOC_LCDCAM_RGB_LCD_SUPPORTED
# (the name used since IDF 5.3); IDF 5.2.7 still calls it SOC_LCD_RGB_SUPPORTED,
# so without this the RGB guards #if-out and lvgl_port_add_disp_rgb() aborts at
# runtime with "This target does not support RGB." Every API the guarded code
# calls exists in 5.2.7, so aliasing the new name to the value ESP32-S3 already
# advertises is safe. Must be set before project() so component targets inherit it.
add_compile_definitions(SOC_LCDCAM_RGB_LCD_SUPPORTED=1)

project(testDisplay)