#!/usr/bin/env python3
"""Plot exported servo curve CSV data as a two-panel SVG.

The script intentionally uses only Python standard library modules so it can run
on macOS or Ubuntu without installing plotting packages.
"""

from __future__ import annotations

import argparse
import csv
import html
from pathlib import Path


POSITION_SERIES = [
    ("target", ("g_servo_curve_target_adc", "target_adc"), "#d62728"),
    ("feedback", ("g_servo_curve_feedback_adc", "feedback_adc"), "#1f77b4"),
    ("traj", ("g_servo_curve_traj_adc", "traj_adc"), "#2ca02c"),
]

CONTROL_SERIES = [
    ("error", ("g_servo_curve_error_adc", "error_adc"), "#d62728"),
    ("duty", ("g_servo_curve_duty", "duty"), "#1f77b4"),
    ("vel_q4", ("g_servo_curve_velocity_q4", "velocity_q4"), "#9467bd"),
    ("ref_vel", ("g_servo_curve_ref_velocity", "ref_velocity"), "#ff7f0e"),
]

EVENT_SERIES = ("event", ("g_servo_curve_event", "event"), "#444444")
TIME_COLUMNS = ("g_servo_curve_time_ms", "time_ms", "time", "ms")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a servo motion curve SVG from VarViewer/debugger CSV export."
    )
    parser.add_argument("csv_file", type=Path)
    parser.add_argument("-o", "--output", type=Path, default=Path("servo_curve_plot.svg"))
    parser.add_argument("--title", default="Servo motion curve")
    return parser.parse_args()


def pick_column(fieldnames: list[str], aliases: tuple[str, ...]) -> str | None:
    lowered = {name.strip().lower(): name for name in fieldnames}
    for alias in aliases:
        found = lowered.get(alias.lower())
        if found is not None:
            return found
    return None


def to_float(value: str) -> float | None:
    text = value.strip()
    if not text:
        return None
    try:
        return float(text)
    except ValueError:
        return None


def load_csv(path: Path) -> tuple[list[float], dict[str, list[float | None]], dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            raise SystemExit("CSV has no header row.")
        fieldnames = reader.fieldnames
        time_column = pick_column(fieldnames, TIME_COLUMNS)

        selected: dict[str, str] = {}
        for label, aliases, _color in POSITION_SERIES + CONTROL_SERIES + [EVENT_SERIES]:
            column = pick_column(fieldnames, aliases)
            if column is not None:
                selected[label] = column

        x_values: list[float] = []
        series = {label: [] for label in selected}
        for index, row in enumerate(reader):
            x_raw = to_float(row.get(time_column, "")) if time_column else None
            x_values.append(float(index) if x_raw is None else x_raw)
            for label, column in selected.items():
                series[label].append(to_float(row.get(column, "")))

    if not x_values:
        raise SystemExit("CSV has no data rows.")
    return x_values, series, selected


def data_range(values: list[float | None]) -> tuple[float, float]:
    finite = [value for value in values if value is not None]
    if not finite:
        return 0.0, 1.0
    low = min(finite)
    high = max(finite)
    if low == high:
        pad = max(1.0, abs(low) * 0.05)
        return low - pad, high + pad
    pad = (high - low) * 0.08
    return low - pad, high + pad


def make_polyline(
    x_values: list[float],
    y_values: list[float | None],
    x_min: float,
    x_max: float,
    y_min: float,
    y_max: float,
    rect: tuple[int, int, int, int],
) -> str:
    left, top, width, height = rect
    points: list[str] = []
    x_span = x_max - x_min if x_max != x_min else 1.0
    y_span = y_max - y_min if y_max != y_min else 1.0
    for x_raw, y_raw in zip(x_values, y_values):
        if y_raw is None:
            continue
        x = left + ((x_raw - x_min) / x_span) * width
        y = top + height - ((y_raw - y_min) / y_span) * height
        points.append(f"{x:.1f},{y:.1f}")
    return " ".join(points)


def draw_panel(
    title: str,
    x_values: list[float],
    series: dict[str, list[float | None]],
    definitions: list[tuple[str, tuple[str, ...], str]],
    rect: tuple[int, int, int, int],
) -> list[str]:
    left, top, width, height = rect
    x_min, x_max = min(x_values), max(x_values)
    y_values: list[float | None] = []
    for label, _aliases, _color in definitions:
        y_values.extend(series.get(label, []))
    y_min, y_max = data_range(y_values)

    items = [
        f'<text x="{left}" y="{top - 12}" font-size="16" font-weight="700">{html.escape(title)}</text>',
        f'<rect x="{left}" y="{top}" width="{width}" height="{height}" fill="#ffffff" stroke="#d0d0d0"/>',
    ]
    for i in range(1, 4):
        y = top + height * i / 4
        items.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left + width}" y2="{y:.1f}" stroke="#eeeeee"/>')
    for i in range(1, 5):
        x = left + width * i / 5
        items.append(f'<line x1="{x:.1f}" y1="{top}" x2="{x:.1f}" y2="{top + height}" stroke="#f2f2f2"/>')

    legend_x = left + 8
    for label, _aliases, color in definitions:
        values = series.get(label)
        if not values:
            continue
        points = make_polyline(x_values, values, x_min, x_max, y_min, y_max, rect)
        if points:
            items.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="2"/>')
            items.append(
                f'<text x="{legend_x}" y="{top + 18}" font-size="12" fill="{color}">{html.escape(label)}</text>'
            )
            legend_x += 82

    items.append(f'<text x="{left}" y="{top + height + 18}" font-size="11" fill="#666">x: time/index</text>')
    items.append(f'<text x="{left + width - 130}" y="{top - 12}" font-size="11" fill="#666">{y_min:.0f}..{y_max:.0f}</text>')
    return items


def main() -> None:
    args = parse_args()
    x_values, series, selected = load_csv(args.csv_file)
    if not selected:
        raise SystemExit("No known servo curve columns found in CSV.")

    width, height = 1120, 760
    pos_rect = (70, 80, 980, 250)
    ctrl_rect = (70, 410, 980, 250)
    svg: list[str] = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#f7f7f7"/>',
        f'<text x="70" y="42" font-size="22" font-weight="700">{html.escape(args.title)}</text>',
    ]
    svg.extend(draw_panel("Position: target / feedback / trajectory", x_values, series, POSITION_SERIES, pos_rect))
    svg.extend(draw_panel("Control: error / duty / velocity / reference", x_values, series, CONTROL_SERIES, ctrl_rect))

    if "event" in series:
        event_rect = (70, 690, 980, 34)
        event_min, event_max = data_range(series["event"])
        points = make_polyline(x_values, series["event"], min(x_values), max(x_values), event_min, event_max, event_rect)
        svg.append(f'<rect x="{event_rect[0]}" y="{event_rect[1]}" width="{event_rect[2]}" height="{event_rect[3]}" fill="#ffffff" stroke="#d0d0d0"/>')
        svg.append(f'<polyline points="{points}" fill="none" stroke="{EVENT_SERIES[2]}" stroke-width="2"/>')
        svg.append(f'<text x="{event_rect[0]}" y="{event_rect[1] - 8}" font-size="13" fill="#444">event: 1 DRIVE, 2 HOLD, 3/4/5/6 BRK, 7 HLD suppress, 8 alarm</text>')

    svg.append("</svg>")
    args.output.write_text("\n".join(svg), encoding="utf-8")
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
