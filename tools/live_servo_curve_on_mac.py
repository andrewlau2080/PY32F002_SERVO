#!/usr/bin/env python3
"""Mac host live UI for SWD servo curve sampling through Parallels Ubuntu."""

from __future__ import annotations

import argparse
import csv
import os
import shlex
import shutil
import subprocess
import sys
import time
from collections import deque
from pathlib import Path

from live_servo_curve_swd import (
    CURVE_VARS,
    HBridgeSnapshot,
    write_cycle_csv,
    write_hbridge_html,
    write_hbridge_response_svg,
    write_html,
    write_svg,
)


PRLCTL = os.environ.get("PRLCTL") or shutil.which("prlctl") or "/usr/local/bin/prlctl"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Open a Mac HTML live curve fed by Ubuntu pyOCD/SWD.")
    parser.add_argument("--vm", default="Ubuntu Linux")
    parser.add_argument("--remote-dir", default="/tmp/servo_curve_swd_live")
    parser.add_argument("--interval-ms", type=int, default=5)
    parser.add_argument("--frequency", type=int, default=1_000_000)
    parser.add_argument("--window", type=int, default=1200)
    parser.add_argument("--output-dir", default="build/servo_curve_live")
    parser.add_argument("--target", default="py32f002ax5")
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--open-html", action="store_true", help="Open the live HTML once when the sampler starts.")
    return parser.parse_args()


def project_dir() -> Path:
    return Path(__file__).resolve().parents[1]


def upload_runtime(args: argparse.Namespace, project: Path) -> None:
    remote = shlex.quote(args.remote_dir)
    vm = shlex.quote(args.vm)
    local = shlex.quote(str(project))
    command = (
        f"tar -C {local} -cf - tools/live_servo_curve_swd.py build/py32f002_servo.elf "
        f"| {shlex.quote(PRLCTL)} exec {vm} \"rm -rf {remote} && mkdir -p {remote} && tar -C {remote} -xf -\""
    )
    subprocess.run(command, shell=True, check=True)


def start_remote_sampler(args: argparse.Namespace) -> subprocess.Popen[str]:
    remote = shlex.quote(args.remote_dir)
    cmd = (
        f"cd {remote} && PYTHONUNBUFFERED=1 /opt/py32-venv/bin/python "
        f"tools/live_servo_curve_swd.py "
        f"--elf build/py32f002_servo.elf "
        f"--target {shlex.quote(args.target)} "
        f"--frequency {args.frequency} "
        f"--interval-ms {args.interval_ms} "
        f"--stream-csv"
    )
    if args.demo:
        cmd += " --demo"
    return subprocess.Popen(
        [PRLCTL, "exec", args.vm, cmd],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )


def reset_remote_probe(args: argparse.Namespace) -> None:
    cmd = (
        "pkill -f '[l]ive_servo_curve_swd.py' >/dev/null 2>&1 || true; "
        "pkill -f '[p]yocd' >/dev/null 2>&1 || true; "
        "if command -v usbreset >/dev/null 2>&1; then "
        "  usbreset 0d28:0204 >/dev/null 2>&1 || true; "
        "fi; "
        "sleep 1"
    )
    subprocess.run([PRLCTL, "exec", args.vm, cmd], check=False)


def as_int_row(headers: list[str], line: str) -> dict[str, int] | None:
    try:
        values = next(csv.reader([line]))
    except csv.Error:
        return None
    if len(values) != len(headers):
        return None
    row: dict[str, int] = {}
    for key, value in zip(headers, values):
        try:
            row[key] = int(float(value))
        except ValueError:
            return None
    return row


def open_html(path: Path) -> None:
    subprocess.run(["open", str(path)], check=False)


def main() -> int:
    args = parse_args()
    project = project_dir()
    output_dir = project / args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    cycle_dir = output_dir / "cycles"
    cycle_dir.mkdir(exist_ok=True)

    csv_path = output_dir / "servo_curve_live.csv"
    svg_path = output_dir / "servo_curve_live.svg"
    html_path = output_dir / "servo_curve_live.html"
    hbridge_svg_path = output_dir / "hbridge_response.svg"
    hbridge_html_path = output_dir / "hbridge_response.html"
    fieldnames = [label for label, _symbol, _kind in CURVE_VARS]

    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        csv.DictWriter(handle, fieldnames=fieldnames).writeheader()
    write_html(html_path, svg_path.name, csv_path.name, None, args.interval_ms)
    write_hbridge_html(hbridge_html_path, hbridge_svg_path.name, csv_path.name, args.interval_ms)
    if args.open_html:
        open_html(html_path)

    print(f"Live HTML: {html_path}", flush=True)
    print("Uploading sampler to Ubuntu...", flush=True)
    upload_runtime(args, project)
    print("Starting SWD sampler. Press Ctrl+C to stop.", flush=True)
    reset_remote_probe(args)

    rows: deque[dict[str, int]] = deque(maxlen=args.window)
    pretrigger: deque[dict[str, int]] = deque(maxlen=max(20, 200 // max(1, args.interval_ms)))
    hbridge_snapshot = HBridgeSnapshot(args.interval_ms)
    cycle_rows: list[dict[str, int]] = []
    active_cycle = False
    last_target: int | None = None
    last_cycle_name: str | None = None
    cycle_count = 0
    hold_count = 0
    last_redraw = 0.0
    proc: subprocess.Popen[str] | None = None

    try:
        while True:
            got_header = False
            print("Opening SWD stream...", flush=True)
            reset_remote_probe(args)
            proc = start_remote_sampler(args)
            assert proc.stdout is not None
            for raw_line in proc.stdout:
                line = raw_line.strip()
                if not line:
                    continue
                if not got_header:
                    if line.startswith("time_ms,"):
                        got_header = True
                    else:
                        print(line, flush=True)
                    continue

                row = as_int_row(fieldnames, line)
                if row is None:
                    print(line, flush=True)
                    continue

                rows.append(row)
                hbridge_snapshot.add(row)
                with csv_path.open("a", newline="", encoding="utf-8") as handle:
                    csv.DictWriter(handle, fieldnames=fieldnames).writerow(row)

                target_adc = int(row["target_adc"])
                target_changed = last_target is not None and abs(target_adc - last_target) >= 3
                if not active_cycle and target_changed:
                    active_cycle = True
                    cycle_rows = list(pretrigger)
                    hold_count = 0
                if active_cycle:
                    cycle_rows.append(row)
                    if int(row["event"]) == 2 and int(row["duty"]) == 0:
                        hold_count += 1
                    else:
                        hold_count = 0
                    if hold_count >= max(8, 180 // max(1, args.interval_ms)) and len(cycle_rows) > hold_count + 5:
                        cycle_count += 1
                        stem = f"cycle_{cycle_count:03d}_{int(cycle_rows[0]['pwm_us'])}_to_{int(row['pwm_us'])}"
                        cycle_csv = cycle_dir / f"{stem}.csv"
                        cycle_svg = cycle_dir / f"{stem}.svg"
                        write_cycle_csv(cycle_csv, cycle_rows)
                        write_svg(cycle_svg, cycle_rows, stem)
                        last_cycle_name = f"cycles/{cycle_svg.name}"
                        active_cycle = False
                        cycle_rows = []
                        hold_count = 0
                        print(f"Saved cycle: {cycle_svg}", flush=True)
                else:
                    pretrigger.append(row)
                last_target = target_adc

                now = time.monotonic()
                if now - last_redraw >= 0.5 and len(rows) >= 2:
                    live_rows = list(rows)
                    write_svg(svg_path, live_rows, "舵机实时曲线")
                    write_html(html_path, svg_path.name, csv_path.name, last_cycle_name, args.interval_ms)
                    last_redraw = now

                hbridge_rows = hbridge_snapshot.take_dirty_rows()
                if hbridge_rows is not None:
                    write_hbridge_response_svg(hbridge_svg_path, hbridge_rows, "H桥PWM与AD响应 - PWM/外力触发快照")
                    write_hbridge_html(hbridge_html_path, hbridge_svg_path.name, csv_path.name, args.interval_ms)

            rc = proc.wait()
            print(f"SWD stream ended rc={rc}; reconnecting in 2s.", flush=True)
            reset_remote_probe(args)
            time.sleep(2.0)
    except KeyboardInterrupt:
        print("\nStopping...")
        if proc is not None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
