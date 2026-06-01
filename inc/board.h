#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include "py32f0xx_hal.h"

#define BOARD_PWM_IN_PORT       GPIOA
#define BOARD_PWM_IN_PIN        GPIO_PIN_4

#define BOARD_POT_ADC_PORT      GPIOA
#define BOARD_POT_ADC_PIN       GPIO_PIN_3
#define BOARD_POT_ADC_CHANNEL   ADC_CHANNEL_3

#define BOARD_HB_IN1_PORT       GPIOA
#define BOARD_HB_IN1_PIN        GPIO_PIN_1
#define BOARD_HB_IN2_PORT       GPIOA
#define BOARD_HB_IN2_PIN        GPIO_PIN_2
#define BOARD_HB_IN3_PORT       GPIOA
#define BOARD_HB_IN3_PIN        GPIO_PIN_13
#define BOARD_HB_IN4_PORT       GPIOA
#define BOARD_HB_IN4_PIN        GPIO_PIN_14

extern TIM_HandleTypeDef g_motor_timer;

void Board_Init(void);
void Board_MotorTimerInit(void);
uint32_t Board_Micros(void);

#endif
