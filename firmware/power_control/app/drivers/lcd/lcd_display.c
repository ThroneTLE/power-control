#include "lcd_display.h"

#include <string.h>
#include "main.h"

#define LCD_DRAW_BUFFER_LINES 40U
#define LCD_DRAW_BUFFER_PIXELS (LCD_DISPLAY_WIDTH * LCD_DRAW_BUFFER_LINES)

static uint16_t lcd_draw_buffer_1[LCD_DRAW_BUFFER_PIXELS] __attribute__((section(".sdram"), aligned(32)));
static uint16_t lcd_draw_buffer_2[LCD_DRAW_BUFFER_PIXELS] __attribute__((section(".sdram"), aligned(32)));

static void lcd_display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);

void lcd_display_reset(void)
{
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(120);
}

void lcd_display_backlight_on(void)
{
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

void lcd_display_backlight_off(void)
{
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);
}

void lcd_display_clear(uint16_t color)
{
  uint16_t *framebuffer = (uint16_t *)LCD_FRAMEBUFFER_ADDRESS;

  for (uint32_t i = 0; i < LCD_DISPLAY_WIDTH * LCD_DISPLAY_HEIGHT; ++i)
  {
    framebuffer[i] = color;
  }
}

lv_display_t *lcd_display_lvgl_init(void)
{
  lv_display_t *display = lv_display_create((int32_t)LCD_DISPLAY_WIDTH, (int32_t)LCD_DISPLAY_HEIGHT);
  if (display == NULL)
  {
    return NULL;
  }

  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, lcd_display_flush);
  lv_display_set_buffers(display,
                         lcd_draw_buffer_1,
                         lcd_draw_buffer_2,
                         sizeof(lcd_draw_buffer_1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  return display;
}

static void lcd_display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;
  const uint16_t *source = (const uint16_t *)px_map;
  uint16_t *framebuffer = (uint16_t *)LCD_FRAMEBUFFER_ADDRESS;

  for (int32_t y = 0; y < height; ++y)
  {
    uint16_t *destination = framebuffer + ((area->y1 + y) * LCD_DISPLAY_WIDTH) + area->x1;
    memcpy(destination, source + (y * width), (size_t)width * sizeof(uint16_t));
  }

  lv_display_flush_ready(display);
}
