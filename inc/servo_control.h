#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  SERVO_STATE_NO_SIGNAL = 0,
  SERVO_STATE_HOLD,
  SERVO_STATE_DRIVE,
  SERVO_STATE_STALL_PROTECT
} ServoState;

typedef struct
{
  ServoState state;
  uint16_t input_us;
  uint16_t target_adc;
  uint16_t feedback_adc;
  int16_t error_adc;
  int16_t motor_duty;
  bool input_valid;
} ServoStatus;

void Servo_Control_Init(void);
void Servo_Control_Update1ms(void);
ServoStatus Servo_Control_GetStatus(void);
uint16_t Servo_Control_MapPulseToAdc(uint16_t pulse_us);

#endif
