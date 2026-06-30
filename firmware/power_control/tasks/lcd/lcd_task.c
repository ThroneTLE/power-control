#include "lcd_task.h"

#include "ad5593r.h"
#include "cmsis_os2.h"
#include "i2c.h"
#include "lcd_display.h"
#include "lvgl.h"

#define LCD_TASK_PERIOD_MS        5U
#define AD5593R_REFRESH_PERIOD_MS 200U
#define AD5593R_ADC_DISPLAY_COUNT 4U

typedef struct
{
  lv_obj_t *status_label;
  lv_obj_t *table;
} lcd_ad5593r_ui_t;

static void lcd_ad5593r_create_ui(lcd_ad5593r_ui_t *ui);
static void lcd_ad5593r_set_status(lcd_ad5593r_ui_t *ui, const char *prefix, ad5593r_status_t status);
static void lcd_ad5593r_refresh_table(lcd_ad5593r_ui_t *ui,
                                      ad5593r_handle_t *ad5593r,
                                      const ad5593r_channel_sample_t *adc_samples,
                                      size_t adc_sample_count);
static const char *lcd_ad5593r_status_text(ad5593r_status_t status);

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
      .dac_mask = AD5593R_DEFAULT_DAC_MASK,
      .adc_mask = AD5593R_DEFAULT_ADC_MASK,
  };
  ad5593r_channel_sample_t adc_samples[AD5593R_ADC_DISPLAY_COUNT] = {0};
  lcd_ad5593r_ui_t ui = {0};
  ad5593r_status_t ad5593r_status = AD5593R_STATUS_OK;
  bool ad5593r_ready = false;
  uint32_t refresh_elapsed_ms = AD5593R_REFRESH_PERIOD_MS;

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
    lcd_ad5593r_refresh_table(&ui, &ad5593r, adc_samples, 0U);
  }
  else
  {
    lcd_ad5593r_set_status(&ui, "AD5593R init failed", ad5593r_status);
  }

  for (;;)
  {
    if (ad5593r_ready && refresh_elapsed_ms >= AD5593R_REFRESH_PERIOD_MS)
    {
      size_t adc_sample_count = 0U;

      ad5593r_status = ad5593r_read_adc_sequence(&ad5593r,
                                                 adc_samples,
                                                 AD5593R_ADC_DISPLAY_COUNT,
                                                 &adc_sample_count);
      if (ad5593r_status == AD5593R_STATUS_OK)
      {
        lcd_ad5593r_set_status(&ui, "AD5593R ready", ad5593r_status);
        lcd_ad5593r_refresh_table(&ui, &ad5593r, adc_samples, adc_sample_count);
      }
      else
      {
        lcd_ad5593r_set_status(&ui, "AD5593R read failed", ad5593r_status);
      }

      refresh_elapsed_ms = 0U;
    }

    lv_tick_inc(LCD_TASK_PERIOD_MS);
    lv_timer_handler();
    osDelay(LCD_TASK_PERIOD_MS);
    refresh_elapsed_ms += LCD_TASK_PERIOD_MS;
  }
}

static void lcd_ad5593r_create_ui(lcd_ad5593r_ui_t *ui)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *title = NULL;

  if (ui == NULL)
  {
    return;
  }

  title = lv_label_create(screen);
  lv_label_set_text(title, "AD5593R Monitor");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  ui->status_label = lv_label_create(screen);
  lv_label_set_text(ui->status_label, "AD5593R starting...");
  lv_obj_align(ui->status_label, LV_ALIGN_TOP_MID, 0, 34);

  ui->table = lv_table_create(screen);
  lv_table_set_column_count(ui->table, 4U);
  lv_table_set_row_count(ui->table, AD5593R_CHANNEL_COUNT + 1U);
  lv_table_set_column_width(ui->table, 0U, 90);
  lv_table_set_column_width(ui->table, 1U, 110);
  lv_table_set_column_width(ui->table, 2U, 150);
  lv_table_set_column_width(ui->table, 3U, 160);
  lv_obj_set_size(ui->table, 560, 390);
  lv_obj_align(ui->table, LV_ALIGN_TOP_MID, 0, 76);

  lv_table_set_cell_value(ui->table, 0U, 0U, "CH");
  lv_table_set_cell_value(ui->table, 0U, 1U, "Mode");
  lv_table_set_cell_value(ui->table, 0U, 2U, "Raw");
  lv_table_set_cell_value(ui->table, 0U, 3U, "Voltage");

  for (uint8_t channel = 0U; channel < AD5593R_CHANNEL_COUNT; ++channel)
  {
    lv_table_set_cell_value_fmt(ui->table, (uint32_t)channel + 1U, 0U, "IO%u", channel);
    lv_table_set_cell_value(ui->table,
                            (uint32_t)channel + 1U,
                            1U,
                            channel < 4U ? "DAC" : "ADC");
    lv_table_set_cell_value(ui->table, (uint32_t)channel + 1U, 2U, "--");
    lv_table_set_cell_value(ui->table, (uint32_t)channel + 1U, 3U, "--");
  }
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

static void lcd_ad5593r_refresh_table(lcd_ad5593r_ui_t *ui,
                                      ad5593r_handle_t *ad5593r,
                                      const ad5593r_channel_sample_t *adc_samples,
                                      size_t adc_sample_count)
{
  if (ui == NULL || ui->table == NULL || ad5593r == NULL)
  {
    return;
  }

  for (uint8_t channel = 0U; channel < AD5593R_CHANNEL_COUNT; ++channel)
  {
    const uint32_t row = (uint32_t)channel + 1U;

    if ((ad5593r->dac_mask & (uint8_t)(1U << channel)) != 0U)
    {
      const uint16_t raw = ad5593r->dac_raw[channel];
      const uint16_t millivolts = ad5593r_raw_to_millivolts(ad5593r, raw);

      lv_table_set_cell_value(ui->table, row, 1U, "DAC");
      lv_table_set_cell_value_fmt(ui->table, row, 2U, "%u", raw);
      lv_table_set_cell_value_fmt(ui->table, row, 3U, "%u.%03u V", millivolts / 1000U, millivolts % 1000U);
    }
    else if ((ad5593r->adc_mask & (uint8_t)(1U << channel)) != 0U)
    {
      const ad5593r_channel_sample_t *sample = NULL;

      for (size_t i = 0U; i < adc_sample_count; ++i)
      {
        if (adc_samples[i].valid && adc_samples[i].channel == channel)
        {
          sample = &adc_samples[i];
          break;
        }
      }

      lv_table_set_cell_value(ui->table, row, 1U, "ADC");
      if (sample != NULL)
      {
        lv_table_set_cell_value_fmt(ui->table, row, 2U, "%u", sample->raw);
        lv_table_set_cell_value_fmt(ui->table,
                                    row,
                                    3U,
                                    "%u.%03u V",
                                    sample->millivolts / 1000U,
                                    sample->millivolts % 1000U);
      }
      else
      {
        lv_table_set_cell_value(ui->table, row, 2U, "--");
        lv_table_set_cell_value(ui->table, row, 3U, "--");
      }
    }
    else
    {
      lv_table_set_cell_value(ui->table, row, 1U, "OFF");
      lv_table_set_cell_value(ui->table, row, 2U, "--");
      lv_table_set_cell_value(ui->table, row, 3U, "--");
    }
  }
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
