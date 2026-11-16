#!/usr/bin/env python3
"""Build and run host checks for the LCDM learned brake predictor."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("inc/lcdm_brake_predictor.h", "tools/r09_host_sim/test_lcdm_brake_predictor.c",
         "tools/r09_host_sim/run_lcdm_brake_predictor_test.py", "inc/servo_adaptive.h",
         "src/servo_adaptive.c", "src/app_entry.c")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_lcdm_brake_predictor"
    build = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I",
                            str(ROOT / "inc"), str(ROOT / "tools/r09_host_sim/test_lcdm_brake_predictor.c"),
                            "-o", str(binary)], capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("LCDM brake predictor host test compile failed")
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                               for name in FILES},
              "pass_count": result.stdout.count("PASS:"),
              "fail_count": result.stdout.count("FAIL:"),
              "exit_code": result.returncode,
              "scope": "Production LCDM braking-distance arithmetic; sample qualification, Q4/Q8 conversion, rounding, latency margin, and monotonic response. No motor, ADC, or physical braking evidence."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("LCDM brake predictor checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if result.returncode:
        raise SystemExit("LCDM brake predictor host test failed")


if __name__ == "__main__":
    main()
