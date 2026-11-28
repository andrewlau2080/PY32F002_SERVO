/* Check the production generic pulse-to-position mapping for both calibrations. */
#include <stdio.h>
#include "servo_control.h"
#include "servo_adaptive.h"
#include "servo_model_store.h"
#include "servo_params.h"
#include "servo_config.h"

static ServoParams test_params;
static unsigned failures;
static uint16_t test_command_us = 1500U;
static uint32_t test_time_ms;

uint32_t HAL_GetTick(void) { return test_time_ms; }
const ServoParams *Servo_Params_Get(void) { return &test_params; }
bool Servo_Params_FlagEnabled(uint32_t flag) { return (test_params.flags & flag) != 0U; }
const ServoLearnedModel *Servo_ModelStore_Get(void) { return NULL; }
bool Servo_ModelStore_IsValid(void) { return false; }
void Servo_ModelStore_Init(void) {}
bool PWM_Input_IsFresh(void) { return true; }
uint16_t PWM_Input_GetCommandUs(void) { return test_command_us; }
uint16_t ADC_Feedback_UpdateFiltered(void) { return 2048U; }
uint16_t ADC_Feedback_GetFiltered(void) { return 2048U; }
bool ADC_Feedback_IsVddBelow(uint16_t nominal_mv, uint16_t drop_mv, uint32_t max_age_ms)
{ (void)nominal_mv; (void)drop_mv; (void)max_age_ms; return false; }
void HBridge_SetFrequency(uint16_t frequency_hz) { (void)frequency_hz; }
void HBridge_SetSignedDuty(int16_t duty) { (void)duty; }
void HBridge_Coast(void) {}
void HBridge_Brake(void) {}
int16_t HBridge_GetDuty(void) { return 0; }

static void check(const char *name, int actual, int expected)
{
  printf("%s: %s (actual=%d expected=%d)\n",
         (actual == expected) ? "PASS" : "FAIL", name, actual, expected);
  if (actual != expected) { failures++; }
}

int main(void)
{
  test_params.pulse_lower_us = SERVO_INPUT_RANGE_MIN_US;
  test_params.pulse_center_us = SERVO_INPUT_CENTER_US;
  test_params.pulse_high_us = SERVO_INPUT_RANGE_MAX_US;
  test_params.adc_min_count = SERVO_ADC_MIN_COUNT;
  test_params.adc_center_count = SERVO_ADC_CENTER_COUNT;
  test_params.adc_max_count = SERVO_ADC_MAX_COUNT;
  check("500 us maps to generic minimum", Servo_Control_MapPulseToAdc(500), 600);
  check("1000 us maps to first half of operating span", Servo_Control_MapPulseToAdc(1000), 1324);
  check("1500 us maps to generic center", Servo_Control_MapPulseToAdc(1500), 2048);
  check("2000 us maps to second half of operating span", Servo_Control_MapPulseToAdc(2000), 2771);
  check("2500 us maps to generic maximum", Servo_Control_MapPulseToAdc(2500), 3495);
  test_params.pulse_lower_us = 900U;
  test_params.pulse_high_us = 2100U;
  check("saved 900 us calibration retains its endpoint", Servo_Control_MapPulseToAdc(900), 600);
  check("saved 2100 us calibration retains its endpoint", Servo_Control_MapPulseToAdc(2100), 3495);
  test_params.pulse_lower_us = SERVO_INPUT_RANGE_MIN_US;
  test_params.pulse_high_us = SERVO_INPUT_RANGE_MAX_US;
  test_params.deadband_count_min = SERVO_ADC_DEADBAND_COUNT;

  for (int dir = -1; dir <= 1; dir += 2)
  {
    test_command_us = 1500U;
    Servo_Control_Init();
    Servo_Control_Update1ms();
    int start_adc = Servo_Control_GetStatus().target_adc;
    for (int n = 1; n <= 100; ++n)
    {
      test_command_us = (uint16_t)(1500 + dir * n);
      test_time_ms += 20U;
      Servo_Control_Update1ms();
      if (n == 3)
        check("three cumulative 1 us increments map to a signed center displacement",
              Servo_Control_GetStatus().target_adc - start_adc, dir > 0 ? 4 : -5);
    }
    uint16_t accumulated_target = Servo_Control_GetStatus().target_adc;
    test_command_us = 1500U;
    Servo_Control_Init();
    Servo_Control_Update1ms();
    test_command_us = (uint16_t)(1500 + dir * 100);
    test_time_ms += 20U;
    Servo_Control_Update1ms();
    check("production target is identical for 100 incremental steps and one 100 us command",
          Servo_Control_GetStatus().target_adc, accumulated_target);
  }
  printf("Generic mapping only; physical angles and LCDM control are not tested.\n");
  return failures ? 1 : 0;
}
