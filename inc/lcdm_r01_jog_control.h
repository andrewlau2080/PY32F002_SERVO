#ifndef LCDM_R01_JOG_CONTROL_H
#define LCDM_R01_JOG_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "lcdm_customer_brake.h"
#include "lcdm_velocity_observer.h"

typedef enum
{
  LCDM_R01_JOG_IDLE = 0,
  LCDM_R01_JOG_DRIVE,
  LCDM_R01_JOG_BRAKE,
  LCDM_R01_JOG_COAST,
  LCDM_R01_JOG_TIMEOUT,
  LCDM_R01_JOG_POSITION_LIMIT,
  LCDM_R01_JOG_ADC_FAULT
} LcdmR01JogPhase;

typedef enum
{
  LCDM_R01_JOG_BRAKE_NONE = 0,
  LCDM_R01_JOG_BRAKE_POSITION_LIMIT,
  LCDM_R01_JOG_BRAKE_TARGET_MARGIN,
  LCDM_R01_JOG_BRAKE_DRIVE_TIMEOUT
} LcdmR01JogBrakeReason;

typedef struct
{
  uint16_t target_step_adc;
  uint16_t brake_start_remaining_adc;
  uint16_t position_limit_adc;
  uint16_t max_duty_permille;
  uint16_t brake_duty_permille;
  uint16_t max_velocity_q4;
  uint16_t stop_velocity_q4;
  uint16_t minimum_brake_duty_permille;
  uint16_t hard_timeout_ms;
  uint16_t max_drive_ms;
  uint16_t max_brake_ms;
  uint16_t minimum_brake_ms;
} LcdmR01JogConfig;

typedef struct
{
  LcdmVelocityObserver observer;
  uint32_t start_ms;
  uint32_t brake_start_ms;
  uint16_t anchor_adc;
  uint16_t target_adc;
  uint16_t previous_adc;
  int32_t position_q4;
  int32_t velocity_q4_per_ms;
  int16_t last_duty_permille;
  int8_t direction;
  uint8_t observer_seeded;
  uint8_t active;
  uint8_t brake_reason;
  LcdmR01JogPhase phase;
} LcdmR01JogState;

typedef struct
{
  LcdmR01JogPhase phase;
  uint16_t raw_adc;
  uint16_t target_adc;
  int32_t velocity_q4_per_ms;
  int16_t duty_permille;
  uint16_t elapsed_ms;
  uint8_t active;
} LcdmR01JogOutput;

static inline uint16_t LcdmR01Jog_AbsDiff(uint16_t a, uint16_t b)
{
  return (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static inline bool LcdmR01Jog_Begin(LcdmR01JogState *state,
                                    uint16_t raw_adc,
                                    int8_t direction,
                                    uint32_t now_ms,
                                    const LcdmR01JogConfig *config)
{
  int32_t target;
  if ((state == 0) || (config == 0) ||
      ((direction != 1) && (direction != -1)) ||
      (raw_adc < 30U) || (raw_adc > 4065U) ||
      (config->target_step_adc == 0U) ||
      (config->brake_start_remaining_adc >= config->target_step_adc) ||
      (config->position_limit_adc < config->target_step_adc) ||
      (config->max_duty_permille == 0U) ||
      (config->max_duty_permille > 1000U) ||
      (config->brake_duty_permille > config->max_duty_permille) ||
      (config->minimum_brake_duty_permille > config->brake_duty_permille) ||
      (config->hard_timeout_ms == 0U) ||
      (config->max_drive_ms == 0U) || (config->max_brake_ms == 0U) ||
      (config->minimum_brake_ms == 0U) ||
      (config->max_velocity_q4 <= config->stop_velocity_q4))
  {
    return false;
  }

  target = (int32_t)raw_adc + ((int32_t)direction * config->target_step_adc);
  if ((target < 30) || (target > 4065))
  {
    return false;
  }

  state->observer.tick_ms = now_ms;
  state->start_ms = now_ms;
  state->brake_start_ms = 0U;
  state->anchor_adc = raw_adc;
  state->target_adc = (uint16_t)target;
  state->previous_adc = raw_adc;
  state->position_q4 = (int32_t)raw_adc << 4;
  state->velocity_q4_per_ms = 0;
  state->last_duty_permille = 0;
  state->direction = direction;
  state->observer_seeded = 1U;
  state->active = 1U;
  state->brake_reason = LCDM_R01_JOG_BRAKE_NONE;
  state->phase = LCDM_R01_JOG_DRIVE;
  return true;
}

static inline LcdmR01JogOutput LcdmR01Jog_Update(LcdmR01JogState *state,
                                                 uint16_t raw_adc,
                                                 uint32_t now_ms,
                                                 const LcdmR01JogConfig *config)
{
  LcdmR01JogOutput output = {LCDM_R01_JOG_IDLE, raw_adc, 0U, 0, 0, 0U, 0U};
  uint32_t elapsed;
  uint32_t sample_gap_ms;
  int32_t sample_delta_adc;
  int32_t remaining;

  if ((state == 0) || (config == 0))
  {
    output.phase = LCDM_R01_JOG_ADC_FAULT;
    return output;
  }
  output.phase = state->phase;
  output.target_adc = state->target_adc;
  output.velocity_q4_per_ms = state->velocity_q4_per_ms;
  output.duty_permille = state->last_duty_permille;
  output.active = state->active;
  if (state->active == 0U)
  {
    return output;
  }

  elapsed = now_ms - state->start_ms;
  output.elapsed_ms = (elapsed > UINT16_MAX) ? UINT16_MAX : (uint16_t)elapsed;
  if ((raw_adc < 30U) || (raw_adc > 4065U))
  {
    state->phase = LCDM_R01_JOG_ADC_FAULT;
    state->active = 0U;
    state->last_duty_permille = 0;
    output.phase = state->phase;
    output.duty_permille = 0;
    output.active = 0U;
    return output;
  }
  if (elapsed >= config->hard_timeout_ms)
  {
    state->phase = LCDM_R01_JOG_TIMEOUT;
    state->active = 0U;
    state->last_duty_permille = 0;
    output.phase = state->phase;
    output.duty_permille = 0;
    output.active = 0U;
    return output;
  }

  sample_gap_ms = now_ms - state->observer.tick_ms;
  sample_delta_adc = (int32_t)raw_adc - (int32_t)state->previous_adc;
  (void)LcdmVelocityObserver_Update(&state->observer, raw_adc, now_ms, true,
                                    &state->previous_adc,
                                    &state->observer_seeded,
                                    &state->position_q4,
                                    &state->velocity_q4_per_ms, 0, 0);
  /* The shared observer intentionally drops stale velocity after >4 ms.
   * For this short, hard-limited jog only, preserve the interval-average
   * signed speed so a delayed LCD loop cannot turn a position stop into coast. */
  if (sample_gap_ms > 4U)
  {
    int32_t average_velocity = (sample_delta_adc * 16) / (int32_t)sample_gap_ms;
    if (average_velocity > 512) average_velocity = 512;
    if (average_velocity < -512) average_velocity = -512;
    state->velocity_q4_per_ms = average_velocity;
  }
  remaining = ((int32_t)state->target_adc - (int32_t)raw_adc) *
              (int32_t)state->direction;

  if (LcdmR01Jog_AbsDiff(raw_adc, state->anchor_adc) >= config->position_limit_adc)
  {
    if (state->phase == LCDM_R01_JOG_DRIVE)
    {
      state->phase = LCDM_R01_JOG_BRAKE;
      state->brake_start_ms = now_ms;
      state->brake_reason = LCDM_R01_JOG_BRAKE_POSITION_LIMIT;
    }
  }
  else if ((state->phase == LCDM_R01_JOG_DRIVE) &&
           ((remaining <= (int32_t)config->brake_start_remaining_adc) ||
            (elapsed >= config->max_drive_ms)))
  {
    state->phase = LCDM_R01_JOG_BRAKE;
    state->brake_start_ms = now_ms;
    state->brake_reason = (remaining <= (int32_t)config->brake_start_remaining_adc) ?
        LCDM_R01_JOG_BRAKE_TARGET_MARGIN : LCDM_R01_JOG_BRAKE_DRIVE_TIMEOUT;
  }

  if (state->phase == LCDM_R01_JOG_DRIVE)
  {
    state->last_duty_permille = (int16_t)(state->direction *
                                          (int16_t)config->max_duty_permille);
  }
  else if (state->phase == LCDM_R01_JOG_BRAKE)
  {
    int32_t speed_along = state->velocity_q4_per_ms * (int32_t)state->direction;
    uint32_t brake_elapsed = now_ms - state->brake_start_ms;
    if ((brake_elapsed >= config->minimum_brake_ms) &&
        (speed_along <= (int32_t)config->stop_velocity_q4))
    {
      state->phase = LCDM_R01_JOG_COAST;
      state->active = 0U;
      state->last_duty_permille = 0;
    }
    else if (brake_elapsed >= config->max_brake_ms)
    {
      state->phase = LCDM_R01_JOG_COAST;
      state->active = 0U;
      state->last_duty_permille = 0;
    }
    else
    {
      uint16_t brake_duty = LcdmCustomerBrake_ScaleDuty(
          config->brake_duty_permille, state->velocity_q4_per_ms,
          state->direction, config->max_velocity_q4, config->stop_velocity_q4);
      if (brake_duty < config->minimum_brake_duty_permille)
      {
        brake_duty = config->minimum_brake_duty_permille;
      }
      state->last_duty_permille = (int16_t)(-(int16_t)state->direction *
                                             (int16_t)brake_duty);
    }
  }

  output.phase = state->phase;
  output.target_adc = state->target_adc;
  output.velocity_q4_per_ms = state->velocity_q4_per_ms;
  output.duty_permille = state->last_duty_permille;
  output.active = state->active;
  return output;
}

static inline void LcdmR01Jog_Abort(LcdmR01JogState *state)
{
  if (state != 0)
  {
    state->active = 0U;
    state->phase = LCDM_R01_JOG_COAST;
    state->last_duty_permille = 0;
  }
}

#endif
