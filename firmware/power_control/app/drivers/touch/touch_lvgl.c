#include "touch_lvgl.h"

#include "goodix_touch.h"

#define TOUCH_LVGL_READ_PERIOD_MS  4U
#define TOUCH_LVGL_HOLD_TIMEOUT_MS 60U
#define TOUCH_LVGL_SCROLL_LIMIT_PX 4U
#define TOUCH_LVGL_SCROLL_THROW    20U

#define GOODIX_STATUS_READY_MASK 0x80U
#define GOODIX_STATUS_COUNT_MASK 0x0FU

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

  if (!goodix_touch_init())
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

static void touch_lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  goodix_touch_point_t point = {0};
  goodix_touch_diagnostics_t diag = {0};
  bool explicit_release = false;

  (void)indev;

  if (goodix_touch_read(&point))
  {
    last_point.x = (lv_coord_t)point.x;
    last_point.y = (lv_coord_t)point.y;
    last_press_tick = lv_tick_get();
    last_pressed = true;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    goodix_touch_get_diagnostics(&diag);
    explicit_release = ((diag.status & GOODIX_STATUS_READY_MASK) != 0U) &&
                       ((diag.status & GOODIX_STATUS_COUNT_MASK) == 0U);

    if (!explicit_release && last_pressed && diag.error == 0U &&
        lv_tick_elaps(last_press_tick) <= TOUCH_LVGL_HOLD_TIMEOUT_MS)
    {
      data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
      last_pressed = false;
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }

  data->point = last_point;
}
