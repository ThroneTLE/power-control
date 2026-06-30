#include "lcd_task.h"

#include "ad5593r.h"
#include "cmsis_os2.h"
#include "goodix_touch.h"
#include "i2c.h"
#include "lcd_display.h"
#include "lvgl.h"
#include "touch_lvgl.h"

#include <string.h>

#define LCD_TASK_PERIOD_MS       2U
#define AD5593R_WAVE_PERIOD_MS   16U
#define AD5593R_UI_I2C_TIMEOUT_MS 5U
#define TOUCH_INIT_DELAY_MS      500U
#define TOUCH_INIT_RETRY_MS      500U
#define TOUCH_DEBUG_PERIOD_MS    50U
#define AD5593R_CHART_POINTS     96U
#define AD5593R_WAVE_POINTS      64U
#define AD5593R_TEST_DAC_CHANNEL 0U
#define AD5593R_TEST_ADC_CHANNEL 4U
#define AD5593R_TEST_DAC_MASK    (1U << AD5593R_TEST_DAC_CHANNEL)
#define AD5593R_TEST_ADC_MASK    (1U << AD5593R_TEST_ADC_CHANNEL)

typedef struct
{
  lv_obj_t *status_label;
  lv_obj_t *value_label;
  lv_obj_t *touch_debug_label;
  lv_obj_t *chart;
  lv_chart_series_t *dac_series;
  lv_chart_series_t *adc_series;
  lv_obj_t *table;
} lcd_ad5593r_ui_t;

static void lcd_ad5593r_create_ui(lcd_ad5593r_ui_t *ui);
static void lcd_ad5593r_set_status(lcd_ad5593r_ui_t *ui, const char *prefix, ad5593r_status_t status);
static void lcd_ad5593r_update_wave_ui(lcd_ad5593r_ui_t *ui,
                                       ad5593r_handle_t *ad5593r,
                                       uint16_t dac_raw,
                                       const ad5593r_channel_sample_t *adc_sample);
static ad5593r_status_t lcd_ad5593r_read_adc_channel(ad5593r_handle_t *ad5593r,
                                                     uint8_t channel,
                                                     ad5593r_channel_sample_t *sample);
static void update_touch_debug_label(lv_obj_t *label);
static const char *lcd_ad5593r_status_text(ad5593r_status_t status);

static const uint16_t lcd_ad5593r_sine_raw[AD5593R_WAVE_POINTS] = {
    2048U, 2209U, 2368U, 2523U, 2675U, 2820U, 2958U, 3087U,
    3206U, 3314U, 3410U, 3493U, 3561U, 3615U, 3655U, 3678U,
    3686U, 3678U, 3655U, 3615U, 3561U, 3493U, 3410U, 3314U,
    3206U, 3087U, 2958U, 2820U, 2675U, 2523U, 2368U, 2209U,
    2048U, 1887U, 1728U, 1573U, 1421U, 1276U, 1138U, 1009U,
    890U,  782U,  686U,  603U,  535U,  481U,  441U,  418U,
    410U,  418U,  441U,  481U,  535U,  603U,  686U,  782U,
    890U,  1009U, 1138U, 1276U, 1421U, 1573U, 1728U, 1887U};

void start_lcd_task(void *argument)
{
  lcd_task_entry(argument);
}

void lcd_task_entry(void *argument)
{
  lv_display_t *display = NULL;
  ad5593r_handle_t ad5593r = {0};
  const ad5593r_config_t ad5593r_config = {
      .i2c = &hi2c1,
      .timeout_ms = AD5593R_UI_I2C_TIMEOUT_MS,
      .vref_mv = AD5593R_DEFAULT_VREF_MV,
      .dac_mask = AD5593R_TEST_DAC_MASK,
      .adc_mask = AD5593R_TEST_ADC_MASK,
  };
  ad5593r_channel_sample_t adc_sample = {0};
  lcd_ad5593r_ui_t ui = {0};
  ad5593r_status_t ad5593r_status = AD5593R_STATUS_OK;
  bool ad5593r_ready = false;
  bool touch_initialized = false;
  uint32_t elapsed_ms = 0U;
  uint32_t last_touch_init_attempt_ms = 0U;
  uint32_t last_debug_update_ms = 0U;
  uint32_t wave_elapsed_ms = AD5593R_WAVE_PERIOD_MS;
  uint32_t wave_index = 0U;
  uint16_t dac_raw = lcd_ad5593r_sine_raw[0];

  (void)argument;

  lv_init();
  lcd_display_reset();
  lcd_display_clear(0x0000U);
  display = lcd_display_lvgl_init();
  if (display == NULL)
  {
    for (;;)
    {
      osDelay(1000);
    }
  }

  lcd_ad5593r_create_ui(&ui);
  lcd_display_backlight_on();

  ad5593r_status = ad5593r_init(&ad5593r, &ad5593r_config);
  ad5593r_ready = (ad5593r_status == AD5593R_STATUS_OK);
  if (ad5593r_ready)
  {
    lcd_ad5593r_set_status(&ui, "AD5593R ready", ad5593r_status);
    lcd_ad5593r_update_wave_ui(&ui, &ad5593r, dac_raw, NULL);
  }
  else
  {
    lcd_ad5593r_set_status(&ui, "AD5593R init failed", ad5593r_status);
  }

  for (;;)
  {
    lv_tick_inc(LCD_TASK_PERIOD_MS);
    lv_timer_handler();

    if (!touch_initialized && elapsed_ms >= TOUCH_INIT_DELAY_MS &&
        (elapsed_ms - last_touch_init_attempt_ms) >= TOUCH_INIT_RETRY_MS)
    {
      touch_initialized = touch_lvgl_init(display);
      last_touch_init_attempt_ms = elapsed_ms;
    }

    if ((elapsed_ms - last_debug_update_ms) >= TOUCH_DEBUG_PERIOD_MS)
    {
      update_touch_debug_label(ui.touch_debug_label);
      last_debug_update_ms = elapsed_ms;
    }

    if (ad5593r_ready && wave_elapsed_ms >= AD5593R_WAVE_PERIOD_MS)
    {
      dac_raw = lcd_ad5593r_sine_raw[wave_index];
      ad5593r_status = ad5593r_write_dac(&ad5593r, AD5593R_TEST_DAC_CHANNEL, dac_raw);
      if (ad5593r_status == AD5593R_STATUS_OK)
      {
        ad5593r_status = lcd_ad5593r_read_adc_channel(&ad5593r, AD5593R_TEST_ADC_CHANNEL, &adc_sample);
      }

      if (ad5593r_status == AD5593R_STATUS_OK)
      {
        wave_index = (wave_index + 1U) % AD5593R_WAVE_POINTS;
        lcd_ad5593r_set_status(&ui, "AD5593R ready: connect IO0 to IO4", ad5593r_status);
        lcd_ad5593r_update_wave_ui(&ui, &ad5593r, dac_raw, &adc_sample);
      }
      else
      {
        lcd_ad5593r_set_status(&ui, "AD5593R wave/read failed", ad5593r_status);
      }

      wave_elapsed_ms = 0U;
    }

    osDelay(LCD_TASK_PERIOD_MS);
    elapsed_ms += LCD_TASK_PERIOD_MS;
    wave_elapsed_ms += LCD_TASK_PERIOD_MS;
  }
}

static void lcd_ad5593r_create_ui(lcd_ad5593r_ui_t *ui)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *title = NULL;
  lv_obj_t *legend = NULL;

  if (ui == NULL)
  {
    return;
  }

  title = lv_label_create(screen);
  lv_label_set_text(title, "AD5593R IO0 to IO4 Loopback");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 4);

  ui->status_label = lv_label_create(screen);
  lv_obj_set_style_text_font(ui->status_label, &lv_font_montserrat_16, 0);
  lv_label_set_text(ui->status_label, "AD5593R starting...");
  lv_obj_align(ui->status_label, LV_ALIGN_TOP_LEFT, 16, 34);

  ui->value_label = lv_label_create(screen);
  lv_obj_set_style_text_font(ui->value_label, &lv_font_montserrat_20, 0);
  lv_label_set_text(ui->value_label, "IO0 DAC -- | IO4 ADC --");
  lv_obj_align(ui->value_label, LV_ALIGN_TOP_LEFT, 16, 56);

  ui->touch_debug_label = lv_label_create(screen);
  lv_label_set_long_mode(ui->touch_debug_label, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(ui->touch_debug_label, 310);
  lv_obj_set_style_text_font(ui->touch_debug_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(ui->touch_debug_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(ui->touch_debug_label, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_label_set_text(ui->touch_debug_label, "TP: waiting");

  ui->chart = lv_chart_create(screen);
  lv_obj_set_size(ui->chart, 760, 225);
  lv_obj_align(ui->chart, LV_ALIGN_TOP_MID, 0, 84);
  lv_chart_set_type(ui->chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(ui->chart, AD5593R_CHART_POINTS);
  lv_chart_set_update_mode(ui->chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_axis_range(ui->chart, LV_CHART_AXIS_PRIMARY_Y, 0, AD5593R_DEFAULT_VREF_MV);
  lv_chart_set_div_line_count(ui->chart, 6U, 8U);
  lv_obj_set_style_radius(ui->chart, 4, 0);
  lv_obj_set_style_border_width(ui->chart, 1, 0);
  lv_obj_set_style_pad_all(ui->chart, 8, 0);

  ui->dac_series = lv_chart_add_series(ui->chart, lv_color_hex(0xE67E22), LV_CHART_AXIS_PRIMARY_Y);
  ui->adc_series = lv_chart_add_series(ui->chart, lv_color_hex(0x1F77B4), LV_CHART_AXIS_PRIMARY_Y);
  if (ui->dac_series != NULL)
  {
    lv_chart_set_all_values(ui->chart, ui->dac_series, LV_CHART_POINT_NONE);
  }
  if (ui->adc_series != NULL)
  {
    lv_chart_set_all_values(ui->chart, ui->adc_series, LV_CHART_POINT_NONE);
  }

  legend = lv_label_create(screen);
  lv_obj_set_style_text_font(legend, &lv_font_montserrat_16, 0);
  lv_label_set_text(legend, "Orange: IO0 DAC output    Blue: IO4 ADC input");
  lv_obj_align(legend, LV_ALIGN_TOP_MID, 0, 314);

  ui->table = lv_table_create(screen);
  lv_table_set_column_count(ui->table, 4U);
  lv_table_set_row_count(ui->table, 3U);
  lv_table_set_column_width(ui->table, 0U, 100);
  lv_table_set_column_width(ui->table, 1U, 150);
  lv_table_set_column_width(ui->table, 2U, 210);
  lv_table_set_column_width(ui->table, 3U, 300);
  lv_obj_set_size(ui->table, 760, 136);
  lv_obj_align(ui->table, LV_ALIGN_TOP_MID, 0, 338);
  lv_obj_set_style_text_font(ui->table, &lv_font_montserrat_20, LV_PART_ITEMS);
  lv_obj_set_style_text_align(ui->table, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
  lv_obj_set_style_pad_top(ui->table, 10, LV_PART_ITEMS);
  lv_obj_set_style_pad_bottom(ui->table, 10, LV_PART_ITEMS);
  lv_obj_set_style_pad_left(ui->table, 8, LV_PART_ITEMS);
  lv_obj_set_style_pad_right(ui->table, 8, LV_PART_ITEMS);
  lv_obj_set_style_radius(ui->table, 4, 0);
  lv_obj_set_style_border_width(ui->table, 1, 0);

  lv_table_set_cell_value(ui->table, 0U, 0U, "CH");
  lv_table_set_cell_value(ui->table, 0U, 1U, "Mode");
  lv_table_set_cell_value(ui->table, 0U, 2U, "Raw");
  lv_table_set_cell_value(ui->table, 0U, 3U, "Voltage");

  lv_table_set_cell_value(ui->table, 1U, 0U, "IO0");
  lv_table_set_cell_value(ui->table, 1U, 1U, "DAC OUT");
  lv_table_set_cell_value(ui->table, 1U, 2U, "--");
  lv_table_set_cell_value(ui->table, 1U, 3U, "--");

  lv_table_set_cell_value(ui->table, 2U, 0U, "IO4");
  lv_table_set_cell_value(ui->table, 2U, 1U, "ADC IN");
  lv_table_set_cell_value(ui->table, 2U, 2U, "--");
  lv_table_set_cell_value(ui->table, 2U, 3U, "--");
}

static void lcd_ad5593r_set_status(lcd_ad5593r_ui_t *ui, const char *prefix, ad5593r_status_t status)
{
  const char *current_text = NULL;
  char next_text[96] = {0};

  if (ui == NULL || ui->status_label == NULL || prefix == NULL)
  {
    return;
  }

  current_text = lv_label_get_text(ui->status_label);
  if (status == AD5593R_STATUS_OK)
  {
    if (current_text == NULL || strcmp(current_text, prefix) != 0)
    {
      lv_label_set_text(ui->status_label, prefix);
    }
  }
  else
  {
    lv_snprintf(next_text, sizeof(next_text), "%s: %s", prefix, lcd_ad5593r_status_text(status));
    if (current_text == NULL || strcmp(current_text, next_text) != 0)
    {
      lv_label_set_text(ui->status_label, next_text);
    }
  }
}

static void lcd_ad5593r_update_wave_ui(lcd_ad5593r_ui_t *ui,
                                       ad5593r_handle_t *ad5593r,
                                       uint16_t dac_raw,
                                       const ad5593r_channel_sample_t *adc_sample)
{
  uint16_t dac_millivolts = 0U;

  if (ui == NULL || ui->table == NULL || ad5593r == NULL)
  {
    return;
  }

  dac_millivolts = ad5593r_raw_to_millivolts(ad5593r, dac_raw);

  lv_table_set_cell_value_fmt(ui->table, 1U, 2U, "%u", (unsigned)dac_raw);
  lv_table_set_cell_value_fmt(ui->table,
                              1U,
                              3U,
                              "%u.%03u V",
                              (unsigned)(dac_millivolts / 1000U),
                              (unsigned)(dac_millivolts % 1000U));

  if (ui->chart != NULL && ui->dac_series != NULL)
  {
    lv_chart_set_next_value(ui->chart, ui->dac_series, dac_millivolts);
  }

  if (adc_sample != NULL && adc_sample->valid)
  {
    lv_table_set_cell_value_fmt(ui->table, 2U, 2U, "%u", (unsigned)adc_sample->raw);
    lv_table_set_cell_value_fmt(ui->table,
                                2U,
                                3U,
                                "%u.%03u V",
                                (unsigned)(adc_sample->millivolts / 1000U),
                                (unsigned)(adc_sample->millivolts % 1000U));
    lv_label_set_text_fmt(ui->value_label,
                          "IO0 DAC %u.%03u V    IO4 ADC %u.%03u V",
                          (unsigned)(dac_millivolts / 1000U),
                          (unsigned)(dac_millivolts % 1000U),
                          (unsigned)(adc_sample->millivolts / 1000U),
                          (unsigned)(adc_sample->millivolts % 1000U));

    if (ui->chart != NULL && ui->adc_series != NULL)
    {
      lv_chart_set_next_value(ui->chart, ui->adc_series, adc_sample->millivolts);
    }
  }
  else
  {
    lv_table_set_cell_value(ui->table, 2U, 2U, "--");
    lv_table_set_cell_value(ui->table, 2U, 3U, "--");
    lv_label_set_text_fmt(ui->value_label,
                          "IO0 DAC %u.%03u V    IO4 ADC --",
                          (unsigned)(dac_millivolts / 1000U),
                          (unsigned)(dac_millivolts % 1000U));

    if (ui->chart != NULL && ui->adc_series != NULL)
    {
      lv_chart_set_next_value(ui->chart, ui->adc_series, LV_CHART_POINT_NONE);
    }
  }
}

static ad5593r_status_t lcd_ad5593r_read_adc_channel(ad5593r_handle_t *ad5593r,
                                                     uint8_t channel,
                                                     ad5593r_channel_sample_t *sample)
{
  ad5593r_status_t status = AD5593R_STATUS_OK;

  if (ad5593r == NULL || sample == NULL)
  {
    return AD5593R_STATUS_ERROR_ARGUMENT;
  }

  for (uint8_t attempt = 0U; attempt < AD5593R_CHANNEL_COUNT; ++attempt)
  {
    status = ad5593r_read_adc(ad5593r, sample);
    if (status != AD5593R_STATUS_OK)
    {
      return status;
    }

    if (sample->valid && sample->channel == channel)
    {
      return AD5593R_STATUS_OK;
    }
  }

  sample->valid = false;
  return AD5593R_STATUS_ERROR_BAD_ADC_FRAME;
}

static void update_touch_debug_label(lv_obj_t *label)
{
  goodix_touch_diagnostics_t diag = {0};
  char text[160] = {0};

  if (label == NULL)
  {
    return;
  }

  goodix_touch_get_diagnostics(&diag);
  lv_snprintf(text,
              sizeof(text),
              "TP %s ADDR:%02X PID:%s STA:%02X\n"
              "XY:%u,%u RAW:%u,%u\n"
              "CNT:%lu ERR:%u",
              touch_lvgl_is_initialized() ? "OK" : "FAIL",
              diag.address,
              diag.pid,
              diag.status,
              diag.x,
              diag.y,
              diag.raw_x,
              diag.raw_y,
              (unsigned long)diag.read_count,
              diag.error);
  lv_label_set_text(label, text);
}

static const char *lcd_ad5593r_status_text(ad5593r_status_t status)
{
  switch (status)
  {
    case AD5593R_STATUS_OK:
      return "ok";
    case AD5593R_STATUS_ERROR_ARGUMENT:
      return "argument";
    case AD5593R_STATUS_ERROR_NOT_FOUND:
      return "scan failed";
    case AD5593R_STATUS_ERROR_I2C:
      return "i2c error";
    case AD5593R_STATUS_ERROR_BAD_CHANNEL:
      return "bad channel";
    case AD5593R_STATUS_ERROR_BAD_ADC_FRAME:
      return "bad adc frame";
    default:
      return "unknown";
  }
}
