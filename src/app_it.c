#include "main.h"
#include "board.h"
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

void EXTI4_15_IRQHandler(void)
{
  if (__HAL_GPIO_EXTI_GET_IT(BOARD_PWM_IN_PIN) != RESET)
  {
    __HAL_GPIO_EXTI_CLEAR_IT(BOARD_PWM_IN_PIN);
    PWM_Input_EdgeISR();
  }
}

void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&g_motor_timer);
}

#if SERVO_ENABLE_TJC_LCDM
void USART1_IRQHandler(void)
{
  TJC_LCDM_IRQHandler();
}
#endif
