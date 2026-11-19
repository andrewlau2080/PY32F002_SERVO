#!/usr/bin/env python3
"""Summarize digital transitions and pulse widths from a Saleae VCD export."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Signal:
    code: str
    name: str
    width: int


def parse_vcd(path: Path):
    scopes: list[str] = []
    signals: dict[str, Signal] = {}
    changes: dict[str, list[tuple[int, str]]] = defaultdict(list)
    timescale = "unknown"
    timescale_seconds = None
    timescale_pending = False
    tick = 0
    in_header = True
    scope_re = re.compile(r"^\$scope\s+\S+\s+(\S+)")
    var_re = re.compile(r"^\$var\s+\S+\s+(\d+)\s+(\S+)\s+(.+?)\s+\$end")
    with path.open(errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            if in_header:
                if line.startswith("$timescale"):
                    inline = line.removeprefix("$timescale").removesuffix("$end").strip()
                    if inline:
                        timescale = inline
                        m_scale = re.fullmatch(r"(\d+)\s*(s|ms|us|ns|ps|fs)", inline)
                        if m_scale:
                            timescale_seconds = int(m_scale.group(1)) * {
                                "s": 1, "ms": 1e-3, "us": 1e-6, "ns": 1e-9,
                                "ps": 1e-12, "fs": 1e-15}[m_scale.group(2)]
                    else:
                        timescale_pending = True
                elif timescale_pending:
                    inline = line.removesuffix("$end").strip()
                    if inline and inline != "$end":
                        timescale = inline
                        m_scale = re.fullmatch(r"(\d+)\s*(s|ms|us|ns|ps|fs)", inline)
                        if m_scale:
                            timescale_seconds = int(m_scale.group(1)) * {
                                "s": 1, "ms": 1e-3, "us": 1e-6, "ns": 1e-9,
                                "ps": 1e-12, "fs": 1e-15}[m_scale.group(2)]
                    if "$end" in line:
                        timescale_pending = False
                elif line.startswith("$scope"):
                    m = scope_re.match(line)
                    if m:
                        scopes.append(m.group(1))
                elif line.startswith("$upscope"):
                    if scopes:
                        scopes.pop()
                elif line.startswith("$var"):
                    m = var_re.match(line)
                    if m:
                        width, code, name = int(m.group(1)), m.group(2), m.group(3).strip()
                        signals[code] = Signal(code, "/".join((*scopes, name)), width)
                elif line.startswith("$enddefinitions"):
                    in_header = False
                continue
            if line.startswith("#"):
                try:
                    tick = int(line[1:])
                except ValueError:
                    continue
            elif line[0] in "01xXzZ":
                changes[line[1:]].append((tick, line[0].lower()))
            elif line[0] in "bB":
                parts = line[1:].split()
                if len(parts) == 2:
                    changes[parts[1]].append((tick, parts[0].lower()))
    return timescale, timescale_seconds, signals, changes


def pulse_rows(signals: dict[str, Signal], changes: dict[str, list[tuple[int, str]]],
               seconds_per_tick: float | None):
    rows: list[dict[str, object]] = []
    summary: list[dict[str, object]] = []
    for code, sig in signals.items():
        if sig.width != 1:
            continue
        seq = changes.get(code, [])
        normalized: list[tuple[int, str]] = []
        for t, value in seq:
            value = value[-1:]
            if normalized and normalized[-1][1] == value:
                continue
            normalized.append((t, value))
        highs: list[int] = []
        lows: list[int] = []
        for i, (t, value) in enumerate(normalized):
            previous_known = i > 0 and normalized[i - 1][1] in ("0", "1")
            known = value in ("0", "1")
            edge = ("unknown" if not known else
                    ("initial" if not previous_known else
                     ("rising" if value == "1" else "falling")))
            rows.append({"signal": sig.name, "tick": t,
                         "time_us": f"{t * seconds_per_tick * 1e6:.6f}" if seconds_per_tick else "",
                         "value": value, "edge": edge})
            if i + 1 < len(normalized):
                width = normalized[i + 1][0] - t
                # The first recorded level may already be in progress at capture start.
                next_known = normalized[i + 1][1] in ("0", "1")
                if width >= 0 and previous_known and known and next_known:
                    (highs if value == "1" else lows).append(width)
        high_widths_us = [v * seconds_per_tick * 1e6 for v in highs] if seconds_per_tick else []
        low_widths_us = [v * seconds_per_tick * 1e6 for v in lows] if seconds_per_tick else []
        summary.append({"signal": sig.name, "edges": len(normalized),
                        "rising": sum(1 for i, (_, v) in enumerate(normalized)
                                      if i > 0 and normalized[i - 1][1] == "0" and v == "1"),
                        "falling": sum(1 for i, (_, v) in enumerate(normalized)
                                       if i > 0 and normalized[i - 1][1] == "1" and v == "0"),
                        "high_pulses": len(highs),
                        "high_min_ticks": min(highs, default=""),
                        "high_max_ticks": max(highs, default=""),
                        "high_min_us": f"{min(high_widths_us):.6f}" if high_widths_us else "",
                        "high_max_us": f"{max(high_widths_us):.6f}" if high_widths_us else "",
                        "low_pulses": len(lows),
                        "low_min_ticks": min(lows, default=""),
                        "low_max_ticks": max(lows, default=""),
                        "low_min_us": f"{min(low_widths_us):.6f}" if low_widths_us else "",
                        "low_max_us": f"{max(low_widths_us):.6f}" if low_widths_us else ""})
    return rows, summary


def write_csv(path: Path, rows: list[dict[str, object]], fields: list[str]) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("vcd", type=Path, help="Saleae Logic 1 VCD export")
    p.add_argument("--out-dir", type=Path, default=Path("build/customer_vcd_analysis"))
    a = p.parse_args()
    timescale, seconds_per_tick, signals, changes = parse_vcd(a.vcd)
    edges, summary = pulse_rows(signals, changes, seconds_per_tick)
    a.out_dir.mkdir(parents=True, exist_ok=True)
    write_csv(a.out_dir / "customer_edges.csv", edges,
              ["signal", "tick", "time_us", "value", "edge"])
    write_csv(a.out_dir / "customer_pulse_summary.csv", summary,
              ["signal", "edges", "rising", "falling", "high_pulses",
               "high_min_ticks", "high_max_ticks", "high_min_us", "high_max_us",
               "low_pulses", "low_min_ticks", "low_max_ticks",
               "low_min_us", "low_max_us"])
    all_ticks = [t for seq in changes.values() for t, _ in seq]
    manifest = {
        "vcd_file": str(a.vcd),
        "vcd_sha256": hashlib.sha256(a.vcd.read_bytes()).hexdigest(),
        "timescale": timescale,
        "seconds_per_tick": seconds_per_tick,
        "capture_start_tick": min(all_ticks, default=None),
        "capture_end_tick": max(all_ticks, default=None),
        "signals": [{"name": s.name, "width": s.width,
                     "value_changes": len(changes.get(s.code, []))}
                    for s in signals.values()],
        "edge_csv": "customer_edges.csv",
        "pulse_summary_csv": "customer_pulse_summary.csv",
        "sample_rate_hz": None,
        "sample_rate_note": "VCD change timestamps do not reliably encode the Saleae acquisition sample rate; preserve it from the original capture/export metadata.",
        "trigger_time_note": "VCD timestamp zero is retained as exported; verify trigger offset against the Saleae capture/export metadata.",
    }
    (a.out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"timescale={timescale} signals={len(signals)} single_bit={sum(s.width == 1 for s in signals.values())}")
    print(f"edges={len(edges)} out={a.out_dir}")


if __name__ == "__main__":
    main()
