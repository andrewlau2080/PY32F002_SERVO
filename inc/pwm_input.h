#ifndef PWM_INPUT_H
#define PWM_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint16_t pulse_us;
  uint16_t raw_pulse_us;
  uint16_t candidate_pulse_us;
  uint8_t candidate_count;
  uint32_t period_us;
  uint32_t last_edge_ms;
  uint32_t edge_count;
  uint32_t pulse_count;
  bool raw_high;
  bool valid;
} PWM_InputSample;

void PWM_Input_Init(void);
void PWM_Input_EdgeISR(void);
PWM_InputSample PWM_Input_GetSample(void);
uint16_t PWM_Input_GetCommandUs(void);
bool PWM_Input_IsFresh(void);
uint16_t PWM_Input_TakeRawWindowUs(void);

#endif
