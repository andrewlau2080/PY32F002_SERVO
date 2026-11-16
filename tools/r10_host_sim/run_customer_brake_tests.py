#!/usr/bin/env python3
"""Compile and run the customer-profile speed-scaled brake helper."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("inc/lcdm_customer_brake.h", "src/app_entry.c",
         "tools/r10_host_sim/test_customer_brake.c")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_customer_brake"
    compile_result = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "inc"),
         str(ROOT / "tools/r10_host_sim/test_customer_brake.c"), "-o", str(binary)],
        capture_output=True, text=True)
    (args.output / "compile.log").write_text(compile_result.stdout + compile_result.stderr)
    if compile_result.returncode:
        raise SystemExit("Compile failed; see " + str(args.output / "compile.log"))
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    passed = sum(line.startswith("PASS:") for line in result.stdout.splitlines())
    failed = sum(line.startswith("FAIL:") for line in result.stdout.splitlines())
    report = {"tests_passed": passed, "tests_failed": failed, "exit_code": result.returncode,
              "scope": "Host arithmetic tests only; no target build or motor output.",
              "files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                               for name in FILES}}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    if result.returncode:
        raise SystemExit("One or more tests failed")


if __name__ == "__main__":
    main()
