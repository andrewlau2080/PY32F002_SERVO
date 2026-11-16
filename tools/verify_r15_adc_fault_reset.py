#!/usr/bin/env python3
"""Verify ADC and low-VDD fail-safe paths in TSSOP20 control sources."""
import hashlib
import json
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/app_entry.c"
source = SOURCE.read_text()
CONTROL_SOURCE = ROOT / "src/servo_control.c"
control_source = CONTROL_SOURCE.read_text()
DISPLAY_SOURCE = ROOT / "src/tjc_lcdm.c"
display_source = DISPLAY_SOURCE.read_text()
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

generic_marker = "void Servo_Control_Update1ms(void)"
generic_start = control_source.find(generic_marker)
if generic_start < 0:
    raise SystemExit("generic controller update function not found")
status_start = control_source.find("ServoStatus Servo_Control_GetStatus(void)", generic_start)
if status_start < 0:
    raise SystemExit("generic controller update function boundary not found")
generic_block = control_source[generic_start:status_start]
generic_vdd_start = generic_block.find(vdd_marker)
if generic_vdd_start < 0:
    raise SystemExit("generic TSSOP20 VDD-low guard not found")
generic_open = generic_block.find("{", generic_vdd_start)
generic_depth = 0
generic_end = None
for index in range(generic_open, len(generic_block)):
    if generic_block[index] == "{":
        generic_depth += 1
    elif generic_block[index] == "}":
        generic_depth -= 1
        if generic_depth == 0:
            generic_end = index + 1
            break
if generic_end is None:
    raise SystemExit("generic VDD-low guard block is not balanced")
generic_vdd_block = generic_block[generic_open:generic_end]
generic_if = generic_block.rfind("#if BOARD_TSSOP20_DEBUG_PINS", 0, generic_vdd_start)
checks.update({
    "production generic VDD guard is TSSOP20-only":
        generic_if >= 0 and "#endif" in generic_block[generic_end:generic_end + 32],
    "generic low VDD resets learner and coasts":
        "Servo_Adaptive_Update(now_ms, 0, 0, false);" in generic_vdd_block and
        "HBridge_Coast();" in generic_vdd_block,
    "generic controller reports VDD_LOW with zero duty":
        "SERVO_STATE_VDD_LOW" in generic_vdd_block and
        "s_status.motor_duty = 0;" in generic_vdd_block,
    "generic low VDD exits before drive continuation":
        "return;" in generic_vdd_block,
    "TJC LCDM displays the VDD-low state":
        'case 4U:\n      return "VDD LOW";' in display_source,
})
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + ": " + name)
report = {"checks": checks, "pass_count": sum(checks.values()),
          "fail_count": sum(not value for value in checks.values()),
          "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
          "generic_control_source_sha256": hashlib.sha256(CONTROL_SOURCE.read_bytes()).hexdigest(),
          "display_source_sha256": hashlib.sha256(DISPLAY_SOURCE.read_bytes()).hexdigest(),
          "scope": "TSSOP20 LCDM and production generic source paths; no ADC hardware fault injection or output waveform measurement."}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path,
                    default=ROOT / "firmware/evidence/r15_adc_fault_20261116/report.json")
out = parser.parse_args().output
if not out.is_absolute():
    out = ROOT / out
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2) + "\n")
print("R15 ADC fault-path checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
raise SystemExit(1 if report["fail_count"] else 0)
