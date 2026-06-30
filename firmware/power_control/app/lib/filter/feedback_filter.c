#include "feedback_filter.h"

#define FEEDBACK_FILTER_ALPHA_MIN 0.0f
#define FEEDBACK_FILTER_ALPHA_MAX 1.0f

static float feedback_filter_clamp_alpha(float alpha);
static float feedback_filter_median3(float a, float b, float c);

void feedback_filter_init(feedback_filter_t *filter, float alpha, float initial_value)
{
  if (filter == 0)
  {
    return;
  }

  filter->alpha = feedback_filter_clamp_alpha(alpha);
  filter->initial_value = initial_value;
  filter->samples[0] = initial_value;
  filter->samples[1] = initial_value;
  filter->samples[2] = initial_value;
  filter->output = initial_value;
  filter->sample_count = 0U;
  filter->next_index = 0U;
  filter->initialized = 0U;
}

float feedback_filter_update(feedback_filter_t *filter, float input)
{
  float median = input;

  if (filter == 0)
  {
    return input;
  }

  filter->samples[filter->next_index] = input;
  filter->next_index = (uint8_t)((filter->next_index + 1U) % 3U);
  if (filter->sample_count < 3U)
  {
    filter->sample_count++;
  }

  if (filter->sample_count < 3U)
  {
    return filter->output;
  }

  median = feedback_filter_median3(filter->samples[0], filter->samples[1], filter->samples[2]);

  if (filter->initialized == 0U)
  {
    filter->output = median;
    filter->initialized = 1U;
  }
  else
  {
    filter->output += filter->alpha * (median - filter->output);
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

  filter->samples[0] = value;
  filter->samples[1] = value;
  filter->samples[2] = value;
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

static float feedback_filter_median3(float a, float b, float c)
{
  if (a > b)
  {
    const float temp = a;
    a = b;
    b = temp;
  }

  if (b > c)
  {
    const float temp = b;
    b = c;
    c = temp;
  }

  if (a > b)
  {
    b = a;
  }

  return b;
}
