#!/usr/bin/env python3
"""Check production LCDM HOLD timing and overshoot source invariants."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "src/app_entry.c")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.read_text()
    match = re.search(r"^static[^\n;]*\blcdm_servo_loop_update_fast\s*\([^;]*?\)\s*\{",
                      source, re.M | re.S)
    if not match:
        raise SystemExit("Missing production LCDM fast-loop function")
    opening = source.find("{", match.start())
    depth = 0
    end = None
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                end = index + 1
                break
    if end is None:
        raise SystemExit("Unclosed production LCDM fast-loop function")
    loop = source[match.start():end]
    checks = {
        "static-lock offset needs HOLD/LOCK phase and error above band":
            "static_lock_candidate = (!command_step)" in loop and
            "(abs_error > v7_static_lock_band)" in loop,
        "offset confirmation uses 8 ms timer with 4 ms maximum sample gap":
            "s_lcdm_servo_hold_deviation_timer" in loop and
            "true, stable_elapsed_ms, 8U, 4U" in loop,
        "HOLD entry confirmation uses elapsed time":
            "s_lcdm_servo_hold_entry_timer" in loop and
            "hold_entry_confirmed = ServoStabilityTimer_Update" in loop and
            "stable_elapsed_ms, 8U, 4U" in loop,
        "overshoot HOLD branch requires confirmed stability":
            re.search(r"if\s*\(\(!command_step\)\s*&&\s*hold_entry_confirmed"
                      r"(?:(?!\n\s*\}).)*s_lcdm_servo_overshoot_adc", loop, re.S) is not None,
        "overshoot HOLD preserves offset-confirmation timer":
            "ServoStabilityTimer hold_deviation_timer = s_lcdm_servo_hold_deviation_timer;" in loop and
            "s_lcdm_servo_hold_deviation_timer = hold_deviation_timer;" in loop,
    }
    for name, passed in checks.items():
        print(("PASS" if passed else "FAIL") + ": " + name)
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "results.txt").write_text("\n".join(
        ("PASS" if passed else "FAIL") + ": " + name for name, passed in checks.items()) + "\n")
    report = {"source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
              "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "pass_count": sum(checks.values()), "fail_count": sum(not value for value in checks.values()),
              "exit_code": 0 if all(checks.values()) else 1,
              "scope": "Production-source invariant checks for elapsed HOLD/offset timing and overshoot timer retention; no real timer, ADC, PWM or mechanics."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("R01 current HOLD source checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if report["exit_code"]:
        raise SystemExit(report["exit_code"])


if __name__ == "__main__":
    main()
