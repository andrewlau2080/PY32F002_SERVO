#ifndef TJC_LCDM_H
#define TJC_LCDM_H

#include <stdint.h>

#ifndef SERVO_ENABLE_TJC_LCDM
#define SERVO_ENABLE_TJC_LCDM 0
#endif

#ifndef TJC_LCDM_DEFAULT_BAUD
#define TJC_LCDM_DEFAULT_BAUD       115200UL
#endif
#define TJC_LCDM_REFRESH_MS         100U

void TJC_LCDM_Init(void);
void TJC_LCDM_Process(void);
void TJC_LCDM_ForceRefresh(void);
void TJC_LCDM_IRQHandler(void);
#if defined(SERVO_R01_PRODUCTION_LOOP_TEST) && SERVO_R01_PRODUCTION_LOOP_TEST
void TJC_LCDM_ShowR01Status(uint16_t pulse_us, uint16_t feedback_adc,
                            uint16_t target_adc, int16_t error_adc);
#endif
#endif
