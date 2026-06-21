#ifndef SERVO_CONFIG_H
#define SERVO_CONFIG_H

#include <stdint.h>

#define SERVO_INPUT_MIN_US              1000U
#define SERVO_INPUT_CENTER_US           1500U
#define SERVO_INPUT_MAX_US              2000U
#define SERVO_INPUT_CAPTURE_MIN_US      300U
#define SERVO_INPUT_CAPTURE_MAX_US      2700U
#define SERVO_INPUT_FAILSAFE_US         SERVO_INPUT_CENTER_US
#define SERVO_INPUT_FRAME_MIN_US        18000U
#define SERVO_INPUT_FRAME_MAX_US        22000U
#define SERVO_INPUT_TIMEOUT_MS          100U

#define SERVO_ADC_MIN_COUNT             600U
#define SERVO_ADC_CENTER_COUNT          2048U
#define SERVO_ADC_MAX_COUNT             3495U
#define SERVO_ADC_DEADBAND_COUNT        8
#define SERVO_ADC_FILTER_SHIFT          3U
#define SERVO_ADC_SAMPLE_COUNT          4U

#define SERVO_CONTROL_PERIOD_MS         1U
#define SERVO_CONTROL_KP_NUM            3
#define SERVO_CONTROL_KP_DEN            2
#define SERVO_CONTROL_MIN_DUTY          160
#define SERVO_CONTROL_MAX_DUTY          850
#define SERVO_CONTROL_BRAKE_BAND_COUNT  3

#define SERVO_STALL_ENABLE              1
#define SERVO_STALL_DUTY_THRESHOLD      500
#define SERVO_STALL_MIN_MOVE_COUNT      4
#define SERVO_STALL_TIME_MS             300U
#define SERVO_STALL_RECOVERY_MS         500U

#ifndef HBRIDGE_PWM_PERIOD_US
#define HBRIDGE_PWM_PERIOD_US           1000U
#endif
#ifndef HBRIDGE_PWM_TICK_US
#define HBRIDGE_PWM_TICK_US             20U
#endif
#ifndef HBRIDGE_DUTY_MAX
#define HBRIDGE_DUTY_MAX                1000
#endif

#define SERVO_FIXED_PROGRAM_VERSION     1U

#endif
