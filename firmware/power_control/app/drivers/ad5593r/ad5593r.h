#ifndef AD5593R_H
#define AD5593R_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32h7xx_hal.h"

#define AD5593R_CHANNEL_COUNT          8U
#define AD5593R_I2C_ADDRESS_A0_LOW     0x10U
#define AD5593R_I2C_ADDRESS_A0_HIGH    0x11U
#define AD5593R_DEFAULT_TIMEOUT_MS     20U
#define AD5593R_DEFAULT_VREF_MV        5000U
#define AD5593R_DEFAULT_DAC_MASK       0x0FU
#define AD5593R_DEFAULT_ADC_MASK       0xF0U
#define AD5593R_ADC_RAW_MAX            4095U

typedef enum
{
  AD5593R_STATUS_OK = 0,
  AD5593R_STATUS_ERROR_ARGUMENT,
  AD5593R_STATUS_ERROR_NOT_FOUND,
  AD5593R_STATUS_ERROR_I2C,
  AD5593R_STATUS_ERROR_BAD_CHANNEL,
  AD5593R_STATUS_ERROR_BAD_ADC_FRAME
} ad5593r_status_t;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint32_t timeout_ms;
  uint16_t vref_mv;
  uint8_t dac_mask;
  uint8_t adc_mask;
} ad5593r_config_t;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint32_t timeout_ms;
  uint16_t vref_mv;
  uint8_t address_7bit;
  uint8_t dac_mask;
  uint8_t adc_mask;
  uint16_t dac_raw[AD5593R_CHANNEL_COUNT];
} ad5593r_handle_t;

typedef struct
{
  uint8_t channel;
  uint16_t raw;
  uint16_t millivolts;
  bool valid;
} ad5593r_channel_sample_t;

ad5593r_status_t ad5593r_init(ad5593r_handle_t *device, const ad5593r_config_t *config);
ad5593r_status_t ad5593r_probe(ad5593r_handle_t *device);
ad5593r_status_t ad5593r_write_register(ad5593r_handle_t *device, uint8_t pointer, uint16_t value);
ad5593r_status_t ad5593r_read_register(ad5593r_handle_t *device, uint8_t pointer, uint16_t *value);
ad5593r_status_t ad5593r_write_dac(ad5593r_handle_t *device, uint8_t channel, uint16_t raw);
ad5593r_status_t ad5593r_set_adc_sequence(ad5593r_handle_t *device,
                                           uint8_t channel_mask,
                                           bool repeat,
                                           bool include_temperature);
ad5593r_status_t ad5593r_read_adc(ad5593r_handle_t *device, ad5593r_channel_sample_t *sample);
ad5593r_status_t ad5593r_read_adc_sequence(ad5593r_handle_t *device,
                                            ad5593r_channel_sample_t *samples,
                                            size_t max_samples,
                                            size_t *sample_count);
uint16_t ad5593r_raw_to_millivolts(const ad5593r_handle_t *device, uint16_t raw);

#ifdef __cplusplus
}
#endif

#endif /* AD5593R_H */
