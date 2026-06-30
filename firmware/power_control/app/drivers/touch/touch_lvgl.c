#include "touch_lvgl.h"

#include "goodix_touch.h"

static lv_point_t last_point = {0};
static bool touch_lvgl_initialized = false;

static void touch_lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

bool touch_lvgl_init(lv_display_t *display)
{
  lv_indev_t *indev = NULL;

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

  (void)indev;

  if (goodix_touch_read(&point))
  {
    last_point.x = (lv_coord_t)point.x;
    last_point.y = (lv_coord_t)point.y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }

  data->point = last_point;
}
