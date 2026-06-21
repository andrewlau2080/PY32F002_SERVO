#!/usr/bin/env python3
"""Dump the firmware servo curve ring log through SWD."""

from __future__ import annotations

import argparse
import csv
import os
import subprocess
from pathlib import Path


ENTRY_SIZE = 20
CAPACITY = 35
FIELDS = [
    "time_ms",
    "pwm_us",
    "target_adc",
    "feedback_adc",
    "error_adc",
    "duty",
    "velocity_q4",
    "state",
    "event",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Dump servo curve ring log from PY32 RAM.")
    parser.add_argument("--elf", default="build/py32f002_servo.elf")
    parser.add_argument("--target", default="py32f002ax5")
    parser.add_argument("--frequency", type=int, default=100_000)
    parser.add_argument("--pack", default="/tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack")
    parser.add_argument("--output", default="build/servo_curve_live/servo_curve_ring.csv")
    parser.add_argument("--pyocd-backend", default="hidapiusb")
    return parser.parse_args()


def run_nm(elf: Path) -> dict[str, int]:
    output = subprocess.check_output(["arm-none-eabi-nm", "-n", str(elf)], text=True)
    wanted = {
        "g_servo_curve_log",
        "g_servo_curve_log_head",
        "g_servo_curve_log_count",
    }
    symbols: dict[str, int] = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2] in wanted:
            symbols[parts[2]] = int(parts[0], 16)
    missing = wanted - set(symbols)
    if missing:
        raise SystemExit("Missing symbols: " + ", ".join(sorted(missing)))
    return symbols


def u16(data: list[int], offset: int) -> int:
    return int(data[offset]) | (int(data[offset + 1]) << 8)


def i16(data: list[int], offset: int) -> int:
    value = u16(data, offset)
    return value - 0x10000 if value & 0x8000 else value


def u32(data: list[int], offset: int) -> int:
    return (
        int(data[offset])
        | (int(data[offset + 1]) << 8)
        | (int(data[offset + 2]) << 16)
        | (int(data[offset + 3]) << 24)
    )


def decode_entry(data: list[int], index: int) -> dict[str, int]:
    base = index * ENTRY_SIZE
    return {
        "time_ms": u32(data, base + 0),
        "pwm_us": u16(data, base + 4),
        "target_adc": u16(data, base + 6),
        "feedback_adc": u16(data, base + 8),
        "error_adc": i16(data, base + 10),
        "duty": i16(data, base + 12),
        "velocity_q4": i16(data, base + 14),
        "state": int(data[base + 16]),
        "event": int(data[base + 17]),
    }


def main() -> None:
    args = parse_args()
    if args.pyocd_backend:
        os.environ["PYOCD_USB_BACKEND"] = args.pyocd_backend

    from pyocd.core.helpers import ConnectHelper

    symbols = run_nm(Path(args.elf))
    options = {
        "frequency": args.frequency,
        "connect_mode": "attach",
        "cache.enable_memory": False,
        "cache.enable_register": False,
        "cmsis_dap.prefer_v1": True,
        "cmsis_dap.limit_packets": True,
        "cmsis_dap.deferred_transfers": False,
    }
    if args.pack:
        options["pack"] = args.pack

    session = ConnectHelper.session_with_chosen_probe(
        target_override=args.target,
        blocking=False,
        auto_open=False,
        options=options,
    )
    if session is None:
        raise SystemExit("No SWD probe found.")
    was_halted = False
    session.open()
    try:
        target = session.target
        try:
            was_halted = bool(target.get_state().name == "HALTED")
        except Exception:
            pass
        try:
            target.halt()
        except Exception:
            pass

        head = int(target.read16(symbols["g_servo_curve_log_head"]))
        count = int(target.read16(symbols["g_servo_curve_log_count"]))
        if head > CAPACITY:
            head = 0
        if count > CAPACITY:
            count = CAPACITY

        raw = list(target.read_memory_block8(symbols["g_servo_curve_log"], CAPACITY * ENTRY_SIZE))
        rows = [decode_entry(raw, i) for i in range(CAPACITY)]
        if count < CAPACITY:
            ordered = rows[:count]
        else:
            ordered = rows[head:] + rows[:head]
        ordered = [row for row in ordered if row["time_ms"] != 0]

        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=FIELDS)
            writer.writeheader()
            writer.writerows(ordered)

        print(f"head={head} count={count} rows={len(ordered)} output={output}")
        for row in ordered[-12:]:
            print(row)
    finally:
        try:
            if not was_halted:
                session.target.resume()
        except Exception:
            pass
        session.close()


if __name__ == "__main__":
    main()
