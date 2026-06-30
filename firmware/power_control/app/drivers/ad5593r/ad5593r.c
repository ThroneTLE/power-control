#include "ad5593r.h"

#include <string.h>

#define AD5593R_POINTER_ADC_SEQUENCE    0x02U
#define AD5593R_POINTER_GENERAL_CONTROL 0x03U
#define AD5593R_POINTER_ADC_CONFIG      0x04U
#define AD5593R_POINTER_DAC_CONFIG      0x05U
#define AD5593R_POINTER_PULLDOWN        0x06U
#define AD5593R_POINTER_LDAC            0x07U
#define AD5593R_POINTER_POWER_REF       0x0BU
#define AD5593R_POINTER_RESET           0x0FU

#define AD5593R_POINTER_DAC_WRITE       0x10U
#define AD5593R_POINTER_ADC_READBACK    0x40U

#define AD5593R_RESET_KEY               0x0DACU
#define AD5593R_ADC_SEQUENCE_REP        0x0200U
#define AD5593R_ADC_SEQUENCE_TEMP       0x0100U
#define AD5593R_LDAC_IMMEDIATE          0x0000U

static ad5593r_status_t ad5593r_write_frame(ad5593r_handle_t *device,
                                            uint8_t pointer,
                                            uint16_t value,
                                            bool allow_ack_failure);
static ad5593r_status_t ad5593r_read_words(ad5593r_handle_t *device,
                                           uint8_t pointer,
                                           uint8_t *data,
                                           size_t length);
static ad5593r_status_t ad5593r_decode_adc_sample(ad5593r_handle_t *device,
                                                  uint16_t word,
                                                  ad5593r_channel_sample_t *sample);
static uint16_t ad5593r_hal_address(const ad5593r_handle_t *device);
static uint16_t ad5593r_clamp_raw(uint16_t raw);

ad5593r_status_t ad5593r_init(ad5593r_handle_t *device, const ad5593r_config_t *config)
{
  ad5593r_status_t status = AD5593R_STATUS_OK;

  if (device == NULL || config == NULL || config->i2c == NULL)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  memset(device, 0, sizeof(*device));
  device->i2c = config->i2c;
  device->timeout_ms = config->timeout_ms != 0U ? config->timeout_ms : AD5593R_DEFAULT_TIMEOUT_MS;
  device->vref_mv = config->vref_mv != 0U ? config->vref_mv : AD5593R_DEFAULT_VREF_MV;
  device->dac_mask = config->dac_mask;
  device->adc_mask = config->adc_mask;

  status = ad5593r_probe(device);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  status = ad5593r_write_frame(device, AD5593R_POINTER_RESET, AD5593R_RESET_KEY, true);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }
  HAL_Delay(1);

  status = ad5593r_write_register(device, AD5593R_POINTER_POWER_REF, 0x0000U);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  status = ad5593r_write_register(device, AD5593R_POINTER_GENERAL_CONTROL, 0x0000U);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  status = ad5593r_write_register(device, AD5593R_POINTER_PULLDOWN, (uint16_t)(~(device->dac_mask | device->adc_mask) & 0x00FFU));
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  status = ad5593r_write_register(device, AD5593R_POINTER_DAC_CONFIG, device->dac_mask);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  status = ad5593r_write_register(device, AD5593R_POINTER_ADC_CONFIG, device->adc_mask);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  status = ad5593r_write_register(device, AD5593R_POINTER_LDAC, AD5593R_LDAC_IMMEDIATE);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  for (uint8_t channel = 0U; channel < AD5593R_CHANNEL_COUNT; ++channel)
  {
    if ((device->dac_mask & (uint8_t)(1U << channel)) != 0U)
    {
      status = ad5593r_write_dac(device, channel, 0U);
      if (status != AD5593R_STATUS_OK)
      {
        return status;
      }
    }
  }

  return ad5593r_set_adc_sequence(device, device->adc_mask, true, false);
}

ad5593r_status_t ad5593r_probe(ad5593r_handle_t *device)
{
  static const uint8_t addresses[] = {AD5593R_I2C_ADDRESS_A0_LOW, AD5593R_I2C_ADDRESS_A0_HIGH};

  if (device == NULL || device->i2c == NULL)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  for (size_t i = 0U; i < (sizeof(addresses) / sizeof(addresses[0])); ++i)
  {
    device->address_7bit = addresses[i];
    if (HAL_I2C_IsDeviceReady(device->i2c, ad5593r_hal_address(device), 2U, device->timeout_ms) == HAL_OK)
    {
      return AD5593R_STATUS_OK;
    }
  }

  device->address_7bit = 0U;
  return AD5593R_STATUS_ERROR_NOT_FOUND;
}

ad5593r_status_t ad5593r_write_register(ad5593r_handle_t *device, uint8_t pointer, uint16_t value)
{
  return ad5593r_write_frame(device, pointer, value, false);
}

ad5593r_status_t ad5593r_read_register(ad5593r_handle_t *device, uint8_t pointer, uint16_t *value)
{
  uint8_t data[2] = {0};
  ad5593r_status_t status = AD5593R_STATUS_OK;

  if (value == NULL)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  status = ad5593r_read_words(device, pointer, data, sizeof(data));
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  *value = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
  return AD5593R_STATUS_OK;
}

ad5593r_status_t ad5593r_write_dac(ad5593r_handle_t *device, uint8_t channel, uint16_t raw)
{
  uint16_t data = 0U;
  ad5593r_status_t status = AD5593R_STATUS_OK;

  if (device == NULL || channel >= AD5593R_CHANNEL_COUNT)
  {
    return AD5593R_STATUS_ERROR_BAD_CHANNEL;
  }

  raw = ad5593r_clamp_raw(raw);
  data = (uint16_t)(0x8000U | ((uint16_t)channel << 12U) | raw);
  status = ad5593r_write_frame(device, (uint8_t)(AD5593R_POINTER_DAC_WRITE | channel), data, false);
  if (status == AD5593R_STATUS_OK)
  {
    device->dac_raw[channel] = raw;
  }

  return status;
}

ad5593r_status_t ad5593r_set_adc_sequence(ad5593r_handle_t *device,
                                           uint8_t channel_mask,
                                           bool repeat,
                                           bool include_temperature)
{
  uint16_t value = channel_mask;

  if (device == NULL)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  if (repeat)
  {
    value |= AD5593R_ADC_SEQUENCE_REP;
  }

  if (include_temperature)
  {
    value |= AD5593R_ADC_SEQUENCE_TEMP;
  }

  return ad5593r_write_register(device, AD5593R_POINTER_ADC_SEQUENCE, value);
}

ad5593r_status_t ad5593r_read_adc(ad5593r_handle_t *device, ad5593r_channel_sample_t *sample)
{
  uint8_t data[2] = {0};
  uint16_t word = 0U;
  ad5593r_status_t status = AD5593R_STATUS_OK;

  status = ad5593r_read_words(device, AD5593R_POINTER_ADC_READBACK, data, sizeof(data));
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  word = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
  return ad5593r_decode_adc_sample(device, word, sample);
}

ad5593r_status_t ad5593r_read_adc_sequence(ad5593r_handle_t *device,
                                            ad5593r_channel_sample_t *samples,
                                            size_t max_samples,
                                            size_t *sample_count)
{
  uint8_t data[AD5593R_CHANNEL_COUNT * 2U] = {0};
  size_t receive_count = 0U;
  ad5593r_status_t status = AD5593R_STATUS_OK;

  if (samples == NULL || max_samples == 0U)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  if (max_samples > AD5593R_CHANNEL_COUNT)
  {
    max_samples = AD5593R_CHANNEL_COUNT;
  }

  receive_count = max_samples * 2U;
  status = ad5593r_read_words(device, AD5593R_POINTER_ADC_READBACK, data, receive_count);
  if (status != AD5593R_STATUS_OK)
  {
    return status;
  }

  for (size_t i = 0U; i < max_samples; ++i)
  {
    const uint16_t word = (uint16_t)(((uint16_t)data[i * 2U] << 8U) | data[(i * 2U) + 1U]);
    status = ad5593r_decode_adc_sample(device, word, &samples[i]);
    if (status != AD5593R_STATUS_OK)
    {
      if (sample_count != NULL)
      {
        *sample_count = i;
      }
      return status;
    }
  }

  if (sample_count != NULL)
  {
    *sample_count = max_samples;
  }

  return AD5593R_STATUS_OK;
}

uint16_t ad5593r_raw_to_millivolts(const ad5593r_handle_t *device, uint16_t raw)
{
  const uint32_t vref_mv = (device != NULL && device->vref_mv != 0U) ? device->vref_mv : AD5593R_DEFAULT_VREF_MV;
  const uint32_t clamped_raw = ad5593r_clamp_raw(raw);

  return (uint16_t)(((clamped_raw * vref_mv) + (AD5593R_ADC_RAW_MAX / 2U)) / AD5593R_ADC_RAW_MAX);
}

static ad5593r_status_t ad5593r_write_frame(ad5593r_handle_t *device,
                                            uint8_t pointer,
                                            uint16_t value,
                                            bool allow_ack_failure)
{
  uint8_t data[3] = {pointer, (uint8_t)(value >> 8U), (uint8_t)(value & 0x00FFU)};
  HAL_StatusTypeDef hal_status = HAL_OK;

  if (device == NULL || device->i2c == NULL || device->address_7bit == 0U)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  hal_status = HAL_I2C_Master_Transmit(device->i2c, ad5593r_hal_address(device), data, sizeof(data), device->timeout_ms);
  if (hal_status == HAL_OK)
  {
    return AD5593R_STATUS_OK;
  }

  if (allow_ack_failure && ((HAL_I2C_GetError(device->i2c) & HAL_I2C_ERROR_AF) != 0U))
  {
    return AD5593R_STATUS_OK;
  }

  return AD5593R_STATUS_ERROR_I2C;
}

static ad5593r_status_t ad5593r_read_words(ad5593r_handle_t *device,
                                           uint8_t pointer,
                                           uint8_t *data,
                                           size_t length)
{
  if (device == NULL || device->i2c == NULL || data == NULL || length == 0U || device->address_7bit == 0U)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  if (length > (AD5593R_CHANNEL_COUNT * 2U))
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  if (HAL_I2C_Master_Transmit(device->i2c, ad5593r_hal_address(device), &pointer, 1U, device->timeout_ms) != HAL_OK)
  {
    return AD5593R_STATUS_ERROR_I2C;
  }

  if (HAL_I2C_Master_Receive(device->i2c, ad5593r_hal_address(device), data, (uint16_t)length, device->timeout_ms) != HAL_OK)
  {
    return AD5593R_STATUS_ERROR_I2C;
  }

  return AD5593R_STATUS_OK;
}

static ad5593r_status_t ad5593r_decode_adc_sample(ad5593r_handle_t *device,
                                                  uint16_t word,
                                                  ad5593r_channel_sample_t *sample)
{
  uint8_t channel = 0U;

  if (sample == NULL)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  if ((word & 0x8000U) != 0U)
  {
    sample->valid = false;
    return AD5593R_STATUS_ERROR_BAD_ADC_FRAME;
  }

  channel = (uint8_t)((word >> 12U) & 0x07U);
  sample->channel = channel;
  sample->raw = (uint16_t)(word & 0x0FFFU);
  sample->millivolts = ad5593r_raw_to_millivolts(device, sample->raw);
  sample->valid = true;
  return AD5593R_STATUS_OK;
}

static uint16_t ad5593r_hal_address(const ad5593r_handle_t *device)
{
  return (uint16_t)((uint16_t)device->address_7bit << 1U);
}

static uint16_t ad5593r_clamp_raw(uint16_t raw)
{
  return raw > AD5593R_ADC_RAW_MAX ? AD5593R_ADC_RAW_MAX : raw;
}
