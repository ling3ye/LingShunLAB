idf_component_register(SRCS "main.c" "ch422g.c" "lcd_draw.c" "lvgl_dashboard.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver lvgl esp_lvgl_port)
