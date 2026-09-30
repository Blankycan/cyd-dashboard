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
#include "widgets/calendar.h"
#include "widgets/music.h"
#include "widgets/system.h"
#include "widgets/claude.h"
#include "widgets/status.h"
#include "scenes/scene_player.h"

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

// LVGL touch input callback — maps raw ADC coords to screen pixels and
// forwards press/drag/release to the scene player. A touch that wakes the
// display is swallowed, so waking never also flips or pokes a scene.
static void touch_read_cb(lv_indev_drv_t *, lv_indev_data_t *data) {
    static bool down = false, swallow = false;
    static int  last_x = 0, last_y = 0;
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint();
        last_x = map(p.x, 200, 3900, 0, SCREEN_W - 1);
        last_y = map(p.y, 200, 3900, 0, SCREEN_H - 1);
        data->point.x = last_x;
        data->point.y = last_y;
        data->state   = LV_INDEV_STATE_PRESSED;
        last_active_ms = millis();
        if (sleep_state == SS_ASLEEP) { exit_sleep(); swallow = true; }
        if (!swallow) scene_player_touch(down ? SCENE_TOUCH_DRAG : SCENE_TOUCH_PRESS, last_x, last_y);
        down = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        if (down && !swallow) scene_player_touch(SCENE_TOUCH_RELEASE, last_x, last_y);
        down = swallow = false;
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
static lv_obj_t  *sleep_overlay   = nullptr;
static lv_obj_t  *lbl_sleep_time  = nullptr;
static lv_obj_t  *lbl_sleep_away  = nullptr;
static lv_obj_t  *lbl_z[3]       = {};
static lv_timer_t *zzz_timer      = nullptr;
static uint32_t   last_keyboard_ms = 0;    // millis() of last state.active packet

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

// Format how long since last keyboard activity into buf (e.g. "away 47m", "away 2h 3m")
static void fmt_away(char *buf, size_t n) {
    if (!last_keyboard_ms || state.last_active_str[0] == '\0') {
        buf[0] = '\0';
        return;
    }
    uint32_t mins = (millis() - last_keyboard_ms) / 60000UL;
    uint32_t hrs  = mins / 60;
    if (hrs > 0)
        snprintf(buf, n, "away %luh %lum", (unsigned long)hrs, (unsigned long)(mins % 60));
    else
        snprintf(buf, n, "away %lum", (unsigned long)mins);
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

    lbl_sleep_away = lv_label_create(sleep_overlay);
    lv_label_set_text(lbl_sleep_away, "");
    lv_obj_set_style_text_color(lbl_sleep_away, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_sleep_away, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_sleep_away, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_sleep_away, SCREEN_W);
    lv_obj_set_pos(lbl_sleep_away, 0, SCREEN_H - 30);
}

// Dim backlight and show sleep overlay after SLEEP_TIMEOUT_MS of inactivity
static void enter_sleep() {
    if (sleep_state == SS_ASLEEP) return;
    sleep_state = SS_ASLEEP;
    lv_label_set_text(lbl_sleep_time, state.time_str);
    char away[24];
    fmt_away(away, sizeof(away));
    lv_label_set_text(lbl_sleep_away, away);
    lv_obj_clear_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    bl_set(BL_DIM);
    scene_player_set_paused(true);
    zzz_timer = lv_timer_create(zzz_cb, 700, nullptr);
}

// Restore full brightness and hide overlay (called on touch or host activity)
static void exit_sleep() {
    if (sleep_state == SS_AWAKE) return;
    sleep_state = SS_AWAKE;
    if (zzz_timer) { lv_timer_del(zzz_timer); zzz_timer = nullptr; }
    lv_obj_add_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
    bl_set(BL_FULL);
    scene_player_set_paused(false);
}

// ---------------------------------------------------------------------------
// Offline clock — advances time_str from stored h/m + elapsed millis()
// so the display keeps ticking even when the companion is disconnected.
// ---------------------------------------------------------------------------
static void clock_tick_cb(lv_timer_t *) {
    int total = dash_now_min();
    if (total < 0) return;
    snprintf(state.time_str, sizeof(state.time_str), "%02d:%02d", total / 60, total % 60);
    update_topbar_ui();
    update_calendar_ui();

    static int last_hour = -1;
    if (total / 60 != last_hour) {
        if (last_hour >= 0) {
            SceneEvent e = {};
            e.type  = SCENE_EV_HOUR;
            e.value = total / 60;
            scene_player_event(e);
        }
        last_hour = total / 60;
    }
    if (sleep_state == SS_ASLEEP) {
        lv_label_set_text(lbl_sleep_time, state.time_str);
        char away[24];
        fmt_away(away, sizeof(away));
        lv_label_set_text(lbl_sleep_away, away);
    }
}

// ---------------------------------------------------------------------------
// Dashboard layout — stacks calendar, music, stats, claude, and status panels vertically
// ---------------------------------------------------------------------------
static void build_panels(lv_obj_t *scr) {
    int y = CONTENT_Y;

    lv_obj_t *p;

    p = make_panel(scr, 0, y, SCREEN_W, CALENDAR_H, COL_CALENDAR_BG);
    build_calendar_panel(p);
    scene_player_init(p);   // ambient scenes take turns with the calendar in this slot
    y += CALENDAR_H;

    make_hdiv(scr, y, 0, SCREEN_W, COL_DIVIDER);
    y += DIV_W;

    p = make_panel(scr, 0, y, SCREEN_W, MUSIC_H, COL_MUSIC_BG);
    build_music_panel(p);
    y += MUSIC_H;

    make_hdiv(scr, y, 0, SCREEN_W, COL_DIVIDER);
    y += DIV_W;

    p = make_panel(scr, 0, y, SCREEN_W, STATS_H, COL_SYSTEM_BG);
    build_system_panel(p);
    y += STATS_H;

    make_hdiv(scr, y, 0, SCREEN_W, COL_DIVIDER);
    y += DIV_W;

    p = make_panel(scr, 0, y, SCREEN_W, CLAUDE_H, COL_CLAUDE_BG);
    build_claude_panel(p);
    y += CLAUDE_H;

    make_hdiv(scr, y, 0, SCREEN_W, COL_DIVIDER);

    p = make_panel(scr, 0, SCREEN_H - INDICATOR_H, SCREEN_W, INDICATOR_H, COL_STATUS_BG);
    build_status_panel(p);
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
    state.cpu = 0; state.ram = 0; state.keys_today = 0;
    state.active = false; state.music_active = false;
    state.claude_sessions = 0;
    state.claude_working = 0;
    state.claude_h5_pct = -1; state.claude_h5_secs = -1;
    state.claude_w7_pct = -1; state.claude_w7_secs = -1;
    update_system_ui();
    system_show_disconnected();
    update_music_ui();
    update_claude_ui();
    update_status_ui();

    // Scenes hear that the music and Claude went quiet too
    SceneEvent e = {};
    if (scene_ctx().music != SCENE_MUSIC_NONE) {
        e.type  = SCENE_EV_MUSIC;
        e.music = SCENE_MUSIC_NONE;
        scene_player_event(e);
    }
    if (scene_ctx().claude_working != 0) {
        e.type  = SCENE_EV_CLAUDE;
        e.value = 0;
        scene_player_event(e);
    }
}

// Parse one JSON line from serial and update state + UI widgets.
// Expected shape: {"type":"stats","cpu":..,"ram":..,"keys":..,"time":"..","date":"..",
//   "active":bool,"idle_msg":"..","ip":"..","music":{..},"claude":{..},
//   "cal":[[start_min,end_min,"title"],..]}
static SceneMusic music_state() {
    if (!state.connected || !state.music_active) return SCENE_MUSIC_NONE;
    return state.music_playing ? SCENE_MUSIC_PLAYING : SCENE_MUSIC_PAUSED;
}

// Live events sent between stats packets: key presses, beats, audio frames.
// Returns false if `type` isn't one of them.
static bool handle_live_event(const char *type, JsonDocument &doc) {
    if (strcmp(type, "key") == 0) {
        SceneEvent e = {};
        e.type = SCENE_EV_KEY;
        const char *k = doc["k"] | "o";
        e.key = k[0] == 'c' ? SCENE_KEY_CHAR     : k[0] == 's' ? SCENE_KEY_SPACE
              : k[0] == 'e' ? SCENE_KEY_ENTER    : k[0] == 'b' ? SCENE_KEY_BACKSPACE
              : k[0] == 'm' ? SCENE_KEY_MODIFIER : SCENE_KEY_OTHER;
        scene_player_event(e);
        // Typing is activity: wake right away instead of on the next stats packet
        last_keyboard_ms = last_active_ms = millis();
        if (sleep_state == SS_ASLEEP) exit_sleep();
        return true;
    }
    if (strcmp(type, "beat") == 0) {
        SceneEvent e = {};
        e.type     = SCENE_EV_BEAT;
        e.strength = (uint8_t)constrain((int)(doc["s"] | 50), 1, 100);
        scene_player_event(e);
        return true;
    }
    if (strcmp(type, "au") == 0) {
        scene_player_audio(doc["i"] | 0, doc["b"] | 0, doc["bpm"] | 0);
        return true;
    }
    return false;
}

static void handle_packet(const String &line) {
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return;

    const char *type = doc["type"] | "";
    if (handle_live_event(type, doc)) return;

    if (strcmp(type, "stats") == 0) {
        // For change events after the packet is applied
        SceneMusic prev_music   = music_state();
        int        prev_working = state.claude_working;
        char       prev_title[sizeof(state.music_title)];
        strlcpy(prev_title, state.music_title, sizeof(prev_title));

        bool was_connected = state.connected;
        last_packet_ms     = millis();
        state.connected    = true;

        state.cpu    = doc["cpu"]    | 0;
        state.ram    = doc["ram"]    | 0;
        state.keys_today = doc["keys"] | (int32_t)0;
        state.active = doc["active"] | false;
        strlcpy(state.time_str, doc["time"] | "--:--", sizeof(state.time_str));
        strlcpy(state.date_str, doc["date"] | "",     sizeof(state.date_str));
        strlcpy(state.idle_msg, doc["idle_msg"] | "", sizeof(state.idle_msg));
        strlcpy(state.ip_str,   doc["ip"]       | "", sizeof(state.ip_str));

        // Store parsed h/m so the offline clock can keep ticking after disconnect
        {
            int h = -1, m = -1;
            if (sscanf(state.time_str, "%d:%d", &h, &m) == 2) {
                state.time_h      = (int8_t)h;
                state.time_m      = (int8_t)m;
                state.time_set_ms = millis();
            }
        }

        if (state.active) {
            last_keyboard_ms = millis();
            last_active_ms   = millis();
            strlcpy(state.last_active_str, state.time_str, sizeof(state.last_active_str));
            if (sleep_state == SS_ASLEEP) exit_sleep();
        }

        // Also wake on reconnection (companion restarted while screen was sleeping).
        if (!was_connected) {
            last_active_ms = millis();
            if (sleep_state == SS_ASLEEP) exit_sleep();
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
            state.claude_sessions = claude_obj["sessions"] | 0;
            state.claude_working  = claude_obj["working"]  | 0;
            state.claude_h5_pct   = claude_obj["h5_pct"]  | -1;
            state.claude_h5_secs  = claude_obj["h5_secs"] | -1;
            state.claude_w7_pct   = claude_obj["w7_pct"]  | -1;
            state.claude_w7_secs  = claude_obj["w7_secs"] | -1;
        }

        JsonArray cal = doc["cal"];
        if (cal) {
            state.cal_available = true;
            state.cal_count     = 0;
            for (JsonArray ev : cal) {
                if (state.cal_count >= CAL_MAX_EVENTS) break;
                CalEvent &e = state.cal_events[state.cal_count++];
                e.start_min = ev[0] | 0;
                e.end_min   = ev[1] | 0;
                strlcpy(e.title, ev[2] | "", sizeof(e.title));
            }
        }

        SceneEvent e = {};
        if (music_state() != prev_music) {
            e.type  = SCENE_EV_MUSIC;
            e.music = music_state();
            scene_player_event(e);
        }
        if (state.music_active && prev_title[0] && strcmp(prev_title, state.music_title) != 0) {
            e.type = SCENE_EV_TRACK;
            scene_player_event(e);
        }
        if (state.claude_working != prev_working) {
            e.type  = SCENE_EV_CLAUDE;
            e.value = state.claude_working;
            scene_player_event(e);
        }

        update_topbar_ui();
        update_calendar_ui();
        update_system_ui();
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
    // Packets with a full calendar run past the default 256-byte RX buffer,
    // which can overflow while lv_timer_handler() is busy redrawing.
    Serial.setRxBufferSize(2048);
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
    lv_timer_create(clock_tick_cb, 10000, nullptr);
    last_active_ms = millis();

    Serial.println("{\"boot\":true,\"version\":\"0.8\"}");

    // Reboot (with a backtrace on serial) if loop() ever stalls for 5 s, rather
    // than leaving a frozen screen until someone power-cycles the board.
    enableLoopWDT();
}

// LVGL tick, serial RX, disconnect detection, and auto-sleep
void loop() {
    lv_timer_handler();

    // Drain what's buffered, not just one line: with music playing the host
    // sends ~13 lines/s, and at one per pass a slow frame lets the 2 KB RX
    // buffer fill until stats packets get truncated. Capped so a flood can't
    // starve the display.
    for (int n = 0; n < 8 && Serial.available(); n++) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) handle_packet(line);
    }

    if (state.connected && millis() - last_packet_ms > DISCONNECT_TIMEOUT_MS) {
        state.connected = false;
        show_disconnected();
    }

    // Sleep after keyboard/touch inactivity regardless of connection state.
    if (sleep_state == SS_AWAKE && millis() - last_active_ms > SLEEP_TIMEOUT_MS) {
        enter_sleep();
    }

    delay(5);
}
