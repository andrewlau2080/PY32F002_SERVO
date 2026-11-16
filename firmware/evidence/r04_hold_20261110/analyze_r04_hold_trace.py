#!/usr/bin/env python3
"""Summarize V10.5 HOLD publication counts using consistent SWD frames.

Each LCDM HOLD publication increments the frame sequence by two. SWD captures
are sparse, so average publication rates do not establish per-loop jitter.
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
    frames = [r for r in rows if r["frame_seq_consistent"] == "1" and
              r["error_consistent"] == "1" and r["state"] == "1" and r["event"] == "2"]
    intervals = []
    for a, b in zip(frames, frames[1:]):
        dt = (int(b["frame_time_ms"]) - int(a["frame_time_ms"])) & 0xFFFFFFFF
        ds = (int(b["seq_head"]) - int(a["seq_head"])) & 0xFFFFFFFF
        if dt and ds and ds % 2 == 0:
            intervals.append((dt, ds // 2))
    if not intervals:
        raise SystemExit("No usable HOLD publication intervals")
    elapsed = sum(dt for dt, _ in intervals)
    publications = sum(count for _, count in intervals)
    report = {"capture": str(args.capture), "capture_sha256": sha(args.capture),
              "analysis_source_sha256": sha(Path(__file__)),
              "consistent_hold_frames": len(frames), "adjacent_pairs": len(intervals),
              "frame_time_span_ms": elapsed, "publication_intervals": publications,
              "mean_publications_per_second": publications * 1000 / elapsed,
              "pair_spacing_ms_min": min(dt for dt, _ in intervals),
              "pair_spacing_ms_median": statistics.median(dt for dt, _ in intervals),
              "pair_spacing_ms_max": max(dt for dt, _ in intervals),
              "scope": "HOLD state publication rate averaged over sparse SWD frame pairs; no command step, instantaneous jitter, UI stress test or H-bridge waveform."}
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Consistent HOLD frames:", len(frames), "pairs:", len(intervals),
          "mean publications/s:", report["mean_publications_per_second"])


if __name__ == "__main__":
    main()
