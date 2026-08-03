#pragma once

/**
 * @file lvgl_dashboard.h
 * @brief Live telemetry dashboard (LVGL v9).
 *
 * Builds a five-metric instrument panel on the active screen: ENGINE (RPM) and
 * SPEED (km/h) as two hero cards, with THROTTLE (%), COOLANT (deg C) and
 * BATTERY (V) in a secondary row. Each card shows a big value, a linear bar,
 * and min/max range labels. A header carries the title + a pulsing LIVE badge;
 * the FPS readout stays in the top-left corner.
 *
 * Values are simulated: each metric random-walks a new target every ~1.2 s and
 * eases toward it on a 40 ms tick, so the motion reads smoothly on a panel
 * whose physical refresh is only ~20 Hz. Over-limit values (RPM/coolant/battery)
 * flip red.
 *
 * Must be called while holding the esp_lvgl_port lock (see lvgl_port_lock),
 * since it touches LVGL objects. After it returns, the update timer keeps
 * running on its own inside the LVGL task.
 */
void dashboard_create(void);
