#ifndef ADC_FEEDBACK_H
#define ADC_FEEDBACK_H

#include <stdint.h>

void ADC_Feedback_Init(void);
uint16_t ADC_Feedback_ReadRaw(void);
uint16_t ADC_Feedback_UpdateFiltered(void);
uint16_t ADC_Feedback_GetRaw(void);
uint16_t ADC_Feedback_GetFiltered(void);

#endif
