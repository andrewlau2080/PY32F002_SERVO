#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include "py32f0xx_hal.h"

#define BOARD_PWM_IN_PORT       GPIOA
#define BOARD_PWM_IN_PIN        GPIO_PIN_3

#define BOARD_POT_ADC_PORT      GPIOA
#define BOARD_POT_ADC_PIN       GPIO_PIN_4
#define BOARD_POT_ADC_CHANNEL   ADC_CHANNEL_4

#define BOARD_HB_IN1_PORT       GPIOA
#define BOARD_HB_IN1_PIN        GPIO_PIN_1
#define BOARD_HB_IN2_PORT       GPIOA
#define BOARD_HB_IN2_PIN        GPIO_PIN_2

/* TSSOP20 debug wiring keeps SWD free and leaves PA7 available for LCDM TX.
 * CP2 drives CPG1/left high side through the original resistor network. */
#ifndef BOARD_TSSOP20_DEBUG_PINS
#define BOARD_TSSOP20_DEBUG_PINS 0
#endif

/* PA13/PA14 are SWDIO/SWCLK on PY32F002A. Keep them released by default so
 * the development board can be reprogrammed after the firmware starts. */
#ifndef BOARD_PRESERVE_SWD_PINS
#define BOARD_PRESERVE_SWD_PINS 1
#endif

/* Keep early startup quiet for SWD connect-under-reset. Set to 0 for a final
 * production image if the extra startup delay is not wanted. */
#ifndef BOARD_DEBUG_ATTACH_WINDOW_MS
#define BOARD_DEBUG_ATTACH_WINDOW_MS 200U
#endif

#if BOARD_TSSOP20_DEBUG_PINS
#define BOARD_HB_IN3_ENABLE     1
#define BOARD_HB_IN4_ENABLE     1
#elif BOARD_PRESERVE_SWD_PINS
#define BOARD_HB_IN3_ENABLE     0
#define BOARD_HB_IN4_ENABLE     0
#else
#define BOARD_HB_IN3_ENABLE     1
#define BOARD_HB_IN4_ENABLE     1
#endif

#if BOARD_TSSOP20_DEBUG_PINS
#define BOARD_HB_IN3_PORT       GPIOA
#define BOARD_HB_IN3_PIN        GPIO_PIN_6
#define BOARD_HB_IN4_PORT       GPIOB
#define BOARD_HB_IN4_PIN        GPIO_PIN_0
#else
#define BOARD_HB_IN3_PORT       GPIOA
#define BOARD_HB_IN3_PIN        GPIO_PIN_13
#define BOARD_HB_IN4_PORT       GPIOA
#define BOARD_HB_IN4_PIN        GPIO_PIN_14
#endif

void Board_Init(void);
void Board_MotorTimerInit(void);
void Board_StatusHeartbeat(void);
void Board_StatusError(void);
void Board_PotPowerRefresh(void);
uint32_t Board_Micros(void);
uint32_t Board_MicrosQ4(void);

#endif
