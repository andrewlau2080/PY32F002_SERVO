#include "board.h"
#include "main.h"
#include "servo_config.h"

#ifndef BOARD_HSI_TRIM_OFFSET
#define BOARD_HSI_TRIM_OFFSET (-5)
#endif

#if BOARD_HSI_TRIM_OFFSET != 0
#define BOARD_HSI_24MHZ_CALIBRATION_VALUE ((uint32_t)(RCC_HSICALIBRATION_24MHz + BOARD_HSI_TRIM_OFFSET))
#else
#define BOARD_HSI_24MHZ_CALIBRATION_VALUE RCC_HSICALIBRATION_24MHz
#endif

static void Board_StatusLedInit(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_5;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* D2 is active-low on the TSSOP20 debug board. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

static void Board_PotPowerInit(void)
{
#if SERVO_ENABLE_PA0_POT_POWER
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);

  gpio.Pin = GPIO_PIN_0;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
#endif
}

void Board_PotPowerRefresh(void)
{
#if SERVO_ENABLE_PA0_POT_POWER
  GPIOA->BSRR = GPIO_PIN_0;
#endif
}

static void Board_SystemClockInit(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState = RCC_HSI_ON;
  osc.HSIDiv = RCC_HSI_DIV1;
  osc.HSICalibrationValue = BOARD_HSI_24MHZ_CALIBRATION_VALUE;

  if (HAL_RCC_OscConfig(&osc) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

static void Board_DebugAttachWindow(void)
{
#if BOARD_DEBUG_ATTACH_WINDOW_MS > 0U
  uint32_t start_ms = HAL_GetTick();

  while ((uint32_t)(HAL_GetTick() - start_ms) < BOARD_DEBUG_ATTACH_WINDOW_MS)
  {
    Board_StatusHeartbeat();
  }
#endif
}

void Board_Init(void)
{
  HAL_Init();
  Board_SystemClockInit();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  Board_PotPowerInit();
  Board_StatusLedInit();
  Board_DebugAttachWindow();
}

void Board_StatusHeartbeat(void)
{
  uint32_t phase = HAL_GetTick() % 500U;

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, (phase < 40U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void Board_StatusError(void)
{
  Board_StatusLedInit();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

void Board_MotorTimerInit(void)
{
  uint32_t timer_hz = SystemCoreClock;
  uint32_t prescaler = (timer_hz / 1000000U);

  if (prescaler == 0U)
  {
    prescaler = 1U;
  }

  __HAL_RCC_TIM16_CLK_ENABLE();
  TIM16->CR1 = 0U;
  TIM16->DIER = 0U;
  TIM16->PSC = prescaler - 1U;
  TIM16->ARR = HBRIDGE_PWM_TICK_US - 1U;
  TIM16->CNT = 0U;
  TIM16->SR = 0U;
  TIM16->DIER = TIM_DIER_UIE;
  TIM16->EGR = TIM_EGR_UG;
  TIM16->CR1 = TIM_CR1_CEN;
  HAL_NVIC_SetPriority(TIM16_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(TIM16_IRQn);
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

uint32_t Board_MicrosQ4(void)
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

  return (tick_1 * 16000U) +
         ((((load - 1U - val) * 16U) + (ticks_per_us / 2U)) / ticks_per_us);
}
