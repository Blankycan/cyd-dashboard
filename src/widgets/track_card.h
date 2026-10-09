#pragma once
#include <lvgl.h>

// "Now playing" card — slides in over the scene slot for TRACK_CARD_MS when
// the companion announces a new song, with its cover art, title, artist and
// album. Switched on/off in the settings menu (settings.track_card).
//
// The companion sends a "track" line, then the cover in "art" chunks (see
// companion/track_card.py). The card waits for the cover up to
// TRACK_ART_WAIT_MS, then shows with whatever arrived (a placeholder if none);
// a cover that completes later still pops in.
//
// It stays away while the settings menu is open, while someone is playing a
// scene by touch, and while the display sleeps (main.cpp doesn't announce then).

void build_track_card(lv_obj_t *parent, int x, int y, int w, int h);

void track_card_begin(int id, const char *title, const char *artist, const char *album,
                      int art_w, int art_h);
void track_card_art(int id, int offset, const char *b64);

// A press at screen (x, y): true if a showing card took it (and closed)
bool track_card_take_press(int x, int y);
void track_card_hide();   // at once, no animation (e.g. going to sleep)
