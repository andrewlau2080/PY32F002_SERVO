#!/usr/bin/env python3
"""Build and run the production LCDM RX-ring boundary checks."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FILES = ("inc/lcdm_rx_ring.h", "tools/r03_host_sim/test_lcdm_rx_ring.c",
         "tools/r03_host_sim/run_lcdm_rx_ring_test.py", "src/app_entry.c")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    binary = args.output / "test_lcdm_rx_ring"
    build = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I",
                           str(ROOT / "inc"), str(ROOT / "tools/r03_host_sim/test_lcdm_rx_ring.c"),
                           "-o", str(binary)], capture_output=True, text=True)
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("LCDM RX-ring host test compile failed")
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (args.output / "results.txt").write_text(result.stdout + result.stderr)
    report = {"files_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                              for name in FILES},
              "pass_count": result.stdout.count("PASS:"),
              "fail_count": result.stdout.count("FAIL:"),
              "exit_code": result.returncode,
              "scope": "Production LCDM RX-ring helper; FIFO, 31-byte capacity, overflow accounting, and wraparound. No UART hardware or touch-panel timing."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("LCDM RX-ring checks:", report["pass_count"], "passed,", report["fail_count"], "failed")
    if result.returncode:
        raise SystemExit("LCDM RX-ring host test failed")

if __name__ == "__main__":
    main()
