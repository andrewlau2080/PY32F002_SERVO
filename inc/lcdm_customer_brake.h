#ifndef LCDM_CUSTOMER_BRAKE_H
#define LCDM_CUSTOMER_BRAKE_H

#include <stdint.h>

/* Scale a customer-profile reverse pulse by measured speed in the original
 * motion direction. Once stopped or already reversing, return zero so the
 * fixed tail cannot accelerate the mechanism back the other way. */
static inline uint16_t LcdmCustomerBrake_ScaleDuty(uint16_t base_duty,
                                                   int32_t velocity_q4,
                                                   int8_t original_dir,
                                                   uint32_t max_velocity_q4,
                                                   uint16_t stop_band_q4)
{
  uint32_t along_motion_q4;
  uint32_t available_velocity_q4;
  uint32_t braking_range_q4;
  uint32_t duty;

  if ((base_duty == 0U) || ((original_dir != 1) && (original_dir != -1)) ||
      (max_velocity_q4 <= stop_band_q4))
  {
    return 0U;
  }

  if (original_dir > 0)
  {
    if (velocity_q4 <= (int32_t)stop_band_q4)
    {
      return 0U;
    }
    along_motion_q4 = (uint32_t)velocity_q4;
  }
  else
  {
    if (velocity_q4 >= -(int32_t)stop_band_q4)
    {
      return 0U;
    }
    along_motion_q4 = (uint32_t)(-(int64_t)velocity_q4);
  }

  if (along_motion_q4 >= max_velocity_q4)
  {
    return base_duty;
  }

  available_velocity_q4 = along_motion_q4 - stop_band_q4;
  braking_range_q4 = max_velocity_q4 - stop_band_q4;
  duty = (((uint32_t)base_duty * available_velocity_q4) +
          (braking_range_q4 / 2U)) / braking_range_q4;
  return (duty > base_duty) ? base_duty : (uint16_t)duty;
}

#endif
