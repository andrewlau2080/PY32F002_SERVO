#!/usr/bin/env python3
"""Exercise the actual LCDM HOLD decisions with a latched overshoot.

Fragments are extracted directly from app_entry.c. External functions are
stubbed, so this proves only a narrow control-flow condition, not real motion.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^static[^\n;]*\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{",
                      source, re.M | re.S)
    if not match:
        raise SystemExit("Missing source function " + name)
    pos = source.find("{", match.start())
    depth = 0
    for i in range(pos, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():i + 1]
    raise SystemExit("Unclosed source function " + name)


HEADER = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static int32_t s_lcdm_servo_vel_q4, s_lcdm_servo_traj_pos_q4, s_lcdm_servo_traj_vel_q4;
static int16_t s_lcdm_servo_current_velocity, s_lcdm_servo_ref_velocity, s_lcdm_servo_current_duty;
static uint8_t s_lcdm_servo_current_state, s_lcdm_servo_hold_active, s_lcdm_servo_v3_phase;
static uint8_t s_lcdm_servo_hold_deviation_count;
static int8_t s_lcdm_servo_move_dir, s_lcdm_servo_motion_dir;
static uint16_t s_lcdm_servo_hold_anchor_adc, s_lcdm_servo_last_motion_error;
static uint16_t s_lcdm_servo_motion_settle_count, s_lcdm_servo_last_control_feedback_adc;
static uint16_t s_lcdm_servo_overshoot_adc;
static unsigned hold_calls;
static void HBridge_Brake(void) {}
static void lcdm_servo_clear_output_guard(void) {}
static void lcdm_servo_customer_profile_clear(void) {}
static void lcdm_servo_curve_publish(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
                                     int16_t e, int16_t f, uint8_t g)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; hold_calls++; }
'''

HARNESS = r'''
static bool one_cycle(uint16_t abs_error)
{
  bool command_step = false, static_lock_candidate, static_lock_mode, crossed_target = false;
  uint16_t hold_release_band = 16U, motion_hold_vel_q4 = 28U;
  uint16_t brake_high_vel_q4 = 68U, v7_static_lock_band = 32U, v7_settle_band = 32U;
  uint16_t abs_velocity_q4 = 0U, control_pulse_us = 1500U, raw_adc = 2000U;
  uint16_t s_lcdm_servo_cmd_target_adc = 2100U, feedback = 2000U;
  int16_t error = (int16_t)abs_error;
  STATIC_LOCK_FRAGMENT
  HOLD_FRAGMENT
  return true;
}

int main(void)
{
  unsigned continued = 0U;
  s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_HOLD;
  s_lcdm_servo_overshoot_adc = 100U;
  for (unsigned i = 0; i < 32U; i++) continued += one_cycle(100U);
  printf("overshoot=100, abs_error=100, cycles=32, HOLD calls=%u, cycles past HOLD=%u, deviation_count=%u\n",
         hold_calls, continued, s_lcdm_servo_hold_deviation_count);
  if (continued == 0U) {
    puts("OBSERVED: latched overshoot repeatedly resets the static correction confirmation.");
    return 1;
  }
  puts("OBSERVED: static correction can proceed under these modeled conditions.");
  return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "src/app_entry.c")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.read_text()
    helper = function(source, "lcdm_servo_enter_hold")
    loop = function(source, "lcdm_servo_loop_update_fast")
    macros = []
    for name in ("LCDM_SERVO_STATE_HOLD", "LCDM_SERVO_V3_PHASE_HOLD",
                 "LCDM_SERVO_V3_PHASE_LOCK", "LCDM_SERVO_V3_OVERSHOOT_HOLD_BAND"):
        match = re.search(r"^#define\s+" + name + r"\b[^\n]*", source, re.M)
        if not match:
            raise SystemExit("Missing macro " + name)
        macros.append(match.group(0))
    start = loop.index("  static_lock_candidate = (!command_step)")
    end = loop.index("\n\n  duty_calc = 0;", start)
    static_fragment = loop[start:end]
    start = loop.index("  if ((!command_step) &&\n      (abs_error <= hold_release_band)")
    end = loop.index("\n\n  if (s_lcdm_servo_customer_active != 0U)", start)
    hold_fragment = loop[start:end].replace("return 0;", "return false;")
    args.output.mkdir(parents=True, exist_ok=False)
    generated = args.output / "hold_latch_harness.c"
    generated.write_text(HEADER + "\n" + "\n".join(macros) + "\n" + helper + "\n" +
                         HARNESS.replace("STATIC_LOCK_FRAGMENT", static_fragment)
                         .replace("HOLD_FRAGMENT", hold_fragment) + "\n")
    binary = args.output / "hold_latch_harness"
    build = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(generated), "-o", str(binary)],
                           capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("Harness compilation failed; see " + str(args.output / "compile.log"))
    run = subprocess.run([str(binary.resolve())], capture_output=True, text=True)
    (args.output / "results.txt").write_text(run.stdout + run.stderr)
    report = {"source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
              "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "exit_code": run.returncode, "observed_stuck_in_hold": run.returncode == 1,
              "scope": "Extracted LCDM HOLD decision and actual hold helper, with no ADC, PWM or mechanical model."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(run.stdout, end="")
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == "__main__":
    main()
