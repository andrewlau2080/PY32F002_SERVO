#ifndef SERVO_STABILITY_TIMER_H
#define SERVO_STABILITY_TIMER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint16_t elapsed_ms;
  uint8_t armed;
} ServoStabilityTimer;

static inline void ServoStabilityTimer_Reset(ServoStabilityTimer *timer)
{
  timer->elapsed_ms = 0U;
  timer->armed = 0U;
}

/* Require repeated observations across real time; stale gaps restart the timer. */
static inline bool ServoStabilityTimer_Update(ServoStabilityTimer *timer,
                                               bool condition,
                                               uint32_t dt_ms,
                                               uint16_t required_ms,
                                               uint16_t max_gap_ms)
{
  if (!condition)
  {
    ServoStabilityTimer_Reset(timer);
    return false;
  }
  if (timer->armed == 0U)
  {
    timer->armed = 1U;
    timer->elapsed_ms = 0U;
    return required_ms == 0U;
  }
  if (dt_ms > max_gap_ms)
  {
    timer->elapsed_ms = 0U;
    return required_ms == 0U;
  }
  if ((dt_ms != 0U) && (timer->elapsed_ms < required_ms))
  {
    uint32_t elapsed = (uint32_t)timer->elapsed_ms + dt_ms;
    timer->elapsed_ms = (elapsed >= required_ms) ? required_ms : (uint16_t)elapsed;
  }
  return timer->elapsed_ms >= required_ms;
}

#endif
