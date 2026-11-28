#include "servo_control.h"
#include "servo_adaptive.h"
#include "adc_feedback.h"
#include "hbridge.h"
#include "pwm_input.h"
#include "servo_params.h"

#ifndef SERVO_R01_PRODUCTION_LOOP_TEST
#define SERVO_R01_PRODUCTION_LOOP_TEST 0
#endif
#ifndef SERVO_R01_CENTER_RETURN_TEST
#define SERVO_R01_CENTER_RETURN_TEST 0
#endif
#ifndef SERVO_R01_TEST_MAX_DUTY
#define SERVO_R01_TEST_MAX_DUTY 40
#endif
#ifndef SERVO_R01_TEST_POSITION_LIMIT_ADC
#define SERVO_R01_TEST_POSITION_LIMIT_ADC 16U
#endif
#ifndef SERVO_R01_TEST_ANCHOR_PULSE_US
#define SERVO_R01_TEST_ANCHOR_PULSE_US 1500U
#endif
#if SERVO_R01_TEST_ANCHOR_PULSE_US != 1500U && SERVO_R01_TEST_ANCHOR_PULSE_US != 1550U
#error R01 diagnostic anchor pulse must be 1500 or 1550 us
#endif
#if SERVO_R01_TEST_POSITION_LIMIT_ADC == 0U || SERVO_R01_TEST_POSITION_LIMIT_ADC > 128U
#error R01 test displacement limit must be 1..128 ADC counts
#endif
#include "servo_stability_timer.h"
#include "py32f0xx_hal.h"

#define SERVO_HOLD_SETTLE_MAX_VELOCITY_Q4 36U
#define SERVO_HOLD_SETTLE_MAX_SAMPLE_GAP_MS 4U

static ServoStatus s_status;
static uint16_t s_last_feedback;
static uint16_t s_velocity_last_feedback;
static uint32_t s_velocity_last_time_ms;
static uint8_t s_velocity_seeded;
static uint8_t s_velocity_input_was_valid;
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
static ServoStabilityTimer s_hold_settle_timer;
static uint32_t s_hold_settle_last_ms;
static uint16_t s_hold_settle_target_adc;
static int8_t s_last_drive_dir;
static int8_t s_reverse_pending_dir;
static uint32_t s_startup_until_ms;
static uint32_t s_reverse_pause_until_ms;
static uint32_t s_reverse_brake_until_ms;
static uint32_t s_stall_ms;
static uint32_t s_recover_until_ms;
#if BOARD_TSSOP20_DEBUG_PINS
static uint16_t s_breakaway_command_us;
static uint16_t s_breakaway_origin_adc;
static uint32_t s_breakaway_start_ms;
static bool s_breakaway_available;
static uint16_t s_precoast_target_adc;
static uint16_t s_precoast_origin_adc;
static uint16_t s_precoast_velocity_adc;
static uint32_t s_precoast_velocity_ms;
static uint32_t s_precoast_until_ms;
static int16_t s_precoast_velocity_q4;
static bool s_precoast_used;
#endif

#if SERVO_R01_PRODUCTION_LOOP_TEST
extern volatile uint8_t g_r01_production_test_active;
static int16_t s_r01_prod_center_offset_adc;
static uint16_t s_r01_prod_anchor_feedback_adc;
static uint16_t s_r01_prod_anchor_raw_adc;
static uint8_t s_r01_prod_anchor_seeded;
static uint8_t s_r01_prod_lock_telemetry_captured;
volatile uint8_t g_r01_prod_motion_locked;
volatile uint8_t g_r01_prod_motion_lock_reason;
volatile uint16_t g_r01_prod_max_displacement_adc;
volatile int16_t g_r01_prod_signed_displacement_adc;
volatile uint16_t g_r01_prod_anchor_raw_adc;
volatile uint16_t g_r01_prod_max_raw_displacement_adc;
volatile int16_t g_r01_prod_raw_displacement_adc;
volatile uint32_t g_r01_prod_lock_time_ms;
volatile uint16_t g_r01_prod_lock_input_us;
volatile uint16_t g_r01_prod_lock_feedback_adc;
volatile uint16_t g_r01_prod_lock_raw_adc;
volatile uint16_t g_r01_prod_lock_target_adc;
volatile int16_t g_r01_prod_lock_error_adc;
volatile int16_t g_r01_prod_lock_bridge_duty;
volatile uint16_t g_r01_prod_anchor_adc;
__attribute__((used)) volatile uint16_t g_r01_prod_precoast_adc;
__attribute__((used)) volatile int16_t g_r01_prod_precoast_velocity_q4;
volatile int16_t g_r01_prod_center_offset_adc;
volatile uint32_t g_r01_prod_nonzero_duty_count;
volatile uint16_t g_r01_prod_max_abs_duty;
volatile uint16_t g_r01_prod_feedback_min = 0xFFFFU;
volatile uint16_t g_r01_prod_feedback_max;
volatile int16_t g_r01_prod_last_duty;
#endif

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

  if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_INVERTER) ||
      Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_POT_REVERSE))
  {
    /* 方向反转不改 H 桥接线，只把目标位置镜像到相反方向。 */
    target = left_adc + right_adc - target;
  }

  return (uint16_t)clamp_i16(target, 0, 4095);
}

uint16_t Servo_Control_MapAdcToPulse(uint16_t adc_count)
{
  const ServoParams *params = Servo_Params_Get();
  uint16_t adc_low = Servo_Control_MapPulseToAdc(params->pulse_lower_us);
  uint16_t adc_mid = Servo_Control_MapPulseToAdc(params->pulse_center_us);
  uint16_t adc_high = Servo_Control_MapPulseToAdc(params->pulse_high_us);
  bool increasing = (adc_high >= adc_low);
  int32_t pulse_start;
  int32_t pulse_end;
  int32_t adc_start;
  int32_t adc_end;
  int32_t mapped_pulse;

  if (increasing)
  {
    if (adc_count < adc_low) adc_count = adc_low;
    if (adc_count > adc_high) adc_count = adc_high;
  }
  else
  {
    if (adc_count > adc_low) adc_count = adc_low;
    if (adc_count < adc_high) adc_count = adc_high;
  }

  if ((increasing && (adc_count <= adc_mid)) ||
      (!increasing && (adc_count >= adc_mid)))
  {
    pulse_start = params->pulse_lower_us;
    pulse_end = params->pulse_center_us;
    adc_start = adc_low;
    adc_end = adc_mid;
  }
  else
  {
    pulse_start = params->pulse_center_us;
    pulse_end = params->pulse_high_us;
    adc_start = adc_mid;
    adc_end = adc_high;
  }

  if (adc_end == adc_start)
  {
    return params->pulse_center_us;
  }

  mapped_pulse = pulse_start +
                 (((int32_t)adc_count - adc_start) *
                  (pulse_end - pulse_start)) /
                 (adc_end - adc_start);
  if (mapped_pulse < params->pulse_lower_us)
    mapped_pulse = params->pulse_lower_us;
  if (mapped_pulse > params->pulse_high_us)
    mapped_pulse = params->pulse_high_us;
  return (uint16_t)mapped_pulse;
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
  s_velocity_last_feedback = s_status.feedback_adc;
  s_velocity_last_time_ms = HAL_GetTick();
  s_velocity_seeded = 0U;
  s_velocity_input_was_valid = 0U;
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
  ServoStabilityTimer_Reset(&s_hold_settle_timer);
  s_hold_settle_last_ms = HAL_GetTick();
  s_hold_settle_target_adc = 0xFFFFU;
  s_last_drive_dir = 0;
  s_reverse_pending_dir = 0;
  s_startup_until_ms = HAL_GetTick() + Servo_Params_Get()->startup_delay_ms;
  s_reverse_pause_until_ms = 0U;
  s_reverse_brake_until_ms = 0U;
  s_stall_ms = 0U;
  s_recover_until_ms = 0U;
#if BOARD_TSSOP20_DEBUG_PINS
  s_breakaway_command_us = 0U;
  s_breakaway_available = false;
  s_precoast_target_adc = UINT16_MAX;
  s_precoast_velocity_ms = 0U;
  s_precoast_until_ms = 0U;
  s_precoast_velocity_q4 = 0;
  s_precoast_used = false;
#endif
#if SERVO_R01_PRODUCTION_LOOP_TEST
  s_r01_prod_anchor_seeded = 0U;
  g_r01_prod_motion_locked = 0U;
  g_r01_prod_motion_lock_reason = 0U;
  g_r01_prod_nonzero_duty_count = 0U;
  g_r01_prod_max_abs_duty = 0U;
  g_r01_prod_feedback_min = UINT16_MAX;
  g_r01_prod_feedback_max = 0U;
  g_r01_prod_last_duty = 0;
  g_r01_prod_precoast_adc = UINT16_MAX;
  g_r01_prod_precoast_velocity_q4 = 0;
#endif
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
  uint32_t hold_dt_ms = now_ms - s_hold_settle_last_ms;
  uint32_t adc_span = params->adc_max_count - params->adc_min_count;
  uint32_t us_span = params->pulse_high_us - params->pulse_lower_us;
  uint16_t deadband_count = params->deadband_count_min;
  uint16_t hold_limit;

  s_hold_settle_last_ms = now_ms;

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

#if BOARD_TSSOP20_DEBUG_PINS
  if (ADC_Feedback_IsVddBelow(params->vdd_nominal_mv,
                              params->vdd_warn_drop_mv, 100U))
  {
    /* Do not learn or keep holding a learned output while the supply is low. */
    Servo_Adaptive_Update(now_ms, 0, 0, false);
    HBridge_Coast();
    s_status.state = SERVO_STATE_VDD_LOW;
    s_status.target_adc = feedback;
    s_status.error_adc = 0;
    s_status.motor_duty = 0;
    s_hold_active = 0U;
    ServoStabilityTimer_Reset(&s_hold_settle_timer);
    return;
  }
#endif

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
      ServoStabilityTimer_Reset(&s_hold_settle_timer);
      return;
    }

    s_startup_done = 1U;
    s_startup_ramping = (params->startup_step_count > 0U) ? 1U : 0U;
    s_ramped_target = feedback;
  }

  int32_t velocity_q4 = 0;
  bool velocity_valid = false;
  if ((s_velocity_seeded == 0U) ||
      ((input_valid != false) && (s_velocity_input_was_valid == 0U)))
  {
    /* 输入恢复首帧只重建速度基准，避免把失联期间位移当成瞬时速度。 */
    s_velocity_last_feedback = feedback;
    s_velocity_last_time_ms = now_ms;
    s_velocity_seeded = 1U;
  }
  else
  {
    uint32_t dt_ms = now_ms - s_velocity_last_time_ms;
    if (dt_ms != 0U)
    {
      if (dt_ms <= SERVO_ADAPTIVE_MAX_SAMPLE_GAP_MS)
      {
        velocity_q4 = (((int32_t)feedback - (int32_t)s_velocity_last_feedback) * 16) /
                      (int32_t)dt_ms;
        velocity_valid = true;
      }
      /* A long gap reseeds the baseline without teaching from stale data. */
      s_velocity_last_feedback = feedback;
      s_velocity_last_time_ms = now_ms;
    }
  }
  s_velocity_input_was_valid = input_valid ? 1U : 0U;
  if (velocity_q4 > 32767)
  {
    velocity_q4 = 32767;
  }
  else if (velocity_q4 < -32768)
  {
    velocity_q4 = -32768;
  }
  Servo_Adaptive_Update(now_ms,
                       (int16_t)velocity_q4,
                       HBridge_GetDuty(),
                       input_valid && (s_startup_done != 0U) && velocity_valid);

  if (!input_valid)
  {
#if BOARD_TSSOP20_DEBUG_PINS
    s_breakaway_available = false;
    s_breakaway_command_us = 0U;
#endif
    /* 失信号分支：按参数选择滑行、锁原位置，或回到 1500us 中位。 */
    if (!Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_LOSE_PPM_LOCK))
    {
      HBridge_Coast();
      s_status.state = SERVO_STATE_NO_SIGNAL;
      s_status.target_adc = target;
      s_status.error_adc = 0;
      s_status.motor_duty = 0;
      s_hold_active = 0U;
      ServoStabilityTimer_Reset(&s_hold_settle_timer);
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

#if SERVO_R01_PRODUCTION_LOOP_TEST && !SERVO_R01_CENTER_RETURN_TEST
  if (g_r01_production_test_active != 0U)
  {
    if (s_r01_prod_anchor_seeded == 0U)
    {
      s_r01_prod_center_offset_adc = (int16_t)((int32_t)feedback -
          (int32_t)Servo_Control_MapPulseToAdc(SERVO_R01_TEST_ANCHOR_PULSE_US));
      s_r01_prod_anchor_feedback_adc = feedback;
      s_r01_prod_anchor_raw_adc = ADC_Feedback_GetRaw();
      g_r01_prod_anchor_adc = feedback;
      g_r01_prod_anchor_raw_adc = s_r01_prod_anchor_raw_adc;
      g_r01_prod_center_offset_adc = s_r01_prod_center_offset_adc;
      s_r01_prod_anchor_seeded = 1U;
      g_r01_prod_motion_locked = 0U;
      g_r01_prod_motion_lock_reason = 0U;
      s_r01_prod_lock_telemetry_captured = 0U;
      g_r01_prod_max_displacement_adc = 0U;
      g_r01_prod_signed_displacement_adc = 0;
      g_r01_prod_max_raw_displacement_adc = 0U;
      g_r01_prod_raw_displacement_adc = 0;
    }
    int32_t displacement = (int32_t)feedback - (int32_t)s_r01_prod_anchor_feedback_adc;
    int32_t raw_displacement = (int32_t)ADC_Feedback_GetRaw() -
                               (int32_t)s_r01_prod_anchor_raw_adc;
    g_r01_prod_signed_displacement_adc = (int16_t)displacement;
    g_r01_prod_raw_displacement_adc = (int16_t)raw_displacement;
    if (displacement < 0) displacement = -displacement;
    int32_t raw_abs_displacement = raw_displacement;
    if (raw_abs_displacement < 0) raw_abs_displacement = -raw_abs_displacement;
    if (displacement > g_r01_prod_max_displacement_adc)
      g_r01_prod_max_displacement_adc = (uint16_t)displacement;
    if (raw_abs_displacement > g_r01_prod_max_raw_displacement_adc)
      g_r01_prod_max_raw_displacement_adc = (uint16_t)raw_abs_displacement;
    if ((uint32_t)displacement >= SERVO_R01_TEST_POSITION_LIMIT_ADC)
    {
      g_r01_prod_motion_locked = 1U;
      g_r01_prod_motion_lock_reason |= 1U;
    }
    if ((uint32_t)raw_abs_displacement >= SERVO_R01_TEST_POSITION_LIMIT_ADC)
    {
      g_r01_prod_motion_locked = 1U;
      g_r01_prod_motion_lock_reason |= 2U;
    }
    int32_t anchored_target = (int32_t)target + s_r01_prod_center_offset_adc;
    if (anchored_target < 0) anchored_target = 0;
    if (anchored_target > 4095) anchored_target = 4095;
    target = (uint16_t)anchored_target;
    if (g_r01_prod_motion_locked != 0U)
    {
      if (s_r01_prod_lock_telemetry_captured == 0U)
      {
        g_r01_prod_lock_time_ms = HAL_GetTick();
        g_r01_prod_lock_input_us = input_us;
        g_r01_prod_lock_feedback_adc = feedback;
        g_r01_prod_lock_raw_adc = ADC_Feedback_GetRaw();
        g_r01_prod_lock_target_adc = target;
        g_r01_prod_lock_error_adc = (int16_t)((int32_t)target - feedback);
        g_r01_prod_lock_bridge_duty = HBridge_GetDuty();
        s_r01_prod_lock_telemetry_captured = 1U;
      }
      HBridge_Coast();
      s_status.state = SERVO_STATE_HOLD;
      s_status.target_adc = target;
      s_status.error_adc = (int16_t)((int32_t)target - feedback);
      s_status.motor_duty = 0;
      return;
    }
  }
#endif

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
  if (s_hold_settle_target_adc != effective_target)
  {
    s_hold_settle_target_adc = effective_target;
    ServoStabilityTimer_Reset(&s_hold_settle_timer);
  }
  s_status.target_adc = effective_target;
  s_status.error_adc = error;
  uint16_t abs_error = abs_i16(error);
#if BOARD_TSSOP20_DEBUG_PINS
  if (s_precoast_target_adc != target || !input_valid)
  {
    s_precoast_target_adc = target;
    s_precoast_origin_adc = feedback;
    s_precoast_velocity_adc = feedback;
    s_precoast_velocity_ms = now_ms;
    s_precoast_velocity_q4 = 0;
    s_precoast_until_ms = 0U;
    s_precoast_used = false;
  }
  uint32_t coast_sample_ms = now_ms - s_precoast_velocity_ms;
  if (coast_sample_ms >= 6U)
  {
    s_precoast_velocity_q4 = (coast_sample_ms <= 12U) ?
        (int16_t)(((int32_t)feedback - s_precoast_velocity_adc) * 16 /
                  (int32_t)coast_sample_ms) : 0;
    s_precoast_velocity_adc = feedback;
    s_precoast_velocity_ms = now_ms;
  }

  int32_t velocity_along_q4 = error > 0 ? s_precoast_velocity_q4 :
                                           -(int32_t)s_precoast_velocity_q4;
  /* Bench data show that longer travel toward lower ADC needs more room
   * to coast than short moves on this TSSOP20 mechanism. */
  bool longer_move = abs_i16((int16_t)((int32_t)s_precoast_origin_adc - target)) >= 48U;
  bool longer_reverse = longer_move && target < s_precoast_origin_adc;
  uint32_t coast_prediction_ms = longer_reverse ? 16U : 10U;
  if (input_valid && effective_target == target && !s_precoast_used &&
      abs_error > deadband_count && velocity_along_q4 >= 8 &&
      abs_error <= (uint32_t)deadband_count + (longer_move ? 5U : 0U) +
                   ((uint32_t)velocity_along_q4 * coast_prediction_ms + 15U) / 16U)
  {
    s_precoast_used = true;
#if SERVO_R01_PRODUCTION_LOOP_TEST
    g_r01_prod_precoast_adc = feedback;
    g_r01_prod_precoast_velocity_q4 = s_precoast_velocity_q4;
#endif
    s_precoast_until_ms = now_ms + (longer_reverse ? 18U : 12U);
  }
  if (s_precoast_until_ms != 0U && time_before(now_ms, s_precoast_until_ms))
  {
    HBridge_Coast();
    s_status.state = SERVO_STATE_DRIVE;
    s_status.motor_duty = 0;
    s_hold_active = 0U;
    s_last_feedback = feedback;
    return;
  }
  s_precoast_until_ms = 0U;
#endif
#if SERVO_R01_PRODUCTION_LOOP_TEST
  /* The guarded R01 diagnostic deliberately parks at its anchor before the
   * command step.  The production boost_duty is a minimum drive once the
   * normal closed loop is active, so without this diagnostic-only deadband
   * gate it applies PWM even at the anchor (error near zero).  That unwanted
   * anchor drive obscures the requested direction test and can trip the raw
   * ADC displacement guard before the pulse step begins. */
  if ((g_r01_production_test_active != 0U) && (abs_error <= deadband_count))
  {
    HBridge_Coast();
    s_status.state = SERVO_STATE_HOLD;
    s_status.motor_duty = 0;
    s_stall_ms = 0U;
    s_last_feedback = feedback;
    return;
  }
#endif
  hold_limit = (s_hold_active != 0U) ? params->hold_exit_band_count : deadband_count;
  if (hold_limit < deadband_count)
  {
    hold_limit = deadband_count;
  }
#if SERVO_R01_PRODUCTION_LOOP_TEST
  if (g_r01_production_test_active != 0U)
  {
    hold_limit = deadband_count;
  }
#endif

  bool velocity_settled = velocity_valid &&
                          (abs_i16((int16_t)velocity_q4) <=
                           SERVO_HOLD_SETTLE_MAX_VELOCITY_Q4);
  bool hold_candidate = (abs_error <= hold_limit) && velocity_settled;
  uint32_t stable_dt_ms = (hold_dt_ms <= SERVO_HOLD_SETTLE_MAX_SAMPLE_GAP_MS) ?
                          hold_dt_ms : (SERVO_HOLD_SETTLE_MAX_SAMPLE_GAP_MS + 1U);
  bool hold_confirmed = ServoStabilityTimer_Update(&s_hold_settle_timer,
                                                    hold_candidate,
                                                    stable_dt_ms,
                                                    params->hold_settle_ms,
                                                    SERVO_HOLD_SETTLE_MAX_SAMPLE_GAP_MS);

  if (hold_confirmed)
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

#if BOARD_TSSOP20_DEBUG_PINS
  /* Give a new command one bounded chance to overcome static friction.
   * Use the un-ramped input command to avoid rearming on every ramp tick. */
  if (input_valid && (s_breakaway_command_us == 0U ||
      abs_i16((int16_t)((int32_t)input_us - s_breakaway_command_us)) >= 8U))
  {
    s_breakaway_command_us = input_us;
    s_breakaway_origin_adc = feedback;
    s_breakaway_start_ms = now_ms;
    s_breakaway_available = true;
  }
  bool fine_step = abs_error < 24U;
  uint32_t breakaway_window_ms = fine_step ? 55U : 65U;
  uint16_t breakaway_move_limit = fine_step ? 3U : 4U;
  if (!input_valid || now_ms - s_breakaway_start_ms >= breakaway_window_ms ||
      abs_i16((int16_t)((int32_t)feedback - s_breakaway_origin_adc)) >= breakaway_move_limit)
  {
    s_breakaway_available = false;
  }
  if (s_breakaway_available && velocity_valid &&
      abs_i16((int16_t)velocity_q4) <= 8U &&
      abs_error >= (fine_step ? (uint32_t)deadband_count + 4U : 24U) &&
      duty != 0)
  {
    uint16_t floor = params->max_duty < 220U ? params->max_duty : 220U;
    if (abs_i16(duty) < floor)
    {
      duty = duty > 0 ? (int16_t)floor : -(int16_t)floor;
    }
  }
#endif

  if (Servo_Params_FlagEnabled(SERVO_PARAM_FLAG_MOTOR_REVERSE))
  {
    duty = (int16_t)-duty;
  }

  int8_t new_dir = (duty > 0) ? 1 : ((duty < 0) ? -1 : 0);
  bool reverse_pause_completed = false;
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
  if ((s_reverse_pending_dir != 0) && (new_dir == s_reverse_pending_dir))
  {
    reverse_pause_completed = true;
  }
  s_reverse_pending_dir = 0;

  if (!reverse_pause_completed && (new_dir != 0) &&
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

#if SERVO_R01_PRODUCTION_LOOP_TEST
  if (duty > SERVO_R01_TEST_MAX_DUTY)
  {
    duty = SERVO_R01_TEST_MAX_DUTY;
  }
  else if (duty < -SERVO_R01_TEST_MAX_DUTY)
  {
    duty = -SERVO_R01_TEST_MAX_DUTY;
  }
  if (duty != 0)
  {
    uint16_t abs_duty = abs_i16(duty);
    g_r01_prod_nonzero_duty_count++;
    g_r01_prod_last_duty = duty;
    if (abs_duty > g_r01_prod_max_abs_duty) g_r01_prod_max_abs_duty = abs_duty;
    if (feedback < g_r01_prod_feedback_min) g_r01_prod_feedback_min = feedback;
    if (feedback > g_r01_prod_feedback_max) g_r01_prod_feedback_max = feedback;
  }
#endif
  HBridge_SetSignedDuty(duty);
  s_last_drive_dir = new_dir;
  s_status.state = SERVO_STATE_DRIVE;
  s_status.motor_duty = duty;
}

ServoStatus Servo_Control_GetStatus(void)
{
  return s_status;
}
