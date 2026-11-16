#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "board.h"
#include "hbridge.h"

GPIO_TypeDef gpioa, gpiob;
TIM_TypeDef tim1;
RCC_TypeDef rcc;
uint32_t SystemCoreClock = 24000000U;

void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t mask, GPIO_PinState state)
{
  if (state == GPIO_PIN_SET) { p->ODR |= mask; p->IDR |= mask; }
  else { p->ODR &= ~(uint32_t)mask; p->IDR &= ~(uint32_t)mask; }
}
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *cfg)
{
  assert((p->ODR & cfg->Pin) == 0U);
}

int main(int argc, char **argv)
{
  int scenario = (argc > 1) ? atoi(argv[1]) : 0;
  if (scenario == 16)
  {
    gpioa.ODR = BOARD_HB_IN1_PIN | BOARD_HB_IN2_PIN | BOARD_HB_IN3_PIN;
    gpiob.ODR = BOARD_HB_IN4_PIN;
    HBridge_Init();
    assert((gpioa.ODR & (BOARD_HB_IN1_PIN | BOARD_HB_IN2_PIN | BOARD_HB_IN3_PIN)) == 0U);
    assert((gpiob.ODR & BOARD_HB_IN4_PIN) == 0U);
    return 0;
  }
  gpioa.MODER = (1U << 2) | (1U << 4) | (1U << 12);
  gpiob.MODER = 1U;
  HBridge_DirectionProbeService(1947, 1000);
  HBridge_DirectionProbeService(1947, 31000);
  assert(g_direction_probe_status == 0U && gpioa.ODR == 0U);
  g_direction_probe_request = (scenario == 1) ? 123U : 0x50353050UL;
  if (scenario == 2) gpioa.IDR = BOARD_HB_IN1_PIN;
  if (scenario == 3) gpiob.MODER = 0U;
  if (scenario == 4) rcc.APBENR2 = RCC_APBENR2_TIM1EN;
  if (scenario == 9) gpioa.ODR = BOARD_HB_IN1_PIN;
  if (scenario == 10) gpioa.OTYPER = BOARD_HB_IN3_PIN;
  if (scenario == 11) gpiob.AFR[0] = 2U;
  if (scenario == 12) gpiob.IDR = BOARD_HB_IN4_PIN;
  if (scenario == 13) SystemCoreClock = 24000001U;
  if (scenario == 14) gpioa.MODER &= ~(3U << 12);
  if (scenario == 5) HBridge_DirectionProbeService(280, 31001);
  else if (scenario == 6) HBridge_DirectionProbeService(1947, 10000);
  else if (scenario == 7) HBridge_DirectionProbeService(1960, 31001);
  else HBridge_DirectionProbeService(1947, 31001);
  if ((scenario >= 1 && scenario <= 7) || (scenario >= 9 && scenario <= 14))
  {
    assert(g_direction_probe_status == 3U);
    assert((gpioa.ODR & BOARD_HB_IN3_PIN) == 0U);
    g_direction_probe_request = 0x50353050UL;
    HBridge_DirectionProbeService(1947, 32000);
    assert(g_direction_probe_status == 3U);
    return 0;
  }
  assert(g_direction_probe_status == 1U);
  assert(g_direction_probe_adc == 1947U);
  assert((gpioa.ODR & BOARD_HB_IN3_PIN) != 0U);
  assert((gpiob.MODER & 3U) == 2U && (gpiob.AFR[0] & 15U) == 2U);
  assert(tim1.PSC == 23U && tim1.ARR == 999U && tim1.CCR2 == 1000U - SERVO_DIRECTION_PULSE_US);
  assert(tim1.RCR == 0U && tim1.CCER == TIM_CCER_CC2NE);
  assert(tim1.CR1 == (TIM_CR1_OPM | TIM_CR1_CEN));
  HBridge_SetSignedDuty(1000);
  assert(g_direction_probe_status == 1U && HBridge_GetDuty() == 0);
  if (scenario == 8)
  {
    HBridge_Coast();
    assert(g_direction_probe_status == 4U);
  }
  else
  {
    tim1.SR = (scenario == 15) ? 0U : TIM_SR_UIF | TIM_SR_CC2IF;
    tim1.CR1 &= ~TIM_CR1_CEN; /* emulate one-pulse hardware completion */
    HBridge_TickISR();
    assert(g_direction_probe_status == ((scenario == 15) ? 5U : 2U));
  }
  assert((gpioa.ODR & BOARD_HB_IN3_PIN) == 0U);
  assert((gpiob.MODER & 3U) == 1U && !(rcc.APBENR2 & RCC_APBENR2_TIM1EN));
  g_direction_probe_request = 0x50353050UL;
  HBridge_DirectionProbeService(1947, 32000);
  assert(g_direction_probe_status != 1U);
  return 0;
}
