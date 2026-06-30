#include "control_task.h"

#include "ad5593r.h"
#include "control_context.h"
#include "cmsis_os2.h"
#include "i2c.h"
#include "pid_controller.h"

#define CONTROL_TASK_PERIOD_MS 10U
#define CONTROL_TASK_PERIOD_S  0.010f

#define CONTROL_DAC_CHANNEL 0U
#define CONTROL_ADC_CHANNEL 4U
#define CONTROL_DAC_MASK    (1U << CONTROL_DAC_CHANNEL)
#define CONTROL_ADC_MASK    (1U << CONTROL_ADC_CHANNEL)

#define CONTROL_REFERENCE_MIN_KV 0.0f
#define CONTROL_REFERENCE_MAX_KV 5.0f
#define CONTROL_DAC_MIN_VOLTS    0.0f
#define CONTROL_DAC_MAX_VOLTS    5.0f
#define CONTROL_ADC_MAX_KV       5.0f

#define CONTROL_PID_KP 0.8f
#define CONTROL_PID_KI 0.4f
#define CONTROL_PID_KD 0.0f
#define CONTROL_PID_INTEGRAL_LIMIT (CONTROL_DAC_MAX_VOLTS / CONTROL_PID_KI)

static bool control_task_init_ad5593r(ad5593r_handle_t *ad5593r);
static bool control_task_init_pid(pid_controller_t *pid);
static bool control_task_read_feedback(ad5593r_handle_t *ad5593r, float *feedback_kv);
static uint16_t control_task_volts_to_raw(float volts);
static float control_task_millivolts_to_feedback_kv(uint16_t millivolts);
static float control_task_clamp(float value, float min_value, float max_value);

void start_control_task(void *argument)
{
  control_task_entry(argument);
}

void control_task_entry(void *argument)
{
  ad5593r_handle_t ad5593r = {0};
  pid_controller_t pid = {0};
  uint32_t fault_flags = CONTROL_FAULT_NONE;
  float feedback_kv = 0.0f;
  float dac_volts = 0.0f;
  uint16_t dac_raw = 0U;

  (void)argument;

  if (!control_context_init())
  {
    fault_flags |= CONTROL_FAULT_CONTEXT;
  }

  if (!control_task_init_pid(&pid))
  {
    fault_flags |= CONTROL_FAULT_PID;
  }

  if (!control_task_init_ad5593r(&ad5593r))
  {
    fault_flags |= CONTROL_FAULT_AD5593R_INIT;
  }

  control_set_loop_enabled(fault_flags == CONTROL_FAULT_NONE);
  control_update_snapshot(feedback_kv, dac_volts, fault_flags);

  for (;;)
  {
    if (fault_flags == CONTROL_FAULT_NONE && control_get_loop_enabled())
    {
      const float reference_kv =
          control_task_clamp(control_get_reference_kv(), CONTROL_REFERENCE_MIN_KV, CONTROL_REFERENCE_MAX_KV);

      if (control_task_read_feedback(&ad5593r, &feedback_kv))
      {
        dac_volts = pid_controller_update(&pid, reference_kv, feedback_kv, CONTROL_TASK_PERIOD_S);
        dac_volts = control_task_clamp(dac_volts, CONTROL_DAC_MIN_VOLTS, CONTROL_DAC_MAX_VOLTS);
        dac_raw = control_task_volts_to_raw(dac_volts);

        if (ad5593r_write_dac(&ad5593r, CONTROL_DAC_CHANNEL, dac_raw) != AD5593R_STATUS_OK)
        {
          fault_flags |= CONTROL_FAULT_AD5593R_IO;
          control_set_loop_enabled(false);
        }
      }
      else
      {
        fault_flags |= CONTROL_FAULT_AD5593R_IO;
        control_set_loop_enabled(false);
      }
    }
    else
    {
      dac_volts = CONTROL_DAC_MIN_VOLTS;
      if (ad5593r.address_7bit != 0U)
      {
        (void)ad5593r_write_dac(&ad5593r, CONTROL_DAC_CHANNEL, 0U);
      }
    }

    control_update_snapshot(feedback_kv, dac_volts, fault_flags);
    osDelay(CONTROL_TASK_PERIOD_MS);
  }
}

static bool control_task_init_ad5593r(ad5593r_handle_t *ad5593r)
{
  const ad5593r_config_t config = {
      .i2c = &hi2c1,
      .timeout_ms = AD5593R_DEFAULT_TIMEOUT_MS,
      .vref_mv = AD5593R_DEFAULT_VREF_MV,
      .dac_mask = CONTROL_DAC_MASK,
      .adc_mask = CONTROL_ADC_MASK,
  };

  if (ad5593r_init(ad5593r, &config) != AD5593R_STATUS_OK)
  {
    return false;
  }

  return ad5593r_write_dac(ad5593r, CONTROL_DAC_CHANNEL, 0U) == AD5593R_STATUS_OK;
}

static bool control_task_init_pid(pid_controller_t *pid)
{
  const pid_controller_config_t config = {
      .kp = CONTROL_PID_KP,
      .ki = CONTROL_PID_KI,
      .kd = CONTROL_PID_KD,
      .output_min = CONTROL_DAC_MIN_VOLTS,
      .output_max = CONTROL_DAC_MAX_VOLTS,
      .integral_min = -CONTROL_PID_INTEGRAL_LIMIT,
      .integral_max = CONTROL_PID_INTEGRAL_LIMIT,
      .setpoint_rate_limit = 1.0f,
      .output_rate_limit = 2.0f,
      .derivative_filter_tau = 0.0f,
  };

  if (!pid_controller_init(pid, &config))
  {
    return false;
  }

  pid_controller_reset(pid, CONTROL_DAC_MIN_VOLTS, 0.0f, 0.0f);
  return true;
}

static bool control_task_read_feedback(ad5593r_handle_t *ad5593r, float *feedback_kv)
{
  ad5593r_channel_sample_t sample = {0};

  if (ad5593r == NULL || feedback_kv == NULL)
  {
    return false;
  }

  for (uint8_t attempt = 0U; attempt < AD5593R_CHANNEL_COUNT; ++attempt)
  {
    if (ad5593r_read_adc(ad5593r, &sample) != AD5593R_STATUS_OK)
    {
      return false;
    }

    if (sample.valid && sample.channel == CONTROL_ADC_CHANNEL)
    {
      *feedback_kv = control_task_millivolts_to_feedback_kv(sample.millivolts);
      return true;
    }
  }

  return false;
}

static uint16_t control_task_volts_to_raw(float volts)
{
  const float clamped = control_task_clamp(volts, CONTROL_DAC_MIN_VOLTS, CONTROL_DAC_MAX_VOLTS);
  return (uint16_t)(((clamped * (float)AD5593R_ADC_RAW_MAX) / CONTROL_DAC_MAX_VOLTS) + 0.5f);
}

static float control_task_millivolts_to_feedback_kv(uint16_t millivolts)
{
  const float volts = (float)millivolts / 1000.0f;
  return control_task_clamp((volts / CONTROL_DAC_MAX_VOLTS) * CONTROL_ADC_MAX_KV,
                            CONTROL_REFERENCE_MIN_KV,
                            CONTROL_REFERENCE_MAX_KV);
}

static float control_task_clamp(float value, float min_value, float max_value)
{
  if (value < min_value)
  {
    return min_value;
  }

  if (value > max_value)
  {
    return max_value;
  }

  return value;
}
