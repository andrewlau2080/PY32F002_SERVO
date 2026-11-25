#include <stdio.h>

#include "lcdm_motion_phase.h"
#include "lcdm_output_guard.h"

static int failures;

static void check(const char *name, int condition)
{
  if (condition)
  {
    printf("PASS: %s\n", name);
  }
  else
  {
    printf("FAIL: %s\n", name);
    failures++;
  }
}

int main(void)
{
  LcdmMotionPhaseConfig config = {
    500U, 32U, 96U, 16U, 160U, 0U, 192U,
    200U, 200U, 0U, 200U, 80U, 192U, 40U
  };
  LcdmMotionPhaseInput input = {
    40U, -1600, 800, 1200U, LCDM_MOTION_PHASE_DRIVE, 200, true, false
  };
  LcdmMotionPhaseOutput phase;
  LcdmOutputGuardState guard = {1, 0U, 0U};
  LcdmOutputGuardResult guard_result;
  int16_t applied = 0;

  phase = LcdmMotionPhase_Update(&input, &config);
  check("moving-away phase requests reverse braking",
        phase.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE && phase.signed_duty_permille < 0);

  guard_result = LcdmOutputGuard_Apply(&guard, 100U, phase.signed_duty_permille,
                                       200, 8U, 180U, 40U, 1U, &applied);
  check("phase reversal starts 8ms blanking", 
        guard_result == LCDM_OUTPUT_GUARD_DEADTIME_STARTED && applied == 0);

  guard_result = LcdmOutputGuard_Apply(&guard, 107U, phase.signed_duty_permille,
                                       0, 8U, 180U, 40U, 1U, &applied);
  check("reverse output remains coasted before deadline",
        guard_result == LCDM_OUTPUT_GUARD_DEADTIME_WAIT && applied == 0);

  guard_result = LcdmOutputGuard_Apply(&guard, 108U, phase.signed_duty_permille,
                                       0, 8U, 180U, 40U, 1U, &applied);
  check("reverse braking resumes after blanking",
        guard_result == LCDM_OUTPUT_GUARD_READY && applied < 0);

  input.elapsed_ms = config.hard_timeout_ms;
  phase = LcdmMotionPhase_Update(&input, &config);
  check("absolute phase timeout returns zero duty",
        phase.phase == LCDM_MOTION_PHASE_TIMEOUT && phase.signed_duty_permille == 0);

  input.elapsed_ms = 20U;
  input.feedback_valid = false;
  phase = LcdmMotionPhase_Update(&input, &config);
  check("invalid feedback fails closed before output guard",
        phase.phase == LCDM_MOTION_PHASE_FAULT && phase.signed_duty_permille == 0);

  printf("Motion phase + output guard: 6 checks, %d failures\n", failures);
  return failures ? 1 : 0;
}
