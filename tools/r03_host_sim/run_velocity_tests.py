#!/usr/bin/env python3
"""Test generic control velocity passed to learning using explicit timestamps."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("src/servo_control.c", "tools/r03_host_sim/test_velocity.c", "tools/r03_host_sim/run_velocity_tests.py")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_velocity"
    cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
           "-I", str(ROOT / "tools/r02_host_sim"), "-I", str(ROOT / "inc"),
           str(ROOT / "tools/r03_host_sim/test_velocity.c"), str(ROOT / "src/servo_control.c"),
           "-o", str(binary)]
    build = subprocess.run(cmd, capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("Velocity harness failed to compile; see " + str(args.output / "compile.log"))
    result = subprocess.run([str(binary.resolve())], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in FILES},
              "pass_count": result.stdout.count("PASS:"), "fail_count": result.stdout.count("FAIL:"),
              "exit_code": result.returncode,
              "scope": "Generic production control, injected position/time, captured learning input; no motor or LCDM."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Velocity checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if result.returncode:
        raise SystemExit("Velocity checks failed")

if __name__ == "__main__":
    main()
