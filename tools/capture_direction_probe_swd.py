#!/usr/bin/env python3
"""Capture and explicitly arm one diagnostic direction pulse on the Ubuntu probe host."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import runpy
import statistics
import subprocess
import time
from unittest.mock import patch

MAGIC = 0x50353050
NAMES = ("g_direction_probe_request", "g_direction_probe_status", "g_direction_probe_adc",
         "s_last_raw", "uwTick", "s_duty", "s_vrefint_raw", "s_vdd_mv",
         "s_vdd_sample_ms")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", type=Path, required=True)
    parser.add_argument("--uid", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report_path = args.output.with_suffix(".json")
    if args.output.exists() or report_path.exists():
        raise SystemExit("Output exists; refusing to overwrite evidence")
    manifest = json.loads((args.release / "manifest.json").read_text())
    image = (args.release / "py32f002_servo.bin").read_bytes()
    elf = args.release / "py32f002_servo.elf"
    if not (manifest.get("direction_probe") and manifest.get("force_coast") and
            manifest.get("internal_step_test") and b"P50 V10.6SIMB2P" in image and
            hashlib.sha256(image).hexdigest() == manifest["artifacts"]["py32f002_servo.bin"]["sha256"] and
            hashlib.sha256(elf.read_bytes()).hexdigest() == manifest["artifacts"]["py32f002_servo.elf"]["sha256"]):
        raise SystemExit("Release identity/flags/hash check failed")
    symbols = {}
    for line in subprocess.check_output(["arm-none-eabi-nm", "-n", str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] in NAMES:
            symbols[parts[-1]] = int(parts[0], 16)
    if set(symbols) != set(NAMES) or any(not 0x20000000 <= a < 0x20000c00 for a in symbols.values()):
        raise SystemExit("Diagnostic symbol identity/range check failed")

    os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
    with patch("runpy.run_module"):
        runpy.run_path("/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    from pyocd.core.helpers import ConnectHelper
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.uid, target_override="py32f002ax5", blocking=False,
        auto_open=False, options={"frequency": 1000000, "connect_mode": "attach",
        "pack": "/home/andrew/ARTERY/DQ110_FW/1.2.8.pack", "resume_on_disconnect": False,
        "cache.enable_memory": False, "cache.enable_register": False,
        "cmsis_dap.prefer_v1": True, "cmsis_dap.limit_packets": True,
        "cmsis_dap.deferred_transfers": False})
    if session is None:
        raise SystemExit("DAPLink not found")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    report = {"armed": False, "success": False, "pulse_us": manifest["probe_pulse_us"],
              "note": "One requested timer pulse; SWD sampling does not independently measure the physical pulse width."}
    session.open()
    target = session.target
    start = time.monotonic()
    try:
        if not str(target.get_state()).endswith("RUNNING"):
            raise RuntimeError("Core is not running")
        observed = bytearray()
        for offset in range(0, len(image), 512):
            observed.extend(target.read_memory_block8(0x08000000 + offset, min(512, len(image) - offset)))
        if observed != image:
            raise RuntimeError("Board application is not the selected diagnostic image")
        report["application_matches"] = True
        with args.output.open("x", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=["seconds", "stage", "tick", "raw_adc",
                "vrefint_raw", "vdd_mv", "vdd_sample_ms", "status", "request", "bridge_duty"])
            writer.writeheader()
            def sample(stage):
                row = {"seconds": round(time.monotonic() - start, 5), "stage": stage,
                       "tick": target.read32(symbols["uwTick"]),
                       "raw_adc": target.read16(symbols["s_last_raw"]),
                       "vrefint_raw": target.read16(symbols["s_vrefint_raw"]),
                       "vdd_mv": target.read16(symbols["s_vdd_mv"]),
                       "vdd_sample_ms": target.read32(symbols["s_vdd_sample_ms"]),
                       "status": target.read32(symbols["g_direction_probe_status"]),
                       "request": target.read32(symbols["g_direction_probe_request"]),
                       "bridge_duty": target.read16(symbols["s_duty"])}
                writer.writerow(row)
                stream.flush()
                return row
            deadline = time.monotonic() + 40
            baseline = []
            while time.monotonic() < deadline:
                row = sample("baseline")
                baseline.append(row)
                if row["status"] != 0 or row["request"] != 0 or row["bridge_duty"] != 0:
                    raise RuntimeError("Diagnostic not idle/coasting; refusing to arm")
                if row["tick"] >= 31000 and len(baseline) >= 12:
                    break
                time.sleep(0.1)
            else:
                raise RuntimeError("Baseline wait timed out")
            recent = [r["raw_adc"] for r in baseline[-12:]]
            if min(recent) < 1700 or max(recent) > 2300 or max(recent) - min(recent) > 8:
                raise RuntimeError("ADC outside stable center window; refusing to arm")
            if (target.read32(0x50000010) & 0x46 or target.read32(0x50000410) & 1 or
                target.read32(0x50000014) & 0x46 or target.read32(0x50000414) & 1):
                raise RuntimeError("Motor GPIO is not coast; refusing to arm")
            report["baseline_adc_range"] = [min(recent), max(recent)]
            report["baseline_vdd_mv_range"] = [min(r["vdd_mv"] for r in baseline[-12:]),
                                                max(r["vdd_mv"] for r in baseline[-12:])]
            report["baseline_vrefint_range"] = [min(r["vrefint_raw"] for r in baseline[-12:]),
                                                  max(r["vrefint_raw"] for r in baseline[-12:])]
            report["request_time_s"] = round(time.monotonic() - start, 5)
            # A write failure is treated as ambiguous; this tool never retries.
            report["armed"] = True
            target.write32(symbols["g_direction_probe_request"], MAGIC)
            samples = []
            end = time.monotonic() + 5
            while time.monotonic() < end:
                samples.append(sample("after_request"))
                time.sleep(0.05)
            status = samples[-1]["status"]
            report["status"] = status
            report["firmware_adc_at_request"] = target.read16(symbols["g_direction_probe_adc"])
            report["post_adc_range"] = [min(r["raw_adc"] for r in samples), max(r["raw_adc"] for r in samples)]
            report["baseline_adc_median"] = statistics.median(recent)
            report["post_adc_median_last_10"] = statistics.median(r["raw_adc"] for r in samples[-10:])
            report["post_vdd_mv_range"] = [min(r["vdd_mv"] for r in samples),
                                             max(r["vdd_mv"] for r in samples)]
            report["post_vrefint_range"] = [min(r["vrefint_raw"] for r in samples),
                                              max(r["vrefint_raw"] for r in samples)]
            report["final_gpio_a_idr"] = target.read32(0x50000010) & 0x46
            report["final_gpio_b_idr"] = target.read32(0x50000410) & 1
            report["final_tim1_clock_enabled"] = bool(target.read32(0x40021040) & (1 << 11))
            report["success"] = (status == 2 and not report["final_gpio_a_idr"] and
                                 not report["final_gpio_b_idr"] and not report["final_tim1_clock_enabled"])
    except Exception as exc:
        report["error"] = str(exc)
    finally:
        try:
            report["core_end"] = str(target.get_state())
        finally:
            session.close()
            report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    if not report["success"]:
        raise SystemExit("Probe not verified; do not retry automatically")


if __name__ == "__main__":
    main()
