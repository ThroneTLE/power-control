#ifndef RLS_ESTIMATOR_H
#define RLS_ESTIMATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RLS_ESTIMATOR_MAX_PARAMETERS 4U

typedef struct
{
  uint8_t parameter_count;
  float initial_covariance;
  float forgetting_factor;
  float min_denominator;
} rls_estimator_config_t;

typedef struct
{
  rls_estimator_config_t config;
  float theta[RLS_ESTIMATOR_MAX_PARAMETERS];
  float covariance[RLS_ESTIMATOR_MAX_PARAMETERS][RLS_ESTIMATOR_MAX_PARAMETERS];
  float last_prediction;
  float last_error;
  uint32_t update_count;
} rls_estimator_t;

bool rls_estimator_init(rls_estimator_t *estimator, const rls_estimator_config_t *config);
void rls_estimator_reset(rls_estimator_t *estimator);
bool rls_estimator_update(rls_estimator_t *estimator, const float *regressor, float measurement);
float rls_estimator_predict(const rls_estimator_t *estimator, const float *regressor);
float rls_estimator_get_parameter(const rls_estimator_t *estimator, uint8_t index);
size_t rls_estimator_get_parameter_count(const rls_estimator_t *estimator);

#ifdef __cplusplus
}
#endif

#endif /* RLS_ESTIMATOR_H */
