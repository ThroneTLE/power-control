#include "lcd_display.h"

#include "ltdc.h"
#include "main.h"

#define LCD_FRAMEBUFFER_0_ADDRESS LCD_FRAMEBUFFER_ADDRESS
#define LCD_RELOAD_TIMEOUT_LOOPS   1000000U

static uint16_t lcd_back_buffer[LCD_DISPLAY_WIDTH * LCD_DISPLAY_HEIGHT] __attribute__((section(".sdram"), aligned(32)));

static void lcd_display_swap_buffers(uint8_t *px_map);
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
    lcd_back_buffer[i] = color;
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
                         (void *)LCD_FRAMEBUFFER_0_ADDRESS,
                         lcd_back_buffer,
                         LCD_FRAMEBUFFER_SIZE,
                         LV_DISPLAY_RENDER_MODE_DIRECT);

  return display;
}

static void lcd_display_swap_buffers(uint8_t *px_map)
{
  uint32_t next_address = (uint32_t)px_map;
  uint32_t timeout = LCD_RELOAD_TIMEOUT_LOOPS;

  (void)HAL_LTDC_SetAddress_NoReload(&hltdc, next_address, 0U);
  (void)HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING);
  while ((hltdc.Instance->SRCR & LTDC_SRCR_VBR) != 0U && timeout > 0U)
  {
    --timeout;
  }
}

static void lcd_display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
  (void)area;

  if (lv_display_flush_is_last(display))
  {
    lcd_display_swap_buffers(px_map);
  }

  lv_display_flush_ready(display);
}
