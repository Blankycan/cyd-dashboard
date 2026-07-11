#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <ArduinoJson.h>

#include "layout.h"
#include "state.h"
#include "ui_helpers.h"
#include "theme.h"
#include "widgets/topbar.h"
#include "widgets/music.h"
#include "widgets/stats.h"
#include "widgets/claude.h"
#include "widgets/indicator.h"

// CYD Dashboard firmware entry point.
// Host PC sends JSON stats over serial (115200); LVGL renders the UI on the TFT.

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
DashState state;

// ---------------------------------------------------------------------------
// LVGL / display
// ---------------------------------------------------------------------------
static TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t lv_buf[SCREEN_W * 10];

// Push an LVGL dirty region to the TFT via SPI
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t*>(&px->full), w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

// ---------------------------------------------------------------------------
// Sleep state — declared early so touch_read_cb can reference it
// ---------------------------------------------------------------------------
enum SleepState { SS_AWAKE, SS_ASLEEP };
SleepState sleep_state = SS_AWAKE;

// ---------------------------------------------------------------------------
// Touch — XPT2046 on HSPI (CYD wiring: SCK=25, MISO=39, MOSI=32, CS=33)
// ---------------------------------------------------------------------------
static SPIClass        touch_spi(HSPI);
static XPT2046_Touchscreen ts(33, 36);

static uint32_t last_active_ms = 0;
static void exit_sleep();   // forward decl

// LVGL touch input callback — maps raw ADC coords to screen pixels
static void touch_read_cb(lv_indev_drv_t *, lv_indev_data_t *data) {
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint();
        data->point.x = map(p.x, 200, 3900, 0, SCREEN_W - 1);
        data->point.y = map(p.y, 200, 3900, 0, SCREEN_H - 1);
        data->state   = LV_INDEV_STATE_PRESSED;
        last_active_ms = millis();
        if (sleep_state == SS_ASLEEP) exit_sleep();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ---------------------------------------------------------------------------
// Backlight
// ---------------------------------------------------------------------------
static void bl_set(uint8_t level) {
    ledcWrite(BL_PWM_CHANNEL, level);
}

// ---------------------------------------------------------------------------
// Sleep overlay
// ---------------------------------------------------------------------------
static lv_obj_t  *sleep_overlay  = nullptr;
static lv_obj_t  *lbl_sleep_time = nullptr;
static lv_obj_t  *lbl_z[3]      = {};
static lv_timer_t *zzz_timer     = nullptr;

// Animate the three "z" labels on the sleep screen (cycling which one is large)
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
    step = (step + 1) % 4;
}

// Full-screen overlay shown when the display enters sleep mode
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

    static const int zx[3] = { 100, 116, 132 };
    int zy_small = (SCREEN_H - 30) / 2 + 34 + 12;
    for (int i = 0; i < 3; i++) {
        lbl_z[i] = lv_label_create(sleep_overlay);
        lv_label_set_text(lbl_z[i], "z");
        lv_obj_set_style_text_color(lbl_z[i], COL_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl_z[i], &lv_font_montserrat_12, 0);
        lv_obj_set_pos(lbl_z[i], zx[i], zy_small);
    }
}

// Dim backlight and show sleep overlay after SLEEP_TIMEOUT_MS of inactivity
static void enter_sleep() {
    if (sleep_state == SS_ASLEEP) return;
    sleep_state = SS_ASLEEP;
    lv_label_set_text(lbl_sleep_time, state.time_str);
    lv_obj_clear_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    bl_set(BL_DIM);
    zzz_timer = lv_timer_create(zzz_cb, 700, nullptr);
}

// Restore full brightness and hide overlay (called on touch or host activity)
static void exit_sleep() {
    if (sleep_state == SS_AWAKE) return;
    sleep_state = SS_AWAKE;
    if (zzz_timer) { lv_timer_del(zzz_timer); zzz_timer = nullptr; }
    lv_obj_add_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    bl_set(BL_FULL);
}

// ---------------------------------------------------------------------------
// Dashboard layout — stacks music, stats, claude, and status panels vertically
// ---------------------------------------------------------------------------
static void build_panels(lv_obj_t *scr) {
    int y = CONTENT_Y;

    lv_obj_t *p;

    p = make_panel(scr, 0, y, SCREEN_W, MUSIC_H);
    build_music_panel(p);
    y += MUSIC_H;

    make_hdiv(scr, y, 0, SCREEN_W);
    y += DIV_W;

    p = make_panel(scr, 0, y, SCREEN_W, STATS_H);
    build_stats_panel(p);
    y += STATS_H;

    make_hdiv(scr, y, 0, SCREEN_W);
    y += DIV_W;

    p = make_panel(scr, 0, y, SCREEN_W, CLAUDE_H);
    build_claude_panel(p);
    y += CLAUDE_H;

    make_hdiv(scr, y, 0, SCREEN_W);

    p = make_panel(scr, 0, SCREEN_H - INDICATOR_H, SCREEN_W, INDICATOR_H);
    build_indicator_panel(p);
}

static void build_dashboard() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_topbar(scr);
    build_panels(scr);
}

// ---------------------------------------------------------------------------
// Serial packet handler — JSON lines from host, type "stats" updates all widgets
// ---------------------------------------------------------------------------
static uint32_t last_packet_ms = 0;

// Clear numeric values and refresh UI when host stops sending packets
static void show_disconnected() {
    state.cpu = 0; state.ram = 0; state.wpm = 0;
    state.active = false; state.music_active = false;
    update_stats_ui();
    stats_show_disconnected();
    update_music_ui();
    update_status_ui();
}

// Parse one JSON line from serial and update state + UI widgets.
// Expected shape: {"type":"stats","cpu":..,"ram":..,"wpm":..,"time":"..","date":"..",
//   "active":bool,"idle_msg":"..","ip":"..","music":{..},"claude":{..}}
static void handle_packet(const String &line) {
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return;

    const char *type = doc["type"] | "";

    if (strcmp(type, "stats") == 0) {
        last_packet_ms  = millis();
        state.connected = true;

        state.cpu    = doc["cpu"]    | 0;
        state.ram    = doc["ram"]    | 0;
        state.wpm    = doc["wpm"]    | 0;
        state.active = doc["active"] | false;
        strlcpy(state.time_str, doc["time"] | "--:--", sizeof(state.time_str));
        strlcpy(state.date_str, doc["date"] | "",     sizeof(state.date_str));
        strlcpy(state.idle_msg, doc["idle_msg"] | "", sizeof(state.idle_msg));
        strlcpy(state.ip_str,   doc["ip"]       | "", sizeof(state.ip_str));

        if (state.active) {
            last_active_ms = millis();
            if (sleep_state == SS_ASLEEP) exit_sleep();
        }
        if (sleep_state == SS_ASLEEP) {
            lv_label_set_text(lbl_sleep_time, state.time_str);
        }

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

        update_topbar_ui();
        update_stats_ui();
        update_music_ui();
        update_claude_ui();
        update_status_ui();
        Serial.println("{\"ack\":true}");
    }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

// Init display, touch, LVGL, build UI, announce boot on serial
void setup() {
    Serial.begin(115200);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

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

    touch_spi.begin(25, 39, 32, 33);
    ts.begin(touch_spi);
    ts.setRotation(0);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    build_dashboard();
    build_sleep_overlay();
    last_active_ms = millis();

    Serial.println("{\"boot\":true,\"version\":\"0.8\"}");
}

// LVGL tick, serial RX, disconnect detection, and auto-sleep
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
