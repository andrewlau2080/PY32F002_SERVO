#!/usr/bin/env python3
"""Read selected R01 RAM symbols one at a time from the running TSSOP20 board."""

import argparse
import base64
import json
import os
import runpy
import shlex
import subprocess
import sys
import time
from unittest.mock import patch


REMOTE_READ = r'''import base64, json, os, runpy, sys, time
from unittest.mock import patch
cfg = json.loads(base64.b64decode(sys.argv[1]))
os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
with patch("runpy.run_module"):
    runpy.run_path(cfg["compat"])
from pyocd.core.helpers import ConnectHelper
options = {"frequency": 1000000, "connect_mode": "attach", "pack": cfg["pack"],
           "resume_on_disconnect": False, "cache.enable_memory": False,
           "cache.enable_register": False, "cmsis_dap.prefer_v1": True,
           "cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False}
session = ConnectHelper.session_with_chosen_probe(unique_id=cfg["uid"],
    target_override="py32f002ax5", blocking=False, auto_open=False, options=options)
if session is None:
    raise SystemExit("DAPLink not found")
session.open()
try:
    target = session.target
    if cfg["stop"]:
        target.write_memory_block8(cfg["symbols"]["g_r01_production_test_active"][0], [0])
        target.write32(cfg["symbols"]["g_r01_active_test_arm_request"][0], 2)
        print("STOP_REQUEST_WRITTEN", flush=True)
    if cfg["arm_motion"]:
        active_addr = cfg["symbols"].get("g_r01_production_test_active", [None])[0]
        arm_addr = cfg["symbols"].get("g_r01_active_test_arm_request", [None])[0]
        if active_addr is None or arm_addr is None:
            raise SystemExit("Selected ELF lacks explicit-arm production test symbols")
        active_now = target.read8(active_addr)
        arm_state = target.read32(arm_addr)
        if active_now != 0 or arm_state not in (0, 2):
            raise SystemExit(f"R01 test is already active or armed (active={active_now}, arm={arm_state}); refusing trigger")
        target.write32(arm_addr, 0x52303141)
        print(json.dumps({"event": "R01_ARM_SENT", "active_before": active_now,
                          "arm_before": arm_state, "pulse_us": target.read16(
                              cfg["symbols"]["g_r01_production_test_pulse_us"][0])}), flush=True)
    for sample in range(cfg["samples"]):
        out = {"sample": sample, "monotonic_s": time.monotonic(),
               "core": str(target.get_state()), "symbols": {}}
        for name, (address, size, signed) in cfg["symbols"].items():
            if address is None:
                out["symbols"][name] = "missing"
                continue
            try:
                data = bytes(target.read_memory_block8(address, size))
                if name == "s_status":
                    fields = ("state", "input_us", "target_adc", "feedback_adc", "error_adc", "motor_duty", "input_valid")
                    offsets = (0, 2, 4, 6, 8, 10, 12)
                    out["symbols"][name] = dict(zip(fields, [int.from_bytes(data[o:o + (1 if o == 12 else 2)], "little", signed=o in (8, 10)) for o in offsets]))
                else:
                    out["symbols"][name] = int.from_bytes(data, "little", signed=signed)
            except Exception as exc:
                out["symbols"][name] = "READ_ERROR " + type(exc).__name__ + ": " + str(exc)
        out["core_end"] = str(target.get_state())
        print(json.dumps(out), flush=True)
        if sample + 1 < cfg["samples"]:
            time.sleep(cfg["interval_ms"] / 1000.0)
finally:
    session.close()
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--uid", required=True)
    parser.add_argument("--stop", action="store_true", help="stop an armed R01 loop and request coast")
    parser.add_argument("--samples", type=int, default=1, help="number of read-only RAM snapshots")
    parser.add_argument("--interval-ms", type=int, default=100, help="delay between snapshots")
    parser.add_argument("--arm-r01-motion", action="store_true",
                        help="explicitly arm the preconfigured, limited R01 production motion test")
    parser.add_argument("--summary", action="store_true", help="summarize repeated remote snapshots instead of printing every frame")
    parser.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    parser.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    args = parser.parse_args()
    if args.samples < 1 or args.interval_ms < 10:
        parser.error("samples must be positive and interval-ms at least 10")
    nm_output = subprocess.check_output(["arm-none-eabi-nm", "-n", args.elf], text=True)
    addresses = {}
    for line in nm_output.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            try:
                addresses[parts[-1]] = int(parts[0], 16)
            except ValueError:
                pass
    names = (
        ("g_r01_active_test_arm_request", 4, False),
        ("g_r01_production_test_active", 1, False),
        ("g_r01_production_test_pulse_us", 2, False),
        ("g_r01_test_sweep_stage", 1, False),
        ("g_r01_test_sweep_result", 1, False),
        ("g_r01_test_sweep_baseline_adc", 2, False),
        ("g_r01_test_sweep_delta_adc", 2, True),
        ("g_r01_test_sweep_filtered_delta_adc", 2, True),
        ("g_r01_test_sweep_peak_delta_adc", 2, False),
        ("g_r01_test_sweep_confirm_count", 1, False),
        ("s_pulse_count", 4, False),
        ("s_valid", 1, False),
        ("s_test_pulse_count", 4, False),
        ("s_test_valid", 1, False),
        ("s_status", 16, False),
        ("g_r01_diag_nonzero_request_count", 4, False),
        ("g_r01_diag_max_abs_request_duty", 2, False),
        ("g_r01_diag_last_request_duty", 2, True),
        ("g_r01_diag_lock_reason", 1, False),
        ("g_r01_prod_nonzero_duty_count", 4, False),
        ("g_r01_prod_max_abs_duty", 2, False),
        ("g_r01_prod_feedback_min", 2, False),
        ("g_r01_prod_feedback_max", 2, False),
        ("g_r01_prod_last_duty", 2, True),
        ("g_r01_prod_anchor_adc", 2, False),
        ("g_r01_prod_center_offset_adc", 2, True),
        ("g_r01_prod_motion_locked", 1, False),
        ("g_r01_prod_motion_lock_reason", 1, False),
        ("g_r01_prod_max_displacement_adc", 2, False),
        ("g_r01_prod_signed_displacement_adc", 2, True),
        ("g_r01_prod_anchor_raw_adc", 2, False),
        ("g_r01_prod_max_raw_displacement_adc", 2, False),
        ("g_r01_prod_raw_displacement_adc", 2, True),
        ("g_r01_prod_lock_time_ms", 4, False),
        ("g_r01_prod_lock_input_us", 2, False),
        ("g_r01_prod_lock_feedback_adc", 2, False),
        ("g_r01_prod_lock_raw_adc", 2, False),
        ("g_r01_prod_lock_target_adc", 2, False),
        ("g_r01_prod_lock_error_adc", 2, True),
        ("g_r01_prod_lock_bridge_duty", 2, True),
        ("s_vrefint_raw", 2, False),
        ("s_vdd_mv", 2, False),
        ("s_vdd_sample_valid", 1, False),
        ("s_vdd_sample_ms", 4, False),
        ("s_vdd_sample_us_q4", 4, False),
        ("s_adc_raw_sample_ms", 4, False),
        ("s_adc_raw_sample_us_q4", 4, False),
        ("g_adc_diag_seq", 4, False),
        ("g_adc_diag_sample0", 2, False),
        ("g_adc_diag_sample1", 2, False),
        ("g_adc_diag_sample2", 2, False),
        ("g_adc_diag_sample3", 2, False),
        ("g_adc_diag_vdd_mv", 2, False),
        ("g_adc_diag_time0_q4", 4, False),
        ("g_adc_diag_time3_q4", 4, False),
        ("g_adc_diag_batch_count", 2, False),
        ("g_adc_diag_vdd_sample_us_q4", 4, False),
        ("g_r01_center_live_adc", 2, False),
        ("g_r01_center_start_adc", 2, False),
        ("g_r01_center_target_adc", 2, False),
        ("g_r01_center_result", 1, False),
        ("s_hold_active", 1, False),
        ("s_reverse_pending_dir", 1, True),
        ("s_stall_ms", 4, False),
        ("g_r01_bridge_forward_on_ticks", 4, False),
        ("g_r01_bridge_reverse_on_ticks", 4, False),
        ("s_lcdm_servo_current_duty", 2, True),
        ("s_lcdm_servo_current_state", 1, False),
        ("s_last_raw", 2, False),
        ("s_duty", 2, True),
        ("s_state", 1, False),
        ("s_tjc_tx_cmd_count", 4, False),
        ("s_tjc_rx_byte_count", 4, False),
        ("s_tjc_rx_packet_count", 4, False),
        ("s_tjc_last_packet_len", 1, False),
        ("s_tjc_last_packet0", 1, False),
        ("s_tjc_last_packet1", 1, False),
        ("s_tjc_last_packet2", 1, False),
        ("s_touch_x", 2, False),
        ("s_touch_y", 2, False),
        ("s_touch_event", 1, False),
        ("s_touch_seq", 1, False),
        ("s_tjc_tx_error_count", 4, False),
        ("s_tjc_uart_error_count", 4, False),
    )
    if not sys.platform.startswith("linux"):
        if args.stop and ("g_r01_production_test_active" not in addresses or
                          "g_r01_active_test_arm_request" not in addresses):
            raise SystemExit("Selected ELF lacks R01 stop symbols")
        cfg = {"uid": args.uid, "pack": args.pack, "compat": args.compat,
               "stop": args.stop, "arm_motion": args.arm_r01_motion,
               "samples": args.samples, "interval_ms": args.interval_ms,
               "symbols": {name: [addresses.get(name), size, signed]
                           for name, size, signed in names}}
        encoded = base64.b64encode(json.dumps(cfg).encode()).decode()
        host = os.getenv("SERVO_REMOTE_HOST", "andrew@10.211.55.4")
        python = os.getenv("SERVO_REMOTE_PYTHON", "/home/andrew/ARTERY/.tools/pyocd-venv/bin/python")
        command = shlex.join([python, "-", encoded])
        result = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", host, command],
                                input=REMOTE_READ, text=True, capture_output=True, timeout=60)
        if result.stdout:
            if args.summary and args.samples > 1:
                rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
                frames = [row for row in rows if "symbols" in row]
                def series(name):
                    return [row["symbols"].get(name) for row in frames
                            if isinstance(row["symbols"].get(name), int)]
                status = [row["symbols"].get("s_status") for row in frames
                          if isinstance(row["symbols"].get("s_status"), dict)]
                summary = {
                    "arm_event": next((row for row in rows if row.get("event") == "R01_ARM_SENT"), None),
                    "frames": len(frames),
                    "duration_s": round(frames[-1]["monotonic_s"] - frames[0]["monotonic_s"], 3) if len(frames) > 1 else 0,
                    "core_states": sorted({row.get("core") for row in frames}),
                    "commands_us": sorted({row.get("symbols", {}).get("g_r01_production_test_pulse_us") for row in frames}),
                    "sweep_stages": sorted(set(series("g_r01_test_sweep_stage"))),
                    "sweep_results": sorted(set(series("g_r01_test_sweep_result"))),
                    "sweep_baseline_adc": sorted(set(series("g_r01_test_sweep_baseline_adc"))),
                    "sweep_delta_adc_min_max": [min(series("g_r01_test_sweep_delta_adc")), max(series("g_r01_test_sweep_delta_adc"))] if series("g_r01_test_sweep_delta_adc") else [],
                    "sweep_filtered_delta_adc_min_max": [min(series("g_r01_test_sweep_filtered_delta_adc")), max(series("g_r01_test_sweep_filtered_delta_adc"))] if series("g_r01_test_sweep_filtered_delta_adc") else [],
                    "sweep_peak_delta_adc_max": max(series("g_r01_test_sweep_peak_delta_adc"), default=0),
                    "sweep_confirm_counts": sorted(set(series("g_r01_test_sweep_confirm_count"))),
                    "feedback_adc_min_max": [min(series("s_last_raw")), max(series("s_last_raw"))] if series("s_last_raw") else [],
                    "filtered_feedback_min_max": [min(item.get("feedback_adc") for item in status), max(item.get("feedback_adc") for item in status)] if status else [],
                    "position_lock_values": sorted(set(series("g_r01_prod_motion_locked"))),
                    "position_lock_reason_values": sorted(set(series("g_r01_prod_motion_lock_reason"))),
                    "max_anchor_displacement_adc": max(series("g_r01_prod_max_displacement_adc"), default=0),
                    "signed_anchor_displacement_min_max": [min(series("g_r01_prod_signed_displacement_adc")), max(series("g_r01_prod_signed_displacement_adc"))] if series("g_r01_prod_signed_displacement_adc") else [],
                    "max_raw_anchor_displacement_adc": max(series("g_r01_prod_max_raw_displacement_adc"), default=0),
                    "signed_raw_displacement_min_max": [min(series("g_r01_prod_raw_displacement_adc")), max(series("g_r01_prod_raw_displacement_adc"))] if series("g_r01_prod_raw_displacement_adc") else [],
                    "lock_snapshot_latest": {name: frames[-1]["symbols"].get(name) for name in
                        ("g_r01_prod_motion_lock_reason", "g_r01_prod_lock_time_ms",
                         "g_r01_prod_lock_input_us", "g_r01_prod_lock_feedback_adc",
                         "g_r01_prod_lock_raw_adc", "g_r01_prod_lock_target_adc",
                         "g_r01_prod_lock_error_adc", "g_r01_prod_lock_bridge_duty")} if frames else {},
                    "vdd_mv_min_max": [min(series("s_vdd_mv")), max(series("s_vdd_mv"))] if series("s_vdd_mv") else [],
                    "adc_diag_samples_latest": {name: frames[-1]["symbols"].get(name) for name in
                        ("g_adc_diag_sample0", "g_adc_diag_sample1", "g_adc_diag_sample2", "g_adc_diag_sample3",
                         "g_adc_diag_vdd_mv", "g_adc_diag_time0_q4", "g_adc_diag_time3_q4",
                         "g_adc_diag_vdd_sample_us_q4")} if frames else {},
                    "duty_abs_max": max(max((abs(v) for v in series("s_duty")), default=0),
                                        max(series("g_r01_prod_max_abs_duty"), default=0)),
                    "active_values": sorted(set(series("g_r01_production_test_active"))),
                    "nonzero_duty_count_start_end": [series("g_r01_prod_nonzero_duty_count")[0], series("g_r01_prod_nonzero_duty_count")[-1]] if series("g_r01_prod_nonzero_duty_count") else [],
                    "bridge_forward_ticks_start_end": [series("g_r01_bridge_forward_on_ticks")[0], series("g_r01_bridge_forward_on_ticks")[-1]] if series("g_r01_bridge_forward_on_ticks") else [],
                    "bridge_reverse_ticks_start_end": [series("g_r01_bridge_reverse_on_ticks")[0], series("g_r01_bridge_reverse_on_ticks")[-1]] if series("g_r01_bridge_reverse_on_ticks") else [],
                    "status_states": sorted({item.get("state") for item in status}),
                }
                print(json.dumps(summary))
            else:
                print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        if result.returncode:
            raise SystemExit(result.returncode)
        return
    os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
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
        raise SystemExit("DAPLink not found")
    session.open()
    try:
        target = session.target
        if args.stop:
            active = addresses.get("g_r01_production_test_active")
            arm = addresses.get("g_r01_active_test_arm_request")
            if active is None or arm is None:
                raise SystemExit("Selected ELF lacks R01 stop symbols")
            target.write_memory_block8(active, [0])
            target.write32(arm, 2)
            print("STOP_REQUEST_WRITTEN", flush=True)
        print("CORE", target.get_state())
        for name, size, signed in names:
            address = addresses.get(name)
            if address is None:
                print(name, "missing")
                continue
            try:
                data = bytes(target.read_memory_block8(address, size))
                if name == "s_status":
                    fields = ("state", "input_us", "target_adc", "feedback_adc",
                              "error_adc", "motor_duty", "input_valid")
                    offsets = (0, 2, 4, 6, 8, 10, 12)
                    values = [int.from_bytes(data[o:o + (1 if o == 12 else 2)],
                                             "little", signed=o in (8, 10)) for o in offsets]
                    print(name, dict(zip(fields, values)))
                else:
                    print(name, int.from_bytes(data, "little", signed=signed))
            except Exception as exc:
                print(name, "READ_ERROR", type(exc).__name__, str(exc))
        print("CORE_END", target.get_state())
    finally:
        session.close()


if __name__ == "__main__":
    main()
