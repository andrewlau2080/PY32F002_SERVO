#!/usr/bin/env python3
"""Temporarily scan PY32 HSI trim against the PA4 PWM diagnostic capture."""

from __future__ import annotations

import statistics
import time

from pyocd.core.helpers import ConnectHelper


RCC_ICSCR = 0x40021004
PWM_RAW_US = 0x2000007E
PWM_PERIOD_US = 0x2000035C
FACTORY_24MHZ_CAL = 0x9304


def sample(target, label: str) -> None:
    time.sleep(0.15)
    high_values: list[int] = []
    period_values: list[int] = []
    for _ in range(60):
        high_values.append(target.read_memory(PWM_RAW_US, 16))
        period = target.read_memory(PWM_PERIOD_US, 32)
        if 850 <= period <= 1100:
            period_values.append(period)
        time.sleep(0.008)

    print(
        label,
        "H",
        min(high_values),
        max(high_values),
        round(statistics.mean(high_values), 3),
        sorted(set(high_values)),
        "T",
        min(period_values),
        max(period_values),
        round(statistics.mean(period_values), 3),
        sorted(set(period_values)),
    )


def main() -> None:
    session = ConnectHelper.session_with_chosen_probe(
        target_override="py32f002ax5",
        blocking=False,
        auto_open=False,
        options={
            "frequency": 100000,
            "connect_mode": "attach",
            "cmsis_dap.prefer_v1": True,
            "cmsis_dap.limit_packets": True,
            "cmsis_dap.deferred_transfers": False,
        },
    )
    if session is None:
        raise SystemExit("No SWD probe found")

    session.open()
    target = session.target
    try:
        target.resume()
    except Exception:
        pass

    current = target.read_memory(RCC_ICSCR, 32)
    factory = (current & ~0xFFFF) | FACTORY_24MHZ_CAL
    target.write_memory(RCC_ICSCR, factory, 32)
    time.sleep(0.2)

    base = factory & 0xFFFF
    fs = base & 0xE000
    trim = base & 0x1FFF
    print("restored", hex(target.read_memory(RCC_ICSCR, 32)))
    print("fs", hex(fs), "trim", trim)

    for offset in [0, -1, -2, -3, -4, -5, -6, -8, -10, -12, -16, -20, -24, -32]:
        new_trim = max(0, min(0x1FFF, trim + offset))
        target.write_memory(RCC_ICSCR, (factory & ~0xFFFF) | fs | new_trim, 32)
        sample(target, f"off{offset:+d} trim{new_trim}")

    target.write_memory(RCC_ICSCR, factory, 32)
    print("final_restored", hex(target.read_memory(RCC_ICSCR, 32)))
    session.close()


if __name__ == "__main__":
    main()
