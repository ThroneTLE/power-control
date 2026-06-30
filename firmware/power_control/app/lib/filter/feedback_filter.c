#include "feedback_filter.h"

#define FEEDBACK_FILTER_ALPHA_MIN 0.0f
#define FEEDBACK_FILTER_ALPHA_MAX 1.0f

static float feedback_filter_clamp_alpha(float alpha);

void feedback_filter_init(feedback_filter_t *filter, float alpha, float initial_value)
{
  if (filter == 0)
  {
    return;
  }

  filter->alpha = feedback_filter_clamp_alpha(alpha);
  filter->initial_value = initial_value;
  for (uint8_t i = 0U; i < FEEDBACK_FILTER_AVERAGE_WINDOW; ++i)
  {
    filter->samples[i] = initial_value;
  }
  filter->sum = 0.0f;
  filter->output = initial_value;
  filter->sample_count = 0U;
  filter->next_index = 0U;
  filter->initialized = 0U;
}

float feedback_filter_update(feedback_filter_t *filter, float input)
{
  float average = input;

  if (filter == 0)
  {
    return input;
  }

  if (filter->sample_count < FEEDBACK_FILTER_AVERAGE_WINDOW)
  {
    filter->samples[filter->next_index] = input;
    filter->sum += input;
    filter->sample_count++;
  }
  else
  {
    filter->sum -= filter->samples[filter->next_index];
    filter->samples[filter->next_index] = input;
    filter->sum += input;
  }

  filter->next_index = (uint8_t)((filter->next_index + 1U) % FEEDBACK_FILTER_AVERAGE_WINDOW);
  average = filter->sum / (float)filter->sample_count;

  if (filter->initialized == 0U)
  {
    filter->output = average;
    filter->initialized = 1U;
  }
  else
  {
    filter->output += filter->alpha * (average - filter->output);
  }

  return filter->output;
}

float feedback_filter_get_output(const feedback_filter_t *filter)
{
  if (filter == 0)
  {
    return 0.0f;
  }

  return filter->output;
}

void feedback_filter_reset(feedback_filter_t *filter, float value)
{
  if (filter == 0)
  {
    return;
  }

  for (uint8_t i = 0U; i < FEEDBACK_FILTER_AVERAGE_WINDOW; ++i)
  {
    filter->samples[i] = value;
  }
  filter->sum = 0.0f;
  filter->output = value;
  filter->sample_count = 0U;
  filter->next_index = 0U;
  filter->initialized = 0U;
}

static float feedback_filter_clamp_alpha(float alpha)
{
  if (alpha < FEEDBACK_FILTER_ALPHA_MIN)
  {
    return FEEDBACK_FILTER_ALPHA_MIN;
  }

  if (alpha > FEEDBACK_FILTER_ALPHA_MAX)
  {
    return FEEDBACK_FILTER_ALPHA_MAX;
  }

  return alpha;
}
