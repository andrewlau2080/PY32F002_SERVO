#include "hbridge.h"
#include "board.h"
#include "servo_config.h"
#include <stddef.h>

#ifndef SERVO_HBRIDGE_FORCE_HIZ
#define SERVO_HBRIDGE_FORCE_HIZ 0
#endif
#ifndef SERVO_HBRIDGE_FORCE_COAST
#define SERVO_HBRIDGE_FORCE_COAST 0
#endif
#ifndef SERVO_DIRECTION_PROBE
#define SERVO_DIRECTION_PROBE 0
#endif
#ifndef SERVO_DIRECTION_PULSE_US
#define SERVO_DIRECTION_PULSE_US 80
#endif
#if SERVO_DIRECTION_PROBE && (SERVO_DIRECTION_PULSE_US < 1 || SERVO_DIRECTION_PULSE_US > 160)
#error Direction probe pulse must be between 1 and 160 microseconds
#endif
#if SERVO_DIRECTION_PROBE && (!SERVO_HBRIDGE_FORCE_COAST || !BOARD_TSSOP20_DEBUG_PINS)
#error Direction probe requires force-coast and the TSSOP20 debug pinout
#endif

static volatile int16_t s_duty;
static volatile HBridgeState s_state = HBRIDGE_COAST;
static volatile uint16_t s_pwm_phase_us;
static volatile uint16_t s_pwm_period_us = HBRIDGE_PWM_PERIOD_US;
static volatile uint32_t s_hard_coast_deadline_ms;
static volatile uint8_t s_hard_coast_deadline_armed;

#if SERVO_R01_PRODUCTION_LOOP_TEST
volatile uint32_t g_r01_bridge_forward_on_ticks;
volatile uint32_t g_r01_bridge_reverse_on_ticks;
#endif

#if SERVO_DIRECTION_PROBE
/* Diagnostic-only SWD request. Zero after reset: no automatic motor action. */
volatile uint32_t g_direction_probe_request;
volatile uint32_t g_direction_probe_status; /* 0 idle, 1 active, 2 done, 3 rejected, 4 aborted */
volatile uint16_t g_direction_probe_adc;
static volatile uint8_t s_direction_probe_active;
static uint8_t s_direction_probe_used;

static void direction_probe_finish(uint32_t status)
{
  /* Remove the high-side signal before changing the timer or low-side pin. */
  HAL_GPIO_WritePin(BOARD_HB_IN3_PORT, BOARD_HB_IN3_PIN, GPIO_PIN_RESET);
  TIM1->BDTR = 0U;
  TIM1->CCER = 0U;
  TIM1->CR1 = 0U;
  GPIOB->BRR = BOARD_HB_IN4_PIN;
  GPIOB->MODER = (GPIOB->MODER & ~3UL) | 1UL;
  GPIOB->AFR[0] &= ~15UL;
  __HAL_RCC_TIM1_CLK_DISABLE();
  s_direction_probe_active = 0U;
  g_direction_probe_status = status;
}

void HBridge_DirectionProbeService(uint16_t raw_adc, uint32_t now_ms)
{
  static uint16_t stable_adc;
  static uint32_t stable_since_ms;
  static uint8_t stable_seeded;
  uint16_t delta = (raw_adc > stable_adc) ? raw_adc - stable_adc : stable_adc - raw_adc;
  if (!stable_seeded || raw_adc < 1700U || raw_adc > 2300U || delta > 8U)
  {
    stable_adc = raw_adc;
    stable_since_ms = now_ms;
    stable_seeded = 1U;
  }
  if (g_direction_probe_request == 0U)
  {
    return;
  }
  uint32_t request = g_direction_probe_request;
  g_direction_probe_request = 0U;
  if (s_direction_probe_used)
  {
    return;
  }
  s_direction_probe_used = 1U;
  g_direction_probe_adc = raw_adc;
  if (request != 0x50353050UL || now_ms < 30000U || raw_adc < 1700U || raw_adc > 2300U ||
      (uint32_t)(now_ms - stable_since_ms) < 500U || (RCC->APBENR2 & RCC_APBENR2_TIM1EN) ||
      ((GPIOA->MODER >> 2) & 3U) != 1U || ((GPIOA->MODER >> 4) & 3U) != 1U ||
      ((GPIOA->MODER >> 12) & 3U) != 1U || (GPIOB->MODER & 3U) != 1U ||
      (GPIOA->OTYPER & (BOARD_HB_IN1_PIN | BOARD_HB_IN2_PIN | BOARD_HB_IN3_PIN)) ||
      (GPIOB->OTYPER & BOARD_HB_IN4_PIN) ||
      (GPIOA->ODR & (BOARD_HB_IN1_PIN | BOARD_HB_IN2_PIN | BOARD_HB_IN3_PIN)) ||
      (GPIOB->ODR & BOARD_HB_IN4_PIN) || (GPIOB->AFR[0] & 15UL) ||
      (GPIOA->IDR & (BOARD_HB_IN1_PIN | BOARD_HB_IN2_PIN | BOARD_HB_IN3_PIN)) ||
      (GPIOB->IDR & BOARD_HB_IN4_PIN) || (SystemCoreClock % 1000000UL) || SystemCoreClock < 1000000UL)
  {
    g_direction_probe_status = 3U;
    return;
  }
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  __HAL_RCC_TIM1_CLK_ENABLE();
  TIM1->CR1 = 0U; TIM1->CR2 = 0U; TIM1->SMCR = 0U; TIM1->DIER = 0U;
  TIM1->CCER = 0U; TIM1->BDTR = 0U;
  TIM1->PSC = (SystemCoreClock / 1000000UL) - 1U;
  TIM1->ARR = 999U; TIM1->RCR = 0U; TIM1->CCR2 = 1000U - SERVO_DIRECTION_PULSE_US;
  TIM1->CCMR1 = 7UL << 12; /* PWM2: one bounded pulse at the end of 1 ms */
  TIM1->EGR = TIM_EGR_UG; TIM1->SR = 0U; TIM1->CNT = 0U;
  TIM1->CCER = TIM_CCER_CC2NE; TIM1->BDTR = TIM_BDTR_MOE;
  GPIOB->BRR = BOARD_HB_IN4_PIN;
  GPIOB->AFR[0] = (GPIOB->AFR[0] & ~15UL) | 2UL;
  GPIOB->MODER = (GPIOB->MODER & ~3UL) | 2UL;
  if (GPIOB->IDR & BOARD_HB_IN4_PIN)
  {
    direction_probe_finish(3U);
  }
  else
  {
    s_direction_probe_active = 1U;
    g_direction_probe_status = 1U;
    HAL_GPIO_WritePin(BOARD_HB_IN3_PORT, BOARD_HB_IN3_PIN, GPIO_PIN_SET);
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
  }
  if (!primask) { __enable_irq(); }
}
#endif

/* 四个控制脚集中写入，后续若 H 桥逻辑相反，只需改这里的组合。 */
static void write_outputs(GPIO_PinState in1,
                          GPIO_PinState in2,
                          GPIO_PinState in3,
                          GPIO_PinState in4)
{
#if SERVO_HBRIDGE_FORCE_COAST
  in1 = GPIO_PIN_RESET;
  in2 = GPIO_PIN_RESET;
  in3 = GPIO_PIN_RESET;
  in4 = GPIO_PIN_RESET;
#if SERVO_DIRECTION_PROBE
  if (s_direction_probe_active) { in3 = GPIO_PIN_SET; }
#endif
#endif
#if SERVO_HBRIDGE_FORCE_HIZ
  (void)in1;
  (void)in2;
  (void)in3;
  (void)in4;
#else
  HAL_GPIO_WritePin(BOARD_HB_IN1_PORT, BOARD_HB_IN1_PIN, in1);
  HAL_GPIO_WritePin(BOARD_HB_IN2_PORT, BOARD_HB_IN2_PIN, in2);
#if BOARD_HB_IN3_ENABLE
  HAL_GPIO_WritePin(BOARD_HB_IN3_PORT, BOARD_HB_IN3_PIN, in3);
#else
  (void)in3;
#endif
#if BOARD_HB_IN4_ENABLE
  HAL_GPIO_WritePin(BOARD_HB_IN4_PORT, BOARD_HB_IN4_PIN, in4);
#else
  (void)in4;
#endif
#endif
}

static void apply_coast(void)
{
  /* 滑行：四脚关闭，马达靠惯性停止。 */
  write_outputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
}

static void apply_forward(void)
{
  /* Schematic mapping is crossed: CP2 drives left high side, CP1 drives right high side. */
  write_outputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET);
}

static void apply_reverse(void)
{
  write_outputs(GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
}

static void enable_gpio_clock(GPIO_TypeDef *port)
{
  if (port == GPIOA)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
#ifdef GPIOB
  else if (port == GPIOB)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
#endif
#ifdef GPIOF
  else if (port == GPIOF)
  {
    __HAL_RCC_GPIOF_CLK_ENABLE();
  }
#endif
}

static void init_output_pin(GPIO_TypeDef *port, uint16_t pin)
{
  GPIO_InitTypeDef gpio = {0};

  enable_gpio_clock(port);

  /* Preload the inactive level before the pin becomes an output. */
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  gpio.Pin = pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(port, &gpio);
}

void HBridge_Init(void)
{
#if SERVO_HBRIDGE_FORCE_HIZ
  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;

  enable_gpio_clock(BOARD_HB_IN1_PORT);
  gpio.Pin = BOARD_HB_IN1_PIN;
  HAL_GPIO_Init(BOARD_HB_IN1_PORT, &gpio);
  enable_gpio_clock(BOARD_HB_IN2_PORT);
  gpio.Pin = BOARD_HB_IN2_PIN;
  HAL_GPIO_Init(BOARD_HB_IN2_PORT, &gpio);
#if BOARD_HB_IN3_ENABLE
  enable_gpio_clock(BOARD_HB_IN3_PORT);
  gpio.Pin = BOARD_HB_IN3_PIN;
  HAL_GPIO_Init(BOARD_HB_IN3_PORT, &gpio);
#endif
#if BOARD_HB_IN4_ENABLE
  enable_gpio_clock(BOARD_HB_IN4_PORT);
  gpio.Pin = BOARD_HB_IN4_PIN;
  HAL_GPIO_Init(BOARD_HB_IN4_PORT, &gpio);
#endif
  __disable_irq();
  s_duty = 0;
  s_state = HBRIDGE_COAST;
  s_pwm_phase_us = 0U;
  __enable_irq();
#else
  /* H 桥相关 IO 上电后立即配置为下拉输出，并进入滑行安全状态。 */
  init_output_pin(BOARD_HB_IN1_PORT, BOARD_HB_IN1_PIN);
  init_output_pin(BOARD_HB_IN2_PORT, BOARD_HB_IN2_PIN);
#if BOARD_HB_IN3_ENABLE
  init_output_pin(BOARD_HB_IN3_PORT, BOARD_HB_IN3_PIN);
#endif
#if BOARD_HB_IN4_ENABLE
  init_output_pin(BOARD_HB_IN4_PORT, BOARD_HB_IN4_PIN);
#endif

  HBridge_Coast();
#endif
}

void HBridge_SetFrequency(uint16_t frequency_hz)
{
  /* 驱动频率来自参数区；范围限制避免过低抖动或过高超出 20us tick。 */
  if (frequency_hz < 100U)
  {
    frequency_hz = 100U;
  }
  else if (frequency_hz > 20000U)
  {
    frequency_hz = 20000U;
  }

  uint32_t period_us = 1000000UL / frequency_hz;
  if (period_us < HBRIDGE_PWM_TICK_US)
  {
    period_us = HBRIDGE_PWM_TICK_US;
  }
  if (period_us > 10000UL)
  {
    period_us = 10000UL;
  }

  __disable_irq();
  s_pwm_period_us = (uint16_t)period_us;
  s_pwm_phase_us = 0U;
  __enable_irq();
}

void HBridge_SetSignedDuty(int16_t duty)
{
#if SERVO_HBRIDGE_FORCE_COAST
  (void)duty;
#if SERVO_DIRECTION_PROBE
  if (s_direction_probe_active) { return; }
#endif
  HBridge_Coast();
  return;
#endif
#if SERVO_HBRIDGE_FORCE_HIZ
  (void)duty;
  __disable_irq();
  s_duty = 0;
  s_state = HBRIDGE_COAST;
  s_pwm_phase_us = 0U;
  __enable_irq();
  return;
#else
  /* duty 正负代表方向，绝对值代表占空比，范围 0-1000。 */
  if (duty > HBRIDGE_DUTY_MAX)
  {
    duty = HBRIDGE_DUTY_MAX;
  }
  else if (duty < -HBRIDGE_DUTY_MAX)
  {
    duty = -HBRIDGE_DUTY_MAX;
  }

  __disable_irq();
  s_duty = duty;
  if (duty > 0)
  {
    s_state = HBRIDGE_FORWARD;
  }
  else if (duty < 0)
  {
    s_state = HBRIDGE_REVERSE;
  }
  else
  {
    s_state = HBRIDGE_COAST;
  }
  __enable_irq();
#endif
}

void HBridge_SetHardCoastDeadline(uint32_t deadline_ms)
{
  __disable_irq();
  s_hard_coast_deadline_ms = deadline_ms;
  s_hard_coast_deadline_armed = 1U;
  __enable_irq();
}

void HBridge_Coast(void)
{
  __disable_irq();
#if SERVO_DIRECTION_PROBE
  if (s_direction_probe_active) { direction_probe_finish(4U); }
#endif
  s_duty = 0;
  s_state = HBRIDGE_COAST;
  s_pwm_phase_us = 0U;
  s_hard_coast_deadline_armed = 0U;
  __enable_irq();
#if !SERVO_HBRIDGE_FORCE_HIZ
  apply_coast();
#endif
}

void HBridge_Brake(void)
{
  HBridge_Coast();
}

void HBridge_TickISR(void)
{
#if SERVO_DIRECTION_PROBE
  if (s_direction_probe_active && !(TIM1->CR1 & TIM_CR1_CEN))
  {
    direction_probe_finish(((TIM1->SR & (TIM_SR_UIF | TIM_SR_CC2IF)) ==
                            (TIM_SR_UIF | TIM_SR_CC2IF)) ? 2U : 5U);
  }
#endif
#if SERVO_HBRIDGE_FORCE_HIZ
  return;
#else
  /* A diagnostic jog must stop on the PWM interrupt even if the LCD/main
   * loop is delayed by UART or drawing work. Keep the deadline armed until
   * the application observes the timeout and calls HBridge_Coast(). */
  if (s_hard_coast_deadline_armed != 0U &&
      (int32_t)(HAL_GetTick() - s_hard_coast_deadline_ms) >= 0)
  {
    s_duty = 0;
    s_state = HBRIDGE_COAST;
    s_pwm_phase_us = 0U;
    apply_coast();
    return;
  }
  /* TIM1 每 20us 进入一次，用软件 PWM 方式驱动 H 桥。 */
  int16_t duty = s_duty;
  HBridgeState state = s_state;

  s_pwm_phase_us += HBRIDGE_PWM_TICK_US;
  uint16_t period_us = s_pwm_period_us;
  if (s_pwm_phase_us >= period_us)
  {
    s_pwm_phase_us = 0U;
  }

  if ((state == HBRIDGE_COAST) || (duty == 0))
  {
    apply_coast();
    return;
  }

  uint16_t abs_duty = (duty < 0) ? (uint16_t)(-duty) : (uint16_t)duty;
  uint32_t on_time = ((uint32_t)abs_duty * period_us) / HBRIDGE_DUTY_MAX;

  if (s_pwm_phase_us >= on_time)
  {
    apply_coast();
  }
  else if (duty > 0)
  {
    apply_forward();
#if SERVO_R01_PRODUCTION_LOOP_TEST
    g_r01_bridge_forward_on_ticks++;
#endif
  }
  else
  {
    apply_reverse();
#if SERVO_R01_PRODUCTION_LOOP_TEST
    g_r01_bridge_reverse_on_ticks++;
#endif
  }
#endif
}

int16_t HBridge_GetDuty(void)
{
  return s_duty;
}

HBridgeState HBridge_GetState(void)
{
  return s_state;
}

void HBridge_GetInputDutyPermille(uint16_t *in1, uint16_t *in2,
                                  uint16_t *in3, uint16_t *in4)
{
  uint32_t primask = __get_PRIMASK();
  int16_t duty;
  HBridgeState state;
  uint16_t magnitude;
  uint16_t d1 = 0U;
  uint16_t d2 = 0U;
  uint16_t d3 = 0U;
  uint16_t d4 = 0U;

  __disable_irq();
  duty = s_duty;
  state = s_state;
  if (primask == 0U)
  {
    __enable_irq();
  }

  magnitude = (duty < 0) ? (uint16_t)(-duty) : (uint16_t)duty;
  if ((state == HBRIDGE_FORWARD) && (duty > 0))
  {
    d3 = magnitude;
    d4 = magnitude;
  }
  else if ((state == HBRIDGE_REVERSE) && (duty < 0))
  {
    d1 = magnitude;
    d2 = magnitude;
  }

  if (in1 != NULL) *in1 = d1;
  if (in2 != NULL) *in2 = d2;
  if (in3 != NULL) *in3 = d3;
  if (in4 != NULL) *in4 = d4;
}
