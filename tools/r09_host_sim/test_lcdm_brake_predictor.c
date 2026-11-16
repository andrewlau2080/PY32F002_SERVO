#include <stdio.h>
#include "lcdm_brake_predictor.h"

static unsigned failures;
static void check(const char *name, uint32_t actual, uint32_t expected)
{
  int pass = actual == expected;
  printf("%s: %s (actual=%lu expected=%lu)\n", pass ? "PASS" : "FAIL", name,
         (unsigned long)actual, (unsigned long)expected);
  if (!pass) failures++;
}

int main(void)
{
  check("untrained model disabled", LcdmBrakePredictor_StoppingDistanceQ4(160U, 128U, 7U, 8U, 4U), 0U);
  check("zero deceleration disabled", LcdmBrakePredictor_StoppingDistanceQ4(160U, 0U, 8U, 8U, 4U), 0U);
  check("distance rounds up and includes latency", LcdmBrakePredictor_StoppingDistanceQ4(160U, 128U, 8U, 8U, 4U), 2240U);
  check("lower measured braking predicts longer distance",
        LcdmBrakePredictor_StoppingDistanceQ4(160U, 64U, 8U, 8U, 4U) >
        LcdmBrakePredictor_StoppingDistanceQ4(160U, 256U, 8U, 8U, 4U), 1U);
  check("zero speed has zero distance", LcdmBrakePredictor_StoppingDistanceQ4(0U, 128U, 8U, 8U, 4U), 0U);
  check("latency pad grows with speed",
        LcdmBrakePredictor_StoppingDistanceQ4(160U, 128U, 8U, 8U, 4U) -
        LcdmBrakePredictor_StoppingDistanceQ4(160U, 128U, 8U, 8U, 0U), 640U);
  check("caps corrupted velocity input",
        LcdmBrakePredictor_StoppingDistanceQ4(65535U, 128U, 8U, 8U, 0U),
        LcdmBrakePredictor_StoppingDistanceQ4(2048U, 128U, 8U, 8U, 0U));
  check("test harness clean", failures, 0U);
  return failures ? 1 : 0;
}
