#!/usr/bin/env python3
"""Exercise production adaptive-learning gates with injected valid/invalid samples."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("src/servo_adaptive.c", "inc/servo_adaptive.h", "inc/servo_model_store.h",
         "tools/r05_host_sim/test_adaptive_quality.c",
         "tools/r05_host_sim/run_adaptive_quality_tests.py")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_adaptive_quality"
    command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I", str(ROOT / "inc"),
               str(ROOT / "tools/r05_host_sim/test_adaptive_quality.c"),
               str(ROOT / "src/servo_adaptive.c"), "-o", str(binary)]
    build = subprocess.run(command, capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("Adaptive quality test failed to compile; see " + str(args.output / "compile.log"))
    result = subprocess.run([str(binary.resolve())], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                               for name in FILES},
              "pass_count": result.stdout.count("PASS:"),
              "fail_count": result.stdout.count("FAIL:"),
              "exit_code": result.returncode,
              "scope": "Production adaptive learner with injected duty, velocity, time and validity; no real bridge, ADC or mechanism."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Adaptive quality checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if result.returncode:
        raise SystemExit("Adaptive quality checks failed")


if __name__ == "__main__":
    main()
