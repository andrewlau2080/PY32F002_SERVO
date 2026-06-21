#ifndef MAIN_H
#define MAIN_H

#include "py32f0xx_hal.h"

void APP_ErrorHandler(void);
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
void LCDM_Runtime_IRQHandler(void);
#endif

#endif
