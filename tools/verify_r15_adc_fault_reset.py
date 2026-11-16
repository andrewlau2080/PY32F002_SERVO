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
