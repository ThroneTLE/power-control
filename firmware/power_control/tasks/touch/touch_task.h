#ifndef TOUCH_TASK_H
#define TOUCH_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

void start_touch_task(void *argument);
void touch_task_entry(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_TASK_H */
