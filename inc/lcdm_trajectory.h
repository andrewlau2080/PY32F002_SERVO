#ifndef LCDM_TRAJECTORY_H
#define LCDM_TRAJECTORY_H

#include <stdint.h>

static inline uint32_t LcdmTrajectory_SqrtBounded(uint32_t value, uint16_t max_value)
{
  uint32_t low = 0U;
  uint32_t high = max_value;
  while (low < high)
  {
    uint32_t mid = low + ((high - low + 1U) / 2U);
    if ((mid != 0U) && (mid <= (value / mid))) low = mid;
    else high = mid - 1U;
  }
  return low;
}

/* Position/velocity are Q4 ADC and Q4 ADC per millisecond. Acceleration is
 * Q4 ADC per millisecond squared. Updates are bounded to 1..4 ms. */
static inline void LcdmTrajectory_Update(uint32_t now_ms,
                                         int32_t target_pos_q4,
                                         int32_t measured_pos_q4,
                                         uint16_t max_velocity_q4_per_ms,
                                         uint16_t acceleration_q4_per_ms2,
                                         uint32_t *last_tick_ms,
                                         int32_t *position_q4,
                                         int32_t *velocity_q4_per_ms)
{
  uint32_t dt_ms = now_ms - *last_tick_ms;
  if (dt_ms == 0U) return;
  *last_tick_ms = now_ms;
  if ((dt_ms > 4U) || (max_velocity_q4_per_ms == 0U) || (acceleration_q4_per_ms2 == 0U))
  {
    *position_q4 = measured_pos_q4;
    *velocity_q4_per_ms = 0;
    return;
  }

  int32_t distance_q4 = target_pos_q4 - *position_q4;
  if (distance_q4 == 0)
  {
    *position_q4 = target_pos_q4;
    *velocity_q4_per_ms = 0;
    return;
  }

  uint32_t abs_distance_q4 = (distance_q4 < 0) ? (uint32_t)(-distance_q4) : (uint32_t)distance_q4;
  uint32_t stopping_velocity_sq = 2UL * acceleration_q4_per_ms2 * abs_distance_q4;
  uint16_t desired_abs_velocity = (uint16_t)LcdmTrajectory_SqrtBounded(
      stopping_velocity_sq, max_velocity_q4_per_ms);
  int32_t desired_velocity = (distance_q4 > 0) ? desired_abs_velocity : -(int32_t)desired_abs_velocity;
  int32_t max_delta_velocity = (int32_t)acceleration_q4_per_ms2 * (int32_t)dt_ms;
  int32_t old_velocity = *velocity_q4_per_ms;
  int32_t direction = (distance_q4 > 0) ? 1 : -1;
  int32_t candidate_velocity = desired_velocity;
  if (candidate_velocity > old_velocity)
  {
    candidate_velocity = old_velocity + max_delta_velocity;
    if (candidate_velocity > desired_velocity) candidate_velocity = desired_velocity;
  }
  else if (candidate_velocity < old_velocity)
  {
    candidate_velocity = old_velocity - max_delta_velocity;
    if (candidate_velocity < desired_velocity) candidate_velocity = desired_velocity;
  }
  if (candidate_velocity > (int32_t)max_velocity_q4_per_ms)
    candidate_velocity = max_velocity_q4_per_ms;
  if (candidate_velocity < -(int32_t)max_velocity_q4_per_ms)
    candidate_velocity = -(int32_t)max_velocity_q4_per_ms;
  if (((old_velocity * direction) > 0) && ((candidate_velocity * direction) > 0))
  {
    uint32_t old_speed = (uint32_t)(old_velocity * direction);
    uint32_t next_speed = (uint32_t)(candidate_velocity * direction);
    uint32_t step_distance = ((old_speed + next_speed) * dt_ms) / 2U;
    uint32_t future_stop_distance = (next_speed * next_speed) /
        (2UL * acceleration_q4_per_ms2) + next_speed * dt_ms;
    if (step_distance + future_stop_distance >= abs_distance_q4)
    {
      desired_velocity = old_velocity - direction * max_delta_velocity;
      if ((desired_velocity * direction) < 0) desired_velocity = 0;
    }
  }
  int32_t new_velocity = desired_velocity;
  if (desired_velocity > old_velocity)
  {
    new_velocity = old_velocity + max_delta_velocity;
    if (new_velocity > desired_velocity) new_velocity = desired_velocity;
  }
  else if (desired_velocity < old_velocity)
  {
    new_velocity = old_velocity - max_delta_velocity;
    if (new_velocity < desired_velocity) new_velocity = desired_velocity;
  }
  if (new_velocity > (int32_t)max_velocity_q4_per_ms) new_velocity = max_velocity_q4_per_ms;
  if (new_velocity < -(int32_t)max_velocity_q4_per_ms) new_velocity = -(int32_t)max_velocity_q4_per_ms;

  int32_t delta_pos_q4 = ((old_velocity + new_velocity) * (int32_t)dt_ms) / 2;
  int32_t new_position = *position_q4 + delta_pos_q4;
  if (((distance_q4 > 0) && (new_position >= target_pos_q4)) ||
      ((distance_q4 < 0) && (new_position <= target_pos_q4)))
  {
    new_position = target_pos_q4;
    new_velocity = 0;
  }
  *position_q4 = new_position;
  *velocity_q4_per_ms = new_velocity;
}

#endif
