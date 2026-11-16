# R04 timing trace R10: window-local counters

- Purpose: rerun the synchronized center sweep and clear timing/scheduler counters at arm so the reported maxima belong to the measurement window.
- Image: `build/releases/V10.6_R04_TIMING_TRACE_COAST_R10_WINDOW_20261115/`; BIN 19,148 bytes, SHA256 `322ea25a8e3588dd67515fa55ac75583e64d2f6bb38364debbc5ac00f0a7b2302`.
- Download/readback: under-reset; target `py32f002ax5`; full 20 KB readback SHA256 `b68f3e82cd67f2bd2e035b9fa3cd69f8d1faccc3ac6da3d51d4da266c8e3af36`; core Running.
- Capture: monitor armed after attaching; 40.231 seconds, 129 SWD samples; center PWM synchronization passed (`1503→1506→1503` us) under force-coast.
- First-to-last sample deltas: timing samples +39,907; HAL tick +39,904; dropped ticks 0; max loop execution 574 us; executions over 1000 us 0. The initial CSV row was written after approximately 250 loop intervals had elapsed since arm, so the count difference is used for the rate comparison.
- Timing interval histogram and SysTick phase conversion remain uncalibrated. The bins are irregular for a nominal 1 ms loop and cannot establish jitter. Force-coast means there is no physical motor or H-bridge response result.
- Restored R6 force-coast afterward under-reset. Full 20 KB readback SHA256 `89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`; core Running.
