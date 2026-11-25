#ifndef LCDM_MOTION_PHASE_H
#define LCDM_MOTION_PHASE_H

#include <stdbool.h>
#include <stdint.h>

#include "lcdm_customer_brake.h"

typedef enum
{
  LCDM_MOTION_PHASE_FAULT = 0,
  LCDM_MOTION_PHASE_DRIVE,
  LCDM_MOTION_PHASE_REVERSE_BRAKE,
  LCDM_MOTION_PHASE_BRAKE_COAST,
  LCDM_MOTION_PHASE_SETTLE,
  LCDM_MOTION_PHASE_COAST,
  LCDM_MOTION_PHASE_TIMEOUT
} LcdmMotionPhase;

typedef struct
{
  uint32_t elapsed_ms;
  int32_t error_q4;       /* target position minus measured position, Q4 ADC */
  int32_t velocity_q4;   /* measured signed velocity, Q4 ADC/ms */
  uint32_t stop_distance_q4;
  LcdmMotionPhase previous_phase;
  int16_t previous_signed_duty_permille;
  bool feedback_valid;
  bool stop_model_valid;
} LcdmMotionPhaseInput;

typedef struct
{
  uint32_t hard_timeout_ms;
  uint16_t stop_error_band_q4;
  uint16_t settle_error_band_q4;
  uint16_t stop_velocity_band_q4;
  uint16_t max_velocity_q4;
  uint16_t brake_margin_q4;
  uint16_t fallback_brake_distance_q4;
  uint16_t max_duty_permille;
  uint16_t drive_duty_permille;
  uint16_t untrained_drive_duty_permille;
  uint16_t brake_duty_permille;
  uint16_t settle_duty_permille;
  uint16_t approach_distance_q4;
  uint16_t approach_min_duty_permille;
} LcdmMotionPhaseConfig;

typedef struct
{
  LcdmMotionPhase phase;
  int16_t signed_duty_permille;
} LcdmMotionPhaseOutput;

static inline uint16_t LcdmMotionPhase_SettleDuty(
    uint32_t abs_error_q4,
    const LcdmMotionPhaseConfig *config,
    uint32_t max_duty)
{
  uint32_t requested = (config->settle_duty_permille > max_duty) ?
                       max_duty : config->settle_duty_permille;
  uint32_t band_width = (uint32_t)config->settle_error_band_q4 -
                        config->stop_error_band_q4;
  if ((band_width == 0U) ||
      (abs_error_q4 >= config->settle_error_band_q4))
  {
    return (uint16_t)requested;
  }
  if (abs_error_q4 <= config->stop_error_band_q4)
  {
    return 0U;
  }
  requested = (requested *
      (abs_error_q4 - config->stop_error_band_q4)) / band_width;
  if (requested == 0U)
  {
    requested = 1U;
  }
  return (uint16_t)requested;
}

/*
 * Decide the next signed output from feedback and a learned stopping-distance
 * estimate. This is a pure decision layer: callers own ADC validation,
 * velocity estimation, PWM application, and the independent hardware cutoff.
 * It deliberately has no customer-sample absolute timing or duty defaults.
 */
static inline LcdmMotionPhaseOutput LcdmMotionPhase_Update(
    const LcdmMotionPhaseInput *input,
    const LcdmMotionPhaseConfig *config)
{
  LcdmMotionPhaseOutput output = {LCDM_MOTION_PHASE_FAULT, 0};
  uint32_t abs_error_q4;
  uint32_t abs_velocity_q4;
  uint32_t max_duty;
  uint32_t distance_to_brake_q4;
  int8_t error_dir;
  int8_t velocity_dir;
  bool within_settle_distance;
  bool moving_toward_target;
  bool brake_now;

  if ((input == 0) || (config == 0) || !input->feedback_valid ||
      (config->hard_timeout_ms == 0U) ||
      (config->stop_error_band_q4 > config->settle_error_band_q4) ||
      (config->stop_velocity_band_q4 >= config->max_velocity_q4))
  {
    return output;
  }

  if (input->elapsed_ms >= config->hard_timeout_ms)
  {
    output.phase = LCDM_MOTION_PHASE_TIMEOUT;
    return output;
  }

  abs_error_q4 = (input->error_q4 < 0) ?
      (uint32_t)(-(int64_t)input->error_q4) : (uint32_t)input->error_q4;
  abs_velocity_q4 = (input->velocity_q4 < 0) ?
      (uint32_t)(-(int64_t)input->velocity_q4) : (uint32_t)input->velocity_q4;
  error_dir = (input->error_q4 > 0) ? 1 : ((input->error_q4 < 0) ? -1 : 0);
  velocity_dir = (input->velocity_q4 > 0) ? 1 :
                 ((input->velocity_q4 < 0) ? -1 : 0);
  max_duty = (config->max_duty_permille > 1000U) ?
             1000U : config->max_duty_permille;
  within_settle_distance = (abs_error_q4 <= config->settle_error_band_q4);

  if ((abs_error_q4 <= config->stop_error_band_q4) &&
      (abs_velocity_q4 <= config->stop_velocity_band_q4))
  {
    output.phase = LCDM_MOTION_PHASE_COAST;
    return output;
  }

  if ((input->previous_phase == LCDM_MOTION_PHASE_BRAKE_COAST) &&
      (abs_velocity_q4 > config->stop_velocity_band_q4))
  {
    output.phase = LCDM_MOTION_PHASE_BRAKE_COAST;
    return output;
  }

  if ((input->previous_phase == LCDM_MOTION_PHASE_REVERSE_BRAKE) &&
      (velocity_dir != 0) &&
      (((input->previous_signed_duty_permille > 0) && (velocity_dir > 0)) ||
       ((input->previous_signed_duty_permille < 0) && (velocity_dir < 0))))
  {
    /* The mechanism has crossed zero and is now moving in the direction of
     * the prior counter-drive. Release reverse torque, coast until slow, then
     * resume any residual position correction. */
    output.phase = LCDM_MOTION_PHASE_BRAKE_COAST;
    return output;
  }

  moving_toward_target = (error_dir != 0) && (velocity_dir == error_dir);
  distance_to_brake_q4 = input->stop_distance_q4;
  if (distance_to_brake_q4 <= UINT32_MAX - config->brake_margin_q4)
  {
    distance_to_brake_q4 += config->brake_margin_q4;
  }
  else
  {
    distance_to_brake_q4 = UINT32_MAX;
  }

  brake_now = false;
  if ((velocity_dir != 0) && (abs_velocity_q4 > config->stop_velocity_band_q4))
  {
    /* Keep an initiated brake phase active until measured speed falls into
     * the stop band. This prevents phase chatter as predicted distance shrinks. */
    if (input->previous_phase == LCDM_MOTION_PHASE_REVERSE_BRAKE)
    {
      brake_now = true;
    }
    else if (!moving_toward_target)
    {
      /* First cancel velocity that opposes the current position error, even
       * when the mechanism is still far from the target. Resume target drive
       * only after measured speed falls into the stop band. */
      brake_now = true;
    }
    else if (moving_toward_target && input->stop_model_valid &&
             (distance_to_brake_q4 >= abs_error_q4))
    {
      brake_now = true;
    }
    else if (moving_toward_target && !input->stop_model_valid &&
             (abs_error_q4 <= config->fallback_brake_distance_q4))
    {
      /* Conservative configured fallback while the learned model is unqualified. */
      brake_now = true;
    }
  }

  if (brake_now)
  {
    uint16_t brake_duty = LcdmCustomerBrake_ScaleDuty(
        config->brake_duty_permille, input->velocity_q4, velocity_dir,
        config->max_velocity_q4, config->stop_velocity_band_q4);
    if (brake_duty > max_duty)
    {
      brake_duty = (uint16_t)max_duty;
    }
    output.phase = (brake_duty == 0U) ? LCDM_MOTION_PHASE_SETTLE :
                                      LCDM_MOTION_PHASE_REVERSE_BRAKE;
    if (brake_duty != 0U)
    {
      output.signed_duty_permille = (int16_t)(-velocity_dir * (int16_t)brake_duty);
    }
    else if ((error_dir != 0) && (abs_error_q4 > config->stop_error_band_q4))
    {
      uint16_t settle_duty = LcdmMotionPhase_SettleDuty(
          abs_error_q4, config, max_duty);
      output.signed_duty_permille = (int16_t)(error_dir * (int16_t)settle_duty);
    }
    return output;
  }

  if (within_settle_distance)
  {
    uint16_t settle_duty = LcdmMotionPhase_SettleDuty(
        abs_error_q4, config, max_duty);
    output.phase = LCDM_MOTION_PHASE_SETTLE;
    if ((error_dir != 0) && (abs_error_q4 > config->stop_error_band_q4))
    {
      output.signed_duty_permille = (int16_t)(error_dir * (int16_t)settle_duty);
    }
    return output;
  }

  {
    uint16_t requested_drive_duty = input->stop_model_valid ?
        config->drive_duty_permille : config->untrained_drive_duty_permille;
    uint16_t drive_duty = (requested_drive_duty > max_duty) ?
                          (uint16_t)max_duty : requested_drive_duty;
    uint16_t approach_min = (config->approach_min_duty_permille > drive_duty) ?
                            drive_duty : config->approach_min_duty_permille;
    if ((config->approach_distance_q4 != 0U) &&
        (abs_error_q4 < config->approach_distance_q4))
    {
      uint32_t duty_span = (uint32_t)drive_duty - approach_min;
      drive_duty = (uint16_t)(approach_min +
          ((duty_span * abs_error_q4) / config->approach_distance_q4));
    }
    if (drive_duty == 0U)
    {
      output.phase = LCDM_MOTION_PHASE_COAST;
      return output;
    }
    output.phase = LCDM_MOTION_PHASE_DRIVE;
    output.signed_duty_permille = (int16_t)(error_dir * (int16_t)drive_duty);
  }
  return output;
}

#endif
