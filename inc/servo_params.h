#ifndef SERVO_PARAMS_H
#define SERVO_PARAMS_H

#include <stdbool.h>
#include <stdint.h>

#define SERVO_PARAM_FLAG_INVERTER          (1U << 0)
#define SERVO_PARAM_FLAG_OL_PROTECT        (1U << 1)
#define SERVO_PARAM_FLAG_PWM_100_PERCENT   (1U << 2)
#define SERVO_PARAM_FLAG_NARROW_BAND       (1U << 3)
#define SERVO_PARAM_FLAG_SOFT_START        (1U << 4)
#define SERVO_PARAM_FLAG_LOSE_PPM_ENABLE   (1U << 5)
#define SERVO_PARAM_FLAG_LOSE_PPM_LOCK     (1U << 6)

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint16_t pulse_lower_us;
  uint16_t pulse_center_us;
  uint16_t pulse_high_us;
  uint16_t adc_min_count;
  uint16_t adc_center_count;
  uint16_t adc_max_count;
  int16_t neutral_offset_count;
  int16_t left_range_count;
  int16_t right_range_count;
  uint16_t deadband_us;
  uint16_t deadband_count_min;
  uint16_t stretcher_q8;
  uint16_t max_duty;
  uint16_t boost_duty;
  uint16_t brake_band_count;
  uint16_t drive_frequency_hz;
  uint16_t forward_speed_q8;
  uint16_t reverse_speed_q8;
  uint16_t soft_start_step_count;
  uint16_t stall_duty_threshold;
  uint16_t stall_min_move_count;
  uint16_t stall_time_ms;
  uint16_t stall_recovery_ms;
  uint16_t input_timeout_ms;
  uint32_t flags;
  uint32_t crc32;
} ServoParams;

void Servo_Params_Init(void);
const ServoParams *Servo_Params_Get(void);
void Servo_Params_GetDefaults(ServoParams *params);
bool Servo_Params_Validate(const ServoParams *params);
bool Servo_Params_Save(const ServoParams *params);
uint32_t Servo_Params_Crc32(const ServoParams *params);

bool Servo_Params_FlagEnabled(uint32_t flag);

#endif
