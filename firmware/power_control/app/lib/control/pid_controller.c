#include "pid_controller.h"

#include <math.h>
#include <string.h>

static float pid_clamp(float value, float min_value, float max_value);
static float pid_apply_rate_limit(float target, float current, float rate_limit, float dt_s, bool *limited);
static bool pid_config_is_valid(const pid_controller_config_t *config);

bool pid_controller_init(pid_controller_t *controller, const pid_controller_config_t *config)
{
  if (controller == NULL || !pid_config_is_valid(config))
  {
    return false;
  }

  memset(controller, 0, sizeof(*controller));
  controller->config = *config;
  controller->state.output = pid_clamp(0.0f, config->output_min, config->output_max);
  controller->state.active_setpoint = 0.0f;
  return true;
}

void pid_controller_reset(pid_controller_t *controller, float output, float measurement, float setpoint)
{
  if (controller == NULL)
  {
    return;
  }

  controller->state.active_setpoint = setpoint;
  controller->state.error = 0.0f;
  controller->state.proportional = 0.0f;
  controller->state.integral = 0.0f;
  controller->state.derivative = 0.0f;
  controller->state.output = pid_clamp(output, controller->config.output_min, controller->config.output_max);
  controller->state.flags = PID_CONTROLLER_FLAG_NONE;
  controller->previous_measurement = measurement;
  controller->filtered_derivative = 0.0f;
  controller->has_previous_measurement = true;
}

float pid_controller_update(pid_controller_t *controller, float setpoint, float measurement, float dt_s)
{
  bool setpoint_limited = false;
  bool output_rate_limited = false;
  float derivative = 0.0f;
  float unsaturated_output = 0.0f;
  float saturated_output = 0.0f;
  float output = 0.0f;

  if (controller == NULL || dt_s <= 0.0f)
  {
    return 0.0f;
  }

  controller->state.flags = PID_CONTROLLER_FLAG_NONE;
  controller->state.active_setpoint =
      pid_apply_rate_limit(setpoint,
                           controller->state.active_setpoint,
                           controller->config.setpoint_rate_limit,
                           dt_s,
                           &setpoint_limited);
  if (setpoint_limited)
  {
    controller->state.flags |= PID_CONTROLLER_FLAG_SETPOINT_RATE_LIMITED;
  }

  controller->state.error = controller->state.active_setpoint - measurement;
  controller->state.proportional = controller->config.kp * controller->state.error;
  controller->state.integral += controller->state.error * dt_s;
  controller->state.integral =
      pid_clamp(controller->state.integral, controller->config.integral_min, controller->config.integral_max);

  if (controller->has_previous_measurement)
  {
    derivative = -(measurement - controller->previous_measurement) / dt_s;
  }
  else
  {
    derivative = 0.0f;
    controller->has_previous_measurement = true;
  }

  if (controller->config.derivative_filter_tau > 0.0f)
  {
    const float alpha = dt_s / (controller->config.derivative_filter_tau + dt_s);
    controller->filtered_derivative += alpha * (derivative - controller->filtered_derivative);
    derivative = controller->filtered_derivative;
  }
  else
  {
    controller->filtered_derivative = derivative;
  }

  controller->previous_measurement = measurement;
  controller->state.derivative = controller->config.kd * derivative;

  unsaturated_output = controller->state.proportional + (controller->config.ki * controller->state.integral) +
                       controller->state.derivative;
  saturated_output = pid_clamp(unsaturated_output, controller->config.output_min, controller->config.output_max);
  if (saturated_output != unsaturated_output || saturated_output <= controller->config.output_min ||
      saturated_output >= controller->config.output_max)
  {
    controller->state.flags |= PID_CONTROLLER_FLAG_OUTPUT_SATURATED;
  }

  output = pid_apply_rate_limit(saturated_output,
                                controller->state.output,
                                controller->config.output_rate_limit,
                                dt_s,
                                &output_rate_limited);
  if (output_rate_limited)
  {
    controller->state.flags |= PID_CONTROLLER_FLAG_OUTPUT_RATE_LIMITED;
  }

  output = pid_clamp(output, controller->config.output_min, controller->config.output_max);
  controller->state.output = output;
  return output;
}

pid_controller_state_t pid_controller_get_state(const pid_controller_t *controller)
{
  pid_controller_state_t empty_state = {0};

  if (controller == NULL)
  {
    return empty_state;
  }

  return controller->state;
}

static float pid_clamp(float value, float min_value, float max_value)
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

static float pid_apply_rate_limit(float target, float current, float rate_limit, float dt_s, bool *limited)
{
  float max_delta = 0.0f;

  if (limited != NULL)
  {
    *limited = false;
  }

  if (rate_limit <= 0.0f || dt_s <= 0.0f)
  {
    return target;
  }

  max_delta = rate_limit * dt_s;
  if ((target - current) > max_delta)
  {
    if (limited != NULL)
    {
      *limited = true;
    }
    return current + max_delta;
  }

  if ((current - target) > max_delta)
  {
    if (limited != NULL)
    {
      *limited = true;
    }
    return current - max_delta;
  }

  return target;
}

static bool pid_config_is_valid(const pid_controller_config_t *config)
{
  if (config == NULL)
  {
    return false;
  }

  if (!isfinite(config->kp) || !isfinite(config->ki) || !isfinite(config->kd))
  {
    return false;
  }

  if (!isfinite(config->output_min) || !isfinite(config->output_max) || config->output_min > config->output_max)
  {
    return false;
  }

  if (!isfinite(config->integral_min) || !isfinite(config->integral_max) ||
      config->integral_min > config->integral_max)
  {
    return false;
  }

  return config->setpoint_rate_limit >= 0.0f && config->output_rate_limit >= 0.0f &&
         config->derivative_filter_tau >= 0.0f;
}
