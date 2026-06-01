#include "board.h"
#include "hbridge.h"
#include "main.h"
#include "servo_config.h"

TIM_HandleTypeDef g_motor_timer;

void Board_Init(void)
{
  HAL_Init();
  __HAL_RCC_GPIOA_CLK_ENABLE();
}

void Board_MotorTimerInit(void)
{
  uint32_t timer_hz = SystemCoreClock;
  uint32_t prescaler = (timer_hz / 1000000U);

  if (prescaler == 0U)
  {
    prescaler = 1U;
  }

  g_motor_timer.Instance = TIM1;
  g_motor_timer.Init.Period = HBRIDGE_PWM_TICK_US - 1U;
  g_motor_timer.Init.Prescaler = prescaler - 1U;
  g_motor_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  g_motor_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  g_motor_timer.Init.RepetitionCounter = 0U;
  g_motor_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&g_motor_timer) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  if (HAL_TIM_Base_Start_IT(&g_motor_timer) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

uint32_t Board_Micros(void)
{
  uint32_t tick_1;
  uint32_t tick_2;
  uint32_t val;
  uint32_t load = SysTick->LOAD + 1U;
  uint32_t ticks_per_us = SystemCoreClock / 1000000U;

  if (ticks_per_us == 0U)
  {
    ticks_per_us = 1U;
  }

  do
  {
    tick_1 = HAL_GetTick();
    val = SysTick->VAL;
    tick_2 = HAL_GetTick();
  } while (tick_1 != tick_2);

  return (tick_1 * 1000U) + ((load - 1U - val) / ticks_per_us);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    HBridge_TickISR();
  }
}
