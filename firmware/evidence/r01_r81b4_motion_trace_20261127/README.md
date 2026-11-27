# R81B4 motion-phase trace

- Board image: R81B4; full readback SHA256 `45246fb157ee912008cef872eeb32ae435e681c427a84d140ca04e7c72657891`; 18,368B app matched BIN, final 384B unchanged, core Running.
- One 1500→1550µs internal step, 200‰ cap, 60ms hard cutoff, 8-count guard. Trace count 24; CSV has elapsed ms, raw ADC, observer position/velocity, requested/final duty and phase.
- Start observer ADC 2506. DRIVE continued through 43ms (feedback 2512, velocity 12 Q4). At 51ms feedback 2516 (+10), velocity 14 Q4; guard changed directly to COAST. No reverse PWM ticks. Trace peak raw 2523; post-window snapshot raw 2520, filtered 2522. Active=0, arm=0, H-bridge duty=0, core Running; VDD 3106–3127mV during window.
- This confirms the nominal 28 Q4 stop band classified measured 14 Q4 motion as stopped. Coast continued to about +16 counts in the later readback. It does not validate a corrected braking law.
- `post_trace_snapshot.jsonl` is the SWD readback, and `trace.csv` is its decoded 24-frame controller trace. Candidate BIN SHA256 `709f3a0045cc920b6f6adddd1bc630c477b40879c904142acfb9d7da48d9fa0e`.
