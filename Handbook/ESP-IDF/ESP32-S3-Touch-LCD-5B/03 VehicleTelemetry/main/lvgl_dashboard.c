/**
 * Telemetry dashboard, built with LVGL v9 (replaces the old speedometer).
 *
 * Layout (1024x600):
 *   Header  : "VEHICLE TELEMETRY" title (top-centre), pulsing green LIVE badge
 *             (top-right), self-counted FPS readout (top-left, kept from before).
 *   Hero row: ENGINE (RPM) and SPEED (km/h) - big cards with a large value +
 *             a full-width linear bar + min/max tick labels.
 *   Sec row : THROTTLE (%), COOLANT (deg C), BATTERY (V) - smaller cards, same
 *             anatomy. Voltage is shown with one decimal (stored x10).
 *
 * Each metric has a current value and a moving target. Every ~1.2 s the target
 * does a random walk (clamped to range); on every 40 ms tick the displayed value
 * eases one step toward the target (exponential smoothing). That easing - not a
 * linear ramp - is what reads as smooth on a panel whose physical refresh is
 * only ~20 Hz (PCLK is capped at 16 MHz on this board): a constant-velocity
 * chase would look mechanical, the eased approach decelerates into the target
 * and feels organic. The linear *bar* is the "linear" visual the eye locks onto.
 *
 * Readout text is only rewritten when the formatted string actually changes
 * (lv_label_set_text strdup's + invalidates the rect each call), and the bar /
 * danger colour are touched every tick - cheap, and they're the things that
 * convey "live".
 *
 * Danger state (RPM >= 6800, coolant >= 105 C, battery <= 10.8 V or >= 14.6 V)
 * flips the value text and the bar indicator to red.
 *
 * Call dashboard_create() while holding the esp_lvgl_port lock.
 */
#include "lvgl_dashboard.h"

#include <string.h>

#include "lvgl.h"
#include "esp_random.h"   /* esp_random() - its own header since IDF 5.x */

/* ---- palette (hex 0xRRGGBB; LVGL converts to RGB565 internally) ---- */
#define COL_BG        lv_color_hex(0x0B1220)   /* near-black navy screen bg      */
#define COL_CARD      lv_color_hex(0x141F30)   /* card face                       */
#define COL_BORDER    lv_color_hex(0x27374E)   /* hairline card border            */
#define COL_DIV       lv_color_hex(0x1C2740)   /* header divider line             */
#define COL_BAR_BG    lv_color_hex(0x1B2738)   /* bar trough                      */
#define COL_LABEL     lv_color_hex(0x8195AF)   /* muted label / title text        */
#define COL_MINOR     lv_color_hex(0x5C6E87)   /* min/max range numbers           */
#define COL_VALUE     lv_color_hex(0xF3F6FB)   /* big primary readout (near white)*/
#define COL_DANGER    lv_color_hex(0xFF3B47)   /* over-limit value + indicator    */
#define COL_OK        lv_color_hex(0x33E36B)   /* LIVE status                     */

/* ---- per-metric static config ----
 * Values are stored as scaled ints (scale 1 for integers, 10 for the one
 * decimal voltage wants) so all the easing math stays integer. */
typedef struct {
    const char *label;
    const char *unit;
    int16_t  x, y, w, h;        /* card geometry (absolute, on the 1024x600 screen) */
    int16_t  pad;               /* card inner padding                              */
    int16_t  value_y;           /* top offset of the big value inside the card      */
    int32_t  min, max;          /* range, in scaled units                          */
    int32_t  danger_hi;         /* scaled; 0 = no upper limit                       */
    int32_t  danger_lo;         /* scaled; 0 = no lower limit                       */
    int32_t  current;           /* initial displayed value (scaled)                 */
    uint32_t accent_hex;        /* bar / unit accent colour                         */
    uint8_t  scale;             /* 1 = integer, 10 = one decimal place              */
    uint8_t  big;               /* 1 = hero card (taller bar / bigger pad)          */
} metric_cfg_t;

/* Hero row at y=84 h=242 (two 478-wide cards). Secondary row at y=346 h=230
 * (three 312-wide cards). 24 px outer margin, 20 px gutters. */
static const metric_cfg_t CFG[] = {
    /* label      unit    x    y   w    h  pad v_y  min  max  dHi  dLo  init accent     sc big */
    { "ENGINE",  "RPM",  24,  84, 478, 242, 28, 78,    0, 8000, 6800, 0,   850, 0xFF5A3C, 1, 1 },
    { "SPEED",   "km/h", 522, 84, 478, 242, 28, 78,    0, 240,  0,    0,   0,   0x4CC2FF, 1, 1 },
    { "THROTTLE","%",    24, 346, 312, 230, 24, 64,    0, 100,  0,    0,   0,   0xFFB020, 1, 0 },
    { "COOLANT", "\xC2\xB0""C", 356, 346, 312, 230, 24, 64, 20, 130,  105,  0,   70,  0x3CD9B0, 1, 0 },
    { "BATTERY", "V",    688,346, 312, 230, 24, 64,  100, 150, 146, 108, 124, 0xB08CFF,10, 0 },
};
#define N_METRICS   (int)(sizeof(CFG) / sizeof(CFG[0]))

/* ---- per-metric runtime state ---- */
typedef struct {
    lv_obj_t *value;          /* big readout label                          */
    lv_obj_t *bar;            /* linear bar                                 */
    lv_color_t accent;        /* resolved accent colour (unit + indicator)  */
    int32_t  current;         /* eased value shown now (scaled)             */
    int32_t  target;          /* value being approached (scaled)            */
    int32_t  min, max;        /* range (scaled)                             */
    int32_t  danger_hi, danger_lo;
    int32_t  scale;
    char     last_text[12];   /* last formatted string written to `value`    */
} metric_t;
static metric_t s_m[N_METRICS];

/* ---- small helpers ---- */
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int rnd_range(int lo, int hi)
{
    return lo + (int)(esp_random() % (uint32_t)(hi - lo + 1));
}

/* Format a scaled value into buf (e.g. 143/scale10 -> "14.3"). */
static void fmt_scaled(int32_t v, int32_t scale, char *buf, size_t n)
{
    if (scale == 10) {
        lv_snprintf(buf, n, "%d.%d", (int)(v / 10), (int)(v % 10));
    } else {
        lv_snprintf(buf, n, "%d", (int)v);
    }
}

static bool in_danger(const metric_t *m)
{
    if (m->danger_hi && m->current >= m->danger_hi) return true;
    if (m->danger_lo && m->current <= m->danger_lo) return true;
    return false;
}

/* ---- build one card; fills s_m[i] ---- */
static void make_card(lv_obj_t *parent, int i)
{
    const metric_cfg_t *c = &CFG[i];
    metric_t *m = &s_m[i];

    m->accent      = lv_color_hex(c->accent_hex);
    m->current     = c->current;
    m->target      = c->current;
    m->min         = c->min;
    m->max         = c->max;
    m->scale       = c->scale;
    m->danger_hi   = c->danger_hi;
    m->danger_lo   = c->danger_lo;
    m->last_text[0] = '\0';

    /* ---- card body ---- */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, c->x, c->y);
    lv_obj_set_size(card, c->w, c->h);
    lv_obj_set_style_bg_color(card, COL_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_border_color(card, COL_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, c->pad, 0);

    /* ---- label (top-left) + unit (top-right, accent) ---- */
    lv_obj_t *lab = lv_label_create(card);
    lv_label_set_text(lab, c->label);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lab, COL_LABEL, 0);
    lv_obj_set_style_text_letter_space(lab, 1, 0);
    lv_obj_align(lab, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *unit = lv_label_create(card);
    lv_label_set_text(unit, c->unit);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(unit, m->accent, 0);
    lv_obj_align(unit, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* ---- big value ---- */
    lv_obj_t *val = lv_label_create(card);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(val, COL_VALUE, 0);
    lv_obj_align(val, LV_ALIGN_TOP_LEFT, 0, c->value_y);
    m->value = val;

    /* ---- linear bar ---- */
    int bar_h     = c->big ? 14 : 10;
    int content_w = c->w - 2 * c->pad;
    lv_obj_t *bar = lv_bar_create(card);
    lv_obj_remove_style_all(bar);                  /* start clean, then style both parts */
    lv_bar_set_range(bar, c->min, c->max);
    lv_bar_set_value(bar, c->current, LV_ANIM_OFF);
    lv_obj_set_size(bar, content_w, bar_h);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, -(16 + 8));  /* sit above the min/max row */
    /* trough */
    lv_obj_set_style_bg_color(bar, COL_BAR_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, bar_h / 2, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    /* indicator */
    lv_obj_set_style_bg_color(bar, m->accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, bar_h / 2, LV_PART_INDICATOR);
    m->bar = bar;

    /* ---- min / max range labels under the bar ---- */
    char buf[12];
    lv_obj_t *lmin = lv_label_create(card);
    fmt_scaled(c->min, c->scale, buf, sizeof(buf));
    lv_label_set_text(lmin, buf);
    lv_obj_set_style_text_font(lmin, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lmin, COL_MINOR, 0);
    lv_obj_align(lmin, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *lmax = lv_label_create(card);
    fmt_scaled(c->max, c->scale, buf, sizeof(buf));
    lv_label_set_text(lmax, buf);
    lv_obj_set_style_text_font(lmax, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lmax, COL_MINOR, 0);
    lv_obj_align(lmax, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

/* ---- simulation + redraw tick (40 ms) ----
 *
 * Every 30th tick (~1.2 s) each metric picks a new target by random-walking a
 * quarter of its range from the current value (clamped) - so the signal wanders
 * like real telemetry instead of teleporting. Each tick then eases `current`
 * one exponential step toward `target`; the bar + (changed) text follow. */
#define EASE_DEN          6     /* current += diff / EASE_DEN per tick            */
#define RETARGET_TICKS    30    /* 30 * 40 ms = 1.2 s between target changes      */

static void update_cb(lv_timer_t *t)
{
    (void)t;
    static int tick = 0;
    tick++;

    if (tick % RETARGET_TICKS == 0) {
        for (int i = 0; i < N_METRICS; i++) {
            metric_t *m = &s_m[i];
            int span = (int)((m->max - m->min) / 3);
            if (span < 1) span = 1;
            m->target = clampi(m->current + rnd_range(-span, span), m->min, m->max);
        }
    }

    for (int i = 0; i < N_METRICS; i++) {
        metric_t *m = &s_m[i];

        /* ease toward target; snap when close so it doesn't stall a hair away */
        int diff = (int)(m->target - m->current);
        int snap = (m->scale == 10) ? 2 : EASE_DEN;
        if (diff > -snap && diff < snap) {
            m->current = m->target;
        } else {
            m->current += diff / EASE_DEN;
        }

        bool danger = in_danger(m);

        /* bar (every tick - it's the live visual) */
        lv_bar_set_value(m->bar, m->current, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(m->bar, danger ? COL_DANGER : m->accent, LV_PART_INDICATOR);

        /* value text - only when the formatted string actually changes */
        char buf[12];
        fmt_scaled(m->current, m->scale, buf, sizeof(buf));
        if (strcmp(buf, m->last_text) != 0) {
            strcpy(m->last_text, buf);
            lv_label_set_text(m->value, buf);
        }
        lv_obj_set_style_text_color(m->value, danger ? COL_DANGER : COL_VALUE, 0);
    }
}

/* ---- LIVE badge: a small green dot that gently pulses opacity ---- */
static void dot_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
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

    /* ---- header: title (centre) + LIVE badge (right) + divider ---- */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "VEHICLE TELEMETRY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, COL_VALUE, 0);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *live = lv_obj_create(scr);
    lv_obj_remove_style_all(live);
    lv_obj_clear_flag(live, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(live, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(live, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(live, 6, 0);
    lv_obj_set_flex_align(live, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(live, LV_ALIGN_TOP_RIGHT, -24, 20);

    lv_obj_t *dot = lv_obj_create(live);
    lv_obj_remove_style_all(dot);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COL_OK, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

    lv_obj_t *live_txt = lv_label_create(live);
    lv_label_set_text(live_txt, "LIVE");
    lv_obj_set_style_text_font(live_txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(live_txt, COL_OK, 0);
    lv_obj_set_style_text_letter_space(live_txt, 1, 0);

    lv_obj_t *div = lv_obj_create(scr);
    lv_obj_remove_style_all(div);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(div, 1024 - 48, 2);
    lv_obj_set_style_bg_color(div, COL_DIV, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 60);

    /* gentle opacity pulse on the LIVE dot */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, dot);
    lv_anim_set_exec_cb(&a, dot_opa_cb);
    lv_anim_set_values(&a, LV_OPA_40 + 40, LV_OPA_COVER);   /* ~0.55 .. 1.0 */
    lv_anim_set_duration(&a, 1100);
    lv_anim_set_playback_duration(&a, 1100);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    /* ---- the five metric cards ---- */
    for (int i = 0; i < N_METRICS; i++) {
        make_card(scr, i);
    }

    /* first paint of every value so the cards don't show "--" for a tick */
    for (int i = 0; i < N_METRICS; i++) {
        char buf[12];
        fmt_scaled(s_m[i].current, s_m[i].scale, buf, sizeof(buf));
        strcpy(s_m[i].last_text, buf);
        lv_label_set_text(s_m[i].value, buf);
    }

    /* ---- drive the simulation ---- */
    lv_timer_create(update_cb, 40, NULL);

    /* ---- FPS readout in the top-left corner ---- */
    lv_obj_t *fps = lv_label_create(scr);
    lv_label_set_text(fps, "FPS: --");                 /* placeholder until the 1 s tick fires */
    lv_obj_set_style_text_font(fps, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(fps, COL_VALUE, 0);
    /* faint dark badge: keeps it legible over any later content, and reads as
     * a debug HUD rather than part of the dash. */
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
