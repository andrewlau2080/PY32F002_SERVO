#include <stdio.h>
#include "lcdm_velocity_observer.h"

static unsigned failures;
static void check(const char *name, int actual, int expected)
{
  int pass = actual == expected;
  printf("%s: %s (actual=%d expected=%d)\n", pass ? "PASS" : "FAIL", name, actual, expected);
  if (!pass) failures++;
}

int main(void)
{
  LcdmVelocityObserver o = {0};
  uint16_t previous_adc = 0U;
  uint8_t seeded = 0U;
  int32_t position = 0, velocity = 0;
  int32_t position_before, velocity_before;
  check("first sample seeds observer", LcdmVelocityObserver_Update(&o, 2000U, 100U, true, &previous_adc, &seeded, &position, &velocity, 0, 0), 0);
  check("seed position", position, 32000);
  (void)LcdmVelocityObserver_Update(&o, 2002U, 101U, true, &previous_adc, &seeded, &position, &velocity, 0, 0);
  check("1ms sample advances observer tick", o.tick_ms, 101U);
  check("1ms positive motion yields positive velocity", velocity > 0, 1);
  position_before = position; velocity_before = velocity;
  check("same tick does not update", LcdmVelocityObserver_Update(&o, 2010U, 101U, true, &previous_adc, &seeded, &position, &velocity, 0, 0), 0);
  check("same tick leaves position unchanged", position, position_before);
  check("same tick leaves velocity unchanged", velocity, velocity_before);
  (void)LcdmVelocityObserver_Update(&o, 2004U, 103U, true, &previous_adc, &seeded, &position, &velocity, 0, 0);
  check("2ms interval accepted", o.tick_ms, 103U);
  check("2ms positive motion remains positive", velocity > 0, 1);
  (void)LcdmVelocityObserver_Update(&o, 2002U, 104U, true, &previous_adc, &seeded, &position, &velocity, 0, 0);
  check("reverse motion changes velocity downward", velocity < velocity_before, 1);
  position_before = position;
  check("long gap reseeds without extrapolating", LcdmVelocityObserver_Update(&o, 2010U, 110U, true, &previous_adc, &seeded, &position, &velocity, 0, 0), 0);
  check("long gap position is reseeded", position, 2010 * 16);
  check("long gap velocity is cleared", velocity, 0);
  seeded = 0U;
  (void)LcdmVelocityObserver_Update(&o, 2000U, UINT32_MAX - 1U, true, &previous_adc, &seeded, &position, &velocity, 0, 0);
  (void)LcdmVelocityObserver_Update(&o, 2002U, 0U, true, &previous_adc, &seeded, &position, &velocity, 0, 0);
  check("tick wrap uses elapsed time", o.tick_ms, 0U);
  check("tick wrap positive velocity", velocity > 0, 1);
  {
    LcdmVelocityObserver one_ms = {0}, two_ms = {0};
    uint16_t one_prev = 0U, two_prev = 0U;
    uint8_t one_seeded = 0U, two_seeded = 0U;
    int32_t one_pos = 0, one_vel = 0, two_pos = 0, two_vel = 0;
    (void)LcdmVelocityObserver_Update(&one_ms, 2000U, 10U, true, &one_prev, &one_seeded,
                                      &one_pos, &one_vel, 0, 0);
    (void)LcdmVelocityObserver_Update(&two_ms, 2000U, 10U, true, &two_prev, &two_seeded,
                                      &two_pos, &two_vel, 0, 0);
    for (uint32_t sample = 1U; sample <= 28U; sample++)
      (void)LcdmVelocityObserver_Update(&one_ms, (uint16_t)(2000U + 2U * sample),
                                        10U + sample, true, &one_prev, &one_seeded,
                                        &one_pos, &one_vel, 0, 0);
    for (uint32_t sample = 1U; sample <= 14U; sample++)
      (void)LcdmVelocityObserver_Update(&two_ms, (uint16_t)(2000U + 4U * sample),
                                        10U + 2U * sample, true, &two_prev, &two_seeded,
                                        &two_pos, &two_vel, 0, 0);
    printf("Equivalent ramp outputs: one_ms=%ld two_ms=%ld Q4/ms\n",
           (long)one_vel, (long)two_vel);
    check("1ms observer converges near +2 ADC/ms", one_vel >= 26 && one_vel <= 32, 1);
    check("2ms observer converges near +2 ADC/ms", two_vel >= 26 && two_vel <= 32, 1);
    check("equivalent sample rates agree within 2 Q4", (one_vel - two_vel <= 2) &&
          (two_vel - one_vel <= 2), 1);
  }
  check("returns nonzero on failure", failures != 0U, 0);
  return failures ? 1 : 0;
}
