#include <stdint.h>

#include <stdbool.h>

#include <stdio.h>

#include <string.h>

#include "servo_params.h"

#include "servo_config.h"

#define FLASH_PAGE_SIZE 128U

#define SERVO_PARAMS_MAGIC       0x53525650UL

#define SERVO_PARAMS_VERSION     3U

static ServoParams fake_flash;

#define SERVO_PARAMS_FLASH_ADDR ((uintptr_t)&fake_flash)

static ServoParams s_params;

static uint32_t crc32_update(uint32_t crc, uint8_t data)
{
  crc ^= data;
  for (uint32_t i = 0; i < 8U; i++)
  {
    if ((crc & 1U) != 0U)
    {
      crc = (crc >> 1U) ^ 0xEDB88320UL;
    }
    else
    {
      crc >>= 1U;
    }
  }
  return crc;
}

uint32_t Servo_Params_Crc32(const ServoParams *params)
{
  const uint8_t *bytes = (const uint8_t *)params;
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t crc_offset = (uint32_t)((const uint8_t *)&params->crc32 - bytes);

  for (uint32_t i = 0; i < sizeof(*params); i++)
  {
    if ((i >= crc_offset) && (i < (crc_offset + sizeof(params->crc32))))
    {
      crc = crc32_update(crc, 0U);
    }
    else
    {
      crc = crc32_update(crc, bytes[i]);
    }
  }

  return ~crc;
}

void Servo_Params_GetDefaults(ServoParams *params)
{
  /* 出厂默认参数：没有合法 Flash 参数时使用，也作为上位机模板。 */
  params->magic = SERVO_PARAMS_MAGIC;
  params->version = SERVO_PARAMS_VERSION;
  params->size = sizeof(*params);
  params->pulse_lower_us = SERVO_INPUT_RANGE_MIN_US;
  params->pulse_center_us = 1500U;
  params->pulse_high_us = SERVO_INPUT_RANGE_MAX_US;
  params->adc_min_count = SERVO_ADC_MIN_COUNT;
  params->adc_center_count = SERVO_ADC_CENTER_COUNT;
  params->adc_max_count = SERVO_ADC_MAX_COUNT;
  params->neutral_offset_count = 0;
  params->left_range_count = 0;
  params->right_range_count = 0;
  params->deadband_us = 2U;
  params->deadband_count_min = SERVO_ADC_DEADBAND_COUNT;
  params->stretcher_q8 = 384U;
  params->max_duty = SERVO_CONTROL_MAX_DUTY;
  params->boost_duty = SERVO_CONTROL_MIN_DUTY;
  params->brake_band_count = SERVO_CONTROL_BRAKE_BAND_COUNT;
  params->drive_frequency_hz = 1000U;
  params->forward_speed_q8 = 256U;
  params->reverse_speed_q8 = 256U;
  params->soft_start_step_count = 8U;
  params->stall_duty_threshold = SERVO_STALL_DUTY_THRESHOLD;
  params->stall_min_move_count = SERVO_STALL_MIN_MOVE_COUNT;
  params->stall_time_ms = SERVO_STALL_TIME_MS;
  params->stall_recovery_ms = SERVO_STALL_RECOVERY_MS;
  params->input_timeout_ms = SERVO_INPUT_TIMEOUT_MS;
  params->model_id = 0U;
  params->profile_id = 0U;
  params->startup_delay_ms = 200U;
  params->startup_input_stable_count = 5U;
  params->startup_input_stable_us = 3U;
  params->startup_adc_stable_count = 5U;
  params->startup_adc_stable_band_count = 4U;
  params->startup_step_count = 4U;
  params->adc_sample_count = SERVO_ADC_SAMPLE_COUNT;
  params->adc_filter_shift = SERVO_ADC_FILTER_SHIFT;
  params->adc_jump_limit_count = 0U;
  params->adc_noise_band_count = 4U;
  params->hold_mode = SERVO_HOLD_MODE_BRAKE_THEN_COAST;
  params->hold_exit_band_count = 16U;
  params->hold_brake_time_ms = 8U;
  params->hold_settle_ms = 20U;
  params->close_error_count = 24U;
  params->close_stretcher_q8 = 192U;
  params->close_boost_duty = 60U;
  params->approach_error_count = 120U;
  params->approach_stretcher_q8 = 320U;
  params->approach_boost_duty = 120U;
  params->reverse_pause_ms = 8U;
  params->reverse_brake_ms = 3U;
  params->vdd_nominal_mv = 3300U;
  params->vdd_warn_drop_mv = 250U;
  params->vdd_noise_band_mv = 80U;
  params->vdd_sample_interval_ms = 20U;
  params->servo_angle_deg = 180U;
  params->pot_angle_deg = 220U;
  params->lock_gain_q8 = 256U;
  params->counter_emf_gain_q8 = 256U;
  params->flags = SERVO_PARAM_FLAG_OL_PROTECT |
                  SERVO_PARAM_FLAG_SOFT_START |
                  SERVO_PARAM_FLAG_LOSE_PPM_LOCK;
  params->crc32 = 0U;
  params->crc32 = Servo_Params_Crc32(params);
}

bool Servo_Params_Validate(const ServoParams *params)
{
  /* 先检查身份、版本、长度和 CRC，再检查各参数是否在安全范围内。 */
  if ((params->magic != SERVO_PARAMS_MAGIC) ||
      (params->version != SERVO_PARAMS_VERSION) ||
      (params->size != sizeof(*params)) ||
      (params->crc32 != Servo_Params_Crc32(params)))
  {
    return false;
  }

  if ((params->pulse_lower_us < SERVO_INPUT_RANGE_MIN_US) ||
      (params->pulse_high_us > SERVO_INPUT_RANGE_MAX_US) ||
      (params->pulse_lower_us >= params->pulse_center_us) ||
      (params->pulse_center_us >= params->pulse_high_us))
  {
    return false;
  }

  if ((params->adc_min_count >= params->adc_center_count) ||
      (params->adc_center_count >= params->adc_max_count) ||
      (params->adc_max_count > 4095U))
  {
    return false;
  }

  if ((params->max_duty == 0U) ||
      (params->max_duty > HBRIDGE_DUTY_MAX) ||
      (params->boost_duty > params->max_duty) ||
      (params->close_boost_duty > params->max_duty) ||
      (params->approach_boost_duty > params->max_duty) ||
      (params->drive_frequency_hz < 100U) ||
      (params->drive_frequency_hz > 20000U))
  {
    return false;
  }

  if ((params->adc_sample_count == 0U) ||
      (params->adc_sample_count > 32U) ||
      (params->adc_filter_shift > 8U) ||
      (params->adc_jump_limit_count > 4095U) ||
      (params->hold_mode > SERVO_HOLD_MODE_BRAKE_THEN_COAST) ||
      (params->hold_exit_band_count > 512U) ||
      (params->close_error_count > params->approach_error_count) ||
      (params->approach_error_count > 2048U) ||
      (params->close_stretcher_q8 > 2048U) ||
      (params->approach_stretcher_q8 > 2048U) ||
      (params->stretcher_q8 > 2048U) ||
      (params->reverse_brake_ms > params->reverse_pause_ms) ||
      (params->vdd_nominal_mv < 1800U) ||
      (params->vdd_nominal_mv > 5500U) ||
      (params->vdd_warn_drop_mv > 2000U) ||
      (params->vdd_noise_band_mv > 1000U) ||
      (params->servo_angle_deg == 0U) ||
      (params->servo_angle_deg > 360U) ||
      (params->pot_angle_deg == 0U) ||
      (params->pot_angle_deg > 360U) ||
      (params->lock_gain_q8 > 2048U) ||
      (params->counter_emf_gain_q8 > 2048U))
  {
    return false;
  }

  if (sizeof(*params) > FLASH_PAGE_SIZE)
  {
    return false;
  }

  return true;
}

void Servo_Params_Init(void)
{
  const ServoParams *flash_params = (const ServoParams *)SERVO_PARAMS_FLASH_ADDR;

  /* 上电优先使用最后一页 Flash 参数；无效则回退到默认值。 */
  if (Servo_Params_Validate(flash_params))
  {
    s_params = *flash_params;
  }
  else
  {
    Servo_Params_GetDefaults(&s_params);
  }
}

const ServoParams *Servo_Params_Get(void)
{
  return &s_params;
}


static unsigned failures;
static void check(const char *name, int actual, int expected)
{
  printf("%s: %s (actual=%d expected=%d)\n",
         (actual == expected) ? "PASS" : "FAIL", name, actual, expected);
  if (actual != expected) { failures++; }
}
int main(void)
{
  ServoParams old;
  memset(&fake_flash, 0xff, sizeof(fake_flash));
  Servo_Params_Init();
  check("blank flash defaults lower endpoint to 500 us", Servo_Params_Get()->pulse_lower_us, 500);
  check("blank flash defaults center to 1500 us", Servo_Params_Get()->pulse_center_us, 1500);
  check("blank flash defaults upper endpoint to 2500 us", Servo_Params_Get()->pulse_high_us, 2500);
  check("new default block validates", Servo_Params_Validate(Servo_Params_Get()), 1);
  old = *Servo_Params_Get();
  old.pulse_lower_us = 900U;
  old.pulse_high_us = 2100U;
  old.crc32 = 0U;
  old.crc32 = Servo_Params_Crc32(&old);
  check("existing 900/2100 calibration remains valid", Servo_Params_Validate(&old), 1);
  fake_flash = old;
  Servo_Params_Init();
  check("existing lower calibration is retained", Servo_Params_Get()->pulse_lower_us, 900);
  check("existing upper calibration is retained", Servo_Params_Get()->pulse_high_us, 2100);
  check("existing calibration CRC is retained", Servo_Params_Validate(Servo_Params_Get()), 1);
  fake_flash.pulse_lower_us = 901U;
  check("modified calibration fails CRC", Servo_Params_Validate(&fake_flash), 0);
  printf("Flash is a simulated RAM object; no physical Flash write was performed.\n");
  return failures ? 1 : 0;
}

