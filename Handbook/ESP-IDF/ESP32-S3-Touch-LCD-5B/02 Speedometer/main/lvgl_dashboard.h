#pragma once

/**
 * @file lvgl_dashboard.h
 * @brief Animated semicircular speedometer (LVGL v9).
 *
 * Builds a speedometer on the active screen: a round `lv_scale` with tick
 * marks + a red "danger zone" section, a line needle, and a big digital
 * readout. An `lv_anim_t` sweeps the needle 0 -> 120 -> 0 forever and updates
 * the readout in lock-step.
 *
 * Must be called while holding the esp_lvgl_port lock (see lvgl_port_lock),
 * since it touches LVGL objects. After it returns, the animation keeps
 * running on its own inside the LVGL task.
 */
void dashboard_create(void);
