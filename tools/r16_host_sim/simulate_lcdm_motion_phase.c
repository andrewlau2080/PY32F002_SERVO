#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lcdm_motion_phase.h"
#include "lcdm_velocity_observer.h"

typedef struct
{
  uint16_t target_span_q4;
  uint16_t drive_gain_q4_per_1000;
  uint16_t viscous_drag_permille;
  uint16_t brake_decel_estimate_q4;
  uint16_t fallback_brake_distance_q4;
  uint8_t sample_period_ms;
  uint8_t sample_noise_adc;
  uint8_t latency_ms;
} PlantCase;

typedef struct
{
  uint16_t elapsed_ms;
  LcdmMotionPhase phase;
  int32_t error_q4;
  int32_t measured_velocity_q4;
  int32_t plant_velocity_q4;
  int16_t duty_permille;
} PhaseTransition;

static int32_t quantize_adc_q4(int32_t position_q4)
{
  if (position_q4 >= 0)
  {
    return ((position_q4 + 8) / 16) * 16;
  }
  return ((position_q4 - 8) / 16) * 16;
}

static int run_case(const PlantCase *plant)
{
  const LcdmMotionPhaseConfig config = {
    500U, 32U, 96U, 8U, 160U, 16U, plant->fallback_brake_distance_q4,
    400U, 200U, 120U, 180U, 70U, 1280U, 60U
  };
  LcdmVelocityObserver observer = {0U};
  uint16_t previous_adc = 0U;
  uint8_t observer_seeded = 0U;
  int32_t position_q4 = 0;
  int32_t measured_position_q4 = 0;
  int32_t measured_velocity_q4 = 0;
  int32_t plant_velocity_q4 = 0;
  int32_t target_q4 = plant->target_span_q4;
  int32_t peak_overshoot_q4 = 0;
  int32_t max_abs_duty = 0;
  uint16_t settled_ms = 0U;
  uint16_t drive_ticks = 0U;
  uint16_t brake_ticks = 0U;
  uint16_t settle_ticks = 0U;
  PhaseTransition transitions[48];
  uint8_t transition_count = 0U;
  LcdmMotionPhase previous_phase = LCDM_MOTION_PHASE_COAST;
  int16_t previous_duty_permille = 0;
  LcdmMotionPhaseOutput output = {LCDM_MOTION_PHASE_COAST, 0};
  bool reached_settle = false;

  for (uint32_t elapsed_ms = 0U; elapsed_ms < config.hard_timeout_ms; ++elapsed_ms)
  {
    bool sample_due = (plant->sample_period_ms != 0U) &&
                      ((elapsed_ms % plant->sample_period_ms) == 0U);
    if (sample_due)
    {
      int32_t raw_adc = quantize_adc_q4(position_q4) / 16;
      if ((elapsed_ms != 0U) && (plant->sample_noise_adc != 0U))
      {
        if ((elapsed_ms % 31U) == 0U) raw_adc += plant->sample_noise_adc;
        else if ((elapsed_ms % 37U) == 0U) raw_adc -= plant->sample_noise_adc;
      }
      if (raw_adc < 0) raw_adc = 0;
      if (raw_adc > 4095) raw_adc = 4095;
      (void)LcdmVelocityObserver_Update(&observer, (uint16_t)raw_adc,
                                        elapsed_ms, true, &previous_adc,
                                        &observer_seeded, &measured_position_q4,
                                        &measured_velocity_q4, 0, 0);
    }
    if (sample_due || (elapsed_ms >= config.hard_timeout_ms))
    {
      int32_t error_q4 = target_q4 - measured_position_q4;
      uint32_t speed_q4 = (measured_velocity_q4 < 0) ?
                          (uint32_t)(-measured_velocity_q4) : (uint32_t)measured_velocity_q4;
      uint32_t stop_distance_q4 = 0U;
      if (plant->brake_decel_estimate_q4 != 0U)
      {
        uint32_t denom = (uint32_t)plant->brake_decel_estimate_q4 * 2U;
        stop_distance_q4 = ((speed_q4 * speed_q4) + denom - 1U) / denom;
        stop_distance_q4 += speed_q4 * plant->latency_ms;
      }
      LcdmMotionPhaseInput input = {
        elapsed_ms,
        error_q4,
        measured_velocity_q4,
        stop_distance_q4,
        previous_phase,
        previous_duty_permille,
        true,
        (plant->brake_decel_estimate_q4 != 0U)
      };
      output = LcdmMotionPhase_Update(&input, &config);
    }
    if ((output.phase != previous_phase) &&
        (transition_count < (uint8_t)(sizeof(transitions) / sizeof(transitions[0]))))
    {
      PhaseTransition *transition = &transitions[transition_count++];
      transition->elapsed_ms = (uint16_t)elapsed_ms;
      transition->phase = output.phase;
      transition->error_q4 = target_q4 - measured_position_q4;
      transition->measured_velocity_q4 = measured_velocity_q4;
      transition->plant_velocity_q4 = plant_velocity_q4;
      transition->duty_permille = output.signed_duty_permille;
    }
    int32_t duty = output.signed_duty_permille;
    int32_t abs_duty = (duty < 0) ? -duty : duty;
    if (abs_duty > max_abs_duty) max_abs_duty = abs_duty;
    if (output.phase == LCDM_MOTION_PHASE_DRIVE) drive_ticks++;
    if (output.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE) brake_ticks++;
    if (output.phase == LCDM_MOTION_PHASE_SETTLE) settle_ticks++;

    /* Synthetic first-order object; these coefficients are sweep inputs, not
     * measured motor, geartrain, or ADC calibration values. Position is
     * quantized to one ADC count; velocity comes from the production observer
     * using the selected sampling interval and optional deterministic noise. */
    int32_t accel_q4 = (duty * (int32_t)plant->drive_gain_q4_per_1000) / 1000;
    accel_q4 -= (plant_velocity_q4 * (int32_t)plant->viscous_drag_permille) / 1000;
    plant_velocity_q4 += accel_q4;
    if (plant_velocity_q4 > 160) plant_velocity_q4 = 160;
    if (plant_velocity_q4 < -160) plant_velocity_q4 = -160;
    position_q4 += plant_velocity_q4;
    int32_t overshoot_q4 = position_q4 - target_q4;
    if (overshoot_q4 > peak_overshoot_q4) peak_overshoot_q4 = overshoot_q4;

    if ((measured_position_q4 - target_q4 <= 32) &&
        (target_q4 - measured_position_q4 <= 32) &&
        (measured_velocity_q4 <= 8) && (measured_velocity_q4 >= -8) &&
        (output.phase == LCDM_MOTION_PHASE_COAST))
    {
      if (settled_ms < UINT16_MAX) settled_ms++;
      if (settled_ms >= 10U)
      {
        reached_settle = true;
        previous_phase = output.phase;
        elapsed_ms++;
        printf("%s: target=%u gain=%u drag=%u brake_est=%u fallback=%u sample=%ums noise=%u latency=%ums "
               "settled=%lu ms error=%ld q4 peak_overshoot=%ld q4 phases D/B/S=%u/%u/%u max_duty=%ld\n",
               reached_settle ? "PASS" : "FAIL", plant->target_span_q4,
               plant->drive_gain_q4_per_1000, plant->viscous_drag_permille,
               plant->brake_decel_estimate_q4, plant->fallback_brake_distance_q4,
               plant->sample_period_ms, plant->sample_noise_adc, plant->latency_ms,
               (unsigned long)elapsed_ms,
               (long)(target_q4 - measured_position_q4), (long)peak_overshoot_q4,
               drive_ticks, brake_ticks, settle_ticks, (long)max_abs_duty);
        return reached_settle ? 0 : 1;
      }
    }
    else
    {
      settled_ms = 0U;
    }
    if (sample_due || (elapsed_ms >= config.hard_timeout_ms))
    {
      previous_phase = output.phase;
      previous_duty_permille = output.signed_duty_permille;
    }
  }

  printf("FAIL: target=%u gain=%u drag=%u brake_est=%u fallback=%u sample=%ums noise=%u latency=%ums "
         "timeout error=%ld q4 velocity_obs=%ld plant_velocity=%ld q4/ms "
         "peak_overshoot=%ld q4 phases D/B/S=%u/%u/%u\n",
         plant->target_span_q4, plant->drive_gain_q4_per_1000,
         plant->viscous_drag_permille, plant->brake_decel_estimate_q4,
         plant->fallback_brake_distance_q4,
         plant->sample_period_ms, plant->sample_noise_adc, plant->latency_ms,
         (long)(target_q4 - measured_position_q4), (long)measured_velocity_q4,
         (long)plant_velocity_q4,
         (long)peak_overshoot_q4, drive_ticks, brake_ticks, settle_ticks);
  for (uint8_t i = 0U; i < transition_count; ++i)
  {
    const PhaseTransition *transition = &transitions[i];
    printf("  t=%ums phase=%u error=%ld vel_obs=%ld vel_plant=%ld duty=%d\n",
           transition->elapsed_ms, (unsigned)transition->phase,
           (long)transition->error_q4,
           (long)transition->measured_velocity_q4,
           (long)transition->plant_velocity_q4,
           transition->duty_permille);
  }
  return 1;
}

int main(void)
{
  static const PlantCase cases[] = {
    {320U, 80U, 180U, 8U, 192U, 1U, 0U, 8U},
    {320U, 120U, 250U, 16U, 192U, 1U, 0U, 8U},
    {800U, 80U, 180U, 8U, 192U, 1U, 0U, 8U},
    {800U, 120U, 250U, 16U, 192U, 1U, 0U, 8U},
    {1600U, 80U, 180U, 8U, 192U, 1U, 0U, 8U},
    {1600U, 120U, 250U, 16U, 192U, 1U, 0U, 8U},
    {1600U, 120U, 250U, 0U, 192U, 1U, 0U, 8U},
    {1600U, 120U, 250U, 0U, 480U, 1U, 0U, 8U},
    {1600U, 120U, 250U, 0U, 800U, 1U, 0U, 8U},
    {800U, 120U, 250U, 16U, 192U, 2U, 1U, 8U},
    {800U, 120U, 250U, 16U, 192U, 4U, 1U, 8U},
    {1600U, 120U, 250U, 16U, 192U, 4U, 1U, 8U},
    {1600U, 120U, 250U, 0U, 480U, 4U, 1U, 8U}
  };
  unsigned failures = 0U;
  for (unsigned i = 0U; i < (sizeof(cases) / sizeof(cases[0])); ++i)
  {
    failures += (unsigned)run_case(&cases[i]);
  }
  printf("Synthetic plant sweep: %u cases, %u failed convergence\n",
         (unsigned)(sizeof(cases) / sizeof(cases[0])), failures);
  return failures ? 1 : 0;
}
