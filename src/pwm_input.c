#include "pwm_input.h"
#include "board.h"
#include "servo_params.h"
#include "servo_config.h"

#ifndef SERVO_ENABLE_INTERNAL_STEP_TEST
#define SERVO_ENABLE_INTERNAL_STEP_TEST 0
#endif

#if SERVO_ENABLE_INTERNAL_STEP_TEST

void PWM_Input_Init(void)
{
}

void PWM_Input_EdgeISR(void)
{
}

PWM_InputSample PWM_Input_GetSample(void)
{
  PWM_InputSample sample = {0};
  sample.pulse_us = SERVO_INPUT_FAILSAFE_US;
  sample.raw_pulse_us = SERVO_INPUT_FAILSAFE_US;
  sample.candidate_pulse_us = SERVO_INPUT_FAILSAFE_US;
  sample.period_us = 20000UL;
  sample.valid = true;
  return sample;
}

uint16_t PWM_Input_TakeRawWindowUs(void)
{
  return 0U;
}

uint16_t PWM_Input_GetCommandUs(void)
{
  return SERVO_INPUT_FAILSAFE_US;
}

bool PWM_Input_IsFresh(void)
{
  return true;
}

#else

#define PWM_INPUT_RAW_WINDOW_BAND_US  8U
#define PWM_INPUT_DIAG_FRAME_MIN_US   500U
#define PWM_INPUT_DIAG_FRAME_MAX_US   30000U
#define PWM_INPUT_CONTROL_FRAME_MIN_US 3000U
#define PWM_INPUT_CONTROL_FRAME_MAX_US PWM_INPUT_DIAG_FRAME_MAX_US
#ifndef PWM_INPUT_1KHZ_WIDTH_DIAG_ONLY
#define PWM_INPUT_1KHZ_WIDTH_DIAG_ONLY 0U
#endif

static volatile uint16_t s_last_rise_us;
static volatile uint32_t s_last_period_us;
static volatile uint16_t s_raw_pulse_us = SERVO_INPUT_FAILSAFE_US;
static volatile uint16_t s_last_pulse_us = SERVO_INPUT_FAILSAFE_US;
static volatile uint16_t s_candidate_pulse_us = SERVO_INPUT_FAILSAFE_US;
static volatile uint16_t s_raw_window_min = 0xFFFFU;
static volatile uint16_t s_raw_window_max;
static volatile uint32_t s_last_edge_ms;
static volatile uint32_t s_edge_count;
static volatile uint32_t s_pulse_count;
static volatile uint8_t s_candidate_count;
static volatile uint8_t s_valid;
static volatile uint8_t s_waiting_falling;
static volatile uint8_t s_have_rise;

static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
  return (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static void raw_window_reset_to(uint16_t width)
{
  s_raw_window_min = width;
  s_raw_window_max = width;
}

static void accept_pulse_width(uint16_t width)
{
  /*
   * PA3/TIM1_CH1 is hardware-captured. Accept each legal control frame here.
   * A fast external sweep changes every 20 ms servo frame, so multi-frame
   * candidate confirmation can keep resetting and make the servo lag. Static
   * jitter suppression belongs in the command latch/hold layer.
   */
  s_last_pulse_us = width;
  s_candidate_pulse_us = width;
  s_candidate_count = 0U;
  s_valid = 1U;
  raw_window_reset_to(width);
}

static void raw_window_add(uint16_t width)
{
  if (width < s_raw_window_min)
  {
    s_raw_window_min = width;
  }
  if (width > s_raw_window_max)
  {
    s_raw_window_max = width;
  }
}

static void raw_window_add_plausible(uint16_t width)
{
  if ((s_valid == 0U) ||
      (abs_diff_u16(width, s_last_pulse_us) <= PWM_INPUT_RAW_WINDOW_BAND_US) ||
      ((s_candidate_count != 0U) &&
       (abs_diff_u16(width, s_candidate_pulse_us) <= PWM_INPUT_RAW_WINDOW_BAND_US)))
  {
    raw_window_add(width);
  }
}

void PWM_Input_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t timer_hz = SystemCoreClock;
  uint32_t prescaler = timer_hz / 1000000U;

  /* PA3/TIM1_CH1 hardware input capture. TIM1 runs at 1 MHz, so CCR1 is us. */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();

  gpio.Pin = BOARD_PWM_IN_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF13_TIM1;
  HAL_GPIO_Init(BOARD_PWM_IN_PORT, &gpio);

  if (prescaler == 0U)
  {
    prescaler = 1U;
  }

  TIM1->CR1 = 0U;
  TIM1->DIER = 0U;
  TIM1->PSC = prescaler - 1U;
  TIM1->ARR = 0xFFFFU;
  TIM1->CNT = 0U;
  TIM1->CCMR1 = TIM_CCMR1_CC1S_0;
  TIM1->CCER = TIM_CCER_CC1E;
  TIM1->EGR = TIM_EGR_UG;
  TIM1->SR = 0U;
  TIM1->DIER = TIM_DIER_CC1IE;
  TIM1->CR1 = TIM_CR1_CEN;

  s_waiting_falling = 0U;
  s_have_rise = 0U;

  HAL_NVIC_SetPriority(TIM1_CC_IRQn, 0, 0);
  HAL_NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
  HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
}

void PWM_Input_EdgeISR(void)
{
  uint16_t capture_us;
  uint32_t now_ms = HAL_GetTick();

  if ((TIM1->SR & TIM_SR_CC1IF) == 0U)
  {
    return;
  }
  TIM1->SR &= ~TIM_SR_CC1IF;
  capture_us = (uint16_t)TIM1->CCR1;
  s_edge_count++;

  if (s_waiting_falling == 0U)
  {
    if (s_have_rise != 0U)
    {
      s_last_period_us = (uint16_t)(capture_us - s_last_rise_us);
    }
    s_last_rise_us = capture_us;
    s_have_rise = 1U;
    s_waiting_falling = 1U;
    TIM1->CCER |= TIM_CCER_CC1P;
    return;
  }

  uint16_t width = (uint16_t)(capture_us - s_last_rise_us);
  s_waiting_falling = 0U;
  TIM1->CCER &= ~TIM_CCER_CC1P;

  if ((width >= SERVO_INPUT_CAPTURE_MIN_US) &&
      (width <= SERVO_INPUT_CAPTURE_MAX_US) &&
      (width < s_last_period_us) &&
      (s_last_period_us >= PWM_INPUT_DIAG_FRAME_MIN_US) &&
      (s_last_period_us <= PWM_INPUT_DIAG_FRAME_MAX_US))
  {
    s_raw_pulse_us = width;
    raw_window_add_plausible(width);
    if ((s_last_period_us >= PWM_INPUT_CONTROL_FRAME_MIN_US) &&
        (s_last_period_us <= PWM_INPUT_CONTROL_FRAME_MAX_US))
    {
#if !PWM_INPUT_1KHZ_WIDTH_DIAG_ONLY
      accept_pulse_width(width);
      s_last_edge_ms = now_ms;
      s_pulse_count++;
#endif
    }
  }
}

PWM_InputSample PWM_Input_GetSample(void)
{
  PWM_InputSample sample;

  /* ISR 会更新这些变量，读取时短暂关中断保证一组数据一致。 */
  __disable_irq();
  sample.pulse_us = s_last_pulse_us;
  sample.raw_pulse_us = s_raw_pulse_us;
  sample.candidate_pulse_us = s_candidate_pulse_us;
  sample.candidate_count = s_candidate_count;
  sample.period_us = s_last_period_us;
  sample.last_edge_ms = s_last_edge_ms;
  sample.edge_count = s_edge_count;
  sample.pulse_count = s_pulse_count;
  sample.raw_high = (HAL_GPIO_ReadPin(BOARD_PWM_IN_PORT, BOARD_PWM_IN_PIN) == GPIO_PIN_SET);
  sample.valid = (s_valid != 0U);
  __enable_irq();

  return sample;
}

uint16_t PWM_Input_TakeRawWindowUs(void)
{
  uint16_t window = 0U;

  __disable_irq();
  if ((s_raw_window_min != 0xFFFFU) &&
      (s_raw_window_max >= s_raw_window_min))
  {
    window = (uint16_t)(s_raw_window_max - s_raw_window_min);
  }
  s_raw_window_min = 0xFFFFU;
  s_raw_window_max = 0U;
  __enable_irq();

  return window;
}

uint16_t PWM_Input_GetCommandUs(void)
{
  PWM_InputSample sample = PWM_Input_GetSample();
  const ServoParams *params = Servo_Params_Get();

  if ((sample.valid == false) ||
      ((uint32_t)(HAL_GetTick() - sample.last_edge_ms) > params->input_timeout_ms))
  {
    /* 控制层会根据 input_valid 决定失信号策略，这里返回中位作为保底值。 */
    return params->pulse_center_us;
  }

  if (sample.pulse_us < params->pulse_lower_us)
  {
    return params->pulse_lower_us;
  }
  if (sample.pulse_us > params->pulse_high_us)
  {
    return params->pulse_high_us;
  }
  return sample.pulse_us;
}

bool PWM_Input_IsFresh(void)
{
  PWM_InputSample sample = PWM_Input_GetSample();
  const ServoParams *params = Servo_Params_Get();
  return sample.valid &&
         ((uint32_t)(HAL_GetTick() - sample.last_edge_ms) <= params->input_timeout_ms);
}

#endif
