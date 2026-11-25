#include <stdint.h>
#include <stdio.h>

#include "lcdm_velocity_observer.h"

int main(void)
{
  LcdmVelocityObserver observer = {0U};
  uint16_t previous_adc = 0U;
  uint8_t seeded = 0U;
  int32_t position_q4 = 0;
  int32_t velocity_q4 = 0;
  uint32_t rise90_ms = 0U;
  uint32_t fall_to_stop_band_ms = 0U;
  const uint32_t stop_at_ms = 31U;

  (void)LcdmVelocityObserver_Update(&observer, 2000U, 0U, true,
                                    &previous_adc, &seeded, &position_q4,
                                    &velocity_q4, 0, 0);
  for (uint32_t now_ms = 1U; now_ms <= 60U; ++now_ms)
  {
    uint16_t raw_adc = (now_ms < stop_at_ms) ?
                       (uint16_t)(2000U + (2U * now_ms)) : 2060U;
    (void)LcdmVelocityObserver_Update(&observer, raw_adc, now_ms, true,
                                      &previous_adc, &seeded, &position_q4,
                                      &velocity_q4, 0, 0);
    if ((now_ms < stop_at_ms) && (rise90_ms == 0U) && (velocity_q4 >= 29))
    {
      rise90_ms = now_ms;
    }
    if ((now_ms >= stop_at_ms) && (fall_to_stop_band_ms == 0U) &&
        (velocity_q4 <= 8))
    {
      fall_to_stop_band_ms = now_ms - stop_at_ms;
    }
    if ((now_ms == 1U) || (now_ms == 4U) || (now_ms == 8U) ||
        (now_ms == 12U) || (now_ms == 16U) || (now_ms == 24U) ||
        (now_ms == 30U) || (now_ms == 32U) || (now_ms == 36U) ||
        (now_ms == 40U) || (now_ms == 48U))
    {
      printf("t=%2lu ms raw=%u pos_q4=%ld vel_q4_per_ms=%ld\n",
             (unsigned long)now_ms, raw_adc, (long)position_q4,
             (long)velocity_q4);
    }
  }
  printf("rise_to_90pct=%lu ms; post-stop_to_8Q4_band=%lu ms; "
         "fast_observer_vel_div=7\n",
         (unsigned long)rise90_ms,
         (unsigned long)fall_to_stop_band_ms);
  return (rise90_ms == 0U) || (fall_to_stop_band_ms == 0U);
}
