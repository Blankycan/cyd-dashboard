#pragma once
#include <stdint.h>

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
    int  wpm              = 0;
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
    int32_t claude_out      = 0;     // output tokens today
    int32_t claude_inp      = 0;     // input tokens today
    int     claude_sessions = 0;     // distinct sessions with token activity *today* (JSONL scan)
    int     claude_working  = 0;     // sessions *currently* mid-turn, live (hook-based, unrelated to claude_sessions)
    int  claude_h5_pct  = -1;          // 5-hour rate limit % (-1 = unavailable)
    int  claude_h5_secs = -1;          // seconds until 5h window resets
    int  claude_w7_pct  = -1;          // 7-day rate limit %
    int  claude_w7_secs = -1;
};

extern DashState state;  // defined in main.cpp
