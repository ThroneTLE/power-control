#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "lvgl.h"

#define LCD_DISPLAY_WIDTH       800U
#define LCD_DISPLAY_HEIGHT      480U
#define LCD_FRAMEBUFFER_SIZE    (LCD_DISPLAY_WIDTH * LCD_DISPLAY_HEIGHT * 2U)
#define LCD_FRAMEBUFFER_ADDRESS 0xC0000000UL

void lcd_display_reset(void);
void lcd_display_backlight_on(void);
void lcd_display_backlight_off(void);
void lcd_display_clear(uint16_t color);
lv_display_t *lcd_display_lvgl_init(void);
void lcd_display_ltdc_irq_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_DISPLAY_H */
