#!/usr/bin/env python3
"""Read the existing servo ring from a running PY32, without halt or writes.

Run on the verified Ubuntu DAPLink host. Two matching RAM reads and unchanged
ring headers reduce torn snapshots; they do not guarantee an atomic snapshot.
No PWM stimulus or motor command is issued.
"""
import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import runpy
import struct
import subprocess
import time
from unittest.mock import patch


ENTRY_SIZE = 20
REQUIRED = {"g_servo_curve_log", "g_servo_curve_log_head", "g_servo_curve_log_count",
            "s_servo_curve_log_generation", "uwTick", "s_edge_count", "s_pulse_count", "s_valid"}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_symbols(elf):
    output = subprocess.check_output(["arm-none-eabi-nm", "-S", str(elf)], text=True)
    found = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] in REQUIRED:
            found[parts[3]] = (int(parts[0], 16), int(parts[1], 16))
    missing = sorted(REQUIRED - found.keys())
    if missing:
        raise SystemExit("Missing expected symbols: " + ", ".join(missing))
    for name, (address, size) in found.items():
        if not (0x20000000 <= address < address + size <= 0x20000c00):
            raise SystemExit("Unexpected RAM address: " + name)
    if found["g_servo_curve_log"][1] % ENTRY_SIZE:
        raise SystemExit("Invalid ring size")
    return found


def decode_entry(data, index):
    time_ms, pwm, target, feedback, error, duty, velocity, state, event = struct.unpack_from(
        "<IHHHhhhBB", data, index * ENTRY_SIZE)
    return dict(time_ms=time_ms, pwm_us=pwm, target_adc=target, feedback_adc=feedback,
                error_adc=error, duty=duty, velocity_q4=velocity, state=state, event=event)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", type=Path, required=True)
    p.add_argument("--elf-sha256", required=True)
    p.add_argument("--bin", type=Path, required=True)
    p.add_argument("--bin-sha256", required=True)
    p.add_argument("--uid", required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--seconds", type=float, default=15.0)
    p.add_argument("--interval-ms", type=int, default=500)
    p.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    p.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    args = p.parse_args()
    if args.seconds <= 0 or args.interval_ms < 250:
        p.error("seconds must be positive and interval-ms must be at least 250")
    if sha(args.elf) != args.elf_sha256 or sha(args.bin) != args.bin_sha256:
        raise SystemExit("Selected ELF or BIN hash mismatch")
    if args.output.exists():
        raise SystemExit("Output exists; choose a new directory")
    image = args.bin.read_bytes()
    marker = b"P50 V10.5"
    version_offset = image.find(marker)
    if len(image) < 128 or version_offset < 0 or image.find(marker, version_offset + 1) >= 0:
        raise SystemExit("Selected BIN does not contain exactly one V10.5 marker")
    symbols = read_symbols(args.elf)
    capacity = symbols["g_servo_curve_log"][1] // ENTRY_SIZE
    if not (1 <= capacity <= 64):
        raise SystemExit("Unexpected ring capacity")
    args.output.mkdir(parents=True, exist_ok=False)

    os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
    with patch("runpy.run_module"):
        runpy.run_path(args.compat)
    from pyocd.core.helpers import ConnectHelper
    options = {"frequency": 1000000, "connect_mode": "attach", "pack": args.pack,
               "resume_on_disconnect": False, "cache.enable_memory": False,
               "cache.enable_register": False, "cmsis_dap.prefer_v1": True,
               "cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False}
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.uid, target_override="py32f002ax5", blocking=False,
        auto_open=False, options=options)
    if session is None:
        raise SystemExit("Selected DAPLink probe was not found")
    session.open()
    target = session.target
    state_start = str(target.get_state())
    if not state_start.endswith("RUNNING"):
        session.close()
        raise SystemExit("Target is not running: " + state_start)
    try:
        if bytes(target.read_memory_block8(0x08000000, 128)) != image[:128]:
            raise SystemExit("Flash prefix does not match selected V10.5 BIN")
        if bytes(target.read_memory_block8(0x08000000 + version_offset, len(marker))) != marker:
            raise SystemExit("Flash version marker does not match selected V10.5 BIN")
        addrs = {name: val[0] for name, val in symbols.items()}
        start_tick = int(target.read32(addrs["uwTick"]))
        edge_start = int(target.read32(addrs["s_edge_count"]))
        pulse_start = int(target.read32(addrs["s_pulse_count"]))
        start_wall = datetime.datetime.now().astimezone().isoformat()
        started = time.monotonic()
        seen = set()
        events = []
        snapshots = []

        def header():
            # The four scalar reads may span a writer update, so all three
            # headers and both ring copies must agree before accepting a view.
            return (int(target.read16(addrs["g_servo_curve_log_head"])),
                    int(target.read16(addrs["g_servo_curve_log_count"])),
                    int(target.read32(addrs["s_servo_curve_log_generation"])))

        while time.monotonic() - started < args.seconds:
            loop_start = time.monotonic()
            h1 = header()
            raw1 = bytes(target.read_memory_block8(addrs["g_servo_curve_log"], capacity * ENTRY_SIZE))
            h2 = header()
            raw2 = bytes(target.read_memory_block8(addrs["g_servo_curve_log"], capacity * ENTRY_SIZE))
            h3 = header()
            now_tick = int(target.read32(addrs["uwTick"]))
            stable = h1 == h2 == h3 and raw1 == raw2 and h3[0] < capacity and h3[1] <= capacity
            snapshots.append({"wall_s": round(loop_start - started, 4), "tick": now_tick,
                              "head": h3[0], "count": h3[1], "generation": h3[2],
                              "stable_double_read": stable})
            if stable:
                head, count, generation = h3
                indices = list(range(count)) if count < capacity else list(range(head, capacity)) + list(range(head))
                for index in indices:
                    row = decode_entry(raw2, index)
                    if row["time_ms"] == 0:
                        continue
                    key = (generation,) + tuple(row.values())
                    if key in seen:
                        continue
                    seen.add(key)
                    row["generation"] = generation
                    row["after_capture_start"] = int(((row["time_ms"] - start_tick) & 0xffffffff) < 0x80000000)
                    row["error_consistent"] = int(row["error_adc"] == row["target_adc"] - row["feedback_adc"])
                    row["first_seen_wall_s"] = round(loop_start - started, 4)
                    events.append(row)
            time.sleep(max(0, args.interval_ms / 1000 - (time.monotonic() - loop_start)))
        end_tick = int(target.read32(addrs["uwTick"]))
        edge_end = int(target.read32(addrs["s_edge_count"]))
        pulse_end = int(target.read32(addrs["s_pulse_count"]))
        valid_end = int(target.read8(addrs["s_valid"]))
        state_end = str(target.get_state())
    finally:
        session.close()

    fields = ["generation", "time_ms", "pwm_us", "target_adc", "feedback_adc", "error_adc", "duty",
              "velocity_q4", "state", "event", "after_capture_start", "error_consistent", "first_seen_wall_s"]
    with (args.output / "events.csv").open("x", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(events)
    report = {"elf_sha256": args.elf_sha256, "bin_sha256": args.bin_sha256,
              "partial_flash_identity": "first 128 bytes and V10.5 marker match; full application not reread",
              "probe_uid": args.uid, "core_start": state_start, "core_end": state_end,
              "start_time": start_wall, "elapsed_seconds": round(time.monotonic() - started, 3),
              "start_tick": start_tick, "end_tick": end_tick,
              "edges_delta": (edge_end - edge_start) & 0xffffffff,
              "pulses_delta": (pulse_end - pulse_start) & 0xffffffff, "input_valid_end": valid_end,
              "ring_capacity": capacity, "snapshot_count": len(snapshots),
              "stable_snapshot_count": sum(x["stable_double_read"] for x in snapshots),
              "unique_rows": len(events), "fresh_rows": sum(x["after_capture_start"] for x in events),
              "fresh_nonzero_duty_rows": sum(x["after_capture_start"] and x["duty"] != 0 for x in events),
              "snapshots": snapshots, "monitor_sha256": sha(Path(__file__)),
              "scope": "Existing event-triggered 16-entry ring only; sparse SWD, double-read is not atomic, no motor/input/target writes or reset."}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "snapshots"}, indent=2))


if __name__ == "__main__":
    main()
