#include "touch_task.h"

#include "cmsis_os2.h"
#include "goodix_touch.h"
#include "touch_lvgl.h"

#define TOUCH_TASK_PERIOD_MS      1U
#define TOUCH_TASK_INIT_RETRY_MS  500U
#define TOUCH_TASK_HOLD_TIMEOUT_MS 60U

void start_touch_task(void *argument)
{
  touch_task_entry(argument);
}

void touch_task_entry(void *argument)
{
  goodix_touch_point_t point = {0};
  bool initialized = false;
  bool pressed = false;
  uint32_t elapsed_ms = 0U;
  uint32_t last_init_attempt_ms = TOUCH_TASK_INIT_RETRY_MS;
  uint32_t last_press_ms = 0U;

  (void)argument;

  for (;;)
  {
    if (!initialized && (elapsed_ms - last_init_attempt_ms) >= TOUCH_TASK_INIT_RETRY_MS)
    {
      initialized = goodix_touch_init();
      last_init_attempt_ms = elapsed_ms;
    }

    if (initialized)
    {
      if (goodix_touch_read(&point))
      {
        pressed = true;
        last_press_ms = elapsed_ms;
        touch_lvgl_set_sample(point.x, point.y, true, osKernelGetTickCount());
      }
      else if (pressed && (elapsed_ms - last_press_ms) > TOUCH_TASK_HOLD_TIMEOUT_MS)
      {
        pressed = false;
        touch_lvgl_set_sample(point.x, point.y, false, osKernelGetTickCount());
      }
    }

    osDelay(TOUCH_TASK_PERIOD_MS);
    elapsed_ms += TOUCH_TASK_PERIOD_MS;
  }
}
