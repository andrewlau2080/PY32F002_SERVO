#ifndef SERVO_MODEL_STORE_H
#define SERVO_MODEL_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define SERVO_MODEL_CONFIDENCE_MAX 1000U

/* 持久化的是已确认的机构响应摘要，不是每周期控制器的瞬时状态。 */
typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t generation;
  uint32_t calibration_fingerprint;
  uint16_t model_id;
  uint16_t profile_id;
  uint16_t confidence_permille;
  uint16_t motion_sample_count;
  uint16_t brake_sample_count;
  uint16_t successful_move_count;
  uint16_t rejected_sample_count;
  uint16_t adc_noise_band_count;
  uint16_t start_duty_forward;
  uint16_t start_duty_reverse;
  uint16_t speed_gain_q8_forward;
  uint16_t speed_gain_q8_reverse;
  uint16_t velocity_decay_q15_forward;
  uint16_t velocity_decay_q15_reverse;
  uint16_t brake_accel_q8_forward;
  uint16_t brake_accel_q8_reverse;
  uint16_t max_speed_q4_forward;
  uint16_t max_speed_q4_reverse;
  uint16_t mean_abs_stop_error_count;
  uint16_t p95_overshoot_count;
  uint16_t mean_settle_time_ms;
  uint16_t reference_motor_supply_mv;
  uint32_t crc32;
} ServoLearnedModel;

void Servo_ModelStore_Init(void);
void Servo_ModelStore_InitFor(uint16_t expected_model_id,
                              uint16_t expected_profile_id,
                              uint32_t expected_calibration_fingerprint,
                              uint16_t expected_motor_supply_mv,
                              uint16_t supply_tolerance_mv);
const ServoLearnedModel *Servo_ModelStore_Get(void);
bool Servo_ModelStore_IsValid(void);
bool Servo_ModelStore_Validate(const ServoLearnedModel *model);

/* Only commit a qualified model while the motor is confirmed stationary. */
bool Servo_ModelStore_Save(const ServoLearnedModel *model,
                           bool qualified_and_stationary);

#endif
