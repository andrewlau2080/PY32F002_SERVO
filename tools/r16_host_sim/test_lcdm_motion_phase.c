#include <stdio.h>
#include "lcdm_motion_phase.h"

static unsigned failures;

static void check(const char *name, LcdmMotionPhaseOutput output,
                  LcdmMotionPhase phase, int16_t duty)
{
  int pass = (output.phase == phase) && (output.signed_duty_permille == duty);
  printf("%s: %s (phase=%u duty=%d expected_phase=%u expected_duty=%d)\n",
         pass ? "PASS" : "FAIL", name,
         (unsigned)output.phase, (int)output.signed_duty_permille,
         (unsigned)phase, (int)duty);
  if (!pass) failures++;
}

int main(void)
{
  const LcdmMotionPhaseConfig config = {
    500U, 16U, 160U, 16U, 160U, 0U, 300U,
    600U, 400U, 200U, 300U, 100U, 512U, 100U
  };
  LcdmMotionPhaseInput input = {
    0U, 0, 0, 0U, LCDM_MOTION_PHASE_COAST, 0, true, true
  };
  LcdmMotionPhaseOutput output;

  input.feedback_valid = false;
  output = LcdmMotionPhase_Update(&input, &config);
  check("invalid feedback fails closed", output, LCDM_MOTION_PHASE_FAULT, 0);

  input.feedback_valid = true;
  input.elapsed_ms = 500U;
  output = LcdmMotionPhase_Update(&input, &config);
  check("hard timeout coasts", output, LCDM_MOTION_PHASE_TIMEOUT, 0);

  input.elapsed_ms = 10U;
  input.error_q4 = 8;
  input.velocity_q4 = 8;
  output = LcdmMotionPhase_Update(&input, &config);
  check("settled position coasts", output, LCDM_MOTION_PHASE_COAST, 0);

  input.error_q4 = 1600;
  input.velocity_q4 = 80;
  input.stop_distance_q4 = 100U;
  output = LcdmMotionPhase_Update(&input, &config);
  check("far target drives toward error", output, LCDM_MOTION_PHASE_DRIVE, 400);

  input.error_q4 = 256;
  input.velocity_q4 = 0;
  output = LcdmMotionPhase_Update(&input, &config);
  check("drive tapers with remaining distance", output,
        LCDM_MOTION_PHASE_DRIVE, 250);

  input.error_q4 = 161;
  output = LcdmMotionPhase_Update(&input, &config);
  check("approach taper never exceeds full requested drive", output,
        LCDM_MOTION_PHASE_DRIVE, 194);

  input.error_q4 = 400;
  input.velocity_q4 = 80;
  input.stop_distance_q4 = 400U;
  output = LcdmMotionPhase_Update(&input, &config);
  check("learned stopping distance starts reverse brake", output,
        LCDM_MOTION_PHASE_REVERSE_BRAKE, -133);

  input.error_q4 = 800;
  input.velocity_q4 = 32;
  input.previous_phase = LCDM_MOTION_PHASE_REVERSE_BRAKE;
  input.previous_signed_duty_permille = -133;
  output = LcdmMotionPhase_Update(&input, &config);
  check("brake phase remains active while moving", output,
        LCDM_MOTION_PHASE_REVERSE_BRAKE, -33);

  input.error_q4 = 80;
  input.velocity_q4 = 8;
  output = LcdmMotionPhase_Update(&input, &config);
  check("settle duty tapers with residual error", output,
        LCDM_MOTION_PHASE_SETTLE, 44);

  input.error_q4 = 17;
  output = LcdmMotionPhase_Update(&input, &config);
  check("settle taper keeps nonzero request above stop band", output,
        LCDM_MOTION_PHASE_SETTLE, 1);

  input.error_q4 = 240;
  input.velocity_q4 = 80;
  input.stop_distance_q4 = 0U;
  input.stop_model_valid = false;
  output = LcdmMotionPhase_Update(&input, &config);
  check("untrained fallback brakes inside configured distance", output,
        LCDM_MOTION_PHASE_REVERSE_BRAKE, -133);

  input.error_q4 = 800;
  input.previous_phase = LCDM_MOTION_PHASE_COAST;
  input.stop_model_valid = false;
  output = LcdmMotionPhase_Update(&input, &config);
  check("untrained model drives outside fallback distance", output,
        LCDM_MOTION_PHASE_DRIVE, 200);

  input.error_q4 = -80;
  input.velocity_q4 = 80;
  input.stop_model_valid = true;
  input.previous_phase = LCDM_MOTION_PHASE_COAST;
  output = LcdmMotionPhase_Update(&input, &config);
  check("near-target moving-away velocity is opposed", output,
        LCDM_MOTION_PHASE_REVERSE_BRAKE, -133);

  input.error_q4 = 0;
  input.velocity_q4 = -80;
  output = LcdmMotionPhase_Update(&input, &config);
  check("zero error still brakes residual motion", output,
        LCDM_MOTION_PHASE_REVERSE_BRAKE, 133);

  input.error_q4 = 1000;
  input.velocity_q4 = -80;
  input.previous_phase = LCDM_MOTION_PHASE_COAST;
  output = LcdmMotionPhase_Update(&input, &config);
  check("moving away from a far target is first decelerated", output,
        LCDM_MOTION_PHASE_REVERSE_BRAKE, 133);

  {
    LcdmMotionPhaseConfig capped = config;
    capped.max_duty_permille = 1200U;
    capped.drive_duty_permille = 1100U;
    input.error_q4 = 1600;
    input.velocity_q4 = 0;
    input.previous_phase = LCDM_MOTION_PHASE_COAST;
    output = LcdmMotionPhase_Update(&input, &capped);
    check("duty is capped at full scale", output, LCDM_MOTION_PHASE_DRIVE, 1000);
  }

  {
    LcdmMotionPhaseConfig blocked = config;
    blocked.untrained_drive_duty_permille = 0U;
    input.error_q4 = 800;
    input.velocity_q4 = 0;
    input.stop_model_valid = false;
    input.previous_phase = LCDM_MOTION_PHASE_COAST;
    output = LcdmMotionPhase_Update(&input, &blocked);
    check("zero untrained drive allowance remains coasted", output,
          LCDM_MOTION_PHASE_COAST, 0);
  }

  {
    LcdmMotionPhaseConfig invalid = config;
    invalid.stop_error_band_q4 = 200U;
    invalid.settle_error_band_q4 = 100U;
    output = LcdmMotionPhase_Update(&input, &invalid);
    check("invalid band configuration fails closed", output, LCDM_MOTION_PHASE_FAULT, 0);
  }

  input.error_q4 = -80;
  input.velocity_q4 = INT32_MIN;
  input.previous_phase = LCDM_MOTION_PHASE_REVERSE_BRAKE;
  input.previous_signed_duty_permille = 300;
  output = LcdmMotionPhase_Update(&input, &config);
  check("INT32_MIN velocity magnitude remains bounded", output,
        LCDM_MOTION_PHASE_REVERSE_BRAKE, 300);

  input.error_q4 = 1000;
  input.velocity_q4 = -40;
  input.previous_phase = LCDM_MOTION_PHASE_REVERSE_BRAKE;
  input.previous_signed_duty_permille = -100;
  output = LcdmMotionPhase_Update(&input, &config);
  check("speed reversal after braking releases reverse duty", output,
        LCDM_MOTION_PHASE_BRAKE_COAST, 0);
  input.velocity_q4 = -32;
  input.previous_phase = output.phase;
  input.previous_signed_duty_permille = output.signed_duty_permille;
  output = LcdmMotionPhase_Update(&input, &config);
  check("brake coast persists until speed falls", output,
        LCDM_MOTION_PHASE_BRAKE_COAST, 0);
  input.velocity_q4 = -8;
  input.previous_phase = output.phase;
  output = LcdmMotionPhase_Update(&input, &config);
  check("position drive resumes after brake coast", output,
        LCDM_MOTION_PHASE_DRIVE, 200);

  {
    int sequence_ok = 1;
    input.elapsed_ms = 10U;
    input.error_q4 = 640;
    input.velocity_q4 = 0;
    input.stop_distance_q4 = 0U;
    input.stop_model_valid = true;
    input.previous_phase = LCDM_MOTION_PHASE_COAST;
    input.previous_signed_duty_permille = 0;
    output = LcdmMotionPhase_Update(&input, &config);
    sequence_ok &= (output.phase == LCDM_MOTION_PHASE_DRIVE) &&
                   (output.signed_duty_permille == 400);
    input.error_q4 = 480;
    input.velocity_q4 = 80;
    input.previous_phase = output.phase;
    input.previous_signed_duty_permille = output.signed_duty_permille;
    output = LcdmMotionPhase_Update(&input, &config);
    sequence_ok &= (output.phase == LCDM_MOTION_PHASE_DRIVE) &&
                   (output.signed_duty_permille == 381);
    input.error_q4 = 300;
    input.stop_distance_q4 = 300U;
    input.previous_phase = output.phase;
    input.previous_signed_duty_permille = output.signed_duty_permille;
    output = LcdmMotionPhase_Update(&input, &config);
    sequence_ok &= (output.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE) &&
                   (output.signed_duty_permille == -133);
    input.error_q4 = 200;
    input.velocity_q4 = 40;
    input.previous_phase = output.phase;
    input.previous_signed_duty_permille = output.signed_duty_permille;
    output = LcdmMotionPhase_Update(&input, &config);
    sequence_ok &= (output.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE) &&
                   (output.signed_duty_permille == -50);
    input.error_q4 = 80;
    input.velocity_q4 = 8;
    input.previous_phase = output.phase;
    input.previous_signed_duty_permille = output.signed_duty_permille;
    output = LcdmMotionPhase_Update(&input, &config);
    sequence_ok &= (output.phase == LCDM_MOTION_PHASE_SETTLE) &&
                   (output.signed_duty_permille == 44);
    input.error_q4 = 8;
    input.previous_phase = output.phase;
    output = LcdmMotionPhase_Update(&input, &config);
    sequence_ok &= (output.phase == LCDM_MOTION_PHASE_COAST) &&
                   (output.signed_duty_permille == 0);
    printf("%s: drive→brake→settle→coast sequence\n",
           sequence_ok ? "PASS" : "FAIL");
    if (!sequence_ok) failures++;
  }

  {
    int monotonic_ok = 1;
    int seen_brake = 0;
    input.error_q4 = 1000;
    input.velocity_q4 = 80;
    input.previous_phase = LCDM_MOTION_PHASE_COAST;
    input.stop_model_valid = true;
    for (uint32_t stop_distance = 0U; stop_distance <= 1200U; stop_distance += 10U)
    {
      input.stop_distance_q4 = stop_distance;
      output = LcdmMotionPhase_Update(&input, &config);
      if (output.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE)
      {
        seen_brake = 1;
      }
      else if (seen_brake && (output.phase != LCDM_MOTION_PHASE_REVERSE_BRAKE))
      {
        monotonic_ok = 0;
      }
    }
    printf("%s: increasing stop estimate cannot revert brake to drive\n",
           monotonic_ok && seen_brake ? "PASS" : "FAIL");
    if (!monotonic_ok || !seen_brake) failures++;
  }

  {
    int scaling_ok = 1;
    uint16_t previous_abs_duty = config.brake_duty_permille;
    input.error_q4 = 1000;
    input.stop_distance_q4 = 0U;
    input.previous_phase = LCDM_MOTION_PHASE_REVERSE_BRAKE;
    input.previous_signed_duty_permille = -300;
    input.stop_model_valid = true;
    for (int32_t velocity = 160; velocity > 16; --velocity)
    {
      input.velocity_q4 = velocity;
      output = LcdmMotionPhase_Update(&input, &config);
      uint16_t abs_duty = (uint16_t)(output.signed_duty_permille < 0 ?
                                     -output.signed_duty_permille :
                                     output.signed_duty_permille);
      if ((output.phase != LCDM_MOTION_PHASE_REVERSE_BRAKE) ||
          (output.signed_duty_permille >= 0) || (abs_duty > previous_abs_duty))
      {
        scaling_ok = 0;
      }
      previous_abs_duty = abs_duty;
    }
    printf("%s: reverse-brake duty falls monotonically with speed\n",
           scaling_ok ? "PASS" : "FAIL");
    if (!scaling_ok) failures++;
  }

  printf("%s: %u checks, %u failures\n", failures ? "FAIL" : "PASS",
         25U, failures);
  return failures ? 1 : 0;
}
