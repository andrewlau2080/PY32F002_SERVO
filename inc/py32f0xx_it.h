#ifndef PY32F0XX_IT_H
#define PY32F0XX_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void EXTI2_3_IRQHandler(void);
void EXTI4_15_IRQHandler(void);
void TIM1_CC_IRQHandler(void);
void TIM16_IRQHandler(void);
#if SERVO_ENABLE_TJC_LCDM || SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
void USART1_IRQHandler(void);
#endif

#endif
