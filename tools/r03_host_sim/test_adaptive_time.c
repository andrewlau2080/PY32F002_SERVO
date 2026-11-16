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

static void forward_brake(uint32_t dt_ms, uint16_t expected)
{
  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, -200, true);
  Servo_Adaptive_Update(100U + dt_ms, 144, -200, true);
  check("forward brake rate uses elapsed milliseconds",
        g_servo_adaptive_snapshot.brake_accel_q8_forward, expected);
  check("forward brake sample counted",
        g_servo_adaptive_snapshot.brake_sample_count_forward, 1);
}

static void reverse_brake(uint32_t dt_ms, uint16_t expected)
{
  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, -160, 200, true);
  Servo_Adaptive_Update(100U + dt_ms, -144, 200, true);
  check("reverse brake rate uses elapsed milliseconds",
        g_servo_adaptive_snapshot.brake_accel_q8_reverse, expected);
  check("reverse brake sample counted",
        g_servo_adaptive_snapshot.brake_sample_count_reverse, 1);
}

int main(void)
{
  forward_brake(1U, 256U);
  forward_brake(2U, 128U);
  forward_brake(4U, 64U);
  reverse_brake(1U, 256U);
  reverse_brake(2U, 128U);
  reverse_brake(4U, 64U);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(10U, 160, -200, true);
  Servo_Adaptive_Update(10U, 144, -200, true);
  check("same-tick sample is rejected", g_servo_adaptive_snapshot.brake_sample_count_forward, 0);
  Servo_Adaptive_Update(12U, 128, -200, true);
  check("same-tick sample reseeds following interval",
        g_servo_adaptive_snapshot.brake_accel_q8_forward, 128);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, -200, true);
  Servo_Adaptive_Update(105U, 144, -200, true);
  check("gap above maximum is rejected", g_servo_adaptive_snapshot.brake_sample_count_forward, 0);
  Servo_Adaptive_Update(107U, 128, -200, true);
  check("long gap reseeds following interval",
        g_servo_adaptive_snapshot.brake_accel_q8_forward, 128);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(0xFFFFFFFEUL, 160, -200, true);
  Servo_Adaptive_Update(0U, 144, -200, true);
  check("tick wrap preserves 2 ms interval",
        g_servo_adaptive_snapshot.brake_accel_q8_forward, 128);

  Servo_Adaptive_Init();
  Servo_Adaptive_Update(100U, 160, -200, true);
  Servo_Adaptive_Update(101U, 144, -200, false);
  check("invalid sample does not learn braking",
        g_servo_adaptive_snapshot.brake_sample_count_forward, 0);
  Servo_Adaptive_Update(102U, 144, -200, true);
  Servo_Adaptive_Update(104U, 128, -200, true);
  check("valid samples after invalid input reseed",
        g_servo_adaptive_snapshot.brake_accel_q8_forward, 128);

  printf("R03 adaptive time checks: %u passed, %u failed\n", 19U - failures, failures);
  return failures ? 1 : 0;
}
