#pragma once

#include "driver/gpio.h"

/* 与 bringup 项目 / 黑白版相同的接线方案（新板） */
#define BOARD_PIN_LCD_CS          GPIO_NUM_9
#define BOARD_PIN_LCD_RST         GPIO_NUM_10
#define BOARD_PIN_LCD_TE          GPIO_NUM_11
#define BOARD_PIN_LCD_CLK         GPIO_NUM_12
#define BOARD_PIN_LCD_MOSI        GPIO_NUM_13
#define BOARD_PIN_LCD_RS          GPIO_NUM_14

/* 兼容旧宏：LCD 驱动 / 构造函数仍用这组名字（不要加括号，会和类成员名冲突） */
#define DC_PIN                    BOARD_PIN_LCD_RS    /* D/C，丝印 RS */
#define RES_PIN                   BOARD_PIN_LCD_RST
#define CS_PIN                    BOARD_PIN_LCD_CS
#define SCLK_PIN                  BOARD_PIN_LCD_CLK
#define SDIN_PIN                  BOARD_PIN_LCD_MOSI
#define TE_PIN                    BOARD_PIN_LCD_TE
