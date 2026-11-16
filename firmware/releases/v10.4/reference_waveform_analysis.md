# Customer waveform findings used for V10.4

## Source files

- Direct H-bridge pulse reference: `PY32F002_SIGNAL_CAPTURE/CUSTOMER.logicdata`.
- Additional 2026 captures: `2026/PA7串口*.logicdata`.
- The 2026 files include RX/TX annotations and generic Channel 2/3 labels. Their channel mapping does not identify motor direction or duty by itself, so the directly detected 49.9 kHz Channel 2 pulse groups in `CUSTOMER.logicdata` are the timing reference used here.

## Customer pulse timing

The direct reference contains 16 pulse blocks in four motion clusters. Carrier frequency is 49.89–49.91 kHz. Each cluster spans 11.2–13.3 ms, with 9.2–12.4 ms of detected Channel 2 pulse activity. The clearest final cluster is 7.195 ms, 2.006 ms, and 3.166 ms, which supports a practical 7/2/3 ms drive/reverse/tail timing model. A second cluster includes a short 0.281 ms startup segment before 6.633 ms, 2.006 ms, and 3.447 ms blocks. Since this is a single-channel pulse extraction, phase polarity and exact duty are taken from the established customer profile settings (930/360/220), not inferred from pulse duration alone.

The five 2026 files are retained as supplementary evidence. The generic channel names and RX/TX annotations mean they cannot independently establish which line is forward drive, reverse drive, or coast without a pin map.

## V10.2 comparison

The 99.92-second V10.2 capture contains 734 deduplicated, error-consistent frames. The observed motion commands commonly changed by 500 µs. The firmware's prior 700 µs customer-profile gate excluded those steps, leaving them in normal closed-loop control. Near the center target, 79.2% of valid drive frames with 257–512 ADC error were at 900/1000 duty or higher, while feedback crossed the target repeatedly.

## V10.4 changes

- Trigger the established customer waveform for command changes of at least 400 µs, covering the observed 500 µs steps while ignoring small input jitter.
- Use 7 ms drive, 2 ms reverse brake, and 3 ms tail, followed by the established 24 ms coast gate.
- Do not cancel the timed drive/brake/tail sequence merely because the sampled position error enters the narrow hold band early.
- Preserve V10.3's near-target soft duty cap for movements that remain in normal closed-loop control.

These changes match the best-supported timing evidence. V10.4 stability still requires field confirmation.
