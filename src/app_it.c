#include "main.h"
#include "board.h"
#include "hbridge.h"
#include "pwm_input.h"
#include "py32f0xx_it.h"
#include "tjc_lcdm.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
  APP_ErrorHandler();
}

void SVC_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

void EXTI2_3_IRQHandler(void)
{
#if !SERVO_ENABLE_INTERNAL_STEP_TEST
  if (__HAL_GPIO_EXTI_GET_IT(BOARD_PWM_IN_PIN) != RESET)
  {
    __HAL_GPIO_EXTI_CLEAR_IT(BOARD_PWM_IN_PIN);
    PWM_Input_EdgeISR();
  }
#endif
}

void EXTI4_15_IRQHandler(void)
{
}

void TIM1_CC_IRQHandler(void)
{
#if !SERVO_ENABLE_INTERNAL_STEP_TEST
  PWM_Input_EdgeISR();
#endif
}

void TIM16_IRQHandler(void)
{
  if ((TIM16->SR & TIM_SR_UIF) != 0U)
  {
    TIM16->SR &= ~TIM_SR_UIF;
    HBridge_TickISR();
  }
}

#if SERVO_ENABLE_TJC_LCDM || SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
void USART1_IRQHandler(void)
{
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  LCDM_Runtime_IRQHandler();
#else
  TJC_LCDM_IRQHandler();
#endif
}
#endif
