#!/usr/bin/env python3
"""Check simulated LCDM-independent controller HOLD settling against HAL time."""
import csv
import sys


def main(path):
    with open(path, newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    segments = sorted({int(row["segment"]) for row in rows})
    passed = 0
    failed = 0
    for segment in segments:
        samples = [row for row in rows if int(row["segment"]) == segment]
        candidate_ms = None
        hold_ms = None
        for index, row in enumerate(samples):
            time_ms = int(row["time_ms"])
            if index == 0:
                velocity_q4 = 0
            else:
                previous = samples[index - 1]
                dt_ms = time_ms - int(previous["time_ms"])
                if dt_ms <= 0:
                    continue
                delta = int(row["feedback_adc"]) - int(previous["feedback_adc"])
                velocity_q4 = (abs(delta) * 16) // dt_ms
            if candidate_ms is None and abs(int(row["error_adc"])) <= 8 and velocity_q4 <= 36:
                candidate_ms = time_ms
            if hold_ms is None and int(row["state"]) == 1:
                hold_ms = time_ms

        delay_ms = None if candidate_ms is None or hold_ms is None else hold_ms - candidate_ms
        ok = delay_ms is not None and delay_ms >= 20
        print(("PASS" if ok else "FAIL") +
              f": segment {segment} stable candidate to HOLD = {delay_ms} ms (minimum 20 ms)")
        if ok:
            passed += 1
        else:
            failed += 1
    print(f"R11 production hold-settle simulation: {passed} passed, {failed} failed")
    return 0 if failed == 0 and passed == 6 else 1


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_hold_settle.py simulation.csv")
    raise SystemExit(main(sys.argv[1]))
