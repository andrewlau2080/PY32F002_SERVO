#!/usr/bin/env python3
"""Reconstruct idealized one-way motion from the customer's Saleae exports.

This is an analysis proxy, not a recovered customer PID implementation. It
assumes near-constant speed up to the first opposite-direction PWM packet and
linear deceleration to rest through that packet.
"""

from __future__ import annotations

import bisect
import csv
import math
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "firmware/evidence/logicdata_exports_20261122"
BIN_S = 0.002
OUTPUT_LIMIT = 0.9


@dataclass(frozen=True)
class Segment:
    file: str
    label: str
    span_us: float
    direction: int  # +1 increasing command, -1 decreasing command
    drive_start_s: float
    drive_end_s: float
    brake_start_s: float
    brake_end_s: float


SEGMENTS = (
    Segment("1000-1500_all8.csv", "1000->1500", 500, +1, 3.365255, 3.482457, 3.491595, 3.559096),
    Segment("1000-1500_all8.csv", "1500->1000", 500, -1, 4.284965, 4.379576, 4.379700, 4.438180),
    Segment("1500-2000_all8.csv", "1500->2000", 500, +1, 3.648481, 3.761163, 3.761287, 3.810750),
    Segment("1500-2000_all8.csv", "2000->1500", 500, -1, 4.910515, 5.005098, 5.005162, 5.050173),
    Segment("2000-1000_all8.csv", "2000->1000", 1000, -1, 3.267247, 3.442962, 3.443085, 3.501555),
    Segment("2000-1000_all8.csv", "1000->2000", 1000, +1, 4.907729, 5.115082, 5.119593, 5.169169),
)


def load_edges(path: Path) -> tuple[list[float], list[list[int]]]:
    with path.open(newline="") as stream:
        reader = csv.reader(stream)
        headers = [cell.strip() for cell in next(reader)]
        if headers[0] != "Time[s]" or headers[5] not in ("1000-1500", "1500-2000", "2000-1000"):
            raise ValueError(f"unexpected Saleae export header: {headers}")
        if headers[7] != "Channel 6":
            raise ValueError(f"expected PWM on digital index 6, got {headers[7]}")
        times: list[float] = []
        states: list[list[int]] = []
        for row in reader:
            if len(row) >= 9:
                times.append(float(row[0]))
                states.append([int(value) for value in row[1:9]])
    return times, states


def envelope_samples(segment: Segment, times: list[float], states: list[list[int]]) -> list[tuple[float, float, float]]:
    t0, tdrive_end, tbrake, tend = (
        segment.drive_start_s,
        segment.drive_end_s,
        segment.brake_start_s,
        segment.brake_end_s,
    )
    tdrive = tdrive_end - t0
    tgap = tbrake - tdrive_end
    tbrake_len = tend - tbrake
    effective_time = tdrive + tgap + tbrake_len / 2
    velocity_norm = 1.0 / effective_time
    count = math.ceil((tend - t0) / BIN_S)
    sums4 = [0.0] * count
    sums6 = [0.0] * count
    index = max(0, bisect.bisect_right(times, t0) - 1)

    for bin_index in range(count):
        start = t0 + bin_index * BIN_S
        end = min(tend, start + BIN_S)
        cursor = start
        while cursor < end and index < len(times) - 1:
            next_edge = times[index + 1]
            overlap_end = min(end, next_edge)
            if overlap_end > cursor:
                duration = overlap_end - cursor
                sums4[bin_index] += duration * states[index][4]
                sums6[bin_index] += duration * states[index][6]
                cursor = overlap_end
            if next_edge <= cursor or next_edge <= start:
                index += 1
            else:
                break
        if cursor < end:
            state = states[index]
            sums4[bin_index] += (end - cursor) * state[4]
            sums6[bin_index] += (end - cursor) * state[6]

    result: list[tuple[float, float, float]] = []
    for i in range(count):
        start = t0 + i * BIN_S
        end = min(tend, start + BIN_S)
        center_rel = (start + end) / 2 - t0
        if center_rel <= tdrive + tgap:
            progress = center_rel / effective_time
            speed = velocity_norm
        else:
            tau = min(center_rel - (tdrive + tgap), tbrake_len)
            progress_at_brake = (tdrive + tgap) / effective_time
            progress = progress_at_brake + velocity_norm * (tau - tau * tau / (2 * tbrake_len))
            speed = velocity_norm * (1 - tau / tbrake_len)
        error = max(0.0, min(1.0, 1.0 - progress))
        width = end - start
        signed_output = segment.direction * (sums4[i] - sums6[i]) / width
        result.append((error, speed, signed_output))
    return result


def clamp(value: float) -> float:
    return max(-OUTPUT_LIMIT, min(OUTPUT_LIMIT, value))


def average_effort(
    segment: Segment,
    start_s: float,
    end_s: float,
    times: list[float],
    states: list[list[int]],
) -> float:
    """Average signed H-bridge duty over a time interval."""
    total = 0.0
    index = max(0, bisect.bisect_right(times, start_s) - 1)
    cursor = start_s
    while cursor < end_s and index < len(times) - 1:
        next_edge = times[index + 1]
        overlap_end = min(end_s, next_edge)
        if overlap_end > cursor:
            total += (overlap_end - cursor) * segment.direction * (states[index][4] - states[index][6])
            cursor = overlap_end
        if next_edge <= cursor:
            index += 1
        else:
            break
    if cursor < end_s:
        total += (end_s - cursor) * segment.direction * (states[index][4] - states[index][6])
    return total / (end_s - start_s)


def fit_normalized_motor(segments: tuple[Segment, ...], cache: dict[str, tuple[list[float], list[list[int]]]]) -> tuple[float, float, list[tuple[float, float]]]:
    """Fit v_dot = motor_gain*u - viscous_drag*v to the ideal segments.

    Cruise is treated as steady speed and the first reverse packet as constant
    deceleration. This only produces a normalized simulator plant.
    """
    motor_gains: list[float] = []
    drags: list[float] = []
    for segment in segments:
        times, states = cache[segment.file]
        drive = average_effort(segment, segment.drive_start_s, segment.drive_end_s, times, states)
        brake = average_effort(segment, segment.brake_start_s, segment.brake_end_s, times, states)
        drive_s = segment.drive_end_s - segment.drive_start_s
        gap_s = segment.brake_start_s - segment.drive_end_s
        brake_s = segment.brake_end_s - segment.brake_start_s
        cruise_speed = 1.0 / (drive_s + gap_s + brake_s / 2)
        gain = cruise_speed / (brake_s * (drive - brake))
        drag = gain * drive / cruise_speed
        motor_gains.append(gain)
        drags.append(drag)
    return sum(motor_gains) / len(motor_gains), sum(drags) / len(drags), list(zip(motor_gains, drags))


def simulate_step(kp: float, kd: float, motor_gain: float, drag: float) -> tuple[float | None, float, float | None]:
    """Run a normalized 1-D closed loop and return 98% time, overshoot, settle time."""
    dt = 0.001
    x = velocity = 0.0
    peak = x
    rise_time: float | None = None
    settled_at: float | None = None
    stable_samples = 0
    for index in range(1200):
        time_s = index * dt
        error = 1.0 - x
        effort = clamp(kp * error - kd * velocity)
        velocity += dt * (motor_gain * effort - drag * velocity)
        x += dt * velocity
        peak = max(peak, x)
        if rise_time is None and x >= 0.98:
            rise_time = time_s
        if abs(1.0 - x) < 0.01 and abs(velocity) < 0.05:
            stable_samples += 1
            if stable_samples == 20 and settled_at is None:
                settled_at = time_s - 0.019
        else:
            stable_samples = 0
    return rise_time, max(0.0, peak - 1.0), settled_at


def main() -> None:
    cache: dict[str, tuple[list[float], list[list[int]]]] = {}
    data: list[tuple[Segment, list[tuple[float, float, float]]]] = []
    for segment in SEGMENTS:
        if segment.file not in cache:
            cache[segment.file] = load_edges(EVIDENCE / segment.file)
        data.append((segment, envelope_samples(segment, *cache[segment.file])))

    best = (float("inf"), 0.0, 0.0)
    for kp_index in range(101):
        kp = kp_index * 0.05
        for kd_index in range(101):
            kd = kd_index * 0.005
            error_sum = 0.0
            sample_count = 0
            for _, samples in data:
                for error, speed, observed in samples:
                    residual = observed - clamp(kp * error - kd * speed)
                    error_sum += residual * residual
                    sample_count += 1
            mse = error_sum / sample_count
            if mse < best[0]:
                best = (mse, kp, kd)

    mse, kp, kd = best
    print("ideal linear-speed / linear-brake reconstruction; Ki fixed at 0")
    print("segment,drive_ms,gap_ms,brake_ms,command_us_per_s,brake_start_pct,brake_distance_us,PD_RMSE")
    for segment, samples in data:
        td = segment.drive_end_s - segment.drive_start_s
        gap = segment.brake_start_s - segment.drive_end_s
        tb = segment.brake_end_s - segment.brake_start_s
        effective_time = td + gap + tb / 2
        speed = segment.span_us / effective_time
        brake_start_pct = (td + gap) / effective_time * 100
        brake_distance = speed * tb / 2
        segment_mse = sum(
            (observed - clamp(kp * error - kd * velocity)) ** 2
            for error, velocity, observed in samples
        ) / len(samples)
        print(
            f"{segment.label},{td*1000:.3f},{gap*1000:.3f},{tb*1000:.3f},"
            f"{speed:.1f},{brake_start_pct:.1f},{brake_distance:.1f},{math.sqrt(segment_mse):.3f}"
        )
    print(f"samples={sum(len(samples) for _, samples in data)}; Kp={kp:.2f}; Ki=0; Kd={kd:.3f}; total_RMSE={math.sqrt(mse):.3f}")
    print("Note: fitted gains belong only to this normalized idealized proxy, not verified customer firmware constants.")

    motor_gain, drag, per_segment = fit_normalized_motor(SEGMENTS, cache)
    print(f"normalized plant: motor_gain={motor_gain:.1f} s^-2/duty; viscous_drag={drag:.2f} s^-1")
    print("segment_plant_parameters:")
    for segment, (gain_i, drag_i) in zip(SEGMENTS, per_segment):
        print(f"  {segment.label}: gain={gain_i:.1f}, drag={drag_i:.2f}")

    for label, test_kp, test_kd in (("curve_fit_PD", kp, kd), ("closed_loop_seed", 14.0, 0.50)):
        rise, overshoot, settle = simulate_step(test_kp, test_kd, motor_gain, drag)
        print(
            f"step_sim {label}: Kp={test_kp:.2f}, Ki=0, Kd={test_kd:.3f}, "
            f"t98={rise if rise is not None else 'not reached'}, "
            f"overshoot={overshoot:.4f}, settle={settle if settle is not None else 'not settled'}"
        )

    print("segment_plant_step_sim using closed_loop_seed Kp=14, Ki=0, Kd=0.50:")
    for segment, (gain_i, drag_i) in zip(SEGMENTS, per_segment):
        rise, overshoot, settle = simulate_step(14.0, 0.50, gain_i, drag_i)
        print(
            f"  {segment.label}: t98={rise if rise is not None else 'not reached'}, "
            f"overshoot={overshoot:.4f}, settle={settle if settle is not None else 'not settled'}"
        )

    candidates: list[tuple[float, float, float, float, float]] = []
    for kp_index in range(10, 41):
        test_kp = kp_index * 0.5
        for kd_index in range(2, 25):
            test_kd = kd_index * 0.025
            rise, overshoot, settle = simulate_step(test_kp, test_kd, motor_gain, drag)
            if rise is not None and settle is not None and 0.14 <= rise <= 0.24 and overshoot <= 0.01:
                # Prefer a quick, low-overshoot response without selecting the
                # edge of the search grid just because it has a tiny time gain.
                candidates.append((settle + 0.5 * overshoot, overshoot, rise, test_kp, test_kd))
    if candidates:
        _, overshoot, rise, test_kp, test_kd = min(candidates)
        _, _, settle = simulate_step(test_kp, test_kd, motor_gain, drag)
        print(
            f"bounded_gain_candidate: Kp={test_kp:.2f}, Ki=0, Kd={test_kd:.3f}, "
            f"t98={rise:.3f}s, overshoot={overshoot:.4f}, settled={settle:.3f}s"
        )


if __name__ == "__main__":
    main()
