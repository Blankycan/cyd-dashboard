#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "config.h"

struct CalEvent {
    int16_t start_min;   // minutes since local midnight
    int16_t end_min;
    char    title[40];
};

// Shared application state — populated by serial JSON packets from the host PC
// and read by all widget update functions.
struct DashState {
    // Top bar / general
    char     time_str[6]  = "--:--";
    char     date_str[12] = "";
    int8_t   time_h       = -1;     // last received hour (-1 = no time yet)
    int8_t   time_m       = -1;     // last received minute
    uint32_t time_set_ms  = 0;      // millis() when time_h/m were stored
    char last_active_str[6] = "";   // HH:MM of last keyboard activity
    int  cpu              = 0;
    int  ram              = 0;
    int32_t keys_today    = 0;       // keypresses since local midnight
    bool active           = false;   // host user is actively typing/working
    bool connected        = false;   // receiving serial packets from host

    // Music (optional sub-object in stats packet)
    char music_title[48]  = "";
    char music_artist[32] = "";
    bool music_playing    = false;
    bool music_active     = false;   // music sub-object was present in packet

    // Status bar extras
    char idle_msg[48]     = "";      // shown under title when nothing is playing
    char ip_str[16]       = "";

    // Claude usage (optional sub-object in stats packet)
    int     claude_sessions = 0;     // distinct sessions with token activity *today* (JSONL scan)
    int     claude_working  = 0;     // sessions *currently* mid-turn, live (hook-based, unrelated to claude_sessions)
    int  claude_h5_pct  = -1;          // 5-hour rate limit % (-1 = unavailable)
    int  claude_h5_secs = -1;          // seconds until 5h window resets
    int  claude_w7_pct  = -1;          // 7-day rate limit %
    int  claude_w7_secs = -1;

    // Calendar (optional "cal" array in stats packet) — today's timed events,
    // sorted by start. Kept through disconnects; the offline clock keeps the
    // countdowns moving.
    bool     cal_available = false;  // companion has sent a calendar at least once
    int      cal_count     = 0;
    CalEvent cal_events[CAL_MAX_EVENTS];
};

extern DashState state;  // defined in main.cpp

// Current local time in minutes since midnight, advanced from the last host
// time by millis() so it keeps ticking offline. -1 until the host sends a time.
inline int dash_now_min() {
    if (state.time_h < 0) return -1;
    uint32_t elapsed_m = (millis() - state.time_set_ms) / 60000UL;
    return (state.time_h * 60 + state.time_m + (int)elapsed_m) % (24 * 60);
}
