#!/usr/bin/env python3
"""Capture R01 input and controller telemetry from a running target, without reset.

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
    "runtime_pwm_start_ms": ("s_lcdm_runtime_pwm_start_ms", 4, False),
    "source": ("s_lcdm_pwm_source", 1, False),
    "edge_count": ("s_edge_count", 4, False),
    "pulse_count": ("s_pulse_count", 4, False),
    "input_valid": ("s_valid", 1, False),
    "last_edge_ms": ("s_last_edge_ms", 4, False),
    "hw_raw_us": ("s_raw_pulse_us", 2, False),
    "hw_accepted_us": ("s_last_pulse_us", 2, False),
    "hw_candidate_us": ("s_candidate_pulse_us", 2, False),
    "latched_us": ("s_lcdm_servo_latched_pulse_us", 2, False),
    "control_duty": ("s_lcdm_servo_current_duty", 2, True),
    "control_state": ("s_lcdm_servo_current_state", 1, False),
    "velocity_q4_live": ("s_lcdm_servo_vel_q4", 4, True),
    "traj_pos_q4_live": ("s_lcdm_servo_traj_pos_q4", 4, True),
    "traj_vel_q4_live": ("s_lcdm_servo_traj_vel_q4", 4, True),
    "adaptive_confidence": ("g_servo_adaptive_confidence_permille", 2, False),
    "adaptive_gain_fwd_q8": ("g_servo_adaptive_speed_gain_q8_forward", 2, False),
    "adaptive_gain_rev_q8": ("g_servo_adaptive_speed_gain_q8_reverse", 2, False),
    "adaptive_samples_fwd": ("g_servo_adaptive_drive_samples_forward", 2, False),
    "adaptive_samples_rev": ("g_servo_adaptive_drive_samples_reverse", 2, False),
    "adaptive_applied_gain_fwd": ("g_servo_adaptive_applied_velocity_gain_q8_forward", 2, False),
    "adaptive_applied_gain_rev": ("g_servo_adaptive_applied_velocity_gain_q8_reverse", 2, False),
    "model_record_valid": ("g_servo_model_record_valid", 1, False),
    "adc_raw_last": ("s_last_raw", 2, False),
    "adc_vdd_compensated_last": ("g_adc_vdd_compensated_last", 2, False),
    "adc_filtered": ("s_filtered", 2, False),
    "vrefint_raw": ("s_vrefint_raw", 2, False),
    "vdd_mv": ("s_vdd_mv", 2, False),
    "vdd_sample_valid": ("s_vdd_sample_valid", 1, False),
    "vdd_sample_ms": ("s_vdd_sample_ms", 4, False),
    "vdd_sample_us_q4": ("s_vdd_sample_us_q4", 4, False),
    "adc_raw_sample_ms": ("s_adc_raw_sample_ms", 4, False),
    "adc_raw_sample_us_q4": ("s_adc_raw_sample_us_q4", 4, False),
    "r01_diag_nonzero_request_count": ("g_r01_diag_nonzero_request_count", 4, False),
    "r01_diag_max_abs_request_duty": ("g_r01_diag_max_abs_request_duty", 2, False),
    "r01_diag_last_request_duty": ("g_r01_diag_last_request_duty", 2, True),
    "r01_diag_min_feedback_when_request": ("g_r01_diag_min_feedback_when_request", 2, False),
    "r01_diag_max_feedback_when_request": ("g_r01_diag_max_feedback_when_request", 2, False),
    "r01_diag_min_vdd_when_request": ("g_r01_diag_min_vdd_when_request", 2, False),
    "r01_diag_max_vdd_when_request": ("g_r01_diag_max_vdd_when_request", 2, False),
    "r01_diag_lock_reason": ("g_r01_diag_lock_reason", 1, False),
    "r01_diag_control_adc": ("g_r01_diag_control_adc", 2, False),
    "r01_diag_lock_feedback": ("g_r01_diag_lock_feedback", 2, False),
    "r01_diag_lock_error": ("g_r01_diag_lock_error", 2, True),
    "r01_diag_lock_displacement": ("g_r01_diag_lock_displacement", 2, True),
    "r01_stage_stats": ("g_r01_stage_stats", 64, False),
    "adc_diag_sample0": ("g_adc_diag_sample0", 2, False),
    "adc_diag_sample1": ("g_adc_diag_sample1", 2, False),
    "adc_diag_sample2": ("g_adc_diag_sample2", 2, False),
    "adc_diag_sample3": ("g_adc_diag_sample3", 2, False),
    "adc_diag_vdd_mv": ("g_adc_diag_vdd_mv", 2, False),
    "adc_diag_time0_q4": ("g_adc_diag_time0_q4", 4, False),
    "adc_diag_time3_q4": ("g_adc_diag_time3_q4", 4, False),
    "adc_diag_batch_count": ("g_adc_diag_batch_count", 2, False),
    "adc_diag_seq": ("g_adc_diag_seq", 4, False),
    "adc_diag_snapshot_request": ("g_adc_diag_snapshot_request", 4, False),
    "adc_diag_snapshot_ack": ("g_adc_diag_snapshot_ack", 4, False),
    "adc_diag_vdd_sample_us_q4": ("g_adc_diag_vdd_sample_us_q4", 4, False),
    "adc_diag_start_request": ("g_adc_diag_start_request", 4, False),
    "r04_scheduler_overrun_contexts": ("g_r04_scheduler_overrun_context_nibbles", 2, False),
    "r04_scheduler_dropped_ticks": ("g_r04_scheduler_dropped_tick_count", 4, False),
    "r04_scheduler_max_dropped_ticks": ("g_r04_scheduler_max_dropped_ticks", 2, False),
    "r04_timing_samples": ("g_r04_timing_samples", 4, False),
    "r04_timing_bin_0_lt500": ("g_r04_timing_bin_0_lt500", 2, False),
    "r04_timing_bin_1_500_899": ("g_r04_timing_bin_1_500_899", 2, False),
    "r04_timing_bin_2_900_1100": ("g_r04_timing_bin_2_900_1100", 2, False),
    "r04_timing_bin_3_1101_1500": ("g_r04_timing_bin_3_1101_1500", 2, False),
    "r04_timing_bin_4_1501_2000": ("g_r04_timing_bin_4_1501_2000", 2, False),
    "r04_timing_bin_5_2001_4000": ("g_r04_timing_bin_5_2001_4000", 2, False),
    "r04_timing_bin_6_4001_10000": ("g_r04_timing_bin_6_4001_10000", 2, False),
    "r04_timing_bin_7_gt10000": ("g_r04_timing_bin_7_gt10000", 2, False),
    "r04_timing_max_interval_us": ("g_r04_timing_max_interval_us", 4, False),
    "r04_timing_max_exec_us": ("g_r04_timing_max_exec_us", 4, False),
    "r04_timing_exec_over_1000us": ("g_r04_timing_exec_over_1000us", 4, False),
    "r04_sweep_start_request": ("g_r04_sweep_start_request", 4, False),
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
    "velocity_q4": (24, 2, True),
    "ref_velocity": (26, 2, True),
    "traj_adc": (28, 2, True),
    "traj_vel_q4": (30, 2, True),
    "overshoot_adc": (32, 2, False),
    "pwm_candidate_us": (34, 2, False),
    "state": (36, 1, False),
    "event": (37, 1, False),
    "candidate_count": (38, 1, False),
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
    required = {"uwTick", "g_servo_curve_frame"}
    missing = sorted(required - found.keys())
    if missing:
        raise SystemExit("Missing required telemetry symbols in the selected ELF: " + ", ".join(missing))
    return found


def diagnostic_snapshot_consistent(seq_before, seq_after):
    """Accept only an unchanged, completed ADC diagnostic write generation."""
    return seq_before == seq_after and (seq_after & 1) == 0


def value(data, offset, size, signed=False):
    return int.from_bytes(data[offset:offset + size], "little", signed=signed)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--uid", required=True)
    p.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    p.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    # Default duration covers the 30 s pre-roll plus the center microstep sweep.
    p.add_argument("--seconds", type=float, default=70.0)
    p.add_argument("--interval-ms", type=int, default=50)
    p.add_argument("--announce-before-sweep-ms", type=int, default=0,
                   help="stream an immediate warning this many ms before the automatic 30 s center sweep")
    p.add_argument("--expect-center-sweep", action="store_true",
                   help="require PWM sweep sync plus fresh four-sample ADC diagnostic batches")
    p.add_argument("--expect-pwm-sweep", action="store_true",
                   help="require a 1503 us first frame, an upward sweep, and a return near center")
    p.add_argument("--arm-center-sweep", action="store_true",
                   help="start the one-shot sweep only after the SWD capture is attached")
    p.add_argument("--arm-r04-sweep", action="store_true",
                   help="start the R04 timing-trace sweep after SWD capture attaches")
    args = p.parse_args()
    monitor_sha256 = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    if args.seconds <= 0 or args.interval_ms < 10:
        p.error("seconds must be positive and interval-ms at least 10")
    if not 0 <= args.announce_before_sweep_ms <= 29000:
        p.error("announce-before-sweep-ms must be in 0..29000")
    if (args.expect_center_sweep or args.expect_pwm_sweep) and args.seconds < 35.0:
        p.error("center microstep capture must cover at least 35 seconds")
    if args.arm_center_sweep and not args.expect_center_sweep:
        p.error("--arm-center-sweep requires --expect-center-sweep")
    if args.arm_r04_sweep and not args.expect_pwm_sweep:
        p.error("--arm-r04-sweep requires --expect-pwm-sweep")
    if (args.expect_center_sweep or args.expect_pwm_sweep) and b"P50 V10.6SIMB2" not in args.elf.read_bytes():
        p.error("PWM sweep checks require the V10.6SIMB2 diagnostic ELF")
    addresses = read_symbols(args.elf)
    active = [label for label, (name, _size, _signed) in SYMBOLS.items() if name in addresses]
    if args.announce_before_sweep_ms and "runtime_pwm_start_ms" not in active:
        raise SystemExit("Selected ELF lacks the runtime PWM start timestamp needed for an advance warning")
    adc_diag_labels = {
        "adc_diag_sample0", "adc_diag_sample1", "adc_diag_sample2", "adc_diag_sample3",
        "adc_diag_time0_q4", "adc_diag_time3_q4", "adc_diag_batch_count",
        "adc_diag_seq", "adc_diag_snapshot_request", "adc_diag_snapshot_ack",
        "adc_diag_vdd_mv", "adc_diag_vdd_sample_us_q4",
    }
    if args.expect_center_sweep and not adc_diag_labels.issubset(active):
        raise SystemExit("Selected ELF lacks the required four-sample ADC diagnostic telemetry")
    if args.arm_center_sweep and "adc_diag_start_request" not in active:
        raise SystemExit("Selected ELF cannot be armed by the SWD monitor")
    if args.arm_r04_sweep and "r04_sweep_start_request" not in active:
        raise SystemExit("Selected ELF lacks R04 sweep-arm telemetry")
    start = min(addresses[SYMBOLS[label][0]] for label in active)
    end = max(addresses[SYMBOLS[label][0]] + SYMBOLS[label][1] for label in active)
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
    print(f"SWD attached; target {core_start}; read-only capture started for {args.seconds:g}s. Potentiometer observation can begin now.", flush=True)
    trigger_written = False
    if args.arm_center_sweep:
        target.write32(addresses[SYMBOLS["adc_diag_start_request"][0]], 1)
        trigger_written = True
    if args.arm_r04_sweep:
        target.write32(addresses[SYMBOLS["r04_sweep_start_request"][0]], 1)
        trigger_written = True

    fields = ["wall_s"] + [k for k in active if k != "frame"] + list(FRAME) + [
        "tick_after", "input_age_ms", "vdd_age_ms", "vdd_to_adc_age_ms",
        "vdd_to_adc_age_q4",
        "adc_diag_vdd_age_start_q4", "adc_diag_vdd_age_end_q4",
        "frame_seq_consistent", "error_consistent",
        "vdd_compensation_input_adc", "feedback_mv_est", "feedback_norm_adc_3300",
        "adc_diag_span_03_q4",
        "adc_diag_batch_fresh", "adc_diag_seq_after", "adc_diag_batch_consistent"]
    fields += ["adc_diag_freeze_ack"]
    count = 0
    last_stage_stats = 0
    core_end = "unknown"
    first_frame_pwm_us = None
    max_frame_pwm_us = 0
    saw_sweep_peak = False
    saw_center_return = False
    diag_snapshot_request_addr = addresses.get("g_adc_diag_snapshot_request")
    diag_snapshot_ack_addr = addresses.get("g_adc_diag_snapshot_ack")
    diag_snapshot_hold = False
    adc_diag_batch_min = None
    adc_diag_batch_max = None
    adc_diag_consistent_rows = 0
    adc_diag_torn_rows = 0
    sweep_warning_sent = False
    wall_start = datetime.datetime.now().astimezone().isoformat()
    started = time.monotonic()
    try:
        with args.output.open("x", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            while time.monotonic() - started < args.seconds:
                sample_start = time.monotonic()
                row = {"wall_s": round(sample_start - started, 6)}
                if diag_snapshot_request_addr is not None and diag_snapshot_ack_addr is not None:
                    deadline = time.monotonic() + 1.0
                    while int(target.read32(diag_snapshot_ack_addr)) != 0:
                        if time.monotonic() >= deadline:
                            raise SystemExit("Timed out waiting for previous ADC snapshot release")
                        time.sleep(0.001)
                    target.write32(diag_snapshot_request_addr, 1)
                    diag_snapshot_hold = True
                    deadline = time.monotonic() + 1.0
                    while int(target.read32(diag_snapshot_ack_addr)) == 0:
                        if time.monotonic() >= deadline:
                            target.write32(diag_snapshot_request_addr, 0)
                            diag_snapshot_hold = False
                            raise SystemExit("Timed out waiting for coherent ADC diagnostic snapshot")
                        time.sleep(0.001)
                # Keep reads short: a full-RAM poll consumes much more SWD bus
                # time and makes it difficult to measure the running system.
                for labels in (
                    ("hw_raw_us", "hw_accepted_us", "hw_candidate_us", "latched_us"),
                    ("tick", "runtime_pwm_start_ms", "edge_count", "pulse_count", "input_valid", "last_edge_ms"),
                    ("source",),
                    ("control_duty", "control_state", "velocity_q4_live",
                     "traj_pos_q4_live", "traj_vel_q4_live"),
                    ("adaptive_confidence", "adaptive_gain_fwd_q8", "adaptive_gain_rev_q8",
                     "adaptive_samples_fwd", "adaptive_samples_rev", "model_record_valid"),
                    ("r01_diag_nonzero_request_count", "r01_diag_max_abs_request_duty",
                     "r01_diag_last_request_duty", "r01_diag_min_feedback_when_request",
                     "r01_diag_max_feedback_when_request", "r01_diag_min_vdd_when_request",
                     "r01_diag_max_vdd_when_request", "r01_diag_lock_reason",
                     "r01_diag_lock_feedback", "r01_diag_lock_error",
                     "r01_diag_lock_displacement", "r01_diag_control_adc"),
                    ("r01_stage_stats",),
                    ("adc_raw_last", "adc_vdd_compensated_last", "adc_filtered", "vrefint_raw", "vdd_mv", "vdd_sample_valid", "vdd_sample_ms",
                     "vdd_sample_us_q4", "adc_raw_sample_ms", "adc_raw_sample_us_q4"),
                    ("adc_diag_sample0", "adc_diag_sample1", "adc_diag_sample2", "adc_diag_sample3",
                     "adc_diag_vdd_mv",
                     "adc_diag_time0_q4", "adc_diag_time3_q4",
                     "adc_diag_batch_count", "adc_diag_seq", "adc_diag_vdd_sample_us_q4",
                     "adc_diag_snapshot_ack"),
                    ("r04_scheduler_overrun_contexts", "r04_scheduler_dropped_ticks",
                     "r04_scheduler_max_dropped_ticks"),
                    ("r04_timing_samples", "r04_timing_bin_0_lt500", "r04_timing_bin_1_500_899",
                     "r04_timing_bin_2_900_1100", "r04_timing_bin_3_1101_1500",
                     "r04_timing_bin_4_1501_2000", "r04_timing_bin_5_2001_4000",
                     "r04_timing_bin_6_4001_10000", "r04_timing_bin_7_gt10000",
                     "r04_timing_max_interval_us", "r04_timing_max_exec_us",
                     "r04_timing_exec_over_1000us"),
                ):
                    labels = tuple(label for label in labels if label in active)
                    if not labels:
                        continue
                    group_start = min(addresses[SYMBOLS[label][0]] for label in labels)
                    group_end = max(addresses[SYMBOLS[label][0]] + SYMBOLS[label][1] for label in labels)
                    data = bytes(target.read_memory_block8(group_start, group_end - group_start))
                    for label in labels:
                        symbol, size, signed = SYMBOLS[label]
                        row[label] = value(data, addresses[symbol] - group_start, size, signed)
                    if "adc_diag_seq" in labels:
                        row["adc_diag_seq_after"] = int(target.read32(addresses["g_adc_diag_seq"]))
                        row["adc_diag_freeze_ack"] = (int(target.read32(diag_snapshot_ack_addr))
                                                        if diag_snapshot_ack_addr is not None else 0)
                        row["adc_diag_batch_consistent"] = int(
                            row["adc_diag_freeze_ack"] == 1 and
                            diagnostic_snapshot_consistent(row["adc_diag_seq"], row["adc_diag_seq_after"]))
                        if diag_snapshot_request_addr is not None:
                            target.write32(diag_snapshot_request_addr, 0)
                            diag_snapshot_hold = False
                last_stage_stats = row.get("r01_stage_stats", last_stage_stats)
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
                if args.announce_before_sweep_ms and not sweep_warning_sent:
                    elapsed_ms = (row["tick_after"] - row["runtime_pwm_start_ms"]) & 0xffffffff
                    remaining_ms = 30000 - elapsed_ms
                    if elapsed_ms >= 38000:
                        message = f"R01_ACTION_WINDOW_EXPIRED elapsed_ms={elapsed_ms}; no visual observation is possible"
                        sweep_warning_sent = True
                    elif elapsed_ms >= 30000:
                        message = f"R01_ACTION_ALREADY_STARTED elapsed_ms={elapsed_ms}; watch now if motion is still visible"
                        sweep_warning_sent = True
                    elif remaining_ms <= args.announce_before_sweep_ms:
                        message = (f"R01_ACTION_IMMINENT elapsed_ms={elapsed_ms} remaining_ms={remaining_ms}; "
                                   "watch the mechanism now")
                        sweep_warning_sent = True
                    else:
                        message = None
                    if message:
                        print(
                            message,
                            flush=True,
                        )
                if "last_edge_ms" in row:
                    row["input_age_ms"] = (row["tick_after"] - row["last_edge_ms"]) & 0xffffffff
                else:
                    row["input_age_ms"] = ""
                if "vdd_sample_ms" in row:
                    row["vdd_age_ms"] = (row["tick_after"] - row["vdd_sample_ms"]) & 0xffffffff
                else:
                    row["vdd_age_ms"] = ""
                if "vdd_sample_ms" in row and "adc_raw_sample_ms" in row:
                    row["vdd_to_adc_age_ms"] = (row["adc_raw_sample_ms"] - row["vdd_sample_ms"]) & 0xffffffff
                    if row["vdd_to_adc_age_ms"] >= 0x80000000:
                        row["vdd_to_adc_age_ms"] -= 0x100000000
                else:
                    row["vdd_to_adc_age_ms"] = ""
                if "vdd_sample_us_q4" in row and "adc_raw_sample_us_q4" in row:
                    age_q4 = (row["adc_raw_sample_us_q4"] - row["vdd_sample_us_q4"]) & 0xffffffff
                    if age_q4 >= 0x80000000:
                        age_q4 -= 0x100000000
                    row["vdd_to_adc_age_q4"] = age_q4
                else:
                    row["vdd_to_adc_age_q4"] = ""
                row["frame_seq_consistent"] = int(row["seq_head"] == row["seq_tail"] and not (row["seq_head"] & 1))
                row["error_consistent"] = int(abs(row["error_adc"] - (row["target_adc"] - row["feedback_adc"])) <= 2)
                # Pair the latest live ADC with the most recent VDD in the same
                # SWD memory-block read; frame.raw_adc may be an older publish.
                # Diagnostic only: never feed this normalized value to control
                # until the potentiometer excitation/reference is confirmed.
                vdd_adc = row.get("adc_raw_last", row["raw_adc"])
                row["vdd_compensation_input_adc"] = vdd_adc
                row["feedback_mv_est"] = round(vdd_adc * row.get("vdd_mv", 0) / 4095.0, 3)
                row["feedback_norm_adc_3300"] = round(vdd_adc * row.get("vdd_mv", 0) / 3300.0, 3)
                if "adc_diag_batch_count" in row:
                    count_value = row["adc_diag_batch_count"]
                    seq_before = row.get("adc_diag_seq", 1)
                    seq_after = row.get("adc_diag_seq_after", seq_before)
                    if "adc_diag_batch_consistent" not in row:
                        row["adc_diag_seq_after"] = seq_after
                        row["adc_diag_batch_consistent"] = 0
                    adc_diag_batch_min = count_value if adc_diag_batch_min is None else min(adc_diag_batch_min, count_value)
                    adc_diag_batch_max = count_value if adc_diag_batch_max is None else max(adc_diag_batch_max, count_value)
                    row["adc_diag_span_03_q4"] = (row["adc_diag_time3_q4"] - row["adc_diag_time0_q4"]) & 0xFFFFFFFF
                    vdd_time = row.get("adc_diag_vdd_sample_us_q4", row.get("vdd_sample_us_q4"))
                    for label, sample_time in (("start", row["adc_diag_time0_q4"]),
                                               ("end", row["adc_diag_time3_q4"])):
                        if vdd_time is None:
                            row[f"adc_diag_vdd_age_{label}_q4"] = ""
                        else:
                            age = (sample_time - vdd_time) & 0xFFFFFFFF
                            row[f"adc_diag_vdd_age_{label}_q4"] = age if age < 0x80000000 else age - 0x100000000
                    row["adc_diag_batch_fresh"] = int(count_value > 0 and row["adc_diag_batch_consistent"])
                    adc_diag_consistent_rows += row["adc_diag_batch_consistent"]
                    adc_diag_torn_rows += 1 - row["adc_diag_batch_consistent"]
                if first_frame_pwm_us is None:
                    first_frame_pwm_us = row["frame_pwm_us"]
                max_frame_pwm_us = max(max_frame_pwm_us, row["frame_pwm_us"])
                saw_sweep_peak = saw_sweep_peak or (row["frame_pwm_us"] >= 1506)
                if saw_sweep_peak and row["frame_pwm_us"] <= 1504:
                    saw_center_return = True
                writer.writerow(row)
                f.flush()
                count += 1
                time.sleep(max(0.0, args.interval_ms / 1000.0 - (time.monotonic() - sample_start)))
        core_end = str(target.get_state())
    finally:
        if diag_snapshot_hold and diag_snapshot_request_addr is not None:
            try:
                target.write32(diag_snapshot_request_addr, 0)
            except Exception:
                pass
        session.close()
    center_sweep_sync = {
        "required": args.expect_center_sweep or args.expect_pwm_sweep,
        "first_frame_pwm_us": first_frame_pwm_us,
        "max_frame_pwm_us": max_frame_pwm_us,
        "saw_upward_peak": saw_sweep_peak,
        "saw_return_near_center": saw_center_return,
        "passed": (first_frame_pwm_us == 1503 and saw_sweep_peak and saw_center_return)
                  if (args.expect_center_sweep or args.expect_pwm_sweep) else None,
    }
    stage_names = (1503, 1504, 1505, 1506)
    packed_stats = int(last_stage_stats).to_bytes(64, "little")
    stage_summary = {}
    for index, pulse_us in enumerate(stage_names):
        values = [int.from_bytes(packed_stats[index * 16 + field * 2:index * 16 + field * 2 + 2], "little")
                  for field in range(8)]
        stage_summary[str(pulse_us)] = {
            "control_cycles": values[0] & 0x3FFF,
            "positive_bridge_duty_seen": bool(values[0] & 0x4000),
            "negative_bridge_duty_seen": bool(values[0] & 0x8000),
            "raw_adc_min": values[1], "raw_adc_max": values[2],
            "feedback_adc_min": values[3], "feedback_adc_max": values[4],
            "vdd_mv_min": values[5], "vdd_mv_max": values[6], "max_abs_bridge_duty": values[7],
        }
    internal_stage_sync = {
        "required": args.expect_center_sweep,
        "passed": all(stage_summary[str(pulse)]["control_cycles"] >= 500
                      for pulse in (1504, 1505, 1506)),
        "minimum_cycles_per_motion_step": 500,
    }
    center_sweep_sync["frame_publish_matches_sweep"] = center_sweep_sync["passed"]
    if args.expect_center_sweep:
        center_sweep_sync["passed"] = internal_stage_sync["passed"]
    report = {
        "elf": str(args.elf), "elf_sha256": hashlib.sha256(args.elf.read_bytes()).hexdigest(),
        "probe_uid": args.uid, "target": "py32f002ax5", "connect_mode": "attach",
        "core_start": core_start, "core_end": core_end, "samples": count,
        "start_time": wall_start,
        "monitor_sha256": monitor_sha256,
        "requested_interval_ms": args.interval_ms,
        "center_sweep_sync": center_sweep_sync,
        "internal_stage_sync": internal_stage_sync,
        "center_sweep_trigger_written_by_monitor": trigger_written,
        "per_command_stage_summary": stage_summary,
        "adc_diag_batch_count_min": adc_diag_batch_min,
        "adc_diag_batch_count_max": adc_diag_batch_max,
        "adc_diag_batches_advanced": (adc_diag_batch_max > 0) if adc_diag_batch_max is not None else False,
        "adc_diag_consistent_rows": adc_diag_consistent_rows,
        "adc_diag_torn_rows": adc_diag_torn_rows,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "symbol_addresses": {k: hex(v) for k, v in addresses.items()},
        "note": "SWD observer only; the firmware may generate its own internal center command sweep. Diagnostic request counters summarize nonzero software duty requests and do not prove physical bridge output.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    if args.expect_center_sweep and not internal_stage_sync["passed"]:
        raise SystemExit("Capture did not confirm all internal motion-step stages; evidence was saved.")
    if args.expect_pwm_sweep and not center_sweep_sync["passed"]:
        raise SystemExit("Capture failed PWM frame synchronization checks; evidence was saved.")
    if args.expect_center_sweep and not report["adc_diag_batches_advanced"]:
        raise SystemExit("Capture has no fresh four-sample ADC diagnostic batch")
    if args.expect_center_sweep and report["adc_diag_consistent_rows"] == 0:
        raise SystemExit("Capture has no coherent ADC diagnostic snapshots; inspect adc_diag_torn_rows")


if __name__ == "__main__":
    main()
