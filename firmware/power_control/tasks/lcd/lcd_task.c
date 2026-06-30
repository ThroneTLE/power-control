#include "lcd_task.h"

#include "cmsis_os2.h"
#include "lcd_display.h"
#include "lvgl.h"
#include "touch_lvgl.h"

static void touch_test_button_event_cb(lv_event_t *event);

void start_lcd_task(void *argument)
{
  lcd_task_entry(argument);
}

void lcd_task_entry(void *argument)
{
  lv_display_t *display = NULL;

  (void)argument;

  lv_init();
  lcd_display_reset();
  lcd_display_clear(0x0000U);
  display = lcd_display_lvgl_init();
  if (display == NULL)
  {
    for (;;)
    {
      osDelay(1000);
    }
  }

  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Power Control");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);

  lv_obj_t *button = lv_button_create(lv_screen_active());
  lv_obj_set_size(button, 180, 54);
  lv_obj_align(button, LV_ALIGN_CENTER, 0, 40);

  lv_obj_t *button_label = lv_label_create(button);
  lv_label_set_text(button_label, "Touch Test");
  lv_obj_center(button_label);
  lv_obj_add_event_cb(button, touch_test_button_event_cb, LV_EVENT_CLICKED, button_label);

  (void)touch_lvgl_init(display);
  lcd_display_backlight_on();

  for (;;)
  {
    lv_tick_inc(5);
    lv_timer_handler();
    osDelay(5);
  }
}

static void touch_test_button_event_cb(lv_event_t *event)
{
  lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
  lv_label_set_text(label, "Touched");
  lv_obj_center(label);
}
