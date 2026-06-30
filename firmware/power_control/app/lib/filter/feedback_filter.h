#ifndef FEEDBACK_FILTER_H
#define FEEDBACK_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FEEDBACK_FILTER_AVERAGE_WINDOW 10U

typedef struct
{
  float alpha;
  float initial_value;
  float samples[FEEDBACK_FILTER_AVERAGE_WINDOW];
  float sum;
  float output;
  uint8_t sample_count;
  uint8_t next_index;
  uint8_t initialized;
} feedback_filter_t;

void feedback_filter_init(feedback_filter_t *filter, float alpha, float initial_value);
float feedback_filter_update(feedback_filter_t *filter, float input);
float feedback_filter_get_output(const feedback_filter_t *filter);
void feedback_filter_reset(feedback_filter_t *filter, float value);

#ifdef __cplusplus
}
#endif

#endif /* FEEDBACK_FILTER_H */
