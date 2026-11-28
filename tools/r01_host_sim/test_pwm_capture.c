/* Execute the production PWM input ISR with a simulated 1 MHz capture timer. */
#include <stdint.h>
#include <stdio.h>
#include "../../inc/board.h"
#include "pwm_input.h"
#include "servo_params.h"

#ifndef R01_EXPECT_CHANNEL
#define R01_EXPECT_CHANNEL BOARD_PWM_CAPTURE_CHANNEL
#endif

R01TestTimer r01_test_timer;
GPIO_TypeDef r01_test_gpio;
uint32_t SystemCoreClock = 24000000U;
static uint32_t test_tick_ms;
static unsigned failures;
static GPIO_InitTypeDef captured_gpio;
static ServoParams test_params = {
  .pulse_lower_us = 900U, .pulse_center_us = 1500U,
  .pulse_high_us = 2100U, .input_timeout_ms = 100U,
};

uint32_t HAL_GetTick(void) { return test_tick_ms; }
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init) { (void)port; captured_gpio = *init; }
uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin) { (void)port; (void)pin; return 0U; }
void HAL_NVIC_SetPriority(int irq, int priority, int subpriority) { (void)irq; (void)priority; (void)subpriority; }
void HAL_NVIC_ClearPendingIRQ(int irq) { (void)irq; }
void HAL_NVIC_EnableIRQ(int irq) { (void)irq; }
const ServoParams *Servo_Params_Get(void) { return &test_params; }

static void check(const char *name, int actual, int expected)
{
  printf("%s: %s (actual=%d expected=%d)\n",
         (actual == expected) ? "PASS" : "FAIL", name, actual, expected);
  if (actual != expected) { failures++; }
}

static void edge(uint32_t time_us)
{
  test_tick_ms = time_us / 1000U;
#if BOARD_PWM_CAPTURE_CHANNEL == 1U
  TIM1->CCR1 = (uint16_t)time_us;
  TIM1->SR |= TIM_SR_CC1IF;
#else
  TIM1->CCR3 = (uint16_t)time_us;
  TIM1->SR |= TIM_SR_CC3IF;
#endif
  PWM_Input_EdgeISR();
}

static void pulse(uint32_t rising_us, uint16_t width_us)
{
  edge(rising_us);
  edge(rising_us + width_us);
}

int main(void)
{
  PWM_Input_Init();
  check("timer configured for 1 MHz from 24 MHz", (int)TIM1->PSC, 23);
  check("selected capture channel matches build profile", BOARD_PWM_CAPTURE_CHANNEL, R01_EXPECT_CHANNEL);
#if R01_EXPECT_CHANNEL == 1U
  check("development profile selects PA3", (int)captured_gpio.Pin, GPIO_PIN_3);
  check("development profile selects TIM1 AF13", (int)captured_gpio.Alternate, GPIO_AF13_TIM1);
  check("development profile enables TIM1 CH1 interrupt", (int)TIM1->DIER, TIM_DIER_CC1IE);
  check("development feedback uses PA4", (int)BOARD_POT_ADC_PIN, GPIO_PIN_4);
  check("development feedback uses ADC4", (int)BOARD_POT_ADC_CHANNEL, ADC_CHANNEL_4);
#else
  check("SOP8 profile selects PA10 on shared pin 2", (int)captured_gpio.Pin, GPIO_PIN_10);
  check("SOP8 profile selects TIM1 AF2", (int)captured_gpio.Alternate, GPIO_AF2_TIM1);
  check("SOP8 profile enables TIM1 CH3 interrupt", (int)TIM1->DIER, TIM_DIER_CC3IE);
  check("SOP8 feedback uses PA3", (int)BOARD_POT_ADC_PIN, GPIO_PIN_3);
  check("SOP8 feedback uses ADC3", (int)BOARD_POT_ADC_CHANNEL, ADC_CHANNEL_3);
#endif
  pulse(0U, 1500U);
  check("first pulse has no measured period", PWM_Input_GetSample().valid, 0);

  static const uint16_t widths[] = {500, 1000, 1500, 1501, 1502, 1503, 2000, 2500};
  uint32_t start_us = 0U;
  char name[80];
  for (unsigned i = 0; i < sizeof(widths) / sizeof(widths[0]); i++) {
    start_us += 20000U;
    pulse(start_us, widths[i]);
    PWM_InputSample sample = PWM_Input_GetSample();
    snprintf(name, sizeof(name), "accepted %u us at 20 ms period", widths[i]);
    check(name, sample.pulse_us, widths[i]);
    check("period is 20 ms across 16-bit timer wrap", (int)sample.period_us, 20000);
    check("input is valid and fresh", sample.valid && PWM_Input_IsFresh(), 1);
  }
  check("generic command clamps 2500 us to its configured 2100 us", PWM_Input_GetCommandUs(), 2100);
  test_params.pulse_lower_us = 500U;
  test_params.pulse_high_us = 2500U;
  check("generic command accepts 2500 us with full-range calibration", PWM_Input_GetCommandUs(), 2500);
  test_params.pulse_lower_us = 900U;
  test_params.pulse_high_us = 2100U;
  uint32_t accepted_count = PWM_Input_GetSample().pulse_count;
  uint32_t accepted_edge_ms = PWM_Input_GetSample().last_edge_ms;

  start_us += 20000U;
  pulse(start_us, 299U);
  check("299 us is rejected", PWM_Input_GetSample().pulse_us, 2500);
  check("rejected pulse does not refresh input", (int)PWM_Input_GetSample().last_edge_ms, (int)accepted_edge_ms);
  start_us += 20000U;
  pulse(start_us, 2701U);
  check("2701 us is rejected", PWM_Input_GetSample().pulse_us, 2500);
  check("rejected widths do not increase accepted count", (int)PWM_Input_GetSample().pulse_count, (int)accepted_count);

  start_us += 20000U;
  pulse(start_us, 500U);
  check("generic command clamps 500 us to its configured 900 us", PWM_Input_GetCommandUs(), 900);
  test_params.pulse_lower_us = 500U;
  test_params.pulse_high_us = 2500U;
  check("generic command accepts 500 us with full-range calibration", PWM_Input_GetCommandUs(), 500);
  accepted_count = PWM_Input_GetSample().pulse_count;
  accepted_edge_ms = PWM_Input_GetSample().last_edge_ms;
  start_us += 1000U;
  pulse(start_us, 750U);
  check("1 ms period is diagnostic only", PWM_Input_GetSample().raw_pulse_us, 750);
  check("diagnostic-only pulse does not change control width", PWM_Input_GetSample().pulse_us, 500);
  check("diagnostic-only pulse does not increase accepted count", (int)PWM_Input_GetSample().pulse_count, (int)accepted_count);

  test_tick_ms = accepted_edge_ms + 100U;
  check("input remains fresh at 100 ms", PWM_Input_IsFresh(), 1);
  test_tick_ms = accepted_edge_ms + 101U;
  check("input becomes stale after 100 ms", PWM_Input_IsFresh(), 0);
  check("stale generic command returns center", PWM_Input_GetCommandUs(), 1500);
  start_us += 20000U;
  pulse(start_us, 1500U);
  for (uint16_t n = 1U; n <= 100U; ++n)
  {
    start_us += 20000U;
    pulse(start_us, (uint16_t)(1500U + n));
    snprintf(name, sizeof(name), "cumulative +%u us is visible to the controller", n);
    check(name, PWM_Input_GetCommandUs(), 1500 + n);
  }
  uint16_t accumulated_command = PWM_Input_GetCommandUs();
  start_us += 20000U;
  pulse(start_us, 1500U);
  start_us += 20000U;
  pulse(start_us, 1600U);
  check("one hundred 1 us increments and a single 100 us step deliver the same command",
        PWM_Input_GetCommandUs(), accumulated_command);
  printf("Timer and GPIO are simulated; physical pulse accuracy is not tested.\n");
  return failures ? 1 : 0;
}
