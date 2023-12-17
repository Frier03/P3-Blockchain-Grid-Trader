#ifndef GRAPHICS_H 
#define GRAPHICS_H

#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"

// LCD headers
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define LCD_HOST 1

// To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many.
// More means more memory use, but less overhead for setting up / finishing transfers. Make sure 240
// is dividable by this.
#define PARALLEL_LINES 16

// define pin headers, see pinouts on Lily TTGO t-display
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL  1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL

#define PIN_NUM_MOSI 19
#define PIN_NUM_SCLK 18
#define PIN_NUM_CS 5
#define PIN_NUM_DC 16
#define PIN_NUM_RST 23
#define PIN_NUM_BK_LIGHT 4

// Screen resolution
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// bits
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

// Header tag
#define TAG_DISPLAY "LASET_DISP"

// Byte swap macro
#define COLOR_SWAP(x) ((x >> 8) | (x << 8))


void setup_display();

void draw(esp_lcd_panel_handle_t screen_handle);

#endif