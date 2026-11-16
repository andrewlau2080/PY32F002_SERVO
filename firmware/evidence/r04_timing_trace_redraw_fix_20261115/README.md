# R04 SysTick timing trace R8

- Purpose: inspect fast-loop start intervals and execution time during automatic 250 ms P50 page redraw, with H-bridge force-coast enabled.
- Image: `build/releases/V10.6_R04_TIMING_TRACE_COAST_R8_20261115/`; BIN 19,220 bytes, SHA256 `241a84a8588189c59d27ad488721334e4c5936d02c3c3dbe034981fb06b4f7a1`.
- Deployment: under-reset, target `py32f002ax5`, DAPLink UID `070000011a5a5cc500000000000c00b6a5a5a5a597969908`. Full 20 KB readback SHA256 `92780a0c0826ace603e7bddccccdf91919a41c3c324e8256ee789af5f69ce3f6`; core Running.
- Capture: 40.242 seconds, 142 SWD samples. Internal PWM synchronization failed; all sampled frames remained 1503 us. Do not count this as the intended PWM-sweep check.
- Timing counters: 89,187 interval samples; bins `<500, 500–899, 900–1100, 1101–1500, 1501–2000, 2001–4000, 4001–10000, >10000 us` = `30246, 7446, 5808, 35055, 10543, 108, 1, 2`. Maximum interval 13,121 us; maximum fast-loop execution 566 us; executions over 1000 us = 0; scheduler dropped ticks = 31.
- Limitation: SysTick sub-millisecond phase conversion is not calibrated against an external timebase; bins are exploratory. Sparse SWD frames do not prove every loop interval. PWM sweep failure means the internal test start/reset timing must be investigated before relying on this trace.
- Board restored afterward to R6 force-coast under-reset. Restored 20 KB readback SHA256 `89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`, core Running.
