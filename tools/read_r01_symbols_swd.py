#!/usr/bin/env python3
"""Read selected R01 RAM symbols one at a time from the running TSSOP20 board."""

import argparse
import os
import runpy
import subprocess
from unittest.mock import patch


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--uid", required=True)
    parser.add_argument("--stop", action="store_true", help="stop an armed R01 loop and request coast")
    parser.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    parser.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    args = parser.parse_args()
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
        ("g_r01_center_live_adc", 2, False),
        ("g_r01_center_start_adc", 2, False),
        ("g_r01_center_target_adc", 2, False),
        ("g_r01_center_result", 1, False),
        ("s_r01_prod_motion_locked", 1, False),
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
    )
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
