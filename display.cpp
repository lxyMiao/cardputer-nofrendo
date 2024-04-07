extern "C"
{
#include <nes/nes.h>
}

#include "hw_config.h"
#include <M5Cardputer.h>
#include <lgfx/v1/misc/colortype.hpp>
//#include <Arduino_GFX_Library.h>

/* M5Stack */
#if defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5STACK_FIRE)

#define TFT_BRIGHTNESS 255 /* 0 - 255 */
#define TFT_BL 32
Arduino_DataBus *bus = new Arduino_ESP32SPI(27 /* DC */, 14 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9342 *gfx = new Arduino_ILI9342(bus, 33 /* RST */, 1 /* rotation */);

/* Odroid-Go */
#elif defined(ARDUINO_ODROID_ESP32)

#define TFT_BRIGHTNESS 191 /* 0 - 255 */
#define TFT_BL 14
Arduino_DataBus *bus = new Arduino_ESP32SPI(21 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
Arduino_ILI9341 *gfx = new Arduino_ILI9341(bus, -1 /* RST */, 3 /* rotation */);

/* TTGO T-Watch */
#elif defined(ARDUINO_T) || defined(ARDUINO_TWATCH_BASE) || defined(ARDUINO_TWATCH_2020_V1) || defined(ARDUINO_TWATCH_2020_V2) // TTGO T-Watch

#define TFT_BRIGHTNESS 255 /* 0 - 255 */
#define TFT_BL 12
Arduino_DataBus *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 19 /* MOSI */, -1 /* MISO */);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */, 240, 240, 0, 80);

/* custom hardware */
#else

#define TFT_BRIGHTNESS 128 /* 0 - 255 */

/* HX8357B */
// #define TFT_BL 27
// Arduino_DataBus *bus = new Arduino_ESP32SPI(-1 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, -1 /* MISO */);
// Arduino_TFT *gfx = new Arduino_HX8357B(bus, 33, 3 /* rotation */, true /* IPS */);

/* ST7789 ODROID Compatible pin connection */
// #define TFT_BL 14
// Arduino_DataBus *bus = new Arduino_ESP32SPI(21 /* DC */, 5 /* CS */, SCK, MOSI, MISO);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 1 /* rotation */, true /* IPS */);

/* ST7796 on breadboard */
// #define TFT_BL 32
//Arduino_DataBus *bus = new Arduino_ESP32SPI(32 /* DC */, -1 /* CS */, 25 /* SCK */, 33 /* MOSI */, -1 /* MISO */);
//Arduino_TFT *gfx = new Arduino_ST7796(bus, -1 /* RST */, 1 /* rotation */);

/* ST7796 on LCDKit */
// #define TFT_BL 23
// Arduino_DataBus *bus = new Arduino_ESP32SPI(19 /* DC */, 5 /* CS */, 22 /* SCK */, 21 /* MOSI */, -1 /* MISO */);
// Arduino_ST7796 *gfx = new Arduino_ST7796(bus, 18, 1 /* rotation */);

#endif /* custom hardware */

static int16_t w, h, frame_x, frame_y, frame_x_offset, frame_width, frame_height, frame_line_pixels;
extern int16_t bg_color;
extern uint16_t myPalette[];

extern void display_begin()
{
    //gfx->begin();
    //bg_color = gfx->color565(24, 28, 24); // DARK DARK GREY
  //  gfx->fillScreen(bg_color);
  M5Cardputer.Display.setRotation(1);
  bg_color=M5Cardputer.Display.color565(24,28,24);
    M5Cardputer.Display.fillScreen(bg_color);
#ifdef TFT_BL
    // turn display backlight on
    ledcAttachPin(TFT_BL, 1);     // assign TFT_BL pin to channel 1
    ledcSetup(1, 12000, 8);       // 12 kHz PWM, 8-bit resolution
    ledcWrite(1, TFT_BRIGHTNESS); // brightness 0 - 255
#endif
}

extern "C" void display_init()
{
    w = M5Cardputer.Display.width();
    h = M5Cardputer.Display.height();
    if (w < 480) // assume only 240x240 or 320x240
    {
        if (w > NES_SCREEN_WIDTH)
        {
            frame_x = (w - NES_SCREEN_WIDTH) / 2;
            frame_x_offset = 0;
            frame_width = NES_SCREEN_WIDTH;
            frame_height = NES_SCREEN_HEIGHT;
            frame_line_pixels = frame_width;
        }
        else
        {
            frame_x = 0;
            frame_x_offset = (NES_SCREEN_WIDTH - w) / 2;
            frame_width = w;
            frame_height = 135;
            frame_line_pixels = frame_width;
        }
        frame_y = (h - 135) / 2;
    }
    else // assume 480x320
    {
        frame_x = 0;
        frame_y = 0;
        frame_x_offset = 8;
        frame_width = w;
        frame_height = h;
        frame_line_pixels = frame_width / 2;
    }
}
static uint8_t _lcd_buffer[240];
extern "C" void display_write_frame(const uint8_t *data[])
{
    
    M5Cardputer.Display.startWrite();
   // M5Cardputer.Display.setAddrWindow(frame_x, frame_y, frame_width, frame_height);
    //Serial.println("flush screen");
   
   //M5Cardputer.Display.endWrite();
    for (size_t i = 0; i < 135; i++)
    {
        for (size_t j = 0; j < 240; j++)
        {
           _lcd_buffer[j]=data[i*NES_SCREEN_HEIGHT/135][j*NES_SCREEN_WIDTH/240];
        }
        M5Cardputer.Display.writeIndexedPixels(_lcd_buffer,reinterpret_cast<lgfx::v1::rgb565_t*>(myPalette),240);
    }
   // M5Cardputer.Display.pushImageDMA(0,0,240,135,reinterpret_cast<lgfx::v1::rgb565_t*>(_lcd_buffer));
    M5Cardputer.Display.endWrite();

}

extern "C" void display_clear()
{
    M5Cardputer.Display.fillScreen(bg_color);
}
