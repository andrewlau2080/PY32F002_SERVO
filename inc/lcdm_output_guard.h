#ifndef LCDM_OUTPUT_GUARD_H
#define LCDM_OUTPUT_GUARD_H

#include <stdint.h>

typedef struct
{
  int8_t output_dir;
  uint32_t reverse_dead_until_ms;
  uint8_t reverse_dead_active;
} LcdmOutputGuardState;

typedef enum
{
  LCDM_OUTPUT_GUARD_READY = 0,
  LCDM_OUTPUT_GUARD_DEADTIME_STARTED,
  LCDM_OUTPUT_GUARD_DEADTIME_WAIT
} LcdmOutputGuardResult;

/* Apply the existing LCDM direction blanking and same-direction duty slew.
 * The caller still owns final PWM application, coast, and fault handling. */
static inline LcdmOutputGuardResult LcdmOutputGuard_Apply(
    LcdmOutputGuardState *state,
    uint32_t now_ms,
    int16_t requested_duty,
    int16_t previous_duty,
    uint32_t reverse_dead_ms,
    uint16_t reverse_min_duty,
    uint16_t slew_permille_per_ms,
    uint32_t elapsed_ms,
    int16_t *applied_duty)
{
  int8_t requested_dir;
  uint32_t requested_abs;
  uint64_t slew;
  int32_t result;

  if ((state == 0) || (applied_duty == 0))
  {
    return LCDM_OUTPUT_GUARD_DEADTIME_WAIT;
  }

  *applied_duty = 0;
  if (state->reverse_dead_active != 0U)
  {
    if ((int32_t)(now_ms - state->reverse_dead_until_ms) < 0)
    {
      return LCDM_OUTPUT_GUARD_DEADTIME_WAIT;
    }
    state->reverse_dead_active = 0U;
    state->output_dir = 0;
  }

  requested_dir = (requested_duty > 0) ? 1 : ((requested_duty < 0) ? -1 : 0);
  requested_abs = (requested_duty < 0) ?
      (uint32_t)(-(int32_t)requested_duty) : (uint32_t)requested_duty;
  if ((state->output_dir != 0) && (requested_dir != 0) &&
      (requested_dir != state->output_dir))
  {
    if (requested_abs >= reverse_min_duty)
    {
      state->reverse_dead_until_ms = now_ms + reverse_dead_ms;
      state->reverse_dead_active = 1U;
      return LCDM_OUTPUT_GUARD_DEADTIME_STARTED;
    }
    state->output_dir = 0;
    state->reverse_dead_active = 0U;
  }

  slew = (uint64_t)slew_permille_per_ms * elapsed_ms;
  if (slew > 32767U) slew = 32767U;
  result = requested_duty;
  if ((previous_duty > 0) && (requested_duty < 0))
  {
    result = (int32_t)previous_duty - (int32_t)slew;
    if (result < 0) result = 0;
  }
  else if ((previous_duty < 0) && (requested_duty > 0))
  {
    result = (int32_t)previous_duty + (int32_t)slew;
    if (result > 0) result = 0;
  }
  else if ((previous_duty >= 0) && (requested_duty > previous_duty) &&
           ((int32_t)requested_duty - previous_duty > (int32_t)slew))
  {
    result = (int32_t)previous_duty + (int32_t)slew;
  }
  else if ((previous_duty <= 0) && (requested_duty < previous_duty) &&
           ((int32_t)previous_duty - requested_duty > (int32_t)slew))
  {
    result = (int32_t)previous_duty - (int32_t)slew;
  }

  if (result > INT16_MAX) result = INT16_MAX;
  if (result < INT16_MIN) result = INT16_MIN;
  *applied_duty = (int16_t)result;
  state->output_dir = (result > 0) ? 1 : ((result < 0) ? -1 : 0);
  return LCDM_OUTPUT_GUARD_READY;
}

#endif
