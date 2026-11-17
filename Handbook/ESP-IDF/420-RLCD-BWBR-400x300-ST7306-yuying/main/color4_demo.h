#ifndef COLOR4_DEMO_H
#define COLOR4_DEMO_H

#include "st7306_color4.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 跑自动轮播 demo。
 *
 * 前 15 页与黑白屏工程（420-RLCD-400x300-lekang）的 lcd_demo.c 逐页对齐：
 * 相同的页面顺序、相同的内容与布局、相同的驻留时间与帧率，
 * 唯一的差别是"灰阶抖动"用青色代替——4 色屏没有中间灰，只能用青色/红色
 * 表达层次，这样两块屏并排时能直接看出内容一致、色彩表现不同。
 *
 * 第 16 页起是 4 色屏专属的多色测试页。
 */
void color4_demo_run(st7306_handle_t lcd);

/** 只跑多色专属页（1..N），用于快速验证颜色是否正常。 */
void color4_demo_color_pages(st7306_handle_t lcd);

#ifdef __cplusplus
}
#endif

#endif /* COLOR4_DEMO_H */
