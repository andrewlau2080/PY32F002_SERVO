#ifndef ADC_FEEDBACK_H
#define ADC_FEEDBACK_H

#include <stdbool.h>
#include <stdint.h>

void ADC_Feedback_Init(void);
uint16_t ADC_Feedback_ReadRaw(void);
uint16_t ADC_Feedback_ReadFastRaw(void);
uint16_t ADC_Feedback_UpdateFiltered(void);
uint16_t ADC_Feedback_GetRaw(void);
uint16_t ADC_Feedback_GetFiltered(void);
uint16_t ADC_Feedback_GetVrefintRaw(void);
uint16_t ADC_Feedback_GetVddMv(void);
uint32_t ADC_Feedback_GetVddSampleMs(void);
uint16_t ADC_Feedback_GetControlValue(uint16_t raw_adc,
                                      bool supply_is_ratiometric,
                                      uint16_t nominal_vdd_mv,
                                      uint16_t max_sample_age_ms);
uint16_t ADC_Feedback_ApplyVddCompensation(uint16_t raw_adc,
                                           uint16_t nominal_vdd_mv,
                                           uint16_t max_sample_age_ms);
bool ADC_Feedback_IsVddBelow(uint16_t nominal_vdd_mv,
                             uint16_t warn_drop_mv,
                             uint16_t max_sample_age_ms);

#endif
