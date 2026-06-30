#include "lcd_task.h"

#include "cmsis_os2.h"
#include "goodix_touch.h"
#include "lcd_display.h"
#include "lvgl.h"
#include "touch_lvgl.h"

static void touch_test_button_event_cb(lv_event_t *event);
static void update_touch_debug_label(lv_obj_t *label);

void start_lcd_task(void *argument)
{
  lcd_task_entry(argument);
}

void lcd_task_entry(void *argument)
{
  lv_display_t *display = NULL;
  bool touch_initialized = false;
  uint32_t elapsed_ms = 0U;
  uint32_t last_touch_init_attempt_ms = 0U;
  uint32_t last_debug_update_ms = 0U;

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

  lv_obj_t *debug_label = lv_label_create(lv_screen_active());
  lv_obj_set_width(debug_label, 780);
  lv_obj_align(debug_label, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_label_set_text(debug_label, "TP: waiting");

  lcd_display_backlight_on();

  for (;;)
  {
    lv_tick_inc(5);
    lv_timer_handler();

    if (!touch_initialized && elapsed_ms >= 500U &&
        (elapsed_ms - last_touch_init_attempt_ms) >= 1000U)
    {
      touch_initialized = touch_lvgl_init(display);
      last_touch_init_attempt_ms = elapsed_ms;
    }

    if ((elapsed_ms - last_debug_update_ms) >= 200U)
    {
      update_touch_debug_label(debug_label);
      last_debug_update_ms = elapsed_ms;
    }

    elapsed_ms += 5U;
    osDelay(5);
  }
}

static void touch_test_button_event_cb(lv_event_t *event)
{
  lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
  lv_label_set_text(label, "Touched");
  lv_obj_center(label);
}

static void update_touch_debug_label(lv_obj_t *label)
{
  goodix_touch_diagnostics_t diag = {0};
  char text[128] = {0};

  goodix_touch_get_diagnostics(&diag);
  lv_snprintf(text,
              sizeof(text),
              "TP %s ADDR:%02X PID:%s STA:%02X XY:%u,%u RAW:%u,%u CNT:%lu ERR:%u",
              touch_lvgl_is_initialized() ? "OK" : "FAIL",
              diag.address,
              diag.pid,
              diag.status,
              diag.x,
              diag.y,
              diag.raw_x,
              diag.raw_y,
              (unsigned long)diag.read_count,
              diag.error);
  lv_label_set_text(label, text);
}
