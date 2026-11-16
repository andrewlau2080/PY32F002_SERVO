#include <stdint.h>
#include <stdio.h>
#include "servo_adaptive.h"
#include "servo_model_store.h"

static ServoLearnedModel empty_model;
static unsigned failures;

const ServoLearnedModel *Servo_ModelStore_Get(void) { return &empty_model; }
bool Servo_ModelStore_IsValid(void) { return false; }

static void check_range(const char *name, int actual, int lower, int upper)
{
  int pass = actual >= lower && actual <= upper;
  printf("%s: %s (actual=%d expected=%d..%d)\n",
         pass ? "PASS" : "FAIL", name, actual, lower, upper);
  if (!pass) failures++;
}

static void object_samples(uint32_t *time_ms, int16_t velocity_q4, int16_t duty, unsigned count)
{
  for (unsigned i = 0U; i < count; i++)
  {
    (*time_ms)++;
    Servo_Adaptive_Update(*time_ms, velocity_q4, duty, true);
  }
}

int main(void)
{
  uint32_t time_ms = 100U;
  uint16_t baseline_gain, slow_gain, fast_gain;
  uint16_t slow_loop_gain, fast_loop_gain, slow_ff, fast_ff;

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(time_ms, 160, 200, true);
  object_samples(&time_ms, 160, 200, 100U);
  baseline_gain = g_servo_adaptive_snapshot.speed_gain_q8_forward;
  check_range("baseline equivalent speed gain", baseline_gain, 12790, 12810);
  check_range("baseline confidence becomes nonzero",
              g_servo_adaptive_snapshot.confidence_permille, 1, 1000);
  check_range("baseline response remains bounded around reference",
              Servo_Adaptive_GetVelocityGain(200U, true), 190, 210);

  time_ms++;
  Servo_Adaptive_Update(time_ms, 80, 200, true); /* Object response changed: half speed at same duty. */
  object_samples(&time_ms, 80, 200, 120U);
  slow_gain = g_servo_adaptive_snapshot.speed_gain_q8_forward;
  slow_loop_gain = Servo_Adaptive_GetVelocityGain(200U, true);
  for (unsigned i = 0U; i < 64U; i++)
  {
    slow_loop_gain = Servo_Adaptive_GetVelocityGain(200U, true);
  }
  slow_ff = Servo_Adaptive_GetFeedForwardDuty(160, 100U, 1000U);
  check_range("slower object response lowers learned gain", slow_gain, 6300, 6500);
  check_range("slower object response raises bounded loop gain", slow_loop_gain, 350, 400);
  check_range("slower object response raises feedforward duty", slow_ff, 350, 410);

  time_ms++;
  Servo_Adaptive_Update(time_ms, 320, 200, true); /* Object response changed: double speed at same duty. */
  object_samples(&time_ms, 320, 200, 140U);
  fast_gain = g_servo_adaptive_snapshot.speed_gain_q8_forward;
  fast_loop_gain = Servo_Adaptive_GetVelocityGain(200U, true);
  for (unsigned i = 0U; i < 64U; i++)
  {
    fast_loop_gain = Servo_Adaptive_GetVelocityGain(200U, true);
  }
  fast_ff = Servo_Adaptive_GetFeedForwardDuty(160, 100U, 1000U);
  check_range("faster object response raises learned gain", fast_gain, 25400, 25800);
  check_range("faster object response lowers loop gain within floor", fast_loop_gain, 100, 130);
  check_range("faster object response lowers feedforward duty", fast_ff, 90, 120);
  check_range("model confidence remains bounded", g_servo_adaptive_snapshot.confidence_permille, 0, 1000);

  printf("R06 equivalent-object response checks: %u passed, %u failed\n", 10U - failures, failures);
  return failures ? 1 : 0;
}
