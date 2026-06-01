#include "pwm_input.h"
#include "board.h"
#include "servo_params.h"
#include "servo_config.h"

static volatile uint32_t s_last_rise_us;
static volatile uint32_t s_last_period_us;
static volatile uint16_t s_last_pulse_us = SERVO_INPUT_FAILSAFE_US;
static volatile uint32_t s_last_edge_ms;
static volatile uint8_t s_valid;

void PWM_Input_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  /* PA4 默认作为普通舵机 PWM 输入；配置通讯物理层后续也会共用该脚。 */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  gpio.Pin = BOARD_PWM_IN_PIN;
  gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BOARD_PWM_IN_PORT, &gpio);

  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
}

void PWM_Input_EdgeISR(void)
{
  /* 双边沿中断：上升沿记录周期，下降沿计算高电平脉宽。 */
  uint32_t now_us = Board_Micros();
  uint32_t now_ms = HAL_GetTick();

  if (HAL_GPIO_ReadPin(BOARD_PWM_IN_PORT, BOARD_PWM_IN_PIN) == GPIO_PIN_SET)
  {
    s_last_period_us = now_us - s_last_rise_us;
    s_last_rise_us = now_us;
  }
  else
  {
    const ServoParams *params = Servo_Params_Get();
    uint32_t width = now_us - s_last_rise_us;
    /* 只有脉宽和周期都在合法范围内，才更新有效命令。 */
    if ((width >= params->pulse_lower_us) &&
        (width <= params->pulse_high_us) &&
        (s_last_period_us >= SERVO_INPUT_FRAME_MIN_US) &&
        (s_last_period_us <= SERVO_INPUT_FRAME_MAX_US))
    {
      s_last_pulse_us = (uint16_t)width;
      s_last_edge_ms = now_ms;
      s_valid = 1U;
    }
  }
}

PWM_InputSample PWM_Input_GetSample(void)
{
  PWM_InputSample sample;

  /* ISR 会更新这些变量，读取时短暂关中断保证一组数据一致。 */
  __disable_irq();
  sample.pulse_us = s_last_pulse_us;
  sample.period_us = s_last_period_us;
  sample.last_edge_ms = s_last_edge_ms;
  sample.valid = (s_valid != 0U);
  __enable_irq();

  return sample;
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
