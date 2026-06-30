#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

enum
{
  PID_CONTROLLER_FLAG_NONE = 0U,
  PID_CONTROLLER_FLAG_OUTPUT_SATURATED = 1U << 0U,
  PID_CONTROLLER_FLAG_SETPOINT_RATE_LIMITED = 1U << 1U,
  PID_CONTROLLER_FLAG_OUTPUT_RATE_LIMITED = 1U << 2U
};

typedef struct
{
  float kp;
  float ki;
  float kd;
  float output_min;
  float output_max;
  float integral_min;
  float integral_max;
  float setpoint_rate_limit;
  float output_rate_limit;
  float derivative_filter_tau;
} pid_controller_config_t;

typedef struct
{
  float active_setpoint;
  float error;
  float proportional;
  float integral;
  float derivative;
  float output;
  uint32_t flags;
} pid_controller_state_t;

typedef struct
{
  pid_controller_config_t config;
  pid_controller_state_t state;
  float previous_measurement;
  float filtered_derivative;
  bool has_previous_measurement;
} pid_controller_t;

bool pid_controller_init(pid_controller_t *controller, const pid_controller_config_t *config);
void pid_controller_reset(pid_controller_t *controller, float output, float measurement, float setpoint);
float pid_controller_update(pid_controller_t *controller, float setpoint, float measurement, float dt_s);
pid_controller_state_t pid_controller_get_state(const pid_controller_t *controller);

#ifdef __cplusplus
}
#endif

#endif /* PID_CONTROLLER_H */
