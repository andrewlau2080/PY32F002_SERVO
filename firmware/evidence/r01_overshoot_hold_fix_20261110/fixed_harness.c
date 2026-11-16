
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static int32_t s_lcdm_servo_vel_q4, s_lcdm_servo_traj_pos_q4, s_lcdm_servo_traj_vel_q4;
static int16_t s_lcdm_servo_current_velocity, s_lcdm_servo_ref_velocity, s_lcdm_servo_current_duty;
static uint8_t s_lcdm_servo_current_state, s_lcdm_servo_hold_active, s_lcdm_servo_v3_phase;
static uint8_t s_lcdm_servo_hold_deviation_count;
static int8_t s_lcdm_servo_move_dir, s_lcdm_servo_motion_dir;
static uint16_t s_lcdm_servo_hold_anchor_adc, s_lcdm_servo_last_motion_error;
static uint16_t s_lcdm_servo_motion_settle_count, s_lcdm_servo_last_control_feedback_adc;
static uint16_t s_lcdm_servo_overshoot_adc;
static unsigned hold_calls;
static void HBridge_Brake(void) {}
static void lcdm_servo_clear_output_guard(void) {}
static void lcdm_servo_customer_profile_clear(void) {}
static void lcdm_servo_curve_publish(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
                                     int16_t e, int16_t f, uint8_t g)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; hold_calls++; }

#define LCDM_SERVO_STATE_HOLD   1U
#define LCDM_SERVO_V3_PHASE_HOLD 1U
#define LCDM_SERVO_V3_PHASE_LOCK 6U
#define LCDM_SERVO_V3_OVERSHOOT_HOLD_BAND 240U
static void lcdm_servo_enter_hold(uint16_t control_pulse_us,
                                  uint16_t raw_adc,
                                  uint16_t target_adc,
                                  uint16_t feedback_adc,
                                  int16_t error_adc,
                                  uint8_t event)
{
  HBridge_Brake();
  s_lcdm_servo_vel_q4 = 0;
  s_lcdm_servo_current_velocity = 0;
  s_lcdm_servo_traj_pos_q4 = (int32_t)target_adc << 4;
  s_lcdm_servo_traj_vel_q4 = 0;
  s_lcdm_servo_ref_velocity = 0;
  s_lcdm_servo_current_duty = 0;
  lcdm_servo_clear_output_guard();
  s_lcdm_servo_current_state = LCDM_SERVO_STATE_HOLD;
  s_lcdm_servo_hold_active = 1U;
  s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_HOLD;
  s_lcdm_servo_hold_anchor_adc = target_adc;
  s_lcdm_servo_hold_deviation_count = 0U;
  s_lcdm_servo_move_dir = 0;
  s_lcdm_servo_motion_dir = 0;
  s_lcdm_servo_last_motion_error = 0xFFFFU;
  s_lcdm_servo_motion_settle_count = 0U;
  lcdm_servo_customer_profile_clear();
  s_lcdm_servo_last_control_feedback_adc = feedback_adc;
  lcdm_servo_curve_publish(control_pulse_us, raw_adc, target_adc, feedback_adc, error_adc, 0, event);
}

static bool one_cycle(uint16_t abs_error)
{
  bool command_step = false, static_lock_candidate, static_lock_mode, crossed_target = false;
  uint16_t hold_release_band = 16U, motion_hold_vel_q4 = 28U;
  uint16_t brake_high_vel_q4 = 68U, v7_static_lock_band = 32U, v7_settle_band = 32U;
  uint16_t abs_velocity_q4 = 0U, control_pulse_us = 1500U, raw_adc = 2000U;
  uint16_t s_lcdm_servo_cmd_target_adc = 2100U, feedback = 2000U;
  int16_t error = (int16_t)abs_error;
    static_lock_candidate = (!command_step) &&
                          ((s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_HOLD) ||
                           (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_LOCK)) &&
                          (abs_error > v7_static_lock_band);
  static_lock_mode = static_lock_candidate;
  if (static_lock_candidate && (s_lcdm_servo_v3_phase != LCDM_SERVO_V3_PHASE_LOCK))
  {
    if (s_lcdm_servo_hold_deviation_count < 8U)
    {
      s_lcdm_servo_hold_deviation_count++;
    }
    static_lock_mode = (s_lcdm_servo_hold_deviation_count >= 8U);
  }
  else if (!static_lock_candidate)
  {
    s_lcdm_servo_hold_deviation_count = 0U;
  }
    if ((!command_step) &&
      (abs_error <= hold_release_band) &&
      (abs_velocity_q4 <= motion_hold_vel_q4))
  {
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    return false;
  }

  if ((!command_step) &&
      ((s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_HOLD) ||
       (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_LOCK)) &&
      (!static_lock_mode) &&
      (abs_error <= v7_static_lock_band) &&
      (abs_velocity_q4 <= (uint16_t)(brake_high_vel_q4 + 32U)))
  {
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    return false;
  }

  if ((!command_step) &&
      (crossed_target || static_lock_candidate) &&
      (!static_lock_mode) &&
      (abs_error <= v7_settle_band) &&
      (abs_velocity_q4 <= (uint16_t)(motion_hold_vel_q4 + 16U)))
  {
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    return false;
  }

  if ((!command_step) &&
      (!static_lock_mode) &&
      (s_lcdm_servo_overshoot_adc != 0U) &&
      (abs_error <= LCDM_SERVO_V3_OVERSHOOT_HOLD_BAND) &&
      (abs_velocity_q4 <= (uint16_t)(brake_high_vel_q4 + 32U)))
  {
    uint8_t hold_deviation_count = s_lcdm_servo_hold_deviation_count;
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    s_lcdm_servo_hold_deviation_count = hold_deviation_count;
    return false;
  }
  return true;
}

int main(void)
{
  unsigned continued = 0U;
  s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_HOLD;
  s_lcdm_servo_overshoot_adc = 100U;
  for (unsigned i = 0; i < 32U; i++) continued += one_cycle(100U);
  printf("overshoot=100, abs_error=100, cycles=32, HOLD calls=%u, cycles past HOLD=%u, deviation_count=%u\n",
         hold_calls, continued, s_lcdm_servo_hold_deviation_count);
  if (continued == 0U) {
    puts("OBSERVED: latched overshoot repeatedly resets the static correction confirmation.");
    return 1;
  }
  puts("OBSERVED: static correction can proceed under these modeled conditions.");
  return 0;
}

