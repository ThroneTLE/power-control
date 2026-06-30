#include "rls_estimator.h"

#include <math.h>
#include <string.h>

static bool rls_config_is_valid(const rls_estimator_config_t *config);

bool rls_estimator_init(rls_estimator_t *estimator, const rls_estimator_config_t *config)
{
  if (estimator == NULL || !rls_config_is_valid(config))
  {
    return false;
  }

  memset(estimator, 0, sizeof(*estimator));
  estimator->config = *config;
  rls_estimator_reset(estimator);
  return true;
}

void rls_estimator_reset(rls_estimator_t *estimator)
{
  if (estimator == NULL)
  {
    return;
  }

  memset(estimator->theta, 0, sizeof(estimator->theta));
  memset(estimator->covariance, 0, sizeof(estimator->covariance));

  for (uint8_t i = 0U; i < estimator->config.parameter_count; ++i)
  {
    estimator->covariance[i][i] = estimator->config.initial_covariance;
  }

  estimator->last_prediction = 0.0f;
  estimator->last_error = 0.0f;
  estimator->update_count = 0U;
}

bool rls_estimator_update(rls_estimator_t *estimator, const float *regressor, float measurement)
{
  float covariance_phi[RLS_ESTIMATOR_MAX_PARAMETERS] = {0};
  float gain[RLS_ESTIMATOR_MAX_PARAMETERS] = {0};
  float denominator = 0.0f;
  float prediction = 0.0f;
  float error = 0.0f;
  float phi_covariance[RLS_ESTIMATOR_MAX_PARAMETERS] = {0};
  const uint8_t n = estimator != NULL ? estimator->config.parameter_count : 0U;

  if (estimator == NULL || regressor == NULL || n == 0U)
  {
    return false;
  }

  prediction = rls_estimator_predict(estimator, regressor);

  for (uint8_t row = 0U; row < n; ++row)
  {
    for (uint8_t col = 0U; col < n; ++col)
    {
      covariance_phi[row] += estimator->covariance[row][col] * regressor[col];
    }
  }

  denominator = estimator->config.forgetting_factor;
  for (uint8_t i = 0U; i < n; ++i)
  {
    denominator += regressor[i] * covariance_phi[i];
  }

  if (fabsf(denominator) < estimator->config.min_denominator)
  {
    return false;
  }

  for (uint8_t i = 0U; i < n; ++i)
  {
    gain[i] = covariance_phi[i] / denominator;
  }

  error = measurement - prediction;
  for (uint8_t i = 0U; i < n; ++i)
  {
    estimator->theta[i] += gain[i] * error;
  }

  for (uint8_t col = 0U; col < n; ++col)
  {
    for (uint8_t row = 0U; row < n; ++row)
    {
      phi_covariance[col] += regressor[row] * estimator->covariance[row][col];
    }
  }

  for (uint8_t row = 0U; row < n; ++row)
  {
    for (uint8_t col = 0U; col < n; ++col)
    {
      estimator->covariance[row][col] =
          (estimator->covariance[row][col] - (gain[row] * phi_covariance[col])) /
          estimator->config.forgetting_factor;
    }
  }

  estimator->last_prediction = prediction;
  estimator->last_error = error;
  estimator->update_count++;
  return true;
}

float rls_estimator_predict(const rls_estimator_t *estimator, const float *regressor)
{
  float prediction = 0.0f;

  if (estimator == NULL || regressor == NULL)
  {
    return 0.0f;
  }

  for (uint8_t i = 0U; i < estimator->config.parameter_count; ++i)
  {
    prediction += estimator->theta[i] * regressor[i];
  }

  return prediction;
}

float rls_estimator_get_parameter(const rls_estimator_t *estimator, uint8_t index)
{
  if (estimator == NULL || index >= estimator->config.parameter_count)
  {
    return 0.0f;
  }

  return estimator->theta[index];
}

size_t rls_estimator_get_parameter_count(const rls_estimator_t *estimator)
{
  if (estimator == NULL)
  {
    return 0U;
  }

  return estimator->config.parameter_count;
}

static bool rls_config_is_valid(const rls_estimator_config_t *config)
{
  if (config == NULL)
  {
    return false;
  }

  return config->parameter_count > 0U && config->parameter_count <= RLS_ESTIMATOR_MAX_PARAMETERS &&
         isfinite(config->initial_covariance) && config->initial_covariance > 0.0f &&
         isfinite(config->forgetting_factor) && config->forgetting_factor > 0.0f &&
         config->forgetting_factor <= 1.0f && isfinite(config->min_denominator) &&
         config->min_denominator > 0.0f;
}
