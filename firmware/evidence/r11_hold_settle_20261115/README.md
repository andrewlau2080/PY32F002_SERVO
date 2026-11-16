# R11 hold settle time verification

- `ServoStabilityTimer` boundary checks: 48/48 pass. Covers 8 ms and 30 ms thresholds, same-tick calls, 2 ms tick intervals, condition loss, saturation, and gaps over 4 ms.
- Production `Servo_Control_Update1ms()` host plant simulation: six direction/range segments completed and entered HOLD. The verifier measured 20, 26, 26, 20, 26, and 20 ms between first stable position/velocity candidate and HOLD. These are deterministic simplified-object results, not hardware acceptance.
- Captured data: `simulation.csv`; run `python3 tools/r11_host_sim/verify_hold_settle.py firmware/evidence/r11_hold_settle_20261115/simulation.csv` to check the six segments.
- Production TJC LCDM build: `build/releases/V10.6_R11_STABILITY_TIME_PRODUCTION_R3_20261115/`, BIN 15,928 bytes, SHA256 `4f55dc44337a45ea1d60de85fded74c3bd57658007cfd2a7ddf424eb4c6df843`, not flashed. `_ebss=0x20000334`; 716 bytes remain after the 1536-byte stack reserve.
- Force-coast LCDM diagnostic build: `build/releases/V10.6_R11_STABILITY_TIME_COAST_R4_20261115/`, BIN 19,180 bytes, not flashed.
- R11 remains partial pending long HOLD and push/release behavior on the real mechanism. Board remains R6 force-coast.
