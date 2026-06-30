#include "lcd_task.h"

#include "control_context.h"
#include "cmsis_os2.h"
#include "goodix_touch.h"
#include "lcd_display.h"
#include "lvgl.h"
#include "touch_lvgl.h"

#include <stdbool.h>
#include <stdint.h>

#define LCD_TASK_PERIOD_MS    2U
#define TOUCH_INIT_DELAY_MS   500U
#define TOUCH_INIT_RETRY_MS   500U
#define TOUCH_DEBUG_PERIOD_MS 50U
#define UI_UPDATE_PERIOD_MS   100U

#define CONTROL_REFERENCE_STEP_CENTIKV 10
#define CONTROL_REFERENCE_MIN_CENTIKV  0
#define CONTROL_REFERENCE_MAX_CENTIKV  500
#define CONTROL_CHART_POINTS           120U

typedef struct
{
  lv_obj_t *status_label;
  lv_obj_t *reference_label;
  lv_obj_t *feedback_label;
  lv_obj_t *dac_label;
  lv_obj_t *fault_label;
  lv_obj_t *touch_debug_label;
  lv_obj_t *reference_slider;
  lv_obj_t *enable_switch;
  lv_obj_t *chart;
  lv_chart_series_t *reference_series;
  lv_chart_series_t *feedback_series;
} lcd_control_ui_t;

static void lcd_control_create_ui(lcd_control_ui_t *ui);
static void lcd_control_update_ui(lcd_control_ui_t *ui, const control_snapshot_t *snapshot);
static void lcd_control_reference_slider_event(lv_event_t *event);
static void lcd_control_reference_step_event(lv_event_t *event);
static void lcd_control_enable_event(lv_event_t *event);
static lv_obj_t *lcd_control_create_step_button(lv_obj_t *parent,
                                                const char *text,
                                                int32_t x,
                                                int32_t y,
                                                int32_t step_centikv);
static void update_touch_debug_label(lv_obj_t *label);
static int32_t lcd_control_kv_to_centikv(float kv);
static float lcd_control_centikv_to_kv(int32_t centikv);
static int32_t lcd_control_clamp_centikv(int32_t centikv);
static int32_t lcd_control_volts_to_millivolts(float volts);

void start_lcd_task(void *argument)
{
  lcd_task_entry(argument);
}

void lcd_task_entry(void *argument)
{
  lv_display_t *display = NULL;
  lcd_control_ui_t ui = {0};
  control_snapshot_t snapshot = {0};
  bool touch_initialized = false;
  uint32_t elapsed_ms = 0U;
  uint32_t last_touch_init_attempt_ms = 0U;
  uint32_t last_debug_update_ms = 0U;
  uint32_t last_ui_update_ms = 0U;

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

  lcd_control_create_ui(&ui);
  lcd_display_backlight_on();

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

    if ((elapsed_ms - last_ui_update_ms) >= UI_UPDATE_PERIOD_MS)
    {
      if (control_get_snapshot(&snapshot))
      {
        lcd_control_update_ui(&ui, &snapshot);
      }
      last_ui_update_ms = elapsed_ms;
    }

    osDelay(LCD_TASK_PERIOD_MS);
    elapsed_ms += LCD_TASK_PERIOD_MS;
  }
}

static void lcd_control_create_ui(lcd_control_ui_t *ui)
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_t *title = NULL;
  lv_obj_t *legend = NULL;
  lv_obj_t *slider_label = NULL;
  lv_obj_t *enable_label = NULL;

  if (ui == NULL)
  {
    return;
  }

  title = lv_label_create(screen);
  lv_label_set_text(title, "Power Voltage Closed Loop");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 4);

  ui->status_label = lv_label_create(screen);
  lv_obj_set_style_text_font(ui->status_label, &lv_font_montserrat_16, 0);
  lv_label_set_text(ui->status_label, "Control task starting");
  lv_obj_align(ui->status_label, LV_ALIGN_TOP_LEFT, 16, 34);

  ui->touch_debug_label = lv_label_create(screen);
  lv_label_set_long_mode(ui->touch_debug_label, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(ui->touch_debug_label, 310);
  lv_obj_set_style_text_font(ui->touch_debug_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(ui->touch_debug_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(ui->touch_debug_label, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_label_set_text(ui->touch_debug_label, "TP: waiting");

  ui->reference_label = lv_label_create(screen);
  lv_obj_set_style_text_font(ui->reference_label, &lv_font_montserrat_24, 0);
  lv_label_set_text(ui->reference_label, "Set 0.00 kV");
  lv_obj_align(ui->reference_label, LV_ALIGN_TOP_LEFT, 16, 70);

  ui->feedback_label = lv_label_create(screen);
  lv_obj_set_style_text_font(ui->feedback_label, &lv_font_montserrat_24, 0);
  lv_label_set_text(ui->feedback_label, "Current 0.000 V");
  lv_obj_align(ui->feedback_label, LV_ALIGN_TOP_LEFT, 260, 70);

  ui->dac_label = lv_label_create(screen);
  lv_obj_set_style_text_font(ui->dac_label, &lv_font_montserrat_20, 0);
  lv_label_set_text(ui->dac_label, "Control Output 0.000 V");
  lv_obj_align(ui->dac_label, LV_ALIGN_TOP_LEFT, 540, 74);

  slider_label = lv_label_create(screen);
  lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_16, 0);
  lv_label_set_text(slider_label, "Reference");
  lv_obj_align(slider_label, LV_ALIGN_TOP_LEFT, 16, 116);

  ui->reference_slider = lv_slider_create(screen);
  lv_obj_set_size(ui->reference_slider, 430, 24);
  lv_obj_align(ui->reference_slider, LV_ALIGN_TOP_LEFT, 120, 116);
  lv_slider_set_range(ui->reference_slider, CONTROL_REFERENCE_MIN_CENTIKV, CONTROL_REFERENCE_MAX_CENTIKV);
  lv_slider_set_value(ui->reference_slider, CONTROL_REFERENCE_MIN_CENTIKV, LV_ANIM_OFF);
  lv_obj_add_event_cb(ui->reference_slider, lcd_control_reference_slider_event, LV_EVENT_VALUE_CHANGED, NULL);

  (void)lcd_control_create_step_button(screen, "-0.10", 572, 104, -CONTROL_REFERENCE_STEP_CENTIKV);
  (void)lcd_control_create_step_button(screen, "+0.10", 668, 104, CONTROL_REFERENCE_STEP_CENTIKV);

  enable_label = lv_label_create(screen);
  lv_obj_set_style_text_font(enable_label, &lv_font_montserrat_16, 0);
  lv_label_set_text(enable_label, "Loop");
  lv_obj_align(enable_label, LV_ALIGN_TOP_LEFT, 16, 156);

  ui->enable_switch = lv_switch_create(screen);
  lv_obj_align(ui->enable_switch, LV_ALIGN_TOP_LEFT, 120, 148);
  lv_obj_add_state(ui->enable_switch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(ui->enable_switch, lcd_control_enable_event, LV_EVENT_VALUE_CHANGED, NULL);

  ui->fault_label = lv_label_create(screen);
  lv_obj_set_style_text_font(ui->fault_label, &lv_font_montserrat_16, 0);
  lv_label_set_text(ui->fault_label, "Fault none");
  lv_obj_align(ui->fault_label, LV_ALIGN_TOP_LEFT, 220, 154);

  ui->chart = lv_chart_create(screen);
  lv_obj_set_size(ui->chart, 760, 250);
  lv_obj_align(ui->chart, LV_ALIGN_TOP_MID, 0, 196);
  lv_chart_set_type(ui->chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(ui->chart, CONTROL_CHART_POINTS);
  lv_chart_set_update_mode(ui->chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_axis_range(ui->chart, LV_CHART_AXIS_PRIMARY_Y, CONTROL_REFERENCE_MIN_CENTIKV, CONTROL_REFERENCE_MAX_CENTIKV);
  lv_chart_set_div_line_count(ui->chart, 6U, 8U);
  lv_obj_set_style_radius(ui->chart, 4, 0);
  lv_obj_set_style_border_width(ui->chart, 1, 0);
  lv_obj_set_style_pad_all(ui->chart, 8, 0);

  ui->reference_series = lv_chart_add_series(ui->chart, lv_color_hex(0xE67E22), LV_CHART_AXIS_PRIMARY_Y);
  ui->feedback_series = lv_chart_add_series(ui->chart, lv_color_hex(0x1F77B4), LV_CHART_AXIS_PRIMARY_Y);
  if (ui->reference_series != NULL)
  {
    lv_chart_set_all_values(ui->chart, ui->reference_series, LV_CHART_POINT_NONE);
  }
  if (ui->feedback_series != NULL)
  {
    lv_chart_set_all_values(ui->chart, ui->feedback_series, LV_CHART_POINT_NONE);
  }

  legend = lv_label_create(screen);
  lv_obj_set_style_text_font(legend, &lv_font_montserrat_16, 0);
  lv_label_set_text(legend, "Orange: reference kV    Blue: feedback kV");
  lv_obj_align(legend, LV_ALIGN_TOP_MID, 0, 452);
}

static void lcd_control_update_ui(lcd_control_ui_t *ui, const control_snapshot_t *snapshot)
{
  int32_t reference_centikv = 0;
  int32_t feedback_centikv = 0;
  int32_t current_millivolts = 0;
  int32_t dac_millivolts = 0;

  if (ui == NULL || snapshot == NULL)
  {
    return;
  }

  reference_centikv = lcd_control_kv_to_centikv(snapshot->reference_kv);
  feedback_centikv = lcd_control_kv_to_centikv(snapshot->feedback_kv);
  current_millivolts = lcd_control_volts_to_millivolts(snapshot->feedback_kv * 1000.0f);
  dac_millivolts = lcd_control_volts_to_millivolts(snapshot->dac_volts);

  if (ui->status_label != NULL)
  {
    lv_label_set_text(ui->status_label, snapshot->loop_enabled ? "Control loop enabled" : "Control loop stopped");
  }

  if (ui->reference_label != NULL)
  {
    lv_label_set_text_fmt(ui->reference_label,
                          "Set %ld.%02ld kV",
                          (long)(reference_centikv / 100),
                          (long)(reference_centikv % 100));
  }

  if (ui->feedback_label != NULL)
  {
    lv_label_set_text_fmt(ui->feedback_label,
                          "Current %ld.%03ld V",
                          (long)(current_millivolts / 1000),
                          (long)(current_millivolts % 1000));
  }

  if (ui->dac_label != NULL)
  {
    lv_label_set_text_fmt(ui->dac_label,
                          "Control Output %ld.%03ld V",
                          (long)(dac_millivolts / 1000),
                          (long)(dac_millivolts % 1000));
  }

  if (ui->fault_label != NULL)
  {
    if (snapshot->fault_flags == CONTROL_FAULT_NONE)
    {
      lv_label_set_text(ui->fault_label, "Fault none");
    }
    else
    {
      lv_label_set_text_fmt(ui->fault_label, "Fault 0x%08lx", (unsigned long)snapshot->fault_flags);
    }
  }

  if (ui->reference_slider != NULL && lv_slider_get_value(ui->reference_slider) != reference_centikv)
  {
    lv_slider_set_value(ui->reference_slider, reference_centikv, LV_ANIM_OFF);
  }

  if (ui->enable_switch != NULL)
  {
    if (snapshot->loop_enabled)
    {
      lv_obj_add_state(ui->enable_switch, LV_STATE_CHECKED);
    }
    else
    {
      lv_obj_remove_state(ui->enable_switch, LV_STATE_CHECKED);
    }
  }

  if (ui->chart != NULL && ui->reference_series != NULL)
  {
    lv_chart_set_next_value(ui->chart, ui->reference_series, reference_centikv);
  }
  if (ui->chart != NULL && ui->feedback_series != NULL)
  {
    lv_chart_set_next_value(ui->chart, ui->feedback_series, feedback_centikv);
  }
}

static void lcd_control_reference_slider_event(lv_event_t *event)
{
  lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(event);

  if (slider == NULL)
  {
    return;
  }

  control_set_reference_kv(lcd_control_centikv_to_kv(lv_slider_get_value(slider)));
}

static void lcd_control_reference_step_event(lv_event_t *event)
{
  const int32_t step = (int32_t)(intptr_t)lv_event_get_user_data(event);
  const int32_t current = lcd_control_kv_to_centikv(control_get_reference_kv());
  control_set_reference_kv(lcd_control_centikv_to_kv(lcd_control_clamp_centikv(current + step)));
}

static void lcd_control_enable_event(lv_event_t *event)
{
  lv_obj_t *switch_obj = (lv_obj_t *)lv_event_get_target(event);

  if (switch_obj == NULL)
  {
    return;
  }

  control_set_loop_enabled(lv_obj_has_state(switch_obj, LV_STATE_CHECKED));
}

static lv_obj_t *lcd_control_create_step_button(lv_obj_t *parent,
                                                const char *text,
                                                int32_t x,
                                                int32_t y,
                                                int32_t step_centikv)
{
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label = NULL;

  lv_obj_set_size(button, 76, 44);
  lv_obj_align(button, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_add_event_cb(button, lcd_control_reference_step_event, LV_EVENT_CLICKED, (void *)(intptr_t)step_centikv);

  label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_center(label);

  return button;
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

static int32_t lcd_control_kv_to_centikv(float kv)
{
  return lcd_control_clamp_centikv((int32_t)((kv * 100.0f) + 0.5f));
}

static float lcd_control_centikv_to_kv(int32_t centikv)
{
  return (float)lcd_control_clamp_centikv(centikv) / 100.0f;
}

static int32_t lcd_control_clamp_centikv(int32_t centikv)
{
  if (centikv < CONTROL_REFERENCE_MIN_CENTIKV)
  {
    return CONTROL_REFERENCE_MIN_CENTIKV;
  }

  if (centikv > CONTROL_REFERENCE_MAX_CENTIKV)
  {
    return CONTROL_REFERENCE_MAX_CENTIKV;
  }

  return centikv;
}

static int32_t lcd_control_volts_to_millivolts(float volts)
{
  if (volts < 0.0f)
  {
    return 0;
  }

  return (int32_t)((volts * 1000.0f) + 0.5f);
}
