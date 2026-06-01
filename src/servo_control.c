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
static uint8_t s_startup_done;
static uint8_t s_startup_ramping;
static uint16_t s_input_stable_count;
static uint16_t s_adc_stable_count;
static uint16_t s_prev_startup_input;
static uint16_t s_prev_startup_feedback;
static uint8_t s_hold_active;
static uint32_t s_hold_enter_ms;
static int8_t s_last_drive_dir;
static int8_t s_reverse_pending_dir;
static uint32_t s_startup_until_ms;
static uint32_t s_reverse_pause_until_ms;
static uint32_t s_reverse_brake_until_ms;
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

static uint16_t abs_i16(int16_t value)
{
  return (value < 0) ? (uint16_t)(-value) : (uint16_t)value;
}

static bool time_before(uint32_t now_ms, uint32_t deadline_ms)
{
  return ((int32_t)(now_ms - deadline_ms) < 0);
}

static bool startup_ready(const ServoParams *params,
                          bool input_valid,
                          uint16_t input_us,
                          uint16_t feedback,
                          uint32_t now_ms)
{
  uint16_t input_delta = (input_us > s_prev_startup_input) ?
                         (uint16_t)(input_us - s_prev_startup_input) :
                         (uint16_t)(s_prev_startup_input - input_us);
  uint16_t feedback_delta = (feedback > s_prev_startup_feedback) ?
                            (uint16_t)(feedback - s_prev_startup_feedback) :
                            (uint16_t)(s_prev_startup_feedback - feedback);

  if (time_before(now_ms, s_startup_until_ms))
  {
    s_prev_startup_input = input_us;
    s_prev_startup_feedback = feedback;
    return false;
  }

  if (params->startup_input_stable_count == 0U)
  {
    s_input_stable_count = 0U;
  }
  else if (!input_valid)
  {
    s_input_stable_count = 0U;
  }
  else if (input_delta <= params->startup_input_stable_us)
  {
    if (s_input_stable_count < params->startup_input_stable_count)
    {
      s_input_stable_count++;
    }
  }
  else
  {
    s_input_stable_count = 1U;
  }

  if (params->startup_adc_stable_count == 0U)
  {
    s_adc_stable_count = 0U;
  }
  else if (feedback_delta <= params->startup_adc_stable_band_count)
  {
    if (s_adc_stable_count < params->startup_adc_stable_count)
    {
      s_adc_stable_count++;
    }
  }
  else
  {
    s_adc_stable_count = 1U;
  }

  s_prev_startup_input = input_us;
  s_prev_startup_feedback = feedback;

  return ((params->startup_input_stable_count == 0U) ||
          (s_input_stable_count >= params->startup_input_stable_count)) &&
         ((params->startup_adc_stable_count == 0U) ||
          (s_adc_stable_count >= params->startup_adc_stable_count));
}

static void apply_hold_output(const ServoParams *params, uint32_t now_ms)
{
  if (params->hold_mode == SERVO_HOLD_MODE_BRAKE)
  {
    HBridge_Brake();
  }
  else if ((params->hold_mode == SERVO_HOLD_MODE_BRAKE_THEN_COAST) &&
           ((uint32_t)(now_ms - s_hold_enter_ms) < params->hold_brake_time_ms))
  {
    HBridge_Brake();
  }
  else
  {
    HBridge_Coast();
  }
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
  s_startup_done = 0U;
  s_startup_ramping = 0U;
  s_input_stable_count = 0U;
  s_adc_stable_count = 0U;
  s_prev_startup_input = s_status.input_us;
  s_prev_startup_feedback = s_status.feedback_adc;
  s_hold_active = 0U;
  s_hold_enter_ms = HAL_GetTick();
  s_last_drive_dir = 0;
  s_reverse_pending_dir = 0;
  s_startup_until_ms = HAL_GetTick() + Servo_Params_Get()->startup_delay_ms;
  s_reverse_pause_until_ms = 0U;
  s_reverse_brake_until_ms = 0U;
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
  uint16_t hold_limit;

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

  if (s_startup_done == 0U)
  {
    if (!startup_ready(params, input_valid, input_us, feedback, now_ms))
    {
      /* 上电阶段先等待 PWM 和 ADC 稳定，避免旧样机出现的上电乱跑。 */
      HBridge_Coast();
      s_ramped_target = feedback;
      s_status.state = SERVO_STATE_NO_SIGNAL;
      s_status.target_adc = feedback;
      s_status.error_adc = 0;
      s_status.motor_duty = 0;
      return;
    }

    s_startup_done = 1U;
    s_startup_ramping = (params->startup_step_count > 0U) ? 1U : 0U;
    s_ramped_target = feedback;
  }

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
      s_hold_active = 0U;
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
    if ((s_startup_ramping != 0U) && (params->startup_step_count > 0U))
    {
      step = params->startup_step_count;
    }
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
      s_startup_ramping = 0U;
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
  uint16_t abs_error = abs_i16(error);
  hold_limit = (s_hold_active != 0U) ? params->hold_exit_band_count : deadband_count;
  if (hold_limit < deadband_count)
  {
    hold_limit = deadband_count;
  }

  if (abs_error <= hold_limit)
  {
    /* 进入到位区后按参数选择滑行、持续刹车或短刹车后滑行。 */
    if (s_hold_active == 0U)
    {
      s_hold_active = 1U;
      s_hold_enter_ms = now_ms;
    }
    apply_hold_output(params, now_ms);
    s_status.state = SERVO_STATE_HOLD;
    s_status.motor_duty = 0;
    s_stall_ms = 0U;
    s_last_feedback = feedback;
    return;
  }
  s_hold_active = 0U;

  if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_OL_PROTECT) &&
      time_before(now_ms, s_recover_until_ms))
  {
    /* 堵转保护恢复等待期内不再驱动，防止反复冲击齿轮和马达。 */
    HBridge_Coast();
    s_status.state = SERVO_STATE_STALL_PROTECT;
    s_status.motor_duty = 0;
    return;
  }

  uint16_t gain_q8 = params->stretcher_q8;
  uint16_t boost_duty = params->boost_duty;
  if (abs_error <= params->close_error_count)
  {
    gain_q8 = params->close_stretcher_q8;
    boost_duty = params->close_boost_duty;
  }
  else if (abs_error <= params->approach_error_count)
  {
    gain_q8 = params->approach_stretcher_q8;
    boost_duty = params->approach_boost_duty;
  }

  /* 近目标、中距离、大误差分段增益，避免到位附近嗞嗞声和反复抖动。 */
  int32_t calc = (((int32_t)abs_error * (int32_t)gain_q8) >> 8);
  calc += boost_duty;
  duty = clamp_i16(calc, boost_duty, params->max_duty);
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

  int8_t new_dir = (duty > 0) ? 1 : ((duty < 0) ? -1 : 0);
  if ((s_reverse_pending_dir != 0) && time_before(now_ms, s_reverse_pause_until_ms))
  {
    if (time_before(now_ms, s_reverse_brake_until_ms))
    {
      HBridge_Brake();
    }
    else
    {
      HBridge_Coast();
    }
    s_status.state = SERVO_STATE_DRIVE;
    s_status.motor_duty = 0;
    return;
  }
  s_reverse_pending_dir = 0;

  if ((new_dir != 0) &&
      (s_last_drive_dir != 0) &&
      (new_dir != s_last_drive_dir) &&
      (params->reverse_pause_ms > 0U))
  {
    s_reverse_pending_dir = new_dir;
    s_reverse_pause_until_ms = now_ms + params->reverse_pause_ms;
    s_reverse_brake_until_ms = now_ms + params->reverse_brake_ms;
    if (params->reverse_brake_ms > 0U)
    {
      HBridge_Brake();
    }
    else
    {
      HBridge_Coast();
    }
    s_status.state = SERVO_STATE_DRIVE;
    s_status.motor_duty = 0;
    return;
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
  s_last_drive_dir = new_dir;
  s_status.state = SERVO_STATE_DRIVE;
  s_status.motor_duty = duty;
}

ServoStatus Servo_Control_GetStatus(void)
{
  return s_status;
}
