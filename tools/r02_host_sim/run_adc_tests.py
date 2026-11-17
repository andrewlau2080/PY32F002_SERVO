#!/usr/bin/env python3
"""Run the production ADC feedback implementation with a simulated ADC HAL."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("src/adc_feedback.c", "inc/board.h", "tools/r02_host_sim/test_adc_feedback.c",
         "tools/r02_host_sim/py32f0xx_hal.h", "tools/r02_host_sim/run_adc_tests.py")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--diag-vdd-each-batch", action="store_true",
                        help="force a settled VREFINT conversion before each diagnostic 4-sample batch")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    profiles = {}
    for name, debug in (("development", "1"), ("sop8", "0")):
        out = args.output / name
        out.mkdir()
        binary = out / "test_adc"
        extra = ["-DSERVO_ADC_VDD_EACH_DIAG_BATCH=1"] if args.diag_vdd_each_batch else []
        cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-DBOARD_TSSOP20_DEBUG_PINS=" + debug,
               "-DSERVO_VREFINT_SETTLE_FIX=1",
               "-DSERVO_ADC_SUBSAMPLE_DIAGNOSTIC=1", *extra,
               "-I", str(ROOT / "tools/r02_host_sim"), "-I", str(ROOT / "inc"),
               str(ROOT / "tools/r02_host_sim/test_adc_feedback.c"),
               str(ROOT / "src/adc_feedback.c"), "-o", str(binary)]
        build = subprocess.run(cmd, capture_output=True, text=True)
        (out / "compile.log").write_text(build.stdout + build.stderr)
        if build.returncode:
            raise SystemExit("ADC harness failed to compile; see " + str(out / "compile.log"))
        result = subprocess.run([str(binary.resolve())], capture_output=True, text=True)
        (out / "results.txt").write_text(result.stdout + result.stderr)
        profiles[name] = {"pass_count": result.stdout.count("PASS:"),
                          "fail_count": result.stdout.count("FAIL:"),
                          "exit_code": result.returncode}
        print(name, profiles[name])
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                               for name in FILES},
              "profiles": profiles,
              "diag_vdd_each_batch": args.diag_vdd_each_batch,
              "scope": "Production ADC source with simulated channel conversions, no physical ADC or motor."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if any(p["exit_code"] for p in profiles.values()):
        raise SystemExit("One or more ADC checks failed")

if __name__ == "__main__":
    main()
