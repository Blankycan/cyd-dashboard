#pragma once
#include <lvgl.h>

// Top bar — clock (left), date, and the settings menu button (right).

#define TOPBAR_MENU_BTN_W  36   // touch target of the menu button at the right end

void build_topbar(lv_obj_t *scr);
void update_topbar_ui();
lv_obj_t *topbar_menu_button();          // menu.cpp hooks its click handler onto this
void topbar_set_menu_open(bool open);    // gear ↔ close icon
