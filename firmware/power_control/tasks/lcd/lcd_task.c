#include "lcd_task.h"

#include "ad5593r.h"
#include "cmsis_os2.h"
#include "i2c.h"
#include "lcd_display.h"
#include "lvgl.h"

#define LCD_TASK_PERIOD_MS       5U
#define AD5593R_WAVE_PERIOD_MS   20U
#define AD5593R_CHART_POINTS     128U
#define AD5593R_WAVE_POINTS      64U
#define AD5593R_TEST_DAC_CHANNEL 0U
#define AD5593R_TEST_ADC_CHANNEL 4U
#define AD5593R_TEST_DAC_MASK    (1U << AD5593R_TEST_DAC_CHANNEL)
#define AD5593R_TEST_ADC_MASK    (1U << AD5593R_TEST_ADC_CHANNEL)

typedef struct
{
  lv_obj_t *status_label;
  lv_obj_t *value_label;
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
  ad5593r_handle_t ad5593r = {0};
  const ad5593r_config_t ad5593r_config = {
      .i2c = &hi2c1,
      .timeout_ms = AD5593R_DEFAULT_TIMEOUT_MS,
      .vref_mv = AD5593R_DEFAULT_VREF_MV,
      .dac_mask = AD5593R_TEST_DAC_MASK,
      .adc_mask = AD5593R_TEST_ADC_MASK,
  };
  ad5593r_channel_sample_t adc_sample = {0};
  lcd_ad5593r_ui_t ui = {0};
  ad5593r_status_t ad5593r_status = AD5593R_STATUS_OK;
  bool ad5593r_ready = false;
  uint32_t wave_elapsed_ms = AD5593R_WAVE_PERIOD_MS;
  uint32_t wave_index = 0U;
  uint16_t dac_raw = lcd_ad5593r_sine_raw[0];

  (void)argument;

  lv_init();
  lcd_display_reset();
  lcd_display_clear(0x0000U);
  if (lcd_display_lvgl_init() == NULL)
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

    lv_tick_inc(LCD_TASK_PERIOD_MS);
    lv_timer_handler();
    osDelay(LCD_TASK_PERIOD_MS);
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
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  ui->status_label = lv_label_create(screen);
  lv_label_set_text(ui->status_label, "AD5593R starting...");
  lv_obj_align(ui->status_label, LV_ALIGN_TOP_MID, 0, 34);

  ui->value_label = lv_label_create(screen);
  lv_label_set_text(ui->value_label, "IO0 DAC -- | IO4 ADC --");
  lv_obj_align(ui->value_label, LV_ALIGN_TOP_MID, 0, 58);

  ui->chart = lv_chart_create(screen);
  lv_obj_set_size(ui->chart, 760, 280);
  lv_obj_align(ui->chart, LV_ALIGN_TOP_MID, 0, 88);
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
  lv_label_set_text(legend, "Orange: IO0 DAC output    Blue: IO4 ADC input");
  lv_obj_align(legend, LV_ALIGN_TOP_MID, 0, 372);

  ui->table = lv_table_create(screen);
  lv_table_set_column_count(ui->table, 4U);
  lv_table_set_row_count(ui->table, 3U);
  lv_table_set_column_width(ui->table, 0U, 120);
  lv_table_set_column_width(ui->table, 1U, 160);
  lv_table_set_column_width(ui->table, 2U, 180);
  lv_table_set_column_width(ui->table, 3U, 220);
  lv_obj_set_size(ui->table, 700, 82);
  lv_obj_align(ui->table, LV_ALIGN_TOP_MID, 0, 396);

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
  if (ui == NULL || ui->status_label == NULL || prefix == NULL)
  {
    return;
  }

  if (status == AD5593R_STATUS_OK)
  {
    lv_label_set_text(ui->status_label, prefix);
  }
  else
  {
    lv_label_set_text_fmt(ui->status_label, "%s: %s", prefix, lcd_ad5593r_status_text(status));
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
