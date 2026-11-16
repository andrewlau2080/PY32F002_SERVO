#include <stdint.h>

#include <stdbool.h>

#include <stdio.h>

#define LCDM_SERVO_INPUT_MID_US 1500U

#define LCDM_SERVO_WORK_LOW_MV  250U

#define LCDM_SERVO_WORK_MID_MV  1650U

#define LCDM_SERVO_WORK_HIGH_MV 3050U

#define LCDM_SERVO_TARGET_ADC_VDD_MV 3300U

#define LCDM_SERVO_RANGE_RELEASE   0U

#define LCDM_SERVO_RANGE_LOCK      1U

#define LCDM_STARTUP_PARAM_INPUT_STABLE_BAND 3U

#define LCDM_SERVO_HOLD_RELEASE_BAND 16U

#define LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4 28

typedef struct
{
  const char *name;
  const char *unit;
  int32_t value;
  int32_t min;
  int32_t max;
  int32_t step;
} LcdmRuntimeParam;

typedef enum
{
  LCDM_PWM_RANGE_IN = 0,
  LCDM_PWM_RANGE_LOW = 1,
  LCDM_PWM_RANGE_HIGH = 2,
} LcdmPwmRangeState;

static LcdmRuntimeParam s_lcdm_cal_params[] = {
  {"Lo", "mV", LCDM_SERVO_WORK_LOW_MV, 150, 1650, 1},
  {"Md", "mV", LCDM_SERVO_WORK_MID_MV, 150, 3200, 1},
  {"Hi", "mV", LCDM_SERVO_WORK_HIGH_MV, 1650, 3200, 1},
  {"InL", "us", 600, 600, 1499, 1},
  {"InH", "us", 2400, 1501, 2400, 1},
};

static LcdmRuntimeParam s_lcdm_startup_params[] = {
  {"Rng", "", LCDM_SERVO_RANGE_LOCK, LCDM_SERVO_RANGE_RELEASE, LCDM_SERVO_RANGE_LOCK, 1},
  {"Dly", "ms", 300, 0, 3000, 10},
  {"Stb", "cnt", 4, 1, 10, 1},
  {"Bd", "us", 0, 0, 50, 1},
  {"CDB", "cnt", 1, 0, 120, 1},
};

static uint16_t s_lcdm_servo_latched_pulse_us = LCDM_SERVO_INPUT_MID_US;

static uint16_t s_lcdm_servo_candidate_pulse_us = LCDM_SERVO_INPUT_MID_US;

static uint32_t s_lcdm_servo_last_processed_pulse_count;

static uint8_t s_lcdm_servo_latched_pulse_seeded;

static uint8_t s_lcdm_servo_candidate_count;

static uint16_t lcdm_clamp_u16(uint32_t value, uint16_t min_value, uint16_t max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return (uint16_t)value;
}

static uint16_t lcdm_mv_to_adc(uint16_t mv, uint16_t vdd_mv)
{
  if (vdd_mv == 0U)
  {
    vdd_mv = 3300U;
  }
  return lcdm_clamp_u16(((((uint32_t)mv * 4095U) + (vdd_mv / 2U)) / vdd_mv), 0U, 4095U);
}

static LcdmPwmRangeState lcdm_pwm_range_state(uint16_t pulse_us)
{
  uint16_t low_us = (uint16_t)s_lcdm_cal_params[3].value;
  uint16_t high_us = (uint16_t)s_lcdm_cal_params[4].value;

  if (low_us >= LCDM_SERVO_INPUT_MID_US)
  {
    low_us = 600U;
  }
  if (high_us <= LCDM_SERVO_INPUT_MID_US)
  {
    high_us = 2400U;
  }

  if (pulse_us < low_us)
  {
    return LCDM_PWM_RANGE_LOW;
  }
  if (pulse_us > high_us)
  {
    return LCDM_PWM_RANGE_HIGH;
  }
  return LCDM_PWM_RANGE_IN;
}

static uint16_t lcdm_servo_update_latched_pulse(uint16_t pulse_us, uint32_t pulse_count)
{
  uint16_t stable_band = (uint16_t)s_lcdm_startup_params[LCDM_STARTUP_PARAM_INPUT_STABLE_BAND].value;
  uint16_t delta;

  if (stable_band > 50U)
  {
    stable_band = 50U;
  }

  if (s_lcdm_servo_latched_pulse_seeded == 0U)
  {
    s_lcdm_servo_latched_pulse_us = pulse_us;
    s_lcdm_servo_candidate_pulse_us = pulse_us;
    s_lcdm_servo_last_processed_pulse_count = pulse_count;
    s_lcdm_servo_latched_pulse_seeded = 1U;
    s_lcdm_servo_candidate_count = 0U;
    return s_lcdm_servo_latched_pulse_us;
  }

  if (pulse_count == s_lcdm_servo_last_processed_pulse_count)
  {
    return s_lcdm_servo_latched_pulse_us;
  }
  s_lcdm_servo_last_processed_pulse_count = pulse_count;

  if (lcdm_pwm_range_state(pulse_us) != LCDM_PWM_RANGE_IN)
  {
    s_lcdm_servo_latched_pulse_us = pulse_us;
    s_lcdm_servo_candidate_pulse_us = pulse_us;
    s_lcdm_servo_candidate_count = 0U;
    return s_lcdm_servo_latched_pulse_us;
  }

  delta = (pulse_us > s_lcdm_servo_latched_pulse_us) ?
          (uint16_t)(pulse_us - s_lcdm_servo_latched_pulse_us) :
          (uint16_t)(s_lcdm_servo_latched_pulse_us - pulse_us);
  if (delta > stable_band)
  {
    s_lcdm_servo_latched_pulse_us = pulse_us;
    s_lcdm_servo_candidate_pulse_us = pulse_us;
    s_lcdm_servo_candidate_count = 0U;
  }
  else
  {
    s_lcdm_servo_candidate_pulse_us = s_lcdm_servo_latched_pulse_us;
    s_lcdm_servo_candidate_count = 0U;
  }

  return s_lcdm_servo_latched_pulse_us;
}

static uint16_t lcdm_pwm_to_target_mv(uint16_t pulse_us)
{
  uint16_t low_mv = (uint16_t)s_lcdm_cal_params[0].value;
  uint16_t mid_mv = (uint16_t)s_lcdm_cal_params[1].value;
  uint16_t high_mv = (uint16_t)s_lcdm_cal_params[2].value;
  uint16_t low_us = (uint16_t)s_lcdm_cal_params[3].value;
  uint16_t high_us = (uint16_t)s_lcdm_cal_params[4].value;
  uint16_t mid_us = LCDM_SERVO_INPUT_MID_US;
  uint32_t numerator;
  uint32_t denominator;

  if (low_us >= mid_us)
  {
    low_us = 600U;
  }
  if (high_us <= mid_us)
  {
    high_us = 2400U;
  }
  if (mid_mv < low_mv)
  {
    mid_mv = low_mv;
  }
  if (high_mv < mid_mv)
  {
    high_mv = mid_mv;
  }

  if (pulse_us <= low_us)
  {
    return low_mv;
  }
  if (pulse_us >= high_us)
  {
    return high_mv;
  }
  if (pulse_us <= mid_us)
  {
    numerator = (uint32_t)(pulse_us - low_us) * (uint32_t)(mid_mv - low_mv);
    denominator = (uint32_t)(mid_us - low_us);
    return (uint16_t)(low_mv + ((numerator + (denominator / 2U)) / denominator));
  }

  numerator = (uint32_t)(pulse_us - mid_us) * (uint32_t)(high_mv - mid_mv);
  denominator = (uint32_t)(high_us - mid_us);
  return (uint16_t)(mid_mv + ((numerator + (denominator / 2U)) / denominator));
}

static uint16_t lcdm_pwm_to_target_adc(uint16_t pulse_us)
{
  uint16_t target_mv = lcdm_pwm_to_target_mv(pulse_us);
  return lcdm_mv_to_adc(target_mv, LCDM_SERVO_TARGET_ADC_VDD_MV);
}


static unsigned failures;
static void check(const char *name, int actual, int expected)
{
  printf("%s: %s (actual=%d expected=%d)\n",
         (actual == expected) ? "PASS" : "FAIL", name, actual, expected);
  if (actual != expected) { failures++; }
}

/* Stub observes whether the exact production HOLD condition is entered. */
static void lcdm_servo_enter_hold(uint16_t p, uint16_t raw, uint16_t target,
                                  uint16_t feedback, int16_t error, uint8_t event)
{
  (void)p; (void)raw; (void)target; (void)feedback; (void)error; (void)event;
}

static int hold_condition(uint16_t pulse_delta, uint16_t abs_error, uint16_t abs_velocity_q4,
                          bool hold_confirmed)
{
  bool target_motion_changed = (pulse_delta >= 3U);
  bool command_step = target_motion_changed;
  bool hold_entry_confirmed = hold_confirmed;
  uint16_t hold_release_band = LCDM_SERVO_HOLD_RELEASE_BAND;
  uint16_t motion_hold_vel_q4 = LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4;
  uint16_t control_pulse_us = 0U, raw_adc = 0U, s_lcdm_servo_cmd_target_adc = 0U, feedback = 0U;
  int16_t error = (int16_t)abs_error;
  
  if ((!command_step) &&
      hold_entry_confirmed &&
      (abs_error <= hold_release_band) &&
      (abs_velocity_q4 <= motion_hold_vel_q4))
  {
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    return 0;
  }
  return 1;
}

int main(void)
{
  struct test_case { uint16_t pulse; uint16_t adc; } cases[] = {
    {500, 310}, {600, 310}, {1000, 1082}, {1497, 2041}, {1498, 2044},
    {1499, 2045}, {1500, 2048}, {1501, 2050}, {1502, 2052},
    {1503, 2054}, {2000, 3013}, {2400, 3785}, {2500, 3785}
  };
  char label[64];
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    snprintf(label, sizeof(label), "target mapping %u us", cases[i].pulse);
    check(label, lcdm_pwm_to_target_adc(cases[i].pulse), cases[i].adc);
  }
  check("599 us is below range", lcdm_pwm_range_state(599), LCDM_PWM_RANGE_LOW);
  check("600 us is in range", lcdm_pwm_range_state(600), LCDM_PWM_RANGE_IN);
  check("2400 us is in range", lcdm_pwm_range_state(2400), LCDM_PWM_RANGE_IN);
  check("2401 us is above range", lcdm_pwm_range_state(2401), LCDM_PWM_RANGE_HIGH);
  check("default input stable band", s_lcdm_startup_params[LCDM_STARTUP_PARAM_INPUT_STABLE_BAND].value, 0);
  s_lcdm_servo_latched_pulse_seeded = 0U;
  check("initial latch 1500 us", lcdm_servo_update_latched_pulse(1500, 1), 1500);
  check("same frame is ignored", lcdm_servo_update_latched_pulse(1501, 1), 1500);
  check("new 1501 us frame updates latch", lcdm_servo_update_latched_pulse(1501, 2), 1501);
  check("new 1502 us frame updates latch", lcdm_servo_update_latched_pulse(1502, 3), 1502);
  check("new 1503 us frame updates latch", lcdm_servo_update_latched_pulse(1503, 4), 1503);
  check("new 1499 us frame updates latch", lcdm_servo_update_latched_pulse(1499, 5), 1499);
  check("+1 us waits for stable-time gate", hold_condition(1, 1, 0, false), 1);
  check("+1 us enters HOLD after stable-time gate", hold_condition(1, 1, 0, true), 0);
  check("+2 us waits for stable-time gate", hold_condition(2, 3, 0, false), 1);
  check("+2 us enters HOLD after stable-time gate", hold_condition(2, 3, 0, true), 0);
  check("+3 us command step skips this HOLD branch", hold_condition(3, 4, 0, true), 1);
  printf("HOLD branch checks describe software control flow, not physical motion.\n");
  return failures ? 1 : 0;
}

