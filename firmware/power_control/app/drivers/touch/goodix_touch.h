#ifndef GOODIX_TOUCH_H
#define GOODIX_TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint16_t x;
  uint16_t y;
} goodix_touch_point_t;

bool goodix_touch_init(void);
bool goodix_touch_read(goodix_touch_point_t *point);

#ifdef __cplusplus
}
#endif

#endif /* GOODIX_TOUCH_H */
