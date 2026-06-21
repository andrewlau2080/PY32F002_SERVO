#ifndef ADC_FEEDBACK_H
#define ADC_FEEDBACK_H

#include <stdint.h>

void ADC_Feedback_Init(void);
uint16_t ADC_Feedback_ReadRaw(void);
uint16_t ADC_Feedback_ReadFastRaw(void);
uint16_t ADC_Feedback_UpdateFiltered(void);
uint16_t ADC_Feedback_GetRaw(void);
uint16_t ADC_Feedback_GetFiltered(void);
uint16_t ADC_Feedback_GetVrefintRaw(void);
uint16_t ADC_Feedback_GetVddMv(void);

#endif
