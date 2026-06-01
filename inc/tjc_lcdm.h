#ifndef TJC_LCDM_H
#define TJC_LCDM_H

#include <stdint.h>

#ifndef SERVO_ENABLE_TJC_LCDM
#define SERVO_ENABLE_TJC_LCDM 0
#endif

#define TJC_LCDM_DEFAULT_BAUD       115200UL
#define TJC_LCDM_REFRESH_MS         100U

void TJC_LCDM_Init(void);
void TJC_LCDM_Process(void);
void TJC_LCDM_ForceRefresh(void);
void TJC_LCDM_IRQHandler(void);

#endif
