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

typedef struct
{
  bool initialized;
  uint8_t address;
  char pid[5];
  uint8_t status;
  uint16_t raw_x;
  uint16_t raw_y;
  uint16_t x;
  uint16_t y;
  uint32_t scan_count;
  uint32_t read_count;
  uint16_t config_checksum;
  uint8_t config_version;
  uint8_t error;
  bool config_applied;
} goodix_touch_diagnostics_t;

bool goodix_touch_init(void);
bool goodix_touch_read(goodix_touch_point_t *point);
void goodix_touch_get_diagnostics(goodix_touch_diagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* GOODIX_TOUCH_H */
