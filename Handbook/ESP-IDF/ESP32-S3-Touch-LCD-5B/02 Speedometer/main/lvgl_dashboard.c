/**
 * Animated semicircular speedometer, built with LVGL v9.
 *
 * Layout (1024x600 screen):
 *   - a round `lv_scale` (LV_SCALE_MODE_ROUND_INNER), 270-degree arc with the
 *     gap at the bottom (rotation=135, angle_range=270) -> classic speedometer.
 *   - 25 minor ticks, a major tick + numeric label every 4 (=> 0,20,...,120).
 *   - a red "danger zone" section over 100..120.
 *   - an orange line needle + a small center hub.
 *   - a big digital readout ("087") + "km/h" unit in the bottom gap.
 *
 * One lv_anim_t sweeps the value 0 -> 120 -> 0 forever; its exec_cb moves the
 * needle (lv_scale_set_line_needle_value) and refreshes the readout label in
 * lock-step. The needle's line points are owned by the scale, so we never
 * touch them directly (see lv_scale.h note on lv_scale_set_line_needle_value).
 *
 * Smoothness: the anim uses an ease-in-out path. With repeat + playback the
 * needle reverses direction at 0 and 120; a linear path flips velocity
 * instantly there (a hard "bounce"), which reads as jank on a panel whose
 * physical refresh is only ~20 Hz (PCLK is capped at 16 MHz on this board).
 * Ease-in-out decelerates into each end and accelerates out, so the turnarounds
 * are seamless - the single biggest perceived-smoothness win at this fps.
 *
 * Call dashboard_create() while holding the esp_lvgl_port lock.
 */
#include "lvgl_dashboard.h"

#include "lvgl.h"

/* ---- geometry / range ---- */
#define GAUGE_SIZE        460          /* square bounding box for the round scale */
#define VAL_MIN           0
#define VAL_MAX           120          /* km/h */
#define TOTAL_TICKS       25           /* one tick every 5 km/h (0,5,...,120)    */
#define MAJOR_EVERY       4            /* major every 4 ticks => 0,20,...,120     */
#define NEEDLE_LEN        150          /* px from pivot (absolute, positive)      */
#define ANIM_PERIOD_MS    2500         /* 0->max (and max->0) sweep duration       */

/* ---- palette (hex 0xRRGGBB; LVGL converts to RGB565 internally) ---- */
#define COL_BG            lv_color_hex(0x081421)   /* dark navy screen bg        */
#define COL_FACE          lv_color_hex(0x10202E)   /* gauge face (slightly lighter) */
#define COL_TICK          lv_color_hex(0x8A98A8)   /* minor ticks / muted text   */
#define COL_TICK_MAJOR    lv_color_hex(0xDCE4EE)   /* major ticks + labels       */
#define COL_NEEDLE        lv_color_hex(0xFF7A1A)   /* vivid orange needle        */
#define COL_VALUE         lv_color_hex(0xFFFFFF)   /* big digital readout        */
#define COL_DANGER        lv_palette_main(LV_PALETTE_RED)

/* Context handed to the animation exec_cb (must outlive the anim -> file-static). */
typedef struct {
    lv_obj_t *scale;
    lv_obj_t *needle;
    lv_obj_t *value_label;
    int32_t  last_int;   /* last integer shown in the readout; -1 = force refresh */
} gauge_ctx_t;

static gauge_ctx_t s_ctx;
static char        s_value_buf[8];

/* Animation step: point the needle at `v` and refresh the digital readout.
 *
 * The needle must update on every tick (it needs every interpolated position),
 * but the digital readout only changes when the *integer* km/h changes. With
 * ease-in-out the needle lingers near 0 / 120, so consecutive ticks frequently
 * round to the same integer - skip the lv_label_set_text (which strdup's +
 * invalidates the label rect each call) when the value hasn't changed. */
static void gauge_set_value(void *var, int32_t v)
{
    gauge_ctx_t *g = (gauge_ctx_t *)var;
    lv_scale_set_line_needle_value(g->scale, g->needle, NEEDLE_LEN, v);

    int vi = (int)v;
    if (vi != g->last_int) {
        g->last_int = vi;
        lv_snprintf(s_value_buf, sizeof(s_value_buf), "%03d", vi);
        lv_label_set_text(g->value_label, s_value_buf);
    }
}

/* ---- FPS overlay (top-left) ----
 *
 * Counts completed refresh passes via LV_EVENT_REFR_READY - the same event
 * LVGL's own perf monitor tallies internally (see lv_sysmon.c) - and reports
 * the rate once a second in a corner label. This needs neither LV_USE_SYSMON
 * nor LV_USE_PERF_MONITOR: those only gate LVGL's *built-in* label + its
 * always-on timer, not this event. (Note: sdkconfig.defaults still carries a
 * stale CONFIG_LV_USE_PERF_MONITOR=y from LVGL v8 - in v9 that symbol is a
 * sub-option of SYSMON and was silently dropped, which is why no overlay ever
 * appeared.) Counting REFR_READY ourselves keeps the readout self-contained
 * and styled to match the dashboard, instead of LVGL's verbose two-line
 * "FPS / CPU% / ms" sysmon label.
 *
 * Without PERF_MONITOR the refresh timer also auto-pauses when nothing is
 * dirty, so the count naturally falls to ~0 when the UI is idle - exactly
 * what an FPS readout should show. Both callbacks run on esp_lvgl_port's LVGL
 * task (under its lock), so the shared counter needs no locking of its own. */
static uint32_t s_refr_cnt;       /* refreshes finished since the last report */
static uint32_t s_refr_last_tick; /* lv_tick_get() sampled at the last report */

static void fps_on_refr_ready(lv_event_t *e)
{
    (void)e;
    s_refr_cnt++;
}

static void fps_report(lv_timer_t *t)
{
    lv_obj_t *label = (lv_obj_t *)lv_timer_get_user_data(t);
    uint32_t elapsed = lv_tick_elaps(s_refr_last_tick);
    s_refr_last_tick = lv_tick_get();

    /* refreshes-per-second over the (roughly 1 s) window since last report */
    int fps = elapsed ? (int)((s_refr_cnt * 1000U) / elapsed) : 0;
    s_refr_cnt = 0;
    lv_label_set_text_fmt(label, "FPS: %d", fps);
}

void dashboard_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ---- title ---- */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SPEEDOMETER");
    lv_obj_set_style_text_color(title, COL_TICK_MAJOR, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    /* ---- the gauge ---- */
    lv_obj_t *scale = lv_scale_create(scr);
    s_ctx.scale = scale;
    lv_obj_set_size(scale, GAUGE_SIZE, GAUGE_SIZE);
    lv_obj_align(scale, LV_ALIGN_CENTER, 0, -10);

    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
    /* Gauge face: a circular, slightly-lighter panel, clipped to the circle. */
    lv_obj_set_style_bg_color(scale, COL_FACE, 0);
    lv_obj_set_style_bg_opa(scale, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(scale, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(scale, true, 0);
    lv_obj_set_style_border_width(scale, 0, 0);

    /* Tick marks + numeric labels. */
    lv_scale_set_label_show(scale, true);
    lv_scale_set_total_tick_count(scale, TOTAL_TICKS);
    lv_scale_set_major_tick_every(scale, MAJOR_EVERY);
    lv_scale_set_range(scale, VAL_MIN, VAL_MAX);
    /* 270-degree sweep, starting at 135deg -> 90deg gap at the bottom. */
    lv_scale_set_angle_range(scale, 270);
    lv_scale_set_rotation(scale, 135);

    /* Tick colours (defaults are dark / theme-driven -> invisible on dark bg). */
    lv_obj_set_style_length(scale, 6, LV_PART_ITEMS);           /* minor tick length */
    lv_obj_set_style_length(scale, 14, LV_PART_INDICATOR);      /* major tick length */
    lv_obj_set_style_line_color(scale, COL_TICK, LV_PART_ITEMS);
    lv_obj_set_style_line_color(scale, COL_TICK_MAJOR, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(scale, COL_TICK_MAJOR, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(scale, &lv_font_montserrat_14, LV_PART_INDICATOR);

    /* Red "danger zone" over the top end of the range. */
    static lv_style_t style_danger;
    lv_style_init(&style_danger);
    lv_style_set_line_color(&style_danger, COL_DANGER);
    lv_style_set_text_color(&style_danger, COL_DANGER);
    lv_scale_section_t *danger = lv_scale_add_section(scale);
    lv_scale_section_set_range(danger, 100, VAL_MAX);
    lv_scale_section_set_style(danger, LV_PART_INDICATOR, &style_danger);
    lv_scale_section_set_style(danger, LV_PART_ITEMS, &style_danger);

    /* ---- needle (a child of the scale; points are managed by the scale) ---- */
    lv_obj_t *needle = lv_line_create(scale);
    s_ctx.needle = needle;
    lv_obj_set_style_line_color(needle, COL_NEEDLE, 0);
    lv_obj_set_style_line_width(needle, 5, 0);
    lv_obj_set_style_line_rounded(needle, true, 0);

    /* Center hub, on top of the needle origin. */
    lv_obj_t *hub = lv_obj_create(scale);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 28, 28);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hub, COL_TICK_MAJOR, 0);
    lv_obj_set_style_border_color(hub, COL_NEEDLE, 0);
    lv_obj_set_style_border_width(hub, 3, 0);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);

    /* ---- digital readout in the bottom gap ---- */
    lv_obj_t *value = lv_label_create(scr);
    s_ctx.value_label = value;
    lv_label_set_text(value, "000");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(value, COL_VALUE, 0);
    lv_obj_align(value, LV_ALIGN_CENTER, 0, 55);

    lv_obj_t *unit = lv_label_create(scr);
    lv_label_set_text(unit, "km/h");
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(unit, COL_TICK, 0);
    lv_obj_align(unit, LV_ALIGN_CENTER, 0, 105);

    /* ---- one-time init so the needle isn't mid-arc before the first tick ---- */
    s_ctx.last_int = -1;                        /* force the readout to draw this frame */
    gauge_set_value(&s_ctx, VAL_MIN);

    /* ---- animation: 0 -> 120 -> 0, forever ---- */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &s_ctx);
    lv_anim_set_exec_cb(&a, gauge_set_value);
    lv_anim_set_values(&a, VAL_MIN, VAL_MAX);
    lv_anim_set_duration(&a, ANIM_PERIOD_MS);
    lv_anim_set_playback_duration(&a, ANIM_PERIOD_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);  /* smooth turnarounds at 0 / 120 */
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    /* ---- FPS readout in the top-left corner ---- */
    lv_obj_t *fps = lv_label_create(scr);
    lv_label_set_text(fps, "FPS: --");                 /* placeholder until the 1 s tick fires */
    lv_obj_set_style_text_font(fps, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(fps, COL_TICK_MAJOR, 0);
    /* faint dark badge: keeps it legible over any later content, and reads as
     * a debug HUD rather than part of the gauge. */
    lv_obj_set_style_bg_opa(fps, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(fps, lv_color_black(), 0);
    lv_obj_set_style_radius(fps, 4, 0);
    lv_obj_set_style_pad_hor(fps, 6, 0);
    lv_obj_set_style_pad_ver(fps, 2, 0);
    lv_obj_align(fps, LV_ALIGN_TOP_LEFT, 8, 8);

    /* Tally refreshes and refresh the label once a second. */
    s_refr_cnt = 0;
    s_refr_last_tick = lv_tick_get();
    lv_display_add_event_cb(lv_display_get_default(), fps_on_refr_ready, LV_EVENT_REFR_READY, NULL);
    lv_timer_create(fps_report, 1000, fps);
}
