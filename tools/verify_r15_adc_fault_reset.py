#!/usr/bin/env python3
"""Verify that the LCDM ADC fault path coasts and resets the learner seed."""
import hashlib
import json
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/app_entry.c"
source = SOURCE.read_text()
marker = "if (!lcdm_servo_adc_is_valid(raw_adc))"
start = source.find(marker)
if start < 0:
    raise SystemExit("ADC validity guard not found")
open_brace = source.find("{", start)
depth = 0
end = None
for index in range(open_brace, len(source)):
    if source[index] == "{":
        depth += 1
    elif source[index] == "}":
        depth -= 1
        if depth == 0:
            end = index + 1
            break
if end is None:
    raise SystemExit("ADC validity block is not balanced")
block = source[open_brace:end]
checks = {
    "invalid ADC coasts H-bridge": "HBridge_Coast();" in block,
    "invalid ADC invalidates learner sample seed": "Servo_Adaptive_Update(now_ms, 0, 0, false);" in block,
    "invalid ADC resets LCDM observer seed": "s_lcdm_servo_ab_seeded = 0U;" in block,
    "invalid ADC returns before closed-loop continuation": "return 0;" in block,
}
vdd_marker = "if (ADC_Feedback_IsVddBelow("
vdd_start = source.find(vdd_marker)
if vdd_start < 0:
    raise SystemExit("TSSOP20 VDD-low guard not found")
vdd_open = source.find("{", vdd_start)
vdd_depth = 0
vdd_end = None
for index in range(vdd_open, len(source)):
    if source[index] == "{":
        vdd_depth += 1
    elif source[index] == "}":
        vdd_depth -= 1
        if vdd_depth == 0:
            vdd_end = index + 1
            break
if vdd_end is None:
    raise SystemExit("TSSOP20 VDD-low guard block is not balanced")
vdd_block = source[vdd_open:vdd_end]
vdd_if = source.rfind("#if BOARD_TSSOP20_DEBUG_PINS", 0, vdd_start)
checks.update({
    "undervoltage coast guard is TSSOP20-only": vdd_if >= 0 and "#endif" in source[vdd_end:vdd_end + 32],
    "low VDD resets learner sample seed": "Servo_Adaptive_Update(now_ms, 0, 0, false);" in vdd_block,
    "low VDD publishes dedicated fault state and event":
        "LCDM_SERVO_STATE_VDD_LOW" in vdd_block and "LCDM_SERVO_EVENT_VDD_LOW" in vdd_block,
    "low VDD fault coasts H-bridge":
        "LCDM_SERVO_EVENT_VDD_LOW, false" in vdd_block,
})
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + ": " + name)
report = {"checks": checks, "pass_count": sum(checks.values()),
          "fail_count": sum(not value for value in checks.values()),
          "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
          "scope": "LCDM source-path guard only; no ADC hardware fault injection or output waveform measurement."}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path,
                    default=ROOT / "firmware/evidence/r15_adc_fault_20261113/report.json")
out = parser.parse_args().output
if not out.is_absolute():
    out = ROOT / out
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2) + "\n")
print("R15 ADC fault-path checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
raise SystemExit(1 if report["fail_count"] else 0)
