#!/usr/bin/env python3
"""Test bounded adaptation to injected changes in equivalent object response."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("src/servo_adaptive.c", "inc/servo_adaptive.h", "inc/servo_model_store.h",
         "tools/r05_host_sim/test_object_response.c",
         "tools/r05_host_sim/run_object_response_tests.py")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_object_response"
    command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I", str(ROOT / "inc"),
               str(ROOT / "tools/r05_host_sim/test_object_response.c"),
               str(ROOT / "src/servo_adaptive.c"), "-o", str(binary)]
    build = subprocess.run(command, capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("Object-response test failed to compile; see " + str(args.output / "compile.log"))
    result = subprocess.run([str(binary.resolve())], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                               for name in FILES},
              "pass_count": result.stdout.count("PASS:"),
              "fail_count": result.stdout.count("FAIL:"),
              "exit_code": result.returncode,
              "scope": "Production adaptive module with synthetic speed response at fixed duty; no physical voltage/load/bridge/mechanism."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Equivalent-object response checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if result.returncode:
        raise SystemExit("Object-response checks failed")


if __name__ == "__main__":
    main()
