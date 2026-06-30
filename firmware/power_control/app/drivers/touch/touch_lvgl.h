#ifndef TOUCH_LVGL_H
#define TOUCH_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "lvgl.h"

bool touch_lvgl_init(lv_display_t *display);
bool touch_lvgl_is_initialized(void);
void touch_lvgl_set_sample(uint16_t x, uint16_t y, bool pressed, uint32_t tick_ms);
bool touch_lvgl_is_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_LVGL_H */
