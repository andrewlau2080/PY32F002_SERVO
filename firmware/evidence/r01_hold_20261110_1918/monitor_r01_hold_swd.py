#!/usr/bin/env python3
"""Capture R01 input and HOLD diagnostic state from a running target, without reset.

Run on the Ubuntu DAPLink host with the exact ELF that is on the board. This is
an observer only: it neither changes the input source nor commands the motor.
"""

import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import time
from unittest.mock import patch


SYMBOLS = {
    "tick": ("uwTick", 4, False),
    "source": ("s_lcdm_pwm_source", 1, False),
    "edge_count": ("s_edge_count", 4, False),
    "pulse_count": ("s_pulse_count", 4, False),
    "input_valid": ("s_valid", 1, False),
    "last_edge_ms": ("s_last_edge_ms", 4, False),
    "hw_raw_us": ("s_raw_pulse_us", 2, False),
    "hw_accepted_us": ("s_last_pulse_us", 2, False),
    "hw_candidate_us": ("s_candidate_pulse_us", 2, False),
    "latched_us": ("s_lcdm_servo_latched_pulse_us", 2, False),
    "overshoot_adc": ("s_lcdm_servo_overshoot_adc", 2, False),
    "hold_deviation_count": ("s_lcdm_servo_hold_deviation_count", 1, False),
    "hold_phase": ("s_lcdm_servo_v3_phase", 1, False),
    "frame": ("g_servo_curve_frame", 44, False),
}

FRAME = {
    "seq_head": (0, 4, False),
    "frame_time_ms": (4, 4, False),
    "frame_pwm_us": (8, 2, False),
    "frame_raw_us": (10, 2, False),
    "period_us": (12, 2, False),
    "raw_adc": (14, 2, False),
    "target_adc": (16, 2, False),
    "feedback_adc": (18, 2, False),
    "error_adc": (20, 2, True),
    "duty": (22, 2, True),
    "state": (36, 1, False),
    "event": (37, 1, False),
    "seq_tail": (40, 4, False),
}


def read_symbols(elf):
    output = subprocess.check_output(["arm-none-eabi-nm", "-n", str(elf)], text=True)
    wanted = {name for name, _size, _signed in SYMBOLS.values()}
    found = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] in wanted:
            found[parts[-1]] = int(parts[0], 16)
    missing = sorted(wanted - found.keys())
    if missing:
        raise SystemExit("Missing expected symbols in the selected ELF: " + ", ".join(missing))
    return found


def value(data, offset, size, signed=False):
    return int.from_bytes(data[offset:offset + size], "little", signed=signed)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--uid", required=True)
    p.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    p.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    p.add_argument("--seconds", type=float, default=20.0)
    p.add_argument("--interval-ms", type=int, default=50)
    args = p.parse_args()
    if args.seconds <= 0 or args.interval_ms < 10:
        p.error("seconds must be positive and interval-ms at least 10")
    addresses = read_symbols(args.elf)
    start = min(addresses[name] for name, _size, _signed in SYMBOLS.values())
    end = max(addresses[name] + size for name, size, _signed in SYMBOLS.values())
    if not (0x20000000 <= start < end <= 0x20000c00):
        raise SystemExit("Expected RAM symbols outside the PY32F002A RAM range")
    if args.output.exists() or args.output.with_suffix(".json").exists():
        raise SystemExit("Output already exists; choose a new capture name")
    args.output.parent.mkdir(parents=True, exist_ok=True)

    os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
    # The verified DQ110 script applies HID compatibility before invoking pyOCD.
    with patch("runpy.run_module"):
        runpy.run_path(args.compat)
    from pyocd.core.helpers import ConnectHelper

    options = {
        "frequency": 1000000, "connect_mode": "attach", "pack": args.pack,
        "resume_on_disconnect": False, "cache.enable_memory": False,
        "cache.enable_register": False, "cmsis_dap.prefer_v1": True,
        "cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False,
    }
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.uid, target_override="py32f002ax5", blocking=False,
        auto_open=False, options=options,
    )
    if session is None:
        raise SystemExit("Selected DAPLink probe was not found")
    session.open()
    target = session.target
    core_start = str(target.get_state())
    if not core_start.endswith("RUNNING"):
        session.close()
        raise SystemExit("Target is not running: " + core_start)

    fields = ["wall_s"] + [k for k in SYMBOLS if k != "frame"] + list(FRAME) + [
        "tick_after", "input_age_ms", "frame_seq_consistent", "error_consistent"]
    count = 0
    core_end = "unknown"
    wall_start = datetime.datetime.now().astimezone().isoformat()
    started = time.monotonic()
    try:
        with args.output.open("x", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            while time.monotonic() - started < args.seconds:
                sample_start = time.monotonic()
                row = {"wall_s": round(sample_start - started, 6)}
                # Keep reads short: a full-RAM poll consumes much more SWD bus
                # time and makes it difficult to measure the running system.
                for labels in (
                    ("hw_raw_us", "hw_accepted_us", "hw_candidate_us", "latched_us"),
                    ("tick", "edge_count", "pulse_count", "input_valid", "last_edge_ms",
                     "overshoot_adc", "hold_deviation_count", "hold_phase"),
                    ("source",),
                ):
                    group_start = min(addresses[SYMBOLS[label][0]] for label in labels)
                    group_end = max(addresses[SYMBOLS[label][0]] + SYMBOLS[label][1] for label in labels)
                    data = bytes(target.read_memory_block8(group_start, group_end - group_start))
                    for label in labels:
                        symbol, size, signed = SYMBOLS[label]
                        row[label] = value(data, addresses[symbol] - group_start, size, signed)
                frame_address = addresses[SYMBOLS["frame"][0]]
                for _attempt in range(3):
                    data = bytes(target.read_memory_block8(frame_address, SYMBOLS["frame"][1]))
                    for label, (off, size, signed) in FRAME.items():
                        row[label] = value(data, off, size, signed)
                    if row["seq_head"] == row["seq_tail"] and not (row["seq_head"] & 1):
                        break
                # Tick must be sampled after last_edge_ms; otherwise an edge
                # arriving during the SWD read can appear as a 32-bit underflow.
                row["tick_after"] = int(target.read32(addresses["uwTick"]))
                row["input_age_ms"] = (row["tick_after"] - row["last_edge_ms"]) & 0xffffffff
                row["frame_seq_consistent"] = int(row["seq_head"] == row["seq_tail"] and not (row["seq_head"] & 1))
                row["error_consistent"] = int(abs(row["error_adc"] - (row["target_adc"] - row["feedback_adc"])) <= 2)
                writer.writerow(row)
                count += 1
                time.sleep(max(0.0, args.interval_ms / 1000.0 - (time.monotonic() - sample_start)))
        core_end = str(target.get_state())
    finally:
        session.close()
    report = {
        "elf": str(args.elf), "elf_sha256": hashlib.sha256(args.elf.read_bytes()).hexdigest(),
        "probe_uid": args.uid, "target": "py32f002ax5", "connect_mode": "attach",
        "core_start": core_start, "core_end": core_end, "samples": count,
        "start_time": wall_start,
        "monitor_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "requested_interval_ms": args.interval_ms,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "symbol_addresses": {k: hex(v) for k, v in addresses.items()},
        "note": "Observed SWD samples only; HOLD state bytes and frame are separate reads and not atomic. No stimulus or target state changes were issued.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
