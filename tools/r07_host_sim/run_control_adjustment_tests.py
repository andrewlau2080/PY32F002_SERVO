#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
from datetime import datetime, timezone

root = pathlib.Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description="Run R07 adaptive control adjustment host checks.")
parser.add_argument("--output", type=pathlib.Path,
                    default=root / "firmware/evidence/r07_control_adjustment_20261111")
evidence = parser.parse_args().output
evidence.mkdir(parents=True, exist_ok=False)
source = root / "tools/r07_host_sim/test_control_adjustment.c"
adaptive = root / "src/servo_adaptive.c"
binary = evidence / "r07_control_adjustment"
compile_cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root / "inc"),
               str(source), str(adaptive), "-o", str(binary)]
compiled = subprocess.run(compile_cmd, text=True, capture_output=True)
(evidence / "compile.log").write_text(compiled.stdout + compiled.stderr)
if compiled.returncode:
    print(compiled.stdout, end="")
    print(compiled.stderr, end="", file=sys.stderr)
    raise SystemExit(compiled.returncode)
run = subprocess.run([str(binary)], text=True, capture_output=True)
(evidence / "results.txt").write_text(run.stdout + run.stderr)
passed = sum(line.startswith("PASS:") for line in run.stdout.splitlines())
failed = sum(line.startswith("FAIL:") for line in run.stdout.splitlines())
inputs = [source, adaptive, root / "inc/servo_adaptive.h", root / "src/app_entry.c"]
hashes = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
report = {
    "requirement": "R07",
    "created_utc": datetime.now(timezone.utc).isoformat(),
    "compile_command": compile_cmd,
    "test_command": [str(binary)],
    "passed": passed,
    "failed": failed,
    "return_code": run.returncode,
    "source_sha256": hashes,
    "scope": "Production adaptive gain/feedforward helpers and one-step EMA/confidence update with synthetic data; no board or physical plant.",
}
(evidence / "report.json").write_text(json.dumps(report, indent=2) + "\n")
print(run.stdout, end="")
print(f"R07 control adjustment checks: {passed} passed, {failed} failed")
raise SystemExit(run.returncode)
