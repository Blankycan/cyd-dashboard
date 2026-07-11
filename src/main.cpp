// CYD Dashboard — Phase 3: WPM + active/idle state
// Parses {"type":"stats","cpu":XX,"ram":XX,"time":"HH:MM","wpm":XX,"active":bool}.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include "theme.h"

// ---------------------------------------------------------------------------
// Layout constants (all sizes in pixels)
// ---------------------------------------------------------------------------
#define SCREEN_W    320
#define SCREEN_H    240
#define BL_PIN      21

#define TOPBAR_H    30      // top bar height
#define SIDEBAR_W   96      // Hotaru column width
#define DIV_W       1       // divider line width

// Content area (below top bar)
#define CONTENT_Y   (TOPBAR_H + DIV_W)
#define CONTENT_H   (SCREEN_H - CONTENT_Y)

// Right panel x start and width
#define RSTART      (SIDEBAR_W + DIV_W)
#define RWIDTH      (SCREEN_W - RSTART)

// Right panel section heights (must sum to CONTENT_H)
#define MUSIC_H     52
#define CLAUDE_H    90
#define STATUS_H    (CONTENT_H - MUSIC_H - DIV_W - CLAUDE_H - DIV_W)

// Top bar CPU/RAM column x positions
#define CPU_X       132
#define RAM_X       226

// Music panel — right-side animation area
#define MA_W          38                   // animation area width
#define MA_X          (RWIDTH - 4 - MA_W) // animation area left x in panel
#define MUSIC_LABEL_W (MA_X - 26)         // title/artist label width (x=22..MA_X-4)

// Disconnection timeout
#define DISCONNECT_TIMEOUT_MS 5000

// Claude panel — rotate between token and rate-limit views (ms per view)
// Set to 0 to disable rotation (shows token view only)
#define CLAUDE_ROTATE_MS 8000

// Sleep / backlight
#define SLEEP_TIMEOUT_MS  (5 * 60 * 1000)  // keyboard idle time before sleep
#define BL_PWM_CHANNEL    0
#define BL_PWM_FREQ       5000
#define BL_PWM_BITS       8
#define BL_FULL           255
#define BL_DIM            12

// ---------------------------------------------------------------------------
// Dashboard state — updated from serial packets
// ---------------------------------------------------------------------------
struct DashState {
    char time_str[6]     = "--:--";
    int  cpu             = 0;
    int  ram             = 0;
    int  wpm             = 0;
    bool active          = false;
    bool connected       = false;
    char music_title[48] = "";
    char music_artist[32]= "";
    bool music_playing   = false;
    bool music_active    = false;
    char idle_msg[48]    = "";
    // Token view (always available from JSONL)
    int32_t claude_out      = 0;
    int32_t claude_inp      = 0;
    int     claude_sessions = 0;
    // Rate-limit view (-1 = unavailable, requires API key)
    int  claude_h5_pct  = -1;
    int  claude_h5_secs = -1;
    int  claude_w7_pct  = -1;
    int  claude_w7_secs = -1;
} state;

// ---------------------------------------------------------------------------
// LVGL globals
// ---------------------------------------------------------------------------
static TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t lv_buf[SCREEN_W * 10];

// Handles updated at runtime
static lv_obj_t *lbl_time    = nullptr;
static lv_obj_t *lbl_cpu_val = nullptr;
static lv_obj_t *bar_cpu     = nullptr;
static lv_obj_t *lbl_ram_val = nullptr;
static lv_obj_t *bar_ram     = nullptr;

// Panel containers — populated by later phases
lv_obj_t *panel_music  = nullptr;
lv_obj_t *panel_claude = nullptr;
lv_obj_t *panel_status = nullptr;

// WPM panel handles
static lv_obj_t *dot_status  = nullptr;
static lv_obj_t *lbl_status  = nullptr;
static lv_obj_t *lbl_wpm_val = nullptr;

// Music panel handles
static lv_obj_t *dot_music        = nullptr;
static lv_obj_t *lbl_music_title  = nullptr;
static lv_obj_t *lbl_music_artist = nullptr;
static lv_obj_t *music_anim_ctr   = nullptr;  // animation area container

// Claude panel handles
static lv_obj_t  *dot_claude       = nullptr;
static lv_obj_t  *claude_view_tok  = nullptr;   // token view container
static lv_obj_t  *claude_view_rl   = nullptr;   // rate-limit view container
static lv_timer_t *claude_rot_timer  = nullptr;
static lv_timer_t *claude_prog_timer = nullptr;
static bool        claude_show_rl    = false;
static uint32_t    claude_switched_ms = 0;    // millis() at last view switch
// Progress indicator (top-right of header, hidden until both views active)
static lv_obj_t   *bar_claude_prog  = nullptr;
// Token view widgets
static lv_obj_t *lbl_claude_out  = nullptr;
static lv_obj_t *bar_claude_tok  = nullptr;
static lv_obj_t *lbl_claude_in   = nullptr;
static lv_obj_t *lbl_claude_sess = nullptr;
// Rate-limit view widgets
static lv_obj_t *lbl_h5_pct   = nullptr;
static lv_obj_t *lbl_h5_reset = nullptr;
static lv_obj_t *bar_h5       = nullptr;
static lv_obj_t *lbl_w7_pct   = nullptr;
static lv_obj_t *lbl_w7_reset = nullptr;
static lv_obj_t *bar_w7       = nullptr;

enum MusicAnim { MA_NONE, MA_NOTES, MA_PAUSE, MA_GRASS };
static MusicAnim ma_state = MA_NONE;

// Disconnect tracking
static uint32_t  last_packet_ms = 0;

// Sleep state
enum SleepState { SS_AWAKE, SS_ASLEEP };
static SleepState sleep_state    = SS_AWAKE;
static uint32_t   last_active_ms = 0;
static lv_obj_t  *sleep_overlay  = nullptr;
static lv_obj_t  *lbl_sleep_time = nullptr;
static lv_obj_t  *lbl_z[3]      = {};
static lv_timer_t *zzz_timer     = nullptr;
static void exit_sleep();   // forward decl (defined before setup)

// Hotaru animation state
enum HotaruState { HS_IDLE, HS_TYPING_SLOW, HS_TYPING_FAST };
static HotaruState   h_state       = HS_IDLE;
static lv_obj_t     *h_container   = nullptr;
static lv_obj_t     *h_cap         = nullptr;
static lv_obj_t     *h_eye_l       = nullptr;
static lv_obj_t     *h_eye_r       = nullptr;
static int32_t       h_base_y      = 0;
#define N_SPARKS 4
static lv_obj_t     *h_sparks[N_SPARKS];
static uint8_t       h_spark_idx   = 0;
static lv_timer_t   *h_spark_timer = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static lv_color_t pct_color(int pct) {
    if (pct >= 90) return COL_ALERT;
    if (pct >= 70) return COL_WARN;
    return COL_OK;
}

// Create a styled progress bar, returns the object
static lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_pos(bar, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_BAR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, COL_OK, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    return bar;
}

// Create a 1px horizontal divider line
static void make_hdiv(lv_obj_t *parent, int y, int x, int w) {
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, w, 1);
    lv_obj_set_pos(div, x, y);
    lv_obj_set_style_bg_color(div, COL_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
}

// Create a clean panel container with no decoration
static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, COL_BG, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

// Create a dim placeholder label centered in a panel
static void make_placeholder(lv_obj_t *parent, const char *text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

// ---------------------------------------------------------------------------
// LVGL flush callback
// ---------------------------------------------------------------------------
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t*>(&px->full), w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

// Touch — XPT2046 is on its own SPI bus (HSPI), separate from the display
// CYD (ESP32-2432S028R) wiring: SCK=25, MISO=39, MOSI=32, CS=33, IRQ=36
static SPIClass        touch_spi(HSPI);
static XPT2046_Touchscreen ts(33, 36);

static void touch_read_cb(lv_indev_drv_t *, lv_indev_data_t *data) {
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint();
        // Raw range ~200–3900; map to screen coords for rotation 1
        data->point.x = map(p.x, 200, 3900, 0, SCREEN_W - 1);
        data->point.y = map(p.y, 200, 3900, 0, SCREEN_H - 1);
        data->state   = LV_INDEV_STATE_PRESSED;
        last_active_ms = millis();
        if (sleep_state == SS_ASLEEP) exit_sleep();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void hotaru_update(int wpm, bool active);  // forward decl

static void fmt_k(char *buf, size_t sz, int32_t n) {
    if      (n >= 1000000) snprintf(buf, sz, "%.1fm", n / 1000000.0f);
    else if (n >= 100000)  snprintf(buf, sz, "%.0fk", n / 1000.0f);
    else if (n >= 1000)    snprintf(buf, sz, "%.1fk", n / 1000.0f);
    else                   snprintf(buf, sz, "%d",    (int)n);
}

static void fmt_reset(char *buf, size_t sz, int secs) {
    if (secs < 0)  { snprintf(buf, sz, "--");    return; }
    if (secs < 60) { snprintf(buf, sz, "< 1m");  return; }
    int m = secs / 60, h = m / 60, d = h / 24;
    if (d > 0)      snprintf(buf, sz, "%dd %dh", d, h % 24);
    else if (h > 0) snprintf(buf, sz, "%dh %dm", h, m % 60);
    else            snprintf(buf, sz, "%dm", m);
}

static lv_color_t pct_col(int pct) {
    if (pct < 0)   return COL_TEXT_DIM;
    if (pct >= 90) return COL_ALERT;
    if (pct >= 70) return COL_WARN;
    return COL_OK;
}

// ---------------------------------------------------------------------------
// UI updates — called whenever state changes
// ---------------------------------------------------------------------------
static void update_wpm_ui() {
    if (state.active) {
        lv_obj_set_style_bg_color(dot_status, COL_OK, 0);
        lv_label_set_text(lbl_status, "ACTIVE");
        lv_obj_set_style_text_color(lbl_status, COL_OK, 0);
    } else {
        lv_obj_set_style_bg_color(dot_status, COL_TEXT_DIM, 0);
        lv_label_set_text(lbl_status, "IDLE");
        lv_obj_set_style_text_color(lbl_status, COL_TEXT_DIM, 0);
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", state.wpm);
    lv_label_set_text(lbl_wpm_val, buf);
    hotaru_update(state.wpm, state.active);
}

static void update_stats_ui() {
    lv_label_set_text(lbl_time, state.time_str);

    char buf[8];

    snprintf(buf, sizeof(buf), "%d%%", state.cpu);
    lv_label_set_text(lbl_cpu_val, buf);
    lv_color_t ccpu = pct_color(state.cpu);
    lv_obj_set_style_text_color(lbl_cpu_val, ccpu, 0);
    lv_bar_set_value(bar_cpu, state.cpu, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_cpu, ccpu, LV_PART_INDICATOR);

    snprintf(buf, sizeof(buf), "%d%%", state.ram);
    lv_label_set_text(lbl_ram_val, buf);
    lv_color_t cram = pct_color(state.ram);
    lv_obj_set_style_text_color(lbl_ram_val, cram, 0);
    lv_bar_set_value(bar_ram, state.ram, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_ram, cram, LV_PART_INDICATOR);
}

// ---------------------------------------------------------------------------
// Build UI
// ---------------------------------------------------------------------------

static void build_topbar(lv_obj_t *scr) {
    lv_obj_t *tb = make_panel(scr, 0, 0, SCREEN_W, TOPBAR_H);
    lv_obj_set_style_bg_color(tb, COL_PANEL, 0);  // slightly raised panel

    // Thin bottom border
    make_hdiv(scr, TOPBAR_H, 0, SCREEN_W);

    // Time — left, vertically centred
    lbl_time = lv_label_create(tb);
    lv_label_set_text(lbl_time, "--:--");
    lv_obj_set_style_text_color(lbl_time, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 8, 0);

    // CPU — dim key label, thin bar, colored value
    lv_obj_t *k_cpu = lv_label_create(tb);
    lv_label_set_text(k_cpu, "CPU");
    lv_obj_set_style_text_color(k_cpu, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(k_cpu, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(k_cpu, CPU_X, 5);

    bar_cpu = make_bar(tb, CPU_X, 19, 36, 4);

    lbl_cpu_val = lv_label_create(tb);
    lv_label_set_text(lbl_cpu_val, "0%");
    lv_obj_set_style_text_color(lbl_cpu_val, COL_OK, 0);
    lv_obj_set_style_text_font(lbl_cpu_val, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_cpu_val, CPU_X + 40, 6);

    // RAM — same layout
    lv_obj_t *k_ram = lv_label_create(tb);
    lv_label_set_text(k_ram, "RAM");
    lv_obj_set_style_text_color(k_ram, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(k_ram, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(k_ram, RAM_X, 5);

    bar_ram = make_bar(tb, RAM_X, 19, 36, 4);

    lbl_ram_val = lv_label_create(tb);
    lv_label_set_text(lbl_ram_val, "0%");
    lv_obj_set_style_text_color(lbl_ram_val, COL_OK, 0);
    lv_obj_set_style_text_font(lbl_ram_val, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_ram_val, RAM_X + 40, 6);
}

// ---------------------------------------------------------------------------
// Hotaru animation system — Phase 4
// Bob, blink, glow state, sparkles driven by WPM state machine.
// ---------------------------------------------------------------------------

static void bob_anim_cb(void *obj, int32_t val) {
    lv_obj_set_y((lv_obj_t*)obj, val);
}

static void spark_fade_cb(void *obj, int32_t val) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)val, 0);
}

static void blink_restore_cb(lv_timer_t *t) {
    lv_timer_del(t);
    lv_obj_set_height(h_eye_l, 5);
    lv_obj_set_height(h_eye_r, 5);
    lv_obj_set_style_radius(h_eye_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(h_eye_r, LV_RADIUS_CIRCLE, 0);
}

static void blink_timer_cb(lv_timer_t *) {
    lv_obj_set_height(h_eye_l, 2);
    lv_obj_set_height(h_eye_r, 2);
    lv_obj_set_style_radius(h_eye_l, 1, 0);
    lv_obj_set_style_radius(h_eye_r, 1, 0);
    lv_timer_create(blink_restore_cb, 150, nullptr);
}

static void spark_tick_cb(lv_timer_t *) {
    if (h_state == HS_IDLE) return;
    lv_obj_t *sp = h_sparks[h_spark_idx % N_SPARKS];
    h_spark_idx++;
    lv_obj_set_style_opa(sp, LV_OPA_COVER, 0);
    lv_anim_del(sp, spark_fade_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sp);
    lv_anim_set_exec_cb(&a, spark_fade_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 500);
    lv_anim_start(&a);
}

static void hotaru_start_bob(uint32_t half_ms, int32_t range) {
    lv_anim_del(h_container, bob_anim_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, h_container);
    lv_anim_set_exec_cb(&a, bob_anim_cb);
    lv_anim_set_values(&a, h_base_y - range, h_base_y + range);
    lv_anim_set_time(&a, half_ms);
    lv_anim_set_playback_time(&a, half_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void hotaru_apply_state() {
    uint32_t   half_ms;
    int32_t    range;
    lv_opa_t   glow_opa;
    lv_coord_t glow_w;
    uint32_t   spark_ms;

    switch (h_state) {
        case HS_TYPING_FAST:
            half_ms=200; range=4; glow_opa=LV_OPA_70; glow_w=22; spark_ms=350; break;
        case HS_TYPING_SLOW:
            half_ms=500; range=3; glow_opa=LV_OPA_50; glow_w=16; spark_ms=650; break;
        default:
            half_ms=1000; range=3; glow_opa=LV_OPA_40; glow_w=14; spark_ms=0; break;
    }

    hotaru_start_bob(half_ms, range);
    lv_obj_set_style_shadow_opa(h_cap, glow_opa, 0);
    lv_obj_set_style_shadow_width(h_cap, glow_w, 0);

    if (h_spark_timer) { lv_timer_del(h_spark_timer); h_spark_timer = nullptr; }
    if (spark_ms > 0) {
        h_spark_timer = lv_timer_create(spark_tick_cb, spark_ms, nullptr);
    } else {
        for (int i = 0; i < N_SPARKS; i++)
            lv_obj_set_style_opa(h_sparks[i], LV_OPA_TRANSP, 0);
    }
}

static void hotaru_update(int wpm, bool active) {
    HotaruState ns;
    if (!active || wpm == 0) ns = HS_IDLE;
    else if (wpm >= 30)      ns = HS_TYPING_FAST;
    else                     ns = HS_TYPING_SLOW;
    if (ns == h_state) return;
    h_state = ns;
    hotaru_apply_state();
}

static void hotaru_build(lv_obj_t *sidebar) {
    // Spark positions (relative to container, flanking the cap)
    static const lv_coord_t sx[N_SPARKS] = { 6, 68,  2, 72};
    static const lv_coord_t sy[N_SPARKS] = { 8,  6, 28, 26};

    h_base_y = (CONTENT_H - 100) / 2;

    h_container = lv_obj_create(sidebar);
    lv_obj_remove_style_all(h_container);
    lv_obj_set_size(h_container, 88, 100);
    lv_obj_set_pos(h_container, (SIDEBAR_W - 88) / 2, h_base_y);
    lv_obj_set_style_bg_opa(h_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(h_container, 0, 0);
    lv_obj_clear_flag(h_container, LV_OBJ_FLAG_SCROLLABLE);

    // Soft glow halo
    lv_obj_t *glow_bg = lv_obj_create(h_container);
    lv_obj_remove_style_all(glow_bg);
    lv_obj_set_size(glow_bg, 80, 80);
    lv_obj_align(glow_bg, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(glow_bg, COL_GLOW, 0);
    lv_obj_set_style_bg_opa(glow_bg, LV_OPA_10, 0);
    lv_obj_set_style_radius(glow_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(glow_bg, 0, 0);
    lv_obj_clear_flag(glow_bg, LV_OBJ_FLAG_SCROLLABLE);

    // Mushroom cap — shadow intensity driven by state machine
    h_cap = lv_obj_create(h_container);
    lv_obj_remove_style_all(h_cap);
    lv_obj_set_size(h_cap, 60, 34);
    lv_obj_align(h_cap, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_bg_color(h_cap, COL_CAP, 0);
    lv_obj_set_style_bg_opa(h_cap, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(h_cap, 17, 0);
    lv_obj_set_style_border_width(h_cap, 0, 0);
    lv_obj_set_style_shadow_color(h_cap, COL_GLOW, 0);
    lv_obj_set_style_shadow_opa(h_cap, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(h_cap, 14, 0);
    lv_obj_set_style_shadow_spread(h_cap, 2, 0);
    lv_obj_clear_flag(h_cap, LV_OBJ_FLAG_SCROLLABLE);

    // Cap highlight
    lv_obj_t *shine = lv_obj_create(h_container);
    lv_obj_remove_style_all(shine);
    lv_obj_set_size(shine, 16, 9);
    lv_obj_align(shine, LV_ALIGN_TOP_MID, -8, 16);
    lv_obj_set_style_bg_color(shine, COL_CAP_SHINE, 0);
    lv_obj_set_style_bg_opa(shine, LV_OPA_70, 0);
    lv_obj_set_style_radius(shine, 4, 0);
    lv_obj_set_style_border_width(shine, 0, 0);
    lv_obj_clear_flag(shine, LV_OBJ_FLAG_SCROLLABLE);

    // Cap spot
    lv_obj_t *spot = lv_obj_create(h_container);
    lv_obj_remove_style_all(spot);
    lv_obj_set_size(spot, 7, 7);
    lv_obj_align(spot, LV_ALIGN_TOP_MID, 16, 18);
    lv_obj_set_style_bg_color(spot, COL_SPOTS, 0);
    lv_obj_set_style_bg_opa(spot, LV_OPA_80, 0);
    lv_obj_set_style_radius(spot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(spot, 0, 0);
    lv_obj_clear_flag(spot, LV_OBJ_FLAG_SCROLLABLE);

    // Body / face
    lv_obj_t *body = lv_obj_create(h_container);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, 30, 38);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_set_style_bg_color(body, COL_FACE, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(body, 10, 0);
    lv_obj_set_style_border_color(body, COL_STEM, 0);
    lv_obj_set_style_border_width(body, 1, 0);
    lv_obj_set_style_border_opa(body, LV_OPA_40, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // Eyes — blink by collapsing height to 2px
    h_eye_l = lv_obj_create(h_container);
    lv_obj_remove_style_all(h_eye_l);
    lv_obj_set_size(h_eye_l, 5, 5);
    lv_obj_align(h_eye_l, LV_ALIGN_TOP_MID, -7, 49);
    lv_obj_set_style_bg_color(h_eye_l, COL_EYES, 0);
    lv_obj_set_style_bg_opa(h_eye_l, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(h_eye_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(h_eye_l, 0, 0);
    lv_obj_clear_flag(h_eye_l, LV_OBJ_FLAG_SCROLLABLE);

    h_eye_r = lv_obj_create(h_container);
    lv_obj_remove_style_all(h_eye_r);
    lv_obj_set_size(h_eye_r, 5, 5);
    lv_obj_align(h_eye_r, LV_ALIGN_TOP_MID, 7, 49);
    lv_obj_set_style_bg_color(h_eye_r, COL_EYES, 0);
    lv_obj_set_style_bg_opa(h_eye_r, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(h_eye_r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(h_eye_r, 0, 0);
    lv_obj_clear_flag(h_eye_r, LV_OBJ_FLAG_SCROLLABLE);

    // Stem
    lv_obj_t *stem = lv_obj_create(h_container);
    lv_obj_remove_style_all(stem);
    lv_obj_set_size(stem, 22, 10);
    lv_obj_align(stem, LV_ALIGN_TOP_MID, 0, 73);
    lv_obj_set_style_bg_color(stem, COL_STEM, 0);
    lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(stem, 5, 0);
    lv_obj_set_style_border_width(stem, 0, 0);
    lv_obj_clear_flag(stem, LV_OBJ_FLAG_SCROLLABLE);

    // Gold sparkle pool — faded in by spark_tick_cb during typing
    for (int i = 0; i < N_SPARKS; i++) {
        h_sparks[i] = lv_obj_create(h_container);
        lv_obj_remove_style_all(h_sparks[i]);
        lv_obj_set_size(h_sparks[i], 3, 3);
        lv_obj_set_pos(h_sparks[i], sx[i], sy[i]);
        lv_obj_set_style_bg_color(h_sparks[i], COL_GLOW, 0);
        lv_obj_set_style_bg_opa(h_sparks[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(h_sparks[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(h_sparks[i], 0, 0);
        lv_obj_set_style_opa(h_sparks[i], LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(h_sparks[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    // Blink every ~4.5 s
    lv_timer_create(blink_timer_cb, 4500, nullptr);

    // Start idle bob and glow
    hotaru_apply_state();
}

static void build_sidebar(lv_obj_t *scr) {
    lv_obj_t *sb = make_panel(scr, 0, CONTENT_Y, SIDEBAR_W, CONTENT_H);
    hotaru_build(sb);
}

static void build_vertical_divider(lv_obj_t *scr) {
    lv_obj_t *div = lv_obj_create(scr);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, 1, CONTENT_H);
    lv_obj_set_pos(div, SIDEBAR_W, CONTENT_Y);
    lv_obj_set_style_bg_color(div, COL_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
}

// ---------------------------------------------------------------------------
// Music panel — right-side animation state machine
// Three states: MA_NOTES (playing), MA_PAUSE (paused/idle), MA_GRASS (disc.)
// lv_obj_clean() on the container removes children + their animations.
// ---------------------------------------------------------------------------

// Shared animation callbacks
static void note_bob_cb(void *obj, int32_t val) {
    lv_obj_set_y((lv_obj_t*)obj, val);
}
static void grass_h_cb(void *obj, int32_t val) {
    lv_obj_set_height((lv_obj_t*)obj, val);
    lv_obj_set_y((lv_obj_t*)obj, MUSIC_H - 6 - val);
}
static void grass_x_cb(void *obj, int32_t val) {
    lv_obj_set_x((lv_obj_t*)obj, val);
}

// Helper — create a solid circle/rect inside parent
static lv_obj_t *ma_rect(lv_obj_t *p, int x, int y, int w, int h,
                          lv_color_t col, int radius = 0) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// Two quarter notes (head + stem) that float slowly together
static void build_ma_notes(lv_obj_t *ctr) {
    // Note group floats as one unit; MA_W×40, initial y=12
    lv_obj_t *grp = lv_obj_create(ctr);
    lv_obj_remove_style_all(grp);
    lv_obj_set_size(grp, MA_W, 40);
    lv_obj_set_pos(grp, 0, 12);
    lv_obj_set_style_bg_opa(grp, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grp, 0, 0);
    lv_obj_clear_flag(grp, LV_OBJ_FLAG_SCROLLABLE);

    // Beam connecting both stems at the top (♫ style)
    ma_rect(grp, 10, 4, 20, 4, COL_GLOW, 0);   // horizontal beam

    // Stem 1 (left, longer) — top at y=4, bottom at y=26
    ma_rect(grp, 10, 4, 2, 22, COL_GLOW, 0);
    // Head 1 (lower note)
    ma_rect(grp,  5, 23, 6, 5, COL_GLOW, 3);

    // Stem 2 (right, shorter) — top at y=4, bottom at y=20
    ma_rect(grp, 28, 4, 2, 16, COL_GLOW, 0);
    // Head 2 (higher note)
    ma_rect(grp, 23, 17, 6, 5, COL_GLOW, 3);

    // Gentle float: y oscillates 8..16 (±4 around 12), 3 s period
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, grp);
    lv_anim_set_exec_cb(&a, note_bob_cb);
    lv_anim_set_values(&a, 8, 16);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_playback_time(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// Classic pause symbol: two vertical bars, static
static void build_ma_pause(lv_obj_t *ctr) {
    int bar_h = 22;
    int bar_y = (MUSIC_H - bar_h) / 2;  // vertically centred
    ma_rect(ctr, 10, bar_y, 5, bar_h, COL_TEXT_SEC, 2);
    ma_rect(ctr, 22, bar_y, 5, bar_h, COL_TEXT_SEC, 2);
}

// Three grass blades swaying gently — shown when disconnected
static void build_ma_grass(lv_obj_t *ctr) {
    static const int bx[3]       = { 6, 18, 30};
    static const int bh[3]       = {18, 22, 16};
    static const uint32_t bd[3]  = { 0, 500, 1000};
    const int y_bot = MUSIC_H - 6;

    static const uint32_t sx_delay[3] = {200, 700, 400};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *blade = ma_rect(ctr, bx[i], y_bot - bh[i], 3, bh[i], COL_STEM, 2);

        // Height animation (roots stay fixed, tips rise and fall)
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, blade);
        lv_anim_set_exec_cb(&a, grass_h_cb);
        lv_anim_set_values(&a, bh[i] - 4, bh[i] + 4);
        lv_anim_set_time(&a, 1500);
        lv_anim_set_playback_time(&a, 1500);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_delay(&a, bd[i]);
        lv_anim_start(&a);

        // X sway animation (lateral lean, slightly different period)
        lv_anim_t ax;
        lv_anim_init(&ax);
        lv_anim_set_var(&ax, blade);
        lv_anim_set_exec_cb(&ax, grass_x_cb);
        lv_anim_set_values(&ax, bx[i] - 2, bx[i] + 2);
        lv_anim_set_time(&ax, 1000);
        lv_anim_set_playback_time(&ax, 1000);
        lv_anim_set_repeat_count(&ax, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&ax, lv_anim_path_ease_in_out);
        lv_anim_set_delay(&ax, sx_delay[i]);
        lv_anim_start(&ax);
    }
}

static void set_music_anim(MusicAnim next) {
    if (next == ma_state) return;
    ma_state = next;
    lv_obj_clean(music_anim_ctr);
    switch (next) {
        case MA_NOTES: build_ma_notes(music_anim_ctr); break;
        case MA_PAUSE: build_ma_pause(music_anim_ctr); break;
        case MA_GRASS: build_ma_grass(music_anim_ctr); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------

static void update_music_ui() {
    MusicAnim anim;
    if (!state.connected || !state.music_active) {
        anim = MA_GRASS;
    } else if (state.music_playing) {
        anim = MA_NOTES;
    } else {
        anim = MA_PAUSE;
    }
    set_music_anim(anim);

    if (state.music_active) {
        lv_color_t dc = state.music_playing ? COL_OK : COL_TEXT_DIM;
        lv_obj_set_style_bg_color(dot_music, dc, 0);
        lv_label_set_text(lbl_music_title, state.music_title);
        lv_obj_set_style_text_color(lbl_music_title,
            state.music_playing ? COL_TEXT_PRI : COL_TEXT_SEC, 0);
        lv_label_set_text(lbl_music_artist, state.music_artist);
        lv_obj_set_style_text_color(lbl_music_artist, COL_TEXT_SEC, 0);
    } else {
        lv_obj_set_style_bg_color(dot_music, COL_TEXT_DIM, 0);
        lv_label_set_text(lbl_music_title, "nothing playing");
        lv_obj_set_style_text_color(lbl_music_title, COL_TEXT_DIM, 0);
        lv_label_set_text(lbl_music_artist, state.idle_msg);
        lv_obj_set_style_text_color(lbl_music_artist, COL_TEXT_DIM, 0);
    }
}

// Fills panel_music (RWIDTH × MUSIC_H ≈ 224 × 52 px).
static void build_music_panel(lv_obj_t *parent) {
    // Playback indicator dot
    dot_music = lv_obj_create(parent);
    lv_obj_remove_style_all(dot_music);
    lv_obj_set_size(dot_music, 6, 6);
    lv_obj_set_pos(dot_music, 8, (MUSIC_H - 6) / 2);
    lv_obj_set_style_bg_color(dot_music, COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(dot_music, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot_music, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_music, 0, 0);
    lv_obj_clear_flag(dot_music, LV_OBJ_FLAG_SCROLLABLE);

    // Track title
    lbl_music_title = lv_label_create(parent);
    lv_label_set_text(lbl_music_title, "nothing playing");
    lv_obj_set_style_text_color(lbl_music_title, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_music_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_music_title, 22, 8);
    lv_obj_set_width(lbl_music_title, MUSIC_LABEL_W);
    lv_label_set_long_mode(lbl_music_title, LV_LABEL_LONG_DOT);

    // Artist / idle message
    lbl_music_artist = lv_label_create(parent);
    lv_label_set_text(lbl_music_artist, "");
    lv_obj_set_style_text_color(lbl_music_artist, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_music_artist, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_music_artist, 22, 30);
    lv_obj_set_width(lbl_music_artist, MUSIC_LABEL_W);
    lv_label_set_long_mode(lbl_music_artist, LV_LABEL_LONG_DOT);

    // Right-side animation container — content rebuilt by set_music_anim()
    music_anim_ctr = lv_obj_create(parent);
    lv_obj_remove_style_all(music_anim_ctr);
    lv_obj_set_size(music_anim_ctr, MA_W, MUSIC_H);
    lv_obj_set_pos(music_anim_ctr, MA_X, 0);
    lv_obj_set_style_bg_opa(music_anim_ctr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(music_anim_ctr, 0, 0);
    lv_obj_clear_flag(music_anim_ctr, LV_OBJ_FLAG_SCROLLABLE);

    // Boot state: not yet connected → show grass
    build_ma_grass(music_anim_ctr);
    ma_state = MA_GRASS;
}

// Helper: create a small static window-label ("5h" / "7d") in a rate-limit row
// Create a transparent overlay container inside the claude panel
static lv_obj_t *make_view(lv_obj_t *parent, int y, int h) {
    lv_obj_t *v = lv_obj_create(parent);
    lv_obj_remove_style_all(v);
    lv_obj_set_size(v, RWIDTH, h);
    lv_obj_set_pos(v, 0, y);
    lv_obj_set_style_bg_opa(v, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(v, 0, 0);
    lv_obj_clear_flag(v, LV_OBJ_FLAG_SCROLLABLE);
    return v;
}

// Rotation timer: swap which view is visible
static void claude_rotate_cb(lv_timer_t *) {
    if (state.claude_h5_pct < 0) return;
    claude_show_rl  = !claude_show_rl;
    claude_switched_ms = millis();
    if (claude_show_rl) {
        lv_obj_add_flag(claude_view_tok,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(claude_view_rl, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(claude_view_tok, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(claude_view_rl,   LV_OBJ_FLAG_HIDDEN);
    }
}

// Progress timer: fill bar_claude_prog from 0→100 over each CLAUDE_ROTATE_MS period
static void claude_prog_cb(lv_timer_t *) {
    if (!bar_claude_prog || state.claude_h5_pct < 0) return;
    uint32_t elapsed = millis() - claude_switched_ms;
    int pct = (int)((float)elapsed / (float)CLAUDE_ROTATE_MS * 100.0f);
    if (pct > 100) pct = 100;
    lv_bar_set_value(bar_claude_prog, pct, LV_ANIM_OFF);
}

// Fills panel_claude.  Panel is RWIDTH × CLAUDE_H (≈224 × 90 px).
// Header (dot + "claude") is shared; two sub-views sit below it.
static void build_claude_panel(lv_obj_t *parent) {
    // Shared indicator dot
    dot_claude = lv_obj_create(parent);
    lv_obj_remove_style_all(dot_claude);
    lv_obj_set_size(dot_claude, 6, 6);
    lv_obj_set_pos(dot_claude, 8, 7);
    lv_obj_set_style_bg_color(dot_claude, COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(dot_claude, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot_claude, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_claude, 0, 0);
    lv_obj_clear_flag(dot_claude, LV_OBJ_FLAG_SCROLLABLE);

    // Shared section label
    lv_obj_t *lbl_hdr = lv_label_create(parent);
    lv_label_set_text(lbl_hdr, "claude");
    lv_obj_set_style_text_color(lbl_hdr, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_hdr, 20, 3);

    // View-switch progress bar — top-right of header, hidden until both views active
    bar_claude_prog = make_bar(parent, RWIDTH - 70, 9, 62, 4);
    lv_obj_set_style_bg_color(bar_claude_prog, COL_TEXT_DIM, LV_PART_INDICATOR);
    lv_bar_set_value(bar_claude_prog, 0, LV_ANIM_OFF);
    lv_obj_add_flag(bar_claude_prog, LV_OBJ_FLAG_HIDDEN);

    const int VIEW_Y = 20, VIEW_H = CLAUDE_H - VIEW_Y;

    // ---- TOKEN VIEW (always visible when no API key) --------------------
    claude_view_tok = make_view(parent, VIEW_Y, VIEW_H);

    // "out today" right-aligned sub-label
    lv_obj_t *lbl_out_hdr = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_out_hdr, "out today");
    lv_obj_set_style_text_color(lbl_out_hdr, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_out_hdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(lbl_out_hdr, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_out_hdr, RWIDTH - 70, 2);
    lv_obj_set_width(lbl_out_hdr, 62);

    // Large output token count
    lbl_claude_out = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_claude_out, "0");
    lv_obj_set_style_text_color(lbl_claude_out, COL_GLOW, 0);
    lv_obj_set_style_text_font(lbl_claude_out, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl_claude_out, 8, 2);

    // Progress bar vs 100k daily reference
    bar_claude_tok = make_bar(claude_view_tok, 8, 30, RWIDTH - 16, 6);
    lv_obj_set_style_bg_color(bar_claude_tok, COL_GLOW, LV_PART_INDICATOR);

    // Bottom row: input tokens (left) + sessions (right)
    lbl_claude_in = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_claude_in, "in  0");
    lv_obj_set_style_text_color(lbl_claude_in, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_claude_in, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_claude_in, 8, 44);

    lbl_claude_sess = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_claude_sess, "0 sess");
    lv_obj_set_style_text_color(lbl_claude_sess, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_claude_sess, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_claude_sess, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_claude_sess, RWIDTH / 2, 44);
    lv_obj_set_width(lbl_claude_sess, RWIDTH / 2 - 8);

    // ---- RATE-LIMIT VIEW (hidden until API data arrives) ----------------
    claude_view_rl = make_view(parent, VIEW_Y, VIEW_H);
    lv_obj_add_flag(claude_view_rl, LV_OBJ_FLAG_HIDDEN);

    // 5h row
    lv_obj_t *l5h = lv_label_create(claude_view_rl);
    lv_label_set_text(l5h, "5h");
    lv_obj_set_style_text_color(l5h, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l5h, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(l5h, 8, 4);

    lbl_h5_pct = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_h5_pct, "--");
    lv_obj_set_style_text_font(lbl_h5_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_h5_pct, 30, 4);

    lbl_h5_reset = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_h5_reset, "--");
    lv_obj_set_style_text_color(lbl_h5_reset, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_h5_reset, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_h5_reset, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_h5_reset, RWIDTH - 78, 4);
    lv_obj_set_width(lbl_h5_reset, 70);

    bar_h5 = make_bar(claude_view_rl, 8, 18, RWIDTH - 16, 6);

    // 7d row
    lv_obj_t *l7d = lv_label_create(claude_view_rl);
    lv_label_set_text(l7d, "7d");
    lv_obj_set_style_text_color(l7d, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l7d, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(l7d, 8, 34);

    lbl_w7_pct = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_w7_pct, "--");
    lv_obj_set_style_text_font(lbl_w7_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_w7_pct, 30, 34);

    lbl_w7_reset = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_w7_reset, "--");
    lv_obj_set_style_text_color(lbl_w7_reset, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_w7_reset, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_w7_reset, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_w7_reset, RWIDTH - 78, 34);
    lv_obj_set_width(lbl_w7_reset, 70);

    bar_w7 = make_bar(claude_view_rl, 8, 48, RWIDTH - 16, 6);
}

static void update_claude_ui() {
    char buf[20], tmp[16];

    // ---- dot + progress bar visibility ----------------------------------
    lv_color_t dot_c = (state.claude_h5_pct >= 0)
                       ? pct_col(state.claude_h5_pct)
                       : (state.claude_sessions > 0 ? COL_OK : COL_TEXT_DIM);
    lv_obj_set_style_bg_color(dot_claude, dot_c, 0);

    // Show progress bar only when both views are active
    if (state.claude_h5_pct >= 0) {
        lv_obj_clear_flag(bar_claude_prog, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(bar_claude_prog, LV_OBJ_FLAG_HIDDEN);
    }

    // ---- token view -----------------------------------------------------
    fmt_k(buf, sizeof(buf), state.claude_out);
    lv_label_set_text(lbl_claude_out, buf);

    int32_t tok_pct = (int32_t)((float)state.claude_out / 100000.0f * 100.0f);
    if (tok_pct > 100) tok_pct = 100;
    lv_bar_set_value(bar_claude_tok, (int)tok_pct, LV_ANIM_OFF);

    fmt_k(tmp, sizeof(tmp), state.claude_inp);
    snprintf(buf, sizeof(buf), "in  %s", tmp);
    lv_label_set_text(lbl_claude_in, buf);

    snprintf(buf, sizeof(buf), "%d sess", state.claude_sessions);
    lv_label_set_text(lbl_claude_sess, buf);

    // ---- rate-limit view ------------------------------------------------
    // 5h
    if (state.claude_h5_pct < 0) {
        lv_label_set_text(lbl_h5_pct, "--");
        lv_obj_set_style_text_color(lbl_h5_pct, COL_TEXT_DIM, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", state.claude_h5_pct);
        lv_label_set_text(lbl_h5_pct, buf);
        lv_obj_set_style_text_color(lbl_h5_pct, pct_col(state.claude_h5_pct), 0);
    }
    fmt_reset(buf, sizeof(buf), state.claude_h5_secs);
    lv_label_set_text(lbl_h5_reset, buf);
    lv_bar_set_value(bar_h5, (state.claude_h5_pct < 0) ? 0 : state.claude_h5_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_h5, pct_col(state.claude_h5_pct), LV_PART_INDICATOR);

    // 7d
    if (state.claude_w7_pct < 0) {
        lv_label_set_text(lbl_w7_pct, "--");
        lv_obj_set_style_text_color(lbl_w7_pct, COL_TEXT_DIM, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", state.claude_w7_pct);
        lv_label_set_text(lbl_w7_pct, buf);
        lv_obj_set_style_text_color(lbl_w7_pct, pct_col(state.claude_w7_pct), 0);
    }
    fmt_reset(buf, sizeof(buf), state.claude_w7_secs);
    lv_label_set_text(lbl_w7_reset, buf);
    lv_bar_set_value(bar_w7, (state.claude_w7_pct < 0) ? 0 : state.claude_w7_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_w7, pct_col(state.claude_w7_pct), LV_PART_INDICATOR);
}

// Fills panel_status with the WPM / active-idle display.
// Panel is RWIDTH × STATUS_H (≈224 × 65 px).
static void build_wpm_panel(lv_obj_t *parent) {
    // Active / idle indicator dot — left side, vertically centred
    dot_status = lv_obj_create(parent);
    lv_obj_remove_style_all(dot_status);
    lv_obj_set_size(dot_status, 9, 9);
    lv_obj_set_pos(dot_status, 10, (STATUS_H - 9) / 2);
    lv_obj_set_style_bg_color(dot_status, COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(dot_status, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot_status, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_status, 0, 0);
    lv_obj_clear_flag(dot_status, LV_OBJ_FLAG_SCROLLABLE);

    // "IDLE" / "ACTIVE" state label
    lbl_status = lv_label_create(parent);
    lv_label_set_text(lbl_status, "IDLE");
    lv_obj_set_style_text_color(lbl_status, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_status, 26, (STATUS_H - 14) / 2);

    // Large WPM number — right side, upper half
    lbl_wpm_val = lv_label_create(parent);
    lv_label_set_text(lbl_wpm_val, "0");
    lv_obj_set_style_text_color(lbl_wpm_val, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_wpm_val, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_wpm_val, LV_ALIGN_RIGHT_MID, -8, -9);

    // "WPM" dim key below the number
    lv_obj_t *k_wpm = lv_label_create(parent);
    lv_label_set_text(k_wpm, "WPM");
    lv_obj_set_style_text_color(k_wpm, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(k_wpm, &lv_font_montserrat_10, 0);
    lv_obj_align(k_wpm, LV_ALIGN_RIGHT_MID, -8, 14);
}

static void build_right_panels(lv_obj_t *scr) {
    int y = CONTENT_Y;

    // Music panel
    panel_music = make_panel(scr, RSTART, y, RWIDTH, MUSIC_H);
    build_music_panel(panel_music);
    y += MUSIC_H;

    make_hdiv(scr, y, RSTART, RWIDTH);
    y += DIV_W;

    // Claude panel
    panel_claude = make_panel(scr, RSTART, y, RWIDTH, CLAUDE_H);
    build_claude_panel(panel_claude);
    y += CLAUDE_H;

    make_hdiv(scr, y, RSTART, RWIDTH);
    y += DIV_W;

    // Status / WPM panel
    panel_status = make_panel(scr, RSTART, y, RWIDTH, STATUS_H);
    build_wpm_panel(panel_status);
}

static void build_dashboard() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_topbar(scr);
    build_sidebar(scr);
    build_vertical_divider(scr);
    build_right_panels(scr);
}

// ---------------------------------------------------------------------------
// Serial packet handler
// ---------------------------------------------------------------------------
static void handle_packet(const String &line) {
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return;

    const char *type = doc["type"] | "";

    if (strcmp(type, "stats") == 0) {
        last_packet_ms = millis();
        state.connected = true;

        state.cpu    = doc["cpu"]    | 0;
        state.ram    = doc["ram"]    | 0;
        state.wpm    = doc["wpm"]    | 0;
        state.active = doc["active"] | false;
        strlcpy(state.time_str, doc["time"] | "--:--", sizeof(state.time_str));

        if (state.active) {
            last_active_ms = millis();
            if (sleep_state == SS_ASLEEP) exit_sleep();
        }
        if (sleep_state == SS_ASLEEP) {
            lv_label_set_text(lbl_sleep_time, state.time_str);
        }
        strlcpy(state.idle_msg, doc["idle_msg"] | "", sizeof(state.idle_msg));

        JsonObject music = doc["music"];
        if (music) {
            state.music_active  = true;
            state.music_playing = music["playing"] | false;
            strlcpy(state.music_title,  music["title"]  | "", sizeof(state.music_title));
            strlcpy(state.music_artist, music["artist"] | "", sizeof(state.music_artist));
        } else {
            state.music_active = false;
        }

        JsonObject claude_obj = doc["claude"];
        if (claude_obj) {
            state.claude_out      = claude_obj["out"]      | (int32_t)0;
            state.claude_inp      = claude_obj["inp"]      | (int32_t)0;
            state.claude_sessions = claude_obj["sessions"] | 0;
            state.claude_h5_pct   = claude_obj["h5_pct"]  | -1;
            state.claude_h5_secs  = claude_obj["h5_secs"] | -1;
            state.claude_w7_pct   = claude_obj["w7_pct"]  | -1;
            state.claude_w7_secs  = claude_obj["w7_secs"] | -1;
        }

        update_stats_ui();
        update_wpm_ui();
        update_music_ui();
        update_claude_ui();
        Serial.println("{\"ack\":true}");
    }
}

// ---------------------------------------------------------------------------
// Backlight + sleep
// ---------------------------------------------------------------------------
static void bl_set(uint8_t level) {
    ledcWrite(BL_PWM_CHANNEL, level);
}

static void build_sleep_overlay() {
    lv_obj_t *scr = lv_scr_act();

    sleep_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(sleep_overlay);
    lv_obj_set_size(sleep_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(sleep_overlay, 0, 0);
    lv_obj_set_style_bg_color(sleep_overlay, COL_BG, 0);
    lv_obj_set_style_bg_opa(sleep_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sleep_overlay, 0, 0);
    lv_obj_clear_flag(sleep_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);

    lbl_sleep_time = lv_label_create(sleep_overlay);
    lv_label_set_text(lbl_sleep_time, "--:--");
    lv_obj_set_style_text_color(lbl_sleep_time, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_sleep_time, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(lbl_sleep_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_sleep_time, SCREEN_W);
    lv_obj_set_pos(lbl_sleep_time, 0, (SCREEN_H - 30) / 2);

    // Three separate z labels so each can independently change font size
    static const int zx[3] = { 126, 152, 178 };
    int zy_big   = (SCREEN_H - 30) / 2 + 34;
    int zy_small = zy_big + 12;  // bottom-align font_12 with font_24
    for (int i = 0; i < 3; i++) {
        lbl_z[i] = lv_label_create(sleep_overlay);
        lv_label_set_text(lbl_z[i], "z");
        lv_obj_set_style_text_color(lbl_z[i], COL_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl_z[i], &lv_font_montserrat_12, 0);
        lv_obj_set_pos(lbl_z[i], zx[i], zy_small);
    }
}

static void zzz_cb(lv_timer_t *) {
    static int step = 0;
    int zy_big   = (SCREEN_H - 30) / 2 + 34;
    int zy_small = zy_big + 12;
    for (int i = 0; i < 3; i++) {
        bool big = (step == i + 1);
        lv_obj_set_style_text_font(lbl_z[i],
            big ? &lv_font_montserrat_24 : &lv_font_montserrat_12, 0);
        lv_obj_set_y(lbl_z[i], big ? zy_big : zy_small);
    }
    step = (step + 1) % 4;  // 0=all small, 1=first big, 2=mid big, 3=last big
}

static void enter_sleep() {
    if (sleep_state == SS_ASLEEP) return;
    sleep_state = SS_ASLEEP;
    lv_label_set_text(lbl_sleep_time, state.time_str);
    lv_obj_clear_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    bl_set(BL_DIM);
    zzz_timer = lv_timer_create(zzz_cb, 700, nullptr);
}

static void exit_sleep() {
    if (sleep_state == SS_AWAKE) return;
    sleep_state = SS_AWAKE;
    if (zzz_timer) { lv_timer_del(zzz_timer); zzz_timer = nullptr; }
    lv_obj_add_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    bl_set(BL_FULL);
}

// ---------------------------------------------------------------------------
// Disconnection handling
// ---------------------------------------------------------------------------
static void show_disconnected() {
    state.cpu = 0; state.ram = 0; state.wpm = 0;
    state.active = false; state.music_active = false;
    update_stats_ui();
    update_music_ui();
    // Override stats labels to "--" instead of "0%"
    lv_label_set_text(lbl_cpu_val, "--");
    lv_label_set_text(lbl_ram_val, "--");
    lv_obj_set_style_text_color(lbl_cpu_val, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_color(lbl_ram_val, COL_TEXT_DIM, 0);
    // WPM panel: show DISC with alert colour
    lv_obj_set_style_bg_color(dot_status, COL_ALERT, 0);
    lv_label_set_text(lbl_status, "DISC");
    lv_obj_set_style_text_color(lbl_status, COL_ALERT, 0);
    lv_label_set_text(lbl_wpm_val, "--");
    // Return Hotaru to idle
    hotaru_update(0, false);
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // Must be after tft.init() — otherwise tft.init() resets pin to digitalWrite
    ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_BITS);
    ledcAttachPin(BL_PIN, BL_PWM_CHANNEL);
    ledcWrite(BL_PWM_CHANNEL, BL_FULL);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lv_buf, nullptr, SCREEN_W * 10);

    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res  = SCREEN_W;
    drv.ver_res  = SCREEN_H;
    drv.flush_cb = disp_flush;
    drv.draw_buf = &draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&drv);

    lv_theme_t *th = lv_theme_basic_init(disp);
    lv_disp_set_theme(disp, th);

    touch_spi.begin(25, 39, 32, 33);  // SCK, MISO, MOSI, CS
    ts.begin(touch_spi);
    ts.setRotation(1);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    build_dashboard();
    build_sleep_overlay();
    last_active_ms = millis();

#if CLAUDE_ROTATE_MS > 0
    claude_switched_ms = millis();
    claude_rot_timer   = lv_timer_create(claude_rotate_cb, CLAUDE_ROTATE_MS, nullptr);
    claude_prog_timer  = lv_timer_create(claude_prog_cb,   200,              nullptr);
#endif

    Serial.println("{\"boot\":true,\"version\":\"0.6\"}");
}

void loop() {
    lv_timer_handler();

    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) handle_packet(line);
    }

    if (state.connected && millis() - last_packet_ms > DISCONNECT_TIMEOUT_MS) {
        state.connected = false;
        show_disconnected();
    }

    if (sleep_state == SS_AWAKE && millis() - last_active_ms > SLEEP_TIMEOUT_MS) {
        enter_sleep();
    }

    delay(5);
}
