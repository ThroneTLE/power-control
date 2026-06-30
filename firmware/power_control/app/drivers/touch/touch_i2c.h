#ifndef TOUCH_I2C_H
#define TOUCH_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void touch_i2c_init(void);
bool touch_i2c_write(uint8_t address, uint16_t reg, const uint8_t *data, size_t length);
bool touch_i2c_read(uint8_t address, uint16_t reg, uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_I2C_H */
