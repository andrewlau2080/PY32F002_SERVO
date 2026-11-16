#ifndef LCDM_VELOCITY_OBSERVER_H
#define LCDM_VELOCITY_OBSERVER_H

#include <stdbool.h>
#include <stdint.h>

static inline int32_t LcdmVelocityObserver_RoundDiv(int32_t numerator, int32_t denominator)
{
  if (numerator >= 0) return (numerator + denominator / 2) / denominator;
  return -((-numerator + denominator / 2) / denominator);
}

/* Position is ADC*16; velocity is ADC*16 per millisecond. */
typedef struct
{
  uint32_t tick_ms;
} LcdmVelocityObserver;

static inline bool LcdmVelocityObserver_Update(LcdmVelocityObserver *observer,
                                                uint16_t raw_adc,
                                                uint32_t now_ms,
                                                bool fast,
                                                uint16_t *previous_adc,
                                                uint8_t *seeded,
                                                int32_t *position_q4,
                                                int32_t *velocity_q4_per_ms,
                                                int32_t *pos_div_out,
                                                int32_t *vel_div_out)
{
  uint32_t dt_ms;
  int32_t pos_div = fast ? 2 : 3;
  int32_t vel_div = fast ? 7 : 12;
  int32_t raw_q4 = (int32_t)raw_adc << 4;

  if (pos_div_out != 0) *pos_div_out = pos_div;
  if (vel_div_out != 0) *vel_div_out = vel_div;
  if (!*seeded)
  {
    *position_q4 = raw_q4;
    *velocity_q4_per_ms = 0;
    *previous_adc = raw_adc;
    observer->tick_ms = now_ms;
    *seeded = true;
    return false;
  }
  dt_ms = now_ms - observer->tick_ms;
  if (dt_ms == 0U) return false;
  observer->tick_ms = now_ms;
  if (dt_ms > 4U)
  {
    *position_q4 = raw_q4;
    *velocity_q4_per_ms = 0;
    *previous_adc = raw_adc;
    return false;
  }
  {
    int32_t pos_alpha_q12 = 4096 / pos_div;
    int32_t vel_alpha_q12 = 4096 / vel_div;
    int32_t predicted_q4 = *position_q4 + *velocity_q4_per_ms * (int32_t)dt_ms;
    int32_t residual_q4 = raw_q4 - predicted_q4;
    int32_t measured_velocity_q4_per_ms =
        ((int32_t)raw_adc - (int32_t)*previous_adc) * 16 / (int32_t)dt_ms;
    for (uint32_t i = 1U; i < dt_ms; i++)
    {
      pos_alpha_q12 += ((4096 - pos_alpha_q12) * (4096 / pos_div)) / 4096;
      vel_alpha_q12 += ((4096 - vel_alpha_q12) * (4096 / vel_div)) / 4096;
    }
    *position_q4 = predicted_q4 + LcdmVelocityObserver_RoundDiv(residual_q4 * pos_alpha_q12, 4096);
    *velocity_q4_per_ms +=
        LcdmVelocityObserver_RoundDiv(
            (measured_velocity_q4_per_ms - *velocity_q4_per_ms) * vel_alpha_q12, 4096);
    *previous_adc = raw_adc;
    if (*velocity_q4_per_ms > 512) *velocity_q4_per_ms = 512;
    if (*velocity_q4_per_ms < -512) *velocity_q4_per_ms = -512;
  }
  return true;
}

#endif
