# PY32F002 Servo Firmware

Target MCU: PY32F002AL15S6TU, SOP8.

This project is an independent firmware workspace for a low-cost DC-motor
servo controller. It uses the OpenPuya SDK from Ubuntu:

```sh
PY32_SDK=/opt/py32/PY32F0xx_Firmware
make
```

Core control path:

```text
PA4 input pulse width -> target ADC
PA3 potentiometer ADC -> feedback ADC
target - feedback -> motor direction and duty
```

Firmware structure:

```text
fixed low-level program: GPIO, ADC, PWM input, H-bridge, timer, interrupts
parameter layer: ServoParams in the final 128-byte flash page
control algorithm: reads ServoParams and applies model-specific tuning
```

The parameter layer follows the KC9806-style tuning model: dead band, stretcher,
max duty, boost, pulse lower/high, left/right range, neutral, drive frequency,
speed adjustment, reverse, overload protection, soft start, and lost-signal
behavior.

Generated outputs:

```text
build/py32f002_servo.elf
build/py32f002_servo.hex
build/py32f002_servo.bin
```

The current build is for bench firmware development. SWD flashing/debugging is
reserved for the CMSIS-DAP/DAPLink or J-Link probe after it arrives.
