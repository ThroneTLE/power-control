#include "touch_lvgl.h"

#include "cmsis_os2.h"

#define TOUCH_LVGL_READ_PERIOD_MS  1U
#define TOUCH_LVGL_HOLD_TIMEOUT_MS 60U
#define TOUCH_LVGL_SCROLL_LIMIT_PX 4U
#define TOUCH_LVGL_SCROLL_THROW    20U

static lv_point_t last_point = {0};
static uint32_t last_press_tick = 0U;
static bool last_pressed = false;
static bool touch_lvgl_initialized = false;

static void touch_lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

bool touch_lvgl_init(lv_display_t *display)
{
  lv_indev_t *indev = NULL;
  lv_timer_t *read_timer = NULL;

  if (touch_lvgl_initialized)
  {
    return true;
  }

  if (display == NULL)
  {
    return false;
  }

  indev = lv_indev_create();
  if (indev == NULL)
  {
    return false;
  }

  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(indev, display);
  lv_indev_set_read_cb(indev, touch_lvgl_read_cb);
  lv_indev_set_scroll_limit(indev, TOUCH_LVGL_SCROLL_LIMIT_PX);
  lv_indev_set_scroll_throw(indev, TOUCH_LVGL_SCROLL_THROW);
  read_timer = lv_indev_get_read_timer(indev);
  if (read_timer != NULL)
  {
    lv_timer_set_period(read_timer, TOUCH_LVGL_READ_PERIOD_MS);
  }

  touch_lvgl_initialized = true;
  return true;
}

bool touch_lvgl_is_initialized(void)
{
  return touch_lvgl_initialized;
}

void touch_lvgl_set_sample(uint16_t x, uint16_t y, bool pressed, uint32_t tick_ms)
{
  last_point.x = (lv_coord_t)x;
  last_point.y = (lv_coord_t)y;
  last_pressed = pressed;

  if (pressed)
  {
    last_press_tick = tick_ms;
  }
}

bool touch_lvgl_is_pressed(void)
{
  return last_pressed && ((osKernelGetTickCount() - last_press_tick) <= TOUCH_LVGL_HOLD_TIMEOUT_MS);
}

static void touch_lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  (void)indev;

  data->point = last_point;
  data->state = touch_lvgl_is_pressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
