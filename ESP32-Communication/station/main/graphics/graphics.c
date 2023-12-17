#include "graphics.h"

void setup_display()
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };

    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    // Configure the bus for sending data to the screen.
    spi_bus_config_t bus_config = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .max_transfer_sz = SCREEN_WIDTH * sizeof(uint16_t)
    };

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,   // The endian of the color, e.i. RGB or BGR
        .bits_per_pixel = 16,
    };

    // Initialize the LCD configuration on the lily TTGO t-display
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    // Turn off backlight to avoid unpredictable display on the LCD screen while initializing
    // the LCD panel driver. (Different LCD screens may need different levels)
    ESP_ERROR_CHECK(gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_OFF_LEVEL));

    // Reset the display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    // Initialize LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Turn on the screen
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    // Turn on backlight (Different LCD screens may need different levels)
    ESP_ERROR_CHECK(gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL));

    ESP_LOGI(TAG_DISPLAY, "It worked?");

    int x_start = 20;
    int y_start = 30;

    int x_end = 60;
    int y_end = 80;

    // FORMAT: COLOR_SWAP(RGB_565) -> BGR565.
    uint16_t a_color = COLOR_SWAP(0x35f7); // Vi bruger macroen color swap, ellers bliver farverne forkerte, da farverne er af typen BGA.

    uint16_t color_map[240];

    for(int i = 0; i < 240; )
    {
        color_map[i] = a_color;
        i++;
    }

    while(1) 
    {
        const int x = 0;
        const int xend = SCREEN_WIDTH;
        const int y = 0;
        const int yend = SCREEN_HEIGHT;

        for (int i = 0; i < yend - y; ++i) {
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, x, y + i, xend, y + i + 1, color_map));
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}


