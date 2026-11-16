#!/usr/bin/env python3
"""Explore window differences in a field trace and synthetic +/-1 us offsets.

This is descriptive only: the external generator has no independent reference,
SWD samples omit most PWM frames, and adjacent windows overlap heavily.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    with args.capture.open(newline="") as source:
        rows = list(csv.DictReader(source))
    if len(rows) < 61 or any(row["input_valid"] != "1" for row in rows):
        raise SystemExit("Expected at least 61 samples with valid PWM input")
    times = [float(row["wall_s"]) for row in rows]
    if any(b <= a for a, b in zip(times, times[1:])):
        raise SystemExit("Wall timestamps are not strictly increasing")
    widths = [int(row["hw_raw_us"]) for row in rows]
    step_ms = statistics.median(b - a for a, b in zip(times, times[1:])) * 1000.0
    results = []
    for n in (5, 10, 20, 30):
        # Sums avoid rounding ambiguity when comparing quarter-microsecond thresholds.
        differences = [sum(widths[i+n:i+2*n]) - sum(widths[i:i+n])
                       for i in range(len(widths) - 2*n + 1)]
        for threshold_quarters in (2, 3, 4):
            threshold = threshold_quarters * n
            denom = len(differences)
            results.append({
                "window_samples": n,
                "typical_window_seconds": round(n * step_ms / 1000.0, 3),
                "threshold_us": threshold_quarters / 4.0,
                "overlapping_window_pairs": denom,
                "apparent_change_fraction": sum(4 * abs(x) >= threshold for x in differences) / denom,
                "synthetic_plus_1us_fraction": sum(4 * (x+n) >= threshold for x in differences) / denom,
                "synthetic_minus_1us_fraction": sum(4 * (x-n) <= -threshold for x in differences) / denom,
            })
    report = {
        "capture": str(args.capture), "capture_sha256": sha(args.capture),
        "analysis_source_sha256": sha(Path(__file__)),
        "samples": len(rows), "median_spacing_ms": round(step_ms, 3),
        "width_min_us": min(widths), "width_max_us": max(widths),
        "width_mean_us": round(statistics.mean(widths), 4),
        "results": results,
        "limitations": [
            "No independent generator reference; apparent changes are not proven false alarms.",
            "Adjacent window pairs overlap, so the fractions are not independent probabilities.",
            "SWD samples omit most PWM frames; these are not per-pulse timing measurements.",
            "Offsets are artificially added to the later window; no physical command step or motor is tested.",
        ],
    }
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    for result in results:
        print("{window_samples:2} samples, threshold {threshold_us:.2f} us: "
              "observed {apparent_change_fraction:.1%}, synthetic +1 {synthetic_plus_1us_fraction:.1%}, "
              "synthetic -1 {synthetic_minus_1us_fraction:.1%}".format(**result))


if __name__ == "__main__":
    main()
