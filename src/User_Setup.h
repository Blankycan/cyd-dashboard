// CYD Dashboard — TFT_eSPI hardware config
// Hardware: ESP32-2432S028R (2.8" ILI9341, XPT2046 touch)
// Verified working on bongo_cat_monitor; ported here unchanged.

// Driver
#define ILI9341_2_DRIVER
#define TFT_INVERSION_ON

// Physical panel dimensions (rotation applied in firmware via setRotation())
#define TFT_WIDTH  320
#define TFT_HEIGHT 240

// SPI pins
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// Backlight managed by firmware via ledcWrite — do NOT define TFT_BL here
// or tft.init() will call pinMode/digitalWrite and kill the PWM channel.

// Touch (XPT2046)
#define TOUCH_CS 33
#define SPI_TOUCH_FREQUENCY 2500000

// SPI speed — 65 MHz confirmed stable on this panel
#define SPI_FREQUENCY      65000000
#define SPI_READ_FREQUENCY 20000000

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
