#pragma once
// =============================================================================
// CYD Dashboard — color theme selector
//
// Pick the active theme via CYD_THEME in config.h. Each theme is a standalone
// palette file under src/themes/ — see src/themes/forest.h for the full set
// of PAL_*/COL_*/TFT_COL_* names a theme must define.
// =============================================================================

#include "config.h"

#if CYD_THEME == CYD_THEME_GRAPE_EMBER
    #include "themes/grape-ember.h"
#elif CYD_THEME == CYD_THEME_NEON_ROSE
    #include "themes/neon-rose.h"
#elif CYD_THEME == CYD_THEME_RAINBOW
    #include "themes/rainbow.h"
#elif CYD_THEME == CYD_THEME_COZYFALL
    #include "themes/cozyfall.h"
#else
    #include "themes/forest.h"
#endif

// =============================================================================
// Per-widget colour fallbacks — define if the selected theme doesn't set them.
// Themes that need different values override these by defining the token before
// this file is included (i.e., inside the theme header itself).
// =============================================================================

// --- Generic dot colours ---
#ifndef COL_DOT_ACTIVE
#define COL_DOT_ACTIVE COL_OK
#endif
#ifndef COL_DOT_IDLE
#define COL_DOT_IDLE COL_TEXT_DIM
#endif
#ifndef COL_DOT_WARN
#define COL_DOT_WARN COL_WARN
#endif
#ifndef COL_DOT_ALERT
#define COL_DOT_ALERT COL_ALERT
#endif

// --- Generic bar colours ---
#ifndef COL_BAR_BG
#define COL_BAR_BG COL_BAR_TRACK
#endif
#ifndef COL_BAR_FILL
#define COL_BAR_FILL COL_OK
#endif
#ifndef COL_BAR_WARN
#define COL_BAR_WARN COL_WARN
#endif
#ifndef COL_BAR_ALERT
#define COL_BAR_ALERT COL_ALERT
#endif
#ifndef COL_BAR_INACTIVE
#define COL_BAR_INACTIVE COL_TEXT_DIM
#endif
#ifndef COL_BAR_LABEL
#define COL_BAR_LABEL COL_TEXT_DIM
#endif
#ifndef COL_BAR_VALUE
#define COL_BAR_VALUE COL_BAR_FILL
#endif
#ifndef COL_BAR_EXTRA
#define COL_BAR_EXTRA COL_TEXT_DIM
#endif
#ifndef COL_TITLE
#define COL_TITLE COL_TEXT_PRI
#endif
#ifndef COL_DIVIDER_INNER
#define COL_DIVIDER_INNER COL_DIVIDER
#endif

// --- Topbar ---
#ifndef COL_TOPBAR_BG
#define COL_TOPBAR_BG COL_BG
#endif
#ifndef COL_TOPBAR_CLOCK
#define COL_TOPBAR_CLOCK COL_TEXT_PRI
#endif
#ifndef COL_TOPBAR_DATE
#define COL_TOPBAR_DATE COL_TEXT_DIM
#endif
#ifndef COL_TOPBAR_MENU
#define COL_TOPBAR_MENU COL_TOPBAR_DATE
#endif

// --- Settings menu (covers the panels below the scene slot) ---
#ifndef COL_MENU_BG
#define COL_MENU_BG COL_BG
#endif
#ifndef COL_MENU_TEXT
#define COL_MENU_TEXT COL_TEXT_PRI
#endif
#ifndef COL_MENU_HINT
#define COL_MENU_HINT COL_TEXT_DIM
#endif
#ifndef COL_MENU_HEADING
#define COL_MENU_HEADING COL_TEXT_SEC
#endif
#ifndef COL_MENU_ACCENT
#define COL_MENU_ACCENT COL_OK
#endif
#ifndef COL_MENU_TRACK
#define COL_MENU_TRACK COL_BAR_TRACK
#endif
#ifndef COL_MENU_KNOB
#define COL_MENU_KNOB COL_TEXT_PRI
#endif
#ifndef COL_MENU_ROW_SEL
#define COL_MENU_ROW_SEL COL_PANEL
#endif
#ifndef COL_MENU_DIVIDER
#define COL_MENU_DIVIDER COL_DIVIDER
#endif

// --- Calendar ---
#ifndef COL_CALENDAR_BG
#define COL_CALENDAR_BG COL_BG
#endif
#ifndef COL_CALENDAR_TIME
#define COL_CALENDAR_TIME COL_TEXT_SEC
#endif
#ifndef COL_CALENDAR_TITLE
#define COL_CALENDAR_TITLE COL_TITLE
#endif
#ifndef COL_CALENDAR_NOW
#define COL_CALENDAR_NOW COL_DOT_ACTIVE
#endif
#ifndef COL_CALENDAR_COUNTDOWN
#define COL_CALENDAR_COUNTDOWN COL_TEXT_SEC
#endif
#ifndef COL_CALENDAR_COUNTDOWN_SOON
#define COL_CALENDAR_COUNTDOWN_SOON COL_WARN
#endif
#ifndef COL_CALENDAR_NEXT
#define COL_CALENDAR_NEXT COL_TEXT_DIM
#endif
#ifndef COL_CALENDAR_DONE
#define COL_CALENDAR_DONE COL_TEXT_DIM
#endif
#ifndef COL_CALENDAR_PROGRESS_BG
#define COL_CALENDAR_PROGRESS_BG COL_BAR_BG
#endif
#ifndef COL_CALENDAR_PROGRESS_FILL
#define COL_CALENDAR_PROGRESS_FILL COL_CALENDAR_NOW
#endif
#ifndef COL_CALENDAR_TRACK
#define COL_CALENDAR_TRACK COL_BAR_BG
#endif
#ifndef COL_CALENDAR_EVENT
#define COL_CALENDAR_EVENT COL_TEXT_SEC
#endif
#ifndef COL_CALENDAR_EVENT_PAST
#define COL_CALENDAR_EVENT_PAST COL_TEXT_DIM
#endif
#ifndef COL_CALENDAR_EVENT_NOW
#define COL_CALENDAR_EVENT_NOW COL_CALENDAR_NOW
#endif
#ifndef COL_CALENDAR_NOW_MARK
#define COL_CALENDAR_NOW_MARK COL_TEXT_PRI
#endif
#ifndef COL_CALENDAR_HOUR
#define COL_CALENDAR_HOUR COL_TEXT_DIM
#endif

// --- Ambient scenes --- (scene_player.cpp, src/scenes/*.cpp)
#ifndef COL_SCENE_BG
#define COL_SCENE_BG COL_CALENDAR_BG
#endif
#ifndef COL_SCENE_LEAVES_1
#define COL_SCENE_LEAVES_1 COL_WARN
#endif
#ifndef COL_SCENE_LEAVES_2
#define COL_SCENE_LEAVES_2 COL_ALERT
#endif
#ifndef COL_SCENE_LEAVES_3
#define COL_SCENE_LEAVES_3 COL_GLOW
#endif
#ifndef COL_SCENE_SNOW_NEAR
#define COL_SCENE_SNOW_NEAR COL_TEXT_PRI
#endif
#ifndef COL_SCENE_SNOW_FAR
#define COL_SCENE_SNOW_FAR COL_TEXT_DIM
#endif
#ifndef COL_SCENE_LIFE_CELL
#define COL_SCENE_LIFE_CELL COL_OK
#endif
#ifndef COL_SCENE_LIFE_BG
#define COL_SCENE_LIFE_BG COL_SCENE_BG
#endif
#ifndef COL_SCENE_STARS_NEAR
#define COL_SCENE_STARS_NEAR COL_TEXT_PRI
#endif
#ifndef COL_SCENE_STARS_MID
#define COL_SCENE_STARS_MID COL_TEXT_SEC
#endif
#ifndef COL_SCENE_STARS_FAR
#define COL_SCENE_STARS_FAR COL_TEXT_DIM
#endif
#ifndef COL_SCENE_FISH_1
#define COL_SCENE_FISH_1 COL_WARN
#endif
#ifndef COL_SCENE_FISH_2
#define COL_SCENE_FISH_2 COL_GLOW
#endif
#ifndef COL_SCENE_FISH_3
#define COL_SCENE_FISH_3 COL_ALERT
#endif
#ifndef COL_SCENE_FISH_EYE
#define COL_SCENE_FISH_EYE COL_BG
#endif
#ifndef COL_SCENE_FISH_WEED
#define COL_SCENE_FISH_WEED COL_OK
#endif
#ifndef COL_SCENE_FISH_BUBBLE
#define COL_SCENE_FISH_BUBBLE COL_TEXT_DIM
#endif
#ifndef COL_SCENE_FISH_FOOD
#define COL_SCENE_FISH_FOOD COL_TEXT_SEC
#endif
// Arcade scenes (invaders, asteroids, pacman)
#ifndef COL_SCENE_ARCADE_BG
#define COL_SCENE_ARCADE_BG COL_SCENE_BG
#endif
#ifndef COL_SCENE_INV_ALIEN_1
#define COL_SCENE_INV_ALIEN_1 COL_ALERT
#endif
#ifndef COL_SCENE_INV_ALIEN_2
#define COL_SCENE_INV_ALIEN_2 COL_GLOW
#endif
#ifndef COL_SCENE_INV_ALIEN_3
#define COL_SCENE_INV_ALIEN_3 COL_OK
#endif
#ifndef COL_SCENE_INV_CANNON
#define COL_SCENE_INV_CANNON COL_TEXT_PRI
#endif
#ifndef COL_SCENE_INV_SHOT
#define COL_SCENE_INV_SHOT COL_TEXT_PRI
#endif
#ifndef COL_SCENE_INV_BOMB
#define COL_SCENE_INV_BOMB COL_WARN
#endif
#ifndef COL_SCENE_INV_BOOM
#define COL_SCENE_INV_BOOM COL_WARN
#endif
#ifndef COL_SCENE_AST_ROCK
#define COL_SCENE_AST_ROCK COL_TEXT_SEC
#endif
#ifndef COL_SCENE_AST_SHIP
#define COL_SCENE_AST_SHIP COL_TEXT_PRI
#endif
#ifndef COL_SCENE_AST_SHOT
#define COL_SCENE_AST_SHOT COL_GLOW
#endif
#ifndef COL_SCENE_AST_THRUST
#define COL_SCENE_AST_THRUST COL_WARN
#endif
#ifndef COL_SCENE_AST_DEBRIS
#define COL_SCENE_AST_DEBRIS COL_TEXT_DIM
#endif
#ifndef COL_SCENE_PAC_WALL
#define COL_SCENE_PAC_WALL COL_TEXT_DIM
#endif
#ifndef COL_SCENE_PAC_FLASH
#define COL_SCENE_PAC_FLASH COL_TEXT_PRI
#endif
#ifndef COL_SCENE_PAC_DOT
#define COL_SCENE_PAC_DOT COL_TEXT_SEC
#endif
#ifndef COL_SCENE_PAC_PACMAN
#define COL_SCENE_PAC_PACMAN COL_WARN
#endif
#ifndef COL_SCENE_PAC_GHOST_1
#define COL_SCENE_PAC_GHOST_1 COL_ALERT
#endif
#ifndef COL_SCENE_PAC_GHOST_2
#define COL_SCENE_PAC_GHOST_2 COL_GLOW
#endif
#ifndef COL_SCENE_PAC_GHOST_3
#define COL_SCENE_PAC_GHOST_3 COL_OK
#endif
#ifndef COL_SCENE_PAC_GHOST_4
#define COL_SCENE_PAC_GHOST_4 COL_TEXT_PRI
#endif
#ifndef COL_SCENE_PAC_FRIGHT
#define COL_SCENE_PAC_FRIGHT COL_TEXT_SEC
#endif
#ifndef COL_SCENE_PAC_EYES
#define COL_SCENE_PAC_EYES COL_SCENE_ARCADE_BG
#endif
#ifndef COL_SCENE_PAC_EATEN
#define COL_SCENE_PAC_EATEN COL_TEXT_PRI
#endif
// Synthwave
#ifndef COL_SCENE_SYNTH_SKY_TOP
#define COL_SCENE_SYNTH_SKY_TOP COL_BG
#endif
#ifndef COL_SCENE_SYNTH_SKY_LOW
#define COL_SCENE_SYNTH_SKY_LOW lv_color_mix(COL_ALERT, COL_BG, 110)
#endif
#ifndef COL_SCENE_SYNTH_STAR
#define COL_SCENE_SYNTH_STAR COL_TEXT_SEC
#endif
#ifndef COL_SCENE_SYNTH_SUN_TOP
#define COL_SCENE_SYNTH_SUN_TOP COL_WARN
#endif
#ifndef COL_SCENE_SYNTH_SUN_LOW
#define COL_SCENE_SYNTH_SUN_LOW COL_ALERT
#endif
#ifndef COL_SCENE_SYNTH_MOUNTAIN_FAR
#define COL_SCENE_SYNTH_MOUNTAIN_FAR lv_color_mix(COL_GLOW, COL_BG, 70)
#endif
#ifndef COL_SCENE_SYNTH_MOUNTAIN
#define COL_SCENE_SYNTH_MOUNTAIN lv_color_mix(COL_GLOW, COL_BG, 30)
#endif
#ifndef COL_SCENE_SYNTH_GROUND_TOP
#define COL_SCENE_SYNTH_GROUND_TOP lv_color_mix(COL_GLOW, COL_BG, 35)
#endif
#ifndef COL_SCENE_SYNTH_GROUND_LOW
#define COL_SCENE_SYNTH_GROUND_LOW COL_BG
#endif
#ifndef COL_SCENE_SYNTH_ROAD
#define COL_SCENE_SYNTH_ROAD COL_BG
#endif
#ifndef COL_SCENE_SYNTH_GRID
#define COL_SCENE_SYNTH_GRID COL_GLOW
#endif
#ifndef COL_SCENE_SYNTH_GRID_BEAT
#define COL_SCENE_SYNTH_GRID_BEAT COL_TEXT_PRI
#endif
#ifndef COL_SCENE_SYNTH_LANE
#define COL_SCENE_SYNTH_LANE COL_TEXT_SEC
#endif
#ifndef COL_SCENE_SYNTH_CAR
#define COL_SCENE_SYNTH_CAR COL_PANEL
#endif
#ifndef COL_SCENE_SYNTH_TAILLIGHT
#define COL_SCENE_SYNTH_TAILLIGHT COL_ALERT
#endif
// Lofi girl
#ifndef COL_SCENE_LOFI_ROOM
#define COL_SCENE_LOFI_ROOM lv_color_mix(COL_PANEL, COL_BG, 120)
#endif
#ifndef COL_SCENE_LOFI_FRAME
#define COL_SCENE_LOFI_FRAME COL_PANEL
#endif
#ifndef COL_SCENE_LOFI_SKY_TOP
#define COL_SCENE_LOFI_SKY_TOP lv_color_mix(COL_GLOW, COL_BG, 40)
#endif
#ifndef COL_SCENE_LOFI_SKY_LOW
#define COL_SCENE_LOFI_SKY_LOW lv_color_mix(COL_ALERT, COL_BG, 100)
#endif
#ifndef COL_SCENE_LOFI_MOON
#define COL_SCENE_LOFI_MOON COL_TEXT_PRI
#endif
#ifndef COL_SCENE_LOFI_CITY
#define COL_SCENE_LOFI_CITY COL_BG
#endif
#ifndef COL_SCENE_LOFI_CITY_LIGHT
#define COL_SCENE_LOFI_CITY_LIGHT COL_WARN
#endif
#ifndef COL_SCENE_LOFI_RAIN
#define COL_SCENE_LOFI_RAIN COL_TEXT_DIM
#endif
#ifndef COL_SCENE_LOFI_DESK
#define COL_SCENE_LOFI_DESK COL_PANEL
#endif
#ifndef COL_SCENE_LOFI_FIGURE
#define COL_SCENE_LOFI_FIGURE COL_BG
#endif
#ifndef COL_SCENE_LOFI_HEADPHONES
#define COL_SCENE_LOFI_HEADPHONES lv_color_mix(lv_color_hex(0xC0342E), COL_BG, 205)
#endif
// Lofi girl character: natural colours, pulled ~20% toward the theme background
// so she sits in the scene's lighting
#ifndef COL_SCENE_LOFI_HAIR
#define COL_SCENE_LOFI_HAIR lv_color_mix(lv_color_hex(0x5B3319), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_HAIR_SHADE
#define COL_SCENE_LOFI_HAIR_SHADE lv_color_mix(lv_color_hex(0x3A1F10), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_HAIR_TIE
#define COL_SCENE_LOFI_HAIR_TIE lv_color_mix(lv_color_hex(0x2F6E4F), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_SKIN
#define COL_SCENE_LOFI_SKIN lv_color_mix(lv_color_hex(0xF2C49B), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_SKIN_SHADE
#define COL_SCENE_LOFI_SKIN_SHADE lv_color_mix(lv_color_hex(0xD29A74), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_EYE
#define COL_SCENE_LOFI_EYE lv_color_mix(lv_color_hex(0x2A1A14), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_PHONES_BAND
#define COL_SCENE_LOFI_PHONES_BAND lv_color_mix(lv_color_hex(0xEDE3C8), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_PHONES_CUSHION
#define COL_SCENE_LOFI_PHONES_CUSHION lv_color_mix(lv_color_hex(0x2A2A30), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_SWEATER
#define COL_SCENE_LOFI_SWEATER lv_color_mix(lv_color_hex(0x1F5E4A), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_SWEATER_SHADE
#define COL_SCENE_LOFI_SWEATER_SHADE lv_color_mix(lv_color_hex(0x154436), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_SCARF
#define COL_SCENE_LOFI_SCARF lv_color_mix(lv_color_hex(0xE0483A), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_SCARF_SHADE
#define COL_SCENE_LOFI_SCARF_SHADE lv_color_mix(lv_color_hex(0xB53428), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_HAIR_SHINE
#define COL_SCENE_LOFI_HAIR_SHINE lv_color_mix(lv_color_hex(0x8A5530), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_MOUTH
#define COL_SCENE_LOFI_MOUTH lv_color_mix(lv_color_hex(0xB0584A), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_BLUSH
#define COL_SCENE_LOFI_BLUSH lv_color_mix(lv_color_hex(0xF0A08A), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_IRIS
#define COL_SCENE_LOFI_IRIS lv_color_mix(lv_color_hex(0x3E6B3A), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_OUTLINE
#define COL_SCENE_LOFI_OUTLINE lv_color_mix(lv_color_hex(0x140C08), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_PEN
#define COL_SCENE_LOFI_PEN lv_color_mix(lv_color_hex(0x3A3A44), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_LAMP
#define COL_SCENE_LOFI_LAMP COL_BG
#endif
#ifndef COL_SCENE_LOFI_LAMP_GLOW
#define COL_SCENE_LOFI_LAMP_GLOW COL_WARN
#endif
#ifndef COL_SCENE_LOFI_MUG
#define COL_SCENE_LOFI_MUG lv_color_mix(lv_color_hex(0x8A5E44), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_MUG_SHADE
#define COL_SCENE_LOFI_MUG_SHADE lv_color_mix(lv_color_hex(0x5A3C2C), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_MUG_RIM
#define COL_SCENE_LOFI_MUG_RIM lv_color_mix(lv_color_hex(0xC8A888), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_MUG_SHINE
#define COL_SCENE_LOFI_MUG_SHINE lv_color_mix(lv_color_hex(0xF4DDB0), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_SHADE_RIM
#define COL_SCENE_LOFI_SHADE_RIM lv_color_mix(lv_color_hex(0xF5C07A), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_COFFEE
#define COL_SCENE_LOFI_COFFEE lv_color_mix(lv_color_hex(0x3A2014), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_STEAM
#define COL_SCENE_LOFI_STEAM COL_TEXT_DIM
#endif
#ifndef COL_SCENE_LOFI_PAPER
#define COL_SCENE_LOFI_PAPER lv_color_mix(lv_color_hex(0xE8DDC0), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_PAPER_SHADE
#define COL_SCENE_LOFI_PAPER_SHADE lv_color_mix(lv_color_hex(0xC4B89C), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_PAGE_EDGE
#define COL_SCENE_LOFI_PAGE_EDGE lv_color_mix(lv_color_hex(0x968870), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_BOOK_COVER
#define COL_SCENE_LOFI_BOOK_COVER lv_color_mix(lv_color_hex(0x7A3A2E), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_INK
#define COL_SCENE_LOFI_INK lv_color_mix(lv_color_hex(0x2A3050), COL_BG, 205)
#endif
#ifndef COL_SCENE_LOFI_NOTE
#define COL_SCENE_LOFI_NOTE COL_GLOW
#endif
// Fireworks
#ifndef COL_SCENE_FW_SKY_TOP
#define COL_SCENE_FW_SKY_TOP COL_BG
#endif
#ifndef COL_SCENE_FW_SKY_LOW
#define COL_SCENE_FW_SKY_LOW lv_color_mix(COL_GLOW, COL_BG, 45)
#endif
#ifndef COL_SCENE_FW_STAR
#define COL_SCENE_FW_STAR COL_TEXT_DIM
#endif
#ifndef COL_SCENE_FW_CITY
#define COL_SCENE_FW_CITY lv_color_mix(COL_PANEL, COL_BG, 80)
#endif
#ifndef COL_SCENE_FW_CITY_LIGHT
#define COL_SCENE_FW_CITY_LIGHT COL_WARN
#endif
#ifndef COL_SCENE_FW_ROCKET
#define COL_SCENE_FW_ROCKET COL_TEXT_PRI
#endif
#ifndef COL_SCENE_FW_1
#define COL_SCENE_FW_1 COL_ALERT
#endif
#ifndef COL_SCENE_FW_2
#define COL_SCENE_FW_2 COL_GLOW
#endif
#ifndef COL_SCENE_FW_3
#define COL_SCENE_FW_3 COL_OK
#endif
#ifndef COL_SCENE_FW_4
#define COL_SCENE_FW_4 COL_TEXT_PRI
#endif
#ifndef COL_SCENE_FW_GOLD
#define COL_SCENE_FW_GOLD COL_WARN
#endif
// Campfire
#ifndef COL_SCENE_FIRE_SKY_TOP
#define COL_SCENE_FIRE_SKY_TOP COL_BG
#endif
#ifndef COL_SCENE_FIRE_SKY_LOW
#define COL_SCENE_FIRE_SKY_LOW lv_color_mix(COL_PANEL, COL_BG, 140)
#endif
#ifndef COL_SCENE_FIRE_STAR
#define COL_SCENE_FIRE_STAR COL_TEXT_SEC
#endif
#ifndef COL_SCENE_FIRE_TREES
#define COL_SCENE_FIRE_TREES lv_color_mix(COL_PANEL, COL_BG, 60)
#endif
#ifndef COL_SCENE_FIRE_GROUND
#define COL_SCENE_FIRE_GROUND lv_color_mix(COL_PANEL, COL_BG, 90)
#endif
#ifndef COL_SCENE_FIRE_GLOW
#define COL_SCENE_FIRE_GLOW COL_WARN
#endif
#ifndef COL_SCENE_FIRE_LOGS
#define COL_SCENE_FIRE_LOGS COL_BG
#endif
#ifndef COL_SCENE_FIRE_STONES
#define COL_SCENE_FIRE_STONES COL_TEXT_DIM
#endif
#ifndef COL_SCENE_FIRE_CORE
#define COL_SCENE_FIRE_CORE COL_TEXT_PRI
#endif
#ifndef COL_SCENE_FIRE_MID
#define COL_SCENE_FIRE_MID COL_WARN
#endif
#ifndef COL_SCENE_FIRE_OUTER
#define COL_SCENE_FIRE_OUTER COL_ALERT
#endif
#ifndef COL_SCENE_FIRE_SPARK
#define COL_SCENE_FIRE_SPARK COL_WARN
#endif
// Night city
#ifndef COL_SCENE_CITY_DAY_TOP
#define COL_SCENE_CITY_DAY_TOP lv_color_mix(lv_color_hex(0x5E9FD8), COL_BG, 205)
#endif
#ifndef COL_SCENE_CITY_DAY_LOW
#define COL_SCENE_CITY_DAY_LOW lv_color_mix(lv_color_hex(0xB8D8EE), COL_BG, 205)
#endif
#ifndef COL_SCENE_CITY_DUSK_TOP
#define COL_SCENE_CITY_DUSK_TOP lv_color_mix(COL_GLOW, COL_BG, 90)
#endif
#ifndef COL_SCENE_CITY_DUSK_LOW
#define COL_SCENE_CITY_DUSK_LOW lv_color_mix(COL_WARN, COL_GLOW, 128)
#endif
#ifndef COL_SCENE_CITY_NIGHT_TOP
#define COL_SCENE_CITY_NIGHT_TOP lv_color_mix(COL_PANEL, COL_BG, 90)
#endif
#ifndef COL_SCENE_CITY_NIGHT_LOW
#define COL_SCENE_CITY_NIGHT_LOW lv_color_mix(COL_GLOW, COL_PANEL, 60)
#endif
#ifndef COL_SCENE_CITY_STAR
#define COL_SCENE_CITY_STAR COL_TEXT_SEC
#endif
#ifndef COL_SCENE_CITY_SUN
#define COL_SCENE_CITY_SUN COL_WARN
#endif
#ifndef COL_SCENE_CITY_MOON
#define COL_SCENE_CITY_MOON COL_TEXT_PRI
#endif
#ifndef COL_SCENE_CITY_FAR
#define COL_SCENE_CITY_FAR lv_color_mix(COL_PANEL, COL_BG, 200)
#endif
#ifndef COL_SCENE_CITY_MID
#define COL_SCENE_CITY_MID lv_color_mix(COL_PANEL, COL_BG, 110)
#endif
#ifndef COL_SCENE_CITY_NEAR
#define COL_SCENE_CITY_NEAR COL_BG
#endif
#ifndef COL_SCENE_CITY_LIGHT
#define COL_SCENE_CITY_LIGHT COL_WARN
#endif
#ifndef COL_SCENE_CITY_TRACK
#define COL_SCENE_CITY_TRACK COL_BG
#endif
#ifndef COL_SCENE_CITY_TRAIN
#define COL_SCENE_CITY_TRAIN COL_PANEL
#endif
#ifndef COL_SCENE_CITY_PLANE
#define COL_SCENE_CITY_PLANE COL_TEXT_DIM
#endif
#ifndef COL_SCENE_CITY_BLINK
#define COL_SCENE_CITY_BLINK COL_ALERT
#endif
#ifndef COL_SCENE_CITY_BIRD
#define COL_SCENE_CITY_BIRD lv_color_mix(COL_BG, COL_PANEL, 200)
#endif
// Lava lamp
#ifndef COL_SCENE_LAVA_BG_TOP
#define COL_SCENE_LAVA_BG_TOP COL_BG
#endif
#ifndef COL_SCENE_LAVA_BG_LOW
#define COL_SCENE_LAVA_BG_LOW lv_color_mix(COL_PANEL, COL_BG, 180)
#endif
#ifndef COL_SCENE_LAVA_WAX
#define COL_SCENE_LAVA_WAX COL_GLOW
#endif
#ifndef COL_SCENE_LAVA_RIM
#define COL_SCENE_LAVA_RIM COL_WARN
#endif
// Dino runner
#ifndef COL_SCENE_DINO_INK
#define COL_SCENE_DINO_INK COL_TEXT_SEC
#endif
#ifndef COL_SCENE_DINO_CLOUD
#define COL_SCENE_DINO_CLOUD COL_TEXT_DIM
#endif
#ifndef COL_SCENE_DINO_STAR
#define COL_SCENE_DINO_STAR COL_TEXT_DIM
#endif
#ifndef COL_SCENE_DINO_MOON
#define COL_SCENE_DINO_MOON COL_TEXT_PRI
#endif
// Flappy Bird (its own daylight colours, toned toward the theme's background)
#ifndef COL_SCENE_FLAPPY_SKY_TOP
#define COL_SCENE_FLAPPY_SKY_TOP lv_color_mix(lv_color_hex(0x4EC0CA), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_SKY_LOW
#define COL_SCENE_FLAPPY_SKY_LOW lv_color_mix(lv_color_hex(0xA8E4E0), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_CLOUD
#define COL_SCENE_FLAPPY_CLOUD lv_color_mix(lv_color_hex(0xE4F6EE), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_CITY
#define COL_SCENE_FLAPPY_CITY lv_color_mix(lv_color_hex(0x9CD6B4), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_BUSH
#define COL_SCENE_FLAPPY_BUSH lv_color_mix(lv_color_hex(0x5EC85A), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_PIPE
#define COL_SCENE_FLAPPY_PIPE lv_color_mix(lv_color_hex(0x74BF2E), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_PIPE_LIGHT
#define COL_SCENE_FLAPPY_PIPE_LIGHT lv_color_mix(lv_color_hex(0xA8E860), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_PIPE_DARK
#define COL_SCENE_FLAPPY_PIPE_DARK lv_color_mix(lv_color_hex(0x4F8A20), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_OUTLINE
#define COL_SCENE_FLAPPY_OUTLINE lv_color_mix(lv_color_hex(0x3A2A30), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_GRASS
#define COL_SCENE_FLAPPY_GRASS lv_color_mix(lv_color_hex(0x8FD85A), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_GRASS_DARK
#define COL_SCENE_FLAPPY_GRASS_DARK lv_color_mix(lv_color_hex(0x62B03A), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_GROUND
#define COL_SCENE_FLAPPY_GROUND lv_color_mix(lv_color_hex(0xDED895), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_BIRD
#define COL_SCENE_FLAPPY_BIRD lv_color_mix(lv_color_hex(0xF8C838), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_EYE
#define COL_SCENE_FLAPPY_EYE lv_color_mix(lv_color_hex(0xFFFFFF), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_BEAK
#define COL_SCENE_FLAPPY_BEAK lv_color_mix(lv_color_hex(0xF06A30), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_WING
#define COL_SCENE_FLAPPY_WING lv_color_mix(lv_color_hex(0xFCF0C8), COL_BG, 215)
#endif
#ifndef COL_SCENE_FLAPPY_SCORE
#define COL_SCENE_FLAPPY_SCORE lv_color_mix(lv_color_hex(0xFFFFFF), COL_BG, 235)
#endif
// Breakout
#ifndef COL_SCENE_BRK_ROW_1
#define COL_SCENE_BRK_ROW_1 COL_ALERT
#endif
#ifndef COL_SCENE_BRK_ROW_2
#define COL_SCENE_BRK_ROW_2 COL_GLOW
#endif
#ifndef COL_SCENE_BRK_ROW_3
#define COL_SCENE_BRK_ROW_3 COL_WARN
#endif
#ifndef COL_SCENE_BRK_ROW_4
#define COL_SCENE_BRK_ROW_4 COL_OK
#endif
#ifndef COL_SCENE_BRK_ROW_5
#define COL_SCENE_BRK_ROW_5 COL_TEXT_SEC
#endif
#ifndef COL_SCENE_BRK_ROW_6
#define COL_SCENE_BRK_ROW_6 COL_TEXT_DIM
#endif
#ifndef COL_SCENE_BRK_SHINE
#define COL_SCENE_BRK_SHINE COL_TEXT_PRI
#endif
#ifndef COL_SCENE_BRK_PADDLE
#define COL_SCENE_BRK_PADDLE COL_TEXT_SEC
#endif
#ifndef COL_SCENE_BRK_BALL
#define COL_SCENE_BRK_BALL COL_TEXT_PRI
#endif
// Missile Command
#ifndef COL_SCENE_MC_GROUND
#define COL_SCENE_MC_GROUND lv_color_mix(COL_TEXT_DIM, COL_BG, 150)
#endif
#ifndef COL_SCENE_MC_CITY
#define COL_SCENE_MC_CITY COL_TEXT_DIM
#endif
#ifndef COL_SCENE_MC_WINDOW
#define COL_SCENE_MC_WINDOW COL_WARN
#endif
#ifndef COL_SCENE_MC_BASE
#define COL_SCENE_MC_BASE COL_TEXT_PRI
#endif
#ifndef COL_SCENE_MC_ENEMY
#define COL_SCENE_MC_ENEMY COL_ALERT
#endif
#ifndef COL_SCENE_MC_HEAD
#define COL_SCENE_MC_HEAD COL_TEXT_PRI
#endif
#ifndef COL_SCENE_MC_SHOT
#define COL_SCENE_MC_SHOT COL_TEXT_PRI
#endif
#ifndef COL_SCENE_MC_FLASH
#define COL_SCENE_MC_FLASH COL_TEXT_PRI
#endif
#ifndef COL_SCENE_MC_BLAST_1
#define COL_SCENE_MC_BLAST_1 COL_WARN
#endif
#ifndef COL_SCENE_MC_BLAST_2
#define COL_SCENE_MC_BLAST_2 COL_GLOW
#endif
#ifndef COL_SCENE_MC_BLAST_3
#define COL_SCENE_MC_BLAST_3 COL_TEXT_PRI
#endif
#ifndef COL_SCENE_QUOTE_TEXT
#define COL_SCENE_QUOTE_TEXT COL_TEXT_PRI
#endif
#ifndef COL_SCENE_QUOTE_AUTHOR
#define COL_SCENE_QUOTE_AUTHOR COL_TEXT_DIM
#endif

// --- Music ---
#ifndef COL_MUSIC_BG
#define COL_MUSIC_BG COL_BG
#endif
#ifndef COL_MUSIC_TITLE
#define COL_MUSIC_TITLE COL_TITLE
#endif
#ifndef COL_MUSIC_TEXT
#define COL_MUSIC_TEXT COL_TEXT_SEC
#endif
#ifndef COL_MUSIC_ICON_PLAY
#define COL_MUSIC_ICON_PLAY COL_TEXT_PRI
#endif
#ifndef COL_MUSIC_ICON_PAUSE
#define COL_MUSIC_ICON_PAUSE COL_TEXT_SEC
#endif
#ifndef COL_MUSIC_ICON_IDLE
#define COL_MUSIC_ICON_IDLE COL_TEXT_DIM
#endif
#ifndef COL_MUSIC_DOT_PLAYING
#define COL_MUSIC_DOT_PLAYING COL_DOT_ACTIVE
#endif
#ifndef COL_MUSIC_DOT_IDLE
#define COL_MUSIC_DOT_IDLE COL_DOT_IDLE
#endif

// --- System ---
#ifndef COL_SYSTEM_BG
#define COL_SYSTEM_BG COL_BG
#endif
#ifndef COL_SYSTEM_LABEL
#define COL_SYSTEM_LABEL COL_BAR_LABEL
#endif
#ifndef COL_SYSTEM_BAR_BG
#define COL_SYSTEM_BAR_BG COL_BAR_BG
#endif
#ifndef COL_SYSTEM_BAR_FILL
#define COL_SYSTEM_BAR_FILL COL_BAR_FILL
#endif
#ifndef COL_SYSTEM_BAR_WARN
#define COL_SYSTEM_BAR_WARN COL_BAR_WARN
#endif
#ifndef COL_SYSTEM_BAR_ALERT
#define COL_SYSTEM_BAR_ALERT COL_BAR_ALERT
#endif
#ifndef COL_SYSTEM_CPU_LABEL
#define COL_SYSTEM_CPU_LABEL COL_SYSTEM_LABEL
#endif
#ifndef COL_SYSTEM_CPU_BAR_BG
#define COL_SYSTEM_CPU_BAR_BG COL_SYSTEM_BAR_BG
#endif
#ifndef COL_SYSTEM_CPU_BAR_FILL
#define COL_SYSTEM_CPU_BAR_FILL COL_SYSTEM_BAR_FILL
#endif
#ifndef COL_SYSTEM_CPU_BAR_WARN
#define COL_SYSTEM_CPU_BAR_WARN COL_SYSTEM_BAR_WARN
#endif
#ifndef COL_SYSTEM_CPU_BAR_ALERT
#define COL_SYSTEM_CPU_BAR_ALERT COL_SYSTEM_BAR_ALERT
#endif
#ifndef COL_SYSTEM_RAM_LABEL
#define COL_SYSTEM_RAM_LABEL COL_SYSTEM_LABEL
#endif
#ifndef COL_SYSTEM_RAM_BAR_BG
#define COL_SYSTEM_RAM_BAR_BG COL_SYSTEM_BAR_BG
#endif
#ifndef COL_SYSTEM_RAM_BAR_FILL
#define COL_SYSTEM_RAM_BAR_FILL COL_SYSTEM_BAR_FILL
#endif
#ifndef COL_SYSTEM_RAM_BAR_WARN
#define COL_SYSTEM_RAM_BAR_WARN COL_SYSTEM_BAR_WARN
#endif
#ifndef COL_SYSTEM_RAM_BAR_ALERT
#define COL_SYSTEM_RAM_BAR_ALERT COL_SYSTEM_BAR_ALERT
#endif

// --- Claude ---
#ifndef COL_CLAUDE_BG
#define COL_CLAUDE_BG COL_BG
#endif
#ifndef COL_CLAUDE_TITLE
#define COL_CLAUDE_TITLE COL_TITLE
#endif
#ifndef COL_CLAUDE_DOT_OK
#define COL_CLAUDE_DOT_OK COL_DOT_ACTIVE
#endif
#ifndef COL_CLAUDE_DOT_WARN
#define COL_CLAUDE_DOT_WARN COL_DOT_WARN
#endif
#ifndef COL_CLAUDE_DOT_ALERT
#define COL_CLAUDE_DOT_ALERT COL_DOT_ALERT
#endif
#ifndef COL_CLAUDE_DOT_IDLE
#define COL_CLAUDE_DOT_IDLE COL_DOT_IDLE
#endif
#ifndef COL_CLAUDE_WORK_DOT_ACTIVE
#define COL_CLAUDE_WORK_DOT_ACTIVE COL_DOT_ACTIVE
#endif
#ifndef COL_CLAUDE_WORK_DOT_IDLE
#define COL_CLAUDE_WORK_DOT_IDLE COL_DOT_IDLE
#endif
#ifndef COL_CLAUDE_WORK_DOT_OVERFLOW
#define COL_CLAUDE_WORK_DOT_OVERFLOW COL_DOT_IDLE
#endif
#ifndef COL_CLAUDE_SESSIONS
#define COL_CLAUDE_SESSIONS COL_TEXT_DIM
#endif
#ifndef COL_CLAUDE_RATE_LABEL
#define COL_CLAUDE_RATE_LABEL COL_BAR_LABEL
#endif
#ifndef COL_CLAUDE_RATE_RESET
#define COL_CLAUDE_RATE_RESET COL_BAR_EXTRA
#endif
#ifndef COL_CLAUDE_RATE_BAR_BG
#define COL_CLAUDE_RATE_BAR_BG COL_BAR_BG
#endif
#ifndef COL_CLAUDE_RATE_BAR_FILL
#define COL_CLAUDE_RATE_BAR_FILL COL_BAR_FILL
#endif
#ifndef COL_CLAUDE_RATE_BAR_WARN
#define COL_CLAUDE_RATE_BAR_WARN COL_BAR_WARN
#endif
#ifndef COL_CLAUDE_RATE_BAR_ALERT
#define COL_CLAUDE_RATE_BAR_ALERT COL_BAR_ALERT
#endif
#ifndef COL_CLAUDE_H5_LABEL
#define COL_CLAUDE_H5_LABEL COL_CLAUDE_RATE_LABEL
#endif
#ifndef COL_CLAUDE_H5_RESET
#define COL_CLAUDE_H5_RESET COL_CLAUDE_RATE_RESET
#endif
#ifndef COL_CLAUDE_H5_BAR_BG
#define COL_CLAUDE_H5_BAR_BG COL_CLAUDE_RATE_BAR_BG
#endif
#ifndef COL_CLAUDE_H5_BAR_FILL
#define COL_CLAUDE_H5_BAR_FILL COL_CLAUDE_RATE_BAR_FILL
#endif
#ifndef COL_CLAUDE_H5_BAR_WARN
#define COL_CLAUDE_H5_BAR_WARN COL_CLAUDE_RATE_BAR_WARN
#endif
#ifndef COL_CLAUDE_H5_BAR_ALERT
#define COL_CLAUDE_H5_BAR_ALERT COL_CLAUDE_RATE_BAR_ALERT
#endif
#ifndef COL_CLAUDE_W7_LABEL
#define COL_CLAUDE_W7_LABEL COL_CLAUDE_RATE_LABEL
#endif
#ifndef COL_CLAUDE_W7_RESET
#define COL_CLAUDE_W7_RESET COL_CLAUDE_RATE_RESET
#endif
#ifndef COL_CLAUDE_W7_BAR_BG
#define COL_CLAUDE_W7_BAR_BG COL_CLAUDE_RATE_BAR_BG
#endif
#ifndef COL_CLAUDE_W7_BAR_FILL
#define COL_CLAUDE_W7_BAR_FILL COL_CLAUDE_RATE_BAR_FILL
#endif
#ifndef COL_CLAUDE_W7_BAR_WARN
#define COL_CLAUDE_W7_BAR_WARN COL_CLAUDE_RATE_BAR_WARN
#endif
#ifndef COL_CLAUDE_W7_BAR_ALERT
#define COL_CLAUDE_W7_BAR_ALERT COL_CLAUDE_RATE_BAR_ALERT
#endif

// --- Status ---
#ifndef COL_STATUS_BG
#define COL_STATUS_BG COL_BG
#endif
#ifndef COL_STATUS_ACTIVE
#define COL_STATUS_ACTIVE COL_DOT_ACTIVE
#endif
#ifndef COL_STATUS_IDLE
#define COL_STATUS_IDLE COL_DOT_IDLE
#endif
#ifndef COL_STATUS_OFFLINE
#define COL_STATUS_OFFLINE COL_DOT_ALERT
#endif
#ifndef COL_STATUS_IDLE_TIME
#define COL_STATUS_IDLE_TIME COL_TEXT_DIM
#endif
#ifndef COL_STATUS_KEYS
#define COL_STATUS_KEYS COL_TEXT_SEC
#endif
#ifndef COL_STATUS_IP
#define COL_STATUS_IP COL_TEXT_DIM
#endif
