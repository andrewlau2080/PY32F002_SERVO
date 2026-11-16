#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "servo_control.h"
#include "servo_params.h"

static ServoParams params;
static uint32_t now_ms;
static uint16_t feedback = 2000U;
static bool input_valid = true;
static int16_t measured_velocity, duty;
static bool measured_valid;
static unsigned failures;

uint32_t HAL_GetTick(void) { return now_ms; }
const ServoParams *Servo_Params_Get(void) { return &params; }
bool Servo_Params_FlagEnabled(uint32_t f) { return (params.flags & f) != 0U; }
bool PWM_Input_IsFresh(void) { return input_valid; }
uint16_t PWM_Input_GetCommandUs(void) { return 1500U; }
uint16_t ADC_Feedback_UpdateFiltered(void) { return feedback; }
uint16_t ADC_Feedback_GetFiltered(void) { return feedback; }
void HBridge_SetFrequency(uint16_t f) { (void)f; }
void HBridge_SetSignedDuty(int16_t d) { duty = d; }
void HBridge_Coast(void) { duty = 0; }
void HBridge_Brake(void) { duty = 0; }
int16_t HBridge_GetDuty(void) { return duty; }
void Servo_Adaptive_Update(uint32_t time_ms, int16_t velocity_q4, int16_t output, bool valid)
{ (void)time_ms; (void)output; measured_velocity = velocity_q4; measured_valid = valid; }

static void check(const char *name, int actual, int expected)
{
  int pass = actual == expected;
  printf("%s: %s (actual=%d expected=%d)\n", pass ? "PASS" : "FAIL", name, actual, expected);
  if (!pass) failures++;
}
static void run(uint32_t time_ms, uint16_t position, bool valid)
{ now_ms = time_ms; feedback = position; input_valid = valid; Servo_Control_Update1ms(); }

int main(void)
{
  params.pulse_lower_us = 500U; params.pulse_center_us = 1500U; params.pulse_high_us = 2500U;
  params.adc_min_count = 600U; params.adc_center_count = 2048U; params.adc_max_count = 3495U;
  params.max_duty = 1000U; params.forward_speed_q8 = 256U; params.reverse_speed_q8 = 256U;
  now_ms = 1000U; feedback = 2000U; Servo_Control_Init();
  run(1000U, 2000U, true);
  check("first position establishes baseline", measured_valid, false);
  run(1001U, 2002U, true);
  check("+2 counts in 1 ms", measured_velocity, 32);
  check("1 ms interval accepted", measured_valid, true);
  run(1003U, 2006U, true);
  check("+4 counts in 2 ms", measured_velocity, 32);
  check("2 ms interval accepted", measured_valid, true);
  run(1007U, 2002U, true);
  check("-4 counts in 4 ms", measured_velocity, -16);
  run(1007U, 2006U, true);
  check("same millisecond rejected", measured_valid, false);
  run(1008U, 2008U, true);
  check("same-tick position retained for next interval", measured_velocity, 96);
  check("next valid interval accepted", measured_valid, true);
  run(1015U, 2015U, true);
  check("gap above 4 ms rejected", measured_valid, false);
  run(1016U, 2014U, true);
  check("velocity reseeded after long gap", measured_velocity, -16);
  check("first short interval after gap accepted", measured_valid, true);
  run(1017U, 2015U, false);
  check("invalid input freezes model sample", measured_valid, false);
  run(1018U, 2016U, true);
  check("input recovery reseeds velocity", measured_valid, false);
  run(1019U, 2017U, true);
  check("velocity resumes after input recovery", measured_velocity, 16);
  check("recovered interval accepted", measured_valid, true);
  now_ms = 0xFFFFFFFEU; feedback = 2000U; input_valid = true; Servo_Control_Init();
  run(0xFFFFFFFEU, 2000U, true);
  run(0U, 2004U, true);
  check("tick wrap with 2 ms interval", measured_velocity, 32);
  check("tick wrap interval accepted", measured_valid, true);
  now_ms = 100U; feedback = 0U; Servo_Control_Init();
  run(100U, 0U, true);
  run(101U, 4095U, true);
  check("positive full-scale velocity saturates", measured_velocity, 32767);
  run(102U, 0U, true);
  check("negative full-scale velocity saturates", measured_velocity, -32768);
  return failures ? 1 : 0;
}
