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
  uint8_t position_guard_latched = 0U;
  LcdmOutputGuardState guard = {1, 0U, 0U};
  LcdmOutputGuardResult guard_result;
  int16_t applied = 0;

  check("sub-step input edge does not consume the one-shot arm",
        !LcdmMotionPhase_ArmEdgeReady(39U, 40U, 100U, 8U));
  check("neutral error does not consume the one-shot arm",
        !LcdmMotionPhase_ArmEdgeReady(50U, 40U, 1U, 8U));
  check("error at the position guard remains coasted",
        !LcdmMotionPhase_ArmEdgeReady(50U, 40U, 8U, 8U));
  check("meaningful target step may consume the one-shot arm",
        LcdmMotionPhase_ArmEdgeReady(50U, 40U, 9U, 8U));

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

  input.elapsed_ms = 40U;
  input.feedback_valid = true;
  input.error_q4 = 800;
  input.velocity_q4 = 80;
  input.stop_distance_q4 = 0U;
  input.previous_phase = LCDM_MOTION_PHASE_DRIVE;
  input.previous_signed_duty_permille = 200;
  input.stop_model_valid = true;
  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 7U, 8U, 80U,
                                                &position_guard_latched);
  check("below position guard keeps normal drive phase",
        phase.phase == LCDM_MOTION_PHASE_DRIVE && phase.signed_duty_permille > 0);

  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 8U, 8U, 80U,
                                                &position_guard_latched);
  check("position guard changes drive to active reverse braking",
        phase.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE && phase.signed_duty_permille < 0 &&
        position_guard_latched == 1U);

  input.previous_phase = phase.phase;
  input.previous_signed_duty_permille = phase.signed_duty_permille;
  input.velocity_q4 = -24;
  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 9U, 8U, 80U,
                                                &position_guard_latched);
  check("guarded brake releases torque after speed crosses zero",
        phase.phase == LCDM_MOTION_PHASE_BRAKE_COAST && phase.signed_duty_permille == 0);

  input.previous_phase = phase.phase;
  input.velocity_q4 = 8;
  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 7U, 8U, 80U,
                                                &position_guard_latched);
  check("latched guard stays coasted after speed falls below limit",
        phase.phase == LCDM_MOTION_PHASE_COAST && phase.signed_duty_permille == 0);

  input.feedback_valid = false;
  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 8U, 8U, 80U,
                                                &position_guard_latched);
  check("latched guard still reports invalid feedback as fault",
        phase.phase == LCDM_MOTION_PHASE_FAULT && phase.signed_duty_permille == 0);

  input.feedback_valid = true;
  input.elapsed_ms = config.hard_timeout_ms;
  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 8U, 8U, 80U,
                                                &position_guard_latched);
  check("latched guard still reports hard timeout",
        phase.phase == LCDM_MOTION_PHASE_TIMEOUT && phase.signed_duty_permille == 0);

  position_guard_latched = 0U;
  input.elapsed_ms = 40U;
  input.previous_phase = LCDM_MOTION_PHASE_DRIVE;
  input.previous_signed_duty_permille = 200;
  input.velocity_q4 = 14;
  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 8U, 8U, 14U,
                                                &position_guard_latched);
  check("position guard uses observed low-speed peak to brake before nominal stop band",
        phase.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE &&
        phase.signed_duty_permille == -200);

  input.previous_phase = phase.phase;
  input.previous_signed_duty_permille = phase.signed_duty_permille;
  input.velocity_q4 = 3;
  phase = LcdmMotionPhase_UpdatePositionGuarded(&input, &config, 9U, 8U, 14U,
                                                &position_guard_latched);
  check("guard coasts when speed falls below one quarter of observed peak",
        phase.phase == LCDM_MOTION_PHASE_COAST && phase.signed_duty_permille == 0);

  printf("Motion phase + output guard: %d checks, %d failures\n", 18, failures);
  return failures ? 1 : 0;
}
