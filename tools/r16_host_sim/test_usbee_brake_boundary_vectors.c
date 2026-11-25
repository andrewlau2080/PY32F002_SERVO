#include <stdio.h>

#include "lcdm_motion_phase.h"

typedef struct
{
  const char *name;
  uint16_t switch_percent;
  int8_t direction;
} UsbEeReference;

static const UsbEeReference references[] = {
  {"1000->1500", 64U, 1},
  {"1500->2000", 55U, 1},
  {"2000->1000", 77U, -1},
  {"1000->2000", 82U, 1}
};

int main(void)
{
  const LcdmMotionPhaseConfig config = {
    500U, 32U, 96U, 8U, 160U, 16U, 0U,
    400U, 200U, 120U, 180U, 70U, 1280U, 60U
  };
  unsigned checks = 0U;
  unsigned failures = 0U;

  for (unsigned i = 0U; i < (sizeof(references) / sizeof(references[0])); ++i)
  {
    const UsbEeReference *reference = &references[i];
    const uint32_t span_q4 = 16000U;
    const uint32_t remaining_q4 =
        ((uint32_t)(100U - reference->switch_percent) * span_q4) / 100U;
    const int32_t signed_remaining =
        (int32_t)remaining_q4 * reference->direction;
    LcdmMotionPhaseInput input = {
      100U,
      signed_remaining,
      (int32_t)(80 * reference->direction),
      remaining_q4 - config.brake_margin_q4 - 1U,
      LCDM_MOTION_PHASE_DRIVE,
      (int16_t)(200 * reference->direction),
      true,
      true
    };
    LcdmMotionPhaseOutput output = LcdmMotionPhase_Update(&input, &config);
    int before_ok = (output.phase == LCDM_MOTION_PHASE_DRIVE) &&
                    ((output.signed_duty_permille * reference->direction) > 0);
    printf("%s: %s, just before predicted stop boundary remains DRIVE\n",
           before_ok ? "PASS" : "FAIL", reference->name);
    checks++;
    failures += (unsigned)!before_ok;

    input.stop_distance_q4++;
    output = LcdmMotionPhase_Update(&input, &config);
    int at_ok = (output.phase == LCDM_MOTION_PHASE_REVERSE_BRAKE) &&
                ((output.signed_duty_permille * reference->direction) < 0);
    printf("%s: %s, at predicted stop boundary reverses duty\n",
           at_ok ? "PASS" : "FAIL", reference->name);
    checks++;
    failures += (unsigned)!at_ok;
  }

  printf("USBee normalized brake-boundary vectors: %u checks, %u failures\n",
         checks, failures);
  return failures ? 1 : 0;
}
