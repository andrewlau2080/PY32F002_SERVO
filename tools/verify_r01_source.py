#!/usr/bin/env python3
"""Compile the actual V10 LCDM mapping/latch functions in an R01 host harness.

The harness extracts production C functions and default parameters from the
selected app_entry.c. It verifies source-level behavior only; it does not
simulate timer capture, the physical motor, noise, or mechanical angles.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
MACROS = (
    "LCDM_SERVO_INPUT_MID_US", "LCDM_SERVO_WORK_LOW_MV", "LCDM_SERVO_WORK_MID_MV",
    "LCDM_SERVO_WORK_HIGH_MV", "LCDM_SERVO_TARGET_ADC_VDD_MV",
    "LCDM_SERVO_RANGE_RELEASE", "LCDM_SERVO_RANGE_LOCK",
    "LCDM_STARTUP_PARAM_INPUT_STABLE_BAND", "LCDM_SERVO_HOLD_RELEASE_BAND",
    "LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4",
)
GLOBALS = (
    "s_lcdm_servo_latched_pulse_us", "s_lcdm_servo_candidate_pulse_us",
    "s_lcdm_servo_last_processed_pulse_count", "s_lcdm_servo_latched_pulse_seeded",
    "s_lcdm_servo_candidate_count",
)
FUNCTIONS = (
    "lcdm_clamp_u16", "lcdm_mv_to_adc", "lcdm_pwm_range_state",
    "lcdm_servo_update_latched_pulse", "lcdm_pwm_to_target_mv", "lcdm_pwm_to_target_adc",
)


def find_one(pattern, source, label):
    match = re.search(pattern, source, re.MULTILINE | re.DOTALL)
    if not match:
        raise SystemExit("Could not locate source fragment: " + label)
    return match.group(0)


def brace_fragment(source, start, label):
    opening = source.find("{", start)
    if opening < 0:
        raise SystemExit("Missing opening brace: " + label)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise SystemExit("Unclosed brace: " + label)


def function(source, name):
    pattern = r"^static[^\n;]*\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{"
    match = re.search(pattern, source, re.MULTILINE | re.DOTALL)
    if not match:
        raise SystemExit("Missing production function: " + name)
    return brace_fragment(source, match.start(), name)


HARNESS = r'''
static unsigned failures;
static void check(const char *name, int actual, int expected)
{
  printf("%s: %s (actual=%d expected=%d)\n",
         (actual == expected) ? "PASS" : "FAIL", name, actual, expected);
  if (actual != expected) { failures++; }
}

/* Stub observes whether the exact production HOLD condition is entered. */
static void lcdm_servo_enter_hold(uint16_t p, uint16_t raw, uint16_t target,
                                  uint16_t feedback, int16_t error, uint8_t event)
{
  (void)p; (void)raw; (void)target; (void)feedback; (void)error; (void)event;
}

static int hold_condition(uint16_t pulse_delta, uint16_t abs_error, uint16_t abs_velocity_q4,
                          bool hold_confirmed)
{
  bool target_motion_changed = MOTION_EXPRESSION;
  bool command_step = target_motion_changed;
  bool hold_entry_confirmed = hold_confirmed;
  uint16_t hold_release_band = LCDM_SERVO_HOLD_RELEASE_BAND;
  uint16_t motion_hold_vel_q4 = LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4;
  uint16_t control_pulse_us = 0U, raw_adc = 0U, s_lcdm_servo_cmd_target_adc = 0U, feedback = 0U;
  int16_t error = (int16_t)abs_error;
  HOLD_BRANCH
  return 1;
}

int main(void)
{
  struct test_case { uint16_t pulse; uint16_t adc; } cases[] = {
    {500, 310}, {600, 310}, {1000, 1082}, {1497, 2041}, {1498, 2044},
    {1499, 2045}, {1500, 2048}, {1501, 2050}, {1502, 2051},
    {1503, 2054}, {2000, 3013}, {2400, 3785}, {2500, 3785}
  };
  char label[64];
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    snprintf(label, sizeof(label), "target mapping %u us", cases[i].pulse);
    check(label, lcdm_pwm_to_target_adc(cases[i].pulse), cases[i].adc);
  }
  check("599 us is below range", lcdm_pwm_range_state(599), LCDM_PWM_RANGE_LOW);
  check("600 us is in range", lcdm_pwm_range_state(600), LCDM_PWM_RANGE_IN);
  check("2400 us is in range", lcdm_pwm_range_state(2400), LCDM_PWM_RANGE_IN);
  check("2401 us is above range", lcdm_pwm_range_state(2401), LCDM_PWM_RANGE_HIGH);
  check("default input stable band", s_lcdm_startup_params[LCDM_STARTUP_PARAM_INPUT_STABLE_BAND].value, 0);
  s_lcdm_servo_latched_pulse_seeded = 0U;
  check("initial latch 1500 us", lcdm_servo_update_latched_pulse(1500, 1), 1500);
  check("same frame is ignored", lcdm_servo_update_latched_pulse(1501, 1), 1500);
  check("new 1501 us frame updates latch", lcdm_servo_update_latched_pulse(1501, 2), 1501);
  check("new 1502 us frame updates latch", lcdm_servo_update_latched_pulse(1502, 3), 1502);
  check("new 1503 us frame updates latch", lcdm_servo_update_latched_pulse(1503, 4), 1503);
  check("new 1499 us frame updates latch", lcdm_servo_update_latched_pulse(1499, 5), 1499);
  check("+1 us waits for stable-time gate", hold_condition(1, 1, 0, false), 1);
  check("+1 us enters HOLD after stable-time gate", hold_condition(1, 1, 0, true), 0);
  check("+2 us waits for stable-time gate", hold_condition(2, 3, 0, false), 1);
  check("+2 us enters HOLD after stable-time gate", hold_condition(2, 3, 0, true), 0);
  check("+3 us command step skips this HOLD branch", hold_condition(3, 4, 0, true), 1);
  printf("HOLD branch checks describe software control flow, not physical motion.\n");
  return failures ? 1 : 0;
}
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path, default=ROOT / "src/app_entry.c")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--expected-sha256")
    args = p.parse_args()
    source_bytes = args.source.read_bytes()
    source_hash = hashlib.sha256(source_bytes).hexdigest()
    if args.expected_sha256 and source_hash != args.expected_sha256:
        raise SystemExit("Source hash does not match the selected firmware build")
    source = source_bytes.decode()
    fragments = ["#include <stdint.h>", "#include <stdbool.h>", "#include <stdio.h>"]
    for name in MACROS:
        fragments.append(find_one(r"^#define\s+" + re.escape(name) + r"\b[^\n]*", source, name))
    fragments.append(find_one(r"typedef\s+struct\s*\{[^{}]*\}\s*LcdmRuntimeParam\s*;", source, "LcdmRuntimeParam"))
    fragments.append(find_one(r"typedef\s+enum\s*\{[^{}]*\}\s*LcdmPwmRangeState\s*;", source, "LcdmPwmRangeState"))
    for name in ("s_lcdm_cal_params", "s_lcdm_startup_params"):
        fragments.append(find_one(r"^static\s+LcdmRuntimeParam\s+" + name + r"\[\]\s*=\s*\{.*?^\};", source, name))
    for name in GLOBALS:
        fragments.append(find_one(r"^static\s+[^;\n]*\b" + name + r"\b[^;\n]*;", source, name))
    fragments.extend(function(source, name) for name in FUNCTIONS)
    expression = find_one(r"target_motion_changed\s*=\s*\(pulse_delta\s*>=\s*3U\)\s*;", source, "motion threshold")
    expression = expression.split("=", 1)[1].strip().rstrip(";")
    find_one(r"command_step\s*=\s*target_motion_changed\s*;", source, "command_step assignment")
    branch = re.search(
        r"^\s*if\s*\(\(!command_step\)\s*&&\s*hold_entry_confirmed"
        r"\s*&&\s*\(abs_error\s*<=\s*hold_release_band\)"
        r"\s*&&\s*\(abs_velocity_q4\s*<=\s*motion_hold_vel_q4\)\s*\)",
        source, re.MULTILINE | re.DOTALL,
    )
    if not branch:
        raise SystemExit("Could not locate the production small-command HOLD branch")
    hold_branch = brace_fragment(source, branch.start(), "small-command HOLD branch")
    fragments.append(HARNESS.replace("MOTION_EXPRESSION", expression).replace("HOLD_BRANCH", hold_branch))
    args.output.mkdir(parents=True, exist_ok=False)
    generated = args.output / "r01_source_harness.c"
    generated.write_text("\n\n".join(fragments) + "\n")
    binary = args.output / "r01_source_harness"
    compile_cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(generated), "-o", str(binary)]
    build = subprocess.run(compile_cmd, text=True, capture_output=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("Host harness compilation failed; inspect " + str(args.output / "compile.log"))
    result = subprocess.run([str(binary.resolve())], text=True, capture_output=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {
        "source": str(args.source), "source_sha256": source_hash,
        "generated_c_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
        "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "pass_count": sum(line.startswith("PASS:") for line in result.stdout.splitlines()),
        "fail_count": sum(line.startswith("FAIL:") for line in result.stdout.splitlines()),
        "exit_code": result.returncode,
        "scope": "Extracted production LCDM source functions and HOLD branch; no real timer, motor or mechanics.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(result.stdout, end="")
    if result.returncode:
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
