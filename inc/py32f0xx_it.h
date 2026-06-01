#ifndef PY32F0XX_IT_H
#define PY32F0XX_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void EXTI4_15_IRQHandler(void);
void TIM1_BRK_UP_TRG_COM_IRQHandler(void);
#if SERVO_ENABLE_TJC_LCDM
void USART1_IRQHandler(void);
#endif

#endif
