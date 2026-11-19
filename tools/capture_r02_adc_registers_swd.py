#!/usr/bin/env python3
"""Read ADC/GPIO configuration and matching RAM values without halting the MCU.

Run on the Ubuntu DAPLink host with the exact ELF currently on the board. ADC_DR
is excluded by default; its optional read waits for ADC disabled, channel 4
selected, and EOC clear, then checks that EOC remains clear.
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


ADC_BASE = 0x40012400
GPIOA_BASE = 0x50000000
REGISTERS = {
    "adc_isr": ADC_BASE + 0x00,
    "adc_ier": ADC_BASE + 0x04,
    "adc_cr": ADC_BASE + 0x08,
    "adc_cfgr1": ADC_BASE + 0x0C,
    "adc_cfgr2": ADC_BASE + 0x10,
    "adc_smpr": ADC_BASE + 0x14,
    "adc_tr": ADC_BASE + 0x20,
    "adc_chselr": ADC_BASE + 0x28,
    "adc_ccr": ADC_BASE + 0x308,
    "gpioa_moder": GPIOA_BASE + 0x00,
    "gpioa_pupdr": GPIOA_BASE + 0x0C,
    "gpioa_idr": GPIOA_BASE + 0x10,
}
SYMBOL_NAMES = {
    "tick": "uwTick",
    "adc_raw_last": "s_last_raw",
    "adc_sample_ms": "s_adc_raw_sample_ms",
    "adc_sample_us_q4": "s_adc_raw_sample_us_q4",
    "feedback_duty": "s_lcdm_servo_current_duty",
}


def symbols(elf):
    found = {}
    for line in subprocess.check_output(["arm-none-eabi-nm", "-n", str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] in SYMBOL_NAMES.values():
            found[parts[-1]] = int(parts[0], 16)
    missing = sorted(set(SYMBOL_NAMES.values()) - found.keys())
    if missing:
        raise SystemExit("Selected ELF lacks ADC telemetry symbols: " + ", ".join(missing))
    return found


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", type=Path, required=True)
    p.add_argument("--uid", required=True)
    p.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    p.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    p.add_argument("--seconds", type=float, default=10.0)
    p.add_argument("--interval-ms", type=int, default=100)
    p.add_argument("--read-idle-dr", action="store_true",
                   help="read ADC_DR once only while ADC is disabled, channel 4 selected, and EOC clear")
    p.add_argument("--output", type=Path, required=True, help="CSV path; JSON is written beside it")
    a = p.parse_args()
    if a.seconds <= 0 or a.interval_ms < 10:
        p.error("seconds must be positive and interval-ms at least 10")
    if b"P50 V10.6SIMB2C" not in a.elf.read_bytes():
        p.error("requires the V10.6SIMB2C force-coast ELF")
    if a.output.exists() or a.output.with_suffix(".json").exists():
        p.error("output exists; choose a new capture path")
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
    rows = 0
    started_wall = datetime.datetime.now().astimezone().isoformat()
    started = time.monotonic()
    try:
        target = session.target
        core_start = str(target.get_state())
        if not core_start.endswith("RUNNING"):
            raise SystemExit("Target is not running: " + core_start)
        a.output.parent.mkdir(parents=True, exist_ok=True)
        fields = ["wall_s", "tick", "adc_raw_last", "adc_sample_ms", "adc_sample_us_q4",
                  "feedback_duty"] + list(REGISTERS)
        with a.output.open("x", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            while time.monotonic() - started < a.seconds:
                begin = time.monotonic()
                row = {"wall_s": round(begin - started, 6)}
                for label, symbol in SYMBOL_NAMES.items():
                    row[label] = int(target.read32(addr[symbol])) if label != "feedback_duty" else int(
                        target.read16(addr[symbol], now=True))
                for label, address in REGISTERS.items():
                    row[label] = int(target.read32(address))
                writer.writerow(row)
                f.flush()
                rows += 1
                time.sleep(max(0.0, a.interval_ms / 1000.0 - (time.monotonic() - begin)))
        idle_dr = None
        if a.read_idle_dr:
            deadline = time.monotonic() + 3.0
            while time.monotonic() < deadline:
                isr_before = int(target.read32(REGISTERS["adc_isr"]))
                cr_before = int(target.read32(REGISTERS["adc_cr"]))
                channel_before = int(target.read32(REGISTERS["adc_chselr"]))
                if cr_before == 0 and channel_before == 0x10 and (isr_before & 0x04) == 0:
                    raw_before = int(target.read16(addr["s_last_raw"], now=True))
                    dr = int(target.read32(ADC_BASE + 0x40)) & 0xFFFF
                    isr_after = int(target.read32(REGISTERS["adc_isr"]))
                    cr_after = int(target.read32(REGISTERS["adc_cr"]))
                    channel_after = int(target.read32(REGISTERS["adc_chselr"]))
                    raw_after = int(target.read16(addr["s_last_raw"], now=True))
                    idle_dr = {
                        "eligible_before_read": True, "adc_isr_before": hex(isr_before),
                        "adc_cr_before": hex(cr_before), "adc_chselr_before": hex(channel_before),
                        "adc_dr_value": dr, "adc_isr_after": hex(isr_after),
                        "adc_cr_after": hex(cr_after), "adc_chselr_after": hex(channel_after),
                        "raw_ram_before": raw_before, "raw_ram_after": raw_after,
                        "ram_unchanged": raw_before == raw_after,
                        "eoc_clear_after": (isr_after & 0x04) == 0,
                    }
                    break
                time.sleep(0.005)
        report = {
            "elf": str(a.elf), "elf_sha256": hashlib.sha256(a.elf.read_bytes()).hexdigest(),
            "probe_uid": a.uid, "target": "py32f002ax5", "connect_mode": "attach",
            "core_start": core_start, "core_end": str(target.get_state()),
            "start_time": started_wall, "samples": rows,
            "adc_dr_read": idle_dr is not None, "adc_dr_idle_snapshot": idle_dr,
            "cpu_halted": False,
            "register_addresses": {k: hex(v) for k, v in REGISTERS.items()},
            "ram_symbol_addresses": {k: hex(addr[v]) for k, v in SYMBOL_NAMES.items()},
            "csv_sha256": hashlib.sha256(a.output.read_bytes()).hexdigest(),
            "note": "Live peripheral snapshots; register values may be sampled during an ADC operation. ADC_DR is read only when disabled, channel 4 is selected, and EOC is clear; the script records whether EOC stayed clear.",
        }
        a.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
    finally:
        session.close()


if __name__ == "__main__":
    main()
