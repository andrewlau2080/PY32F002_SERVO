#include <stdio.h>

#include "lcdm_output_guard.h"

static unsigned failures;
static unsigned checks;

static void check(const char *name, int pass)
{
  printf("%s: %s\n", pass ? "PASS" : "FAIL", name);
  checks++;
  failures += (unsigned)!pass;
}

int main(void)
{
  LcdmOutputGuardState state = {1, 0U, 0U};
  int16_t applied = 0;
  LcdmOutputGuardResult result;

  result = LcdmOutputGuard_Apply(&state, 100U, 200, 100, 8U, 180U,
                                 80U, 1U, &applied);
  check("same-direction output slew caps increase to 180", 
        (result == LCDM_OUTPUT_GUARD_READY) && (applied == 180));

  result = LcdmOutputGuard_Apply(&state, 101U, -300, 180, 8U, 180U,
                                 80U, 1U, &applied);
  check("FWD to REV starts an 8ms coast interval",
        (result == LCDM_OUTPUT_GUARD_DEADTIME_STARTED) && (applied == 0) &&
        (state.reverse_dead_until_ms == 109U));

  result = LcdmOutputGuard_Apply(&state, 108U, -300, 0, 8U, 180U,
                                 80U, 1U, &applied);
  check("reverse output remains coasted before deadline",
        (result == LCDM_OUTPUT_GUARD_DEADTIME_WAIT) && (applied == 0));

  result = LcdmOutputGuard_Apply(&state, 109U, -300, 0, 8U, 180U,
                                 80U, 1U, &applied);
  check("reverse output resumes after deadline through slew",
        (result == LCDM_OUTPUT_GUARD_READY) && (applied == -80) &&
        (state.output_dir == -1) && (state.reverse_dead_active == 0U));

  state = (LcdmOutputGuardState){-1, 0U, 0U};
  result = LcdmOutputGuard_Apply(&state, 200U, 300, -100, 8U, 180U,
                                 80U, 1U, &applied);
  check("REV to FWD also starts blanking",
        (result == LCDM_OUTPUT_GUARD_DEADTIME_STARTED) && (applied == 0));

  state = (LcdmOutputGuardState){1, 0U, 0U};
  result = LcdmOutputGuard_Apply(&state, 300U, -100, 100, 8U, 180U,
                                 80U, 1U, &applied);
  check("sub-threshold reversal slews to zero without bypassing output guard",
        (result == LCDM_OUTPUT_GUARD_READY) && (applied == 20));

  state = (LcdmOutputGuardState){1, 4U, 1U};
  result = LcdmOutputGuard_Apply(&state, UINT32_MAX - 1U, -300, 0, 8U, 180U,
                                 80U, 1U, &applied);
  check("deadtime wait handles tick wrap before deadline",
        (result == LCDM_OUTPUT_GUARD_DEADTIME_WAIT) && (applied == 0));
  result = LcdmOutputGuard_Apply(&state, 4U, -300, 0, 8U, 180U,
                                 80U, 1U, &applied);
  check("deadtime expires correctly across tick wrap",
        (result == LCDM_OUTPUT_GUARD_READY) && (applied == -80));

  state = (LcdmOutputGuardState){0, 0U, 0U};
  result = LcdmOutputGuard_Apply(&state, 10U, 500, 0, 8U, 180U,
                                 80U, UINT32_MAX, &applied);
  check("large elapsed interval saturates slew safely",
        (result == LCDM_OUTPUT_GUARD_READY) && (applied == 500));

  printf("Output guard: %u checks, %u failures\n", checks, failures);
  return failures ? 1 : 0;
}
