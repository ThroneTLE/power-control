#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

void start_control_task(void *argument);
void control_task_entry(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_TASK_H */
