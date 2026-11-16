#!/usr/bin/env python3
"""Compile production model-store code against a fault-injectable host Flash HAL."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("inc/servo_model_store.h", "src/servo_model_store.c",
         "tools/r13_host_sim/fake_hal/py32f0xx_hal.h",
         "tools/r13_host_sim/test_model_store.c",
         "tools/r13_host_sim/run_model_store_test.py")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_model_store"
    command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-DSERVO_MODEL_FLASH_PTR=Fake_ModelFlashPtr",
               "-I", str(ROOT / "tools/r13_host_sim/fake_hal"), "-I", str(ROOT / "inc"),
               str(ROOT / "tools/r13_host_sim/test_model_store.c"),
               str(ROOT / "src/servo_model_store.c"), "-o", str(binary)]
    build = subprocess.run(command, capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("Model-store host test compile failed")
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                               for name in FILES},
              "pass_count": result.stdout.count("PASS:"),
              "fail_count": result.stdout.count("FAIL:"),
              "exit_code": result.returncode,
              "scope": "Production dual-slot model store with mock Flash; CRC fallback, generations, context match/rejection, save identity/quality guards, qualification false, erase/program failures, and IRQ balance. No real Flash, power-loss timing, or production save scheduler."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Model-store host checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if result.returncode:
        raise SystemExit("Model-store host test failed")


if __name__ == "__main__":
    main()
