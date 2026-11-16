import csv
import json
from collections import Counter
from pathlib import Path

root = Path(__file__).resolve().parent
rows = list(csv.DictReader((root / 'servo_curve_v102.csv').open()))
valid = [r for r in rows if r['error_adc'] and
         int(r['error_adc']) == int(r['target_adc']) - int(r['feedback_adc'])]
unique = {r['seq_tail']: r for r in valid}
frames = list(unique.values())
center = [r for r in frames if abs(int(r['target_adc']) - 2051) <= 4]
moving = [r for r in center if int(r['state']) == 2]
near = [r for r in moving if 257 <= abs(int(r['error_adc'])) <= 512]
full_near = [r for r in near if abs(int(r['duty'])) >= 900]
by_target = {}
for target in sorted({int(r['target_adc']) for r in frames}):
    group = [r for r in frames if int(r['target_adc']) == target]
    drives = [r for r in group if int(r['state']) == 2]
    full = [r for r in drives if abs(int(r['duty'])) >= 900]
    by_target[str(target)] = {
        'frames': len(group),
        'feedback_min': min(int(r['feedback_adc']) for r in group),
        'feedback_max': max(int(r['feedback_adc']) for r in group),
        'drive_frames': len(drives),
        'full_duty_frames': len(full),
    }
inputs = list(csv.DictReader((root / 'servo_inputs_v102.csv').open()))
def n(row, key): return int(row[key])
report = {
    'firmware': 'V10.2',
    'capture_seconds': round(float(rows[-1]['wall_s']) - float(rows[0]['wall_s']), 2),
    'poll_rows': len(rows),
    'error_consistent_rows': len(valid),
    'unique_error_consistent_frames': len(frames),
    'torn_or_inconsistent_rows_excluded': len(rows) - len(valid),
    'input_probe_rows': len(inputs),
    'input_valid_values': sorted({n(r, 'valid') for r in inputs}),
    'input_period_us_values': sorted({n(r, 'period_us') for r in inputs}),
    'input_no_edge_final_snapshot': {
        'tick': n(inputs[-1], 'tick'), 'last_edge_ms': n(inputs[-1], 'last_edge_ms'),
        'pulse_count': n(inputs[-1], 'pulse_count'), 'edge_count': n(inputs[-1], 'edge_count'),
        'state': n(inputs[-1], 'state'), 'state_name': 'BAD',
    },
    'center_target_adc_range': [min(int(r['target_adc']) for r in center),
                                max(int(r['target_adc']) for r in center)],
    'center_feedback_adc_range': [min(int(r['feedback_adc']) for r in center),
                                  max(int(r['feedback_adc']) for r in center)],
    'center_raw_adc_range': [min(int(r['raw_adc']) for r in center),
                             max(int(r['raw_adc']) for r in center)],
    'center_raw_vs_filtered_max_abs_delta': max(
        abs(int(r['raw_adc']) - int(r['feedback_adc'])) for r in center),
    'center_error_adc_range': [min(int(r['error_adc']) for r in center),
                               max(int(r['error_adc']) for r in center)],
    'center_near_error_257_512': {
        'drive_frames': len(near), 'duty_ge_900_frames': len(full_near),
        'full_duty_percent': round(100 * len(full_near) / max(1, len(near)), 1),
    },
    'by_target': by_target,
    'parameter_crc': 'valid at all 13 integrity samples; input_timeout_ms=100',
    'limits': [
        'Raw polling is about 13 Hz; it does not capture PWM switching edges or instantaneous peaks.',
        'The primary curve reader did not collect both sequence-lock words; rows with error != target-feedback were excluded, then duplicate seq_tail rows were removed.',
        'The final auxiliary snapshot had no new PWM edges for 182.5 seconds, so BAD/coast there is the configured input-timeout response, not evidence of a control-loop deadlock.',
    ],
}
(root / 'analysis_v102.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
