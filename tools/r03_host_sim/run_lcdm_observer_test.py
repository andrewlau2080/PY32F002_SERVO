#!/usr/bin/env python3
"""Build and run the LCDM velocity observer host checks."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("inc/lcdm_velocity_observer.h", "tools/r03_host_sim/test_lcdm_observer.c",
         "tools/r03_host_sim/run_lcdm_observer_test.py")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_lcdm_observer"
    build = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I",
                           str(ROOT / "inc"), str(ROOT / "tools/r03_host_sim/test_lcdm_observer.c"),
                           "-o", str(binary)], capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("LCDM observer host test compile failed")
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                              for name in FILES},
              "pass_count": result.stdout.count("PASS:"),
              "fail_count": result.stdout.count("FAIL:"),
              "exit_code": result.returncode,
              "scope": "Production LCDM observer helper with injected ADC and HAL ticks; no motor or ADC hardware."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("LCDM observer checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if result.returncode:
        raise SystemExit("LCDM observer host test failed")

if __name__ == "__main__":
    main()
