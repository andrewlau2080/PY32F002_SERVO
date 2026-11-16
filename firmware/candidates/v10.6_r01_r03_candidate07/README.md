# V10.6 R01–R03 candidate07

Unflashed candidate containing the R01 overshoot-HOLD counter correction. Built successfully in Parallels Ubuntu from `/media/psf/MOTOR_PY32F002_SERVO` with the pinned PY32 SDK and LCDM field flags; see `ubuntu_build.log`.

Build: text 18,280 B, data 576 B, bss 2,496 B as reported by `arm-none-eabi-size`; the RAM map places 576 B data + 960 B static bss and reserves the configured 1,536 B minimum stack. The 20 KB Flash reserves 384 B for parameter/model pages; application limit is 20,096 B and BIN is 18,856 B (1,240 B headroom).

Not downloaded. The narrow overshoot-HOLD regression is archived under `firmware/evidence/r01_overshoot_hold_fix_20261110/`; R01's 1–3 µs input response and physical range/angle/direction/loss recovery remain unverified.
