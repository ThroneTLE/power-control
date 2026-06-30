#include "touch_i2c.h"

#include "main.h"

#define TOUCH_I2C_ACK_TIMEOUT   250U
#define TOUCH_I2C_DELAY_CYCLES  80U

static void touch_i2c_delay(void);
static void touch_i2c_start(void);
static void touch_i2c_stop(void);
static bool touch_i2c_wait_ack(void);
static void touch_i2c_ack(void);
static void touch_i2c_nack(void);
static void touch_i2c_send_byte(uint8_t data);
static uint8_t touch_i2c_read_byte(bool ack);
static bool touch_i2c_write_reg_address(uint8_t address, uint16_t reg);
static void touch_i2c_config_gpio(void);
static void touch_scl_write(GPIO_PinState state);
static void touch_sda_write(GPIO_PinState state);
static GPIO_PinState touch_sda_read(void);

void touch_i2c_init(void)
{
  touch_i2c_config_gpio();
  touch_sda_write(GPIO_PIN_SET);
  touch_scl_write(GPIO_PIN_SET);
  touch_i2c_stop();
}

bool touch_i2c_write(uint8_t address, uint16_t reg, const uint8_t *data, size_t length)
{
  touch_i2c_start();

  if (!touch_i2c_write_reg_address(address, reg))
  {
    touch_i2c_stop();
    return false;
  }

  for (size_t i = 0; i < length; ++i)
  {
    touch_i2c_send_byte(data[i]);
    if (!touch_i2c_wait_ack())
    {
      touch_i2c_stop();
      return false;
    }
  }

  touch_i2c_stop();
  return true;
}

bool touch_i2c_read(uint8_t address, uint16_t reg, uint8_t *data, size_t length)
{
  if (data == NULL || length == 0U)
  {
    return false;
  }

  touch_i2c_start();
  if (!touch_i2c_write_reg_address(address, reg))
  {
    touch_i2c_stop();
    return false;
  }

  touch_i2c_start();
  touch_i2c_send_byte((uint8_t)((address << 1U) | 0x01U));
  if (!touch_i2c_wait_ack())
  {
    touch_i2c_stop();
    return false;
  }

  for (size_t i = 0; i < length; ++i)
  {
    data[i] = touch_i2c_read_byte(i < (length - 1U));
  }

  touch_i2c_stop();
  return true;
}

static void touch_i2c_delay(void)
{
  for (volatile uint32_t i = 0; i < TOUCH_I2C_DELAY_CYCLES; ++i)
  {
    __NOP();
  }
}

static void touch_i2c_start(void)
{
  touch_sda_write(GPIO_PIN_SET);
  touch_scl_write(GPIO_PIN_SET);
  touch_i2c_delay();
  touch_sda_write(GPIO_PIN_RESET);
  touch_i2c_delay();
  touch_scl_write(GPIO_PIN_RESET);
  touch_i2c_delay();
}

static void touch_i2c_stop(void)
{
  touch_sda_write(GPIO_PIN_RESET);
  touch_i2c_delay();
  touch_scl_write(GPIO_PIN_SET);
  touch_i2c_delay();
  touch_sda_write(GPIO_PIN_SET);
  touch_i2c_delay();
}

static bool touch_i2c_wait_ack(void)
{
  uint32_t timeout = 0U;

  touch_sda_write(GPIO_PIN_SET);
  touch_i2c_delay();
  touch_scl_write(GPIO_PIN_SET);
  touch_i2c_delay();

  while (touch_sda_read() == GPIO_PIN_SET)
  {
    if (++timeout > TOUCH_I2C_ACK_TIMEOUT)
    {
      touch_i2c_stop();
      return false;
    }
    touch_i2c_delay();
  }

  touch_scl_write(GPIO_PIN_RESET);
  touch_i2c_delay();
  return true;
}

static void touch_i2c_ack(void)
{
  touch_sda_write(GPIO_PIN_RESET);
  touch_i2c_delay();
  touch_scl_write(GPIO_PIN_SET);
  touch_i2c_delay();
  touch_scl_write(GPIO_PIN_RESET);
  touch_i2c_delay();
  touch_sda_write(GPIO_PIN_SET);
  touch_i2c_delay();
}

static void touch_i2c_nack(void)
{
  touch_sda_write(GPIO_PIN_SET);
  touch_i2c_delay();
  touch_scl_write(GPIO_PIN_SET);
  touch_i2c_delay();
  touch_scl_write(GPIO_PIN_RESET);
  touch_i2c_delay();
}

static void touch_i2c_send_byte(uint8_t data)
{
  for (uint8_t i = 0; i < 8U; ++i)
  {
    touch_sda_write((data & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    touch_i2c_delay();
    touch_scl_write(GPIO_PIN_SET);
    touch_i2c_delay();
    touch_scl_write(GPIO_PIN_RESET);
    data <<= 1U;
  }

  touch_sda_write(GPIO_PIN_SET);
}

static uint8_t touch_i2c_read_byte(bool ack)
{
  uint8_t data = 0U;

  touch_sda_write(GPIO_PIN_SET);
  for (uint8_t i = 0; i < 8U; ++i)
  {
    data <<= 1U;
    touch_scl_write(GPIO_PIN_SET);
    touch_i2c_delay();
    if (touch_sda_read() == GPIO_PIN_SET)
    {
      data |= 0x01U;
    }
    touch_scl_write(GPIO_PIN_RESET);
    touch_i2c_delay();
  }

  if (ack)
  {
    touch_i2c_ack();
  }
  else
  {
    touch_i2c_nack();
  }

  return data;
}

static bool touch_i2c_write_reg_address(uint8_t address, uint16_t reg)
{
  touch_i2c_send_byte((uint8_t)(address << 1U));
  if (!touch_i2c_wait_ack())
  {
    return false;
  }

  touch_i2c_send_byte((uint8_t)(reg >> 8U));
  if (!touch_i2c_wait_ack())
  {
    touch_i2c_stop();
    return false;
  }

  touch_i2c_send_byte((uint8_t)(reg & 0xFFU));
  if (!touch_i2c_wait_ack())
  {
    touch_i2c_stop();
    return false;
  }

  return true;
}

static void touch_i2c_config_gpio(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin = CTP_SCL_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(CTP_SCL_GPIO_Port, &gpio);

  gpio.Pin = CTP_SDA_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(CTP_SDA_GPIO_Port, &gpio);
}

static void touch_scl_write(GPIO_PinState state)
{
  if (state == GPIO_PIN_SET)
  {
    CTP_SCL_GPIO_Port->BSRR = CTP_SCL_Pin;
  }
  else
  {
    CTP_SCL_GPIO_Port->BSRR = (uint32_t)CTP_SCL_Pin << 16U;
  }
}

static void touch_sda_write(GPIO_PinState state)
{
  if (state == GPIO_PIN_SET)
  {
    CTP_SDA_GPIO_Port->BSRR = CTP_SDA_Pin;
  }
  else
  {
    CTP_SDA_GPIO_Port->BSRR = (uint32_t)CTP_SDA_Pin << 16U;
  }
}

static GPIO_PinState touch_sda_read(void)
{
  return ((CTP_SDA_GPIO_Port->IDR & CTP_SDA_Pin) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
