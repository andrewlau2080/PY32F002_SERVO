#ifndef SERVO_ADAPTIVE_H
#define SERVO_ADAPTIVE_H

#include <stdbool.h>
#include <stdint.h>

/* Reject learning intervals too long to represent a single control sample. */
#define SERVO_ADAPTIVE_MAX_SAMPLE_GAP_MS 4U

typedef struct
{
  uint16_t confidence_permille;
  uint16_t reference_speed_gain_q8_forward;
  uint16_t reference_speed_gain_q8_reverse;
  uint16_t speed_gain_q8_forward;
  uint16_t speed_gain_q8_reverse;
  uint16_t coast_decay_q15_forward;
  uint16_t coast_decay_q15_reverse;
  uint16_t brake_accel_q8_forward;
  uint16_t brake_accel_q8_reverse;
  uint16_t drive_sample_count_forward;
  uint16_t drive_sample_count_reverse;
  uint16_t brake_sample_count_forward;
  uint16_t brake_sample_count_reverse;
} ServoAdaptiveSnapshot;

extern volatile ServoAdaptiveSnapshot g_servo_adaptive_snapshot;
extern volatile uint16_t g_servo_adaptive_confidence_permille;
extern volatile uint16_t g_servo_adaptive_speed_gain_q8_forward;
extern volatile uint16_t g_servo_adaptive_speed_gain_q8_reverse;
extern volatile uint16_t g_servo_adaptive_brake_accel_q8_forward;
extern volatile uint16_t g_servo_adaptive_brake_accel_q8_reverse;
extern volatile uint16_t g_servo_adaptive_brake_samples_forward;
extern volatile uint16_t g_servo_adaptive_brake_samples_reverse;
extern volatile uint16_t g_servo_adaptive_drive_samples_forward;
extern volatile uint16_t g_servo_adaptive_drive_samples_reverse;
extern volatile uint16_t g_servo_adaptive_applied_velocity_gain_q8_forward;
extern volatile uint16_t g_servo_adaptive_applied_velocity_gain_q8_reverse;

void Servo_Adaptive_Init(void);
void Servo_Adaptive_Update(uint32_t now_ms,
                           int16_t velocity_q4,
                           int16_t applied_duty,
                           bool sample_valid);
void Servo_Adaptive_GetSnapshot(ServoAdaptiveSnapshot *snapshot);
uint16_t Servo_Adaptive_GetFeedForwardDuty(int16_t ref_velocity_q4,
                                           uint16_t fallback_duty,
                                           uint16_t max_duty);
uint16_t Servo_Adaptive_GetVelocityGain(uint16_t base_gain,
                                       bool forward_direction);

#endif
