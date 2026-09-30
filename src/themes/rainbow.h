#pragma once
// =============================================================================
// CYD Dashboard — color theme: Rainbow (diagnostic / every token unique)
//
// Every colour token is intentionally different so any fallback that silently
// fires shows the wrong hue immediately. Widget families each own a hue band:
//   Topbar   indigo bg / yellow clock / cyan date
//   Music    dark-green bg / lime text / orange|yellow|sky icons
//   Calendar dark-crimson bg; pinks/roses, yellow "soon", white-ish now mark
//   Scenes   near-black blue bg; leaves=orange/brown/olive, snow=ice blues, life=chartreuse,
//            stars=lavenders, fish=coral/gold/purple, quote=mint/sage,
//            arcade: invaders=reds/limes, asteroids=greys/cyan, pacman=blue walls/classic ghosts,
//            synthwave=neon purple/pink/cyan, lofi=dusk blues, fireworks=primaries, campfire=embers
//   System   dark-violet bg; CPU=reds, RAM=greens
//   Claude   dark-teal bg / light-blue sessions; H5=violets, W7=teals; working dots=lime/blue-violet/pink
//   Status   dark-gold bg / lime active / sky-blue idle / red offline / orange keys
//
// Selected via CYD_THEME_RAINBOW in config.h (see theme.h for the dispatch).
// =============================================================================

// -----------------------------------------------------------------------------
// LEVEL 1 — RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

// Neutrals / structure
#define PAL_BLACK       0x080810  // near-black indigo  — default background
#define PAL_GLOW        0xFFDD00  // gold               — generic accent / glow

// Generic text & structure (intentionally distinct from every widget override)
#define PAL_BG          0x080810  // near-black indigo
#define PAL_PANEL       0x2200AA  // deep indigo        — topbar panel
#define PAL_BORDER      0xFF00FF  // magenta            — generic borders
#define PAL_DIVIDER     0x8800FF  // violet             — between-widget rules

#define PAL_TEXT_PRI    0x00FFFF  // cyan               — generic primary text
#define PAL_TEXT_SEC    0x00FF88  // mint               — generic secondary text
#define PAL_TEXT_DIM    0xFF8800  // orange             — generic dim text

#define PAL_OK          0x00DD44  // green              — generic ok/connected
#define PAL_WARN        0xFFDD00  // gold               — generic warn
#define PAL_ALERT       0xFF2222  // red                — generic alert
#define PAL_BAR_TRACK   0x111122  // dark indigo        — generic bar track

// -----------------------------------------------------------------------------
// LEVEL 1 — LVGL COLOR MACROS
// -----------------------------------------------------------------------------
#include <lvgl.h>

#define LVC(hex)            lv_color_hex(hex)

#define COL_BG              LVC(PAL_BG)
#define COL_PANEL           LVC(PAL_PANEL)
#define COL_BORDER          LVC(PAL_BORDER)
#define COL_DIVIDER         LVC(PAL_DIVIDER)

#define COL_TEXT_PRI        LVC(PAL_TEXT_PRI)
#define COL_TEXT_SEC        LVC(PAL_TEXT_SEC)
#define COL_TEXT_DIM        LVC(PAL_TEXT_DIM)

#define COL_OK              LVC(PAL_OK)
#define COL_WARN            LVC(PAL_WARN)
#define COL_ALERT           LVC(PAL_ALERT)
#define COL_BAR_TRACK       LVC(PAL_BAR_TRACK)

#define COL_GLOW            LVC(PAL_GLOW)

// -----------------------------------------------------------------------------
// LEVEL 2 — Generic semantic tokens (distinct from every Level 3 override)
// -----------------------------------------------------------------------------

#define COL_TITLE           LVC(0xFFFF00)  // yellow       — generic title
#define COL_DIVIDER_INNER   LVC(0xFF00AA)  // hot pink     — within-widget rules

#define COL_BAR_BG          LVC(0x111122)  // dark indigo  — generic bar bg
#define COL_BAR_FILL        LVC(0x00CCFF)  // sky blue     — generic bar fill
#define COL_BAR_WARN        LVC(0xFFAA00)  // amber        — generic bar warn
#define COL_BAR_ALERT       LVC(0xFF3300)  // orange-red   — generic bar alert
#define COL_BAR_INACTIVE    LVC(0x333366)  // slate        — generic inactive
#define COL_BAR_LABEL       LVC(0xFF8800)  // orange       — generic key label
#define COL_BAR_VALUE       LVC(0x00CCFF)  // sky blue     — generic value text
#define COL_BAR_EXTRA       LVC(0xFF44AA)  // pink         — generic extra info

// -----------------------------------------------------------------------------
// LEVEL 3 — Per-widget overrides (every token explicitly set for diagnostics)
// -----------------------------------------------------------------------------

// Topbar — indigo/yellow/cyan
#define COL_TOPBAR_BG         LVC(0x2200AA)  // deep indigo
#define COL_TOPBAR_CLOCK      LVC(0xFFFF00)  // yellow
#define COL_TOPBAR_DATE       LVC(0x00FFFF)  // cyan
#define COL_TOPBAR_MENU       LVC(0xFF8800)  // orange
#define COL_MENU_BG           LVC(0x101030)  // near-black navy
#define COL_MENU_TEXT         LVC(0xFFFFFF)  // white
#define COL_MENU_HINT         LVC(0x888888)  // grey
#define COL_MENU_HEADING      LVC(0xFF00FF)  // magenta
#define COL_MENU_ACCENT       LVC(0x00FF00)  // green
#define COL_MENU_TRACK        LVC(0x440000)  // dark red
#define COL_MENU_KNOB         LVC(0xFFFF88)  // pale yellow
#define COL_MENU_ROW_SEL      LVC(0x004444)  // dark teal
#define COL_MENU_DIVIDER      LVC(0x0000FF)  // blue

// Music — green family for bg/text, icons spread across orange/yellow/sky
#define COL_MUSIC_BG          LVC(0x001A00)  // very dark green
#define COL_MUSIC_TITLE       LVC(0x00FF44)  // lime
#define COL_MUSIC_TEXT        LVC(0x44FF88)  // mint
#define COL_MUSIC_ICON_PLAY   LVC(0xFF8800)  // orange       (note icon)
#define COL_MUSIC_ICON_PAUSE  LVC(0xFFFF00)  // yellow       (pause bars)
#define COL_MUSIC_ICON_IDLE   LVC(0x00CCFF)  // sky blue     (grass blades)
#define COL_MUSIC_DOT_PLAYING LVC(0xFF4400)  // red-orange
#define COL_MUSIC_DOT_IDLE    LVC(0x0044FF)  // bright blue

// Calendar — dark crimson bg
#define COL_CALENDAR_BG         LVC(0x1A0008)  // very dark crimson
#define COL_CALENDAR_TIME       LVC(0xFF6688)  // pink-red
#define COL_CALENDAR_TITLE      LVC(0xFFCCDD)  // pale pink
#define COL_CALENDAR_NOW        LVC(0xFF0055)  // crimson
#define COL_CALENDAR_COUNTDOWN  LVC(0xFF99AA)  // salmon pink
#define COL_CALENDAR_COUNTDOWN_SOON LVC(0xFFEE00) // yellow
#define COL_CALENDAR_NEXT       LVC(0xCC6677)  // dusty rose
#define COL_CALENDAR_DONE       LVC(0x884455)  // dark rose
#define COL_CALENDAR_PROGRESS_BG   LVC(0x330011)  // dark crimson
#define COL_CALENDAR_PROGRESS_FILL LVC(0xFF3366)  // raspberry
#define COL_CALENDAR_TRACK      LVC(0x2A0010)  // deep crimson
#define COL_CALENDAR_EVENT      LVC(0xFF7799)  // rose
#define COL_CALENDAR_EVENT_PAST LVC(0x662233)  // dim wine
#define COL_CALENDAR_EVENT_NOW  LVC(0xFF0033)  // red
#define COL_CALENDAR_NOW_MARK   LVC(0xFFFFAA)  // pale yellow
#define COL_CALENDAR_HOUR       LVC(0xAA5566)  // muted rose

// Scenes — near-black blue bg
#define COL_SCENE_BG            LVC(0x00060F)  // near-black blue
#define COL_SCENE_LEAVES_1      LVC(0xFF7722)  // tangerine
#define COL_SCENE_LEAVES_2      LVC(0x995522)  // brown
#define COL_SCENE_LEAVES_3      LVC(0xAAAA22)  // olive
#define COL_SCENE_SNOW_NEAR     LVC(0xDDF4FF)  // ice white
#define COL_SCENE_SNOW_FAR      LVC(0x5588AA)  // steel blue
#define COL_SCENE_LIFE_CELL     LVC(0x99FF00)  // chartreuse
#define COL_SCENE_LIFE_BG       LVC(0x000F06)  // near-black green
#define COL_SCENE_STARS_NEAR    LVC(0xF0E0FF)  // pale lavender
#define COL_SCENE_STARS_MID     LVC(0xB090E0)  // lavender
#define COL_SCENE_STARS_FAR     LVC(0x604880)  // dim purple
#define COL_SCENE_FISH_1        LVC(0xFF7F50)  // coral
#define COL_SCENE_FISH_2        LVC(0xFFC020)  // gold
#define COL_SCENE_FISH_3        LVC(0xC040FF)  // purple
#define COL_SCENE_FISH_EYE      LVC(0x101010)  // near-black
#define COL_SCENE_FISH_WEED     LVC(0x228855)  // sea green
#define COL_SCENE_FISH_BUBBLE   LVC(0x66CCDD)  // aqua
#define COL_SCENE_FISH_FOOD     LVC(0xCC9966)  // tan
#define COL_SCENE_QUOTE_TEXT    LVC(0xAAFFCC)  // mint
#define COL_SCENE_QUOTE_AUTHOR  LVC(0x779988)  // sage
#define COL_SCENE_ARCADE_BG     LVC(0x050505)  // near-black
#define COL_SCENE_INV_ALIEN_1   LVC(0xFF3355)  // red-pink
#define COL_SCENE_INV_ALIEN_2   LVC(0xFFAA33)  // amber
#define COL_SCENE_INV_ALIEN_3   LVC(0x66FF33)  // lime
#define COL_SCENE_INV_CANNON    LVC(0x33FFAA)  // spring green
#define COL_SCENE_INV_SHOT      LVC(0xEEFFEE)  // off-white
#define COL_SCENE_INV_BOMB      LVC(0xFF66FF)  // pink
#define COL_SCENE_INV_BOOM      LVC(0xFFFF66)  // light yellow
#define COL_SCENE_AST_ROCK      LVC(0xAAAAAA)  // grey
#define COL_SCENE_AST_SHIP      LVC(0x55FFFF)  // cyan
#define COL_SCENE_AST_SHOT      LVC(0xFFFFFF)  // white
#define COL_SCENE_AST_THRUST    LVC(0xFF6600)  // orange
#define COL_SCENE_AST_DEBRIS    LVC(0x777766)  // khaki grey
#define COL_SCENE_PAC_WALL      LVC(0x2233FF)  // arcade blue
#define COL_SCENE_PAC_FLASH     LVC(0xCCDDFF)  // pale blue
#define COL_SCENE_PAC_DOT       LVC(0xFFCCAA)  // peach
#define COL_SCENE_PAC_PACMAN    LVC(0xFFFF00)  // yellow
#define COL_SCENE_PAC_GHOST_1   LVC(0xFF0000)  // Blinky red
#define COL_SCENE_PAC_GHOST_2   LVC(0xFFB8FF)  // Pinky pink
#define COL_SCENE_PAC_GHOST_3   LVC(0x00FFFF)  // Inky cyan
#define COL_SCENE_PAC_GHOST_4   LVC(0xFFB852)  // Clyde orange
#define COL_SCENE_PAC_FRIGHT    LVC(0x2121DE)  // frightened blue
#define COL_SCENE_PAC_EYES      LVC(0x0000AA)  // dark blue
#define COL_SCENE_PAC_EATEN     LVC(0xF0F0F0)  // white
#define COL_SCENE_SYNTH_SKY_TOP          LVC(0x14002A)  // deep purple
#define COL_SCENE_SYNTH_SKY_LOW          LVC(0xC0306A)  // magenta rose
#define COL_SCENE_SYNTH_STAR             LVC(0xFFE8FF)  // pale pink
#define COL_SCENE_SYNTH_SUN_TOP          LVC(0xFFE040)  // sun yellow
#define COL_SCENE_SYNTH_SUN_LOW          LVC(0xFF2D95)  // hot pink
#define COL_SCENE_SYNTH_MOUNTAIN_FAR     LVC(0x5A1E8C)  // violet
#define COL_SCENE_SYNTH_MOUNTAIN         LVC(0x2A0E4A)  // dark violet
#define COL_SCENE_SYNTH_GROUND_TOP       LVC(0x2A0038)  // plum
#define COL_SCENE_SYNTH_GROUND_LOW       LVC(0x08000F)  // near-black purple
#define COL_SCENE_SYNTH_ROAD             LVC(0x0A0A14)  // asphalt
#define COL_SCENE_SYNTH_GRID             LVC(0x00E5FF)  // neon cyan
#define COL_SCENE_SYNTH_GRID_BEAT        LVC(0xE0FFFF)  // cyan white
#define COL_SCENE_SYNTH_LANE             LVC(0xFFB000)  // amber
#define COL_SCENE_SYNTH_CAR              LVC(0x1C1C2E)  // dark slate
#define COL_SCENE_SYNTH_TAILLIGHT        LVC(0xFF1E3C)  // tail red
#define COL_SCENE_LOFI_ROOM              LVC(0x1A1420)  // dusk room
#define COL_SCENE_LOFI_FRAME             LVC(0x3A2A40)  // window frame
#define COL_SCENE_LOFI_SKY_TOP           LVC(0x1E2A5A)  // night blue
#define COL_SCENE_LOFI_SKY_LOW           LVC(0x7A4A8A)  // city haze
#define COL_SCENE_LOFI_MOON              LVC(0xFFF4D0)  // moon
#define COL_SCENE_LOFI_CITY              LVC(0x121830)  // buildings
#define COL_SCENE_LOFI_CITY_LIGHT        LVC(0xFFD27A)  // window light
#define COL_SCENE_LOFI_RAIN              LVC(0x8AA8D0)  // rain
#define COL_SCENE_LOFI_DESK              LVC(0x4A3428)  // desk
#define COL_SCENE_LOFI_FIGURE            LVC(0x07060A)  // silhouette
#define COL_SCENE_LOFI_HEADPHONES        LVC(0xFF7AB0)  // headphones
#define COL_SCENE_LOFI_LAMP              LVC(0x0C0A10)  // lamp
#define COL_SCENE_LOFI_LAMP_GLOW         LVC(0xFFB35C)  // lamp glow
#define COL_SCENE_LOFI_MUG               LVC(0x10101A)  // mug
#define COL_SCENE_LOFI_STEAM             LVC(0xC8C8D8)  // steam
#define COL_SCENE_LOFI_PAPER             LVC(0xE8E0C8)  // paper
#define COL_SCENE_LOFI_INK               LVC(0x202A50)  // ink
#define COL_SCENE_LOFI_NOTE              LVC(0x9AE8FF)  // music note
#define COL_SCENE_LOFI_HAIR             LVC(0x5B3319)
#define COL_SCENE_LOFI_HAIR_SHADE       LVC(0x3A1F10)
#define COL_SCENE_LOFI_HAIR_TIE         LVC(0x2F6E4F)
#define COL_SCENE_LOFI_SKIN             LVC(0xF2C49B)
#define COL_SCENE_LOFI_SKIN_SHADE       LVC(0xD29A74)
#define COL_SCENE_LOFI_EYE              LVC(0x2A1A14)
#define COL_SCENE_LOFI_PHONES_BAND      LVC(0xEDE3C8)
#define COL_SCENE_LOFI_PHONES_CUSHION   LVC(0x2A2A30)
#define COL_SCENE_LOFI_SWEATER          LVC(0x1F5E4A)
#define COL_SCENE_LOFI_SWEATER_SHADE    LVC(0x154436)
#define COL_SCENE_LOFI_SCARF            LVC(0xE0483A)
#define COL_SCENE_LOFI_SCARF_SHADE      LVC(0xB53428)
#define COL_SCENE_LOFI_HAIR_SHINE       LVC(0x8A5530)
#define COL_SCENE_LOFI_MOUTH            LVC(0xB0584A)
#define COL_SCENE_LOFI_BLUSH            LVC(0xF0A08A)
#define COL_SCENE_LOFI_IRIS             LVC(0x3E6B3A)
#define COL_SCENE_LOFI_COFFEE           LVC(0x3A2014)
#define COL_SCENE_LOFI_MUG_SHADE        LVC(0x5A3C2C)
#define COL_SCENE_LOFI_MUG_RIM          LVC(0xC8A888)
#define COL_SCENE_LOFI_MUG_SHINE        LVC(0xF4DDB0)
#define COL_SCENE_LOFI_SHADE_RIM        LVC(0xF5C07A)
#define COL_SCENE_LOFI_BOOK_COVER       LVC(0x7A3A2E)
#define COL_SCENE_LOFI_PAPER_SHADE      LVC(0xC4B89C)
#define COL_SCENE_LOFI_PAGE_EDGE        LVC(0x968870)
#define COL_SCENE_LOFI_OUTLINE          LVC(0x140C08)
#define COL_SCENE_LOFI_PEN              LVC(0x3A3A44)
#define COL_SCENE_CITY_DAY_TOP           LVC(0x3F8FE0)
#define COL_SCENE_CITY_DAY_LOW           LVC(0xA8D4F2)
#define COL_SCENE_CITY_DUSK_TOP          LVC(0x4A2A6A)
#define COL_SCENE_CITY_DUSK_LOW          LVC(0xFF8A5C)
#define COL_SCENE_CITY_NIGHT_TOP         LVC(0x0A1030)
#define COL_SCENE_CITY_NIGHT_LOW         LVC(0x2A2050)
#define COL_SCENE_CITY_STAR              LVC(0xE8E8FF)
#define COL_SCENE_CITY_SUN               LVC(0xFFD84A)
#define COL_SCENE_CITY_MOON              LVC(0xF4F0D8)
#define COL_SCENE_CITY_FAR               LVC(0x3A3A5A)
#define COL_SCENE_CITY_MID               LVC(0x24243C)
#define COL_SCENE_CITY_NEAR              LVC(0x101018)
#define COL_SCENE_CITY_LIGHT             LVC(0xFFD27A)
#define COL_SCENE_CITY_TRACK             LVC(0x181818)
#define COL_SCENE_CITY_TRAIN             LVC(0x505A6A)
#define COL_SCENE_CITY_PLANE             LVC(0xC0C0C0)
#define COL_SCENE_CITY_BLINK             LVC(0xFF3030)
#define COL_SCENE_CITY_BIRD              LVC(0x1A0A2A)
#define COL_SCENE_LAVA_BG_TOP            LVC(0x1A0830)
#define COL_SCENE_LAVA_BG_LOW            LVC(0x3A1050)
#define COL_SCENE_LAVA_WAX               LVC(0xFF3A6A)
#define COL_SCENE_LAVA_RIM               LVC(0xFFA04A)
#define COL_SCENE_DINO_INK               LVC(0x9AA0A8)
#define COL_SCENE_DINO_CLOUD             LVC(0x5A6070)
#define COL_SCENE_DINO_STAR              LVC(0xD0D8FF)
#define COL_SCENE_DINO_MOON              LVC(0xFFF4C0)
#define COL_SCENE_FLAPPY_SKY_TOP             LVC(0x2080FF)
#define COL_SCENE_FLAPPY_SKY_LOW             LVC(0x60C0FF)
#define COL_SCENE_FLAPPY_CLOUD               LVC(0xF0F0F0)
#define COL_SCENE_FLAPPY_CITY                LVC(0x80A0C0)
#define COL_SCENE_FLAPPY_BUSH                LVC(0x00C040)
#define COL_SCENE_FLAPPY_PIPE                LVC(0x40FF40)
#define COL_SCENE_FLAPPY_PIPE_LIGHT          LVC(0xC0FF80)
#define COL_SCENE_FLAPPY_PIPE_DARK           LVC(0x006020)
#define COL_SCENE_FLAPPY_OUTLINE             LVC(0x400040)
#define COL_SCENE_FLAPPY_GRASS               LVC(0xA0FF00)
#define COL_SCENE_FLAPPY_GRASS_DARK          LVC(0x508000)
#define COL_SCENE_FLAPPY_GROUND              LVC(0xE0C080)
#define COL_SCENE_FLAPPY_BIRD                LVC(0xFFE000)
#define COL_SCENE_FLAPPY_EYE                 LVC(0xFFFFF0)
#define COL_SCENE_FLAPPY_BEAK                LVC(0xFF6000)
#define COL_SCENE_FLAPPY_WING                LVC(0xFFD0A0)
#define COL_SCENE_FLAPPY_SCORE               LVC(0xFF00FF)
#define COL_SCENE_BRK_ROW_1                  LVC(0xFF2020)
#define COL_SCENE_BRK_ROW_2                  LVC(0xFF8000)
#define COL_SCENE_BRK_ROW_3                  LVC(0xFFE000)
#define COL_SCENE_BRK_ROW_4                  LVC(0x20D040)
#define COL_SCENE_BRK_ROW_5                  LVC(0x2090FF)
#define COL_SCENE_BRK_ROW_6                  LVC(0x9040FF)
#define COL_SCENE_BRK_SHINE                  LVC(0xFFFFFF)
#define COL_SCENE_BRK_PADDLE                 LVC(0xC0C0D0)
#define COL_SCENE_BRK_BALL                   LVC(0xFFF080)
#define COL_SCENE_MC_GROUND                  LVC(0x806020)
#define COL_SCENE_MC_CITY                    LVC(0x40A0FF)
#define COL_SCENE_MC_WINDOW                  LVC(0xFFFF60)
#define COL_SCENE_MC_BASE                    LVC(0xE0E0E0)
#define COL_SCENE_MC_ENEMY                   LVC(0xFF3030)
#define COL_SCENE_MC_HEAD                    LVC(0xFFFFFF)
#define COL_SCENE_MC_SHOT                    LVC(0x30FF60)
#define COL_SCENE_MC_FLASH                   LVC(0xFFF8E0)
#define COL_SCENE_MC_BLAST_1                 LVC(0xFFE040)
#define COL_SCENE_MC_BLAST_2                 LVC(0xFF60C0)
#define COL_SCENE_MC_BLAST_3                 LVC(0x60E0FF)
#define COL_SCENE_FW_SKY_TOP             LVC(0x02030C)  // night
#define COL_SCENE_FW_SKY_LOW             LVC(0x1A1840)  // horizon blue
#define COL_SCENE_FW_STAR                LVC(0x9090B0)  // star
#define COL_SCENE_FW_CITY                LVC(0x0C0C14)  // skyline
#define COL_SCENE_FW_CITY_LIGHT          LVC(0xFFCC66)  // lit window
#define COL_SCENE_FW_ROCKET              LVC(0xFFFFE0)  // rocket
#define COL_SCENE_FW_1                   LVC(0xFF3B3B)  // red
#define COL_SCENE_FW_2                   LVC(0x3BFF6E)  // green
#define COL_SCENE_FW_3                   LVC(0x3BA0FF)  // blue
#define COL_SCENE_FW_4                   LVC(0xFF3BF0)  // magenta
#define COL_SCENE_FW_GOLD                LVC(0xFFC23B)  // gold
#define COL_SCENE_FIRE_SKY_TOP           LVC(0x030612)  // night
#define COL_SCENE_FIRE_SKY_LOW           LVC(0x1A2238)  // dusk blue
#define COL_SCENE_FIRE_STAR              LVC(0xD0D8FF)  // star
#define COL_SCENE_FIRE_TREES             LVC(0x06100C)  // pines
#define COL_SCENE_FIRE_GROUND            LVC(0x141008)  // earth
#define COL_SCENE_FIRE_GLOW              LVC(0xFF8A2A)  // firelight
#define COL_SCENE_FIRE_LOGS              LVC(0x2A160A)  // logs
#define COL_SCENE_FIRE_STONES            LVC(0x5A5A60)  // stones
#define COL_SCENE_FIRE_CORE              LVC(0xFFF6C0)  // white-hot
#define COL_SCENE_FIRE_MID               LVC(0xFFA020)  // orange
#define COL_SCENE_FIRE_OUTER             LVC(0xE02810)  // deep red
#define COL_SCENE_FIRE_SPARK             LVC(0xFFD060)  // spark

// System — dark violet bg; CPU reds, RAM greens
#define COL_SYSTEM_BG         LVC(0x0F001A)  // very dark violet
#define COL_SYSTEM_LABEL      LVC(0xFF00FF)  // magenta (widget-level fallback)
#define COL_SYSTEM_BAR_BG     LVC(0x150015)  // very dark magenta (widget fallback)
#define COL_SYSTEM_BAR_FILL   LVC(0xFF44FF)  // magenta (widget fallback)
#define COL_SYSTEM_BAR_WARN   LVC(0xFF88FF)  // light magenta (widget fallback)
#define COL_SYSTEM_BAR_ALERT  LVC(0xFFAAFF)  // pale magenta (widget fallback)

#define COL_SYSTEM_CPU_LABEL    LVC(0xFF4444)  // red
#define COL_SYSTEM_CPU_BAR_BG   LVC(0x220000)  // very dark red
#define COL_SYSTEM_CPU_BAR_FILL LVC(0xFF4444)  // coral red
#define COL_SYSTEM_CPU_BAR_WARN LVC(0xFF8800)  // orange
#define COL_SYSTEM_CPU_BAR_ALERT LVC(0xFF0000) // pure red

#define COL_SYSTEM_RAM_LABEL    LVC(0x00FF44)  // lime
#define COL_SYSTEM_RAM_BAR_BG   LVC(0x002200)  // very dark green
#define COL_SYSTEM_RAM_BAR_FILL LVC(0x00DD44)  // green
#define COL_SYSTEM_RAM_BAR_WARN LVC(0x88FF00)  // yellow-green
#define COL_SYSTEM_RAM_BAR_ALERT LVC(0xFF8800) // orange


// Claude — dark teal bg; light-blue sessions; H5=violets, W7=teals
#define COL_CLAUDE_BG           LVC(0x001A1A)  // very dark teal
#define COL_CLAUDE_TITLE        LVC(0x00FFFF)  // cyan
#define COL_CLAUDE_DOT_OK       LVC(0x00FF88)  // bright mint
#define COL_CLAUDE_DOT_WARN     LVC(0xFFAA00)  // amber
#define COL_CLAUDE_DOT_ALERT    LVC(0xFF2244)  // red-pink
#define COL_CLAUDE_DOT_IDLE     LVC(0x0044AA)  // medium blue
#define COL_CLAUDE_WORK_DOT_ACTIVE   LVC(0x33FF00)  // yellow-green
#define COL_CLAUDE_WORK_DOT_IDLE     LVC(0x6633FF)  // blue-violet
#define COL_CLAUDE_WORK_DOT_OVERFLOW LVC(0xFF3399)  // hot pink
#define COL_CLAUDE_SESSIONS     LVC(0x88CCFF)  // light blue

#define COL_CLAUDE_H5_LABEL     LVC(0xAA44FF)  // violet
#define COL_CLAUDE_H5_RESET     LVC(0xCC88FF)  // light violet
#define COL_CLAUDE_H5_BAR_BG    LVC(0x110022)  // very dark violet
#define COL_CLAUDE_H5_BAR_FILL  LVC(0xAA44FF)  // violet
#define COL_CLAUDE_H5_BAR_WARN  LVC(0xFF44FF)  // magenta
#define COL_CLAUDE_H5_BAR_ALERT LVC(0xFF0088)  // hot pink

#define COL_CLAUDE_W7_LABEL     LVC(0x00CCAA)  // teal
#define COL_CLAUDE_W7_RESET     LVC(0x88FFEE)  // light teal
#define COL_CLAUDE_W7_BAR_BG    LVC(0x001A14)  // very dark teal
#define COL_CLAUDE_W7_BAR_FILL  LVC(0x00CCAA)  // teal
#define COL_CLAUDE_W7_BAR_WARN  LVC(0x00FFAA)  // bright teal
#define COL_CLAUDE_W7_BAR_ALERT LVC(0xFF8800)  // orange

// Status — dark gold bg; lime active, sky idle, red offline
#define COL_STATUS_BG           LVC(0x1A1000)  // very dark gold
#define COL_STATUS_ACTIVE       LVC(0x00FF44)  // lime
#define COL_STATUS_IDLE         LVC(0x4488FF)  // sky blue
#define COL_STATUS_OFFLINE      LVC(0xFF2222)  // red
#define COL_STATUS_IDLE_TIME    LVC(0xFFDD44)  // gold
#define COL_STATUS_KEYS         LVC(0xFF8844)  // orange
#define COL_STATUS_IP           LVC(0xFF88CC)  // pink

// -----------------------------------------------------------------------------
// RGB565 MACROS  (for direct TFT_eSPI drawing outside LVGL)
// -----------------------------------------------------------------------------
#define _R5(hex)  (((hex) >> 19) & 0x1F)
#define _G6(hex)  (((hex) >> 10) & 0x3F)
#define _B5(hex)  (((hex) >>  3) & 0x1F)
#define PAL565(hex) ((_R5(hex) << 11) | (_G6(hex) << 5) | _B5(hex))

#define TFT_COL_BG          PAL565(PAL_BG)
#define TFT_COL_PANEL       PAL565(PAL_PANEL)
#define TFT_COL_TEXT_PRI    PAL565(PAL_TEXT_PRI)
#define TFT_COL_TEXT_SEC    PAL565(PAL_TEXT_SEC)
#define TFT_COL_OK          PAL565(PAL_OK)
#define TFT_COL_WARN        PAL565(PAL_WARN)
#define TFT_COL_ALERT       PAL565(PAL_ALERT)
#define TFT_COL_GLOW        PAL565(PAL_GLOW)
