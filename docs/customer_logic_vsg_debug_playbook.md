# Customer LOGICDATA and VSG Debug Playbook

> Architecture authority: `servo_adaptive_motion_architecture.md` (2026-11-08). This playbook preserves the evidence workflow, but historical phase numbers below are hypotheses/baseline notes until the raw customer levels and both output lanes are decoded or remeasured. Per the user's 2026-11-22 instruction, analyze `2026/1000-1500.logicdata`, `2026/1500-2000.logicdata`, and `2026/2000-1000.logicdata` as the selected customer motion samples. Keep the older `PY32F002_SIGNAL_CAPTURE/CUSTOMER.logicdata` as historical reference, and do not mix in `2026/PA7串口*.logicdata` as motor traces.

This project must use this workflow whenever customer LOGICDATA exists or the user says to compare with customer waveforms. Do not fall back to slow PID/limit tuning before this comparison is visible.

## Core Rule

Use one shared comparison page so the user and Codex see the same evidence:

- Page: `build/customer_logic_compare/customer_logic_compare.html`
- SVG: `build/customer_logic_compare/customer_logic_compare.svg`
- Customer side: first valid LOGICDATA drive edge is `t=0`.
- Our side: target step or first nonzero H-bridge duty is `t=0`.
- Never align by absolute file time or by the start of SWD sampling.
- Do not repeatedly open browser windows. Update the same HTML/SVG only.

## Fast Call Index

When the user reports shake, dead loop, slower motion, invisible VSG, LCDM no display, or asks to compare customer `LOGICDATA`, call this playbook first.

Use this order:

1. Stop duplicate live viewers and keep only the shared comparison page.
2. Confirm the flashed firmware version, vector, LCDM pins, baud, and `Successfully resumed device`.
3. Confirm VSG/SWD data is real: rows exist, `time_ms` changes, and `error_adc` matches `target_adc - feedback_adc`.
4. Align customer and our traces by action start, then update `build/customer_logic_compare/customer_logic_compare.html`.
5. Identify waveform mismatch before editing code.
6. Change the smallest code path that explains the mismatch.
7. Rebuild, flash, read back version, run, and capture again.
8. Record version, hash/vector, evidence file, and user result.

Do not skip steps 2-4. Most wasted time in this project came from flashing a build with the wrong LCDM physical layer, reading empty/stale VSG data, or tuning without the customer waveform visible.

## Debug Priority

1. Reproduce the customer drive/brake/tail/coast waveform first.
2. Use PID, deadband, slew, or output limiting only as final trimming.
3. If a change makes motion globally slower, it is not a valid LOGICDATA reproduction unless the customer waveform is also slow in the same time window.
4. Mark bad trial lines as failed immediately; do not keep tuning from them.

## Known Customer Waveform Baseline

Historical working values from earlier LOGICDATA notes (not yet independently decoded from the raw file):

- Main drive: about `12 ms` at high duty, currently represented as `930/1000`.
- Reverse brake: about `4 ms` at `360/1000`.
- Tail brake/damp: about `3 ms` at `220/1000`.
- Coast/gate window: about `24 ms` before normal control fully resumes.

The exact customer clusters in `build/customer_logic_compare/customer_logic_blocks.csv` include shorter variants; always compare by start-aligned blocks, not a single average number.

## Required Evidence Loop

1. Confirm board firmware and LCDM wiring before capture:
   - Use `tools/build_flash_lcdm_runtime_macos.sh` for field downloads.
   - Required flags include `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1`, `BOARD_TSSOP20_DEBUG_PINS=1`, `SERVO_TJC_UART_ALT_PINS=1`, `LCDM_TJC_BAUD=230400U`, `SERVO_ENABLE_PA0_POT_POWER=1`.
2. Confirm version readback and `Successfully resumed device` after flashing.
3. Capture VSG/SWD data from the running target.
4. Generate/update the single comparison page with start-aligned customer vs our waveform.
5. Only then decide what code to change.

## SWD/VSG Capture Notes

- Preferred live variables are `g_servo_curve_frame` and `g_servo_curve_*`.
- If strict `seq_head == seq_tail` rejects all samples, directly inspect RAM frame values and use the physical consistency check `error_adc ~= target_adc - feedback_adc` to recover a usable diagnostic plot.
- If live sampling misses the event, use forced ring capture:
  - `tools/force_servo_curve_log.py`
  - Dump/read `g_servo_curve_log` with `tools/dump_servo_curve_log.py`.
- If CSV has only a header, there is no VSG plot. Fix sampling before discussing waveform differences.
- If CSV has many rows but identical `time_ms`, MCU was halted or not actually executing the control loop. Resume correctly before using the graph.

## Symptom To Action

| Symptom | First action | Main meaning if confirmed |
|---|---|---|
| VSG page has no curve | Check CSV row count before discussing control logic | Sampling did not capture usable data |
| CSV rows exist but `time_ms` is identical | Resume/reset-run MCU and verify `g_servo_curve_frame` updates | Core is halted or app loop is not publishing |
| `seq_head != seq_tail` rejects many samples | Prefer halt-read ring buffer; use consistency check only as secondary evidence | SWD read raced with frame update |
| LCDM no display after flash | Verify build flags for `PA7/PB2` and `230400` before suspecting code logic | Wrong physical LCDM build, not necessarily broken screen |
| Motion became globally slow | Compare drive duration/duty against customer `LOGICDATA` immediately | A global limiter was added in the wrong layer |
| Starts fine, gets worse over time | Capture ring/VSG at the moment it worsens; inspect target stability, event sequence, duty sign, and HOLD/LOCK exits | Usually accumulated energy or repeated phase restart, not one PID number |
| Fixed internal signal still shakes | Ignore external PWM path; inspect target fixedness, `event12/13/16/18`, duty, feedback velocity, and HOLD reference | Internal close-loop/tail/lock problem |
| Fixed external PWM shakes | Check `pwm_us`, latched PWM, target ADC, and command gate | 1-3 us jitter may be restarting motion or exiting HOLD |
| User says "again, monitor now" | Capture immediately; do not ask for a better verbal description | The live failure window matters more than explanation |

## Single-Page UI Rule

- Keep only `build/customer_logic_compare/customer_logic_compare.html` open for customer comparison.
- Add/keep auto-refresh in that page if needed.
- Do not call `open` repeatedly from scripts.
- If another VSG page is needed for raw detail, link it from the comparison page instead of opening new windows.

## Interpretation Checklist

Look for these mismatches first:

- Our first drive starts later than customer after aligned `t=0`.
- Our drive duration is too long or duty too low/high versus customer.
- Reverse brake starts too late or is missing.
- Tail brake/coast gate is missing, too short, or interrupted by normal PID.
- Normal `event12/event13` resumes inside the customer coast/gate window.
- External PWM 1-3 us jitter restarts a customer profile during motion.
- Large command steps fail to restart the customer profile.

## 9.5 To 9.6 Recovery Clues

When tracing `V9.5 -> V9.6`, keep these findings together:

- Original board `P50 V9.6` evidence: `SP=0x20000c00`, `PC=0x0800495d`, version string around `0x08004b6e`.
- `P50J` had the strongest matching clue because its startup vector also matched `PC=0x0800495d`.
- The likely useful difference was not a PID-only change. It combined V9.5 internal stability with P50J/K-style command gating:
  - external/internal small command jitter updates target cautiously but does not restart the active motion profile;
  - large target steps still restart the customer profile;
  - HOLD/LOCK/tail behavior must not inject new energy near the end.
- Do not label a rebuilt test image as the original `V9.6` unless HEX/BIN bytes or hash prove it.

## Customer Profile Baseline

Keep these numbers visible when reviewing code or graph output:

| Phase | Current target behavior |
|---|---:|
| Main drive | about `12 ms` at `930/1000` |
| Reverse brake | about `4 ms` at `360/1000` |
| Tail damp/brake | about `3 ms` at `220/1000` |
| Coast/gate | about `24 ms` before normal control fully resumes |

If our curve is slower, longer, or starts normal PID inside the customer gate window, fix the profile/phase logic first.

## Required Field Build Guard

For current LCDM field tests, use the fixed script unless there is a documented reason not to:

```bash
./tools/build_flash_lcdm_runtime_macos.sh
```

This exists to prevent repeating the PA7/PB2 and baud mistake. The build must include:

- `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1`
- `BOARD_TSSOP20_DEBUG_PINS=1`
- `SERVO_TJC_UART_ALT_PINS=1`
- `LCDM_TJC_BAUD=230400U`
- `SERVO_ENABLE_PA0_POT_POWER=1`

After download, read back the version string and resume with the flashed vector. A pyOCD result that says `programmed 0 bytes` can still be valid if the target already matches the HEX; readback decides.

## Communication Rule

The user may not be able to describe the waveform problem precisely in words. In that case, use VSG/ring capture and the shared comparison page as the common language. The correct response is to make the evidence visible quickly, not to ask the user to keep explaining the same shake.

## What Not To Repeat

- Do not globally limit `event12/event13` just to suppress shake; this caused `P50 V9.6R7` to become globally slow.
- Do not diagnose from user description alone when VSG or ring data can be captured.
- Do not blame external PWM when the user has explicitly switched to internal signal testing.
- Do not change LCDM pins or baud implicitly; field setup is PA7/PB2 at 230400 bps.
