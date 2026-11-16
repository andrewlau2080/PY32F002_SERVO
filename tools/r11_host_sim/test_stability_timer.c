#include <stdio.h>
#include "servo_stability_timer.h"

static unsigned s_passed;
static unsigned s_failed;

static void check(const char *name, bool actual, bool expected)
{
  if (actual == expected)
  {
    printf("PASS: %s\n", name);
    s_passed++;
  }
  else
  {
    printf("FAIL: %s (actual=%u expected=%u)\n", name,
           actual ? 1U : 0U, expected ? 1U : 0U);
    s_failed++;
  }
}

int main(void)
{
  ServoStabilityTimer timer = {0};

  check("first qualifying sample only arms timer",
        ServoStabilityTimer_Update(&timer, true, 1U, 8U, 4U), false);
  for (unsigned i = 0U; i < 7U; i++)
  {
    check("8 ms hold remains unconfirmed before threshold",
          ServoStabilityTimer_Update(&timer, true, 1U, 8U, 4U), false);
  }
  check("8 ms hold confirms at elapsed duration",
        ServoStabilityTimer_Update(&timer, true, 1U, 8U, 4U), true);

  ServoStabilityTimer_Reset(&timer);
  (void)ServoStabilityTimer_Update(&timer, true, 0U, 8U, 4U);
  check("same-tick calls do not advance stability time",
        ServoStabilityTimer_Update(&timer, true, 0U, 8U, 4U), false);
  check("real 2 ms tick advances by two milliseconds",
        ServoStabilityTimer_Update(&timer, true, 2U, 8U, 4U), false);
  (void)ServoStabilityTimer_Update(&timer, true, 2U, 8U, 4U);
  (void)ServoStabilityTimer_Update(&timer, true, 2U, 8U, 4U);
  check("jittered 2 ms intervals confirm at 8 ms",
        ServoStabilityTimer_Update(&timer, true, 2U, 8U, 4U), true);

  ServoStabilityTimer_Reset(&timer);
  (void)ServoStabilityTimer_Update(&timer, true, 1U, 8U, 4U);
  (void)ServoStabilityTimer_Update(&timer, true, 2U, 8U, 4U);
  check("long observation gap restarts elapsed stability time",
        ServoStabilityTimer_Update(&timer, true, 5U, 8U, 4U), false);
  check("timer resumes from fresh 1 ms observation",
        ServoStabilityTimer_Update(&timer, true, 1U, 8U, 4U), false);
  for (unsigned i = 0U; i < 6U; i++)
  {
    (void)ServoStabilityTimer_Update(&timer, true, 1U, 8U, 4U);
  }
  check("timer reconfirms only after 8 fresh milliseconds",
        ServoStabilityTimer_Update(&timer, true, 1U, 8U, 4U), true);

  ServoStabilityTimer_Reset(&timer);
  check("30 ms load confirmation starts unarmed",
        ServoStabilityTimer_Update(&timer, true, 1U, 30U, 4U), false);
  for (unsigned i = 0U; i < 30U; i++)
  {
    bool confirmed = ServoStabilityTimer_Update(&timer, true, 1U, 30U, 4U);
    check("30 ms load duration threshold", confirmed, i == 29U);
  }

  check("condition loss clears timer", 
        ServoStabilityTimer_Update(&timer, false, 1U, 30U, 4U), false);
  check("condition must re-arm after loss",
        ServoStabilityTimer_Update(&timer, true, 30U, 30U, 4U), false);

  printf("R11 stability timer checks: %u passed, %u failed\n", s_passed, s_failed);
  return s_failed == 0U ? 0 : 1;
}
