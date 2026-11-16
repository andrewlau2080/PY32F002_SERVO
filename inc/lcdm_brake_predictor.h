#ifndef LCDM_BRAKE_PREDICTOR_H
#define LCDM_BRAKE_PREDICTOR_H

#include <stdint.h>

/*
 * Velocity is Q4 ADC/ms and learned deceleration is Q8 ADC/ms^2.
 * Return predicted distance in Q4 ADC, rounded up and padded for observer
 * latency. Do not use a model until enough direction-specific samples exist.
 */
static inline uint32_t LcdmBrakePredictor_StoppingDistanceQ4(
    uint16_t speed_q4,
    uint16_t brake_accel_q8,
    uint16_t brake_sample_count,
    uint16_t minimum_samples,
    uint8_t latency_ms)
{
  if ((brake_sample_count < minimum_samples) || (brake_accel_q8 == 0U))
  {
    return 0U;
  }

  /* Production speed is bounded by the LCDM parameter range (<= 800 Q4/ms). */
  if (speed_q4 > 2048U) speed_q4 = 2048U;
  uint32_t numerator = (uint32_t)speed_q4 * (uint32_t)speed_q4 * 8UL;
  uint32_t distance_q4 = (numerator + brake_accel_q8 - 1U) / brake_accel_q8;
  distance_q4 += (uint32_t)speed_q4 * latency_ms;
  return distance_q4;
}

#endif
