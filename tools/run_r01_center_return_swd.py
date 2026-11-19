#!/usr/bin/env python3
"""Arm one 1500 us low-end return test after checking live PA4 feedback."""

import argparse
import os
import runpy
import subprocess
import time
from unittest.mock import patch


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--uid", required=True)
    parser.add_argument("--pack", default="/home/andrew/ARTERY/DQ110_FW/1.2.8.pack")
    parser.add_argument("--compat", default="/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    args = parser.parse_args()
    symbols = {}
    for line in subprocess.check_output(["arm-none-eabi-nm", "-n", args.elf], text=True).splitlines():
        parts = line.split()
        if len(parts) >= 3:
            try:
                symbols[parts[-1]] = int(parts[0], 16)
            except ValueError:
                pass
    needed = ("g_r01_center_live_adc", "g_r01_center_start_adc",
              "g_r01_center_target_adc", "g_r01_center_result",
              "g_r01_active_test_arm_request", "g_r01_production_test_active")
    if not all(name in symbols for name in needed):
        raise SystemExit("Selected ELF is not the center-return image")
    os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
    with patch("runpy.run_module"):
        runpy.run_path(args.compat)
    from pyocd.core.helpers import ConnectHelper

    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.uid, target_override="py32f002ax5", blocking=False,
        auto_open=False, options={
            "frequency": 1000000, "connect_mode": "attach", "pack": args.pack,
            "resume_on_disconnect": False, "cache.enable_memory": False,
            "cache.enable_register": False, "cmsis_dap.prefer_v1": True,
            "cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False,
        },
    )
    if session is None:
        raise SystemExit("DAPLink not found")
    session.open()
    target = session.target

    def read(name, size):
        return int.from_bytes(bytes(target.read_memory_block8(symbols[name], size)), "little")

    def read_live_consistent():
        values = []
        for _ in range(7):
            try:
                value = read("g_r01_center_live_adc", 2)
                if 0 <= value <= 4095:
                    values.append(value)
            except Exception:
                pass
            time.sleep(0.02)
        if len(values) < 3:
            raise SystemExit(f"Live ADC read is inconsistent: {values}")
        values.sort()
        middle = values[len(values) // 2]
        if sum(abs(value - middle) <= 12 for value in values) < 3:
            raise SystemExit(f"Live ADC read is inconsistent: {values}")
        return middle

    try:
        if not str(target.get_state()).endswith("RUNNING"):
            raise SystemExit("Target is not running")
        time.sleep(0.1)
        start = read_live_consistent()
        target_adc = read("g_r01_center_target_adc", 2)
        arm_state = read("g_r01_active_test_arm_request", 4)
        print(f"LIVE ADC={start}, TARGET={target_adc}, ARM={arm_state}", flush=True)
        if not 200 <= start <= 900 or target_adc == 0 or arm_state != 0:
            print("START REJECTED: board is not at the measured low end or test is already used", flush=True)
            return
        print("1500 us center-return will start in 8 seconds. Observe the motor now.", flush=True)
        time.sleep(8)
        # The firmware checks its live ADC again when it consumes this arm word.
        # Long-lived SWD sessions can fail reads after the observation delay.
        target.write32(symbols["g_r01_active_test_arm_request"], 0x52303141)
        print("STARTED 1500 us center-return", flush=True)
        deadline = time.monotonic() + 11
        next_report = 0
        while time.monotonic() < deadline:
            time.sleep(0.25)
            try:
                live = read("g_r01_center_live_adc", 2)
                result = read("g_r01_center_result", 1)
                now = time.monotonic()
                if now >= next_report or result >= 2:
                    print(f"ADC={live}, TARGET={target_adc}, RESULT={result}", flush=True)
                    next_report = now + 1
                if result >= 2:
                    break
            except Exception as exc:
                print(f"SWD_READ_ERROR={type(exc).__name__}; firmware self-stops", flush=True)
                time.sleep(0.5)
        print("TEST_WINDOW_ENDED", flush=True)
    finally:
        try:
            session.close()
        except Exception as exc:
            print(f"PROBE_DISCONNECT_ERROR={type(exc).__name__}", flush=True)


if __name__ == "__main__":
    main()
