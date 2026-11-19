#!/usr/bin/env python3
"""Inject one synthetic scheduler backlog on a verified force-coast ELF.

This writes only the runtime scheduler timestamp in RAM. It requires the exact
force-coast V10.6 ELF and refuses to continue unless the target is running and
the control duty is zero. It does not reset the MCU or write Flash.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import time
from unittest.mock import patch


def symbols(elf):
    found = {}
    for line in subprocess.check_output(["arm-none-eabi-nm", "-n", str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] in {
            "uwTick", "s_lcdm_runtime_last_motor_ms", "s_lcdm_servo_current_duty",
            "g_r04_scheduler_dropped_tick_count", "g_r04_scheduler_max_dropped_ticks",
            "g_r04_scheduler_overrun_context_nibbles",
        }:
            found[parts[-1]] = int(parts[0], 16)
    required = {
        "uwTick", "s_lcdm_runtime_last_motor_ms", "s_lcdm_servo_current_duty",
        "g_r04_scheduler_dropped_tick_count", "g_r04_scheduler_max_dropped_ticks",
        "g_r04_scheduler_overrun_context_nibbles",
    }
    missing = sorted(required - found.keys())
    if missing:
        raise SystemExit("Selected ELF lacks scheduler symbols: " + ", ".join(missing))
    return found


def u32(target, address):
    return int(target.read32(address)) & 0xFFFFFFFF


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", type=Path, required=True)
    p.add_argument("--uid", required=True)
    p.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    p.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    p.add_argument("--lag-ms", type=int, default=20)
    p.add_argument("--timeout-s", type=float, default=3.0)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    image = a.elf.read_bytes()
    if b"P50 V10.6SIMB2C" not in image or not 1 <= a.lag_ms <= 100:
        p.error("requires V10.6SIMB2C force-coast ELF and lag-ms 1..100")
    if a.output.exists():
        p.error("output exists; select a new JSON path")
    addr = symbols(a.elf)
    os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
    with patch("runpy.run_module"):
        runpy.run_path(a.compat)
    from pyocd.core.helpers import ConnectHelper

    options = {
        "frequency": 1000000, "connect_mode": "attach", "pack": a.pack,
        "resume_on_disconnect": False, "cache.enable_memory": False,
        "cache.enable_register": False, "cmsis_dap.prefer_v1": True,
        "cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False,
    }
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=a.uid, target_override="py32f002ax5", blocking=False,
        auto_open=False, options=options,
    )
    if session is None:
        raise SystemExit("Selected DAPLink probe was not found")
    session.open()
    try:
        target = session.target
        start_state = str(target.get_state())
        if not start_state.endswith("RUNNING"):
            raise SystemExit("Target is not running: " + start_state)
        initial = {
            "tick": u32(target, addr["uwTick"]),
            "scheduler_last_ms": u32(target, addr["s_lcdm_runtime_last_motor_ms"]),
            "control_duty": int(target.read16(addr["s_lcdm_servo_current_duty"], now=True)),
            "dropped_ticks": u32(target, addr["g_r04_scheduler_dropped_tick_count"]),
            "max_dropped_ticks": int(target.read16(addr["g_r04_scheduler_max_dropped_ticks"], now=True)),
            "overrun_contexts": int(target.read16(addr["g_r04_scheduler_overrun_context_nibbles"], now=True)),
        }
        if initial["control_duty"] != 0:
            raise SystemExit("Refusing injection: control duty is not zero")
        trigger_tick = u32(target, addr["uwTick"])
        target.write32(addr["s_lcdm_runtime_last_motor_ms"], (trigger_tick - a.lag_ms) & 0xFFFFFFFF)
        deadline = time.monotonic() + a.timeout_s
        final = dict(initial)
        while time.monotonic() < deadline:
            time.sleep(0.02)
            final = {
                "tick": u32(target, addr["uwTick"]),
                "scheduler_last_ms": u32(target, addr["s_lcdm_runtime_last_motor_ms"]),
                "control_duty": int(target.read16(addr["s_lcdm_servo_current_duty"], now=True)),
                "dropped_ticks": u32(target, addr["g_r04_scheduler_dropped_tick_count"]),
                "max_dropped_ticks": int(target.read16(addr["g_r04_scheduler_max_dropped_ticks"], now=True)),
                "overrun_contexts": int(target.read16(addr["g_r04_scheduler_overrun_context_nibbles"], now=True)),
            }
            if final["dropped_ticks"] > initial["dropped_ticks"]:
                break
        report = {
            "elf": str(a.elf), "elf_sha256": hashlib.sha256(image).hexdigest(),
            "probe_uid": a.uid, "target": "py32f002ax5", "core_start": start_state,
            "core_end": str(target.get_state()), "lag_ms_written": a.lag_ms,
            "initial": initial, "final": final,
            "scheduler_fault_observed": final["dropped_ticks"] > initial["dropped_ticks"],
            "force_coast": True,
            "note": "Synthetic RAM timestamp injection; no Flash write or reset. Force-coast build disables H-bridge drive.",
        }
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
        if not report["scheduler_fault_observed"] or final["control_duty"] != 0:
            raise SystemExit("Fault injection did not produce the expected overrun/zero-duty result")
    finally:
        session.close()


if __name__ == "__main__":
    main()
