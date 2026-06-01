#include "hbridge.h"
#include "board.h"
#include "servo_config.h"

static volatile int16_t s_duty;
static volatile HBridgeState s_state = HBRIDGE_COAST;
static volatile uint16_t s_pwm_phase_us;
static volatile uint16_t s_pwm_period_us = HBRIDGE_PWM_PERIOD_US;

/* 四个控制脚集中写入，后续若 H 桥逻辑相反，只需改这里的组合。 */
static void write_outputs(GPIO_PinState in1,
                          GPIO_PinState in2,
                          GPIO_PinState in3,
                          GPIO_PinState in4)
{
  HAL_GPIO_WritePin(BOARD_HB_IN1_PORT, BOARD_HB_IN1_PIN, in1);
  HAL_GPIO_WritePin(BOARD_HB_IN2_PORT, BOARD_HB_IN2_PIN, in2);
  HAL_GPIO_WritePin(BOARD_HB_IN3_PORT, BOARD_HB_IN3_PIN, in3);
  HAL_GPIO_WritePin(BOARD_HB_IN4_PORT, BOARD_HB_IN4_PIN, in4);
}

static void apply_coast(void)
{
  /* 滑行：四脚关闭，马达靠惯性停止。 */
  write_outputs(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
}

static void apply_brake(void)
{
  /* 刹车：当前按低边短接的假设处理，上板后需按实际 H 桥确认。 */
  write_outputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET);
}

static void apply_forward(void)
{
  write_outputs(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
}

static void apply_reverse(void)
{
  write_outputs(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET);
}

void HBridge_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  /* H 桥相关 IO 上电后立即配置为下拉输出，并进入滑行安全状态。 */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;

  gpio.Pin = BOARD_HB_IN1_PIN | BOARD_HB_IN2_PIN | BOARD_HB_IN3_PIN | BOARD_HB_IN4_PIN;
  HAL_GPIO_Init(GPIOA, &gpio);

  HBridge_Coast();
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
}

void HBridge_Coast(void)
{
  __disable_irq();
  s_duty = 0;
  s_state = HBRIDGE_COAST;
  s_pwm_phase_us = 0U;
  __enable_irq();
  apply_coast();
}

void HBridge_Brake(void)
{
  __disable_irq();
  s_duty = 0;
  s_state = HBRIDGE_BRAKE;
  s_pwm_phase_us = 0U;
  __enable_irq();
  apply_brake();
}

void HBridge_TickISR(void)
{
  /* TIM1 每 20us 进入一次，用软件 PWM 方式驱动 H 桥。 */
  int16_t duty = s_duty;
  HBridgeState state = s_state;

  s_pwm_phase_us += HBRIDGE_PWM_TICK_US;
  uint16_t period_us = s_pwm_period_us;
  if (s_pwm_phase_us >= period_us)
  {
    s_pwm_phase_us = 0U;
  }

  if (state == HBRIDGE_BRAKE)
  {
    apply_brake();
    return;
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
  }
  else
  {
    apply_reverse();
  }
}

int16_t HBridge_GetDuty(void)
{
  return s_duty;
}

HBridgeState HBridge_GetState(void)
{
  return s_state;
}
