#include <stdio.h>
#include "lcdm_trajectory.h"

static unsigned failures;
static void check(const char *name, int actual, int expected)
{
  int pass = actual == expected;
  printf("%s: %s (actual=%d expected=%d)\n", pass ? "PASS" : "FAIL", name, actual, expected);
  if (!pass) failures++;
}

int main(void)
{
  uint32_t tick = 0U;
  uint32_t now_ms = 0U;
  int32_t position = 0, velocity = 0;
  LcdmTrajectory_Update(0U, 16000, 0, 416U, 36U, &tick, &position, &velocity);
  check("same tick does not move", position, 0);
  now_ms = 1U;
  LcdmTrajectory_Update(now_ms, 16000, 0, 416U, 36U, &tick, &position, &velocity);
  check("first step respects acceleration", velocity, 36);
  check("first position integrates elapsed time", position, 18);
  check("velocity remains below max", velocity <= 416, 1);

  int32_t previous_velocity = velocity;
  now_ms = 5U;
  LcdmTrajectory_Update(now_ms, 16000, 0, 416U, 36U, &tick, &position, &velocity);
  check("4ms update respects acceleration bound", velocity - previous_velocity <= 144, 1);
  check("4ms update respects negative acceleration bound", previous_velocity - velocity <= 144, 1);
  check("4ms update does not exceed max speed", velocity <= 416, 1);

  unsigned steps = 0U;
  int acceleration_bounded = 1;
  while ((position != 16000) && (steps < 2000U))
  {
    int32_t previous = velocity;
    now_ms++;
    LcdmTrajectory_Update(now_ms, 16000, position, 416U, 36U,
                          &tick, &position, &velocity);
    if ((velocity - previous > 36) || (previous - velocity > 36))
    {
      acceleration_bounded = 0;
    }
    steps++;
  }
  check("acceleration bound holds through endpoint", acceleration_bounded, 1);
  check("positive trajectory reaches target", position, 16000);
  check("positive trajectory stops at target", velocity, 0);
  check("positive trajectory completes in bounded time", steps < 2000U, 1);

  now_ms++;
  LcdmTrajectory_Update(now_ms, 0, position, 416U, 36U, &tick, &position, &velocity);
  check("reverse command acceleration is bounded", velocity >= -36 && velocity <= 36, 1);
  check("reverse trajectory remains within endpoint", position >= 0 && position <= 16000, 1);

  uint32_t before_gap_tick = tick;
  now_ms += 5U;
  LcdmTrajectory_Update(now_ms, 8000, 12345, 416U, 36U,
                        &tick, &position, &velocity);
  check("gap above 4ms reseeds from measurement", position, 12345);
  check("gap above 4ms clears stale velocity", velocity, 0);
  check("gap advances timing baseline", (int32_t)(tick - before_gap_tick), 5);
  check("returns nonzero on failure", failures != 0U, 0);
  return failures ? 1 : 0;
}
