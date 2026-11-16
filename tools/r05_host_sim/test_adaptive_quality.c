#include <stdint.h>
#include <stdio.h>
#include "servo_adaptive.h"
#include "servo_model_store.h"

static ServoLearnedModel empty_model;
static unsigned failures;

const ServoLearnedModel *Servo_ModelStore_Get(void) { return &empty_model; }
bool Servo_ModelStore_IsValid(void) { return false; }

static void check(const char *name, int actual, int expected)
{
  if (actual == expected)
  {
    printf("PASS: %s (actual=%d expected=%d)\n", name, actual, expected);
  }
  else
  {
    printf("FAIL: %s (actual=%d expected=%d)\n", name, actual, expected);
    failures++;
  }
}

int main(void)
{
  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, 200, true);
  Servo_Adaptive_Update(101U, 160, 200, true);
  check("stable positive duty and velocity learn forward gain",
        g_servo_adaptive_snapshot.speed_gain_q8_forward, 12800);
  check("forward sample count advances", g_servo_adaptive_snapshot.drive_sample_count_forward, 1);
  check("reverse gain stays isolated", g_servo_adaptive_snapshot.drive_sample_count_reverse, 0);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, -160, -200, true);
  Servo_Adaptive_Update(101U, -160, -200, true);
  check("stable negative duty and velocity learn reverse gain",
        g_servo_adaptive_snapshot.speed_gain_q8_reverse, 12800);
  check("reverse sample count advances", g_servo_adaptive_snapshot.drive_sample_count_reverse, 1);
  check("forward gain stays isolated", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, 200, true);
  Servo_Adaptive_Update(101U, 160, 200, false);
  check("invalid sample is not learned", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);
  Servo_Adaptive_Update(102U, 160, 200, true);
  check("first valid sample after invalid only reseeds",
        g_servo_adaptive_snapshot.drive_sample_count_forward, 0);
  Servo_Adaptive_Update(103U, 160, 200, true);
  check("learning resumes after two valid samples",
        g_servo_adaptive_snapshot.drive_sample_count_forward, 1);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, -200, true);
  Servo_Adaptive_Update(101U, 160, -200, true);
  check("opposite signed duty is rejected", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, 200, true);
  Servo_Adaptive_Update(101U, 160, 300, true);
  check("unstable duty change is rejected", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, 200, true);
  Servo_Adaptive_Update(101U, 220, 200, true);
  check("large velocity transient is rejected", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 8, 200, true);
  Servo_Adaptive_Update(101U, 8, 200, true);
  check("below minimum speed is rejected", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, 200, true);
  Servo_Adaptive_Update(105U, 160, 200, true);
  check("long sample interval is rejected", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, 200, true);
  Servo_Adaptive_Update(101U, -160, 200, true);
  check("velocity reversal is rejected", g_servo_adaptive_snapshot.drive_sample_count_forward, 0);

  printf("R05 adaptive-quality checks: %u passed, %u failed\n", 15U - failures, failures);
  return failures ? 1 : 0;
}
