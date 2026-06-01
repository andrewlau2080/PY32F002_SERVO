#include "servo_control.h"
#include "adc_feedback.h"
#include "hbridge.h"
#include "pwm_input.h"
#include "servo_params.h"
#include "py32f0xx_hal.h"

static ServoStatus s_status;
static uint16_t s_last_feedback;
static uint16_t s_ramped_target;
static uint8_t s_target_seeded;
static uint32_t s_stall_ms;
static uint32_t s_recover_until_ms;

/* 小工具函数：限制参数或控制量范围，避免异常参数导致溢出输出。 */
static int16_t clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return (int16_t)value;
}

uint16_t Servo_Control_MapPulseToAdc(uint16_t pulse_us)
{
  const ServoParams *params = Servo_Params_Get();
  /* 把 KC9806 类似的 Neutral/Left Range/Right Range 全部合成到 ADC 目标端点。 */
  int32_t left_adc = (int32_t)params->adc_min_count - params->left_range_count +
                     params->neutral_offset_count;
  int32_t center_adc = (int32_t)params->adc_center_count + params->neutral_offset_count;
  int32_t right_adc = (int32_t)params->adc_max_count + params->right_range_count +
                      params->neutral_offset_count;

  left_adc = clamp_i16(left_adc, 0, 4095);
  center_adc = clamp_i16(center_adc, 0, 4095);
  right_adc = clamp_i16(right_adc, 0, 4095);

  if (pulse_us <= params->pulse_lower_us)
  {
    return (uint16_t)left_adc;
  }
  if (pulse_us >= params->pulse_high_us)
  {
    return (uint16_t)right_adc;
  }

  int32_t target;
  /* 以中位为分界做左右两段线性映射，便于分别调左右行程。 */
  if (pulse_us <= params->pulse_center_us)
  {
    uint32_t us_span = params->pulse_center_us - params->pulse_lower_us;
    uint32_t us_offset = pulse_us - params->pulse_lower_us;
    target = left_adc + (((center_adc - left_adc) * (int32_t)us_offset) / (int32_t)us_span);
  }
  else
  {
    uint32_t us_span = params->pulse_high_us - params->pulse_center_us;
    uint32_t us_offset = pulse_us - params->pulse_center_us;
    target = center_adc + (((right_adc - center_adc) * (int32_t)us_offset) / (int32_t)us_span);
  }

  if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_INVERTER))
  {
    /* 方向反转不改 H 桥接线，只把目标位置镜像到相反方向。 */
    target = left_adc + right_adc - target;
  }

  return (uint16_t)clamp_i16(target, 0, 4095);
}

void Servo_Control_Init(void)
{
  /* 控制状态从无信号开始；目标先用中位，避免上电瞬间误冲。 */
  s_status.state = SERVO_STATE_NO_SIGNAL;
  s_status.input_us = Servo_Params_Get()->pulse_center_us;
  s_status.target_adc = Servo_Params_Get()->adc_center_count;
  s_status.feedback_adc = ADC_Feedback_GetFiltered();
  s_status.error_adc = 0;
  s_status.motor_duty = 0;
  s_status.input_valid = false;
  s_last_feedback = s_status.feedback_adc;
  s_ramped_target = s_status.feedback_adc;
  s_target_seeded = 0U;
  s_stall_ms = 0U;
  s_recover_until_ms = 0U;
  /* 驱动频率属于可调参数，不同马达可能对噪声和效率有不同表现。 */
  HBridge_SetFrequency(Servo_Params_Get()->drive_frequency_hz);
}

void Servo_Control_Update1ms(void)
{
  const ServoParams *params = Servo_Params_Get();
  bool input_valid = PWM_Input_IsFresh();
  uint16_t input_us = PWM_Input_GetCommandUs();
  uint16_t feedback = ADC_Feedback_UpdateFiltered();
  uint16_t target = Servo_Control_MapPulseToAdc(input_us);
  uint16_t effective_target = target;
  int16_t error;
  int16_t duty = 0;
  uint32_t now_ms = HAL_GetTick();
  uint32_t adc_span = params->adc_max_count - params->adc_min_count;
  uint32_t us_span = params->pulse_high_us - params->pulse_lower_us;
  uint16_t deadband_count = params->deadband_count_min;

  /* Dead Band 用 us 表示更贴近调试软件；运行时换算成 ADC count。 */
  if (us_span > 0U)
  {
    uint32_t converted = (adc_span * params->deadband_us) / us_span;
    if (converted > deadband_count)
    {
      deadband_count = (uint16_t)converted;
    }
  }

  if (s_target_seeded == 0U)
  {
    /* 第一次进入闭环时以当前位置作为软启动起点，减少上电冲击。 */
    s_ramped_target = feedback;
    s_target_seeded = 1U;
  }

  s_status.input_valid = input_valid;
  s_status.input_us = input_us;
  s_status.feedback_adc = feedback;

  if (!input_valid)
  {
    /* 失信号分支：按参数选择滑行、锁原位置，或回到 1500us 中位。 */
    if (!Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_LOSE_PPM_LOCK))
    {
      HBridge_Coast();
      s_status.state = SERVO_STATE_NO_SIGNAL;
      s_status.target_adc = target;
      s_status.error_adc = 0;
      s_status.motor_duty = 0;
      return;
    }

    if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_LOSE_PPM_ENABLE))
    {
      target = Servo_Control_MapPulseToAdc(params->pulse_center_us);
    }
    else
    {
      target = s_status.target_adc;
    }
  }

  if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_SOFT_START))
  {
    /* 软启动不是限制马达 PWM，而是限制“目标位置”的移动速度。 */
    uint16_t step = params->soft_start_step_count;
    if (step == 0U)
    {
      step = 1U;
    }
    if (target > (uint16_t)(s_ramped_target + step))
    {
      s_ramped_target = (uint16_t)(s_ramped_target + step);
    }
    else if ((uint16_t)(target + step) < s_ramped_target)
    {
      s_ramped_target = (uint16_t)(s_ramped_target - step);
    }
    else
    {
      s_ramped_target = target;
    }
    effective_target = s_ramped_target;
  }
  else
  {
    effective_target = target;
    s_ramped_target = target;
  }

  error = (int16_t)((int32_t)effective_target - (int32_t)feedback);
  s_status.target_adc = effective_target;
  s_status.error_adc = error;

  if ((int16_t)error <= (int16_t)deadband_count &&
      (int16_t)error >= -(int16_t)deadband_count)
  {
    /* 进入死区后停止追踪。极小误差可刹车保持，稍大但仍在死区则滑行。 */
    if ((int16_t)error <= (int16_t)params->brake_band_count &&
        (int16_t)error >= -(int16_t)params->brake_band_count)
    {
      HBridge_Brake();
    }
    else
    {
      HBridge_Coast();
    }
    s_status.state = SERVO_STATE_HOLD;
    s_status.motor_duty = 0;
    s_stall_ms = 0U;
    s_last_feedback = feedback;
    return;
  }

  if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_OL_PROTECT) &&
      ((int32_t)(now_ms - s_recover_until_ms) < 0))
  {
    /* 堵转保护恢复等待期内不再驱动，防止反复冲击齿轮和马达。 */
    HBridge_Coast();
    s_status.state = SERVO_STATE_STALL_PROTECT;
    s_status.motor_duty = 0;
    return;
  }

  int16_t abs_error = (error < 0) ? (int16_t)(-error) : error;
  /* Stretcher 是误差增益；Boost 是起动补偿，帮助克服齿轮箱静摩擦。 */
  int32_t calc = (((int32_t)abs_error * (int32_t)params->stretcher_q8) >> 8);
  calc += params->boost_duty;
  duty = clamp_i16(calc, params->boost_duty, params->max_duty);
  if (error < 0)
  {
    duty = (int16_t)-duty;
  }

  if (duty > 0)
  {
    /* Speed Adj 分开修正正反转速度，补偿机构左右阻力差。 */
    duty = clamp_i16(((int32_t)duty * params->forward_speed_q8) >> 8, 0, params->max_duty);
  }
  else if (duty < 0)
  {
    duty = (int16_t)-clamp_i16((((int32_t)(-duty)) * params->reverse_speed_q8) >> 8,
                               0,
                               params->max_duty);
  }

  int16_t move = (int16_t)((int32_t)feedback - (int32_t)s_last_feedback);
  if (move < 0)
  {
    move = (int16_t)-move;
  }

  if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_OL_PROTECT) &&
      ((duty > (int16_t)params->stall_duty_threshold) ||
       (duty < -(int16_t)params->stall_duty_threshold)) &&
      (move < (int16_t)params->stall_min_move_count))
  {
    /* 大占空比输出但电位器反馈几乎不动，认为可能堵转。 */
    if (s_stall_ms < params->stall_time_ms)
    {
      s_stall_ms++;
    }
    else
    {
      HBridge_Coast();
      s_status.state = SERVO_STATE_STALL_PROTECT;
      s_status.motor_duty = 0;
      s_recover_until_ms = now_ms + params->stall_recovery_ms;
      return;
    }
  }
  else
  {
    s_stall_ms = 0U;
  }
  s_last_feedback = feedback;

  HBridge_SetSignedDuty(duty);
  s_status.state = SERVO_STATE_DRIVE;
  s_status.motor_duty = duty;
}

ServoStatus Servo_Control_GetStatus(void)
{
  return s_status;
}
