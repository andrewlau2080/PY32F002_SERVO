#!/usr/bin/env python3
"""Build and run the production input ISR against four host pin profiles."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PROFILES = {
    "development_default": ["-DBOARD_TSSOP20_DEBUG_PINS=1", "-DR01_EXPECT_CHANNEL=1"],
    "sop8_default": ["-DBOARD_TSSOP20_DEBUG_PINS=0", "-DR01_EXPECT_CHANNEL=3"],
    "development_override": ["-DBOARD_TSSOP20_DEBUG_PINS=0", "-DBOARD_SOP8_PINOUT=0", "-DR01_EXPECT_CHANNEL=1"],
    "sop8_override": ["-DBOARD_TSSOP20_DEBUG_PINS=1", "-DBOARD_SOP8_PINOUT=1", "-DR01_EXPECT_CHANNEL=3"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    reports = {}
    for name, flags in PROFILES.items():
        out = args.output / name
        out.mkdir()
        binary = out / "test_pwm_capture"
        cmd = [
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
            "-I", str(ROOT / "inc"), "-I", str(ROOT / "tools/r01_host_sim"),
            str(ROOT / "tools/r01_host_sim/test_pwm_capture.c"), str(ROOT / "src/pwm_input.c"),
            "-o", str(binary),
        ]
        build = subprocess.run(cmd, text=True, capture_output=True)
        (out / "compile.log").write_text(build.stdout + build.stderr)
        if build.returncode:
            raise SystemExit("Build failed for " + name + "; see " + str(out / "compile.log"))
        result = subprocess.run([str(binary.resolve())], text=True, capture_output=True)
        (out / "results.txt").write_text(result.stdout + result.stderr)
        reports[name] = {
            "flags": flags,
            "pass_count": sum(line.startswith("PASS:") for line in result.stdout.splitlines()),
            "fail_count": sum(line.startswith("FAIL:") for line in result.stdout.splitlines()),
            "exit_code": result.returncode,
        }
        print(name, reports[name])
    files = ("inc/board.h", "src/pwm_input.c", "tools/r01_host_sim/test_pwm_capture.c",
             "tools/r01_host_sim/py32f0xx_hal.h")
    report = {
        "files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in files},
        "profiles": reports,
        "scope": "Actual board header and input ISR with simulated timer/GPIO; no physical device.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if any(item["exit_code"] for item in reports.values()):
        raise SystemExit("One or more pin-profile tests failed")


if __name__ == "__main__":
    main()
