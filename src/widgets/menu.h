#pragma once
#include <lvgl.h>

// Settings menu — opened with the gear at the right end of the topbar. It
// covers the panels below the scene slot (music down to status) and leaves the
// slot itself visible, so tapping a scene's name previews it right there, at
// its real size. Changes apply straight away; they're saved to flash when the
// menu closes (the gear again, MENU_IDLE_CLOSE_MS untouched, or sleep).
//
// Built when opened and deleted when closed, so it holds no memory the scenes
// could use while it isn't showing.

void build_menu(lv_obj_t *scr);   // once, after the topbar and panels exist
void menu_close();
bool menu_is_open();

// Whether a press at this screen point belongs to the menu (or its button)
// rather than the scene player's tap-to-flip. Safe from the touch callback.
bool menu_owns_point(int x, int y);
