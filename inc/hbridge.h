#ifndef HBRIDGE_H
#define HBRIDGE_H

#include <stdint.h>

typedef enum
{
  HBRIDGE_COAST = 0,
  HBRIDGE_BRAKE,
  HBRIDGE_FORWARD,
  HBRIDGE_REVERSE
} HBridgeState;

void HBridge_Init(void);
void HBridge_SetFrequency(uint16_t frequency_hz);
void HBridge_SetSignedDuty(int16_t duty);
void HBridge_Coast(void);
void HBridge_Brake(void);
void HBridge_TickISR(void);
int16_t HBridge_GetDuty(void);
HBridgeState HBridge_GetState(void);

#if SERVO_DIRECTION_PROBE
/* Debug build only: one explicitly armed hardware-limited positive pulse. */
extern volatile uint32_t g_direction_probe_request;
extern volatile uint32_t g_direction_probe_status;
extern volatile uint16_t g_direction_probe_adc;
void HBridge_DirectionProbeService(uint16_t raw_adc, uint32_t now_ms);
#endif

#endif
