# R04 timing trace R9: attach-synchronized sweep

- Purpose: re-baseline the force-coast center sweep only after the SWD observer attaches, so the 30-second startup wait cannot expire before capture.
- Image: `build/releases/V10.6_R04_TIMING_TRACE_COAST_R9_ARMED_20261115/`; BIN 19,240 bytes, SHA256 `8d8b964cfcea9395154a28803b6d96d207d765cd1fc859b269ee0fe2d1d62c62`.
- Download/readback: under-reset; target `py32f002ax5`; full 20 KB readback SHA256 `051b393ea2f30027079df861611dd3d55fc89c60892bb4530f8d90450af1368e`; core Running.
- Capture: monitor armed `g_r04_sweep_start_request` after attaching; 40.037 seconds, 141 SWD samples; center PWM sequence synchronized and passed: `1503→1504→1505→1506→1505→1504→1503` us. Software duty request peak 40/1000; H-bridge force-coast was active, so this is not physical motor output.
- Window deltas: fast-loop interval samples +39,687; HAL tick +39,688; scheduler dropped ticks +0; maximum measured loop execution rose from 513 to 578 us; >1000 us execution count +0. Average loop count was about 999.97 Hz for this capture.
- The SysTick sub-millisecond histogram remains uncalibrated and disagrees with the expected period distribution; do not use it as jitter evidence. Max interval is a since-reset maximum, not a window-local value. The 141 SWD samples cannot prove each cycle or command-to-pin latency.
- Board restored afterward to R6 force-coast under-reset. Full 20 KB readback SHA256 `89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`, core Running.
