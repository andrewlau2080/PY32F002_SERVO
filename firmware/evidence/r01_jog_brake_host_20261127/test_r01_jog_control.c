#include <assert.h>
#include <stdio.h>
#include "lcdm_r01_jog_control.h"

static const LcdmR01JogConfig cfg = {6U, 3U, 8U, 185U, 70U, 80U, 1U, 30U, 60U, 35U, 15U, 3U};
static const LcdmR01JogConfig cfg_full_window = {8U, 3U, 8U, 185U, 120U, 80U, 1U, 100U, 60U, 35U, 15U, 3U};
static const LcdmR01JogConfig cfg_r74 = {8U, 3U, 8U, 150U, 120U, 80U, 1U, 100U, 60U, 35U, 15U, 3U};

static void run_direction(int8_t direction)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  uint16_t raw = 2000U;
  int saw_drive = 0, saw_brake = 0, saw_coast = 0;
  assert(LcdmR01Jog_Begin(&s, raw, direction, 0U, &cfg));
  o = LcdmR01Jog_Update(&s, raw, 1U, &cfg);
  assert(o.phase == LCDM_R01_JOG_DRIVE);
  assert(o.duty_permille == direction * 185);
  saw_drive = 1;
  for (uint32_t t = 2U; t <= 20U && s.active; ++t)
  {
    if (t <= 5U) raw = (uint16_t)((int32_t)2000 + direction * (int32_t)(t - 1U));
    else if (t <= 8U) raw = (uint16_t)((int32_t)2000 + direction * (int32_t)(t - 1U));
    else raw = (uint16_t)((int32_t)2000 + direction * 5);
    o = LcdmR01Jog_Update(&s, raw, t, &cfg);
    assert(o.duty_permille <= 185 && o.duty_permille >= -185);
    if (o.phase == LCDM_R01_JOG_BRAKE)
    {
      saw_brake = 1;
      assert(o.duty_permille == 0 || (o.duty_permille * direction) <= 0);
    }
    if (o.phase == LCDM_R01_JOG_COAST) saw_coast = 1;
  }
  assert(saw_drive && saw_brake && saw_coast);
}

static void test_fault_and_limits(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg));
  o = LcdmR01Jog_Update(&s, 4090U, 1U, &cfg);
  assert(o.phase == LCDM_R01_JOG_ADC_FAULT && o.duty_permille == 0 && !o.active);
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg));
  o = LcdmR01Jog_Update(&s, 2000U, 60U, &cfg);
  assert(o.phase == LCDM_R01_JOG_TIMEOUT && o.duty_permille == 0 && !o.active);
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg));
  o = LcdmR01Jog_Update(&s, 2008U, 2U, &cfg);
  assert(o.phase == LCDM_R01_JOG_DRIVE);
  o = LcdmR01Jog_Update(&s, 2008U, 3U, &cfg);
  assert(o.phase == LCDM_R01_JOG_BRAKE || o.phase == LCDM_R01_JOG_COAST);
  assert(o.duty_permille <= 0);
}

static void test_stale_sample_still_brakes(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg));
  o = LcdmR01Jog_Update(&s, 2001U, 1U, &cfg);
  assert(o.phase == LCDM_R01_JOG_DRIVE);
  o = LcdmR01Jog_Update(&s, 2008U, 9U, &cfg);
  assert(o.phase == LCDM_R01_JOG_DRIVE);
  o = LcdmR01Jog_Update(&s, 2009U, 17U, &cfg);
  assert(o.phase == LCDM_R01_JOG_BRAKE);
  assert(o.velocity_q4_per_ms > 0);
  assert(o.duty_permille < 0 && o.duty_permille >= -70);
}

static void test_zero_speed_does_not_get_reverse_kick(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg));
  o = LcdmR01Jog_Update(&s, 2000U, 35U, &cfg);
  assert(o.phase == LCDM_R01_JOG_BRAKE);
  assert(o.duty_permille == 0);
  o = LcdmR01Jog_Update(&s, 2000U, 38U, &cfg);
  assert(o.phase == LCDM_R01_JOG_COAST && o.duty_permille == 0 && !o.active);
}

static void test_reverse_brake_remains_active_while_feedback_moves(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg));
  (void)LcdmR01Jog_Update(&s, 2001U, 1U, &cfg);
  (void)LcdmR01Jog_Update(&s, 2002U, 2U, &cfg);
  (void)LcdmR01Jog_Update(&s, 2003U, 3U, &cfg);
  (void)LcdmR01Jog_Update(&s, 2004U, 4U, &cfg);
  o = LcdmR01Jog_Update(&s, 2005U, 5U, &cfg);
  assert(o.phase == LCDM_R01_JOG_BRAKE);
  assert(o.velocity_q4_per_ms > (int32_t)cfg.stop_velocity_q4);
  assert(o.duty_permille < 0);
}

static void test_full_position_window_and_noise_margin(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 10U, &cfg_full_window));
  o = LcdmR01Jog_Update(&s, 2003U, 11U, &cfg_full_window);
  assert(o.phase == LCDM_R01_JOG_DRIVE && o.duty_permille == 185);
}

static void test_r74_trace_discrepancy_reproduction(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2496U, 1, 119025U, &cfg_r74));
  o = LcdmR01Jog_Update(&s, 2496U, 119031U, &cfg_r74);
  assert(o.target_adc == 2504U);
  assert(o.phase == LCDM_R01_JOG_DRIVE);
  assert(o.duty_permille == 150);
  assert(s.brake_reason == LCDM_R01_JOG_BRAKE_NONE);
}

static void test_brake_reason_telemetry(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg_r74));
  o = LcdmR01Jog_Update(&s, 2008U, 1U, &cfg_r74);
  assert(s.brake_reason == LCDM_R01_JOG_BRAKE_NONE);
  o = LcdmR01Jog_Update(&s, 2008U, 2U, &cfg_r74);
  assert(s.brake_reason == LCDM_R01_JOG_BRAKE_POSITION_LIMIT);
  assert(o.phase == LCDM_R01_JOG_BRAKE || o.phase == LCDM_R01_JOG_COAST);
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg_r74));
  o = LcdmR01Jog_Update(&s, 2005U, 1U, &cfg_r74);
  assert(s.brake_reason == LCDM_R01_JOG_BRAKE_NONE);
  o = LcdmR01Jog_Update(&s, 2005U, 2U, &cfg_r74);
  assert(s.brake_reason == LCDM_R01_JOG_BRAKE_TARGET_MARGIN);
  assert(o.phase == LCDM_R01_JOG_BRAKE);
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg_r74));
  o = LcdmR01Jog_Update(&s, 2000U, 35U, &cfg_r74);
  assert(s.brake_reason == LCDM_R01_JOG_BRAKE_DRIVE_TIMEOUT);
  assert(o.phase == LCDM_R01_JOG_BRAKE);
}

static void test_idle_noise_does_not_trip_reverse_position_guard(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  const uint16_t samples[] = {2495U, 2499U, 2495U, 2498U, 2497U, 2496U};
  assert(LcdmR01Jog_Begin(&s, 2491U, -1, 100U, &cfg_r74));
  for (uint32_t i = 0U; i < (uint32_t)(sizeof(samples) / sizeof(samples[0])); ++i)
  {
    o = LcdmR01Jog_Update(&s, samples[i], 101U + i, &cfg_r74);
    assert(s.brake_reason == LCDM_R01_JOG_BRAKE_NONE);
    assert(o.phase == LCDM_R01_JOG_DRIVE);
    assert(o.duty_permille == -150);
  }
}

static void test_sustained_displacement_still_trips_position_guard(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2491U, -1, 100U, &cfg_r74));
  o = LcdmR01Jog_Update(&s, 2499U, 101U, &cfg_r74);
  assert(o.phase == LCDM_R01_JOG_DRIVE);
  o = LcdmR01Jog_Update(&s, 2499U, 102U, &cfg_r74);
  assert(s.brake_reason == LCDM_R01_JOG_BRAKE_POSITION_LIMIT);
  assert(o.phase == LCDM_R01_JOG_BRAKE || o.phase == LCDM_R01_JOG_COAST);
}

int main(void)
{
  run_direction(1);
  run_direction(-1);
  test_fault_and_limits();
  test_stale_sample_still_brakes();
  test_zero_speed_does_not_get_reverse_kick();
  test_reverse_brake_remains_active_while_feedback_moves();
  test_full_position_window_and_noise_margin();
  test_r74_trace_discrepancy_reproduction();
  test_brake_reason_telemetry();
  test_idle_noise_does_not_trip_reverse_position_guard();
  test_sustained_displacement_still_trips_position_guard();
  puts("R01 jog controller host regression passed (controller arithmetic only)");
  return 0;
}
