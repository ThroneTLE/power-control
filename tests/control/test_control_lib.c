#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "pid_controller.h"
#include "rls_estimator.h"

#define ASSERT_TRUE(expr)                                                                 \
  do                                                                                      \
  {                                                                                       \
    if (!(expr))                                                                          \
    {                                                                                     \
      fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);    \
      exit(1);                                                                            \
    }                                                                                     \
  } while (0)

#define ASSERT_NEAR(actual, expected, tolerance)                                                    \
  do                                                                                                \
  {                                                                                                 \
    const float actual_value = (actual);                                                            \
    const float expected_value = (expected);                                                        \
    const float tolerance_value = (tolerance);                                                      \
    if (fabsf(actual_value - expected_value) > tolerance_value)                                      \
    {                                                                                               \
      fprintf(stderr,                                                                                \
              "ASSERT_NEAR failed at %s:%d: got %.6f expected %.6f tolerance %.6f\n",               \
              __FILE__,                                                                             \
              __LINE__,                                                                             \
              (double)actual_value,                                                                 \
              (double)expected_value,                                                               \
              (double)tolerance_value);                                                             \
      exit(1);                                                                                      \
    }                                                                                               \
  } while (0)

static void test_pid_clamps_output_and_integral(void)
{
  pid_controller_t pid;
  const pid_controller_config_t config = {
      .kp = 0.0f,
      .ki = 10.0f,
      .kd = 0.0f,
      .output_min = 0.0f,
      .output_max = 5.0f,
      .integral_min = -0.5f,
      .integral_max = 0.5f,
      .setpoint_rate_limit = 0.0f,
      .output_rate_limit = 0.0f,
      .derivative_filter_tau = 0.0f,
  };

  ASSERT_TRUE(pid_controller_init(&pid, &config));

  for (int i = 0; i < 20; ++i)
  {
    (void)pid_controller_update(&pid, 5.0f, 0.0f, 0.1f);
  }

  const pid_controller_state_t state = pid_controller_get_state(&pid);
  ASSERT_NEAR(state.integral, 0.5f, 0.0001f);
  ASSERT_NEAR(state.output, 5.0f, 0.0001f);
  ASSERT_TRUE((state.flags & PID_CONTROLLER_FLAG_OUTPUT_SATURATED) != 0U);
}

static void test_pid_derivative_on_measurement_avoids_setpoint_kick(void)
{
  pid_controller_t pid;
  const pid_controller_config_t config = {
      .kp = 0.0f,
      .ki = 0.0f,
      .kd = 1.0f,
      .output_min = -100.0f,
      .output_max = 100.0f,
      .integral_min = -10.0f,
      .integral_max = 10.0f,
      .setpoint_rate_limit = 0.0f,
      .output_rate_limit = 0.0f,
      .derivative_filter_tau = 0.0f,
  };

  ASSERT_TRUE(pid_controller_init(&pid, &config));
  ASSERT_NEAR(pid_controller_update(&pid, 0.0f, 1.0f, 0.1f), 0.0f, 0.0001f);
  ASSERT_NEAR(pid_controller_update(&pid, 5.0f, 1.0f, 0.1f), 0.0f, 0.0001f);
  ASSERT_NEAR(pid_controller_update(&pid, 5.0f, 2.0f, 0.1f), -10.0f, 0.0001f);
}

static void test_pid_limits_setpoint_and_output_slew(void)
{
  pid_controller_t pid;
  const pid_controller_config_t config = {
      .kp = 1.0f,
      .ki = 0.0f,
      .kd = 0.0f,
      .output_min = 0.0f,
      .output_max = 5.0f,
      .integral_min = -10.0f,
      .integral_max = 10.0f,
      .setpoint_rate_limit = 1.0f,
      .output_rate_limit = 0.5f,
      .derivative_filter_tau = 0.0f,
  };

  ASSERT_TRUE(pid_controller_init(&pid, &config));
  ASSERT_NEAR(pid_controller_update(&pid, 5.0f, 0.0f, 0.1f), 0.05f, 0.0001f);

  const pid_controller_state_t state = pid_controller_get_state(&pid);
  ASSERT_NEAR(state.active_setpoint, 0.1f, 0.0001f);
  ASSERT_TRUE((state.flags & PID_CONTROLLER_FLAG_SETPOINT_RATE_LIMITED) != 0U);
  ASSERT_TRUE((state.flags & PID_CONTROLLER_FLAG_OUTPUT_RATE_LIMITED) != 0U);
}

static void test_rls_estimates_linear_model(void)
{
  rls_estimator_t rls;
  const rls_estimator_config_t config = {
      .parameter_count = 2U,
      .initial_covariance = 1000.0f,
      .forgetting_factor = 0.99f,
      .min_denominator = 1.0e-6f,
  };

  ASSERT_TRUE(rls_estimator_init(&rls, &config));

  for (int i = 0; i < 80; ++i)
  {
    const float x = (float)(i % 20) * 0.25f;
    const float phi[2] = {x, 1.0f};
    const float y = (2.0f * x) + 0.5f;
    ASSERT_TRUE(rls_estimator_update(&rls, phi, y));
  }

  ASSERT_NEAR(rls_estimator_get_parameter(&rls, 0U), 2.0f, 0.01f);
  ASSERT_NEAR(rls_estimator_get_parameter(&rls, 1U), 0.5f, 0.01f);
}

int main(void)
{
  test_pid_clamps_output_and_integral();
  test_pid_derivative_on_measurement_avoids_setpoint_kick();
  test_pid_limits_setpoint_and_output_slew();
  test_rls_estimates_linear_model();
  puts("control library tests passed");
  return 0;
}
