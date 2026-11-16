#include <stdint.h>
#include <stdio.h>
#include "servo_adaptive.h"
#include "servo_model_store.h"

static ServoLearnedModel empty_model;
static unsigned failures;
static unsigned checks;

const ServoLearnedModel *Servo_ModelStore_Get(void) { return &empty_model; }
bool Servo_ModelStore_IsValid(void) { return false; }

static void check(const char *name, int actual, int expected)
{
  checks++;
  int pass = actual == expected;
  printf("%s: %s (actual=%d expected=%d)\n", pass ? "PASS" : "FAIL", name, actual, expected);
  if (!pass) failures++;
}

int main(void)
{
  uint16_t base = 200U;
  g_servo_adaptive_snapshot.reference_speed_gain_q8_forward = 12800U;
  g_servo_adaptive_snapshot.speed_gain_q8_forward = 12800U;
  g_servo_adaptive_snapshot.reference_speed_gain_q8_reverse = 12800U;
  g_servo_adaptive_snapshot.speed_gain_q8_reverse = 12800U;

  g_servo_adaptive_snapshot.confidence_permille = 0U;
  check("zero confidence leaves base gain unchanged",
        Servo_Adaptive_GetVelocityGain(base, true), base);
  check("first adaptive output starts from fixed base",
        g_servo_adaptive_applied_velocity_gain_q8_forward, base);
  g_servo_adaptive_snapshot.confidence_permille = 1000U;
  check("reference object leaves base gain unchanged",
        Servo_Adaptive_GetVelocityGain(base, true), base);

  g_servo_adaptive_snapshot.speed_gain_q8_forward = 6400U;
  check("gain correction ramps toward 2x base bound",
        Servo_Adaptive_GetVelocityGain(base, true), 204);
  check("per-cycle gain step stays within 1/64 base",
        g_servo_adaptive_applied_velocity_gain_q8_forward, 204);
  g_servo_adaptive_snapshot.speed_gain_q8_forward = 3200U;
  check("extreme target still uses bounded increment",
        Servo_Adaptive_GetVelocityGain(base, true), 208);
  for (unsigned i = 0; i < 64U; i++)
  {
    (void)Servo_Adaptive_GetVelocityGain(base, true);
  }
  check("strong correction converges to 2x base within bound",
        g_servo_adaptive_applied_velocity_gain_q8_forward, 400);
  g_servo_adaptive_snapshot.speed_gain_q8_forward = 51200U;
  check("strong reverse correction is rate limited",
        Servo_Adaptive_GetVelocityGain(base, true), 396);
  for (unsigned i = 0; i < 100U; i++)
  {
    (void)Servo_Adaptive_GetVelocityGain(base, true);
  }
  check("strong speedup remains clipped at half base",
        g_servo_adaptive_applied_velocity_gain_q8_forward, 100);

  g_servo_adaptive_snapshot.speed_gain_q8_forward = 6400U;
  g_servo_adaptive_snapshot.confidence_permille = 500U;
  check("half confidence blends halfway to bounded gain",
        Servo_Adaptive_GetVelocityGain(base, true), 104);
  for (unsigned i = 0; i < 100U; i++)
  {
    (void)Servo_Adaptive_GetVelocityGain(base, true);
  }
  check("half confidence converges to blended target",
        g_servo_adaptive_applied_velocity_gain_q8_forward, 300);
  g_servo_adaptive_snapshot.speed_gain_q8_reverse = 25600U;
  g_servo_adaptive_snapshot.confidence_permille = 1000U;
  check("reverse direction has independent base seed",
        Servo_Adaptive_GetVelocityGain(base, false), 200);
  check("forward output remains independent",
        g_servo_adaptive_applied_velocity_gain_q8_forward, 300);
  for (unsigned i = 0; i < 100U; i++)
  {
    (void)Servo_Adaptive_GetVelocityGain(base, false);
  }
  check("reverse direction converges independently",
        g_servo_adaptive_applied_velocity_gain_q8_reverse, 100);

  g_servo_adaptive_snapshot.confidence_permille = 0U;
  check("zero confidence leaves feedforward at fallback",
        Servo_Adaptive_GetFeedForwardDuty(160, 200U, 1000U), 200);
  g_servo_adaptive_snapshot.confidence_permille = 500U;
  check("half confidence blends feedforward halfway",
        Servo_Adaptive_GetFeedForwardDuty(160, 200U, 1000U), 300);
  g_servo_adaptive_snapshot.confidence_permille = 1000U;
  check("full confidence applies model feedforward",
        Servo_Adaptive_GetFeedForwardDuty(160, 200U, 1000U), 400);
  g_servo_adaptive_snapshot.speed_gain_q8_forward = 1600U;
  check("feedforward is capped at maximum duty",
        Servo_Adaptive_GetFeedForwardDuty(160, 200U, 500U), 500);
  check("zero reference velocity returns fallback",
        Servo_Adaptive_GetFeedForwardDuty(0, 200U, 500U), 200);
  check("zero max duty returns fallback",
        Servo_Adaptive_GetFeedForwardDuty(160, 200U, 0U), 200);

  g_servo_adaptive_snapshot.speed_gain_q8_forward = 12800U;
  g_servo_adaptive_snapshot.reference_speed_gain_q8_forward = 12800U;
  g_servo_adaptive_snapshot.confidence_permille = 500U;
  g_servo_adaptive_snapshot.drive_sample_count_forward = 10U;
  Servo_Adaptive_Update(100U, 80, 200, true);
  Servo_Adaptive_Update(101U, 80, 200, true);
  check("one valid sample updates gain through 1/8 EMA",
        g_servo_adaptive_snapshot.speed_gain_q8_forward, 12000);
  check("confidence change is gradual",
        g_servo_adaptive_snapshot.confidence_permille, 495);

  printf("R07 control adjustment checks: %u passed, %u failed\n", checks - failures, failures);
  return failures ? 1 : 0;
}
