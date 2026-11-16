#!/usr/bin/env python3
"""Check idle TIM1 one-pulse counting with all motor pins disconnected from TIM1.

Runs on the Ubuntu probe host against the force-coast diagnostic only. It does
not configure alternate-function pins or enable timer output channels.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import time
from unittest.mock import patch

RCC_APBENR2 = 0x40021040
TIM1 = 0x40012C00
GPIOA = 0x50000000
GPIOB = 0x50000400
TIM1_CLOCK = 1 << 11
OFFSETS = {"cr1": 0x00, "dier": 0x0C, "sr": 0x10, "ccmr1": 0x18,
           "ccer": 0x20, "cnt": 0x24, "psc": 0x28, "arr": 0x2C, "rcr": 0x30,
           "ccr2": 0x38, "bdtr": 0x44}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uid", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--release", type=Path, required=True,
                        help="Exact force-coast release currently on the board")
    parser.add_argument("--check-pb0-output", action="store_true",
                        help="Observe PB0 timer output while all other H-bridge pins stay low")
    parser.add_argument("--repeat-cycles", type=int, choices=[1, 10], default=1,
                        help="Use 1 or 10 hardware periods, totaling one second")
    parser.add_argument("--pulse-permille", type=int, choices=[80, 120, 160, 500], default=500,
                        help="Output high fraction of each slowed test period")
    parser.add_argument("--periodic-repetition", action="store_true",
                        help="Test repetition in continuous mode with outputs disabled")
    args = parser.parse_args()
    if args.periodic_repetition and (args.check_pb0_output or args.repeat_cycles != 10):
        parser.error("--periodic-repetition requires --repeat-cycles 10 and no GPIO output")
    if args.output.exists():
        raise SystemExit("Output already exists")
    manifest = json.loads((args.release / "manifest.json").read_text())
    image = (args.release / "py32f002_servo.bin").read_bytes()
    if not manifest.get("force_coast") or hashlib.sha256(image).hexdigest() != manifest["artifacts"]["py32f002_servo.bin"]["sha256"]:
        raise SystemExit("Selected release is not a verified force-coast image")
    os.environ["PYOCD_USB_BACKEND"] = "hidapiusb"
    with patch("runpy.run_module"):
        runpy.run_path("/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py")
    from pyocd.core.helpers import ConnectHelper

    session = ConnectHelper.session_with_chosen_probe(
        unique_id=args.uid, target_override="py32f002ax5", blocking=False,
        auto_open=False, options={"frequency": 1000000, "connect_mode": "attach",
        "pack": "/home/andrew/ARTERY/DQ110_FW/1.2.8.pack", "resume_on_disconnect": False,
        "cache.enable_memory": False, "cmsis_dap.prefer_v1": True,
        "cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False})
    if session is None:
        raise SystemExit("DAPLink not found")
    session.open()
    target = session.target
    clock_enabled = False
    saved = None
    report = {"samples": [], "gpio_outputs_enabled": args.check_pb0_output, "restored": False}
    pb0_changed = False
    try:
        if not str(target.get_state()).endswith("RUNNING"):
            raise RuntimeError("Target is not running")
        # Check the entire application before writing peripheral registers.
        observed = bytearray()
        for offset in range(0, len(image), 512):
            observed.extend(target.read_memory_block8(0x08000000 + offset, min(512, len(image) - offset)))
        if observed != image:
            raise RuntimeError("Board image does not match selected release")
        rcc = target.read32(RCC_APBENR2)
        mode_a, mode_b = target.read32(GPIOA), target.read32(GPIOB)
        odr_a, odr_b = target.read32(GPIOA + 0x14), target.read32(GPIOB + 0x14)
        if target.read32(GPIOA + 0x10) & 0x46 or target.read32(GPIOB + 0x10) & 1:
            raise RuntimeError("Motor GPIO input levels are not all low")
        if (rcc & TIM1_CLOCK or mode_b & 3 != 1 or
            any((mode_a >> (2 * pin)) & 3 != 1 for pin in (1, 2, 6)) or
            odr_a & 0x46 or odr_b & 1):
            raise RuntimeError("TIM1 not idle or motor GPIO is not coast; refusing")
        target.write32(RCC_APBENR2, rcc | TIM1_CLOCK)
        clock_enabled = True
        saved = {key: target.read32(TIM1 + offset) for key, offset in OFFSETS.items()}
        if saved["cr1"] & 1 or saved["dier"] or saved["ccer"] or saved["bdtr"] & 0x8000:
            raise RuntimeError("TIM1 has active counter, interrupt or output")
        # 24 MHz / 24000 = 1 kHz; 1000 ticks total in 1 or 10 periods.
        # No CCx output enable, MOE or pin alternate-function is set.
        target.write32(TIM1 + 0x00, 0 if args.periodic_repetition else 8)
        target.write32(TIM1 + 0x28, 23999)
        period_ticks = 1000 // args.repeat_cycles
        target.write32(TIM1 + 0x2C, period_ticks - 1)
        target.write32(TIM1 + 0x30, args.repeat_cycles - 1)
        target.write32(TIM1 + 0x38, period_ticks - period_ticks * args.pulse_permille // 1000)
        report["repeat_cycles"] = args.repeat_cycles
        report["pulse_permille"] = args.pulse_permille
        target.write32(TIM1 + 0x18, 7 << 12)  # OC2 PWM mode 2
        target.write32(TIM1 + 0x14, 1)  # UG: load prescaler
        target.write32(TIM1 + 0x10, 0)
        target.write32(TIM1 + 0x24, 0)
        if args.check_pb0_output:
            # CP1, NG1 and CP2 remain GPIO outputs low, so PB0 alone cannot
            # form either normal H-bridge drive combination.
            # With CC2E=0 and CC2NE=1, OC2N follows OC2REF when CC2NP=0.
            target.write32(TIM1 + 0x20, 0x40)  # CC2NE; active-high OC2N
            target.write32(TIM1 + 0x44, 0x8000)  # MOE
            afr = target.read32(GPIOB + 0x20)
            target.write32(GPIOB + 0x20, (afr & ~0xF) | 2)  # PB0 AF2
            target.write32(GPIOB, (target.read32(GPIOB) & ~3) | 2)  # AF mode
            pb0_changed = True
            if target.read32(GPIOB + 0x10) & 1:
                raise RuntimeError("PB0 was not low before start")
        started = time.monotonic()
        target.write32(TIM1 + 0x00, 1 if args.periodic_repetition else 9)
        while time.monotonic() - started < 1.5:
            report["samples"].append({"seconds": round(time.monotonic() - started, 4),
                "cr1": target.read32(TIM1), "cnt": target.read32(TIM1 + 0x24),
                "sr": target.read32(TIM1 + 0x10),
                "gpioa_idr": target.read32(GPIOA + 0x10) & 0x46,
                "pb0_idr": target.read32(GPIOB + 0x10) & 1})
            if report["samples"][-1]["gpioa_idr"]:
                raise RuntimeError("A second H-bridge input became high")
            time.sleep(0.005 if args.check_pb0_output else 0.05)
        if args.periodic_repetition:
            report["pass"] = (all(x["cr1"] & 1 for x in report["samples"]) and
                              any(x["seconds"] < 0.8 and not (x["sr"] & 1) for x in report["samples"]) and
                              bool(report["samples"][-1]["sr"] & 1))
        else:
            report["pass"] = (any(x["cr1"] & 1 for x in report["samples"]) and
                              any(x["sr"] & 4 for x in report["samples"]) and
                              bool(report["samples"][-1]["sr"] & 1) and
                              not (report["samples"][-1]["cr1"] & 1))
        report["motor_gpio_a_odr"] = hex(target.read32(GPIOA + 0x14) & 0x46)
        report["motor_gpio_b_odr"] = hex(target.read32(GPIOB + 0x14) & 1)
        report["pass"] = report["pass"] and report["motor_gpio_a_odr"] == "0x0" and report["motor_gpio_b_odr"] == "0x0"
        if args.check_pb0_output:
            report["pass"] = (report["pass"] and
                              any(x["pb0_idr"] == 1 for x in report["samples"]) and
                              report["samples"][-1]["pb0_idr"] == 0)
    finally:
        if clock_enabled:
            target.write32(TIM1, 0)
            target.write32(TIM1 + 0x20, 0)
            target.write32(TIM1 + 0x44, 0)
            if pb0_changed:
                target.write32(GPIOB + 0x28, 1)  # set GPIO output latch low
                target.write32(GPIOB, (target.read32(GPIOB) & ~3) | (mode_b & 3))
                target.write32(GPIOB + 0x20, (target.read32(GPIOB + 0x20) & ~0xF) | (afr & 0xF))
                report["pb0_restored"] = ((target.read32(GPIOB) & 3) == (mode_b & 3) and
                                          not (target.read32(GPIOB + 0x10) & 1))
            if saved is not None:
                for key in ("psc", "arr", "rcr", "ccr2", "ccmr1"):
                    target.write32(TIM1 + OFFSETS[key], saved[key])
                target.write32(TIM1 + 0x14, 1)  # reload the restored prescaler
                for key in ("cnt", "sr", "dier", "ccer", "bdtr", "cr1"):
                    target.write32(TIM1 + OFFSETS[key], saved[key])
                report["registers_restored"] = all(
                    target.read32(TIM1 + OFFSETS[key]) == saved[key] for key in OFFSETS)
            target.write32(RCC_APBENR2, target.read32(RCC_APBENR2) & ~TIM1_CLOCK)
            report["restored"] = (report.get("registers_restored", False) and
                                  (not pb0_changed or report.get("pb0_restored", False)) and
                                  not bool(target.read32(RCC_APBENR2) & TIM1_CLOCK))
        report["core_end"] = str(target.get_state())
        session.close()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    if not report.get("pass") or not report["restored"]:
        raise SystemExit("One-pulse timer check failed")


if __name__ == "__main__":
    main()
