# PY32F002 Servo Firmware

Target MCU: PY32F002AL15S6TU, SOP8.

## 当前开发版本约定（更新至 2026-11-10）

用户指定：以已恢复的原始 **P50 V9.6** 为基准，从 **V10.0** 开始继续开发，后续依次 **V10.1、V10.2……**。每个交付测试的修改版本单独编号、留档；失败版本也保留记录，不复用编号。

- 当前板上：`P50 V10.5`，2026-11-10 按用户本次指令经 Ubuntu 下载，整片 Flash 回读后应用区与 18,768 B BIN 一致，核心 Running。下载前后末尾 384 B 均全为 `FF`，固件将按默认参数启动；现场动作、波形和稳定性尚未验证。证据见 [`firmware/releases/v10.5`](firmware/releases/v10.5/verification_20261110.json)。
- 当前研发源码：`P50 V10.6 candidate06`，**未下载**；通用默认 PWM 范围调整为 500–2500 µs（已有有效标定不覆盖），开发板与 SOP8 输入捕获引脚已分配置，通用反馈滤波及学习速度时间归一已修正。两种配置 Ubuntu 构建和离板检查通过；LCDM 1–2 µs HOLD 缺陷仍在、SOP8 和机械响应未实测。候选证据见 [`firmware/candidates/v10.6_r01_r03_candidate06`](firmware/candidates/v10.6_r01_r03_candidate06/README.md)。
- 逐项需求与源码/输出验收状态见[`servo_requirements_acceptance_matrix.md`](docs/servo_requirements_acceptance_matrix.md)，按 R01–R22 维护，不以构建通过替代行为验收。
- Word 版完整控制器要求与验收矩阵：[`servo_controller_requirements.docx`](docs/servo_controller_requirements.docx)（R01–R22，横向 A4）。
- 永久恢复目录：[`firmware/baselines/v9.6_original`](firmware/baselines/v9.6_original/manifest.json)，包含完整 Flash、保留参数区的应用 ELF/BIN、哈希及下载验证记录。不得覆盖。
- V10.0：已完成首轮 V9.6 行为重建并在 Ubuntu 编译通过，394 组离线控制对照及 6 页显示命令对照通过；已下载后因 RAM 参数损坏标记现场失败。后续源码不是逐字节找回的原始 C 文件。归档见 [`firmware/releases/v10.0/recovery_report.html`](firmware/releases/v10.0/recovery_report.html)。
- V10.2现场采集：约100秒，中心目标时原始ADC864–3076、反馈863–3074（两者差值不超过18），确认是实际位置大幅往返；误差257–512区间的有效驱动帧中79.2%达到≥900占空。V10.4沿用V10.3近目标软上限，并将500µs步进纳入客户波形时序。数据与归档见[`firmware/releases/v10.4`](firmware/releases/v10.4/)。
- 每版保留：来源源码或 commit、构建选项、ELF/BIN/HEX、SHA256、下载前备份、读回版本、运行状态、波形与现场结果。区分“编译通过”“下载并运行”“稳定性通过”。
- 下载仍沿用 Ubuntu + DAPLink/HID、`py32f002ax5` 及项目 SWD 规范；LCDM 固定 PA7/PB2、230400bps。构建用 `python3 tools/build_v10_ubuntu.py`（固定 SDK 子集及原版完整 UI）；本次 V10.5 ELF 为 `firmware/releases/v10.5/py32f002_servo.elf`。默认 `build/py32f002_servo.*` 仍是旧 L1，不能误下。

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
parameter layer: ServoParams in the final 128-byte flash page; two 128-byte pages immediately before it are reserved for transactional learned-model memory
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

Debug playbook:

```text
docs/customer_logic_vsg_debug_playbook.md
```

Use that playbook whenever shake, dead loop, slow motion, invisible VSG, LCDM no
display after flash, or customer LOGICDATA comparison comes up. The required
method is one shared customer/VSG comparison page, action-start alignment,
firmware readback, and evidence-first changes.

The V10.6 builds are unflashed development candidates. The board remains on
V10.5; programming and physical acceptance require the documented workflow.

历史对比页：[客户 LOGICDATA 与 V10.2 VSG](build/customer_logic_compare/customer_logic_compare.html)。V10.5 的短时实板遥测见 [`field_20261110_1741`](firmware/releases/v10.5/field_20261110_1741/README.md)；尚无客户双路输出对照，按用户要求不自行回退 V9.6。
