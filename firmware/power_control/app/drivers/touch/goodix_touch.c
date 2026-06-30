#include "goodix_touch.h"

#include <string.h>

#include "lcd_display.h"
#include "main.h"
#include "touch_i2c.h"

#define GOODIX_ADDRESS_PRIMARY   0x14U
#define GOODIX_ADDRESS_SECONDARY 0x5DU

#define GOODIX_REG_CONTROL 0x8040U
#define GOODIX_REG_PID     0x8140U
#define GOODIX_REG_STATUS  0x814EU
#define GOODIX_REG_POINT1  0x8150U

#define GOODIX_STATUS_READY_MASK 0x80U
#define GOODIX_STATUS_COUNT_MASK 0x0FU

static uint8_t goodix_address = GOODIX_ADDRESS_PRIMARY;
static bool goodix_initialized = false;
static goodix_touch_diagnostics_t goodix_diagnostics = {
    .address = GOODIX_ADDRESS_PRIMARY,
    .pid = "----",
};

static bool goodix_probe_address(uint8_t address);
static bool goodix_write_u8(uint16_t reg, uint8_t value);
static bool goodix_read(uint16_t reg, uint8_t *data, size_t length);
static void goodix_reset(void);
static void goodix_config_int_output(GPIO_PinState state);
static void goodix_config_int_input(void);
static void goodix_map_point(uint16_t raw_x, uint16_t raw_y, goodix_touch_point_t *point);

bool goodix_touch_init(void)
{
  uint8_t control = 0U;

  goodix_initialized = false;
  goodix_diagnostics.initialized = false;
  goodix_diagnostics.error = 0U;
  goodix_diagnostics.status = 0U;
  memcpy(goodix_diagnostics.pid, "----", 5U);

  touch_i2c_init();
  goodix_reset();

  if (goodix_probe_address(GOODIX_ADDRESS_PRIMARY))
  {
    goodix_address = GOODIX_ADDRESS_PRIMARY;
  }
  else if (goodix_probe_address(GOODIX_ADDRESS_SECONDARY))
  {
    goodix_address = GOODIX_ADDRESS_SECONDARY;
  }
  else
  {
    goodix_diagnostics.error = 1U;
    return false;
  }

  control = 0x02U;
  (void)goodix_write_u8(GOODIX_REG_CONTROL, control);
  HAL_Delay(10);

  control = 0x00U;
  if (!goodix_write_u8(GOODIX_REG_CONTROL, control))
  {
    goodix_diagnostics.error = 2U;
    return false;
  }

  (void)goodix_write_u8(GOODIX_REG_STATUS, 0x00U);
  goodix_initialized = true;
  goodix_diagnostics.initialized = true;
  goodix_diagnostics.error = 0U;
  return true;
}

bool goodix_touch_read(goodix_touch_point_t *point)
{
  uint8_t status = 0U;
  uint8_t data[7] = {0};
  uint8_t points = 0U;
  uint16_t raw_x = 0U;
  uint16_t raw_y = 0U;

  if (!goodix_initialized || point == NULL)
  {
    goodix_diagnostics.error = 3U;
    return false;
  }

  if (!goodix_read(GOODIX_REG_STATUS, &status, 1U))
  {
    goodix_diagnostics.error = 4U;
    return false;
  }

  goodix_diagnostics.status = status;

  if ((status & GOODIX_STATUS_READY_MASK) == 0U)
  {
    return false;
  }

  points = status & GOODIX_STATUS_COUNT_MASK;
  if (points == 0U)
  {
    (void)goodix_write_u8(GOODIX_REG_STATUS, 0x00U);
    return false;
  }

  if (points > 5U)
  {
    (void)goodix_write_u8(GOODIX_REG_STATUS, 0x00U);
    goodix_diagnostics.error = 5U;
    return false;
  }

  if (!goodix_read(GOODIX_REG_POINT1, data, sizeof(data)))
  {
    (void)goodix_write_u8(GOODIX_REG_STATUS, 0x00U);
    goodix_diagnostics.error = 6U;
    return false;
  }

  (void)goodix_write_u8(GOODIX_REG_STATUS, 0x00U);

  raw_x = (uint16_t)data[1] | ((uint16_t)data[2] << 8U);
  raw_y = (uint16_t)data[3] | ((uint16_t)data[4] << 8U);

  goodix_diagnostics.raw_x = raw_x;
  goodix_diagnostics.raw_y = raw_y;
  goodix_map_point(raw_x, raw_y, point);
  goodix_diagnostics.x = point->x;
  goodix_diagnostics.y = point->y;
  goodix_diagnostics.read_count++;
  goodix_diagnostics.error = 0U;
  return true;
}

void goodix_touch_get_diagnostics(goodix_touch_diagnostics_t *diagnostics)
{
  if (diagnostics != NULL)
  {
    *diagnostics = goodix_diagnostics;
  }
}

static bool goodix_probe_address(uint8_t address)
{
  uint8_t pid[5] = {0};

  if (!touch_i2c_read(address, GOODIX_REG_PID, pid, 4U))
  {
    return false;
  }

  pid[4] = '\0';
  if (memcmp(pid, "911", 3U) == 0 || memcmp(pid, "9147", 4U) == 0 ||
      memcmp(pid, "115", 3U) == 0 || memcmp(pid, "9271", 4U) == 0 ||
      memcmp(pid, "967", 3U) == 0)
  {
    goodix_diagnostics.address = address;
    memcpy(goodix_diagnostics.pid, pid, sizeof(goodix_diagnostics.pid));
    return true;
  }

  return false;
}

static bool goodix_write_u8(uint16_t reg, uint8_t value)
{
  return touch_i2c_write(goodix_address, reg, &value, 1U);
}

static bool goodix_read(uint16_t reg, uint8_t *data, size_t length)
{
  return touch_i2c_read(goodix_address, reg, data, length);
}

static void goodix_reset(void)
{
  goodix_config_int_output(GPIO_PIN_SET);
  HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(30);
  HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  goodix_config_int_input();
  HAL_Delay(100);
}

static void goodix_config_int_output(GPIO_PinState state)
{
  GPIO_InitTypeDef gpio = {0};

  HAL_GPIO_WritePin(CTP_INT_GPIO_Port, CTP_INT_Pin, state);

  gpio.Pin = CTP_INT_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &gpio);
}

static void goodix_config_int_input(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin = CTP_INT_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &gpio);
}

static void goodix_map_point(uint16_t raw_x, uint16_t raw_y, goodix_touch_point_t *point)
{
  if (raw_x >= LCD_DISPLAY_WIDTH)
  {
    raw_x = LCD_DISPLAY_WIDTH - 1U;
  }

  if (raw_y >= LCD_DISPLAY_HEIGHT)
  {
    raw_y = LCD_DISPLAY_HEIGHT - 1U;
  }

  point->x = raw_x;
  point->y = raw_y;
}
