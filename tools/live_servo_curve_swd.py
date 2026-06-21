#!/usr/bin/env python3
"""Live SWD sampler for servo curve variables.

This reads the g_servo_curve_* RAM variables from the running target through
pyOCD, writes a CSV log, and refreshes a small HTML/SVG dashboard.
"""

from __future__ import annotations

import argparse
import csv
import html
import math
import random
import subprocess
import sys
import time
from collections import deque
from dataclasses import dataclass
from pathlib import Path


CURVE_VARS = [
    ("time_ms", "g_servo_curve_time_ms", "u32"),
    ("pwm_us", "g_servo_curve_pwm_us", "u16"),
    ("pwm_raw_us", "g_servo_curve_pwm_raw_us", "u16"),
    ("pwm_period_us", "g_servo_curve_pwm_period_us", "u16"),
    ("raw_adc", "g_servo_curve_raw_adc", "u16"),
    ("target_adc", "g_servo_curve_target_adc", "u16"),
    ("feedback_adc", "g_servo_curve_feedback_adc", "u16"),
    ("error_adc", "g_servo_curve_error_adc", "i16"),
    ("duty", "g_servo_curve_duty", "i16"),
    ("velocity_q4", "g_servo_curve_velocity_q4", "i16"),
    ("ref_velocity", "g_servo_curve_ref_velocity", "i16"),
    ("traj_adc", "g_servo_curve_traj_adc", "i16"),
    ("traj_vel_q4", "g_servo_curve_traj_vel_q4", "i16"),
    ("overshoot_adc", "g_servo_curve_overshoot_adc", "u16"),
    ("pwm_candidate_us", "g_servo_curve_pwm_candidate_us", "u16"),
    ("state", "g_servo_curve_state", "u8"),
    ("event", "g_servo_curve_event", "u8"),
    ("pwm_candidate_count", "g_servo_curve_pwm_candidate_count", "u8"),
]

CURVE_SYNC_HEAD = "g_servo_curve_seq_head"
CURVE_SYNC_TAIL = "g_servo_curve_seq_tail"
CURVE_FRAME_SYMBOL = "g_servo_curve_frame"
CURVE_FRAME_SIZE = 44
CURVE_FRAME_OFFSETS = {
    "seq_head": (0, "u32"),
    "time_ms": (4, "u32"),
    "pwm_us": (8, "u16"),
    "pwm_raw_us": (10, "u16"),
    "pwm_period_us": (12, "u16"),
    "raw_adc": (14, "u16"),
    "target_adc": (16, "u16"),
    "feedback_adc": (18, "u16"),
    "error_adc": (20, "i16"),
    "duty": (22, "i16"),
    "velocity_q4": (24, "i16"),
    "ref_velocity": (26, "i16"),
    "traj_adc": (28, "i16"),
    "traj_vel_q4": (30, "i16"),
    "overshoot_adc": (32, "u16"),
    "pwm_candidate_us": (34, "u16"),
    "state": (36, "u8"),
    "event": (37, "u8"),
    "pwm_candidate_count": (38, "u8"),
    "seq_tail": (40, "u32"),
}

POSITION_SERIES = [
    ("目标AD", "target_adc", "#d62728"),
    ("反馈AD", "feedback_adc", "#1f77b4"),
    ("轨迹AD", "traj_adc", "#2ca02c"),
]

CONTROL_SERIES = [
    ("误差", "error_adc", "#d62728"),
    ("H桥占空比", "duty", "#1f77b4"),
    ("AD速度q4", "velocity_q4", "#9467bd"),
    ("目标速度", "ref_velocity", "#ff7f0e"),
]

HBRIDGE_EVENT_COLORS = {
    4: "#e377c2",
    5: "#17becf",
    6: "#bcbd22",
    9: "#8c564b",
    11: "#2ca02c",
    14: "#17becf",
    15: "#8c564b",
    16: "#2ca02c",
    17: "#ff7f0e",
    18: "#d62728",
}
HBRIDGE_PRETRIGGER_MS = 260
HBRIDGE_POST_MIN_MS = 220
HBRIDGE_POST_MAX_MS = 1400
HBRIDGE_HOLD_MS = 220
HBRIDGE_PWM_TRIGGER_US = 4
HBRIDGE_TARGET_TRIGGER_ADC = 3


def hbridge_find_pwm_trigger(rows: list[dict[str, int]]) -> tuple[int, int]:
    if not rows:
        return 0, 1
    previous_pwm = int(rows[0]["pwm_us"])
    previous_target = int(rows[0]["target_adc"])
    for row in rows[1:]:
        pwm_delta = int(row["pwm_us"]) - previous_pwm
        target_delta = int(row["target_adc"]) - previous_target
        if abs(pwm_delta) >= HBRIDGE_PWM_TRIGGER_US or abs(target_delta) >= HBRIDGE_TARGET_TRIGGER_ADC:
            command_dir = 1
            if target_delta > 0:
                command_dir = 1
            elif target_delta < 0:
                command_dir = -1
            elif pwm_delta < 0:
                command_dir = -1
            return int(row["time_ms"]), command_dir
        previous_pwm = int(row["pwm_us"])
        previous_target = int(row["target_adc"])
    for row in rows[1:]:
        duty = int(row["duty"])
        if duty != 0 or int(row["event"]) in HBRIDGE_EVENT_COLORS:
            return int(row["time_ms"]), 1 if duty >= 0 else -1
    return int(rows[0]["time_ms"]), 1


class HBridgeSnapshot:
    def __init__(self, interval_ms: int) -> None:
        self.interval_ms = max(1, interval_ms)
        self.pretrigger: deque[dict[str, int]] = deque(
            maxlen=max(30, HBRIDGE_PRETRIGGER_MS // self.interval_ms)
        )
        self.rows: list[dict[str, int]] = []
        self.active = False
        self.trigger_time_ms = 0
        self.hold_count = 0
        self.sequence = 0
        self.dirty = False
        self.last_pwm_us: int | None = None
        self.last_target_adc: int | None = None

    @staticmethod
    def row_has_hbridge_activity(row: dict[str, int]) -> bool:
        return int(row["duty"]) != 0 or int(row["event"]) in HBRIDGE_EVENT_COLORS

    def row_has_external_pwm_change(self, row: dict[str, int]) -> bool:
        if self.last_pwm_us is None or self.last_target_adc is None:
            return False
        pwm_delta = int(row["pwm_us"]) - self.last_pwm_us
        target_delta = int(row["target_adc"]) - self.last_target_adc
        return abs(pwm_delta) >= HBRIDGE_PWM_TRIGGER_US or abs(target_delta) >= HBRIDGE_TARGET_TRIGGER_ADC

    def remember_command(self, row: dict[str, int]) -> None:
        self.last_pwm_us = int(row["pwm_us"])
        self.last_target_adc = int(row["target_adc"])

    def add(self, row: dict[str, int]) -> bool:
        external_pwm_change = self.row_has_external_pwm_change(row)
        self.remember_command(row)

        hbridge_activity = self.row_has_hbridge_activity(row)
        if not self.active and (external_pwm_change or hbridge_activity):
            self.active = True
            self.sequence += 1
            self.rows = list(self.pretrigger)
            self.rows.append(row)
            self.trigger_time_ms = int(row["time_ms"])
            self.hold_count = 0
            return False

        if self.active:
            self.rows.append(row)
            if int(row["event"]) == 2 and int(row["duty"]) == 0:
                self.hold_count += 1
            else:
                self.hold_count = 0

            elapsed_ms = int(row["time_ms"]) - self.trigger_time_ms
            hold_ready = self.hold_count >= max(8, HBRIDGE_HOLD_MS // self.interval_ms)
            if ((elapsed_ms >= HBRIDGE_POST_MIN_MS) and hold_ready) or (elapsed_ms >= HBRIDGE_POST_MAX_MS):
                self.active = False
                self.dirty = True
                return True
            return False

        self.pretrigger.append(row)
        return False

    def take_dirty_rows(self) -> list[dict[str, int]] | None:
        if self.dirty and len(self.rows) >= 2:
            self.dirty = False
            return list(self.rows)
        return None


@dataclass
class CurveVar:
    label: str
    symbol: str
    kind: str
    address: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Live plot servo curve variables through SWD/pyOCD.")
    parser.add_argument("--elf", default="build/py32f002_servo.elf")
    parser.add_argument("--target", default="py32f002ax5")
    parser.add_argument("--frequency", type=int, default=1_000_000)
    parser.add_argument("--interval-ms", type=int, default=5)
    parser.add_argument("--window", type=int, default=1200)
    parser.add_argument("--output-dir", default="build/servo_curve_live")
    parser.add_argument("--pyocd-backend", default="hidapiusb")
    parser.add_argument("--pack", default=None, help="Optional CMSIS-Pack path for pyOCD target support.")
    parser.add_argument("--demo", action="store_true", help="Generate simulated data without hardware.")
    parser.add_argument("--stream-csv", action="store_true", help="Write sampled rows to stdout for a host UI.")
    parser.add_argument("--duration-sec", type=float, default=0.0, help="Stop automatically after this many seconds; 0 means run until Ctrl+C.")
    return parser.parse_args()


def run_nm(elf: Path) -> dict[str, int]:
    output = subprocess.check_output(["arm-none-eabi-nm", "-n", str(elf)], text=True)
    symbols: dict[str, int] = {}
    wanted = {symbol for _label, symbol, _kind in CURVE_VARS}
    optional = {CURVE_SYNC_HEAD, CURVE_SYNC_TAIL, CURVE_FRAME_SYMBOL}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2] in wanted | optional:
            symbols[parts[2]] = int(parts[0], 16)
    missing = sorted(wanted - set(symbols))
    if missing:
        raise SystemExit("Missing symbols in ELF: " + ", ".join(missing))
    return symbols


def open_target(args: argparse.Namespace):
    if args.pyocd_backend:
        import os

        os.environ["PYOCD_USB_BACKEND"] = args.pyocd_backend

    from pyocd.core.helpers import ConnectHelper

    options = {
        "frequency": args.frequency,
        "connect_mode": "attach",
        "cache.enable_memory": False,
        "cache.enable_register": False,
        "cmsis_dap.prefer_v1": True,
        "cmsis_dap.limit_packets": True,
        "cmsis_dap.deferred_transfers": False,
    }
    if args.pack:
        options["pack"] = args.pack

    session = ConnectHelper.session_with_chosen_probe(
        target_override=args.target,
        blocking=False,
        auto_open=False,
        options=options,
    )
    if session is None:
        raise SystemExit("No SWD probe found.")
    session.open()
    try:
        session.target.resume()
    except Exception:
        pass
    return session


def sign_extend(value: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (value & (sign - 1)) - (value & sign)


def kind_size(kind: str) -> int:
    if kind in ("u8",):
        return 1
    if kind in ("u16", "i16"):
        return 2
    if kind in ("u32",):
        return 4
    raise ValueError(kind)


def decode_value(data: list[int], offset: int, kind: str) -> int:
    if kind == "u8":
        return int(data[offset])
    if kind in ("u16", "i16"):
        value = int(data[offset]) | (int(data[offset + 1]) << 8)
        return sign_extend(value, 16) if kind == "i16" else value
    if kind == "u32":
        return (
            int(data[offset])
            | (int(data[offset + 1]) << 8)
            | (int(data[offset + 2]) << 16)
            | (int(data[offset + 3]) << 24)
        )
    raise ValueError(kind)


def read_value(target, var: CurveVar) -> int:
    if var.kind == "u32":
        return int(target.read32(var.address))
    if var.kind == "u16":
        return int(target.read16(var.address))
    if var.kind == "i16":
        return sign_extend(int(target.read16(var.address)), 16)
    if var.kind == "u8":
        return int(target.read8(var.address))
    raise ValueError(var.kind)


def sample_target_individual(target, variables: list[CurveVar]) -> dict[str, int]:
    return {var.label: read_value(target, var) for var in variables}


def sample_target_block(target, variables: list[CurveVar], symbols: dict[str, int]) -> dict[str, int] | None:
    head_addr = symbols.get(CURVE_SYNC_HEAD)
    tail_addr = symbols.get(CURVE_SYNC_TAIL)
    if head_addr is None or tail_addr is None:
        return sample_target_individual(target, variables)

    items = [(head_addr, "seq_head", "u32"), (tail_addr, "seq_tail", "u32")]
    items.extend((var.address, var.label, var.kind) for var in variables)
    base = min(address for address, _label, _kind in items)
    end = max(address + kind_size(kind) for address, _label, kind in items)
    length = end - base

    for _attempt in range(8):
        head_before = int(target.read32(head_addr))
        if head_before & 1:
            time.sleep(0.001)
            continue
        data = list(target.read_memory_block8(base, length))
        head_after = int(target.read32(head_addr))
        tail_after = int(target.read32(tail_addr))
        if (head_before != head_after) or (head_after != tail_after) or (head_after & 1):
            time.sleep(0.001)
            continue
        decoded = {
            label: decode_value(data, address - base, kind)
            for address, label, kind in items
        }
        head = decoded["seq_head"]
        tail = decoded["seq_tail"]
        row = {var.label: decoded[var.label] for var in variables}
        expected_error = int(row["target_adc"]) - int(row["feedback_adc"])
        frame_is_stable = (head == tail) and ((head & 1) == 0)
        error_is_stable = abs(int(row["error_adc"]) - expected_error) <= 2
        if frame_is_stable and error_is_stable:
            return row
        time.sleep(0.001)

    return None


def sample_target_frame(target, frame_addr: int) -> dict[str, int] | None:
    fallback: dict[str, int] | None = None
    for _attempt in range(8):
        data = list(target.read_memory_block8(frame_addr, CURVE_FRAME_SIZE))
        decoded = {
            label: decode_value(data, offset, kind)
            for label, (offset, kind) in CURVE_FRAME_OFFSETS.items()
        }
        head = decoded["seq_head"]
        tail = decoded["seq_tail"]
        row = {label: decoded[label] for label, _symbol, _kind in CURVE_VARS}
        expected_error = int(row["target_adc"]) - int(row["feedback_adc"])
        if abs(int(row["error_adc"]) - expected_error) <= 4:
            fallback = row
        if (head != tail) or (head & 1):
            time.sleep(0.001)
            continue
        if abs(int(row["error_adc"]) - expected_error) <= 2:
            return row
        time.sleep(0.001)
    return fallback


def demo_sample(index: int) -> dict[str, int]:
    phase = (index // 180) % 4
    targets = [1200, 2400, 1800, 3100]
    target = targets[phase]
    local = index % 180
    previous = targets[(phase - 1) % 4]
    k = min(1.0, local / 70.0)
    feedback = int(previous + (target - previous) * (1.0 - math.exp(-4.0 * k)))
    feedback += int(math.sin(local / 5.0) * max(0, 45 - local) / 3)
    error = target - feedback
    moving = abs(error) > 8
    return {
        "time_ms": index * 5,
        "pwm_us": 1000 + phase * 300,
        "raw_adc": feedback + random.randint(-2, 2),
        "target_adc": target,
        "feedback_adc": feedback,
        "error_adc": error,
        "duty": max(-260, min(260, error // 2)) if moving else 0,
        "velocity_q4": int((target - previous) / 50) if moving else 0,
        "ref_velocity": int((target - previous) / 80) if moving else 0,
        "traj_adc": int(previous + (target - previous) * min(1.0, local / 80.0)),
        "traj_vel_q4": int((target - previous) / 70) if moving else 0,
        "overshoot_adc": max(0, -error if (target > previous and error < 0) else error if (target < previous and error > 0) else 0),
        "state": 2 if moving else 1,
        "event": 1 if moving else 2,
    }


def value_range(rows: list[dict[str, int]], keys: list[str]) -> tuple[float, float]:
    values = [float(row[key]) for row in rows for key in keys if key in row]
    if not values:
        return 0.0, 1.0
    low, high = min(values), max(values)
    if low == high:
        pad = max(1.0, abs(low) * 0.05)
        return low - pad, high + pad
    pad = (high - low) * 0.08
    return low - pad, high + pad


def polyline(rows: list[dict[str, int]], key: str, rect: tuple[int, int, int, int], y_min: float, y_max: float) -> str:
    left, top, width, height = rect
    x_min = float(rows[0]["time_ms"])
    x_max = float(rows[-1]["time_ms"])
    if x_max == x_min:
        x_max = x_min + 1.0
    if y_max == y_min:
        y_max = y_min + 1.0
    points = []
    for row in rows:
        x = left + (float(row["time_ms"]) - x_min) * width / (x_max - x_min)
        y = top + height - (float(row[key]) - y_min) * height / (y_max - y_min)
        points.append(f"{x:.1f},{y:.1f}")
    return " ".join(points)


def y_for_value(value: float, top: int, height: int, y_min: float, y_max: float) -> float:
    if y_max == y_min:
        y_max = y_min + 1.0
    return top + height - (value - y_min) * height / (y_max - y_min)


def x_for_time(row: dict[str, int], rows: list[dict[str, int]], left: int, width: int) -> float:
    time_key = "t_rel_ms" if "t_rel_ms" in rows[0] else "time_ms"
    x_min = float(rows[0][time_key])
    x_max = float(rows[-1][time_key])
    if x_max == x_min:
        x_max = x_min + 1.0
    return left + (float(row[time_key]) - x_min) * width / (x_max - x_min)


def panel(title: str, rows: list[dict[str, int]], series: list[tuple[str, str, str]], rect: tuple[int, int, int, int]) -> list[str]:
    left, top, width, height = rect
    keys = [key for _name, key, _color in series]
    y_min, y_max = value_range(rows, keys)
    out = [
        f'<text x="{left}" y="{top - 12}" font-size="15" font-weight="700">{html.escape(title)}</text>',
        f'<rect x="{left}" y="{top}" width="{width}" height="{height}" fill="#fff" stroke="#ccc"/>',
    ]
    for i in range(1, 4):
        y = top + height * i / 4
        out.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left + width}" y2="{y:.1f}" stroke="#eee"/>')
    legend_x = left + 8
    for name, key, color in series:
        out.append(f'<polyline points="{polyline(rows, key, rect, y_min, y_max)}" fill="none" stroke="{color}" stroke-width="2"/>')
        out.append(f'<text x="{legend_x}" y="{top + 18}" font-size="12" fill="{color}">{name}</text>')
        legend_x += 82
    out.append(f'<text x="{left + width - 150}" y="{top - 12}" font-size="11" fill="#666">{y_min:.0f}..{y_max:.0f}</text>')
    return out


def hbridge_response_panel(rows: list[dict[str, int]], rect: tuple[int, int, int, int]) -> list[str]:
    left, top, width, height = rect
    adc_height = int(height * 0.58)
    gap = 58
    drive_top = top + adc_height + gap
    drive_height = height - adc_height - gap
    adc_rect = (left, top, width, adc_height)
    drive_rect = (left, drive_top, width, drive_height)
    out = [
        f'<text x="{left}" y="{top - 12}" font-size="15" font-weight="700">H桥PWM与AD响应</text>',
        f'<rect x="{left}" y="{top}" width="{width}" height="{adc_height}" fill="#fff" stroke="#ccc"/>',
        f'<rect x="{left}" y="{drive_top}" width="{width}" height="{drive_height}" fill="#fff" stroke="#ccc"/>',
    ]

    adc_position_keys = ["target_adc", "feedback_adc", "raw_adc", "traj_adc"]
    adc_min, adc_max = value_range(rows, adc_position_keys)
    duty_key = "hbridge_rel_duty" if "hbridge_rel_duty" in rows[0] else "duty"
    duty_limit = max(100.0, max(abs(float(row[duty_key])) for row in rows))
    rate_keys = ["adc_step_x16", "velocity_q4", "ref_velocity"]
    rate_limit = max(16.0, max(abs(float(row[key])) for row in rows for key in rate_keys if key in row))
    duty_y_min, duty_y_max = -duty_limit * 1.08, duty_limit * 1.08
    rate_y_min, rate_y_max = -rate_limit * 1.08, rate_limit * 1.08
    duty_zero_y = y_for_value(0.0, drive_top, drive_height, duty_y_min, duty_y_max)
    rate_zero_y = y_for_value(0.0, drive_top, drive_height, rate_y_min, rate_y_max)

    def add_y_grid(panel_top: int, panel_height: int, low: float, high: float, label: str) -> None:
        for i in range(5):
            y = panel_top + panel_height * i / 4
            value = high - (high - low) * i / 4
            out.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left + width}" y2="{y:.1f}" stroke="#eee"/>')
            out.append(f'<text x="{left - 8}" y="{y + 4:.1f}" font-size="11" fill="#666" text-anchor="end">{value:.0f}</text>')
        out.append(f'<text x="{left + width - 128}" y="{panel_top - 8}" font-size="11" fill="#666">{html.escape(label)} {low:.0f}..{high:.0f}</text>')

    add_y_grid(top, adc_height, adc_min, adc_max, "AD计数")
    add_y_grid(drive_top, drive_height, duty_y_min, duty_y_max, "H桥PWM")
    out.append(f'<line x1="{left}" y1="{duty_zero_y:.1f}" x2="{left + width}" y2="{duty_zero_y:.1f}" stroke="#555" stroke-width="1"/>')
    if abs(rate_zero_y - duty_zero_y) > 1.0:
        out.append(f'<line x1="{left}" y1="{rate_zero_y:.1f}" x2="{left + width}" y2="{rate_zero_y:.1f}" stroke="#999" stroke-width="1" stroke-dasharray="4,4"/>')

    use_relative_time = "t_rel_ms" in rows[0]
    t_min = -50.0 if use_relative_time else float(rows[0]["time_ms"])
    t_last = float(rows[-1]["t_rel_ms"] if use_relative_time else rows[-1]["time_ms"])
    if use_relative_time:
        t_max = max(50.0, math.ceil(max(0.0, t_last) / 50.0) * 50.0)
    else:
        t_max = float(rows[-1]["time_ms"])

    def hx(row: dict[str, int]) -> float:
        value = float(row["t_rel_ms"] if use_relative_time else row["time_ms"])
        return left + (value - t_min) * width / (t_max - t_min if t_max != t_min else 1.0)

    if use_relative_time:
        tick = 0
        while tick <= int(t_max):
            x = left + (float(tick) - t_min) * width / (t_max - t_min if t_max != t_min else 1.0)
            out.append(f'<line x1="{x:.1f}" y1="{top}" x2="{x:.1f}" y2="{drive_top + drive_height}" stroke="#f0f0f0"/>')
            out.append(f'<text x="{x:.1f}" y="{drive_top + drive_height + 20}" font-size="11" fill="#666" text-anchor="middle">{tick} ms</text>')
            tick += 50
    else:
        for i in range(7):
            t_value = t_min + (t_max - t_min) * i / 6
            x = left + width * i / 6
            out.append(f'<line x1="{x:.1f}" y1="{top}" x2="{x:.1f}" y2="{drive_top + drive_height}" stroke="#f0f0f0"/>')
            out.append(f'<text x="{x:.1f}" y="{drive_top + drive_height + 20}" font-size="11" fill="#666" text-anchor="middle">{t_value:.0f} ms</text>')
    if use_relative_time:
        x0 = left + (0.0 - t_min) * width / (t_max - t_min if t_max != t_min else 1.0)
        out.append(f'<line x1="{x0:.1f}" y1="{top}" x2="{x0:.1f}" y2="{drive_top + drive_height}" stroke="#111" stroke-width="2" opacity="0.55"/>')
        out.append(f'<text x="{x0 + 6:.1f}" y="{top + 16}" font-size="12" fill="#111">0ms PWM/H桥触发点</text>')
    elif t_min <= 0.0 <= t_max:
        x0 = left + (0.0 - t_min) * width / (t_max - t_min if t_max != t_min else 1.0)
        out.append(f'<line x1="{x0:.1f}" y1="{top}" x2="{x0:.1f}" y2="{drive_top + drive_height}" stroke="#111" stroke-width="2" opacity="0.55"/>')
        out.append(f'<text x="{x0 + 6:.1f}" y="{top + 16}" font-size="12" fill="#111">0ms PWM/H桥触发点</text>')

    step_width = max(1.0, width / max(1, len(rows) - 1) * 0.72)
    last_duty_label_time: float | None = None
    last_duty_label_value: int | None = None
    for row in rows:
        x = hx(row)
        event = int(row["event"])
        if event in HBRIDGE_EVENT_COLORS:
            color = HBRIDGE_EVENT_COLORS[event]
            out.append(
                f'<line x1="{x:.1f}" y1="{top}" x2="{x:.1f}" y2="{drive_top + drive_height}" '
                f'stroke="{color}" stroke-width="1" opacity="0.18"/>'
            )
        duty = float(row[duty_key])
        if duty != 0.0:
            y = y_for_value(duty, drive_top, drive_height, duty_y_min, duty_y_max)
            rect_y = min(y, duty_zero_y)
            rect_h = max(1.0, abs(duty_zero_y - y))
            out.append(
                f'<rect x="{x - (step_width / 2):.1f}" y="{rect_y:.1f}" '
                f'width="{step_width:.1f}" height="{rect_h:.1f}" fill="#1f77b4" opacity="0.32"/>'
            )
            duty_i = int(duty)
            row_time = float(row["t_rel_ms"] if use_relative_time else row["time_ms"])
            should_label = (
                abs(duty_i) >= 40 and
                (
                    last_duty_label_time is None or
                    abs(duty_i - (last_duty_label_value or 0)) >= 80 or
                    (row_time - last_duty_label_time) >= 50
                )
            )
            if should_label:
                pct = duty_i / 10.0
                label_y = rect_y - 5 if duty > 0 else rect_y + rect_h + 13
                label_y = max(drive_top + 28, min(drive_top + drive_height - 6, label_y))
                direction_text = "同向" if duty_i > 0 else "反向"
                out.append(
                    f'<text x="{x:.1f}" y="{label_y:.1f}" font-size="11" fill="#0b4f8a" '
                    f'text-anchor="middle">{direction_text}{duty_i:+d} {pct:+.0f}%</text>'
                )
                last_duty_label_time = row_time
                last_duty_label_value = duty_i

    adc_position_series = [
        ("target_adc", "目标AD", "#d62728", "5,3"),
        ("feedback_adc", "反馈AD", "#1f77b4", ""),
        ("raw_adc", "原始AD", "#7f7f7f", "2,3"),
        ("traj_adc", "轨迹AD", "#2ca02c", "6,3"),
    ]
    for key, _name, color, dash in adc_position_series:
        points = []
        for row in rows:
            x = hx(row)
            y = y_for_value(float(row[key]), top, adc_height, adc_min, adc_max)
            points.append(f"{x:.1f},{y:.1f}")
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        out.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="2"{dash_attr}/>')

    rate_series = [
        ("adc_step_x16", "AD单次变化x16", "#d62728", "3,3"),
        ("velocity_q4", "AD速度q4", "#9467bd", ""),
        ("ref_velocity", "目标速度", "#ff7f0e", "6,3"),
    ]
    for key, _name, color, dash in rate_series:
        points = []
        for row in rows:
            x = hx(row)
            y = y_for_value(float(row[key]), drive_top, drive_height, rate_y_min, rate_y_max)
            points.append(f"{x:.1f},{y:.1f}")
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        out.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="2"{dash_attr}/>')

    top_legend = [
        ("目标AD", "#d62728"),
        ("反馈AD", "#1f77b4"),
        ("原始AD", "#7f7f7f"),
        ("轨迹AD", "#2ca02c"),
    ]
    legend_x = left + 8
    for label, color in top_legend:
        out.append(f'<text x="{legend_x}" y="{top + 18}" font-size="12" fill="{color}">{html.escape(label)}</text>')
        legend_x += 122

    bottom_legend = [
        ("H桥PWM：同向为正/反向为负", "#1f77b4"),
        ("AD单次变化x16", "#d62728"),
        ("AD速度q4", "#9467bd"),
        ("目标速度", "#ff7f0e"),
        ("刹车/反打事件", "#17becf"),
    ]
    legend_x = left + 8
    for label, color in bottom_legend:
        out.append(f'<text x="{legend_x}" y="{drive_top + 18}" font-size="12" fill="{color}">{html.escape(label)}</text>')
        legend_x += 165

    out.append(f'<text x="{left + width - 430}" y="{drive_top - 8}" font-size="11" fill="#666">相对目标方向PWM占空比 +/-{duty_limit:.0f} | 单次AD变化/速度 +/-{rate_limit:.0f}</text>')
    out.append(f'<text x="{left + width - 385}" y="{drive_top + drive_height + 42}" font-size="11" fill="#666">事件标记：4过冲刹车，5接近刹车，6低占空比刹车，9过冲返回，11尾段微调</text>')
    return out


def write_hbridge_response_svg(path: Path, rows: list[dict[str, int]], title: str) -> None:
    if len(rows) < 2:
        return
    plotted_rows = rows_with_adc_step(rows, relative_to_hbridge_trigger=True)
    plotted_rows = [row for row in plotted_rows if int(row.get("t_rel_ms", 0)) >= 0]
    if len(plotted_rows) < 2:
        return
    plotted_rows[0]["adc_step"] = 0
    plotted_rows[0]["adc_step_x16"] = 0
    latest = plotted_rows[-1]
    max_forward_duty = max(0, max(int(row["hbridge_rel_duty"]) for row in plotted_rows))
    max_reverse_duty = max(0, max(-int(row["hbridge_rel_duty"]) for row in plotted_rows))
    brake_events = sum(1 for row in plotted_rows if int(row["event"]) in HBRIDGE_EVENT_COLORS)
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="2400" height="900" viewBox="0 0 2400 900">',
        '<rect width="100%" height="100%" fill="#f7f7f7"/>',
        f'<text x="60" y="42" font-size="24" font-weight="700">{html.escape(title)}</text>',
        f'<text x="60" y="70" font-size="13" fill="#555">控制PWM {latest["pwm_us"]}us | 原始PWM {latest.get("pwm_raw_us", latest["pwm_us"])}us | 周期 {latest.get("pwm_period_us", 0)}us | 候选 {latest.get("pwm_candidate_us", 0)}/{latest.get("pwm_candidate_count", 0)} | 目标AD {latest["target_adc"]} | 反馈AD {latest["feedback_adc"]} | H桥 {latest["duty"]} | AD步进 {latest["adc_step"]} | 事件 {latest["event"]} | 同向峰值 {max_forward_duty} | 反向峰值 {max_reverse_duty}</text>',
    ]
    svg.extend(hbridge_response_panel(plotted_rows, (60, 120, 2280, 640)))
    svg.append('<text x="60" y="840" font-size="13" fill="#444">触发快照：外部PWM变化或静止外力触发H桥输出时从0ms开始记录，0ms左侧预留50ms；无触发不刷新。</text>')
    svg.append('<text x="60" y="862" font-size="13" fill="#444">蓝色柱为H桥PWM：触发方向在正半轴，反向/回拉在负半轴；红色虚线为AD单次变化x16，即相邻两点反馈AD差值乘16。</text>')
    svg.append("</svg>")
    path.write_text("\n".join(svg), encoding="utf-8")
    path.with_suffix(".version").write_text(str(time.time_ns()), encoding="utf-8")


def rows_with_adc_step(rows: list[dict[str, int]], relative_to_hbridge_trigger: bool = False) -> list[dict[str, int]]:
    plotted: list[dict[str, int]] = []
    previous_feedback: int | None = None
    trigger_time_ms = int(rows[0]["time_ms"]) if rows else 0
    command_dir = 1
    if relative_to_hbridge_trigger:
        trigger_time_ms, command_dir = hbridge_find_pwm_trigger(rows)
    for row in rows:
        item = dict(row)
        feedback = int(item["feedback_adc"])
        item["adc_step"] = 0 if previous_feedback is None else feedback - previous_feedback
        item["adc_step_x16"] = int(item["adc_step"]) * 16
        if relative_to_hbridge_trigger:
            item["t_rel_ms"] = int(item["time_ms"]) - trigger_time_ms
            item["hbridge_rel_duty"] = int(item["duty"]) * command_dir
        previous_feedback = feedback
        plotted.append(item)
    return plotted


def write_svg(path: Path, rows: list[dict[str, int]], title: str) -> None:
    if len(rows) < 2:
        return
    plotted_rows = rows_with_adc_step(rows)
    latest = plotted_rows[-1]
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="1040" viewBox="0 0 1120 1040">',
        '<rect width="100%" height="100%" fill="#f7f7f7"/>',
        f'<text x="70" y="38" font-size="22" font-weight="700">{html.escape(title)}</text>',
        f'<text x="70" y="62" font-size="12" fill="#555">控制PWM {latest["pwm_us"]}us | 原始PWM {latest.get("pwm_raw_us", latest["pwm_us"])}us | 周期 {latest.get("pwm_period_us", 0)}us | 候选 {latest.get("pwm_candidate_us", 0)}/{latest.get("pwm_candidate_count", 0)} | 目标AD {latest["target_adc"]} | 反馈AD {latest["feedback_adc"]} | H桥 {latest["duty"]} | AD步进 {latest["adc_step"]} | 事件 {latest["event"]}</text>',
    ]
    svg.extend(panel("位置曲线", plotted_rows, POSITION_SERIES, (70, 90, 980, 230)))
    svg.extend(panel("控制量", plotted_rows, CONTROL_SERIES, (70, 380, 980, 230)))
    svg.extend(hbridge_response_panel(plotted_rows, (70, 670, 980, 260)))
    svg.append('<text x="70" y="985" font-size="12" fill="#444">事件：2保持，12驱动，13减速，14预测刹车，15受限回位，16静止外力锁止，17近点阻尼，18过点吸能</text>')
    svg.append("</svg>")
    path.write_text("\n".join(svg), encoding="utf-8")


def write_html(path: Path, svg_name: str, csv_name: str, cycle_name: str | None, interval_ms: int) -> None:
    cycle_line = (
        f'最近动作: <a href="{html.escape(cycle_name)}">{html.escape(cycle_name)}</a>'
        if cycle_name
        else "最近动作: 等待PWM变化"
    )
    path.write_text(
        f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>舵机实时曲线</title>
  <style>
    body {{ margin: 0; font-family: Arial, sans-serif; background: #f7f7f7; color: #222; }}
    .bar {{ padding: 10px 14px; background: #fff; border-bottom: 1px solid #ddd; font-size: 14px; }}
    a {{ color: #0b61a4; }}
    img {{ display: block; width: 100%; max-width: 1120px; margin: 0 auto; }}
    #stamp {{ color: #666; margin-left: 12px; }}
  </style>
</head>
<body>
  <div class="bar">
    主曲线刷新: 1秒 | 采样间隔: {interval_ms}ms |
    CSV: <a href="{html.escape(csv_name)}">{html.escape(csv_name)}</a> |
    H桥大图: <a href="hbridge_response.html">hbridge_response.html</a> |
    {cycle_line}
    <span id="stamp"></span>
  </div>
  <img id="curve" src="{html.escape(svg_name)}?t={time.time():.0f}" alt="舵机实时曲线">
  <script>
    const svgName = "{html.escape(svg_name)}";
    function reloadCurve() {{
      const now = Date.now();
      document.getElementById("curve").src = svgName + "?t=" + now;
      document.getElementById("stamp").textContent = "主曲线刷新: " + new Date(now).toLocaleTimeString();
    }}
    setInterval(reloadCurve, 1000);
  </script>
</body>
</html>
""",
        encoding="utf-8",
    )


def write_hbridge_html(path: Path, svg_name: str, csv_name: str, interval_ms: int) -> None:
    path.write_text(
        f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>H桥PWM与AD响应</title>
  <style>
    body {{ margin: 0; font-family: Arial, sans-serif; background: #f7f7f7; color: #222; }}
    .bar {{ padding: 10px 14px; background: #fff; border-bottom: 1px solid #ddd; font-size: 14px; }}
    a {{ color: #0b61a4; }}
    .viewport {{ width: 100vw; overflow-x: auto; }}
    img {{ display: block; width: 2400px; max-width: none; margin: 0; }}
    #stamp {{ color: #666; margin-left: 12px; }}
  </style>
</head>
<body>
  <div class="bar">
    PWM/外力快照: 仅外部PWM变化或静止外力触发后刷新 | 采样间隔: {interval_ms}ms |
    CSV: <a href="{html.escape(csv_name)}">{html.escape(csv_name)}</a> |
    主曲线: <a href="servo_curve_live.html">servo_curve_live.html</a>
    <span id="stamp"></span>
  </div>
  <div class="viewport">
  <img id="curve" src="{html.escape(svg_name)}" alt="H桥PWM与AD响应">
  </div>
  <script>
    const svgName = "{html.escape(svg_name)}";
    const versionName = svgName.replace(/\\.svg$/, ".version");
    let currentVersion = "";
    async function pollVersion() {{
      try {{
        const response = await fetch(versionName + "?t=" + Date.now(), {{ cache: "no-store" }});
        if (!response.ok) {{
          return;
        }}
        const version = (await response.text()).trim();
        if (version && version !== currentVersion) {{
          currentVersion = version;
          document.getElementById("curve").src = svgName + "?v=" + encodeURIComponent(version);
          document.getElementById("stamp").textContent = "快照刷新: " + new Date().toLocaleTimeString();
        }}
      }} catch (_err) {{
      }}
    }}
    pollVersion();
    setInterval(pollVersion, 1000);
  </script>
</body>
</html>
""",
        encoding="utf-8",
    )


def write_csv_header(path: Path) -> None:
    if path.exists():
        return
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=[label for label, _symbol, _kind in CURVE_VARS])
        writer.writeheader()


def append_csv(path: Path, row: dict[str, int]) -> None:
    with path.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=[label for label, _symbol, _kind in CURVE_VARS])
        writer.writerow(row)


def write_cycle_csv(path: Path, rows: list[dict[str, int]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=[label for label, _symbol, _kind in CURVE_VARS])
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    args = parse_args()
    variables: list[CurveVar] = []
    session = None
    if args.demo:
        target = None
    else:
        symbols = run_nm(Path(args.elf))
        variables = [CurveVar(label, symbol, kind, symbols[symbol]) for label, symbol, kind in CURVE_VARS]
        frame_addr = symbols.get(CURVE_FRAME_SYMBOL)
        session = open_target(args)
        target = session.target

    if args.stream_csv:
        writer = csv.DictWriter(sys.stdout, fieldnames=[label for label, _symbol, _kind in CURVE_VARS])
        writer.writeheader()
        sys.stdout.flush()
        index = 0
        stop_at = time.monotonic() + args.duration_sec if args.duration_sec > 0 else None
        try:
            while True:
                if stop_at is not None and time.monotonic() >= stop_at:
                    return
                start = time.monotonic()
                if args.demo:
                    row = demo_sample(index)
                elif frame_addr is not None:
                    row = sample_target_frame(target, frame_addr)
                else:
                    row = sample_target_block(target, variables, symbols)
                if row is None:
                    continue
                writer.writerow(row)
                sys.stdout.flush()
                index += 1
                elapsed = time.monotonic() - start
                time.sleep(max(0.0, args.interval_ms / 1000.0 - elapsed))
        except KeyboardInterrupt:
            return
        finally:
            if session is not None:
                session.close()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / "servo_curve_live.csv"
    svg_path = output_dir / "servo_curve_live.svg"
    html_path = output_dir / "servo_curve_live.html"
    hbridge_svg_path = output_dir / "hbridge_response.svg"
    hbridge_html_path = output_dir / "hbridge_response.html"
    cycle_dir = output_dir / "cycles"
    cycle_dir.mkdir(exist_ok=True)
    write_csv_header(csv_path)
    write_html(html_path, svg_path.name, csv_path.name, None, args.interval_ms)
    write_hbridge_html(hbridge_html_path, hbridge_svg_path.name, csv_path.name, args.interval_ms)

    rows: deque[dict[str, int]] = deque(maxlen=args.window)
    pretrigger: deque[dict[str, int]] = deque(maxlen=max(20, 200 // max(1, args.interval_ms)))
    hbridge_snapshot = HBridgeSnapshot(args.interval_ms)
    cycle_rows: list[dict[str, int]] = []
    active_cycle = False
    last_target: int | None = None
    last_cycle_name: str | None = None
    cycle_count = 0
    hold_count = 0
    last_redraw = 0.0
    index = 0

    print(f"Live page: {html_path}")
    print("Press Ctrl+C to stop.")
    stop_at = time.monotonic() + args.duration_sec if args.duration_sec > 0 else None
    try:
        while True:
            if stop_at is not None and time.monotonic() >= stop_at:
                print("\nStopped.")
                break
            start = time.monotonic()
            if args.demo:
                row = demo_sample(index)
            elif frame_addr is not None:
                row = sample_target_frame(target, frame_addr)
            else:
                row = sample_target_block(target, variables, symbols)
            if row is None:
                continue
            rows.append(row)
            append_csv(csv_path, row)
            hbridge_snapshot.add(row)

            target_adc = int(row["target_adc"])
            target_changed = last_target is not None and abs(target_adc - last_target) >= 3
            if not active_cycle and target_changed:
                active_cycle = True
                cycle_rows = list(pretrigger)
                hold_count = 0
            if active_cycle:
                cycle_rows.append(row)
                if int(row["event"]) == 2 and int(row["duty"]) == 0:
                    hold_count += 1
                else:
                    hold_count = 0
                if hold_count >= max(8, 180 // max(1, args.interval_ms)) and len(cycle_rows) > hold_count + 5:
                    cycle_count += 1
                    stem = f"cycle_{cycle_count:03d}_{int(cycle_rows[0]['pwm_us'])}_to_{int(row['pwm_us'])}"
                    cycle_csv = cycle_dir / f"{stem}.csv"
                    cycle_svg = cycle_dir / f"{stem}.svg"
                    write_cycle_csv(cycle_csv, cycle_rows)
                    write_svg(cycle_svg, cycle_rows, stem)
                    last_cycle_name = f"cycles/{cycle_svg.name}"
                    active_cycle = False
                    cycle_rows = []
                    hold_count = 0
                    print(f"Saved cycle: {cycle_svg}")
            else:
                pretrigger.append(row)
            last_target = target_adc

            now = time.monotonic()
            if now - last_redraw >= 0.5 and len(rows) >= 2:
                live_rows = list(rows)
                write_svg(svg_path, live_rows, "舵机实时曲线")
                write_html(html_path, svg_path.name, csv_path.name, last_cycle_name, args.interval_ms)
                last_redraw = now

            hbridge_rows = hbridge_snapshot.take_dirty_rows()
            if hbridge_rows is not None:
                write_hbridge_response_svg(hbridge_svg_path, hbridge_rows, "H桥PWM与AD响应 - PWM/外力触发快照")
                write_hbridge_html(hbridge_html_path, hbridge_svg_path.name, csv_path.name, args.interval_ms)

            index += 1
            elapsed = time.monotonic() - start
            sleep_s = max(0.0, args.interval_ms / 1000.0 - elapsed)
            time.sleep(sleep_s)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        if session is not None:
            session.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
