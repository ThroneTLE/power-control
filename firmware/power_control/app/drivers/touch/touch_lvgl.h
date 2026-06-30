#ifndef TOUCH_LVGL_H
#define TOUCH_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "lvgl.h"

bool touch_lvgl_init(lv_display_t *display);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_LVGL_H */
