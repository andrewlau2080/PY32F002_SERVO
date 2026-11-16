/*
 * Host-side plant simulation for the production servo_control.c and
 * servo_adaptive.c. The plant below is intentionally simple; results validate
 * software flow under stated assumptions, not the real motor or H-bridge.
 */
#include "servo_control.h"
#include "servo_adaptive.h"
#include "servo_model_store.h"
#include "servo_params.h"
#include "adc_feedback.h"
#include "hbridge.h"
#include "pwm_input.h"

#include <stdio.h>
#include <stdlib.h>

static ServoParams params;
static uint32_t now_ms;
static uint16_t command_us = 1500U;
static uint16_t feedback_adc = 2048U;
static int16_t bridge_duty;
static int32_t velocity_q8;

uint32_t HAL_GetTick(void) { return now_ms; }
const ServoLearnedModel *Servo_ModelStore_Get(void) { return NULL; }
bool Servo_ModelStore_IsValid(void) { return false; }
void Servo_ModelStore_Init(void) {}
const ServoParams *Servo_Params_Get(void) { return &params; }
bool Servo_Params_FlagEnabled(uint32_t flag) { return (params.flags & flag) != 0U; }
bool PWM_Input_IsFresh(void) { return true; }
uint16_t PWM_Input_GetCommandUs(void) { return command_us; }
uint16_t ADC_Feedback_UpdateFiltered(void) { return feedback_adc; }
uint16_t ADC_Feedback_GetFiltered(void) { return feedback_adc; }
void HBridge_SetFrequency(uint16_t frequency_hz) { (void)frequency_hz; }
void HBridge_SetSignedDuty(int16_t duty) { bridge_duty = duty; }
void HBridge_Coast(void) { bridge_duty = 0; }
void HBridge_Brake(void) { bridge_duty = 0; }
int16_t HBridge_GetDuty(void) { return bridge_duty; }

static void init_params(void)
{
  params.pulse_lower_us = 500U;
  params.pulse_center_us = 1500U;
  params.pulse_high_us = 2500U;
  params.adc_min_count = 200U;
  params.adc_center_count = 2048U;
  params.adc_max_count = 3900U;
  params.deadband_count_min = 8U;
  params.deadband_us = 3U;
  params.max_duty = 850U;
  params.boost_duty = 180U;
  params.stretcher_q8 = 170U;
  params.forward_speed_q8 = 256U;
  params.reverse_speed_q8 = 256U;
  params.soft_start_step_count = 16U;
  params.startup_input_stable_count = 1U;
  params.startup_adc_stable_count = 1U;
  params.startup_input_stable_us = 2U;
  params.startup_adc_stable_band_count = 3U;
  params.close_error_count = 20U;
  params.close_stretcher_q8 = 120U;
  params.close_boost_duty = 100U;
  params.approach_error_count = 100U;
  params.approach_stretcher_q8 = 150U;
  params.approach_boost_duty = 140U;
  params.hold_exit_band_count = 10U;
  params.hold_settle_ms = 20U;
  params.hold_mode = SERVO_HOLD_MODE_COAST;
  params.stall_duty_threshold = 900U;
  params.stall_min_move_count = 2U;
  params.stall_time_ms = 100U;
  params.reverse_pause_ms = 0U;
  params.flags = SERVO_PARAM_FLAG_SOFT_START;
}

static void plant_step(void)
{
  /* First-order motor model with inertia, static friction, and hard travel. */
  int32_t drive_q8 = (int32_t)bridge_duty * 3;
  velocity_q8 += drive_q8 - (velocity_q8 / 5);
  if ((bridge_duty == 0) && (abs(velocity_q8) < 48)) velocity_q8 = 0;
  int32_t position = (int32_t)feedback_adc + (velocity_q8 / 256);
  if (position < 200) { position = 200; velocity_q8 = 0; }
  if (position > 3900) { position = 3900; velocity_q8 = 0; }
  feedback_adc = (uint16_t)position;
}

int main(int argc, char **argv)
{
  static const uint16_t moves[][2] = {
    {1000U,1500U},{1500U,2000U},{2000U,1500U},
    {1500U,1000U},{1000U,2000U},{2000U,1000U}
  };
  FILE *out;
  if (argc != 2) { fprintf(stderr, "usage: %s output.csv\n", argv[0]); return 2; }
  out = fopen(argv[1], "w");
  if (!out) { perror(argv[1]); return 2; }
  init_params();
  Servo_Adaptive_Init();
  Servo_Control_Init();
  fprintf(out, "time_ms,segment,start_pwm_us,end_pwm_us,pwm_us,target_adc,feedback_adc,error_adc,duty,state,model_confidence\n");

  /* Settle startup, then place at each start point before its test transition. */
  for (now_ms = 0U; now_ms < 30U; ++now_ms) {
    Servo_Control_Update1ms(); plant_step();
  }
  for (unsigned seg = 0; seg < 6; ++seg) {
    command_us = moves[seg][0];
    for (unsigned step = 0; step < 700U; ++step, ++now_ms) {
      Servo_Control_Update1ms(); plant_step();
      ServoStatus s = Servo_Control_GetStatus();
      if (step > 300U && s.state == SERVO_STATE_HOLD && abs(s.error_adc) <= 10) break;
    }
    command_us = moves[seg][1];
    for (unsigned step = 0; step < 900U; ++step, ++now_ms) {
      Servo_Control_Update1ms();
      ServoStatus s = Servo_Control_GetStatus();
      fprintf(out, "%lu,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u\n",
              (unsigned long)now_ms, seg, moves[seg][0], moves[seg][1], command_us,
              s.target_adc,
              s.feedback_adc, s.error_adc, s.motor_duty, (unsigned)s.state,
              (unsigned)g_servo_adaptive_snapshot.confidence_permille);
      plant_step();
      if (step > 300U && s.state == SERVO_STATE_HOLD && abs(s.error_adc) <= 10) break;
    }
  }
  fclose(out);
  printf("host simulation CSV: %s\n", argv[1]);
  return 0;
}
