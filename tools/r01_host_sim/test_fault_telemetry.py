#!/usr/bin/env python3
"""Exercise actual LCDM early-return branches with a fake curve publisher."""
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "src/app_entry.c").read_text()


def branch(marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[start:pos + 1]
    raise ValueError("unbalanced branch")


adc = branch("if (!lcdm_servo_adc_is_valid(raw_adc))")
lost = branch("if (!fresh)")
out_of_range = branch("if ((range_state != LCDM_PWM_RANGE_IN) &&")
names = ("LCDM_SERVO_ADC_LOST_TRIP_COUNT", "LCDM_SERVO_STATE_ADC_LOST",
         "LCDM_SERVO_STATE_HOLD", "LCDM_SERVO_STATE_NO_PWM", "LCDM_SERVO_STATE_BAD",
         "LCDM_SERVO_STATE_RANGE_OFF", "LCDM_SERVO_V3_PHASE_IDLE",
         "LCDM_SERVO_RANGE_RELEASE", "LCDM_STARTUP_PARAM_RANGE_MODE",
         "LCDM_SERVO_EVENT_ADC_INVALID", "LCDM_SERVO_EVENT_INPUT_LOST",
         "LCDM_SERVO_EVENT_INPUT_RANGE")
defines = "\n".join(re.search(r"^#define\s+" + name + r"\s+[^\n]+", source, re.M).group()
                    for name in names)
defines += "\nenum { " + re.search(r"LCDM_PWM_RANGE_IN\s*=\s*\d+", source).group() + " };"
stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
static int coast_calls, invalid_calls, publish_calls, pub_event, pub_duty, pub_error, pub_feedback, pub_raw, pub_state;
static int32_t s_lcdm_servo_vel_q4, s_lcdm_servo_pos_q4, s_lcdm_servo_traj_pos_q4, s_lcdm_servo_traj_vel_q4;
static uint32_t s_lcdm_servo_traj_last_update_ms;
static int s_lcdm_servo_current_duty, s_lcdm_servo_current_velocity, s_lcdm_servo_ref_velocity;
static int s_lcdm_servo_hold_active, s_lcdm_servo_dir_bad_count, s_lcdm_servo_dir_alarm;
static int s_lcdm_servo_filter_seeded, s_lcdm_servo_adc_jump_count, s_lcdm_servo_ab_seeded;
static int s_lcdm_servo_hold_deviation_count, s_lcdm_servo_motion_settle_count, s_lcdm_servo_v3_phase;
static int s_lcdm_servo_current_state, s_lcdm_servo_current_range;
static uint8_t s_lcdm_servo_adc_lost_count;
static uint16_t s_lcdm_servo_prev_feedback_adc, s_lcdm_servo_velocity_feedback_adc;
static uint16_t s_lcdm_servo_prev_target_adc, s_lcdm_servo_last_motion_error, s_lcdm_servo_motion_start_error;
static uint16_t s_lcdm_servo_cmd_target_adc = 2052;
static struct { uint32_t tick_ms; } s_lcdm_velocity_observer;
static struct { int value; } s_lcdm_startup_params[1];
typedef struct { uint16_t pulse_us; uint32_t edge_count; } Sample;
static void HBridge_Coast(void) { coast_calls++; }
static void Servo_Adaptive_Update(uint32_t t, int a, int b, bool valid) { (void)t; (void)a; (void)b; if (!valid) invalid_calls++; }
static void lcdm_servo_clear_output_guard(void) {}
static void lcdm_servo_customer_profile_clear(void) {}
static bool lcdm_servo_adc_is_valid(uint16_t raw) { return raw > 30 && raw < 4065; }
static void lcdm_servo_curve_publish(uint16_t pwm, uint16_t raw, uint16_t target, uint16_t feedback, int16_t error, int16_t duty, uint8_t event)
{ (void)pwm; publish_calls++; pub_event=event; pub_duty=duty; pub_error=error; pub_feedback=feedback; pub_raw=raw; pub_state=s_lcdm_servo_current_state; assert(error == (int)target - feedback); }
'''
wrappers = ("\nstatic int adc_guard(uint16_t raw_adc) { uint32_t now_ms=42; Sample sample={1503,1};\n" + adc + "\nreturn 1; }\n" +
            "static int lost_guard(bool fresh, uint32_t edges) { uint32_t now_ms=42; uint16_t raw_adc=1947, feedback=1948; Sample sample={1503,edges};\n" + lost + "\nreturn 1; }\n" +
            "static int range_guard(int range_state) { uint32_t now_ms=42; uint16_t raw_adc=1947, feedback=1948; Sample sample={2501,1};\n" + out_of_range + "\nreturn 1; }\n")
checks = r'''
int main(void) {
  s_lcdm_servo_vel_q4=123; s_lcdm_servo_current_duty=500;
  for (int i=0;i<6;i++) {
    assert(adc_guard(0)==0); assert(pub_event==19 && pub_raw==0 && pub_feedback==0 && pub_error==2052 && pub_duty==0);
    assert(s_lcdm_servo_vel_q4==0 && s_lcdm_servo_current_duty==0);
    assert(pub_state==(i<5 ? LCDM_SERVO_STATE_HOLD : LCDM_SERVO_STATE_ADC_LOST));
  }
  assert(invalid_calls==6 && coast_calls==6 && publish_calls==6);
  assert(adc_guard(4095)==0 && pub_error==-2043 && pub_event==19);
  assert(adc_guard(1947)==1 && publish_calls==7);
  assert(lost_guard(false,0)==0 && pub_event==20 && pub_state==LCDM_SERVO_STATE_NO_PWM && pub_feedback==1948 && pub_error==104 && pub_duty==0);
  assert(lost_guard(false,3)==0 && pub_state==LCDM_SERVO_STATE_BAD && pub_event==20);
  assert(lost_guard(true,3)==1);
  s_lcdm_startup_params[0].value=LCDM_SERVO_RANGE_RELEASE;
  assert(range_guard(1)==0 && pub_event==21 && pub_state==LCDM_SERVO_STATE_RANGE_OFF && pub_error==104 && pub_duty==0);
  assert(range_guard(LCDM_PWM_RANGE_IN)==1);
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="servo_fault_") as folder:
    src = Path(folder) / "fault.c"
    exe = Path(folder) / "fault"
    src.write_text(stub + "\n" + defines + wrappers + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("fault telemetry: ADC low/high, debounce, input lost and range release passed")
