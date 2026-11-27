#include <assert.h>
#include <stdio.h>
#include "lcdm_r01_jog_control.h"

static const LcdmR01JogConfig cfg = {6U, 3U, 8U, 185U, 70U, 80U, 1U, 30U, 60U, 35U, 15U, 3U};
static const LcdmR01JogConfig cfg_full_window = {8U, 3U, 8U, 185U, 120U, 80U, 1U, 100U, 60U, 35U, 15U, 3U};

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
  o = LcdmR01Jog_Update(&s, 2009U, 17U, &cfg);
  assert(o.phase == LCDM_R01_JOG_BRAKE);
  assert(o.velocity_q4_per_ms == 8);
  assert(o.duty_permille < 0 && o.duty_permille >= -70);
}

static void test_zero_speed_brake_kick_is_bounded(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 0U, &cfg));
  o = LcdmR01Jog_Update(&s, 2000U, 35U, &cfg);
  assert(o.phase == LCDM_R01_JOG_BRAKE);
  assert(o.duty_permille == -30);
  o = LcdmR01Jog_Update(&s, 2000U, 38U, &cfg);
  assert(o.phase == LCDM_R01_JOG_COAST && o.duty_permille == 0 && !o.active);
}

static void test_full_position_window_and_noise_margin(void)
{
  LcdmR01JogState s = {0};
  LcdmR01JogOutput o;
  assert(LcdmR01Jog_Begin(&s, 2000U, 1, 10U, &cfg_full_window));
  o = LcdmR01Jog_Update(&s, 2003U, 11U, &cfg_full_window);
  assert(o.phase == LCDM_R01_JOG_DRIVE && o.duty_permille == 185);
}

int main(void)
{
  run_direction(1);
  run_direction(-1);
  test_fault_and_limits();
  test_stale_sample_still_brakes();
  test_zero_speed_brake_kick_is_bounded();
  test_full_position_window_and_noise_margin();
  puts("R01 jog brake host vectors: 11/11 passed (controller arithmetic only)");
  return 0;
}
