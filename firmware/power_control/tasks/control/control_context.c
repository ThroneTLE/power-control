#include "control_context.h"

#include "cmsis_os2.h"

#define CONTROL_REFERENCE_MIN_KV 0.0f
#define CONTROL_REFERENCE_MAX_KV 5.0f

static float g_reference_kv = 0.0f;
static control_snapshot_t g_snapshot = {0};
static osMutexId_t g_context_mutex = NULL;

static float control_clamp_reference(float kv);
static void control_lock(void);
static void control_unlock(void);

bool control_context_init(void)
{
  if (g_context_mutex == NULL)
  {
    static const osMutexAttr_t context_mutex_attributes = {
        .name = "control_context",
    };
    g_context_mutex = osMutexNew(&context_mutex_attributes);
  }

  control_lock();
  g_reference_kv = CONTROL_REFERENCE_MIN_KV;
  g_snapshot.reference_kv = g_reference_kv;
  g_snapshot.feedback_kv = 0.0f;
  g_snapshot.dac_volts = 0.0f;
  g_snapshot.loop_enabled = false;
  g_snapshot.fault_flags = (g_context_mutex == NULL) ? CONTROL_FAULT_CONTEXT : CONTROL_FAULT_NONE;
  control_unlock();

  return g_context_mutex != NULL;
}

void control_set_reference_kv(float kv)
{
  control_lock();
  g_reference_kv = control_clamp_reference(kv);
  g_snapshot.reference_kv = g_reference_kv;
  control_unlock();
}

float control_get_reference_kv(void)
{
  float kv = 0.0f;
  control_lock();
  kv = g_reference_kv;
  control_unlock();
  return kv;
}

void control_set_loop_enabled(bool enabled)
{
  control_lock();
  g_snapshot.loop_enabled = enabled;
  control_unlock();
}

bool control_get_loop_enabled(void)
{
  bool enabled = false;
  control_lock();
  enabled = g_snapshot.loop_enabled;
  control_unlock();
  return enabled;
}

void control_update_snapshot(float feedback_kv, float dac_volts, uint32_t fault_flags)
{
  control_lock();
  g_snapshot.reference_kv = g_reference_kv;
  g_snapshot.feedback_kv = feedback_kv;
  g_snapshot.dac_volts = dac_volts;
  g_snapshot.fault_flags = fault_flags;
  control_unlock();
}

bool control_get_snapshot(control_snapshot_t *snapshot)
{
  if (snapshot == NULL)
  {
    return false;
  }

  control_lock();
  *snapshot = g_snapshot;
  control_unlock();
  return true;
}

static float control_clamp_reference(float kv)
{
  if (kv < CONTROL_REFERENCE_MIN_KV)
  {
    return CONTROL_REFERENCE_MIN_KV;
  }

  if (kv > CONTROL_REFERENCE_MAX_KV)
  {
    return CONTROL_REFERENCE_MAX_KV;
  }

  return kv;
}

static void control_lock(void)
{
  if (g_context_mutex != NULL && osKernelGetState() == osKernelRunning)
  {
    (void)osMutexAcquire(g_context_mutex, osWaitForever);
    return;
  }

  (void)osKernelLock();
}

static void control_unlock(void)
{
  if (g_context_mutex != NULL && osKernelGetState() == osKernelRunning)
  {
    (void)osMutexRelease(g_context_mutex);
    return;
  }

  (void)osKernelUnlock();
}
