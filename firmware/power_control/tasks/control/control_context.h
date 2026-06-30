#ifndef CONTROL_CONTEXT_H
#define CONTROL_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  float reference_kv;
  float feedback_kv;
  float dac_volts;
  bool loop_enabled;
  uint32_t fault_flags;
} control_snapshot_t;

enum
{
  CONTROL_FAULT_NONE = 0U,
  CONTROL_FAULT_CONTEXT = 1U << 0U,
  CONTROL_FAULT_AD5593R_INIT = 1U << 1U,
  CONTROL_FAULT_AD5593R_IO = 1U << 2U,
  CONTROL_FAULT_PID = 1U << 3U
};

bool control_context_init(void);
void control_set_reference_kv(float kv);
float control_get_reference_kv(void);
void control_set_loop_enabled(bool enabled);
bool control_get_loop_enabled(void);
void control_update_snapshot(float feedback_kv, float dac_volts, uint32_t fault_flags);
bool control_get_snapshot(control_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_CONTEXT_H */
