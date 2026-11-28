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
    if cfg["arm_motion_phase"]:
        arm = cfg["symbols"].get("g_lcdm_motion_phase_arm_request", [None])[0]
        internal = cfg["symbols"].get("g_lcdm_motion_phase_use_internal_pwm", [None])[0]
        armed = cfg["symbols"].get("s_lcdm_motion_phase_armed", [None])[0]
        active = cfg["symbols"].get("s_lcdm_servo_customer_active", [None])[0]
        duty = cfg["symbols"].get("s_lcdm_servo_current_duty", [None])[0]
        if None in (arm, internal, armed, active, duty):
            raise SystemExit("Selected ELF lacks motion-phase arm/status symbols")
        current = {"core": str(target.get_state()),
                   "arm_request": target.read32(arm),
                   "armed": target.read8(armed),
                   "active": target.read8(active),
                   "duty": int.from_bytes(target.read_memory_block8(duty, 2), "little", signed=True)}
        if ("running" not in current["core"].lower() or current["arm_request"] != 0 or
                current["armed"] != 0 or current["active"] != 0 or current["duty"] != 0):
            raise SystemExit("Motion-phase arm refused unless core is running and phase/duty are idle: " +
                             json.dumps(current))
        from_pulse = cfg["motion_phase_arm_from_us"]
        if from_pulse is not None:
            pulse = cfg["symbols"].get("s_lcdm_servo_last_control_pulse_us", [None])[0]
            if pulse is None or target.read8(internal) != 1:
                raise SystemExit("Directional arm requires the internal source already enabled and visible")
            # Arm just after the chosen plateau begins, so the next edge has
            # the intended direction. Finish all SWD reads before arming.
            previous = target.read16(pulse)
            until = time.monotonic() + 9.0
            while time.monotonic() < until:
                time.sleep(0.1)
                current_pulse = target.read16(pulse)
                if current_pulse == from_pulse and previous != from_pulse:
                    break
                previous = current_pulse
            else:
                raise SystemExit("Requested starting plateau was not observed; no motion armed")
        else:
            target.write8(internal, 1)
        target.write32(arm, 0x50484153)
        print(json.dumps({"event": "MOTION_PHASE_ARM_SENT", "before": current,
                          "from_pulse_us": from_pulse,
                          "internal_pwm_source": "configured alternating command; verify input_pulse_us telemetry"}), flush=True)
    if cfg["inject_touch_jog"]:
        direction = cfg["inject_touch_jog"]
        required = ("g_r01_production_test_active", "g_r01_active_test_arm_request",
                    "s_duty", "s_touch_x", "s_touch_y", "s_touch_event",
                    "s_touch_seq", "s_touch_consumed_seq")
        if any(cfg["symbols"].get(name, [None])[0] is None for name in required):
            raise SystemExit("Selected ELF lacks guarded LCDM touch-jog symbols")
        addr = {name: cfg["symbols"][name][0] for name in required}
        active = target.read8(addr["g_r01_production_test_active"])
        arm = target.read32(addr["g_r01_active_test_arm_request"])
        duty = int.from_bytes(target.read_memory_block8(addr["s_duty"], 2), "little", signed=True)
        seq = target.read8(addr["s_touch_seq"])
        consumed = target.read8(addr["s_touch_consumed_seq"])
        core = str(target.get_state())
        before = {"core": core, "active": active, "arm_request": arm,
                  "duty": duty, "touch_seq": seq, "touch_consumed_seq": consumed}
        if ("running" not in core.lower() or active != 0 or duty != 0 or
                arm not in (0, 2) or seq != consumed):
            raise SystemExit("Touch-jog injection refused unless core is running, coasted, disarmed, and touch queue empty: " +
                             json.dumps(before))
        x, y = (406, 81) if direction == "fwd" else (406, 135)
        target.write_memory_block8(addr["s_touch_x"], list(x.to_bytes(2, "little")))
        target.write_memory_block8(addr["s_touch_y"], list(y.to_bytes(2, "little")))
        target.write_memory_block8(addr["s_touch_event"], [1])
        target.write_memory_block8(addr["s_touch_seq"], [(seq + 1) & 0xff])
        print(json.dumps({"event": "R01_TOUCH_JOG_INJECTED", "direction": direction,
                          "coordinates": [x, y], "before": before}), flush=True)
        time.sleep(0.24)
    if cfg["redraw_r01_lcd"]:
        required = ("g_r01_production_test_active", "s_duty", "page_drawn.7",
                    "s_page", "s_tjc_tx_cmd_count")
        if any(cfg["symbols"].get(name, [None])[0] is None for name in required):
            raise SystemExit("Selected ELF lacks R01 LCD redraw symbols")
        addr = {name: cfg["symbols"][name][0] for name in required}
        active = target.read8(addr["g_r01_production_test_active"])
        duty = int.from_bytes(target.read_memory_block8(addr["s_duty"], 2), "little", signed=True)
        page = target.read8(addr["s_page"])
        tx_before = target.read32(addr["s_tjc_tx_cmd_count"])
        core = str(target.get_state())
        if "running" not in core.lower() or active != 0 or duty != 0 or page != 0:
            raise SystemExit("LCD redraw refused unless core is running, motor coasted, and page 0 active")
        target.write_memory_block8(addr["page_drawn.7"], [0])
        time.sleep(0.60)
        print(json.dumps({"event": "R01_LCD_REDRAW_REQUESTED", "before":
                          {"core": core, "active": active, "duty": duty, "page": page,
                           "tx_count": tx_before}, "after":
                          {"core": str(target.get_state()),
                           "page_drawn": target.read8(addr["page_drawn.7"]),
                           "tx_count": target.read32(addr["s_tjc_tx_cmd_count"])}}), flush=True)
    if cfg["enable_motion_phase_input"]:
        internal = cfg["symbols"].get("g_lcdm_motion_phase_use_internal_pwm", [None])[0]
        armed = cfg["symbols"].get("s_lcdm_motion_phase_armed", [None])[0]
        active = cfg["symbols"].get("s_lcdm_servo_customer_active", [None])[0]
        duty = cfg["symbols"].get("s_lcdm_servo_current_duty", [None])[0]
        arm = cfg["symbols"].get("g_lcdm_motion_phase_arm_request", [None])[0]
        if None in (internal, armed, active, duty, arm):
            raise SystemExit("Selected ELF lacks motion-phase input/status symbols")
        current = {"core": str(target.get_state()), "arm_request": target.read32(arm),
                   "internal_pwm": target.read8(internal), "armed": target.read8(armed),
                   "active": target.read8(active),
                   "duty": int.from_bytes(target.read_memory_block8(duty, 2), "little", signed=True)}
        if ("running" not in current["core"].lower() or current["arm_request"] != 0 or
                current["armed"] != 0 or current["active"] != 0 or current["duty"] != 0):
            raise SystemExit("Internal input enable refused unless core is running and motion is disarmed/coasted: " +
                             json.dumps(current))
        target.write8(internal, 1)
        print(json.dumps({"event": "MOTION_PHASE_INPUT_ENABLED", "before": current,
                          "motor_arm": "unchanged/disarmed"}), flush=True)
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
                elif name == "g_r01_touch_jog_trace":
                    count = min(32, int.from_bytes(target.read_memory_block8(
                        cfg["symbols"]["g_r01_touch_jog_trace_count"][0], 1)[0:1], "little"))
                    out["symbols"][name] = [list(data[i * 16:(i + 1) * 16]) for i in range(count)]
                elif name == "g_lcdm_motion_phase_trace":
                    count = min(24, int.from_bytes(target.read_memory_block8(
                        cfg["symbols"]["g_lcdm_motion_phase_trace_count"][0], 1)[0:1], "little"))
                    records = []
                    for i in range(count):
                        row = data[i * 12:(i + 1) * 12]
                        records.append({
                            "elapsed_ms": row[0] + (256 if row[11] & 0x80 else 0),
                            "raw_adc": int.from_bytes(row[1:3], "little"),
                            "feedback_adc": int.from_bytes(row[3:5], "little"),
                            "velocity_q4": int.from_bytes(row[5:7], "little", signed=True),
                            "requested_duty": int.from_bytes(row[7:9], "little", signed=True),
                            "final_duty": int.from_bytes(row[9:11], "little", signed=True),
                            "phase": row[11] & 0x7f,
                        })
                    out["symbols"][name] = records
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
    parser.add_argument("--motion-phase", action="store_true",
                        help="read only the limited LCDM motion-phase telemetry symbols")
    parser.add_argument("--arm-motion-phase", action="store_true",
                        help="enable the configured internal alternating PWM and one-shot guarded motion after zero-duty checks")
    parser.add_argument("--arm-motion-phase-only", action="store_true",
                        help="arm once and disconnect immediately so SWD polling cannot interrupt the motion loop")
    parser.add_argument("--motion-phase-arm-from-us", type=int,
                        help="arm after this internal PWM plateau begins, then disconnect")
    parser.add_argument("--enable-motion-phase-input", action="store_true",
                        help="enable the configured internal alternating command source only; refuses unless motion is disarmed/coasted")
    parser.add_argument("--inject-touch-jog", choices=("fwd", "rev"),
                        help="inject one decoded LCDM jog press; requires a running, coasted, disarmed touch-jog image")
    parser.add_argument("--redraw-r01-lcd", action="store_true",
                        help="request a safe page-0 redraw; refuses unless motor is coasted")
    parser.add_argument("--summary", action="store_true", help="summarize repeated remote snapshots instead of printing every frame")
    parser.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    parser.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    args = parser.parse_args()
    if args.arm_motion_phase_only and not args.arm_motion_phase:
        parser.error("--arm-motion-phase-only requires --arm-motion-phase")
    if args.motion_phase_arm_from_us is not None and (
            not args.arm_motion_phase_only or not 500 <= args.motion_phase_arm_from_us <= 2500):
        parser.error("--motion-phase-arm-from-us requires arm-only mode and a pulse within 500..2500 us")
    if args.samples < (0 if args.arm_motion_phase_only else 1) or args.interval_ms < 10:
        parser.error("samples must be positive (or zero for arm-only) and interval-ms at least 10")
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
        ("g_lcdm_motion_phase_arm_request", 4, False),
        ("g_lcdm_motion_phase_use_internal_pwm", 1, False),
        ("g_lcdm_motion_phase_trace_count", 1, False),
        ("g_lcdm_motion_phase_trace", 24 * 12, False),
        ("g_lcdm_motion_phase_trace_start_ms", 4, False),
        ("g_lcdm_motion_phase_requested_duty", 2, True),
        ("g_lcdm_motion_phase_final_duty", 2, True),
        ("g_lcdm_motion_phase_error_adc", 2, True),
        ("g_lcdm_motion_phase_feedback_valid", 1, False),
        ("g_lcdm_motion_phase_active_loops", 2, False),
        ("g_lcdm_motion_phase_seen_mask", 1, False),
        ("g_lcdm_motion_phase_request_min", 2, True),
        ("g_lcdm_motion_phase_request_max", 2, True),
        ("g_lcdm_motion_phase_final_min", 2, True),
        ("g_lcdm_motion_phase_final_max", 2, True),
        ("g_lcdm_motion_phase_error_min", 2, True),
        ("g_lcdm_motion_phase_error_max", 2, True),
        ("g_lcdm_motion_phase_feedback_min", 2, False),
        ("g_lcdm_motion_phase_feedback_max", 2, False),
        ("g_lcdm_motion_phase_valid_loops", 2, False),
        ("g_lcdm_motion_phase_exit_reason", 1, False),
        ("g_lcdm_motion_phase_exit_phase", 1, False),
        ("g_lcdm_motion_phase_exit_state", 1, False),
        ("g_lcdm_motion_phase_exit_elapsed_ms", 2, False),
        ("g_lcdm_motion_phase_exit_feedback_adc", 2, False),
        ("g_lcdm_motion_phase_exit_hbridge_duty", 2, True),
        ("g_lcdm_motion_phase_exit_count", 2, False),
        ("g_lcdm_motion_phase_target_adc", 2, False),
        ("g_lcdm_motion_phase_stop_flags", 1, False),
        ("g_r04_scheduler_dropped_tick_count", 4, False),
        ("g_r04_scheduler_max_dropped_ticks", 2, False),
        ("g_r04_scheduler_overrun_context_nibbles", 2, False),
        ("g_motion_phase_forward_pwm_ticks", 4, False),
        ("g_motion_phase_reverse_pwm_ticks", 4, False),
        ("g_motion_phase_forward_pa6_high_ticks", 4, False),
        ("g_motion_phase_forward_pb0_high_ticks", 4, False),
        ("g_motion_phase_forward_pad_mismatch_ticks", 4, False),
        ("s_lcdm_motion_phase", 1, False),
        ("s_lcdm_motion_phase_armed", 1, False),
        ("s_lcdm_motion_phase_start_adc", 2, False),
        ("s_lcdm_servo_customer_active", 1, False),
        ("s_lcdm_servo_last_control_pulse_us", 2, False),
        ("s_lcdm_servo_filtered_feedback_adc", 2, False),
        ("s_lcdm_servo_cmd_target_adc", 2, False),
        ("s_lcdm_servo_vel_q4", 4, True),
        ("s_last_raw", 2, False),
        ("s_duty", 2, True),
        ("s_state", 1, False),
        ("s_tjc_tx_cmd_count", 4, False),
        ("s_page", 1, False),
        ("s_tjc_debug_page", 1, False),
        ("page_drawn.7", 1, False),
        ("s_tjc_rx_byte_count", 4, False),
        ("s_tjc_rx_packet_count", 4, False),
        ("s_rx_byte", 1, False),
        ("s_rx_len", 1, False),
        ("s_rx_ff_count", 1, False),
        ("s_r01_touch_jog_direction", 1, True),
        ("s_r01_touch_jog_anchor_adc", 2, False),
        ("s_r01_touch_jog_start_ms", 4, False),
        ("uwTick", 4, False),
        ("s_tjc_last_packet_len", 1, False),
        ("s_tjc_last_packet0", 1, False),
        ("s_tjc_last_packet1", 1, False),
        ("s_tjc_last_packet2", 1, False),
        ("s_touch_x", 2, False),
        ("s_touch_y", 2, False),
        ("s_touch_event", 1, False),
        ("s_touch_seq", 1, False),
        ("s_touch_consumed_seq", 1, False),
        ("g_r01_touch_jog_trace_count", 1, False),
        ("g_r01_touch_jog_trace_start_ms", 4, False),
        ("g_r01_touch_jog_trace", 512, False),
        ("g_r01_bridge_forward_on_ticks", 4, False),
        ("g_r01_bridge_reverse_on_ticks", 4, False),
        ("g_motion_phase_forward_pa6_high_ticks", 4, False),
        ("g_motion_phase_forward_pb0_high_ticks", 4, False),
        ("g_motion_phase_forward_pad_mismatch_ticks", 4, False),
        ("s_tjc_tx_error_count", 4, False),
        ("s_tjc_uart_error_count", 4, False),
    )
    if args.motion_phase or args.arm_motion_phase or args.enable_motion_phase_input or args.inject_touch_jog or args.redraw_r01_lcd:
        phase_names = {
            "g_lcdm_motion_phase_arm_request", "g_lcdm_motion_phase_use_internal_pwm",
            "g_lcdm_motion_phase_trace_count", "g_lcdm_motion_phase_trace",
            "g_lcdm_motion_phase_trace_start_ms",
            "g_lcdm_motion_phase_requested_duty", "g_lcdm_motion_phase_final_duty",
            "g_lcdm_motion_phase_error_adc", "g_lcdm_motion_phase_feedback_valid",
            "g_lcdm_motion_phase_active_loops", "g_lcdm_motion_phase_seen_mask",
            "g_lcdm_motion_phase_request_min", "g_lcdm_motion_phase_request_max",
            "g_lcdm_motion_phase_final_min", "g_lcdm_motion_phase_final_max",
            "g_lcdm_motion_phase_error_min", "g_lcdm_motion_phase_error_max",
            "g_lcdm_motion_phase_feedback_min", "g_lcdm_motion_phase_feedback_max",
            "g_lcdm_motion_phase_valid_loops", "g_lcdm_motion_phase_exit_reason",
            "g_lcdm_motion_phase_exit_phase", "g_lcdm_motion_phase_exit_state",
            "g_lcdm_motion_phase_exit_elapsed_ms",
            "g_lcdm_motion_phase_exit_feedback_adc",
            "g_lcdm_motion_phase_exit_hbridge_duty",
            "g_lcdm_motion_phase_exit_count", "g_lcdm_motion_phase_target_adc",
            "g_lcdm_motion_phase_stop_flags",
            "g_r04_scheduler_dropped_tick_count",
            "g_r04_scheduler_max_dropped_ticks",
            "g_r04_scheduler_overrun_context_nibbles",
            "g_motion_phase_forward_pwm_ticks", "g_motion_phase_reverse_pwm_ticks",
            "g_motion_phase_forward_pa6_high_ticks", "g_motion_phase_forward_pb0_high_ticks",
            "g_motion_phase_forward_pad_mismatch_ticks",
            "g_r01_touch_jog_trace_count", "g_r01_touch_jog_trace_start_ms",
            "g_r01_touch_jog_trace", "g_r01_bridge_forward_on_ticks",
            "g_r01_bridge_reverse_on_ticks", "s_touch_x", "s_touch_y",
            "s_touch_event", "s_touch_seq", "s_touch_consumed_seq",
            "g_r01_production_test_active", "g_r01_active_test_arm_request",
            "s_last_raw",
            "page_drawn.7", "s_page", "s_tjc_debug_page",
            "s_tjc_tx_cmd_count",
            "s_lcdm_motion_phase", "s_lcdm_motion_phase_armed",
            "s_lcdm_motion_phase_start_adc", "s_lcdm_servo_customer_active",
            "s_lcdm_servo_last_control_pulse_us", "s_lcdm_servo_filtered_feedback_adc",
            "s_lcdm_servo_cmd_target_adc", "s_lcdm_servo_vel_q4",
            "s_lcdm_servo_current_duty", "s_lcdm_servo_current_state",
            "s_duty", "s_state",
            "s_tjc_tx_cmd_count", "s_tjc_tx_error_count", "s_tjc_uart_error_count",
            "s_last_raw", "s_vdd_mv"
        }
        names = tuple(item for item in names if item[0] in phase_names)
    if not sys.platform.startswith("linux"):
        if args.stop and ("g_r01_production_test_active" not in addresses or
                          "g_r01_active_test_arm_request" not in addresses):
            raise SystemExit("Selected ELF lacks R01 stop symbols")
        cfg = {"uid": args.uid, "pack": args.pack, "compat": args.compat,
               "stop": args.stop, "arm_motion": args.arm_r01_motion,
               "inject_touch_jog": args.inject_touch_jog,
               "redraw_r01_lcd": args.redraw_r01_lcd,
               "arm_motion_phase": args.arm_motion_phase,
               "motion_phase_arm_from_us": args.motion_phase_arm_from_us,
               "enable_motion_phase_input": args.enable_motion_phase_input,
               "samples": 0 if args.arm_motion_phase_only else args.samples,
               "interval_ms": args.interval_ms,
               "symbols": {name: [addresses.get(name), size, signed]
                           for name, size, signed in names}}
        encoded = base64.b64encode(json.dumps(cfg).encode()).decode()
        host = os.getenv("SERVO_REMOTE_HOST", "andrew@10.211.55.4")
        python = os.getenv("SERVO_REMOTE_PYTHON", "/home/andrew/ARTERY/.tools/pyocd-venv/bin/python")
        command = shlex.join([python, "-", encoded])
        result = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", host, command],
                                input=REMOTE_READ, text=True, capture_output=True,
                                timeout=max(60, args.samples * 0.65 + 30))
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
                    "arm_event": next((row for row in rows if row.get("event") in
                                       ("R01_ARM_SENT", "MOTION_PHASE_ARM_SENT")), None),
                    "frames": len(frames),
                    "duration_s": round(frames[-1]["monotonic_s"] - frames[0]["monotonic_s"], 3) if len(frames) > 1 else 0,
                    "core_states": sorted({row.get("core") for row in frames}),
                    "motion_phase_values": sorted(set(series("s_lcdm_motion_phase"))),
                    "motion_active_values": sorted(set(series("s_lcdm_servo_customer_active"))),
                    "motion_requested_duty_abs_max": max((abs(v) for v in series("g_lcdm_motion_phase_requested_duty")), default=0),
                    "motion_final_duty_abs_max": max((abs(v) for v in series("g_lcdm_motion_phase_final_duty")), default=0),
                    "motion_feedback_valid_values": sorted(set(series("g_lcdm_motion_phase_feedback_valid"))),
                    "input_pulse_us": sorted(set(series("s_lcdm_servo_last_control_pulse_us"))),
                    "motion_error_adc_min_max": [min(series("g_lcdm_motion_phase_error_adc")), max(series("g_lcdm_motion_phase_error_adc"))] if series("g_lcdm_motion_phase_error_adc") else [],
                    "motion_active_loops": max(series("g_lcdm_motion_phase_active_loops"), default=0),
                    "motion_phase_seen_mask": sorted(set(series("g_lcdm_motion_phase_seen_mask"))),
                    "motion_trace": next((row["symbols"].get("g_lcdm_motion_phase_trace") for row in reversed(frames)
                                          if row["symbols"].get("g_lcdm_motion_phase_trace")), []),
                    "motion_request_duty_min_max": [min(series("g_lcdm_motion_phase_request_min")), max(series("g_lcdm_motion_phase_request_max"))] if series("g_lcdm_motion_phase_request_min") and series("g_lcdm_motion_phase_request_max") else [],
                    "motion_final_duty_min_max": [min(series("g_lcdm_motion_phase_final_min")), max(series("g_lcdm_motion_phase_final_max"))] if series("g_lcdm_motion_phase_final_min") and series("g_lcdm_motion_phase_final_max") else [],
                    "motion_error_range_min_max": [min(series("g_lcdm_motion_phase_error_min")), max(series("g_lcdm_motion_phase_error_max"))] if series("g_lcdm_motion_phase_error_min") and series("g_lcdm_motion_phase_error_max") else [],
                    "motion_feedback_range_min_max": [min(series("g_lcdm_motion_phase_feedback_min")), max(series("g_lcdm_motion_phase_feedback_max"))] if series("g_lcdm_motion_phase_feedback_min") and series("g_lcdm_motion_phase_feedback_max") else [],
                    "motion_valid_loops": max(series("g_lcdm_motion_phase_valid_loops"), default=0),
                    "forward_pwm_ticks_start_end": [series("g_motion_phase_forward_pwm_ticks")[0], series("g_motion_phase_forward_pwm_ticks")[-1]] if series("g_motion_phase_forward_pwm_ticks") else [],
                    "reverse_pwm_ticks_start_end": [series("g_motion_phase_reverse_pwm_ticks")[0], series("g_motion_phase_reverse_pwm_ticks")[-1]] if series("g_motion_phase_reverse_pwm_ticks") else [],
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
