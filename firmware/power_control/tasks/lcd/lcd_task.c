#include "lcd_task.h"

#include "cmsis_os2.h"
#include "lcd_display.h"
#include "lvgl.h"

void start_lcd_task(void *argument)
{
  lcd_task_entry(argument);
}

void lcd_task_entry(void *argument)
{
  (void)argument;

  lv_init();
  lcd_display_reset();
  lcd_display_clear(0x0000U);
  if (lcd_display_lvgl_init() == NULL)
  {
    for (;;)
    {
      osDelay(1000);
    }
  }

  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Power Control");
  lv_obj_center(label);
  lcd_display_backlight_on();

  for (;;)
  {
    lv_tick_inc(5);
    lv_timer_handler();
    osDelay(5);
  }
}
