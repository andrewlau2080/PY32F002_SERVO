#include "servo_adaptive.h"
#include "servo_model_store.h"

#include <string.h>

#define ADAPTIVE_MIN_DUTY          120U
#define ADAPTIVE_STABLE_DUTY_DELTA 80U
#define ADAPTIVE_MIN_VELOCITY_Q4   16U
#define ADAPTIVE_MIN_STABLE_DELTA_Q4 8U
#define ADAPTIVE_GAIN_STEP_DIVISOR   64U

volatile ServoAdaptiveSnapshot g_servo_adaptive_snapshot;
volatile uint16_t g_servo_adaptive_confidence_permille;
volatile uint16_t g_servo_adaptive_speed_gain_q8_forward;
volatile uint16_t g_servo_adaptive_speed_gain_q8_reverse;
volatile uint16_t g_servo_adaptive_brake_accel_q8_forward;
volatile uint16_t g_servo_adaptive_brake_accel_q8_reverse;
volatile uint16_t g_servo_adaptive_brake_samples_forward;
volatile uint16_t g_servo_adaptive_brake_samples_reverse;
volatile uint16_t g_servo_adaptive_drive_samples_forward;
volatile uint16_t g_servo_adaptive_drive_samples_reverse;
volatile uint16_t g_servo_adaptive_applied_velocity_gain_q8_forward;
volatile uint16_t g_servo_adaptive_applied_velocity_gain_q8_reverse;

static int16_t s_previous_velocity_q4;
static int16_t s_previous_duty;
static uint32_t s_previous_time_ms;
static uint8_t s_seeded;
static uint16_t s_prior_confidence_permille;

static void sync_telemetry(void)
{
  g_servo_adaptive_confidence_permille = g_servo_adaptive_snapshot.confidence_permille;
  g_servo_adaptive_speed_gain_q8_forward = g_servo_adaptive_snapshot.speed_gain_q8_forward;
  g_servo_adaptive_speed_gain_q8_reverse = g_servo_adaptive_snapshot.speed_gain_q8_reverse;
  g_servo_adaptive_brake_accel_q8_forward = g_servo_adaptive_snapshot.brake_accel_q8_forward;
  g_servo_adaptive_brake_accel_q8_reverse = g_servo_adaptive_snapshot.brake_accel_q8_reverse;
  g_servo_adaptive_brake_samples_forward = g_servo_adaptive_snapshot.brake_sample_count_forward;
  g_servo_adaptive_brake_samples_reverse = g_servo_adaptive_snapshot.brake_sample_count_reverse;
  g_servo_adaptive_drive_samples_forward = g_servo_adaptive_snapshot.drive_sample_count_forward;
  g_servo_adaptive_drive_samples_reverse = g_servo_adaptive_snapshot.drive_sample_count_reverse;
}

static uint16_t abs_i16(int16_t value)
{
  return (value < 0) ? (uint16_t)(-(int32_t)value) : (uint16_t)value;
}

static uint16_t clamp_u16(uint32_t value)
{
  return (value > 65535UL) ? 65535U : (uint16_t)value;
}

static void update_average(volatile uint16_t *average, uint16_t sample)
{
  if (*average == 0U)
  {
    *average = sample;
  }
  else
  {
    int32_t delta = (int32_t)sample - (int32_t)*average;
    *average = (uint16_t)((int32_t)*average + (delta / 8));
  }
}

static void increment_saturating(volatile uint16_t *count)
{
  if (*count < 65535U)
  {
    (*count)++;
  }
}

void Servo_Adaptive_Init(void)
{
  const ServoLearnedModel *saved = Servo_ModelStore_Get();

  memset((void *)&g_servo_adaptive_snapshot, 0, sizeof(g_servo_adaptive_snapshot));
  s_previous_velocity_q4 = 0;
  s_previous_duty = 0;
  s_previous_time_ms = 0U;
  s_seeded = 0U;
  s_prior_confidence_permille = 0U;
  g_servo_adaptive_applied_velocity_gain_q8_forward = 0U;
  g_servo_adaptive_applied_velocity_gain_q8_reverse = 0U;

  if (Servo_ModelStore_IsValid())
  {
    g_servo_adaptive_snapshot.confidence_permille = saved->confidence_permille;
    s_prior_confidence_permille = saved->confidence_permille;
    g_servo_adaptive_snapshot.reference_speed_gain_q8_forward = saved->speed_gain_q8_forward;
    g_servo_adaptive_snapshot.reference_speed_gain_q8_reverse = saved->speed_gain_q8_reverse;
    g_servo_adaptive_snapshot.speed_gain_q8_forward = saved->speed_gain_q8_forward;
    g_servo_adaptive_snapshot.speed_gain_q8_reverse = saved->speed_gain_q8_reverse;
    g_servo_adaptive_snapshot.coast_decay_q15_forward = saved->velocity_decay_q15_forward;
    g_servo_adaptive_snapshot.coast_decay_q15_reverse = saved->velocity_decay_q15_reverse;
    g_servo_adaptive_snapshot.brake_accel_q8_forward = saved->brake_accel_q8_forward;
    g_servo_adaptive_snapshot.brake_accel_q8_reverse = saved->brake_accel_q8_reverse;
  }
  sync_telemetry();
}

void Servo_Adaptive_Update(uint32_t now_ms,
                           int16_t velocity_q4,
                           int16_t applied_duty,
                           bool sample_valid)
{
  if (!sample_valid)
  {
    s_seeded = 0U;
    return;
  }

  if (s_seeded == 0U)
  {
    s_previous_velocity_q4 = velocity_q4;
    s_previous_duty = applied_duty;
    s_previous_time_ms = now_ms;
    s_seeded = 1U;
    return;
  }

  uint32_t dt_ms = now_ms - s_previous_time_ms;
  if ((dt_ms == 0U) || (dt_ms > SERVO_ADAPTIVE_MAX_SAMPLE_GAP_MS))
  {
    s_previous_velocity_q4 = velocity_q4;
    s_previous_duty = applied_duty;
    s_previous_time_ms = now_ms;
    return;
  }

  uint16_t old_speed = abs_i16(s_previous_velocity_q4);
  uint16_t new_speed = abs_i16(velocity_q4);
  uint16_t applied_duty_abs = abs_i16(applied_duty);
  uint16_t duty_delta = abs_i16((int16_t)(applied_duty - s_previous_duty));
  bool same_velocity_dir = ((s_previous_velocity_q4 > 0) && (velocity_q4 > 0)) ||
                           ((s_previous_velocity_q4 < 0) && (velocity_q4 < 0));
  bool duty_matches_velocity = (((applied_duty > 0) && (s_previous_velocity_q4 >= 0)) ||
                                ((applied_duty < 0) && (s_previous_velocity_q4 <= 0)));
  uint16_t velocity_delta = (new_speed > old_speed) ?
                            (uint16_t)(new_speed - old_speed) :
                            (uint16_t)(old_speed - new_speed);
  uint16_t stable_velocity_limit = old_speed / 4U;
  if (stable_velocity_limit < ADAPTIVE_MIN_STABLE_DELTA_Q4)
  {
    stable_velocity_limit = ADAPTIVE_MIN_STABLE_DELTA_Q4;
  }

  if ((applied_duty_abs >= ADAPTIVE_MIN_DUTY) &&
      (duty_delta <= ADAPTIVE_STABLE_DUTY_DELTA) &&
      duty_matches_velocity &&
      same_velocity_dir &&
      (velocity_delta <= stable_velocity_limit) &&
      (new_speed >= ADAPTIVE_MIN_VELOCITY_Q4))
  {
    /* Effective speed response: (ADC count/ms) per unit PWM, Q8 scaled. */
    uint32_t numerator = (uint32_t)new_speed * 16000UL;
    uint16_t candidate = clamp_u16(numerator / applied_duty_abs);
    if (applied_duty > 0)
    {
      if (g_servo_adaptive_snapshot.reference_speed_gain_q8_forward == 0U)
      {
        g_servo_adaptive_snapshot.reference_speed_gain_q8_forward = candidate;
      }
      update_average(&g_servo_adaptive_snapshot.speed_gain_q8_forward, candidate);
      increment_saturating(&g_servo_adaptive_snapshot.drive_sample_count_forward);
    }
    else
    {
      if (g_servo_adaptive_snapshot.reference_speed_gain_q8_reverse == 0U)
      {
        g_servo_adaptive_snapshot.reference_speed_gain_q8_reverse = candidate;
      }
      update_average(&g_servo_adaptive_snapshot.speed_gain_q8_reverse, candidate);
      increment_saturating(&g_servo_adaptive_snapshot.drive_sample_count_reverse);
    }
    uint32_t samples = (uint32_t)g_servo_adaptive_snapshot.drive_sample_count_forward +
                       (uint32_t)g_servo_adaptive_snapshot.drive_sample_count_reverse;
    uint16_t sample_confidence = (uint16_t)((samples * 1000UL) / (samples + 16UL));
    if (sample_confidence < s_prior_confidence_permille)
    {
      sample_confidence = s_prior_confidence_permille;
    }
    int32_t confidence_delta = (int32_t)sample_confidence -
                              (int32_t)g_servo_adaptive_snapshot.confidence_permille;
    int32_t confidence_step = confidence_delta / 16;
    if ((confidence_step == 0) && (confidence_delta != 0))
    {
      confidence_step = (confidence_delta > 0) ? 1 : -1;
    }
    g_servo_adaptive_snapshot.confidence_permille =
        (uint16_t)((int32_t)g_servo_adaptive_snapshot.confidence_permille + confidence_step);
  }

  if ((s_previous_velocity_q4 > 0) && (applied_duty < 0) &&
      (velocity_q4 > 0) && (new_speed < old_speed))
  {
    uint16_t candidate = clamp_u16(((uint32_t)(old_speed - new_speed) * 16UL) / dt_ms);
    update_average(&g_servo_adaptive_snapshot.brake_accel_q8_forward, candidate);
    increment_saturating(&g_servo_adaptive_snapshot.brake_sample_count_forward);
  }
  else if ((s_previous_velocity_q4 < 0) && (applied_duty > 0) &&
           (velocity_q4 < 0) && (new_speed < old_speed))
  {
    uint16_t candidate = clamp_u16(((uint32_t)(old_speed - new_speed) * 16UL) / dt_ms);
    update_average(&g_servo_adaptive_snapshot.brake_accel_q8_reverse, candidate);
    increment_saturating(&g_servo_adaptive_snapshot.brake_sample_count_reverse);
  }

  if ((dt_ms == 1U) && (applied_duty == 0) && same_velocity_dir &&
      (old_speed >= ADAPTIVE_MIN_VELOCITY_Q4))
  {
    uint16_t candidate = (uint16_t)(((uint32_t)new_speed * 32768UL) / old_speed);
    if (candidate > 32767U)
    {
      candidate = 32767U;
    }
    if (s_previous_velocity_q4 > 0)
    {
      update_average(&g_servo_adaptive_snapshot.coast_decay_q15_forward, candidate);
    }
    else
    {
      update_average(&g_servo_adaptive_snapshot.coast_decay_q15_reverse, candidate);
    }
  }

  s_previous_velocity_q4 = velocity_q4;
  s_previous_duty = applied_duty;
  s_previous_time_ms = now_ms;
  sync_telemetry();
}

void Servo_Adaptive_GetSnapshot(ServoAdaptiveSnapshot *snapshot)
{
  if (snapshot != NULL)
  {
    snapshot->speed_gain_q8_forward = g_servo_adaptive_snapshot.speed_gain_q8_forward;
    snapshot->confidence_permille = g_servo_adaptive_snapshot.confidence_permille;
    snapshot->reference_speed_gain_q8_forward = g_servo_adaptive_snapshot.reference_speed_gain_q8_forward;
    snapshot->reference_speed_gain_q8_reverse = g_servo_adaptive_snapshot.reference_speed_gain_q8_reverse;
    snapshot->speed_gain_q8_reverse = g_servo_adaptive_snapshot.speed_gain_q8_reverse;
    snapshot->coast_decay_q15_forward = g_servo_adaptive_snapshot.coast_decay_q15_forward;
    snapshot->coast_decay_q15_reverse = g_servo_adaptive_snapshot.coast_decay_q15_reverse;
    snapshot->brake_accel_q8_forward = g_servo_adaptive_snapshot.brake_accel_q8_forward;
    snapshot->brake_accel_q8_reverse = g_servo_adaptive_snapshot.brake_accel_q8_reverse;
    snapshot->drive_sample_count_forward = g_servo_adaptive_snapshot.drive_sample_count_forward;
    snapshot->drive_sample_count_reverse = g_servo_adaptive_snapshot.drive_sample_count_reverse;
    snapshot->brake_sample_count_forward = g_servo_adaptive_snapshot.brake_sample_count_forward;
    snapshot->brake_sample_count_reverse = g_servo_adaptive_snapshot.brake_sample_count_reverse;
  }
}

uint16_t Servo_Adaptive_GetFeedForwardDuty(int16_t ref_velocity_q4,
                                           uint16_t fallback_duty,
                                           uint16_t max_duty)
{
  uint16_t speed_gain;
  uint16_t confidence = g_servo_adaptive_snapshot.confidence_permille;
  uint32_t ref_abs = abs_i16(ref_velocity_q4);

  if ((ref_abs == 0U) || (max_duty == 0U))
  {
    return fallback_duty;
  }
  speed_gain = (ref_velocity_q4 > 0) ?
               g_servo_adaptive_snapshot.speed_gain_q8_forward :
               g_servo_adaptive_snapshot.speed_gain_q8_reverse;
  if ((speed_gain == 0U) || (confidence == 0U))
  {
    return fallback_duty;
  }

  uint32_t model_duty = (ref_abs * 16000UL) / speed_gain;
  if (model_duty > max_duty)
  {
    model_duty = max_duty;
  }
  int32_t blended = (int32_t)fallback_duty +
                    (((int32_t)model_duty - (int32_t)fallback_duty) * confidence) / 1000;
  if (blended < 0)
  {
    blended = 0;
  }
  else if (blended > max_duty)
  {
    blended = max_duty;
  }
  return (uint16_t)blended;
}

uint16_t Servo_Adaptive_GetVelocityGain(uint16_t base_gain,
                                       bool forward_direction)
{
  uint16_t current = forward_direction ?
                     g_servo_adaptive_snapshot.speed_gain_q8_forward :
                     g_servo_adaptive_snapshot.speed_gain_q8_reverse;
  uint16_t reference = forward_direction ?
                       g_servo_adaptive_snapshot.reference_speed_gain_q8_forward :
                       g_servo_adaptive_snapshot.reference_speed_gain_q8_reverse;
  uint16_t confidence = g_servo_adaptive_snapshot.confidence_permille;
  volatile uint16_t *applied_gain = forward_direction ?
      &g_servo_adaptive_applied_velocity_gain_q8_forward :
      &g_servo_adaptive_applied_velocity_gain_q8_reverse;
  uint32_t minimum = ((uint32_t)base_gain + 1U) / 2U;
  uint32_t maximum = (uint32_t)base_gain * 2U;
  if (maximum > 65535UL)
  {
    maximum = 65535UL;
  }

  if (base_gain == 0U)
  {
    *applied_gain = 0U;
    return base_gain;
  }

  uint32_t target = base_gain;
  if ((current != 0U) && (reference != 0U) && (confidence != 0U))
  {
    uint32_t tuned = ((uint32_t)base_gain * reference) / current;
    if (tuned < minimum)
    {
      tuned = minimum;
    }
    else if (tuned > maximum)
    {
      tuned = maximum;
    }
    int32_t confidence_clamped = (confidence > 1000U) ? 1000 : (int32_t)confidence;
    int32_t blended = (int32_t)base_gain +
        (((int32_t)tuned - (int32_t)base_gain) * confidence_clamped) / 1000;
    target = (blended < 1) ? 1U : (uint32_t)blended;
  }

  uint32_t output = *applied_gain;
  if (output == 0U)
  {
    /* Start from the fixed product gain; learn toward the model over time. */
    *applied_gain = base_gain;
    return base_gain;
  }
  if (output < minimum)
  {
    output = minimum;
  }
  else if (output > maximum)
  {
    output = maximum;
  }

  uint32_t step = ((uint32_t)base_gain + (ADAPTIVE_GAIN_STEP_DIVISOR - 1U)) /
                  ADAPTIVE_GAIN_STEP_DIVISOR;
  if (step == 0U)
  {
    step = 1U;
  }
  if (target > output)
  {
    output += ((target - output) > step) ? step : (target - output);
  }
  else if (output > target)
  {
    output -= ((output - target) > step) ? step : (output - target);
  }

  *applied_gain = (uint16_t)output;
  return (uint16_t)output;
}
