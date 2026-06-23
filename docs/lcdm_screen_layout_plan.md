 LCDM 屏幕界面规划

版本：v0.1

依据：`../2026/` 目录下旧下载器界面图片、平面布线图、测试问题记录、PA7/PB2 串口联调结果，以及当前 `ServoParams v3` 参数结构。

目标：TSSOP20 调试板作为舵机调试工具和参数工装。LCDM 界面优先服务实物调试，不追求菜单少，而是按问题定位和程序参数分层，方便后续一层一层扩展。

补充参考：`../2026/测试工具使用说明书.pdf`。该说明书中的工具具备输出脉宽设置、三点测试、手动脉宽、连续转动、灵敏度测试、接收机/发射机信号测试和 RPM 测速等功能。LCDM 界面需要吸收这些功能习惯，后续调试会更快。

## 1. 已验证硬件基础

| 项目 | 结果 |
|---|---|
| MCU 到 LCDM | `PA7 / MCU_TX -> LCDM_RX` 已验证，可控制背光并刷新页面控件 |
| LCDM 到 MCU | `PB2 / MCU_RX <- LCDM_TX` 已验证，触摸坐标回包正常 |
| 运行提示 | `PA5 / D2` 保留为程序运行心跳 |
| 调试方式 | 当前 `BOOT0/CON3` 接 VCC 下载，烧录后由 pyOCD 写 Flash 向量 `SP/PC` 启动 |

当前界面调试先固定使用：

| 项目 | 值 |
|---|---:|
| 串口 | 230400 bps, 8N1 |
| 结束符 | `0xFF 0xFF 0xFF` |
| MCU TX | `PA7` |
| MCU RX | `PB2` |

## 2. 设计原则

| 原则 | 说明 |
|---|---|
| 主屏先看状态 | 一眼看到输入、目标、反馈、误差、输出、VDD、状态 |
| 调参按问题分组 | 上电乱跑、到位嗞嗞声、慢跨、抖动、ADC/VDD、堵转分别进不同页面 |
| 保留旧下载器语言 | 旧界面的“过冲、轻微震动、虚位、死区、角度校正、驱动方向”继续显示，背后映射到新参数 |
| 读写分开 | 上电不自动读满参数；手动按“读取”后再编辑，避免覆盖上一台调好的参数 |
| 写入必须确认 | 写 Flash 必须二次确认，写后读回 CRC 和参数版本 |
| 页面可继续加深 | 一级菜单只放大类，二级/三级页面放详细参数 |

## 3. 页面层级

```text
P00 主监控
  ├─ P01 主设置菜单
  │   ├─ P10 控制/测试输出
  │   │   ├─ P11 输出脉宽设置
  │   │   ├─ P12 连续摆动测试
  │   │   ├─ P13 手动/灵敏度测试
  │   │   └─ P14 PWM/RPM外部测试
  │   ├─ P20 角度校正
  │   │   ├─ P21 左中右采样
  │   │   └─ P22 输入脉宽/灵敏度
  │   ├─ P30 调节器参数
  │   │   ├─ P31 到位/死区/保持
  │   │   ├─ P32 分段速度/力度
  │   │   └─ P33 反向/慢跨
  │   ├─ P40 驱动器设置
  │   │   ├─ P41 方向/H桥/PWM
  │   │   └─ P42 堵转保护
  │   ├─ P50 ADC与电源诊断
  │   ├─ P60 上电稳定/失信号
  │   ├─ P70 型号模板
  │   ├─ P80 保存/读取/写入SOP8
  │   └─ P90 接线/电源检查
```

旧图片中的三项主菜单：

| 旧入口 | 新位置 |
|---|---|
| 调节器参数设置 | `P30` |
| 角度校正设置 | `P20` |
| 驱动器设置 | `P40` |

新增入口：

| 新入口 | 原因 |
|---|---|
| 主监控 | 当前需要实时看目标/反馈/输出，判断程序是否正常闭环 |
| 控制/测试输出 | 保留 1000/1500/2000us 测试信号，方便无外部 PWM 源调试 |
| 测试工具模式 | 参考小工具说明书，增加三点脉宽、手动脉宽、连续摆动、灵敏度、PWM/RPM 测试 |
| ADC与电源诊断 | 2026 问题记录明确提到 ADC 超过 1V 抖、3.3V 稳压差异 |
| 上电稳定/失信号 | 处理上电乱跑、插电突然到端点再回中 |
| 保存/写入SOP8 | TSSOP20 调完后写入 SOP8 的生产流程 |

## 4. 屏幕通用布局

屏幕按旧界面习惯使用竖屏，建议设计为 `240x320`。如果实际 TJC 工程是横屏，则保留控件名，重新排版即可。

| 区域 | 高度建议 | 内容 |
|---|---:|---|
| 标题栏 | 32 px | 页面名、连接状态、CRC 状态 |
| 内容区 | 232 px | 5-8 行参数或状态，每行包含名称、当前值、单位/说明 |
| 底部栏 | 56 px | 返回、读取、保存、上一项、下一项、确认等按钮 |

通用控件命名：

| 控件 | 用途 |
|---|---|
| `tTitle` | 页面标题 |
| `tStatus` | 状态提示，如 `RUN / HOLD / STALL / CRC OK / UNSAVED` |
| `bBack` | 返回上级 |
| `bRead` | 读取参数 |
| `bSave` | 保存到当前板 |
| `bEnter` | 确认/进入 |
| `bEsc` | 取消/退出 |

屏幕触摸或按键建议统一发 ASCII 命令给 MCU：

| 动作 | LCDM 发送 |
|---|---|
| 返回 | `key=esc` |
| 确认 | `key=enter` |
| 上/下/左/右 | `key=up/down/left/right` |
| 低/中/高测试 | `test=low/mid/high` |
| 停止输出 | `test=off` |
| 切页 | `page=0`、`page=30` |
| 修改参数 | `set,<id>,<value>` |
| 读取参数 | `read=params` |
| 保存参数 | `save=params` |

## 5. P00 主监控

用途：上电后默认页面，用于判断舵机是否正常运行，是否有信号、是否到位、是否在抖、VDD 是否异常。

| 控件名 | 类型 | 显示/功能 | 数据来源 |
|---|---|---|---|
| `n0` | 数字 | 输入脉宽 us | `telemetry.input_us` |
| `n1` | 数字 | 目标 ADC | `telemetry.target_adc` |
| `n2` | 数字 | 反馈 ADC | `telemetry.feedback_adc` |
| `n3` | 数字 | 误差 ADC | `telemetry.error_adc` |
| `n4` | 数字 | 电机输出 duty，带正负 | `telemetry.motor_duty` |
| `n5` | 数字 | VDD mV | `telemetry.vdd_mv` |
| `j0` | 进度条 | 目标位置 | target 映射 0-100 |
| `j1` | 进度条 | 反馈位置 | feedback 映射 0-100 |
| `j2` | 进度条 | 输出力度 | `abs(motor_duty)` |
| `t0` | 文本 | 状态：`NO SIG/HOLD/DRIVE/STALL` | `telemetry.servo_state` |
| `t1` | 文本 | 参数状态：`CRC OK/CRC BAD/UNSAVED` | 参数层 |

底部按钮：

| 按钮 | 命令 | 说明 |
|---|---|---|
| 菜单 | `page=1` | 进入主设置菜单 |
| 测试 | `page=10` | 进入低/中/高测试输出 |
| 诊断 | `page=50` | 进入 ADC/VDD 诊断 |
| 保存 | `page=80` | 进入保存写入流程 |

## 6. P01 主设置菜单

延续旧图 `主设置菜单.jpg` 的行式菜单，建议每行一个入口。

| 行 | 文本 | 目标页 |
|---:|---|---:|
| 1 | 控制/测试输出 | `P10` |
| 2 | 角度校正设置 | `P20` |
| 3 | 调节器参数设置 | `P30` |
| 4 | 驱动器设置 | `P40` |
| 5 | ADC与电源诊断 | `P50` |
| 6 | 上电稳定/失信号 | `P60` |
| 7 | 型号模板 | `P70` |
| 8 | 保存/读取/写入 | `P80` |
| 9 | 接线/电源检查 | `P90` |

控件建议：`m0` 到 `m7` 为菜单行按钮。

## 7. P10 控制/测试输出

参考旧图 `控制界面.jpg` 和 `调节器参数设置.jpg` 中的测试信号低/中/高。

| 控件名 | 类型 | 文本/数值 | 命令 |
|---|---|---|---|
| `bLow` | 按钮 | 测试信号低 | `test=low` |
| `bMid` | 按钮 | 测试信号中 | `test=mid` |
| `bHigh` | 按钮 | 测试信号高 | `test=high` |
| `bOff` | 按钮 | 停止输出 | `test=off` |
| `n10` | 数字 | 当前测试输出 us | MCU 当前测试值 |
| `n11` | 数字 | 当前反馈 ADC | `feedback_adc` |
| `n12` | 数字 | 当前电机 duty | `motor_duty` |
| `t10` | 文本 | 输出模式 | `EXT PWM / LCDM LOW / LCDM MID / LCDM HIGH / OFF` |

说明：这个页用于没有外部 PWM 信号时直接测试舵机低/中/高位置。正式程序里需要一个“LCDM 测试输入覆盖模式”，退出页面或按停止后恢复外部 PWM 输入。

参考 `测试工具使用说明书.pdf` 后，P10 不应只固定 1000/1500/2000us，而应使用可配置三点：

| 旧工具项目 | LCDM 对应 |
|---|---|
| F0-0 低端脉宽，约 300-1998us | `test_low_us` |
| F0-1 中点脉宽，约 1000-1999us | `test_mid_us` |
| F0-2 高端脉宽，约 1000-2699us | `test_high_us` |
| F0-3 步进/灵敏度，约 1-100us | `test_step_us` |

### P11 输出脉宽设置

| 控件名 | 类型 | 默认 | 说明 |
|---|---|---:|---|
| `n110` | 数字输入 | 1000 | 低端测试脉宽 `test_low_us` |
| `n111` | 数字输入 | 1500 | 中点测试脉宽 `test_mid_us` |
| `n112` | 数字输入 | 2000 | 高端测试脉宽 `test_high_us` |
| `n113` | 数字输入 | 10 | 手动步进 `test_step_us` |
| `n114` | 数字输入 | 20 | 连续摆动间隔/速度档 |
| `b115` | 按钮 | - | 应用到当前测试 RAM |

说明：这组值是测试工具自身参数，不等同于舵机 Flash 参数。它用于快速测试不同舵机或不同客户输入范围。

### P12 连续摆动测试

对应说明书 F3“伺服机连续转动测试”，用于自动控制舵机左右摆动，观察速度、回中、过冲、末端慢跨和反向冲击。

| 控件名 | 类型 | 功能 |
|---|---|---|
| `b120` | 按钮 | 开始/停止连续摆动 |
| `n121` | 数字输入 | 低点脉宽，默认引用 `test_low_us` |
| `n122` | 数字输入 | 高点脉宽，默认引用 `test_high_us` |
| `n123` | 数字输入 | 每端停留 ms |
| `n124` | 数字输入 | 摆动次数，0 表示连续 |
| `n125` | 数字 | 当前循环次数 |
| `t126` | 文本 | `SWEEP RUN / SWEEP STOP` |

### P13 手动/灵敏度测试

对应说明书 F2/F4：手动调输出脉宽和灵敏度测试。用于验证 1-3us 命令变化时舵机是否响应，以及是否出现小步抖动。

| 控件名 | 类型 | 功能 |
|---|---|---|
| `n130` | 数字 | 当前输出脉宽 |
| `n131` | 数字输入 | 步进 us，默认 `test_step_us` |
| `b132` | 按钮 | `-step` |
| `b133` | 按钮 | `+step` |
| `b134` | 按钮 | 回中 |
| `n135` | 数字 | 反馈 ADC |
| `n136` | 数字 | 反馈变化量 |
| `t137` | 文本 | `MOVE / NO MOVE / JITTER` |

判断建议：

| 条件 | LCDM 显示 |
|---|---|
| 输出改变 1-3us，反馈有稳定变化 | `MOVE` |
| 输出改变但反馈未动 | `NO MOVE` |
| 输出不变但反馈来回跳 | `JITTER` |

### P14 PWM/RPM外部测试

对应说明书 F5/F6/F7，用于将来把 TSSOP20 工具板做成更完整的小工具。当前硬件如未接 RPM 光电输入，可先保留页面。

| 控件名 | 类型 | 功能 |
|---|---|---|
| `n140` | 数字 | 外部 PWM 输入脉宽 us |
| `n141` | 数字 | 外部 PWM 周期 ms |
| `n142` | 数字 | 外部 PWM 频率 Hz |
| `n143` | 数字 | RPM 当前值 |
| `n144` | 数字 | RPM 峰值 |
| `b145` | 按钮 | 清除 RPM 峰值 |
| `t146` | 文本 | `PWM OK / NO PWM / RPM OK / NO RPM` |

说明书中的 RPM 最大支持到 `99990 rpm`，显示值带 `x10` 关系。若后续需要该功能，硬件需预留独立光电/RPM 输入，不建议占用 SOP8 最终 IO。

## 8. P20 角度校正

参考旧图 `角度校正设置.jpg`，但改成更适合当前 ADC 闭环的方式。

### P20 概览

| 控件名 | 类型 | 显示/功能 | 对应参数 |
|---|---|---|---|
| `n20` | 数字 | 中点偏移 | `neutral_offset_count` |
| `n21` | 数字 | 左角微调 | `left_range_count` 或 `adc_min_count` |
| `n22` | 数字 | 右角微调 | `right_range_count` 或 `adc_max_count` |
| `n23` | 数字 | 灵敏度/死区 us | `deadband_us` |
| `n24` | 数字 | 当前输入 us | telemetry |
| `n25` | 数字 | 当前 ADC | telemetry |
| `j20` | 进度条 | 反馈位置 | telemetry |

### P21 左中右采样

| 按钮 | 命令 | 动作 |
|---|---|---|
| 到左端 | `test=low` | 输出左端测试位置 |
| 采左端 | `cal=left` | 把当前 ADC 写入待保存 `adc_min_count` |
| 到中点 | `test=mid` | 输出中位 |
| 采中点 | `cal=center` | 写入 `adc_center_count` |
| 到右端 | `test=high` | 输出右端 |
| 采右端 | `cal=right` | 写入 `adc_max_count` |

### P22 输入脉宽/灵敏度

| 控件名 | 类型 | 对应参数 | 说明 |
|---|---|---|---|
| `n220` | 数字输入 | `pulse_lower_us` | 默认 900 或实际低点 |
| `n221` | 数字输入 | `pulse_center_us` | 默认 1500 |
| `n222` | 数字输入 | `pulse_high_us` | 默认 2100 |
| `n223` | 数字输入 | `deadband_us` | 1-3us 灵敏度目标 |
| `n224` | 数字输入 | `deadband_count_min` | ADC 最小死区 |

## 9. P30 调节器参数

参考旧图 `调节器参数设置.jpg`，保留四个调试语言：

| 旧界面语言 | 新参数组 | 主要解决 |
|---|---|---|
| 过冲/卡顿 | 分段增益、起动补偿、最大 duty | 快到点摆过去、末端慢跨 |
| 回中不准/轻微震动 | 死区、保持模式、close 段 | 锁附抖动、回中抖 |
| 齿轮紧/虚位大 | close/approach 增益和 boost | 塑胶齿虚位、金属齿手感 |
| 走电流/死区大 | hold/brake/deadband | 到位静态电流 200-300mA、马达嗞嗞声 |

### P31 到位/死区/保持

| 控件名 | 类型 | 对应参数 | 说明 |
|---|---|---|---|
| `n310` | 数字输入 | `deadband_us` | 输入死区 |
| `n311` | 数字输入 | `deadband_count_min` | ADC 死区 |
| `n312` | 数字输入 | `hold_mode` | 0滑行/1刹车/2短刹后滑行 |
| `n313` | 数字输入 | `hold_exit_band_count` | HOLD 退出滞回 |
| `n314` | 数字输入 | `hold_brake_time_ms` | 短刹时间 |
| `n315` | 数字输入 | `brake_band_count` | 小误差刹车范围 |

### P32 分段速度/力度

运行时测试固件在 2026-06-06 先采用简化 P+D 闭环：`P gain` 负责误差输出，`D damp` 负责运动方向阻尼，`Boost duty` 负责起动补偿，`Max duty` 负责最大锁力。正式 TJC 工程仍可按下表拆成 close/approach/large 三段参数。

| 控件名 | 类型 | 对应参数 | 说明 |
|---|---|---|---|
| `n320` | 数字输入 | `close_error_count` | 小误差区范围 |
| `n321` | 数字输入 | `close_stretcher_q8` | 小误差增益 |
| `n322` | 数字输入 | `close_boost_duty` | 小误差起动补偿 |
| `n323` | 数字输入 | `approach_error_count` | 接近区范围 |
| `n324` | 数字输入 | `approach_stretcher_q8` | 接近区增益 |
| `n325` | 数字输入 | `approach_boost_duty` | 接近区补偿 |
| `n326` | 数字输入 | `stretcher_q8` | 大误差增益 |
| `n327` | 数字输入 | `boost_duty` | 大误差补偿 |
| `n328` | 数字输入 | `max_duty` | 最大输出 |

### P33 反向/慢跨

| 控件名 | 类型 | 对应参数 | 说明 |
|---|---|---|---|
| `n330` | 数字输入 | `forward_speed_q8` | 正向速度比例 |
| `n331` | 数字输入 | `reverse_speed_q8` | 反向速度比例 |
| `n332` | 数字输入 | `reverse_pause_ms` | 反向前暂停 |
| `n333` | 数字输入 | `reverse_brake_ms` | 反向前短刹 |
| `n334` | 数字输入 | `startup_step_count` | 上电首次追目标斜坡 |

## 10. P40 驱动器设置

参考旧图 `驱动器设置.jpg`。

### P41 方向/H桥/PWM

| 控件名 | 类型 | 对应参数/功能 | 说明 |
|---|---|---|---|
| `sw410` | 开关 | 旋转方向反转 | `SERVO_PARAM_FLAG_INVERTER` |
| `sw411` | 开关 | 电位器方向反转 | 软件映射方向 |
| `n412` | 数字输入 | PWM 频率选择 | 例如 330Hz / 1kHz |
| `n413` | 数字输入 | `max_duty` | 最大 H 桥 duty |
| `t414` | 文本 | H 桥状态 | `COAST / BRAKE / DRIVE+ / DRIVE-` |

### P42 堵转保护

| 控件名 | 类型 | 对应参数 |
|---|---|---|
| `n420` | 数字输入 | `stall_duty_threshold` |
| `n421` | 数字输入 | `stall_min_move_count` |
| `n422` | 数字输入 | `stall_time_ms` |
| `n423` | 数字输入 | `stall_recovery_ms` |
| `sw424` | 开关 | `SERVO_PARAM_FLAG_OL_PROTECT` |

## 11. P50 ADC与电源诊断

这个页面是根据 2026 问题记录新增的重点页面。

| 控件名 | 类型 | 显示/功能 | 数据来源/参数 |
|---|---|---|---|
| `n500` | 数字 | ADC raw | 原始反馈 |
| `n501` | 数字 | ADC filtered | 滤波反馈 |
| `n502` | 数字 | ADC min | 运行期间最小值 |
| `n503` | 数字 | ADC max | 运行期间最大值 |
| `n504` | 数字 | ADC 抖动峰峰值 | `max-min` |
| `n505` | 数字 | VDD mV | VREFINT 估算 |
| `n506` | 数字 | VDD 跌落 mV | nominal - current |
| `n507` | 数字输入 | `adc_sample_count` | 采样平均次数 |
| `n508` | 数字输入 | `adc_filter_shift` | IIR 滤波 |
| `n509` | 数字输入 | `adc_jump_limit_count` | 跳变限幅 |
| `n510` | 数字输入 | `vdd_warn_drop_mv` | 电源报警阈值 |
| `t511` | 文本 | 诊断提示 | `ADC OK / ADC JITTER / VDD DROP` |

用途：

| 测试 | 判断 |
|---|---|
| 电位器慢慢转过 1V 附近 | 看 ADC raw/filtered 是否突然抖 |
| 马达启动/反向时 | 看 VDD 是否跌落，ADC 是否同步跳 |
| 更换 LDO/电容后 | 对比 VDD noise 和 ADC peak-to-peak |

## 12. P60 上电稳定/失信号

| 控件名 | 类型 | 对应参数 | 解决问题 |
|---|---|---|---|
| `n600` | 数字输入 | `Range mode` | PWM 超出 P20 `Input low/high` 后的保护策略：`1` 锁到低/高极限，`0` 空载释放；运行测试固件默认 `1` |
| `n601` | 数字输入 | `startup_delay_ms` | 上电后先等电源/ADC 稳 |
| `n602` | 数字输入 | `startup_input_stable_count` | 输入连续稳定次数 |
| `n603` | 数字输入 | `startup_input_stable_us` | 输入稳定阈值 |
| `n604` | 数字输入 | `startup_adc_stable_count` | ADC 连续稳定次数 |
| `n605` | 数字输入 | `startup_adc_stable_band_count` | ADC 稳定阈值 |
| `n606` | 数字输入 | `startup_step_count` | 首次追目标速度限制 |
| `n607` | 数字输入 | `input_timeout_ms` | 失信号时间 |
| `sw608` | 开关 | `SERVO_PARAM_FLAG_LOSE_PPM_ENABLE` | 失信号回中 |
| `sw609` | 开关 | `SERVO_PARAM_FLAG_LOSE_PPM_LOCK` | 失信号锁当前位置 |

## 13. P70 型号模板

| 控件名 | 类型 | 功能 |
|---|---|---|
| `n700` | 数字输入 | `model_id` |
| `n701` | 数字输入 | `profile_id` |
| `b702` | 按钮 | 塑胶齿默认模板 |
| `b703` | 按钮 | 金属齿默认模板 |
| `b704` | 按钮 | 快速模板 |
| `b705` | 按钮 | 静音模板 |
| `b706` | 按钮 | 高保持力模板 |

模板按钮只修改 RAM 中待保存参数，必须到 `P80` 保存后才写 Flash。

## 14. P80 保存/读取/写入SOP8

| 控件名 | 类型 | 功能 |
|---|---|---|
| `b800` | 按钮 | 从当前 TSSOP20 板读取参数 |
| `b801` | 按钮 | 保存到当前 TSSOP20 板 Flash |
| `b802` | 按钮 | 读回校验 |
| `b803` | 按钮 | 恢复默认值到 RAM |
| `b804` | 按钮 | 写入 SOP8 目标板 |
| `b805` | 按钮 | 读取 SOP8 CRC |
| `n806` | 数字 | 参数版本 |
| `n807` | 数字 | 参数长度 |
| `x808` | 文本/十六进制 | 当前 CRC |
| `t809` | 文本 | `READ OK / SAVE OK / CRC FAIL / SOP8 PASS` |

写入 SOP8 的确认流程：

```text
选择“写入SOP8”
  -> 弹出确认页：请确认 PB1 已接 SOP8 PA4，GND 已共地
  -> ENTER 后写入完整 ServoParams
  -> 读回 CRC
  -> 显示 PASS/FAIL
```

## 15. P90 接线/电源检查

参考 `测试工具使用说明书.pdf` 第一页功能介绍：原小工具明确区分 DC 7-15V 输入、BEC 5V 输入、正/负相信号输出端、信号输入端、电源指示、RPM 输入和电池方向。我们的 LCDM 应增加这个页面，减少现场接线错误。

| 控件名 | 类型 | 显示/功能 | 说明 |
|---|---|---|---|
| `t900` | 文本 | LCDM UART | `PA7->RX / PB2<-TX / GND` |
| `t901` | 文本 | SWD | `SWDIO PA13 / SWCLK PA14 / NRST PF2` |
| `t902` | 文本 | BOOT0 | 当前 TSSOP20 下载需 `CON3=VCC` |
| `t903` | 文本 | H桥LED测试 | 当前 H 桥输出接 LED 防烧元件 |
| `n904` | 数字 | MCU VDD mV | 当前 `telemetry.vdd_mv` |
| `t905` | 文本 | LCDM供电 | 屏幕独立供电，必须共地 |
| `t906` | 文本 | SOP8写参线 | `PB1 -> SOP8 PA4`，串 1k-4.7k |
| `t907` | 文本 | 风险提示 | 电源反接/信号接反/BOOT0状态 |

如果后续工具板增加电源采样硬件，可扩展：

| 控件名 | 类型 | 未来输入 |
|---|---|---|
| `n910` | 数字 | 外部 DC 输入电压 |
| `n911` | 数字 | BEC/舵机 5V 输入 |
| `n912` | 数字 | 舵机电流 |
| `t913` | 文本 | `POWER OK / LOW V / REVERSE RISK` |

## 16. 测试工具说明书带来的界面修改结论

| 修改项 | 原规划 | 修改后 |
|---|---|---|
| 测试输出 | 固定低/中/高 1000/1500/2000us | 三点脉宽可编辑，另加步进 us |
| 灵敏度测试 | 只调 `deadband_us` | 增加手动 `+step/-step`，直接观察 1-3us 响应 |
| 连续摆动 | 未单独成页 | 增加 P12，专门看速度、过冲、末端慢跨、反向冲击 |
| 外部 PWM/RPM | 未规划 | 增加 P14，后续工具板可测接收机、发射机和转速 |
| 接线电源 | 分散在文档 | 增加 P90，现场调试前先确认接线、电源、BOOT0、写参线 |
| 参数读写 | 已有 P80 | 保持手动读取/保存，符合说明书“先选择再确认”的使用习惯 |

优先实现顺序也要调整：

| 优先级 | 页面 | 原因 |
|---:|---|---|
| 1 | P00 主监控 | 运行状态必须先可见 |
| 2 | P10/P11/P13 | 三点输出和 1-3us 灵敏度是调舵机最常用动作 |
| 3 | P20 | 左中右角度采样 |
| 4 | P30/P50 | 调控制参数，同时观察 ADC/VDD |
| 5 | P12 | 连续摆动定位慢跨/过冲 |
| 6 | P80/P90 | 保存写入和接线检查 |
| 7 | P14 | PWM/RPM 外部测试，视后续硬件是否支持 |

## 17. 参数编辑规则

| 规则 | 说明 |
|---|---|
| 修改后只进 RAM | 所有数值输入先写入待保存参数，不立即写 Flash |
| 退出页面提示未保存 | `tStatus` 显示 `UNSAVED` |
| 保存前范围检查 | 防止 duty、ADC、时间参数越界 |
| 保存后读回 | 必须读回 CRC 和关键参数 |
| 支持复制到下一台 | 上电不自动覆盖 RAM，用户可以直接写入新舵机 |

## 17A. LCDM 缓存和刷新规则

正式 TJC 工程中，页面、标题、菜单文字、按钮、分隔线等静态内容都保存在 LCDM 内部 Flash/工程资源里。MCU 不应周期性重传整页资料。

| 内容类型 | 放置位置 | 刷新方式 |
|---|---|---|
| 页面布局、固定文字、按钮 | TJC `.HMI/.tft` 工程 | 屏幕本地显示，不由 MCU 周期发送 |
| 当前脉宽、ADC、VDD、状态 | MCU 遥测 | 数值变化后才发送对应控件 |
| 参数表当前值 | MCU RAM/Flash 参数 | 进入页面或参数被修改后发送 |
| 保存/CRC 状态 | MCU 状态机 | 状态变化后发送 |

固件侧保留一份“上次已发送值”缓存。上电、切页或屏幕发送 `refresh` 时清缓存并补发当前页动态值；平时只比较变化字段，没变就不发，避免串口被整页刷新占满。

## 18. 固件实现顺序

| 顺序 | 工作 | 验证方式 |
|---:|---|---|
| 1 | 固定页面号和控件名 | 当前 LCDM 能切页并显示测试值 |
| 2 | 主监控改为真实遥测 | 调 PWM、电位器、马达 LED，看数值同步变化 |
| 3 | P10/P11/P13 测试输出覆盖 PWM 输入 | 低/中/高、手动步进、1-3us 灵敏度测试 |
| 4 | P20 采样左中右 ADC | 采样后参数 RAM 变化，进度条映射正确 |
| 5 | P30/P40 参数输入 | 修改 RAM 参数后控制手感变化 |
| 6 | P50 诊断 | 记录 ADC/VDD 抖动，辅助硬件整改 |
| 7 | P12 连续摆动 | 观察过冲、慢跨、反向冲击 |
| 8 | P80 保存/读回 | Flash CRC 正确 |
| 9 | SOP8 写参 | PB1 -> SOP8 PA4 写入并读回 CRC |
| 10 | P14 PWM/RPM 外部测试 | 后续硬件支持后实现 |

## 19. 当前最小 TJC 工程要求

为了先让固件跑起来，TJC 工程至少需要建立这些页面和控件：

| 页面 | 最少控件 |
|---|---|
| `page0` 主监控 | `n0 n1 n2 n3 n4 n5 j0 j1 j2 t0 t1` |
| `page1` 主菜单 | `m0 m1 m2 m3 m4 m5 m6 m7` |
| `page10` 控制测试 | `bLow bMid bHigh bOff n10 n11 n12 t10` |
| `page11` 输出设置 | `n110 n111 n112 n113 n114 b115` |
| `page13` 手动灵敏度 | `n130 n131 b132 b133 b134 n135 n136 t137` |
| `page20` 角度校正 | `n20 n21 n22 n23 n24 n25 j20` |
| `page30` 调节参数 | `n310 n311 n312 n313 n314 n315` |
| `page40` 驱动设置 | `sw410 sw411 n412 n413 t414` |
| `page50` PWM LED | `n500 n501 n502 n503 n504 t511` |
| `page80` 保存写入 | `b800 b801 b802 b803 b804 b805 n806 n807 x808 t809` |
| `page90` 接线电源 | `t900 t901 t902 t903 n904 t905 t906 t907` |

如果先做不完全部页面，优先顺序为：`page0 -> page1 -> page10 -> page11 -> page13 -> page20 -> page30 -> page50 -> page80 -> page90`。

## 20. 当前已烧录的运行时界面测试

2026-06-06 已写入不依赖 `.HMI/.tft` 工程控件的运行时界面测试固件。该固件用 `PA7` 以 230400bps 直接发送 TJC 画面指令，使用 `cls`、`fill`、`xstr` 在 LCDM 当前页面上画界面。

当前版本标识：主菜单标题显示 `P01 Main Menu GRID V9`。该版所有主页面统一 5 行淡底色间隔；主菜单为左右两列、每列 5 个大触摸格。整页切换时使用 `ref_stop/ref_star` 批量刷新，命令间隔为 2 ms，清屏等待为 30 ms，避免文字一行一行明显出现。启动时会从 9600、38400、115200 和目标波特率尝试切换屏幕波特率，避免上一次固定在 115200 后高波特率版本无法接管。当前开机默认进入 `P50 PWM Input LED`，用于现场先调 PWM 输入和马达 LED 映射。

字体策略：GRID V9 使用 `LCDM_TJC_FONT_ID=1` 尝试让全部 `xstr` 文本稍微放大，同时把标题、行内容、主菜单和底部按钮的文字框高度加大，减少放大后裁切。若屏幕工程没有字体 1，可用 `LCDM_TJC_FONT_ID_OVERRIDE=0` 快速退回字体 0。

触摸策略：运行时绘图版使用 `sendxy=1` 接收屏幕原始触摸坐标。GRID V8 将主菜单改为按下立即进入页面，不再等待松手；底部按钮命中从 `y=232` 起算，顶部行命中保留 8 px 容错，减少手指点在边缘时无反应的问题。触摸 RX 改为 `USART1_RXNE` 中断加 64 字节环形缓冲，MCU 在阻塞发送 `xstr/fill` 画面指令时仍可接收坐标；整页刷新结束后重发 `sendxy=1`，避免第二次点击被刷新期间积压的旧坐标或 USART overrun 拖慢。

编译开关：

```bash
SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1
```

已轮播页面：

| 页面 | 当前运行时显示内容 |
|---|---|
| `P00` | 主监控：输入、目标、反馈、误差、输出、VDD、状态 |
| `P01` | 主菜单：控制测试、角度校正、调节器、驱动器、PWM LED、保存写入、接线检查 |
| `P10` | 输出测试：低/中/高、手动输出、步进、测试模式 |
| `P13` | 手动/灵敏度测试：当前脉宽、3us 步进、反馈变化、MOVE/NO MOVE |
| `P20` | 角度校正：左中右 ADC、输入脉宽、死区 |
| `P30` | 调节器参数：死区、分段增益、boost、max duty、保持和反向 |
| `P50` | PWM 闭环诊断：pulse(us)、Target(mV/ADC)、Feedback(mV/ADC)、Error、H-bridge |
| `P80` | 保存/读取/写入 SOP8：读参、保存、CRC、PB1->PA4 |
| `P90` | 接线/电源检查：LCDM、SWD、BOOT0、H桥 LED、共地、D2 |

烧录命令：

```bash
./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_TJC_UART_ALT_PINS=1 BOARD_TSSOP20_DEBUG_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 LCDM_TJC_CMD_GAP_MS_OVERRIDE=2 LCDM_TJC_PAGE_CLEAR_MS_OVERRIDE=30 LCDM_TJC_FONT_ID_OVERRIDE=1
strings -a build/py32f002_servo.elf | rg 'P50 PWM Servo Loop|P gain|D damp|DIR ALARM|Dir protect|Boost duty|Max duty|sendxy=1'
./tools/flash_run_on_macos.sh
```

验证状态：

| 项目 | 结果 |
|---|---|
| LCDM 运行时界面编译 | 通过，LTO 版 `text=17868 B`，`data=1124 B`，`bss=1516 B` |
| 本地固件校验 | `strings` 可见 `P50 PWM Servo Loop`、`P gain`、`D damp`、`DIR ALARM`、`Dir protect`、`Boost duty`、`Max duty`、`sendxy=1` |
| 烧录 | 通过，使用本机 `build/py32f002_servo.hex`，SHA256 `a3b54fed5c406bcf3ecf110a3c45f3d704d3156999483ece6758b2e6ff9af134` |
| 启动 | pyOCD 已读运行向量 `SP=0x20000c00`、`PC=0x08003d01` 并 `Successfully resumed device` |

P50 PWM Servo Loop 映射：PA4 输入标准舵机 PWM，高电平脉宽单位是 `us`。约 50 Hz 时周期应接近 `20000 us`。当前 P50 抓取显示范围为 `300-2340 us`，不再受正式舵机参数表 `900-2100 us` 限制。闭环当前按 `PWM_us = 目标 mV` 处理，但目标电压钳位在 `1000-2000 mV`，例如 `1000 us -> 1000 mV`，`1500 us -> 1500 mV`，`2000 us -> 2000 mV`，超出该范围也只输出 1.0-2.0 V 目标，防止打到电位器两端；后续稳定后再扩大范围。P50 每约 150 ms 更新数值，闭环控制每约 2 ms 更新一次。到位后 H 桥按 P30 Hold mode 刹车/锁定；马达方向接反时，在 P40 Driver 页面第一行 `Motor invert` 从 `0/1` 切换。P50 底部右键 `P40 DIR` 可直接进入 P40。P40 第二行 `Dir protect` 默认开启：驱动时如果 ADC 在目标稳定、输出足够、误差较大时连续约 160 ms 朝目标反方向移动，P50 的 `H-bridge` 行显示 `DIR ALARM` 并关闭 H 桥；手动快速扭电位器造成的大幅 ADC 跳动不计入报警。当前为主动锁齿调试版：P30 默认 `ADC deadband=8`、`P gain=220`、`D damp=320`、`Boost duty=90`，P30/P40 默认 `Max duty=850`。HOLD 退出滞回从 28 count 缩小到 4 count；已经到位后若被外力拨开，会用不低于 `180/1000` duty 主动反打锁齿，但正常追目标时仍靠 D 阻尼压过冲。

2026-06-06 主动锁齿版下载记录：编译通过，LTO 版 `text=17872 B`，`data=1124 B`，`bss=1516 B`；HEX SHA256 `73d33ae9a4cb68607b5942b99b15d1e6f59f7186bd1eb51503858a011cc7a0f0`；pyOCD 启动向量 `SP=0x20000c00`、`PC=0x08003d05`，并返回 `Successfully resumed device`。

2026-06-06 强制锁齿测试版下载记录：先不调滑行速度和刹车距离，只验证 H 桥能否用正/反向大电流把舵板锁在 ADC 目标点。运行时参数改为 `ADC deadband=2`、`Lock duty=1000`、`Max duty=1000`，额外 ADC 滤波改为超过 1 count 立即跟随，`LCDM_SERVO_FORCE_LOCK_MODE=1` 时误差超过死区就直接按 `Lock duty` 输出，不走 P/D 减力。编译通过，LTO 版 `text=17612 B`，`data=1124 B`，`bss=1508 B`；HEX SHA256 `9986dbda914eb867c6d3d58bdd9c451ca4c0a9beb7b0336ca00673d4f276d6e5`；pyOCD 启动向量 `SP=0x20000c00`、`PC=0x08003c01`，并返回 `Successfully resumed device`。

2026-06-06 强力比例锁齿版下载记录：针对全 duty 正反打找不到平衡点的问题，保留大锁力上限，但目标点附近改为比例输出。运行时参数 `ADC deadband=8`、`P gain=280`、`D damp=360`、`Lock duty=900`、`Max duty=1000`；误差在 40 count 内限制为最多半锁力，误差达到 160 count 以上才使用 `Lock duty` 上限，并用 D damp 抑制朝目标运动时的过冲。编译通过，LTO 版 `text=17872 B`，`data=1124 B`，`bss=1516 B`；HEX SHA256 `5ca09ceeda43af8afbda899ede23d308d67d3c792b4ea45d4f671976583952a5`；pyOCD 启动向量 `SP=0x20000c00`、`PC=0x08003d05`，并返回 `Successfully resumed device`。

2026-06-06 分层指数锁齿版下载记录：确认 1500us 左右仍然来回摆后，把单一比例改成 ADC 误差分层曲线。运行时参数 `ADC deadband=18`、`P gain=256`、`D damp=640`、`Lock duty=900`、`Max duty=1000`；内置误差层级为 `4/8/16/32/64/96/128/180/260/380 count`，对应 duty 比例约 `3%/5%/7%/11%/17%/25%/34%/48%/68%/87%`，超过最高层才接近全锁力；进入 80 count 以内且已朝目标运动时优先刹车进入 HOLD。编译通过，LTO 版 `text=17980 B`，`data=1124 B`，`bss=1516 B`；HEX SHA256 `7af8c07b772afd2a257536ffecdf4b3f6e4fbc24b61191ee71fbb0bdefea0c40`；pyOCD 启动向量 `SP=0x20000c00`、`PC=0x08003d49`，并返回 `Successfully resumed device`。

2026-06-09 软前段分层曲线下载记录：继续压低目标点附近输出，优先解决 1500us 左右摆动。运行时参数 `ADC deadband=28`、`P gain=192`、`D damp=760`、`Lock duty=850`、`Max duty=1000`；HOLD 退出滞回增至 18 count，接近目标刹车区扩大到 130 count，最小锁齿 duty 降到 12。误差层级为 `4/8/16/32/64/96/128/180/260/380/560 count`，前段 duty 比例降为约 `1%/1%/2%/3%/5%/9%/14%/23%/36%/58%/86%`。编译通过，LTO 版 `text=17984 B`，`data=1124 B`，`bss=1516 B`；HEX SHA256 `d23e65306d5d752087da81593f9c21fd1c97af63d72650a728aa93530622aab1`；macOS 未看到 DAPLink，改用 Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003d49`，并返回 `Successfully resumed device`。

2026-06-09 PWM/ADC 行程映射下载记录：按电位器总角度 220 度、低端 0.3V、高端 3.0V、舵机工作角 180 度重新规划映射。默认工作区居中留边 20 度/侧：`500us -> 545mV`、`1500us -> 1650mV`、`2500us -> 2755mV`，因此 `1000-2000us` 对应中间 90 度。P20 改为 5 个可见可调项：`Low mV`、`Mid mV`、`High mV`、`Input low`、`Input high`；P50 改为显示 `Target ADC` 和 `Current ADC`，格式为 `ADC xxxx xxxxmV`，便于空载读取上下极限 ADC。编译通过，LTO 版 `text=18076 B`，`data=1076 B`，`bss=1516 B`；HEX SHA256 `cb0cf1bde2bf151db52cc65ca9d4e80e1237c3371dde796dea20c39de033d7c7`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003dc1`，并返回 `Successfully resumed device`。

2026-06-09 实测电位器端点修正：万用表测得最低约 `0.18V`、最高约 `3.16V`，LCDM 显示 `ADC212 172mV`、`ADC3900 3166mV`。ADC 原始码和电压换算一致，主要不是 MCU ADC 大误差；低端 8mV 左右差异可能来自表笔位置、地线压差、低端接触电阻、采样噪声或源阻抗。按 LCDM 实测端点和 220 度电位器总行程重新计算 180 度工作区，默认改为 `500us -> 444mV`、`1500us -> 1669mV`、`2500us -> 2894mV`。编译通过，LTO 版 `text=18076 B`，`data=1076 B`，`bss=1516 B`；HEX SHA256 `cb79065358776c94950d4f73a476f6ba523776f7956bfc408af68578bd1dd111`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003dc1`，并返回 `Successfully resumed device`。

2026-06-09 高端 3.0V 行程修正：实测高端留边过多，`2500us` 没有足够响应；按用户要求把高端目标设到 `3000mV`。用 LCDM 实测总行程 `172-3166mV`、220 度总角度推算 180 度工作区电压跨度约 `2450mV`，因此默认改为 `500us -> 550mV`、`1500us -> 1775mV`、`2500us -> 3000mV`。这样 `500-2500us` 对应 180 度，`1000-2000us` 对应中间 90 度。编译通过，LTO 版 `text=18076 B`，`data=1076 B`，`bss=1516 B`；HEX SHA256 `b20cd3d4ecd59b33e16827fd6f813384b24b2a92fbb455d9e675fe026812a4d6`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003dc1`，并返回 `Successfully resumed device`。

2026-06-09 PWM 捕获上限漏查修正：把舵机控制范围扩展为 `500-2500us` 后，PWM 捕获上限仍停留在 `2340us`，导致高端如 `2100us` 以上存在被判 `BAD PWM`、目标不更新或无反应的风险。这是范围修改后未全链路核查造成的人为错误。已把 `SERVO_INPUT_CAPTURE_MAX_US` 改为 `2700us`，并修正 P50 显示：PWM 无效时 `Target ADC` 和 `Error` 清为 `--`，避免旧目标和新状态混在一起；PWM pulse 行增加周期显示，格式为 `pulse_us period_us`，便于判断是脉宽还是周期异常。后续任何范围/参数修改必须同步检查：输入捕获范围、有效性判定、映射函数、保护阈值、P50 显示、P20/P30/P40 可调范围、文档记录和实板下载结果。编译通过，LTO 版 `text=18164 B`，`data=1076 B`，`bss=1516 B`；HEX SHA256 `c5a81b86a12b9c3d2fb188a61ce6cb3a565e34eff92743c7d3619a77c5c6825e`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003e15`，并返回 `Successfully resumed device`。

2026-06-09 行程两端再扩下载记录：按用户实测角度仍不足，默认目标改为 `500us -> 250mV`、`1500us -> 1650mV`、`2500us -> 3050mV`。映射函数仍以 `1500us` 为中心分段线性计算，所以 `1000us -> 950mV`、`2000us -> 2350mV`，正好是 `250-3050mV` 全跨度的一半，对应 180 度总工作角里的中间 90 度。同步核查范围链路：PA4 捕获有效上限 `SERVO_INPUT_CAPTURE_MAX_US=2700us`，P20 `Low/Mid/High mV` 可调范围覆盖 `250/1650/3050mV`，P50 显示 `pulse_us period_us`、`Target ADC`、`Current ADC`，无效 PWM 时清空目标和误差。编译通过，LTO 版 `text=18164 B`，`data=1076 B`，`bss=1516 B`；HEX SHA256 `a7a0e2bde70dfb72fc6b2eb8460a15be3f210663fc4d3a7cd12accda2b1d1529`；ELF SHA256 `2767c353ec9f6aff9f70eb7dbf3337fc2c9102e0bdadd9277220d81b0a13dfb2`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003e15`，并返回 `Successfully resumed device`。

2026-06-09 PWM 越界保护下载记录：补充 PWM 宽度和 ADC 目标值保护。P20 的 `Input low/high` 仍为 `500/2500us`，P20 的 `Low/High mV` 仍为 `250/3050mV`，PA4 捕获有效上限仍为 `2700us`。当 PWM 已被捕获但低于 `Input low` 或高于 `Input high` 时，P60 第一行 `Range mode` 决定保护动作：默认 `1` 锁到低/高 ADC 极限目标，P50 的 H-bridge 行显示 `LOW LIM` 或 `HIGH LIM`；改为 `0` 时 H 桥空载释放，P50 显示 `RANGE OFF`，并清空 Target/Error。固件字符串已核验可见 `Range mode`、`LOW LIM`、`HIGH LIM`、`RANGE OFF`。编译通过，LTO 版 `text=18432 B`，`data=1052 B`，`bss=1516 B`；HEX SHA256 `66e06b4d9207a817a8e1519395334ffd20b653850fd6684a13dfd7bb5410cc03`；ELF SHA256 `19a86400fe6f8ba73bb7f40d3bf9bce244e480ea78fb7256b6ee2be429c992bc`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003f19`，并返回 `Successfully resumed device`。

2026-06-09 固定 PWM 目标锁定下载记录：针对“PWM 没变化但 ADC 小漂动不应改变目标、外力改变 ADC 后要快速反向锁住”的问题，增加 PWM 目标锁存。P60 `Input stable band` 默认 `3us`，PWM 脉宽变化小于等于该值时不更新目标，P50 仍显示真实 PWM pulse，但 `Target ADC` 使用锁存目标；只有脉宽变化超过该 band 或进入越界保护时才更新目标。P50 闭环控制改用平均 raw ADC 加 `4 count` 噪声门限，不再使用通用 IIR 滤波值，减少外力拨动后的检测延迟。锁定参数默认改为 `ADC deadband=12`、`P gain=256`、`D damp=640`、`Lock duty=1000`、`Max duty=1000`；HOLD 退出滞回从 `18 count` 降到 `4 count`，小误差最小锁定 duty 从 `12` 提到 `80`，接近目标刹车区从 `130 count` 缩到 `60 count`。编译通过，LTO 版 `text=18468 B`，`data=1056 B`，`bss=1504 B`；HEX SHA256 `2630bf58751b93c485fe4c4050068fb1768fbc467053d4045aa583426177cf30`；ELF SHA256 `c1dcb1d71aa77b1306196d41c5bdae91a5a7362fac876d08511de687314ed6ae`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003f3d`，并返回 `Successfully resumed device`。

2026-06-09 灵敏度/死区规格记录：规格书灵敏度测试定义为：固定 PWM 宽度下舵机静止，增加或减少 PWM 宽度 `1-3us` 时，舵机应按对应方向连续轻微转动；继续同向增加/减少，应继续转动。反向死区定义为：从某一方向动作后反向减小或增加 PWM，直到舵机刚开始反向转动所累计的 PWM 变化值，该值目标约 `3-6us`。为满足该定义，运行测试固件源码已修改：P60 `Input stable band` 默认从 `3us` 改为 `0us`，不再吃掉 `1-3us` 命令变化；P30 `ADC deadband` 默认从 `12 count` 改为 `4 count`，结合 `HOLD_EXIT_EXTRA=4 count`，按当前 `250-3050mV / 500-2500us` 映射约对应 `4-5us` 反向死区。已编译通过，LTO 版 `text=18468 B`，`data=1056 B`，`bss=1504 B`；HEX SHA256 `82d73edff437b817e634ca049a1a8e6d9af8a295701303d2d15b3f188a2029a1`；ELF SHA256 `2edfe50f5d75a48e0b9828a8d322ff4c4b60f998d5061a9c9c5f999c8c1e15e4`。本次未下载到板：Ubuntu `lsusb` 只看到 Parallels 虚拟键鼠/摄像头，macOS 也未枚举到 DAPLink/CMSIS-DAP，pyOCD 停在 `Waiting for a debug probe to be connected`。

2026-06-09 灵敏度/死区版本补下载记录：用户重新接好 PADLink/DAPLink 后，Ubuntu 已枚举到 `0d28:0204 NXP ARM mbed`。已将上条灵敏度/死区固件下载到板，固件未重新编译，HEX SHA256 仍为 `82d73edff437b817e634ca049a1a8e6d9af8a295701303d2d15b3f188a2029a1`；pyOCD 启动向量 `SP=0x20000c00`、`PC=0x08003f3d`，并返回 `Successfully resumed device`。

2026-06-09 间歇性抖动核查/抑制下载记录：用户反馈暂时无法测灵敏度，因为舵机一直间歇性来回抖。原因判断：上一版同时使用 `ADC deadband=4 count`、raw ADC 控制和 `LOCK_MIN_DUTY=80`，静止时只要 ADC 噪声/电位器接触抖动超过约 `4-8 count`，就会退出 HOLD 并反打，形成间歇抖动。新策略把静止抗抖和命令灵敏分开：P30 `ADC deadband` 默认恢复为 `12 count`，用于 PWM 没变化时防止 ADC 小抖动触发马达；P60 第 5 行改为 `Cmd deadband`，默认 `1 count`，只有锁存 PWM 脉宽真实变化后短时间使用，用于保留 `1-3us` 命令灵敏度。P50 `Current ADC` 值格式改为 `Rxxxx Cxxxx Wxx`：`R` 是当前 raw ADC，`C` 是控制用 ADC，`W` 是约 150ms 窗口 raw ADC 峰峰值，用于直接判断 ADC 抖动幅度。编译通过，LTO 版 `text=18628 B`，`data=1060 B`，`bss=1508 B`；HEX SHA256 `6ccd55cccabd06d8391ca8570394c2cf8be154a87178bcb566fd29ce1ce1fbd1`；ELF SHA256 `b6d376cfef0a21b2ed48c11d4f82eb658b4e8be2df91b75c8d07e81d67d72113`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003fd5`，并返回 `Successfully resumed device`。

2026-06-09 正反抖 PWM 连续确认版下载记录：用户反馈上一版仍然正反抖。原因判断补充为：P60 `Input stable band=0us` 用于保留 `1-3us` 灵敏度，但若 PWM 捕获本身存在 `+/-1us` 级抖动，单帧变化会被当成真实新命令，触发 `Cmd deadband=1 count` 小死区追随，导致目标附近正反打。新策略改为按 PWM 帧号处理命令：同一个新脉宽必须连续出现 `Input stable count=2` 帧才更新锁存目标，超出输入范围时仍立即进入低/高极限保护。P50 第一行 PWM 显示改为 `Pxxxx Lxxxx Wxx`：`P` 是当前捕获脉宽，`L` 是锁存目标脉宽，`W` 是约 150ms PWM 脉宽峰峰值；P50 第三行 ADC 继续显示 `Rxxxx Cxxxx Wxx`，用于同时判断 PWM 和 ADC 的抖动源。小命令追随与静态锁齿的最小 duty 分开：命令变化短时间使用 `25/1000` 起步，静态受外力拨动锁齿仍使用 `80/1000` 起步。编译通过，LTO 版 `text=18888 B`，`data=1068 B`，`bss=1524 B`；HEX SHA256 `9ac788e54113f2fc350ec5ca96d89c87e7e68089dc29d2e94e35d8c9860124c8`；ELF SHA256 `f40a03b8670751bb227e4639038eb571a6bfbe09bc3a0b6e1ebeb701a4e594ff`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080040d9`，并返回 `Successfully resumed device`。

2026-06-09 静态 HOLD 抗抖与 ADC 去尖峰下载记录：用户在 PWM 不变化条件下拍到两张 P50 图，PWM 行为 `P1508 L1508 W1`，说明 PWM 输入和锁存目标稳定；ADC/控制行为从 `R1959 C1959 W24 Error +119 DRV +140` 到 `R2065 C2065 W10 Error +13 HOLD`，说明问题主要集中在电机动作、机械惯性和 ADC 反馈窗口上，而不是 PWM 命令变化。固件先把静态抗抖加强：P30 `ADC deadband` 默认从 `12 count` 提到 `24 count`；静态 HOLD 退出滞回从 `+4 count` 改为 `+24 count`，命令变化期间仍保留 `+4 count`；接近目标提前刹车区从 `60 count` 扩到 `120 count`，减少目标附近正反翻转。随后确认马达/H 桥可能通过电源下陷、地弹、开关尖峰和电位器线耦合影响 ADC，已把 `ADC_Feedback_ReadRaw()` 从单纯多次平均改为多次采样后去掉最大/最小值再平均，用于抑制 H 桥开关尖峰和马达刷火噪声。最终下载版本编译通过，LTO 版 `text=18964 B`，`data=1068 B`，`bss=1524 B`；HEX SHA256 `e49e16b751d1b15343f629359a2e67a683709dfd65972d5fb54f195e115343b7`；ELF SHA256 `30cd8d2a3891508fd23b378a172018173ad798166a2bf76ad606bc640abaf30d`；Ubuntu pyOCD 首次 `hidapiusb` 因 `already open` 失败，脚本复位 DAPLink 后用 default backend 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004125`，并返回 `Successfully resumed device`。

2026-06-09 ADC 丢失硬保护下载记录：用户指出完全没有 ADC 值时不能转动马达。已增加闭环反馈安全条件：P50 运行控制中若电位器 ADC 原始值接近 0 或接近满量程，判定反馈线断开、短路或 ADC 无效，立即 `HBridge_Coast()`，清除 HOLD/方向报警/反馈滤波状态，不再计算目标误差、不再输出马达。P50 的 H-bridge 行显示 `ADC LOST`，并且该状态优先级高于 PWM LOW/HIGH LIM，避免 PWM 越界保护覆盖 ADC 丢失报警。当前阈值为 raw ADC `<=30` 或 `>=4065` 触发保护，恢复到有效范围后才重新进入闭环。编译通过，LTO 版 `text=19072 B`，`data=1068 B`，`bss=1524 B`；HEX SHA256 `ab0ecf2834d3d3c14eb5e6b3b7be0d6f35b231a868095e06eb885f4ffc619b53`；ELF SHA256 `b8a8e35cec98089328171b7f5fb797f02df438218bec709b0528675507d78115`；固件字符串核验可见 `ADC LOST`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004189`，并返回 `Successfully resumed device`。

2026-06-09 PA0 临时电位器供电下载记录：为临时验证电位器供电隔离能否降低 ADC 干扰，增加编译开关 `SERVO_ENABLE_PA0_POT_POWER`。该开关打开时，`Board_Init()` 会在 ADC 初始化前把 `PA0` 配置为低速推挽输出并拉高，用作电位器供电脚；默认不开启，避免影响正式固件。已核对当前调试编译使用 `BOARD_TSSOP20_DEBUG_PINS=1` 时 H 桥 IN4 是 `PB0`，不是 `PA0`，因此本次 PA0 临时供电不与当前 H 桥引脚冲突。构建命令已包含 `SERVO_ENABLE_PA0_POT_POWER=1`，LTO 版 `text=19108 B`，`data=1068 B`，`bss=1524 B`；HEX SHA256 `18a464a81800934220c30e67cf61000c296d35750d4a7de54c79e49f40187a85`；ELF SHA256 `2288fbf312ffc0b275884ad5a54b955d6c3dc226b77cab6502b912d4cdb9bdc8`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080041ad`，并返回 `Successfully resumed device`。

2026-06-09 静态安静保持下载记录：用户反馈 PA0 给电位器供电后 ADC 已稳定，但舵机仍会抖。判断原因从 ADC 噪声转向控制输出本身：当前默认 `Hold mode=1` 时，HOLD 会持续执行 `HBridge_Brake()`；若实际 H 桥刹车组合会产生电流扰动，或目标点附近误差刚越过保持带就立即给最小 duty，则 ADC 稳定也会形成 HOLD/DRV 小循环。本版用于定位该问题：P30 `Hold mode` 默认从 `1` 改为 `0`，HOLD 时先 `Coast` 不主动刹车；所有 HOLD 路径统一按 `Hold mode` 执行，接近目标提前停止和 damping 把 duty 压到 0 时也不再硬性调用 Brake；非命令变化状态下增加静态驱动确认，误差需连续 3 个控制周期超过保持带才重新驱动，减少目标点附近反复补偿。PA0 临时电位器供电仍保持开启。编译通过，LTO 版 `text=19220 B`，`data=1068 B`，`bss=1524 B`；HEX SHA256 `f36246c8c069d536c1561bf1a9db81c57e49dc085312fb182b20cb21af87e08b`；ELF SHA256 `dff4707ac94d236c7298f2d2ccd1268aa432325d7813d78f46d80bb4a6de719b`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800421d`，并返回 `Successfully resumed device`。

2026-06-09 PWM 捕获连续确认下载记录：用户反馈 PWM 源未动且相对稳定，但 LCDM 上 PWM 数值仍在抖。代码核查发现当前 PA4 PWM 获取方式不是硬件输入捕获，而是 `GPIO_MODE_IT_RISING_FALLING` 双边沿 EXTI：上升沿用 `Board_Micros()` 记录时间，下降沿再计算 `now_us - last_rise_us`。`Board_Micros()` 基于 SysTick 推算，且系统同时运行 TIM1 20us H 桥软件 PWM 中断、USART1 LCDM 中断，因此 LCDM 上 1us 级跳动可能来自 EXTI/SysTick 测量路径，而不一定是 PWM 源真实变化。本版在 `pwm_input.c` 捕获层加入最小连续确认：有效脉宽第一次变化只作为候选值，只有同一宽度连续出现 2 帧后才更新 `s_last_pulse_us` 和 `pulse_count`，控制环和 P50 显示都使用确认后的宽度；单帧测量毛刺不会再直接显示为 PWM 变化。为放入 20KB Flash，本版保留 `Hold mode=0` 和 PA0 临时电位器供电，但去掉上一版“静态驱动连续 3 次确认”的附加逻辑。长期正式方案仍应把 PA4 改成硬件定时器输入捕获。编译通过，LTO 版 `text=19212 B`，`data=1072 B`，`bss=1520 B`；HEX SHA256 `41b7a5effe67a1529687c4d87bf4e5d55feff0e3e2984cc2c5a67d7ff58680c2`；ELF SHA256 `a9213725f50b642d05eae285007a15915a4c305ec0c6ff67debad5e68cf16d05`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004215`，并返回 `Successfully resumed device`。

2026-06-09 PWM 跟随失效修正下载记录：用户反馈上一版 LCDM 上能读到 PWM 数，但马达不会跟着 PWM 转。复核后确认问题在上一版连续确认逻辑：控制环用 `pulse_count` 判断是否收到新 PWM 帧，如果 `pulse_count` 只在确认后的脉宽变化时才更新，PWM 固定或候选值未确认期间控制环会认为没有新帧，导致屏幕有数但马达闭环不跟随。本版修正为：`accept_pulse_width()` 只负责确认后的 `s_last_pulse_us`，而 `s_pulse_count++` 在每个有效 PWM 帧都执行；也就是 LCDM/P50 的 `P` 仍是 2 帧确认后的抗抖宽度，控制环的新帧标记不再被滤波逻辑卡住。固件字符串核验可见 `P50 PWM Servo Loop`、`ADC LOST`、`P%u L%u W%u`、`R%u C%u W%u`、`ADC deadband`、`Hold mode`。编译通过，LTO 版 `text=19208 B`，`data=1072 B`，`bss=1520 B`；HEX SHA256 `76fca7a8c40facf38d30dd09c8d0097bb36bffca4543dd55b65fc92e1d9acec3`；ELF SHA256 `d06e9dcc831c8c33882212156ec0702381fcdf6a5ab891b6e178e4570a9afb68`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004211`，并返回 `Successfully resumed device`。

2026-06-09 快速跟随/强锁/刹车一体调试下载记录：针对用户提出的三个互相牵制问题进行组合修改：一是 PWM 宽度变化时空载转速太慢，目标为接近 `0.115s/60deg`；二是 PWM 固定时 ADC 端外力反推锁止力不够、软件反应慢；三是在提高速度后过冲刹不住。P50 控制环从约 2ms 调用改为约 1ms 调用，屏幕刷新仍保持约 150ms，避免 LCDM 通讯拖慢电机闭环。控制反馈改为用当前原始 ADC 参与闭环计算，滤波值保留给显示和小噪声缓存；默认 `ADC deadband` 从 24 调到 8，`P gain` 从 256 调到 512，`D damp` 从 640 调到 520，`Hold mode` 默认改为 1 刹车，`Lock min duty` 内部提高到 260，命令变化最小 duty 提高到 180。命令变化时，大误差直接给到 `Max duty`，中小误差按误差比例给 duty；接近目标且 ADC 速度仍朝目标方向较快时，主动反向刹车，并且该主动反刹不会触发方向接反报警。PWM 固定时，若外力导致 ADC 偏离目标，按误差和偏离速度快速增加反向锁止 duty，放手回到死区后进入 HOLD/刹车。为适配 20KB Flash，旧的数组式锁止曲线已删掉，改成更小的强锁线性公式；参数名 `Reverse pause` 缩短为 `Rev pause`。固件字符串核验可见 `P50 PWM Servo Loop`、`ADC LOST`、`ADC deadband`、`P gain`、`D damp`、`Lock duty`、`Hold mode`、`Rev pause`。编译通过，LTO 版 `text=19280 B`，`data=1072 B`，`bss=1520 B`；HEX SHA256 `fa6ea65a3618523731ec50ca73dd4c48586a6fe86aedcd1b83e7047189d8130d`；ELF SHA256 `13e76fc441e7e73f30b627f2feb673e527336284999f15466038580ae8389af0`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004289`，并返回 `Successfully resumed device`。

2026-06-09 过冲刹车加强/代码缩减下载记录：用户反馈上一版锁止力和转动速度明显加强，但过冲仍明显，要求速度暂时不改、继续优化过冲，同时程序量要减少。本版保持命令运动速度段不变：`CMD_MIN_DUTY=180`、`FAST_FULL_ERROR=260` 未改；只加强接近目标后的反向刹车和静态锁止。内部 `LOCK_MIN_DUTY` 从 260 提到 320，`FAST_STOP_GAIN` 从 12 提到 18，使接近目标时更早进入反向刹车；`BRAKE_MIN_DUTY` 从 220 提到 340，`BRAKE_VEL_GAIN` 从 48 提到 72，提高反刹力度；`LOCK_VEL_GAIN` 从 55 提到 70，提高 PWM 固定、外力反推时的速度补偿锁止力。为减少程序量，删除已经不参与控制的旧 `lcdm_servo_filter_feedback()` 函数，P50 控制和显示都直接记录当前 ADC 采样，保留目标变化清 HOLD 和首次速度基准初始化逻辑。编译通过，LTO 版 `text=19248 B`，`data=1072 B`，`bss=1520 B`，比上一版减少 32 B；HEX SHA256 `047fc2e45f0062fcad2ef55db57569add95146250003a82a6caec5c37eac3084`；ELF SHA256 `809eae1b4250592052db34732dab1091ce68bad3a92e4255040de0bb4e2a70d7`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004269`，并返回 `Successfully resumed device`。

2026-06-09 自然 PD 阻尼版下载记录：用户实测上一版过冲更严重、抖动更厉害，说明“强制大反刹”逻辑错误，会把系统变成正反打架。本版删除 `reverse_brake` 大反打分支，不再用 `FAST_STOP_GAIN`、`BRAKE_MIN_DUTY`、`BRAKE_VEL_GAIN`、`LOCK_VEL_GAIN` 这些硬切换常数，改为更基础的闭环公式：`duty = Kp * 位置误差 - Kd * ADC速度`。位置误差决定往目标走，ADC 速度项自然减速；如果接近目标但速度仍过快，D 项会让 duty 降低或轻微反向，而不是突然全力反打。大误差命令变化仍保留满 duty 快速段，避免把速度直接降回去；接近目标后才进入 PD 阻尼。命令变化最小 duty 从 180 降到 120，减少目标附近推力；静态锁止最小 duty 从 320 回到 280，锁止仍保留但不再因小误差强打。编译通过，LTO 版 `text=19040 B`，`data=1072 B`，`bss=1520 B`，比上一版再减少 208 B；HEX SHA256 `a124dfa07b4a5429d009ee71dccde621127afdab579ac62907c553133ff0cd15`；ELF SHA256 `3703b50787b2a611727125dfc0032a9f49b767ff0c46f17a3c48767f02038ac4`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004199`，并返回 `Successfully resumed device`。

2026-06-09 反打限制/过冲记录下载记录：用户反馈自然 PD 版反打仍非常明显，要求通过记录 ADC 值验证严重性并继续优化。本版增加命令动作方向记录：目标 ADC 变化时记录本次运动方向并清零过冲计数；控制循环中若反馈 ADC 越过目标，记录本次命令的最大过冲 `O`，P50 第 4 行从单纯误差改为 `E当前误差 O最大过冲`，例如 `E-35 O82`，便于直接观察过冲 ADC count。算法上保留 PD 阻尼，但对命令动作期间的反向输出做限制：未越过目标前，如果 D 项算出与运动方向相反的 duty，直接压成 0；越过目标后只允许反向修正 duty 最大约 `120/1000`，避免明显反打。编译通过，LTO 版 `text=19244 B`，`data=1072 B`，`bss=1528 B`；HEX SHA256 `856322da5056509407b5faccf8081c821e89f39d356757b6227acb94e5f887e2`；ELF SHA256 `153d0899ce4c107cbe10c1835079a8ea0ea33d72b9875618094388fc6de491cb`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800425d`，并返回 `Successfully resumed device`。

2026-06-09 ADC LOST 去毛刺下载记录：用户反馈运行中掉到 `ADC LOST`。保护逻辑仍需保留，因为 ADC 断线或电位器供电丢失时不能继续驱动马达，但单次 ADC 采样毛刺不应立即锁死。本版增加 `Board_PotPowerRefresh()`，P50 控制环每次采样前重新把临时电位器供电脚 PA0 拉高；同时把 ADC LOST 从单次无效触发改为连续 `6` 次无效才显示 `ADC LOST`，约 6ms，连续有效采样立即清零恢复。前 1-5 次无效时 H 桥仍先安全 `Coast`，但状态暂不显示 `ADC LOST`。编译通过，LTO 版 `text=19268 B`，`data=1072 B`，`bss=1528 B`；HEX SHA256 `45e05b9fbdfdfb43d6dbbab8f28558c7447766e1779be76874bf8730ecce4a7e`；ELF SHA256 `c616db4cd4a32b72fc110ec9d4878849346da9d6cd2c4ff7696085687eadd09a`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004275`，并返回 `Successfully resumed device`。

2026-06-09 过冲实测量级记录：用户现场读到 P50 `E` 最大 900 多、`O` 最大 600 多。按当前电位器有效范围约 `0.25V-3.05V` 估算，对应 ADC 约 `310-3780 count`，总跨度约 `3470 count`。因此 `E900` 约等于全行程 26%，`O600` 约等于全行程 17%。这不是小过冲，说明当前算法在大步命令时没有在到达目标前足够早地降速，目标点处仍有很大机械动能；后续应改为按“剩余距离/当前速度”做提前减速，而不是靠越过目标后的反向修正。

2026-06-09 速度包络/提前减速下载记录：用户提出应把速度、动力和位置形成相互关联的特性曲线，而不是孤立调参数；同时指出不同供电电压下成熟舵机仍能控制稳定。当前硬件未独立采样马达电源电压，MCU `VDD` 若经过 LDO 并不等于马达电压，因此本版不做假的电压补偿，而是先用 ADC 实际速度形成自适应速度包络：大距离允许高速，小距离降低允许速度；若朝目标方向的实际 ADC 速度 `vel_toward` 超过允许速度 `allow_vel=(abs_error>>6)+1`，先把输出压到 0，让马达在目标前减速，不再等越过目标后反打。大误差满速区从 `260 count` 提前到 `700 count`，进入减速区更早；命令动作过程由 `move_dir` 持续标记，直到进入 HOLD 才结束，不再只依赖 80ms 短标志。越过目标后的反向修正上限从 `120/1000` 降到 `70/1000`，减少明显反打。为放入 Flash，压缩了部分 LCDM 参数名字符串，不改变参数含义。编译通过，LTO 版 `text=19276 B`，`data=1072 B`，`bss=1528 B`；HEX SHA256 `dd27992e14a293321ec7b2cae03561336b65554dd9933e6959497ec4ab8c59bb`；ELF SHA256 `c216b76a270cc109c693ed33e333c62d293a6cb9ba89b94be9714c29b9810cee`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080042c5`，并返回 `Successfully resumed device`。正式产品硬件建议增加马达电源分压检测，这样可在速度包络外再做 duty 电压归一化。

2026-06-09 提速/静止抗抖下载记录：用户实测 ADC 变化速度算法方向正确，过冲已明显改善，但速度偏慢，停止后仍有轻微抖动和嗞嗞声。本版不增加复杂程序量，只调整速度包络和 HOLD 退出条件：命令运动最小 duty 从 `120/1000` 提到 `150/1000`，满速误差阈值从 `600 count` 降到 `520 count`，允许速度曲线从 `allow_vel=(abs_error>>5)+2`、最大 `20 count/loop` 提到 `allow_vel=(abs_error>>5)+3`、最大 `26 count/loop`，让大步动作更快；静止默认 `ADC deadband` 从 `16 count` 提到 `20 count`，静止 HOLD 退出附加滞回从 `24 count` 提到 `36 count`，保持 `Hold mode=0`，停住后优先 `Coast`，减少 H 桥被 ADC 小漂移反复唤醒。编译通过，LTO 版 `text=19276 B`，`data=1072 B`，`bss=1528 B`；固件字符串核验可见 `P50 PWM Servo Loop`、`ADC LOST`、`E%+d O%u`、`ADC deadband`、`P gain`、`D damp`、`Hold mode`；HEX SHA256 `0ca49bff7bda7abf92903131340acd23e7addd6de8d7d38d390ffda86ecbef2a`；ELF SHA256 `d2dd6603ad72ab52b864423fc8f1aa44b2f53c407cb1094f9bdd031831eeb797`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080042c5`，并返回 `Successfully resumed device`。

2026-06-09 过冲回退版下载记录：用户反馈上一条提速版越改越差，静止稍好但过冲无法接受，P50 最大 `E` 和 `O` 都超过 `1477 count`。判断为运动包络放宽过度导致目标前动能过大，因此立即退回造成过冲恶化的三项运动参数：命令运动最小 duty 从 `150/1000` 退回 `120/1000`，满速误差阈值从 `520 count` 退回 `600 count`，允许速度曲线从 `allow_vel=(abs_error>>5)+3`、最大 `26 count/loop` 退回 `allow_vel=(abs_error>>5)+2`、最大 `20 count/loop`。静止抗抖保留上一版改善值：默认 `ADC deadband=20 count`、静止 HOLD 退出附加滞回 `+36 count`、`Hold mode=0`。编译通过，LTO 版 `text=19276 B`，`data=1072 B`，`bss=1528 B`；固件字符串核验可见 `P50 PWM Servo Loop`、`ADC LOST`、`E%+d O%u`、`ADC deadband`、`P gain`、`D damp`、`Hold mode`；HEX SHA256 `aeb111c29a93bb768947b1894e89cfb02b0401091104985d000000c3a30639a3`；ELF SHA256 `285bd4fc8bd24f25da4d74e8b73152abf0c3dc501274ecbf60c04139a7a4adee`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080042c5`，并返回 `Successfully resumed device`。下一步若 `O` 仍偏大，应在此版本基础上加目标前轻刹/提前断电，不再继续提高速度参数。

2026-06-09 完整恢复到改静止前基准版下载记录：用户反馈静止抗抖版和后续回退版仍把过冲问题搞乱，要求恢复到上二个版本，即还没有修改静止嗞嗞声的版本，以免继续冲击舵机齿轮。已确认并恢复关键参数：`ADC deadband=16 count`、静止 HOLD 退出附加滞回 `+24 count`、命令运动最小 duty `120/1000`、满速误差阈值 `600 count`、允许速度曲线 `allow_vel=(abs_error>>5)+2`、最大 `20 count/loop`、`Hold mode=0`。编译通过，LTO 版 `text=19276 B`，`data=1072 B`，`bss=1528 B`；固件字符串核验可见 `P50 PWM Servo Loop`、`ADC LOST`、`E%+d O%u`、`ADC deadband`、`P gain`、`D damp`、`Hold mode`；HEX SHA256 `056e5a4d392fddeb0abf096c565ca97054482afbf51224b56d184a63cbc6b343`；ELF SHA256 `5eeac0e584768125cf8df0314583d2f9339affcfcd41482995bbee6c3c624288`；Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080042c5`，并返回 `Successfully resumed device`。后续改善必须以此版作为基准，先确认 P50 `O` 是否回落，再单独处理过冲，不能同时改静止抗抖和速度。

2026-06-09 恢复到真正速度包络初版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不对哦，没有恢复到哦，1000和1500us都会找到极端边，搞坏舵机，再核查” |
| 判断 | 上一条所谓“完整恢复”只回退到提速失败前一版，但不是更早的速度包络初版。真正需要恢复的是更保守的初版速度包络，避免 `1000us/1500us` 动作继续冲到极端边。 |
| 核查 | `1000us/1500us` 的 PWM 到目标 mV/ADC 映射默认并非极端：默认 `500us->250mV`、`1500us->1650mV`、`2500us->3050mV`，所以风险主要来自控制参数未回到足够保守的版本，而不是目标映射本身。 |
| 修改 | 恢复 `ADC deadband=8 count`、静止 HOLD 退出附加滞回 `+8 count`、`Hold mode=1`、命令运动最小 duty `90/1000`、满速误差阈值 `700 count`、允许速度曲线 `allow_vel=(abs_error>>6)+1`、最大 `14 count/loop`。 |
| 编译 | LTO 版编译通过，`text=19276 B`、`data=1072 B`、`bss=1528 B`。 |
| 固件核验 | 固件字符串可见 `P50 PWM Servo Loop`、`ADC LOST`、`E%+d O%u`、`ADC deadband`、`P gain`、`D damp`、`Hold mode`。 |
| 版本 | HEX SHA256 `dd27992e14a293321ec7b2cae03561336b65554dd9933e6959497ec4ab8c59bb`；ELF SHA256 `c216b76a270cc109c693ed33e333c62d293a6cb9ba89b94be9714c29b9810cee`。 |
| 下载 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080042c5`，并返回 `Successfully resumed device`。 |
| 当前结论 | 当前板上固件已恢复到文档中 2026-06-09 “速度包络/提前减速下载记录”的同一 HEX/ELF hash。此版作为新的安全基准，现场先小幅动作确认 `1000us/1500us` 不再冲极端边。 |

### 逐轮沟通记录规则

从 2026-06-09 起，后续每次用户给出硬件现象或调试要求，都按以下格式连续记录在本文件中：`用户原话`、`现场现象/风险`、`我的判断`、`修改内容`、`编译结果`、`固件版本 HEX/ELF SHA256`、`下载结果`、`下一步验证点`。若某次没有修改或没有下载，也必须明确写“未修改”或“未下载”，避免把失败版本和可用版本混在一起。

2026-06-09 调试顺序判断记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “终于回到了稳定一点的版本了，现在接着前面问的问题，是先改速度，还是先改静止时的嗞嗞声” |
| 现场现象/风险 | 当前已回到较稳定的速度包络初版，前两轮失败说明同时改速度和静止区参数会把过冲问题放大，存在冲击齿轮箱风险。 |
| 我的判断 | 先改静止嗞嗞声，但不能再改 `ADC deadband`、HOLD 退出滞回、速度包络、目标映射；只能处理“已经进入 HOLD 之后 H 桥如何输出”。这样不会影响 `1000us/1500us` 的运动路径和过冲。 |
| 建议路线 | 第一步固定当前速度和过冲基准；第二步只把 HOLD 后的 H 桥输出改为更安静的策略，例如进入 HOLD 后短时间 Brake，随后 Coast，或用很低频/低占空保持；第三步确认静止不嗞嗞且 `O` 不变后，再回头优化速度。 |
| 修改内容 | 本轮未修改源码。 |
| 编译结果 | 本轮未编译。 |
| 固件版本 | 当前板上仍是恢复后的速度包络初版：HEX SHA256 `dd27992e14a293321ec7b2cae03561336b65554dd9933e6959497ec4ab8c59bb`；ELF SHA256 `c216b76a270cc109c693ed33e333c62d293a6cb9ba89b94be9714c29b9810cee`。 |
| 下载结果 | 本轮未下载。 |
| 下一步验证点 | 如果继续执行，下一次只允许修改 HOLD 后 H 桥输出，不碰运动参数；验证静止嗞嗞声是否消失，同时确认 P50 `O` 不比当前基准变大。 |

2026-06-09 静止 HOLD 一圈短刹后 Coast 下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “继续” |
| 现场现象/风险 | 用户确认当前已回到相对稳定的速度包络初版，继续处理静止嗞嗞声。前提是不破坏当前过冲基准，避免再次冲击舵机齿轮。 |
| 我的判断 | 只允许改“已经进入静止 HOLD 后 H 桥如何输出”，不改目标映射、速度包络、`ADC deadband` 数值、HOLD 退出滞回、`CMD_MIN_DUTY`、`FAST_FULL_ERROR`、`allow_vel`。 |
| 修改内容 | 静止到位死区内，`Hold=1` 从原来的持续 Brake 改成进入静止 HOLD 第一圈 Brake，之后 Coast；运动中速度包络算出 `duty=0` 的路径仍调用原来的 `lcdm_servo_apply_hold_output()`，保持提前减速/刹车行为不变。为放入 Flash，仅把 P30 显示名 `ADC deadband` 缩短为 `Deadband`，`Hold mode` 缩短为 `Hold`；参数值不变。 |
| 未采用方案 | 先尝试“静止 HOLD 后 Brake 30ms 再 Coast”，但固件超 Flash `64 B`；随后尝试保留 `Hold=2` 连续 Brake 对比选项，仍超 Flash `8-12 B`，因此改成最小的一圈短刹方案。 |
| 保持不变 | `ADC deadband=8 count`、静止 HOLD 退出附加滞回 `+8 count`、命令运动最小 duty `90/1000`、满速误差阈值 `700 count`、允许速度曲线 `allow_vel=(abs_error>>6)+1`、最大 `14 count/loop`。 |
| 编译结果 | LTO 版编译通过，`text=19280 B`、`data=1072 B`、`bss=1528 B`。 |
| 固件核验 | 固件字符串可见 `P50 PWM Servo Loop`、`ADC LOST`、`E%+d O%u`、`Deadband`、`P gain`、`D damp`、`Hold`。 |
| 版本 | HEX SHA256 `d562b49a707d3c7cd2d2885892ed8c3353535a662a8a6b5e76a363a094e8bb82`；ELF SHA256 `7fd7165709ec7b830619eb6f899d259e8d99ed58698fde5bd0b49d48284dc9ac`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080042d1`，并返回 `Successfully resumed device`。 |
| 下一步验证点 | 先固定 PWM 在 `1500us` 或小幅 `1000/1500us` 来回动作，确认停住后的嗞嗞声明显降低或消失；同时观察 P50 `O` 是否保持接近基准，不应再出现 `1000/1500us` 冲极端边。 |

2026-06-09 越过目标即静止 HOLD 下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “并没解决嗞嗞声，没有找到方法，我个人认为是当目标值到达后，立即刹停H桥不再有输出，除非ADC外力引起变化才作出反驱。这里可能有二个原因，一个是还没到达目标点，另一个是，过了目标点，所以一直找到不平衡点。一直嗞嗞说明H桥不停在正反，时间一长，到位了就不响了。” |
| 现场现象/风险 | 上一版静止 HOLD 一圈短刹后 Coast 没有解决嗞嗞声，说明问题不只是 HOLD 输出，而是到达/越过目标后仍在小 duty 正反找平衡。 |
| 我的判断 | 用户判断成立。原逻辑在命令运动越过目标后仍允许 `70/1000` 反向修正，容易在目标两侧反复正反输出；静止 HOLD 也一直以原目标 ADC 为误差源，若实际停在过冲点附近，会继续追目标导致嗞嗞。 |
| 修改内容 | 命令运动越过目标后，不再输出 `70/1000` 反向修正，直接 `move_dir=0` 且 `pd=0` 进入 HOLD；进入 HOLD 时用当时 ADC 保存为保持参考点。HOLD 状态下误差改为 `保持参考 ADC - 当前 ADC`，只有 ADC 因外力离开参考点超过滞回才反驱。 |
| 保持不变 | 速度包络仍保持稳定基准：`CMD_MIN_DUTY=90`、`FAST_FULL_ERROR=700`、`allow_vel=(abs_error>>6)+1`、最大 `14 count/loop`；`Deadband=8 count`、静止 HOLD 退出附加滞回 `+8 count`、目标映射不变。 |
| 空间处理 | 加入 HOLD 参考点逻辑后固件超 Flash `152 B`。为不删控制逻辑，压缩 LCDM 开发界面字符串：例如 `P50 PWM Servo Loop -> P50 Servo`、`ADC deadband -> Deadband`、`Hold mode -> Hold`、主菜单和 P80/P90 标签缩短。 |
| 编译结果 | LTO 版编译通过，`text=19096 B`、`data=1072 B`、`bss=1528 B`。 |
| 固件核验 | 固件字符串可见 `P50 Servo`、`ADC LOST`、`E%+d O%u`、`Deadband`、`P gain`、`D damp`、`Hold`、`P10 Out`、`P80 Save`。 |
| 版本 | HEX SHA256 `8a3b095af96fa393dd8624493554d4f31c39cc5ecbd20e9378d2f4fc9da714d8`；ELF SHA256 `96aa029da3956fbf406f7ab42f48870f2a3d8916ba789f32026d8f06e20f00ff`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004369`，并返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us` 等待到位，看嗞嗞声是否在越过目标后立即消失；再试 `1000us <-> 1500us` 小动作，看是否仍冲极端边。P50 重点观察 `E/O`，若停得偏但不响，下一步再处理停准；若仍响，说明 HOLD 参考点仍未阻断所有反驱路径。 |

2026-06-09 ADC W 与嗞嗞声分析记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还是没有改善，我看到ADC的W显示最高是20，过一段时间平稳下来约8，是否因为这个原因。另外，ADC目前取样和过滤是如何执行的，是否有更快，更高速的处理ADC值，然后过滤因为外界或其它原因的跳动，这样就能很快稳定下来。但这不一定是真正原因，因为我又看到不嗞嗞声时，W值也有20几，是什么原因呢？” |
| 现场现象/风险 | 静止嗞嗞声仍未改善；P50 `ADC W` 最高约 `20 count`，稳定后约 `8 count`；但无嗞嗞声时也可能看到 `W=20+`，说明不能单纯把 `W` 当作唯一原因。 |
| 核查 | P50 `W` 来自 `lcdm_servo_adc_window_take()`，表示约 150ms 显示刷新窗口内 raw ADC 最大值减最小值。它是窗口峰峰值，不带方向，不区分真实机械运动、ADC 噪声、马达动作后的余振或采样毛刺。 |
| ADC 当前执行方式 | LCDM P50 控制环每约 1ms 调一次 `ADC_Feedback_ReadRaw()`。该函数按参数 `adc_sample_count` 多次采样，默认 `4` 次；若采样次数大于等于 `4`，会去掉最大/最小值，再对剩余样本平均。ADC 配置为 12bit、单次软件触发、采样时间 `239.5 cycles`。 |
| 过滤现状 | 项目中存在 `ADC_Feedback_UpdateFiltered()`：支持 `adc_jump_limit_count` 限幅和 `adc_filter_shift` 一阶 IIR。但当前 LCDM P50 伺服控制环没有使用该 IIR 过滤值，而是直接使用 `ReadRaw()` 的去极值平均结果；P50 的 `C` 当前基本等于控制用 raw ADC。这样响应快，但容易受小跳动影响。 |
| 判断 | `W=20` 可能会触发静止 HOLD 退出，因为当前静止 deadband 约 `8 count`、静止 HOLD 退出滞回约 `+8 count`，合计约 `16 count`。但无嗞嗞时也有 `W=20+`，说明关键不只是窗口大小，还要看波动是否跨过保持参考点、方向是否反复、H 桥是否正在输出。 |
| 建议方向 | 下一步应增加“静止 HOLD 专用 ADC 判定”，而不是简单加大 deadband。运动时继续用快速 raw 以保持响应；进入 HOLD 后使用更稳定的参考和连续确认，例如只有连续多次 ADC 偏离保持参考点超过阈值才反驱，并且忽略单次/短窗口跳动。这样能过滤外界/电机噪声，又不会拖慢运动阶段。 |
| 修改内容 | 本轮只分析和记录，未修改源码。 |
| 编译结果 | 本轮未编译。 |
| 下载结果 | 本轮未下载。 |
| 下一步验证点 | 若继续修改，优先实现 HOLD 静止专用过滤/连续确认，并在 P50 增加或复用显示以区分 `raw W`、HOLD 参考误差、是否正在反驱。 |

2026-06-09 HBR DRV 160 分析记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “对，HBR的DRV值最大是160，会是什么原因？有关系吗” |
| 核查 | P50 第 5 行 HBR/DRV 来自 `s_lcdm_servo_current_duty`。显示逻辑为：若状态是 `DRIVE`，按方向显示 `DRV +` 或 `DRV -`，后面的数字是 `abs(duty)`。因此 `DRV 160` 表示 H 桥正在以约 `160/1000 = 16%` 占空比输出，不是电流值。 |
| 关系判断 | 有关系。只要嗞嗞声发生时 HBR 显示 `DRV`，说明 MCU 确实仍在让 H 桥输出，声音不是单纯机械静止噪声。若 `DRV +160` 与 `DRV -160` 来回切换，说明控制环在目标附近正反找平衡；若一直同方向 `DRV 160`，说明还没进入 HOLD，或者 HOLD 参考/目标判定还没有让它停。 |
| 160 的来源 | 当前稳定基准中命令运动最小 duty 是 `90/1000`，静止锁止最小 duty 是 `280/1000`，但运动阶段会按误差比例计算 duty。`160` 多半是小误差段按 `min_duty + (max-min)*abs_error/FAST_FULL_ERROR` 算出来的中小驱动值，也可能是静止反驱公式和阻尼项算出的结果。它不是异常大电流，但如果持续/反复输出，就足以造成嗞嗞声。 |
| 和 ADC W 的关系 | `W=20` 只是 150ms raw ADC 峰峰值。它可能使误差跨过 HOLD 退出带，从而触发 `DRV 160`；但无嗞嗞时也可能有 `W=20+`，说明还要看 HBR 是否实际进入 `DRV`、方向是否来回切换，以及误差是否连续越界。 |
| 下一步建议 | 现场先观察嗞嗞时 HBR 是 `DRV +160`、`DRV -160` 来回切换，还是固定一个方向；同时看 Error 的正负是否跟着变。下一版若修改，应优先做“静止 HOLD 连续确认”：HOLD 后 ADC 偏离参考点必须连续多次超过阈值才允许重新 DRV，过滤单次/短时 W 跳动。 |
| 修改内容 | 本轮只分析和记录，未修改源码。 |
| 编译结果 | 本轮未编译。 |
| 下载结果 | 本轮未下载。 |

2026-06-09 命令到位窗口放宽下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “一个方向，转左时1000US是负数，转右时2000US是正数” |
| 现场现象/风险 | P50 HBR 的方向不是正负来回翻转，而是 `1000us` 左转显示负方向、`2000us` 右转显示正方向。说明方向符号和目标方向大体正确，风险点转为：目标附近仍认为未到位，持续输出同方向 `DRV`，造成嗞嗞声和电机发热。 |
| 我的判断 | 这不像马达方向反了，也不像目标附近正反震荡；更像命令运动到位判定太窄。上一版 `Cmd DB=1 count`，而现场 ADC `W` 常见约 `8-20 count`，所以程序可能长期差一点点不到目标，保持 `DRV 160` 左右的同方向追赶。 |
| 修改内容 | 只改 P60/Startup 的 `Cmd DB` 默认值：从 `1 count` 放宽到 `64 count`，范围 `0..120` 保持可调。未修改速度包络、PWM-ADC 映射、ADC 采样方式、H 桥最大 duty、方向保护和静止 HOLD 逻辑。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19096 B`、`data=1072 B`、`bss=1528 B`。 |
| HEX/ELF SHA256 | HEX `08c4bda542598cc2df00a1ac9e2a3177d2ee88e707bf6f614e339238cd35b852`；ELF `45a868cafb39495354c7247ab7e454b2cc3fd7967c999f130bd07e833ff6344f`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004369`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 在 P50 固定输入 `1000us`、`1500us`、`2000us` 分别观察：到目标附近后 HBR 是否更快进入 `HOLD`；嗞嗞声时是否仍显示固定方向 `DRV 160`；若还是固定方向驱动，则下一步不要改速度，改“运动阶段到位后立即锁存当前位置并退出命令追赶”。 |

2026-06-09 `Cmd DB=64` 作废与回退下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不行，不光是抖得厉害，马达电流也达到2A以下，晃的严重，算法不对，需重新找思路解决抖动。这个测试版本作废。” |
| 现场现象/风险 | `Cmd DB=64` 后抖动更厉害，马达电流上升到接近 `2A`，舵板明显晃动，存在齿轮箱冲击风险。该测试版本确认作废，不能继续作为调试基准。 |
| 我的判断 | 放大命令到位窗口是错误方向：它只是把位置误差判断放宽，并没有真正处理马达动能、过目标后的退出、H 桥刹车/释放时机和静止锁止状态。结果可能在目标附近形成大误差锁存和高 duty 补偿，导致大电流晃动。后续不能再靠单纯加大死区解决抖动。 |
| 修改内容 | 立即回退唯一危险改动：P60/Startup `Cmd DB` 默认值从 `64 count` 恢复为 `1 count`。未修改速度包络、PWM-ADC 映射、ADC 采样、H 桥最大 duty、方向保护和静止 HOLD 逻辑。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19096 B`、`data=1072 B`、`bss=1528 B`。 |
| HEX/ELF SHA256 | HEX `cedbf9fbd77720007c2db454456117b5523f368973266331a33a70baf6e580c3`；ELF `2b84e8b4582572569f1f22111ff42b23f253b445ff3d1f8aa813d518baf07928`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004369`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 先确认板子已不再运行作废版本。下一轮算法应重新分层：`PWM 目标变化`、`运动加速/减速`、`穿越目标后捕获静止点`、`静止外力反驱锁止` 分成不同状态；不能再用同一套 PD 和大死区同时处理运动和静止。 |

2026-06-09 静止 HOLD 脉冲反驱下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “继续啊，不要停啊，要彻底找到解决嗞嗞声” |
| 现场现象/风险 | 前一版 `Cmd DB=64` 已确认作废，说明单纯放大到位死区不能解决嗞嗞声。当前核心风险是静止后只要 ADC 偏离锁定点一点，程序就可能按静止 PD 立即给较大的连续反驱 duty，导致马达在目标附近持续出力、发声、发热甚至晃动。 |
| 我的判断 | 嗞嗞声的根因不是 PWM 宽度和方向符号，而是运动控制与静止保持没有彻底分层：`move_dir=0` 后仍可能直接用 PD 连续输出，且静止最小反驱 duty 较高。正确方向应是静止时先安静、确认偏移，再短脉冲反驱，脉冲后停下来重新读 ADC，不要每 1ms 连续硬顶。 |
| 修改内容 | 新增静止 HOLD 专用脉冲反驱：HOLD 后以锁存 ADC 为参考；`Deadband + 24 count` 内完全 `Coast`；超过安静带的小偏移需连续确认 `4` 个控制周期；确认后反驱只输出 `2ms`，再休息 `4ms` 重新采样；反驱 duty 从 `100/1000` 起按偏移量增加，并受 `Lock/Max` 限制。运动追目标、速度包络、PWM-ADC 映射、`Cmd DB=1` 均保持不变。为放入 20KB Flash，只压缩 LCDM 显示字符串，例如 `P00 Main Monitor -> P00 Main`、`P01 Main Menu GRID V9 -> P01 Menu`、`DIR ALARM -> DIR ALM`。 |
| 编译结果 | 首次编译因 Flash 超出 `176 B` 失败；合并重复 HOLD 输出并压缩 LCDM 字符串后 Ubuntu 编译通过，LTO 版 `text=19152 B`、`data=1072 B`、`bss=1528 B`。 |
| HEX/ELF SHA256 | HEX `82d9a6932b722e341d674cc3b7c08225888bc6feea7d13d9761227424694aae3`；ELF `4d907c48a4f03679f534f4738434775f4c1be310379ebd575eb776ebceabb853`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080044b5`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us` 静止，重点看 P50 的 HBR 是否大部分时间为 `HOLD`，只有外力拨动时短暂出现 `DRV +/-`；听嗞嗞声是否变成很短的间歇或消失；用手轻拨舵板时是否有反向锁力但不会连续大电流晃动。若锁力不足，下一步只调静止脉冲 duty/占空节奏，不改运动速度。 |

2026-06-09 静止慢漂自适应下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “开始一半一半，会嗞嗞声，约20秒后回复平静”；随后补充“然后一段时间后，又嗞嗞声” |
| 现场现象/风险 | 静止脉冲反驱版能在约 `20s` 后平静，说明连续硬顶已减少；但过一段时间又嗞嗞，说明 HOLD 参考 ADC 仍可能因为机械回弹、电位器慢漂、温升或供电微变而变成旧参考点，误差重新越过安静带后再次触发反驱。 |
| 我的判断 | 这不是单纯电机方向或 PWM 捕获问题，而是静止参考点太死。真正静止时，小幅慢变化不应被当成外力；只有明显超过小漂移区的变化才应反驱。否则只要 ADC/机械慢漂越过阈值，就会周期性重新嗞嗞。 |
| 修改内容 | 在静止 HOLD 脉冲反驱基础上增加小慢漂自适应：`Deadband + 24 count` 内仍完全安静；再外加 `32 count` 的小漂移区，在该区内不反驱，只 `Coast`；若小漂移连续约 `120` 个控制周期，则把 HOLD 参考 ADC 更新到当前反馈值。超过小漂移区才按原来的短脉冲反驱。运动追目标、速度包络、PWM-ADC 映射、`Cmd DB=1` 保持不变。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19260 B`、`data=1072 B`、`bss=1528 B`。 |
| HEX/ELF SHA256 | HEX `08e6efc8532ad280ae651257b6f715ac05b3c7d8e3bd9a153ecc5b067b72ec7e`；ELF `00bd8104850e5622bc9a4d66ddf1bdc1b194a80e466ad58df6ada66c58aad6fe`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004521`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us` 长时间观察，至少看 1-2 分钟：是否还会“过一段时间又嗞嗞”。同时轻拨舵板确认：小幅慢漂不出力，大幅/快速外力仍应短暂 `DRV +/-` 反驱。若锁止力明显不足，下一轮缩小 `HOLD_DRIFT_EXTRA` 或提高反驱 duty；若仍周期性嗞嗞，下一轮扩大漂移区或延长参考重置条件。 |

2026-06-09 静止学习窗口限制下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “一直嗞嗞，约一分钟后静止，静止后用手去拨动舵板，左右扭都不会再响。但过一段时间又会响一下。这是什么现象。” |
| 现场现象/风险 | 上一版会先持续嗞嗞约一分钟才静止；静止后手动左右拨舵板也不再响，说明反驱锁止被慢漂学习吞掉；过一段时间又响一下，说明旧参考点/阈值仍会被慢漂周期性触发。风险是虽然安静，但锁齿力丢失，不符合舵机固定 PWM 下必须锁住当前位置的要求。 |
| 我的判断 | 这是“慢漂自适应过度”的典型现象：程序长期把 ADC 变化学习为新的 HOLD 参考点，真实外力如果变化不快，也被当作漂移吸收。因此后面不响不是好现象，而是锁止功能被放掉了。正确做法是只在刚到位后的短窗口学习机械回弹，之后冻结参考点，外力拨动必须重新反驱。 |
| 修改内容 | 增加 `HOLD_LEARN_TICKS=3000` 学习窗口。进入 HOLD 的瞬间启动约 3 秒学习；只有学习窗口内的小漂移区才允许更新 HOLD 参考 ADC。窗口结束后不再长期学习；若偏移超过小漂移区，会立即结束学习并进入反驱确认。为放入 Flash，仅压缩触摸状态字符串如 `MAIN T%u -> M%u`、`BACK T%u -> B%u`。运动追目标、速度包络、PWM-ADC 映射、`Cmd DB=1`、静止脉冲 duty/节奏保持不变。 |
| 编译结果 | 首次编译因 Flash 超出 `32 B` 失败；压缩 LCDM 状态字符串后 Ubuntu 编译通过，LTO 版 `text=19264 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `b48a8f492b66f8112ba465dfd181ee3f0256e77d10cbae93bf0af071707b9685`；ELF `d688a3ec3b25204a9f12ed4fa9b4d51cc58526a65da44c0a975e43bd97bc3fd1`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800454d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us`：刚到位后允许短时间找平衡，但 3 秒后应冻结参考点；再用手左右拨舵板，应能短暂 `DRV +/-` 反驱，不能完全无声无力。长时间静止若仍偶发响一下，再调静止触发阈值/脉冲节奏，而不是恢复长期学习。 |

2026-06-09 静止 Brake-first 锁止下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “会嗞嗞声，左右拨舵板也会嗞嗞声，停不下，没找到问题的根本，而且锁止力更差了。” |
| 现场现象/风险 | 学习窗口限制版仍会嗞嗞；手拨左右也嗞嗞且停不下；锁止力更差。说明短脉冲/Coast 方案仍没有抓到根本，反而因为静止时大部分时间释放 H 桥，使马达没有被电气短接锁住。 |
| 我的判断 | 当前根本问题是静止区不应该靠低占空比 `DRV` 或脉冲反驱来维持位置；低 duty 会发声且锁力弱。静止时应先用 H 桥 `Brake` 做无 PWM 短接锁止，P50 显示应为 `HOLD` 而不是 `DRV`。只有偏离明显超过刹车带，才主动反驱。 |
| 修改内容 | 撤掉静止路径中的长期学习/脉冲节奏实际作用，改成 Brake-first 两段锁止：HOLD 误差在 `Deadband + 24 + 72 count` 内统一 `HBridge_Brake()`，不再 Coast、不再低占空比脉冲；超过该刹车带并连续确认后，才以不低于 `320/1000` duty 主动反驱，回到刹车带内立即回 `HOLD/Brake`。同时修正早期 HOLD 分支：只要 P30 `Hold=1`，已经 HOLD 后也持续 Brake，不再第二圈切回 Coast。运动追目标、速度包络、PWM-ADC 映射、`Cmd DB=1` 保持不变。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=18936 B`、`data=1072 B`、`bss=1528 B`。 |
| HEX/ELF SHA256 | HEX `8f06bd9010d4bb419d78526f990d4083c166e771cbb6e9e3ba5ddce19751e440`；ELF `2724c66d94ee254b3d12305e79601604e65852a86ea45b8873f42f06200c08fd`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004405`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us`：P50 HBR 静止时应主要显示 `HOLD`，不能长期 `DRV`；用手拨舵板，轻微拨动应主要靠 Brake 抗住，明显拨开才短暂 `DRV +/-`，放手后应回 `HOLD` 并停响。若 Brake 本身仍发声或无锁力，需要核查 H 桥 `Brake` 引脚组合是否真正短接马达。 |

2026-06-09 GitHub 参考与运动到位捕获下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “一直DRV，这个算法还是有问题，是否可以查找一下github里面其他人已经做好的程序来参考，这样会更快？” |
| 现场现象/风险 | P50 一直显示 `DRV`，说明固件没有进入 `HOLD/Brake`，因此无论 Brake-first 如何写，只要状态仍卡在运动阶段，就会持续 PWM 输出并嗞嗞。 |
| GitHub 参考 | 查阅 `misan/dcservo` 的 `dcservoUNO.ino`。该开源 DC servo 结构要点：使用 PID 计算输出；主循环中如果 `input == setpoint` 就 `pwmOut(0)`，否则才输出 PID；PWM 配置到约 `31kHz`；H 桥输出通过一边 PWM、一边关断的 sign-magnitude 方式实现。参考链接：`https://github.com/misan/dcservo/blob/master/dcservoUNO.ino`。 |
| 我的判断 | 对照后确认我们当前最大问题不是“静止 Brake 分支写得不够多”，而是运动状态进入 HOLD 的条件太苛刻：`Cmd DB=1 count`，实际 ADC 噪声/机械惯性下几乎无法精确命中，所以会一直 DRV。之前直接把 `Cmd DB` 放到 `64` 是错误，因为它改变了整个命令死区；更合适的是单独加“运动到位捕获带”，只用于运动结束时切断 DRV。 |
| 修改内容 | 新增 `LCDM_SERVO_CMD_ARRIVE_BAND=36 count`：当 `move_dir != 0` 且目标误差进入 36 count 内，立即 `HBridge_Brake()`、状态置 `HOLD`、锁存当前 ADC 作为 HOLD 参考，并清除运动方向和静止保持计数。这样不再要求 `Cmd DB=1` 精确命中才进入 HOLD。保留上一版 Brake-first 静止策略；运动速度、PWM-ADC 映射、`Cmd DB=1` 不变。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19028 B`、`data=1072 B`、`bss=1528 B`。 |
| HEX/ELF SHA256 | HEX `069dbe980e2a01249ba5fff82fd7d24dfb23dd2ded2179fa9e7fe92da590b6ca`；ELF `fcd031d75a506659af5a289d081e3b24498b6f74e535c7b75fcbfdffc79990e4`。 |
| 下载结果 | Ubuntu pyOCD 首次 `hidapiusb` 因 `already open` 失败，脚本复位 DAPLink 后用 default backend 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004461`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us` 或从 `1000/2000us` 回到目标，重点看 P50 是否从 `DRV` 变成 `HOLD`。若仍长期 `DRV`，下一步继续扩大“到位捕获”或加入“速度过低但误差不再缩小则强制 HOLD/Brake”；若能进 `HOLD` 但仍有可听声，再查 PWM 频率和 H 桥 Brake 引脚组合。 |

2026-06-09 近目标停滞捕获下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “没有太大变化” |
| 现场现象/风险 | 上一版 `36 count` 到位捕获效果不明显，说明实际运动可能没有稳定进入 36 count 小范围；P50 仍可能长时间显示 `DRV`。风险是运动阶段持续输出，静止 Brake-first 分支仍然没有机会接管，嗞嗞声和持续电流继续存在。 |
| 我的判断 | 只靠“误差进入小到位带”不够。真实机械有惯性、摩擦、ADC 采样抖动，可能在目标附近误差不再继续缩小但又没进 36 count。此时继续 DRV 只会维持嗞嗞和过冲，应增加“接近目标且误差停滞、速度很低”的强制 Brake/HOLD 捕获。 |
| 修改内容 | 新增 `LCDM_SERVO_CMD_SETTLE_BAND=160 count` 和 `LCDM_SERVO_CMD_SETTLE_COUNT=24`：运动中若误差已进入 160 count、误差不再明显缩小、且 ADC 朝目标速度 `<=2 count/ms`，连续约 24 个控制周期后强制 `HBridge_Brake()`、状态置 `HOLD`、锁存当前 ADC 为 HOLD 参考，并清除运动方向。保留上一版 `36 count` 直接到位捕获和 Brake-first 静止策略；不改运动速度、电流、PWM-ADC 映射、`Cmd DB=1`。为放入 20KB Flash，删除旧静止脉冲/慢漂实验残留变量，并压缩部分 LCDM 文案如 `Manual -> Man`、`Deadband -> DB`、`P gain -> P`、`D damp -> D`、P90 接线说明短文本。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19252 B`、`data=1072 B`、`bss=1528 B`。本次修改前链接因 Flash 超出 `32 B` 失败；清理残留和压缩 LCDM 文本后通过。 |
| HEX/ELF SHA256 | HEX `3db2829120224a5b812d593c4983cbb2126c14c2ba3cde0d9a8b8fa1effaae79`；ELF `fa0c28043e840c8cb2ad82b77b7dd6704d557ffc312204c42d9f8a3db1a508e4`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800457d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us` 或从 `1000/2000us` 回到目标，重点看 P50 HBR 是否能从 `DRV +/-` 自动切到 `HOLD`。若仍长期 `DRV`，说明停滞判定没有触发，需要现场记录 P50 的 `E`、`O`、`ADC W` 和 HBR duty；若能进 `HOLD` 但仍嗞嗞，则下一步查 H 桥 Brake 输出组合/PWM 频率，而不是继续加大运动 duty。 |

2026-06-09 ADC 跳点剔除与 W 验证下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还是解决不了，我觉得出现问题的可能性还是在ADC，程序要过滤超出平均值的下下跳动的AD数，可以反应到ADC显示这里，W要接近0,这样是第一步，然后再来完善未完成的静止状态下不要再运动H桥的做法，一旦到达目标点，就不再受干扰随便动，除非一直测出PWM一直是有增加或减少为止才能动H桥，以上只是参考。” |
| 现场现象/风险 | 运动到位捕获和近目标停滞捕获仍不能解决嗞嗞/DRV 问题，说明触发源可能不是目标算法，而是 ADC 反馈本身仍有跳点。若 ADC 控制值持续抖动，静止 HOLD 会被假误差反复打出 `DRV`，后面所有 H 桥静止逻辑都会被误触发。 |
| 我的判断 | 先不要继续加力或大改 PID。第一步必须把 P50 的 ADC 行稳定下来：`R` 显示原始采样均值，`C` 显示闭环控制实际采用的 ADC，`W` 显示控制 ADC 在显示周期内的窗口波动。若静止时 `W` 不能接近 0，说明还在吃 ADC 跳点；若 `W` 接近 0 但 HBR 仍 `DRV`，下一步再改静止状态机，让 PWM 目标未连续变化时不主动运动 H 桥。 |
| 修改内容 | 在 LCDM 调试闭环中增加控制用 ADC 跳点剔除：首次采样直接建立 `C`；之后原始 ADC 与当前 `C` 差值 `<=12 count` 时视为静止噪声，`C` 不变；超过 12 count 的变化必须连续出现才采信，其中普通变化连续 2 次采信，大跳变超过 `36 count` 要连续 3 次采信。P50 的 `W` 改为统计剔除后的 `C` 波动，不再统计原始 `R` 波动。这样静止时若只是 ADC 上下跳，`C/W` 应明显稳定；真实舵板连续移动仍可被跟随。为放入 20KB Flash，压缩不影响调试的 LCDM 文案，如 `Target -> Tgt`、`Feedback -> FB`、`Error -> Err`、底部 `DEC/BACK/INC -> -/BK/+`。 |
| 编译结果 | 首次编译因 Flash 超出 `160 B` 失败；压缩文案并简化跳点确认状态后 Ubuntu 编译通过，LTO 版 `text=19280 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `4d355609ad562ce838b759b27ede9afc5b68d4d3f4f3ec6915070a5b863bffac`；ELF `944c639c8aef9cb68a1d2f65aacca679f9c00b74d759cc42f0213af7d44fae5f`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080045d5`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us` 且不要碰舵板，先看 P50 第三行 `R... C... W...`：`R` 可以有原始小跳，关键是 `C` 应稳定、`W` 应接近 0。若 `W` 仍明显大，继续加强 ADC 采样/滤波或查硬件干扰；若 `W` 已接近 0 但 HBR 仍出现 `DRV`/嗞嗞，再进入第二步：静止状态下 PWM 未连续增加/减少时禁止主动运动 H 桥，只保留 Brake/HOLD。 |

2026-06-09 精度/过冲/锁止力分离调参下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “你个笨笨，目前嗞嗞声搞定了，ADC的取值获取和算法还要继续完善，说明这里的问题很大。现在的问题就是走不准，过冲，锁止力，请继续加油” |
| 现场现象/风险 | 嗞嗞声已经解决，证明 ADC 跳点剔除和静止不乱驱动方向有效；剩余主要问题变成位置精度、目标点过冲和外力锁止力。若继续只处理安静，会牺牲锁止力；若只加力，会重新带来过冲和抖动。 |
| 我的判断 | 当前代码里仍有三个偏粗的参数：运动到位捕获带 `36 count` 偏大，可能导致“还没准就 HOLD”；静止 Brake 区约 `DB 8 + quiet 24 + brake 72 = 104 count` 偏宽，外力偏移较大才主动反驱，锁止力表现弱；运动速度包络 `allow_vel=(error>>6)+1`、最大 `14 count/ms` 对当前机械惯性仍偏快。 |
| 修改内容 | 保留上一版 ADC 跳点剔除和 P50 `R/C/W` 显示不动。收紧到位和静止区：`CMD_ARRIVE_BAND 36 -> 18 count`，`CMD_SETTLE_BAND 160 -> 96 count`，`CMD_SETTLE_COUNT 24 -> 18`；静止安静区 `HOLD_QUIET_EXTRA 24 -> 12`，主动锁止确认 `4 -> 2`，Brake 额外带宽 `72 -> 24`，主动锁止起步 duty `320 -> 420`，静态最小锁止 duty `280 -> 360`。减小过冲：命令运动最小 duty `90 -> 80`，满速误差阈值 `700 -> 820`，允许速度曲线从 `(error>>6)+1 / max14` 改为 `(error>>7)+1 / max10`，越过目标后的反向修正上限 `70 -> 55`。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19276 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `00a4c70f6e54cdf6e5938d1fa03c0653d58c15fd711b35ec9e40094825747183`；ELF `40932d41a145b6c86adf76f3119d88f6f61a59f8bc630ee1313a29b77c157fb6`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080045d1`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 先测试固定 `1500us` 和小步 `1000/1500/2000us`：P50 的 `W` 应仍接近 0，HBR 不应恢复嗞嗞；观察 `O` 是否比上一版下降、到位误差 `E` 是否更小。再用手轻拨舵板，确认超过小偏移后 HBR 能更快进入 `DRV +/-` 反向锁止，放手后回 `HOLD`。若过冲下降但走得太慢，下一轮只放宽速度包络；若锁止仍弱，下一轮只提高静止锁止 duty/缩小 brake 区。 |

P50 `Input state` 含义：`FRESH` 表示当前 PWM 有效；`NO EDGE H/L` 表示 PA4 保持高/低且没有边沿中断；`BAD PWM H/L` 表示有边沿但脉宽或周期不在有效范围；`STALE H/L` 表示之前有效、后来超时。

说明：这是用于快速确认界面结构和 LCDM 串口显示能力的临时固件。后续正式版仍应建立 TJC `.HMI/.tft` 工程，把页面、触摸按钮和控件名固定下来，再由固件改为更新控件值和解析按键命令。

### 2026-06-05 交互版更新

已把运行时界面从自动轮播改为固定层级菜单：

| 项目 | 当前实现 |
|---|---|
| 屏幕方向 | 按 `480x320` 横屏整屏绘制 |
| 显示语言 | 中文 + 英文关键词共存 |
| 主屏 | 固定在 `P00 主监控 / Main Monitor`，约 750ms 刷新监控数据 |
| 触摸输入 | 开启 `sendxy=1`，通过坐标判断菜单行和底部三段按钮；主菜单按下即进入，底部按钮 y 方向扩大到 232-271 |
| 底部按钮 | 横屏三段：左 `0-160`，中 `160-320`，右 `320-480` |
| 菜单 | 点击 `P01` 行进入 `P10/P13/P20/P30/P40/P50/P60/P70/P80/P90` |
| 参数页 | 点击参数行选中；左侧或底部“减少 -”减小，中右侧或底部“增加 +”增大；中间“返回 BACK”回主菜单 |
| 状态指示 | `D2` 继续微闪表示程序正在跑 |

本次编译烧录：

| 项目 | 结果 |
|---|---|
| 编译 | 通过，Flash 13240 B，`data+bss=2376 B` |
| 烧录 | 通过，擦除 16384 B，写入 14464 B |
| 启动 | pyOCD 已写 `SP=0x20000c00`、`PC=0x08002685` 并 `go` |

注意：当前仍是运行时绘图测试版，方便现场确认菜单逻辑、触摸坐标和页面层级。正式工具界面后续应固化为 TJC `.HMI/.tft` 工程，以获得更好的字体、中文显示、按钮状态和输入控件体验。

### 2026-06-05 固定层级菜单修正

现场发现点击菜单后页面没有稳定停留。原因是 TJC 坐标回包会同时发送按下和松开事件，旧逻辑两个事件都处理，导致一次点击可能先进入菜单、松开时又触发新页面底部返回。已改为只处理按下事件 `event=1`。

同时调整：

| 项目 | 处理 |
|---|---|
| 中文乱码 | 运行时绘图固件改回纯 ASCII 英文；中文需后续通过 TJC `.HMI/.tft` 工程内置字库实现 |
| 主屏刷新 | 主屏/诊断页整页刷新从 750ms 改为 5000ms，避免影响触摸操作 |
| 页面逻辑 | 主屏 `MENU/TEST/DIAG`，菜单行进入下级页，参数页 `DEC/BACK/INC` 固定 |
| 烧录启动 | `flash_run_in_ubuntu.sh` 的 commander 启动步骤改用 pyOCD 默认 USB backend，避免 `hidapiusb` 在 load 后偶发 `already open` |

本次已烧录并手动启动：

| 项目 | 结果 |
|---|---|
| 编译 | 通过，Flash 12132 B，`data+bss=2376 B` |
| 烧录 | 通过，擦除 16384 B，写入 13312 B |
| 启动 | pyOCD 默认 backend 已写 `SP=0x20000c00`、`PC=0x080026f5` 并 `go` |

### 2026-06-05 下载运行链路修正

后续 LCDM/舵机实物调试前，必须先稳定下载运行链路。已确认当前问题不是 LCDM 固件是否能编译，而是 DAPLink 在 Parallels Ubuntu USB 下通信不稳定，加上 `BOOT0/CON3=VCC` 复位不进用户 Flash。

已做处理：

| 项目 | 状态 |
|---|---|
| Ubuntu 脚本 | `tools/flash_run_in_ubuntu.sh` 默认频率降为 2MHz，并改为同一 pyOCD 会话内执行 load + 写 SP/PC + go，减少二次打开 DAPLink |
| macOS 脚本 | 新增 `tools/flash_run_on_macos.sh`，用于绕开 Parallels USB，直接在 macOS 原生 pyOCD 下烧录/运行 |
| macOS pyOCD | 临时环境 `/tmp/pyocd-mac-venv` 已安装 |
| Puya pack | `/tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack` 已准备 |
| Parallels USB | DAPLink 自动连接策略已设为 `host`；当前仍被 Ubuntu 占用，需重插后生效 |

后续优先用：

```bash
./tools/build_in_ubuntu.sh SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1
./tools/flash_run_on_macos.sh
```

只有确认下载后 D2 微闪、LCDM 主界面稳定显示后，才继续舵机产品参数调试。

2026-06-09 突击拨动误判与目标前预刹下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “W要等一下才回0，O没有什么变化，手拨舵板时HBR是反应快回到HOLD，但这里有个问题看是现在修正还是迟点再修，也就是说如果连续拨舵板，即突击性的用力，会误判，然后松锁，这个可能是反应慢还是其它原因造成的。过冲无变化。” |
| 现场现象/风险 | `W` 需要等一下才回 0，说明显示窗口和 ADC 控制滤波仍有记忆时间；`O` 无变化，说明上一版速度包络对过冲帮助不足；连续突击拨舵板会误判后松锁，风险是静止受外力时触发 `DIR ALM` 后 `HBridge_Coast()`，造成锁止被释放。 |
| 我的判断 | 静止状态下的外力拨动不应参与“电机线方向反了”的报警判定。方向报警只适合在 PWM 命令主动移动时保护接线，不能在 HOLD/外力锁止阶段触发。过冲无变化则需要在接近目标且 ADC 速度仍朝目标方向时，提前给小反向刹车，而不是等越过目标后再纠正。 |
| 修改内容 | 保留上一版已解决嗞嗞声的 ADC 跳点剔除和静止 HOLD 逻辑。把 `lcdm_servo_reverse_alarm_update()` 限制为 `s_lcdm_servo_move_dir != 0` 时才执行，避免静止或外力突击拨动触发 `DIR ALM/Coast` 松锁。新增目标前预刹：运动中若 `vel_toward >= 3` 且误差进入 `44 + vel_toward * 16 count` 区域，先输出 `+/-55 duty` 的轻反向刹车。为放入 20KB Flash，仅压缩 LCDM 非关键文字：`P80 Save -> P80 Sav`、`P90 Wire -> P90 IO`、`DO ROW%u -> D%u`、`PAGE %u -> P%u`、部分异常状态短名；P50 的 `R%u C%u W%u`、`E%+d O%u`、`DRV +/-`、`HOLD` 保持不变。 |
| 编译结果 | 首次编译因 Flash 超出 `44 B` 失败；压缩 LCDM 文本后 Ubuntu 编译通过，LTO 版 `text=19280 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `5cc517a437b9dfaed7a0a54f2c7a69742e1f6ac4f6dbaa4613a84396a7858706`；ELF `1114dacb242c7566997deeff94aee6083ddd93fbdccc93ac928ea39cb15143d5`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080045f9`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 固定 `1500us` 先看 P50：`W` 是否仍只是延迟回 0、不会重新嗞嗞；连续突击拨舵板时 HBR 不应进入 `ALM/Coast` 松锁，应快速回 `HOLD` 或短暂反驱；从 `1000/1500/2000us` 切换时观察 `O` 是否下降。若 `O` 仍无变化，下一步不再改报警，而是继续调目标前预刹强度/区域。 |

2026-06-09 锁力确认后过冲专项下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “锁力目前可以了，现在是要解决过冲问题了。” |
| 现场现象/风险 | 锁力已达到当前可接受状态，本轮不能再改静止 HOLD、静止反驱、锁止 duty 等参数，否则容易把已确认的锁力和嗞嗞声状态破坏。当前重点只剩命令运动到目标时的过冲 `O`。 |
| 我的判断 | 上一版目标前预刹触发偏晚：`44 + vel_toward * 16 count` 的预刹区太窄，且反向刹车上限 `55 duty` 偏弱。速度超限分支实际只是 `pd=0`，对应 H 桥滑行，不能有效消耗惯性，所以过冲改善不明显。 |
| 修改内容 | 只改命令运动阶段预刹参数，不改锁力和静止 HOLD 逻辑。反向预刹上限 `LCDM_SERVO_CMD_REVERSE_LIMIT 55 -> 95`；预刹最低触发速度 `3 -> 2 count/control`；预刹基础区域 `44 -> 96 count`；速度扩展区域从 `vel_toward * 16` 改为 `vel_toward * 32`。也就是更早进入预刹，并用更强的短反向输出抵消惯性。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19276 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `166e90ae657c5c54dc7846b0eccf00fd9196fd1aadc297a9458652ea0e281a4d`；ELF `c4ed772f7bf22c762d181ece3acf5040ee1541af849158285e16c2d3e7130534`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080045f5`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看 P50 的 `O`：从 `1000 -> 1500`、`1500 -> 2000`、`2000 -> 1500` 三种切换分别记录最大 `O`。同时确认锁力没有下降、静止不恢复嗞嗞。如果 `O` 明显下降但到位变慢，下一步缩小预刹区；如果 `O` 仍大，下一步把速度超限分支从滑行改为主动刹车/反刹，而不是继续改锁力。 |

2026-06-11 近目标速度超限主动反刹下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “继续” |
| 现场现象/风险 | 上一轮已经确认锁力方向可用，本轮继续沿着过冲问题推进。当前运动控制里，目标前预刹之外还有一个分支：当 `vel_toward > allow_vel` 时只是 `pd=0`，也就是滑行。对有惯性的齿轮箱/电机，滑行不能明显降低过冲。 |
| 我的判断 | 不能再动静止锁力和 HOLD 逻辑。过冲应继续从命令运动阶段处理：远距离速度超限仍允许滑行，避免全程变慢；进入目标半程附近后，如果 ADC 速度仍超过允许速度，则主动给反向刹车，提前消耗惯性。 |
| 修改内容 | 保留上一版预刹参数 `REVERSE_LIMIT=95`、`PREBRAKE_MIN_VEL=2`、`PREBRAKE_BASE=96`、`vel_toward * 32`。新增近目标速度超限主动反刹：当 `vel_toward > allow_vel` 且 `abs_error <= LCDM_SERVO_FAST_FULL_ERROR / 2` 时，不再 `pd=0` 滑行，而是输出与运动方向相反的 `+/-95 duty`；若仍在远距离，则继续 `pd=0` 滑行。为放入 20KB Flash，仅把异常状态文字 `ADC LOST` 压缩为 `ADC`，P50 的 `R/C/W`、`E/O`、`DRV/HOLD` 保持不变。 |
| 编译结果 | 首次编译因 Flash 超出 `4 B` 失败；压缩异常状态文字后 Ubuntu 编译通过，LTO 版 `text=19272 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `9efa547ac73af6c1e052080fb1d72e6a6c958f2a40dd11aeb1a915d00dbb57e0`；ELF `3c2e047c0654ef0cdaef6e33563d5cbd733565c46b0b93f15e716a8e600b3ebb`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080045fd`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 继续只看过冲：记录 `1000 -> 1500`、`1500 -> 2000`、`2000 -> 1500` 的最大 `O`。若 `O` 明显下降但运动变慢，下一步降低反刹进入区域或反刹 duty；若 `O` 仍大，下一步改成按速度大小分级反刹，而不是固定 `95 duty`。 |

2026-06-11 分级刹车与近点 Brake 下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还是不行，到点后来回摆几下才停，明显是很有做好分级刹车和预判等算法，继续改良” / “继续，不要停” / “hi” |
| 现场现象/风险 | 到点后来回摆几下，说明上一版“近目标速度超限固定 `95 duty` 反刹”太粗。它能刹车，但接近目标时仍可能反向打过头，造成二次摆动，最后靠多次来回才停。 |
| 我的判断 | 问题重点从“有没有刹车”变成“刹车档位和目标距离匹配”。非常接近目标时不应再反向驱动，而应直接 H 桥 Brake；稍远且速度高时才反刹，并且反刹 duty 要按速度差分级。锁力和静止 HOLD 仍不能动。 |
| 修改内容 | 保留静止锁力/HOLD/ADC 过滤逻辑。将命令运动阶段固定反刹改成分级：`abs_error <= 48 count` 时 `pd=0`，运动状态下进入 `HBridge_Brake()`；`48-96 count` 内反刹 duty 最高限制为 `55`，避免近点反打；更远且速度超限时按 `24 + (vel_toward - allow_vel) * 12` 分级反刹，最高仍限制在 `95`。预刹触发仍使用 `96 + vel_toward * 32`。为放入 20KB Flash，压缩若干非关键参数菜单名，如 `Dwell -> Dw`、`FB ADC -> FB`、`Cmd DB -> CDB`、`Profile -> Prof`；P50 的 `R/C/W`、`E/O`、`DRV/HOLD` 保持不变。 |
| 编译结果 | 首次编译因 Flash 超出 `24 B` 失败；压缩非关键 LCDM 参数名后 Ubuntu 编译通过，LTO 版 `text=19260 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `fd020a1317e4d52a23a83671b467d5e44f42e5223769627208a8afc464e03c01`；ELF `6f26a1925c37e820cc97b791288f6feb5c29baca7c8d5a489899d5f1619e1bb8`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800461d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 看“到点后来回摆几下”是否减少：重点观察 `1500 -> 2000` 和 `2000 -> 1500` 两个方向，记录 P50 最大 `O`，以及到点后是否仍出现反向摆动。如果仍来回摆，下一步继续降低近点反刹上限或扩大 Brake-only 区；如果过冲小但停得慢，下一步缩小 Brake-only 区或提高远点分级反刹。 |

2026-06-11 动态提前 Brake 下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还是一样，没有明显变好” |
| 现场现象/风险 | 上一版分级反刹仍无明显改善，说明问题不只是反刹 duty 大小，而是近目标仍在反向驱动，造成刹过头后再摆回。 |
| 我的判断 | 继续压过冲应减少近目标反向驱动，改为按 ADC 速度动态提前 Brake。速度越快，提前 Brake 的误差范围越大，让机械惯性自己进入目标区，避免固定反刹造成二次摆动。 |
| 修改内容 | 命令运动阶段改为动态 Brake-only 区：若进入预刹/速度超限条件，且 `abs_error <= 72 + vel_toward * 16`，直接 `pd=0`，运动状态下执行 `HBridge_Brake()`；只有误差仍大于该动态区时，才按速度差分级反刹，最高仍为 `95 duty`。为省 Flash，简化了上限判断和近点二次限幅。锁力、静止 HOLD、ADC 过滤不动。 |
| 编译结果 | 中间版本因 Flash 超出 `8 B` 失败；简化动态 Brake 代码后 Ubuntu 编译通过，LTO 版 `text=19268 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `42fbb75b4d6a289d5d638addacf976ee75677b714355c8daa5739ee4c0585ad2`；ELF `cb9b87082f552b1e6a751615ad1baf71f401e6ca685f86dd783c1c40b3ec4a98`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004625`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 看是否还“到点后来回摆几下”。若仍无变化，说明当前控制周期/ADC 速度判断可能太慢或 Brake 组合刹不住，下一步应改控制结构：以目标变化开始时记录行程和速度，按剩余误差做三段式 Drive/Brake/Hold，而不是继续在现有分支里微调 duty。 |

2026-06-11 过冲曲线图表与实测反馈记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “O361,这又倒回原来出现的问题，我们现在能不能把要调的几个重要参数做成一个整个曲线函数，微调了哪里可以一眼看出，以备下次改进进对比和分析，能做出来这个的图表吗？同时结合我反馈的实际情况。” |
| 现场现象/风险 | 实测 `O=361`，说明动态提前 Brake 版本不是改善，而是回到明显过冲问题。继续盲目改单个 duty/阈值会浪费时间，也无法判断每次改动改变了曲线哪一段。 |
| 我的判断 | 需要先把控制算法表达成 `E=误差`、`V=速度`、`duty/Brake/HOLD=输出` 的曲线，并把每版实测 `O` 和主观现象记录进去。当前曲线显示：近目标仍依赖 Brake/反刹切换，若 Brake 无效或切换太晚，就会出现 `O=361` 和来回摆。 |
| 修改内容 | 新增 `docs/servo_motion_curve_tuning.md`，记录当前曲线公式、关键参数、用户反馈 `O=361`、问题推断和下一版三段式 `DRIVE -> BRAKE -> HOLD` 建议。新增 `docs/servo_motion_curve_current.svg`，用图表显示当前 `HOLD`、Brake-only、Reverse brake、Forward drive 曲线，并标记 `O=361` 实测点。 |
| 编译/下载 | 本次只新增分析文档和图表，不改固件、不编译、不下载。当前板上仍是上一条动态提前 Brake 固件。 |
| 下一步验证点 | 先确认图表/表格表达方式是否够直观。下一版固件应按文档建议改三段式状态机，而不是继续在现有分支里微调 `95/72/96` 这类单点参数。 |

2026-06-11 参数逻辑关系与换件调试方法记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “而且你也可以从我们调试中，抓取和分析出所有参数的逻辑关系，这样将来换了零配件，也好有可以参考的调试方法。” |
| 我的判断 | 这是必要的。舵机参数不能只记录单个数值，要按“输入目标、反馈 ADC、运动驱动、刹车到位、静止锁止、噪声测量”分层分析，否则换电机、齿轮、电位器、H 桥或供电电压后会重新盲调。 |
| 修改内容 | 扩展 `docs/servo_motion_curve_tuning.md`：新增“参数逻辑关系”“换零配件后的调试方法”“零件特性和参数方向”“下一步固件参数化建议”。把当前 `DB/CDB/Max/allow_vel/prebrake/brake_only/reverse_brake/HOLD/ADC W/PWM W` 的相互影响和调试顺序整理成表格。 |
| 编译/下载 | 本次仍只改文档，不改固件、不编译、不下载。当前板上固件不变。 |
| 下一步验证点 | 下一版固件建议先把三段式运动控制参数化，至少把 `motion_min_duty`、`motion_full_error`、`allow_vel`、`prebrake`、`brake_only`、`reverse_brake`、`motion_hold_band` 做成可调或集中曲线表。 |

2026-06-11 近目标 Brake-only 抑制左右晃下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前我看不出来所做的CURVE图，请继续接上一版本改善左右晃，并在CURVE用红色字再描述一下，看是否合理。” |
| 现场现象/风险 | `O=361` 且到点左右晃，说明上一版动态 Brake/反刹曲线仍不清晰，且反向刹车/静止 HOLD 接管可能造成来回摆。 |
| 我的判断 | 先验证一个更简单的曲线：近目标禁止继续正向驱动，也禁止反向刹车，只做 Brake；越过目标后不立刻切成静止锁止，而是保持运动 Brake，速度低且误差进入 180 count 内才切 HOLD。目标是先减少左右晃，再恢复精度。 |
| 修改内容 | 固件中命令运动阶段改为：若 `vel_toward > 0` 且 `abs_error <= 360`，直接 `pd=0` 进入 Brake；若速度超过允许且 `abs_error <= 410`，也 Brake；删除上一版复杂反刹分支。`duty==0` 且仍处于运动方向时，保持 Brake，只有 `abs_error <= 180` 且 `vel_toward <= 1` 才清除 `move_dir` 进入 HOLD。SVG 曲线图增加红色说明：`E<=360 and moving toward target = BRAKE, no drive/reverse`。 |
| 编译结果 | 首次加入完整三段式退出计数后 Flash 超出 `92 B`；删除复杂反刹分支和简化退出条件后 Ubuntu 编译通过，LTO 版 `text=19280 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `fe15db8eeca9e5e0b34d598199adfc91fe277a9bf891fadfcba8348aa4bc8149`；ELF `28a83ab059f53a4608364a8893adb968c26c21f99bde641f1d9b995e70c8b7a0`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004631`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看左右晃是否减少，而不是只看 O。若晃动明显减少但停点偏差大，下一步再把 Brake-only 区缩小或增加低 duty 慢速靠近；若晃动仍一样，说明 H 桥 Brake 或 ADC 速度判断本身没有起到预期作用，需要测 Brake 输出组合和控制周期。 |

2026-06-11 运动 Brake 保持防反复驱动下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “这个也改得没一点进步呢。O也大，ADC的W在晃动也大，不一定是本身大，所以还是没有从根本上解决晃动的问题。” |
| 现场现象/风险 | `ADC W` 大很可能是马达左右晃带着电位器在动，不是 ADC 本身噪声。因此继续调 ADC 滤波会误判。上一版近目标 Brake 后仍无改善，说明程序可能下一控制周期又重新驱动，形成 Brake/Drive 反复切换。 |
| 我的判断 | 根因更接近“运动状态切换反复驱动”而非 ADC 噪声。必须在进入近目标 Brake 后保持若干控制周期，禁止下一圈因为误差还没进死区或速度反向就重新正向驱动。 |
| 修改内容 | 将原近目标停滞计数复用为运动 Brake 保持计数。进入 `abs_error <= 360` 或近目标速度超限 Brake 时，设置 `s_lcdm_servo_motion_settle_count=8`；只要计数未结束且未达到 `abs_error<=180 && vel_toward<=1`，控制循环直接保持 `HBridge_Brake()` 并返回，不再计算新的驱动 duty。删除上一版复杂反刹逻辑，保留“近目标只 Brake、不反打”的简化曲线。SVG 红字说明同步改为 Brake 后保持 8 个控制周期。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19108 B`、`data=1072 B`、`bss=1536 B`。 |
| HEX/ELF SHA256 | HEX `9d3dc33630a1db036d1c41daad3cb8911abfd8a8799b3ff455fc48335960e55a`；ELF `82c2d3343d6d13d1bb088d6acb386cd12b9a44b1d456d4ad762dfe05033cbb42`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004585`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 看 P50 的 `ADC W` 是否随左右晃减少而下降；若 W 仍大且舵板仍明显摆动，下一步不要调 ADC，要验证 H 桥 Brake 是否真的短接有效，或把 Brake 阶段改成可控短反刹脉冲。 |

2026-06-11 P50 速度/BRK 诊断下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前看到没有任何改善，可能要重新打架构了，除了还是在抖，还有时会慢慢的到到目的地，这是不正常的，不然你还是自己看一下ADC值的步进逐级加和减的线性问题，然后看给的PWM值是否充够到目标点，但前提是可能要分100级或更高，这样才能分析出哪里出问题，甚至要介入一个速度环，也就说，看ADC值的变化量来知道马达给的力够不够，然后来进行刹车动作等，请重新考虑。” |
| 现场现象/风险 | 继续微调单个刹车阈值已经不能判断根因。当前需要先把“驱动中”“运动刹车中”“真正静止保持”区分出来，并看到每个 LCDM 更新周期内控制 ADC 的步进速度，否则会把慢爬、过冲反冲、Brake 无效和目标映射不准混在一起。 |
| 我的判断 | 下一步要重新整理成位置环加速度环/刹车状态机，但在改架构前必须补齐 P50 诊断数据。若 `BRK` 时 `V` 仍大，说明 Brake 或惯性控制不足；若 `D+/-` 时 `V` 很小且误差还大，说明驱动力/速度环不足；若 `HLD` 后 `V` 和 `W` 仍大，才继续查 ADC/机械抖动。 |
| 修改内容 | 不改变主控制曲线参数。新增独立的 ADC 步进速度缓存，P50 第 5 行 HBR 显示改为短格式：`D+160 V+5` / `D-160 V-5` / `BRK V+2` / `HLD V0`。修正运动阶段 `duty=0` 和 Brake 保持分支的状态显示，从原来误显示 `HOLD` 改为 `BRK`；真正清除运动方向并进入静止保持时才显示 `HLD`。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19228 B`、`data=1076 B`、`bss=1532 B`。 |
| HEX/ELF SHA256 | HEX `440c53b732accda4084471990fa4cf5efe51c0190ff976cf57e90be54d6e7560`；ELF `489657689974922cc10d4b698652bf85344a406d19598bedb532602002cca10b`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080045f5`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 在 P50 固定观察三种动作：`1000 -> 1500`、`1500 -> 2000`、`2000 -> 1500`。请记录每次最大 `O`、到点后 HBR 是 `D+/-` 还是 `BRK/HLD`、以及此时 `V` 大约是多少。若出现“慢慢爬到目的地”，重点看是不是 `D+/-` 且 `V` 很小；若出现左右晃，重点看 `BRK` 期间 `V` 是否仍正负摆动。 |

2026-06-11 速度超限反刹/近点 Brake 下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “O最大是170，HLD,V+0” / “继续” |
| 现场现象/风险 | `HLD V+0` 表示最终静止保持和 ADC 最终稳定不是当前主要问题；`O最大170` 表示运动阶段仍有较明显越过目标。问题重点转为运动段目标前速度控制：刹车太晚、Brake 区过粗，或者速度超限时没有足够反刹。 |
| 我的判断 | 静止 HOLD/锁力暂时不动。上一版在 `abs_error <= 360` 时一律 Brake，容易出现两种问题：高速时 Brake 仍不够，低速时又过早停止导致慢慢爬。应改为“允许速度曲线 + 刹车距离”：剩余误差越小，允许 ADC 速度越低；速度超过允许且离目标还远时用受限反向刹车；离目标很近时只 Brake，避免反打造成二次摆动。 |
| 修改内容 | 运动段 `allow_vel` 改为 `(abs_error >> 6) + 1`、最大 `12 count/loop`。新增动态近点 Brake 区 `72 + vel_toward * 12`，最大 `260 count`；进入此区只 Brake 并保持运动刹车计数。若 `vel_toward > allow_vel` 且仍在 Brake 区外，则按 `24 + (vel_toward - allow_vel) * 10` 给反向刹车，最高仍限制 `95 duty`。不改变静止 HOLD、锁力、ADC 滤波、PWM-ADC 映射。为放入 20KB Flash，仅压缩 LCDM 非关键文字：`P50 PWM -> P50`、`READY -> RDY`、`SELECT -> SEL`、`NO PWM -> NPWM`、页脚 `MAIN/BACK/ENTER -> MN/BK/ENT` 等；P50 关键数据 `R/C/W`、`E/O`、`D/BRK/HLD/V` 保持。 |
| 编译结果 | 首次链接因 Flash 超出 `36 B` 失败；压缩非关键 LCDM 文字后 Ubuntu 编译通过，LTO 版 `text=19276 B`、`data=1076 B`、`bss=1532 B`。 |
| HEX/ELF SHA256 | HEX `c05c67640c863f877537a02781cc2705f2bca24219e80a15b6e213e67cbda6a3`；ELF `53d4e56c09845734ab57fa501cc85156feea0aaa38e4418e3ad1c75027f1be12`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004649`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 继续记录同样三组动作的最大 `O`。如果 `O` 从 `170` 明显下降但停点变慢，下一步缩小 `brake_only_band` 或提高允许速度；如果仍 `O` 大，下一步提高反刹上限/增益或提前反刹；如果出现来回摆，下一步降低反刹增益并扩大纯 Brake 区。 |

2026-06-11 反刹增强小步下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “O最大是107了” |
| 现场现象/风险 | `O` 从 `170` 降到 `107`，说明上一版速度超限反刹/动态 Brake 方向有效。当前不应换思路，也不应改静止 HOLD；继续小幅增强运动段反刹，并保留 P50 的 `D/BRK/HLD/V` 观察。 |
| 我的判断 | 仍有 `107 count` 过冲，表示目标前动能还偏大。上一版纯 Brake 区最大 `260 count` 可能让中近距离过早进入无主动反刹；反刹上限 `95 duty` 对当前电机/齿轮惯性可能偏弱。 |
| 修改内容 | 仅调整运动刹车参数：反刹上限 `95 -> 120`，反刹基础 `24 -> 30`，反刹增益 `10 -> 12`，动态纯 Brake 区上限 `260 -> 240`，让中近距离速度超限时仍有一段主动反刹空间。静止 HOLD、锁力、ADC 滤波、PWM-ADC 映射不变。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19272 B`、`data=1076 B`、`bss=1532 B`。 |
| HEX/ELF SHA256 | HEX `0c4e24ca0205f0fb01681bc2d65f1bb4a4115e1676abf8a3c8e5659eac3ec128`；ELF `563c0ef195fdc691dcbd2b24db973e66db063c651f53fc7ff71078246c925668`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004645`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 再测最大 `O` 是否低于 `107`。同时看是否出现反向回摆：若 `O` 下降且不回摆，下一步可继续微增反刹或提前一点；若 `O` 下降但回摆，下一步降低反刹增益或扩大纯 Brake 区；若 `O` 不变，说明限制点不在反刹强度，需改刹车触发时机/速度采样周期。 |

2026-06-11 恢复反刹强度并加入正向速度阻尼下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “O有时大于142，也许这个方向还是不对” |
| 现场现象/风险 | 上一版把反刹上限和增益加大后，`O` 从最低 `107` 变成有时大于 `142`，说明单纯加大反向刹车会引入不稳定，可能和速度采样周期、机械惯性、反打时机有关。继续加大反刹不是正确方向。 |
| 我的判断 | 回退到 `O=107` 那版的反刹强度，并把控制重点前移：速度已经起来时先削减正向驱动，减少后段需要反打的动能。这更接近速度环，而不是末端硬刹。 |
| 修改内容 | 恢复反刹参数：反刹上限 `120 -> 95`，反刹基础 `30 -> 24`，反刹增益 `12 -> 10`，动态纯 Brake 区上限 `240 -> 260`。新增正向速度阻尼：在运动段正常正向驱动分支里，若 `vel_toward > 0`，按 `vel_toward * 10` 削减正向 drive，但最多只削到 `min_duty`，不额外增加反向冲击。静止 HOLD、锁力、ADC 滤波、PWM-ADC 映射不变。为放入 Flash，压缩 P00/P80/P90 和参数页非关键文字，P50 关键数据不变。 |
| 编译结果 | 首次链接因 Flash 超出 `72 B` 失败；压缩非关键 LCDM 文字后 Ubuntu 编译通过，LTO 版 `text=19272 B`、`data=1076 B`、`bss=1532 B`。 |
| HEX/ELF SHA256 | HEX `04290764bc16a47eb92a6c0b3d7b53c699c3b0d108995b7aa2d8995b00cd38a5`；ELF `e92d968ac69797f8dd3df4f3ee0ee8bdb7e1c848c1b5bd7b9330f14ced966ffe`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004685`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 观察最大 `O` 是否回到 `107` 附近或更低，以及是否更稳定。若 `O` 更稳定但动作偏慢，下一步降低正向阻尼；若 `O` 仍偶发大，下一步不再加大反刹，而是记录 P50 的到点前 `V`，改刹车触发时机或控制周期。 |

2026-06-11 速度误差阻尼下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “O最大84，但高端方向2000us没什么扭力。可能真的要重新搭建架构了” |
| 现场现象/风险 | `O` 从 `107` 降到 `84`，说明削减正向动能有效；但 `2000us` 高端方向扭力弱，说明上一版阻尼只要 `vel_toward > 0` 就削正向驱动，低负载时能降过冲，高端负载/摩擦大时会把推力压到最小 duty，造成方向力不足。 |
| 我的判断 | 需要把阻尼从“有速度就削力”改成真正速度环含义：低于目标速度时不削力，高于目标速度才削力或进入刹车。这样比完全重写架构风险小，但已经是向速度环架构过渡。后续若仍不够，应把运动段正式拆成 `ACCEL/RUN/BRAKE/HOLD` 状态机。 |
| 修改内容 | 引入 `desired_vel`，由 `allow_vel` 推导：`desired_vel = allow_vel - 2`，最小不低于 `allow_vel` 的低速值。正向驱动阻尼改为只对 `vel_toward - desired_vel` 这部分速度超差生效，阻尼增益 `10 -> 16`；若实际速度低于目标速度，不再削减 drive，以恢复高端 2000us 方向推力。反刹上限、反刹基础/增益、纯 Brake 区、静止 HOLD、锁力、ADC 滤波、PWM-ADC 映射不变。为放入 Flash，继续压缩少量状态文字如 `LOW/HIGH/NPWM`。 |
| 编译结果 | 中间版本先后因 Flash 超出 `4 B` 失败；简化 `desired_vel` 计算并压缩状态文字后 Ubuntu 编译通过，LTO 版 `text=19276 B`、`data=1076 B`、`bss=1532 B`。 |
| HEX/ELF SHA256 | HEX `d90be9ecdaafd1b4742d4f71c471f5307be65d0bfb8bd3360283d6d38a1e7be5`；ELF `07f715e55b983d193893006868ccde3f0fd6fc90e841328aacec253fde279f24`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004689`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看两项：最大 `O` 是否仍接近 `84`，以及 `2000us` 高端方向扭力是否恢复。若扭力恢复但 `O` 增大，下一步降低 `desired_vel` 或提前 Brake；若 `O` 仍低且扭力恢复，下一步把这套速度环参数整理成曲线/菜单参数。 |

2026-06-11 运动段速度/刹车架构重写下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请重新写吧，加油” |
| 现场现象/风险 | 用户已经明确指出不要继续钻在局部参数里。上一版 `O=116` 且高端扭力问题仍需要系统化处理，说明当前补丁式 `allow_vel/brake_only/damp` 不够清晰。 |
| 我的判断 | 先在现有固件空间内重写命令运动段，保留静止 HOLD、锁力、ADC 滤波、PWM-ADC 映射、LCDM P50 诊断不动。新运动段按三层关系执行：位置误差 `E` 决定基础 drive，ADC 速度 `V/vel_toward` 和目标速度决定削力，当前速度和剩余距离决定提前 Brake/比例反刹。 |
| 修改内容 | 删除上一版局部 `allow_vel/brake_only_band/drive_damp` 补丁。新增运动模型：`vpos=max(vel_toward,0)`；`target_vel=(abs_error>>7)+1`，最高 `10 count/loop`；`brake_dist=44 + vpos*18`，最高 `320 count`。若仍朝目标且进入 `brake_dist`，速度高于 `target_vel` 时按 `18 + (vpos-target_vel)*9` 比例反刹，最高 `95 duty`；速度未超则 `pd=0`，进入短 Brake。若未进入刹车距离，则按误差给正向 drive，只有 `vpos > target_vel` 时按速度超差削减 drive，最多削到运动最小 duty。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19232 B`、`data=1076 B`、`bss=1532 B`。 |
| HEX/ELF SHA256 | HEX `0f1c2c1b78a66ee38f51c4b0293d421f5b2d028f0231092753ba7bb4a75e3395`；ELF `1fa93dd91e49376142e076f60dc5195269b95afee91018538d0f81389f5e604c`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800465d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 先看三项：最大 `O`、`2000us` 高端方向扭力、到点是否来回摆。P50 重点观察接近目标时 HBR 是 `D+/-` 还是 `BRK`，以及 `V` 大小。如果 `O` 大但扭力足，下一步提高 `brake_dist` 或降低 `target_vel`；如果扭力弱，下一步提高 `target_vel` 或运动最小 duty；如果反打回摆，下一步降低比例反刹增益。 |

2026-06-11 重写版作废并恢复速度误差阻尼版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “O最大是549，会来回摆” |
| 现场现象/风险 | 运动段速度/刹车架构重写版出现 `O=549` 且来回摆，属于危险版本，可能冲击齿轮箱，不能继续保留在板上。 |
| 我的判断 | 这次重写的 `brake_dist=44+vpos*18` 和进入刹车距离后的比例反刹过激，导致接近目标时输出切换太猛，形成反打和大过冲。应立即撤回，不在这个版本上继续调。 |
| 修改内容 | 撤回重写运动分支，恢复到上一版“速度误差阻尼”结构：`allow_vel=(abs_error>>6)+1`、最高 `12`；`desired_vel=allow_vel-2`；动态纯 Brake 区 `72 + vel_toward*12`、最高 `260`；速度超限反刹 `24 + (vel_toward-allow_vel)*10`、最高 `95`；正向 drive 只对 `vel_toward-desired_vel` 超速部分做阻尼，增益 `16`。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19276 B`、`data=1076 B`、`bss=1532 B`。 |
| 下载过程 | 前两次 pyOCD 下载失败，报 `already open`，板上仍可能停留危险版本；随后在 Ubuntu 内对 DAPLink `0d28:0204` 执行 `usbreset`，第三次下载成功。 |
| HEX/ELF SHA256 | HEX `d90be9ecdaafd1b4742d4f71c471f5307be65d0bfb8bd3360283d6d38a1e7be5`；ELF `07f715e55b983d193893006868ccde3f0fd6fc90e841328aacec253fde279f24`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004689`，返回 `Successfully resumed device`。 |
| 下一步建议 | 不再做大块重写直接上板。下一步应先把新架构拆成可观察的分阶段状态和参数，例如先只加状态显示/日志，再一次只改变一个阶段：加速、速度限制、刹车、SETTLE。 |

2026-06-11 尾段多级降速下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “反刹应该提前太多了，以致于转得慢，具没有力。后一段尾端我想最多是1-2度左右。我还是之前的建议，应该分N多级的动态刹车主法，也就是说如果是45度，可能最后10度是这样子，到数第10度开始降速，倒数第9度又降一点点，一直到倒数1度就开始刹停了，也有可能最后1度又分成10级，因为没人会在乎最后一段的速度是否变慢了。请加油” |
| 我的判断 | 该判断成立。上一恢复版里 `brake_only_band=72+vel_toward*12`，速度一高就可能把 Brake 区提前到 `260 count`，相当于十几度前就开始刹，容易导致中段慢和高端无力。应取消“速度越高越早大范围刹车”的逻辑，改为固定尾段多级降速。 |
| 修改内容 | 将命令运动段改为尾段分级：`DECEL_START=220 count`，约等于最后 10-12 度；`FINE_BRAKE_BAND=24 count`，约等于最后 1 度多。只有 `abs_error <= 220` 才进入降速/反刹判断；尾段目标速度按 `(abs_error+19)/20` 分级，约每 20 count 一级，越接近目标允许速度越低；`abs_error <= 24` 且仍朝目标运动时只 Brake。反刹强度降低为 `18 + speed_error*6`，最高仍 `95`，避免过猛反打。远离目标时不再因速度高提前反刹，保持正常推力。 |
| 编译结果 | Ubuntu 编译通过，LTO 版 `text=19236 B`、`data=1076 B`、`bss=1532 B`。 |
| HEX/ELF SHA256 | HEX `adec34d860d350884ec64798789826e84817937fb73254ac461bbaab33e08424`；ELF `10175a5dbbe3684e1ff3ed3660f1ff273686878e2b108846971d4a0206f8280e`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004661`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看三项：中段是否恢复扭力、最大 `O` 是否低于恢复版、到点是否还来回摆。若中段有力但 `O` 大，下一步只扩大 `DECEL_START` 或降低尾段目标速度；若中段仍慢，下一步缩小 `DECEL_START`；若来回摆，下一步继续降低反刹增益或让最后 24 count 只 Brake 不反刹。 |

2026-06-11 P50-only 与新运动架构下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “你的架构没有更新吗，要重新写一个架构呀，按我给的思路来试试，不然天天拍忙活了。” / “先把P50以外的其它屏蔽掉，要用时再打开” |
| 现场现象/风险 | 前两次大改运动段出现 `O=549`、`O=495` 这类危险过冲，说明不能继续靠局部参数硬改。P50 以外的 LCDM 页面当前对舵机运动调试没有帮助，还占用 Flash 和 LCDM 通讯时间。 |
| 我的判断 | 先把开发固件收敛为 P50 实时诊断版本，释放 Flash 空间并减少界面干扰；运动控制按用户提出的思路改为“本次目标距离 + ADC 速度 + 尾段多级降速 + 越界硬刹”的结构。远段先给足推力，不再过早反刹；接近目标后按剩余 ADC count 分级限速；越过目标点时禁止继续同方向驱动。 |
| 修改内容 | 1. LCDM runtime UI 启动后直接绘制 P50，不再进入 P00/P01/P10/P13/P20/P30/P40/P60/P70/P80/P90；触摸包仍读取清空，但不触发页面切换。2. 命令目标改变时，`move_dir` 改为按当前反馈 ADC 到新目标 ADC 的实际方向锁定，不再按新旧目标大小判断。3. 运动段改为新分段：`motion_error` 锁存本次行程，`decel_start=motion_error/5` 并限制在 `120..360 count`；尾段每 `20 count` 作为一级目标速度；`abs_error<=32 count` 且仍有朝目标速度时只 Brake；越过目标后只 Brake，不再继续原方向推。4. 反刹限幅降到 `75 duty`，远段保留正常推力，避免早反刹造成慢和无力。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15228 B`、`data=168 B`、`bss=1528 B`，总 `16924 B`。相比接近满 Flash 的多页面版本释放了约 4 KB 以上空间。 |
| HEX/ELF SHA256 | HEX `3f72e3a5fab74ee79675a4ee014a7c9e7c134409b2140389c8a363fc3ee7fc20`；ELF `d48f875d0d4c5337aad6cfe6bc5adc51ef710acb668a37be70be9b3afdbdf475`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080038b5`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 只在 P50 看数据：先小幅测试 1500us 附近，再测试 1000/1500/2000。重点记录最大 `O`、到点前 `V`、HBR 是否在尾段出现 `BRK`、是否仍会跑到机械极限。若 `O` 仍大但中段有力，下一步只调整 `DECEL_DIV/DECEL_MAX/TAIL_STEP`；若动作慢，缩短 `decel_start` 或提高尾段目标速度；若来回摆，继续降低反刹或扩大纯 Brake 尾段。 |

2026-06-13 级联位置/速度环架构重写下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “电流环，可能真的只能采用PWM DUTY近似电流算法……无论如何，请按我们查看的最新做来重新搭建架构。请执行重写，谢谢！” |
| 现场现象/风险 | 前面一直用位置误差直接算 H 桥 duty，再叠加手工分段刹车/反刹，已经证明容易出现过冲、摆动、锁力和速度互相牵制。当前硬件没有马达电流采样，所以暂时无法做真实电流环，只能用 duty 近似力矩。 |
| 我的判断 | 采用更接近成熟数字舵机/伺服的分层架构：PWM 输入只生成目标位置；轨迹规划器限制目标位置移动速度和加速度；位置环生成速度参考；速度环用 ADC 估算速度闭环输出 H桥 duty。这样不同电压造成的马达速度变化会先反映到 ADC 速度，再由速度环自动减/加 duty，而不是直接依赖供电电压。 |
| 修改内容 | 1. `lcdm_servo_loop_update_fast()` 核心重写，移除旧的尾段手工刹车/反刹分支。2. 新增轻量 alpha-beta 估算：用 raw ADC 估算 `pos_q4` 和 `vel_q4`，P50 的 `C` 显示控制位置，`V` 显示实际 ADC 速度。3. 新增轨迹规划：`TRAJ_MAX_VEL_Q4=288`，约 18 count/ms；`TRAJ_ACCEL_Q4=24`，约 1.5 count/ms^2；按剩余距离和当前轨迹速度计算刹车距离。4. 新增级联环：`位置误差 -> v_ref`，`v_ref - vel -> duty`，并加 `MOVING_FF_DUTY=80` 克服静摩擦；输出每 1ms 限斜率 `80 duty`。5. HOLD 条件改为目标到达、位置误差小于 `10 count`、速度低于 `24 q4` 时只 Brake；外力拉动超过该范围后重新由位置/速度环给反向 duty 锁住。6. P50 第 5 行增加 `R`，表示速度环目标速度；`V` 是 ADC 实际速度，便于看速度环是否跟得上。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15288 B`、`data=164 B`、`bss=1540 B`，总 `16992 B`。 |
| HEX/ELF SHA256 | HEX `effa6276103ac692da65be6bc64d3dd79f6bd0a372283d5294e3fc91ea051630`；ELF `928ad47e6920cd7bd8592ca346cdf64369533fd7c6627b807c640aeb2e5f8444`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080038e5`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 先只做小幅 PWM 改变，观察 P50 第 5 行 `V/R`：如果 `R` 很高但 `V` 跟不上，说明动力/速度环增益不足；如果 `V` 超过 `R` 后仍过冲，说明速度环增益或轨迹刹车距离不足；如果 `R` 本身变化太猛，调轨迹速度/加速度；如果 HOLD 后外力拨动不够锁，调位置环或前馈 duty。 |

2026-06-13 末端抖动和 R=-1 修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前看到标的地后会抖动几下后停止，另外看R有时会-1，是否跟不上” |
| 现场现象/风险 | 到目标点后还抖几下，说明级联架构已能停住，但末端还在追很小的位置/速度误差。P50 上 `R=-1` 表示速度环目标速度为轻微反向，不是 PWM 输入跟不上；通常是控制器认为已经过了一点点或还需要小反向速度来消除残余滑动。 |
| 我的判断 | 主要风险在输出限斜率：上一版把同方向减力和反向刹车也按 `80 duty/ms` 慢慢变化，接近目标时会多推几下，导致抖动。末端 HOLD 捕获也偏窄，会继续追 1 count 级别的小误差。 |
| 修改内容 | 1. HOLD 捕获从 `10 count / 24 q4` 放宽到 `14 count / 36 q4`，到位且速度已低时直接 Brake/HOLD，不继续追 `R=-1` 的微小修正。2. 输出限斜率改为只限制同方向加大 duty；同方向减力、过零、反向刹车立即执行，让速度环末端刹车更快。控制架构、轨迹规划速度/加速度、速度环增益、P50-only 界面保持不变。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15392 B`、`data=164 B`、`bss=1540 B`，总 `17096 B`。 |
| HEX/ELF SHA256 | HEX `91634908e9fd57274e1483178d36509460cbf5d311bdfb856c443a2042fb1a53`；ELF `b0ebd15996b88e7c5361b3a46ee2f1ceb1ac25ba98e54bfb4c541068c9083bca`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800394d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 观察到点后是否还会抖几下。重点看 `V/R`：若 `R=-1` 但已经直接 HLD，说明末端捕获有效；若仍 D/BRK 来回跳，继续扩大 HOLD 或降低位置环；若过冲变大，说明减力/反刹太快，需要给反向刹车单独限幅而不是恢复旧的双向慢斜率。 |

2026-06-13 HOLD 迟滞和运动力恢复下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “现在P50看到是V和R暂时为0，抖动小改善，时间一长还会嗞嗞声，时不时会出现R-1，整机速度变慢了，感觉不够力。” |
| 现场现象/风险 | 上一版末端抖动有改善，但长时间静止后仍会重新嗞嗞；P50 偶发 `R-1`，说明 HOLD 后速度环仍被极小的位置/速度估算误差唤醒；同时运动轨迹和前馈偏保守，用户感觉速度慢、力不足。 |
| 我的判断 | 这不是 PWM 输入跟不上，而是 HOLD 状态缺少退出迟滞。进入 HOLD 后如果 PWM 目标没有变化，小 ADC 漂移不应让轨迹/速度环重新输出；只有 ADC 偏离超过较宽范围并持续数毫秒，才退出 HOLD 重新反向锁力。运动力不足则单独通过轨迹速度、加速度和运动前馈恢复，不再牺牲静止安静度。 |
| 修改内容 | 1. `TRAJ_MAX_VEL_Q4` 从 `288` 提到 `368`，`TRAJ_ACCEL_Q4` 从 `24` 提到 `32`。2. `MOVING_FF_DUTY` 从 `80` 提到 `120`，提高启动/运动段推力。3. 新增 HOLD 锁存退出迟滞：PWM 目标未变且误差在 `34 ADC count` 内时强制 `HLD/Brake`，把轨迹位置固定到目标点、轨迹速度和参考速度清零，同时清零估算速度，避免长时间后由 `R=-1` 微小反向速度重新驱动 H 桥。4. 误差超过 `34 count` 后需连续 `3 ms` 才退出 HOLD，减少 ADC 偶发跳动误触发；真正外力拨开后仍会回到速度环反向锁力。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15472 B`、`data=164 B`、`bss=1548 B`，总 `17184 B`。 |
| HEX/ELF SHA256 | HEX `94a450794def547ad06b827f596a797b18bec4c14972caab84c57a68f1efe296`；ELF `e33e0afdfd484fc96ae840d4b9b53704ee731349018a3d0cf5daaf3e28f4f123`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800399d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | P50 静止时应长期保持 `HLD V+0 R+0`，偶发 `R-1` 应明显减少或不再驱动 H 桥；运动速度和扭力应比上一版恢复。若仍会长时间后嗞嗞，下一步看当时 `E/W/V/R/HBR`，优先扩大 HOLD exit band 或检查 PWM 锁存是否有细小跳变；若速度恢复但过冲变大，再只调轨迹加速度/速度或速度环增益。 |

2026-06-13 末端调速和到点 Brake 下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “是HLD，速度有提上一点，但还是到目标点会抖几下” |
| 现场现象/风险 | HOLD 锁存已生效，说明静止状态能保持；剩余问题发生在进入目标点前后，属于末端速度/前馈/刹车处理问题。若到点附近仍用运动前馈 duty 硬推，会过目标点后再反向修正，表现为抖几下才 HLD。 |
| 我的判断 | 不应整体降低速度，否则刚恢复的中段速度和力又会变差。应只在轨迹已到目标且实际 ADC 接近目标时生效：限制末端 `R`、减少或取消前馈，若已经很近且仍朝目标运动，直接 `Brake`，避免 `D+ / D-` 来回反打。 |
| 修改内容 | 1. 新增末端窗口：`TERMINAL_BAND=72 ADC count`、`TERMINAL_FINE_BAND=28 ADC count`。2. 在 `72 count` 内把速度参考 `R` 限制到 `96 q4`，运动前馈降为 `MOVING_FF_DUTY/3`。3. 在 `28 count` 内把 `R` 限制到 `32 q4`，完全取消运动前馈。4. 在 `28 count` 内如果 ADC 估算速度仍朝目标方向冲，直接 `HBridge_Brake()`，让 HBR 短暂显示 `BRK` 后进入 `HLD`，减少过点后反打。远段速度、加速度和前馈保持上一版，不影响中段速度/力。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15564 B`、`data=164 B`、`bss=1548 B`，总 `17276 B`。 |
| HEX/ELF SHA256 | HEX `6713fc98e4e35472a0330085bbab023386a1bdd946988a29d1a6c40f308b0ca2`；ELF `363a51ccf6ba1b9277e065d618ec92c650673526966835467bde22ecf40dd9c5`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080039f9`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 到点前 P50 第 5 行应看到 `R` 在末端变小，可能短暂 `BRK`，随后 `HLD V+0 R+0`。重点看抖动次数是否减少、最大 `O` 是否下降、速度是否仍保持上一版提升。如果还是抖几下，下一步只扩大 `TERMINAL_BAND` 或降低末端 `VREF`；如果变慢但不抖，再缩小末端窗口。 |

2026-06-13 末端小 duty 屏蔽下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “除了1500US会稳定一点，1000和2000US都会出现D-20多或D+20多，抖动没改良” |
| 现场现象/风险 | 1500us 较稳定，说明 HOLD/静止逻辑在中位有效；1000/2000 两端出现 `D±20`，说明末端不是大过冲，而是控制器持续输出几十 duty 的小修正。这个 duty 低于有效驱动能力，但会让 H 桥和马达轻微动作，造成抖动和声音。 |
| 我的判断 | 需要增加“末端小 duty 无效输出屏蔽”。不能做全局死区，否则会损害 1-3us 灵敏度；只能在 PWM 目标未变化、轨迹已到目标、实际 ADC 在末端范围内时，把小 duty 视为已稳定，直接 Brake/HLD。 |
| 修改内容 | 新增 `TERMINAL_SETTLE_BAND=96 ADC count` 和 `TERMINAL_SETTLE_DUTY=80`。在轨迹已到目标、轨迹速度为 0、PWM 目标未变化、误差小于 `96 count` 时，如果最终计算输出 `|duty| <= 80`，不再执行 `D+/D-`，而是清零估算速度/参考速度/当前 duty，设置 `HLD` 并 `HBridge_Brake()`。中段速度、前馈和末端限速保持上一版。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15672 B`、`data=164 B`、`bss=1548 B`，总 `17384 B`。 |
| HEX/ELF SHA256 | HEX `bad67cfa51a70edc913621c2f9529830edffd4ecc294220db17f2698a55890be`；ELF `70637eb37259c8dee969de3c7e349c31ccdaebb3164429147403463837088163`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003a65`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看 1000/2000 到点后是否还停在 `D±20`。预期应转为 `HLD V+0 R+0`。如果仍有 `D±20`，说明当时 `E` 超过 `96 count` 或 PWM 目标仍有跳变，需要记录 P50 的 `E/O/W`；如果变成 HLD 但位置偏差太大，再缩小 `SETTLE_BAND` 或修正两端目标映射。 |

2026-06-13 稳定指令低 duty 静默下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “在抖动时，D基本上是+23多，0时基本上没抖动” |
| 现场现象/风险 | 用户明确确认抖动和 `D+23` 这类小 duty 输出直接相关；`D=0` 时基本不抖。上一版仍会出现，说明要求“轨迹已完全到目标”的屏蔽条件过严，轨迹末端尚未完全归零或有轻微目标/估算变化时仍允许小 duty 输出。 |
| 我的判断 | 应把屏蔽逻辑改成“稳定指令低 duty 静默”：最近没有确认到新的 PWM 命令时，若误差处于末端范围且 duty 很小，就不应继续输出 H 桥 PWM。为保留 1-3us 灵敏度，刚确认 PWM 改变后的 `cmd_sensitive` 计数期间不屏蔽。 |
| 修改内容 | 1. `TERMINAL_SETTLE_BAND` 从 `96` 放宽到 `160 ADC count`。2. 末端小 duty 屏蔽不再要求 `traj_pos == cmd_q4` 和 `traj_vel == 0`，改为要求 `s_lcdm_servo_cmd_sensitive == 0` 且 `abs_error <= 160`。3. 当 `|duty| <= 80` 时直接清零估算速度/参考速度/当前 duty，设置 `HLD` 并 `HBridge_Brake()`。这样专门消除稳定指令下的 `D±20` 小功率抖动，不影响刚发生的 PWM 微小指令动作。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15628 B`、`data=164 B`、`bss=1548 B`，总 `17340 B`。 |
| HEX/ELF SHA256 | HEX `4d487629631aa6de0fdd3b98fb475d9d7255d7a93803420aa8ef3357f93b169a`；ELF `2dc836e83a05e5112cdc11d7e1d638eb053e7c5cd7eb9b3f5e143b02e59372e7`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003a39`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看抖动时是否还会显示 `D+23`。预期稳定后应直接显示 `HLD V+0 R+0`。如果 D 被压住但位置明显不到位，下一步把低 duty 改为“脉冲式有效最小 duty”而不是完全静默；如果仍显示 D+23，说明 `cmd_sensitive` 一直未归零，需要检查 PWM 锁存/窗口 `W` 是否持续跳变。 |

2026-06-13 低 duty 静默版作废并回退记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “虽然D有改善，但抖更严重了，比上一版差，上一版相对好些” |
| 现场现象/风险 | “稳定指令低 duty 静默”虽然减少了 `D` 显示，但实际抖动更严重，说明过早把输出切到 HLD/Brake 会让两端位置/速度环断续重新捕获，反而形成更强机械抖动。这个方向不能继续叠加参数。 |
| 我的判断 | 上一版相对好，必须先退回。后续不能简单扩大静默窗口；应改为更连续的末端控制，例如小 duty 区改成短脉冲/最小有效 duty、或从速度估算/目标映射查两端为什么需要持续 `D+23`。 |
| 修改内容 | 撤销 2026-06-13 “稳定指令低 duty 静默”改动：`TERMINAL_SETTLE_BAND` 从 `160` 恢复为 `96`；末端屏蔽条件恢复为 `target_delta <= CMD_HOLD_EXIT_EXTRA`、`traj_pos == cmd_q4`、`traj_vel == 0`、`abs_error <= 96`，即只在轨迹确实到目标后屏蔽小 duty。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15672 B`、`data=164 B`、`bss=1548 B`，总 `17384 B`。 |
| HEX/ELF SHA256 | HEX `bad67cfa51a70edc913621c2f9529830edffd4ecc294220db17f2698a55890be`；ELF `70637eb37259c8dee969de3c7e349c31ccdaebb3164429147403463837088163`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003a65`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 板上已回到用户反馈“上一版相对好些”的版本。下一步不要再扩大静默窗口；需要记录 1000/2000 抖动时的 `E/O/W/V/R/HBR`，重点判断是两端目标映射偏差、速度估算误差，还是小 duty 无法克服静摩擦导致的断续控制。 |

2026-06-13 低 duty Brake 非 HLD 下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “继续改进” |
| 现场现象/风险 | 上一轮证明扩大静默窗口并直接进入 `HLD` 会更差，原因是误差还没真正进入 HOLD 范围时过早锁存，下一轮又退出 HOLD，形成断续控制。用户也已经确认 `D+23` 会抖、`D=0` 基本不抖。 |
| 我的判断 | 不能再扩大 HLD 窗口，但可以把极小 duty 输出转成普通 `BRK`，不设置 `hold_active`、不改轨迹位置、不清零估算速度。这样只切断 H 桥小 PWM，不让控制器误以为已经完全到位，避免上一版 HLD 进出循环。 |
| 修改内容 | 新增 `TERMINAL_LOW_DUTY_BRAKE=45`。当最近没有新 PWM 命令、误差在 `TERMINAL_SETTLE_BAND=96` 内、最终输出 `0 < |D| <= 45` 时，直接 `HBridge_Brake()`，状态显示 `BRK`，`current_duty=0`、`R=0`，但不设置 `HLD`、不改 `traj_pos/traj_vel`、不清零速度估算。原来的严格 HLD 小 duty 屏蔽保留不变。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15688 B`、`data=164 B`、`bss=1548 B`，总 `17400 B`。 |
| HEX/ELF SHA256 | HEX `6605477f5d998decc88a2b3ad5f8fed6474eb466afc90475c3905e01e99b858d`；ELF `eaa8c485e064eef60617541e95af578be311af3a6da1933ad3f0708b523e3d4a`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003a75`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看原来 `D+23` 抖动时现在是否变成 `BRK`，以及机械抖动是否比回退版减少。如果 `BRK` 仍抖，说明 Brake 本身锁桥也在激发机械抖动，下一步要改成 `Coast` 或短 Brake 脉冲；如果显示仍是 `D+23`，说明当时误差超过 `96` 或 `cmd_sensitive` 未归零，需要记录 `E/W/R/V`。 |

2026-06-13 低 duty Brake 覆盖范围调整下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “D会-20-50几之间跳” |
| 现场现象/风险 | 上一版只屏蔽 `|D| <= 45`，用户看到 `D` 在 `-20` 到 `-50` 多之间跳，说明 `50` 附近的小输出仍会漏到 H 桥；也可能误差略超过 `96 count` 时完全不触发低 duty Brake。 |
| 我的判断 | 继续保持“非 HLD、只 BRK”的方向，不再扩大 HLD。把低 duty Brake 单独扩到 `160 count` 范围、`|D| <= 70`，覆盖用户看到的 `-20..-50` 跳动，但严格 HLD 仍保持 `96 count`，避免再次过早 HLD。 |
| 修改内容 | 新增 `TERMINAL_LOW_DUTY_BRAKE_BAND=160`，`TERMINAL_LOW_DUTY_BRAKE` 从 `45` 提高到 `70`。低 duty Brake 条件改为 `cmd_sensitive==0 && abs_error<=160 && 0<|D|<=70`，仍只执行 `HBridge_Brake()`、状态 `BRK`，不设置 `HLD`、不改轨迹、不清零速度估算。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15688 B`、`data=164 B`、`bss=1548 B`，总 `17400 B`。 |
| HEX/ELF SHA256 | HEX `b85ddc68af54b683fcf4e977ca1bc05eacd539389df4cb0ec1260b39b595a042`；ELF `0e0d2b163b71911d9659007e1d7a3242605407a84f9deb729c4d7bc77bbaa1dc`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003a75`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看 `D=-20..-50` 是否消失并变成 `BRK`，以及抖动是否减小。如果仍抖但 HBR 显示 `BRK`，说明 Brake 锁桥本身在激发抖动，下一步试低 duty 区 `Coast` 或短 Brake 脉冲；如果仍显示 `D`，记录当时 `E/W/R/V`，判断是否超出 `160 count` 或 PWM 持续跳变。 |

2026-06-13 LCDM 显示阻塞控制循环修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “变+了，同时发现一个问题，通常由2000-1000US会稳定点，而1000-2000US会多点机会抖，而从1000-1500正反或1500-2000正反都抖，是否算法不够快，还是什么原因？处理不及时？关键是目前也转的不够快啊” |
| 现场现象/风险 | 不同方向和小行程切换的抖动概率不同，同时速度不够快，符合闭环控制节拍被显示通讯打断的风险。P50 每 150ms 更新，`xstr/fill` 经 UART 阻塞发送，每条命令后还有 2ms gap，原代码在这些时间内没有执行 1ms 马达控制。 |
| 我的判断 | 这不是单纯 PID 参数问题。LCDM 是开发显示，但它在调试时和马达控制跑在同一主循环；显示刷新可能造成数毫秒到几十毫秒控制空窗，导致速度估算、刹车和小行程方向切换处理滞后。需要先保证显示通讯期间闭环仍按 1ms 执行。 |
| 修改内容 | 1. 新增全局 `lcdm_runtime_service_motor_control()`，用 `HAL_GetTick()` 维护 1ms 马达控制节拍。2. `pa7_tjc_write_cmd()` 改为 16 字节分段 UART 发送，每段后执行一次控制服务。3. LCDM 命令 gap 延时、LED 延时、清屏延时改为带控制服务的等待。4. 主循环改用全局控制服务，不再使用局部 `last_pwm_motor_ms`。5. 保留上一版 `low duty Brake` 参数不变，本次主要消除 LCDM 阻塞。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=15968 B`、`data=164 B`、`bss=1556 B`，总 `17688 B`。 |
| HEX/ELF SHA256 | HEX `aa90f981a3081075bf174c01f389a6f4937eba51446bdb3c8f6a15d8f966b4d0`；ELF `940708f51fc2bab5a2c41142ee03c7d57ace59c406e9eccabcdecae42200cb1e`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003b8d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看小行程 `1000-1500`、`1500-2000` 正反切换是否减少抖动，以及整机速度是否变快。若速度仍不够，下一步再提高轨迹速度/加速度和速度环前馈；若抖动明显改善，说明 LCDM 阻塞确实是主要干扰源之一。 |

2026-06-13 全局过点捕获和小幅提速下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不建议这样搞一个正么方向独立补偿和刹车参数，把不好的再做好，把好的做到更好，就可以了，继续” |
| 现场现象/风险 | 用户确认不希望做正反方向独立补偿。当前现象是大多会过冲一次回来再停，2000 相对好一些。继续做方向独立参数会增加调试复杂度，也容易掩盖机械/ADC 非线性。 |
| 我的判断 | 采用全局规则改良：过目标后如果仍带速度远离目标，先 Brake 吸收惯性，不立刻反向驱动拉回来；同时小幅提高轨迹速度和加速度，让整体动作更快。这样 1000/1500/2000 使用同一套规则。 |
| 修改内容 | 1. `TRAJ_MAX_VEL_Q4` 从 `368` 提到 `416`。2. `TRAJ_ACCEL_Q4` 从 `32` 提到 `36`。3. 新增 `OVERSHOOT_BRAKE_BAND=120`、`OVERSHOOT_BRAKE_VEL_Q4=28`。4. 当 PWM 目标未变、误差在 `120 ADC count` 内、且位置已经在目标另一侧并继续远离目标时，直接 `HBridge_Brake()`，状态 `BRK`，不马上反向驱动。5. 不引入正反方向独立参数，保留前一版 LCDM 显示阻塞修正和 low duty Brake。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16028 B`、`data=164 B`、`bss=1556 B`，总 `17748 B`。 |
| HEX/ELF SHA256 | HEX `9c7827567cd8135c0fb8f0ffa0ca7e9d3d3f1fa74436a80c9aaf29549fe239c3`；ELF `2cad9640ed40b3f54629daf70692af252b2c798ecb929894677a36bd92eeda42`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003bc9`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 重点看“过冲一次回来”是否变成接近目标后短暂 `BRK` 然后停住，以及整体速度是否有提升。如果过点后停得更稳，下一步可继续小幅提速；如果出现停在目标外侧不回来的情况，需要缩小 `OVERSHOOT_BRAKE_BAND` 或提高退出条件。 |

2026-06-13 舵机曲线调试变量下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我看这样子好了，你能否帮我搜集相关的数据，做一个曲线图出来让我可以看到所有以数在运动时的变化规律，同时在调试时也可以供参考。我先发一个别人在调试时用的二张图，帮忙分析是什么内容构建的。其中他回复了一句：这个曲线是角度，几乎没有过冲。” |
| 参考图判断 | 第一张更像角度/位置阶跃曲线，重点是位置变化后快速贴近新角度且几乎不反弹。第二张更像把角度/目标与控制量叠加观察：蓝线可能是角度或目标位置，灰色尖峰可能是驱动 duty、速度、误差或电流近似量；稳定段灰色接近 0，说明到位后 H 桥不再持续正反输出。 |
| 我的判断 | 仅看 LCDM 的瞬时 `O/D/W/R` 不够，必须把目标、反馈、轨迹、速度、输出和事件同步画出来。这样才能判断过冲来自目标映射、轨迹太快、速度估算滞后、刹车太晚，还是末端 duty 反复输出。 |
| 修改内容 | 在 `src/app_entry.c` 增加全局 `volatile` 曲线变量：`g_servo_curve_time_ms`、`g_servo_curve_pwm_us`、`g_servo_curve_raw_adc`、`g_servo_curve_target_adc`、`g_servo_curve_feedback_adc`、`g_servo_curve_error_adc`、`g_servo_curve_duty`、`g_servo_curve_velocity_q4`、`g_servo_curve_ref_velocity`、`g_servo_curve_traj_adc`、`g_servo_curve_traj_vel_q4`、`g_servo_curve_overshoot_adc`、`g_servo_curve_state`、`g_servo_curve_event`。新增 `docs/servo_curve_debug_variables.md` 说明 VarViewer 应添加哪些变量和事件编号；新增 `tools/plot_servo_curve_csv.py`，用于把 VarViewer/调试器导出的 CSV 画成双面板 SVG 曲线图。 |
| 事件编号 | `1=DRIVE`，`2=HOLD`，`3=duty=0 Brake`，`4=过点后继续远离目标 Brake`，`5=末端仍朝目标冲 Brake`，`6=低 duty Brake`，`7=严格末端 HLD 屏蔽`，`8=方向报警/保护`。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16192 B`、`data=164 B`、`bss=1588 B`，总 `17944 B`。 |
| HEX/ELF SHA256 | HEX `37bb3eb2253f7503e1e66c0c2188c3fc452970937ccf6bd6f4b86a0248a8d0ab`；ELF `50d8e79a4a69f93c3d8b5033f0d58162900e16ae349ab5c2dc9bf5dfad3d7339`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003c6d`，返回 `Successfully resumed device`。 |
| 下一步验证点 | 用 VarViewer 先抓 `1000->1500`、`1500->1000`、`1500->2000`、`2000->1500`、`1000->2000`、`2000->1000`。位置图看 `target_adc/feedback_adc/traj_adc/overshoot_adc`；控制图看 `error_adc/velocity_q4/ref_velocity/duty/event`。重点看过目标瞬间 `event` 是否进入 4/5、`duty` 是否反向过猛、`velocity_q4` 是否滞后或符号异常。 |

2026-06-13 SWD 实时曲线网页记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “输出在哪里，怎样看，是否可以我测完一个周期就会弹出来，或者我一边测一边更新出来电脑上显示” / “对的，你直接用SWD回访所有测试数据，在MAC里面直接HML网页方式显示出来即可。因为下载完后，SWD不用，可以转回读数据用。” |
| 方案 | 固件仍只把曲线数据放在 `g_servo_curve_*` RAM 变量里。下载完成后，SWD 不再用于烧录时，可以由电脑通过 DAPLink/PADLink 持续读这些 RAM 地址；Mac 端生成 `HTML/SVG/CSV`，一边测试一边刷新。 |
| 实现内容 | 新增 `tools/live_servo_curve_swd.py`：通过 pyOCD/SWD 读取变量，支持本机生成网页或输出 CSV 流。新增 `tools/live_servo_curve_on_mac.py` 和 `tools/live_servo_curve_on_mac.sh`：Mac 本地打开网页，Ubuntu 负责 pyOCD 读 SWD，Mac 负责实时生成 `build/servo_curve_live/servo_curve_live.html`、`servo_curve_live.svg`、`servo_curve_live.csv`。 |
| 现场处理 | Ubuntu VM 起初未启动，启动后发现 DAPLink 在 macOS 侧；macOS 能看到 DAPLink，但 pyOCD/hidapi 因系统设备权限无法拿到 probe。通过 `prlctl set 'Ubuntu Linux' --device-connect 'DAPLink CMSIS-DAP'` 把 DAPLink 接给 Ubuntu，Ubuntu `lsusb` 已看到 `0d28:0204 NXP ARM mbed`。 |
| 验证结果 | 真实 SWD 短采样成功，生成 `/tmp/servo_curve_real_test/servo_curve_live.html`、`.svg`、`.csv`，CSV 有 48 行实测数据。随后启动正式后台监视，PID `91418`，输出目录 `build/servo_curve_live/`，3 秒后 CSV 已有 15 行实测数据并继续增加。 |
| 使用方法 | 运行 `./tools/live_servo_curve_on_mac.sh --interval-ms 10 --window 600 --output-dir build/servo_curve_live`。打开 `build/servo_curve_live/servo_curve_live.html` 即可看实时曲线；网页每秒自动刷新。停止后台监视可执行 `kill $(cat build/servo_curve_live/live.pid)`。 |
| 自动周期文件 | 当检测到 `target_adc` 变化，并且动作后回到 `event=2 HOLD` 且 `duty=0` 持续一段时间，会自动在 `build/servo_curve_live/cycles/` 保存单次动作的 `.csv` 和 `.svg`。如果只是保持同一个 PWM 不动，则只更新实时总曲线，不会生成周期文件。 |

2026-06-13 SWD 实时网页不更新修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “HMTL无法更新哦” |
| 现场现象 | 最初的后台 `nohup` 进程会被当前命令会话清理，CSV/HTML/SVG 停在同一时间；改为 launchd 后又遇到 `prlctl` PATH 不完整、pyOCD backend 和远端 `pkill` 自杀等问题。 |
| 根因 | 1. 普通 shell 后台进程不适合作为长期实时采样服务。2. launchd 默认 PATH 找不到 `/usr/local/bin/prlctl`。3. pyOCD `session_with_chosen_probe()` 默认 `auto_open=True`，再手动 `session.open()` 会触发 `already open`。4. 远端 `pkill` 和采样命令放在同一个 shell 时，`pkill` 会匹配并杀掉自身 shell。5. 浏览器本地文件缓存需要 JS 强制刷新 SVG。 |
| 修改内容 | 1. 新增 `tools/start_servo_curve_live_macos.sh` 和 `tools/stop_servo_curve_live_macos.sh`，用 macOS launchd 常驻运行实时采样。2. launchd 使用 `/opt/homebrew/bin/python3`，Python 内部固定 `/usr/local/bin/prlctl`。3. `live_servo_curve_swd.py` 设置 `auto_open=False`，避免 pyOCD 双 open；默认 backend 固定为 `hidapiusb`。4. Mac 端每次重连前单独执行 Ubuntu 侧清理和 DAPLink reset，不再把 `pkill` 和采样命令写在同一条远端 shell。5. HTML 加入 JS，每秒给 SVG 加时间戳重新加载。 |
| 验证结果 | 重新用 Parallels 断开/连接 DAPLink 后，手动 5 秒 SWD 采样成功输出实时数据。随后用 `./tools/start_servo_curve_live_macos.sh` 启动常驻网页：12 秒检查 CSV `65` 行，24 秒检查 CSV `164` 行，`servo_curve_live.html`、`servo_curve_live.svg`、`servo_curve_live.csv` 修改时间持续更新；launchd 状态 `running`，PID `3249`。 |
| 当前使用方法 | 打开 `build/servo_curve_live/servo_curve_live.html`。若页面仍停住，先看页面右上/上方 `View refreshed` 时间是否变化；若时间变化但曲线不变，说明 PWM/ADC数据本身暂时没变化。停止服务：`./tools/stop_servo_curve_live_macos.sh`。重新启动：`./tools/start_servo_curve_live_macos.sh`。 |

2026-06-13 基于 SWD 曲线数据的过冲返回修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前可以看到所有返馈的数据图形，很明显的有过冲和返回动作，请直接读取数据后，再进行自行修正，能做到吗？” |
| 数据结论 | 已读取 `build/servo_curve_live/servo_curve_live.csv` 和 `cycles/*.csv`。典型动作：`1007us -> 1508us` 时目标约 `2171 ADC`，反馈冲到 `2276 ADC`，过冲约 `105 count`；此时误差 `-117`，程序不是先刹车，而是输出 `D=-639`，事件仍是 `DRIVE(1)`，这会造成明显的“过冲后反向拉回”。批量统计里多组动作过冲约 `90..160 count`，并伴随 `|duty|=500..1000` 的返回动作。 |
| 我的判断 | 当前过冲保护太依赖速度符号/速度阈值，实际过目标时速度估算可能只有个位数到几十，导致 `event=4` 不触发。真正需要先压住的是“已经过冲后的返回修正 duty”，不能一过点就大力反向驱动。 |
| 修改内容 | 1. 末端减速区扩大：`TERMINAL_BAND 72 -> 120`，`TERMINAL_FINE_BAND 28 -> 48`。2. 末端速度参考降低：`TERMINAL_VREF_Q4 96 -> 72`，`TERMINAL_FINE_VREF_Q4 32 -> 24`。3. 过冲刹车区扩大：`OVERSHOOT_BRAKE_BAND 120 -> 180`，速度阈值 `28 -> 16`。4. 新增 `OVERSHOOT_RETURN_DUTY=180`：本次动作已经记录到过冲且误差在过冲窗口内时，返回目标的 DRIVE duty 限制到 `+/-180`，不允许再出现 `-600/+600` 的强反打。5. 新增曲线事件 `event=9` 表示“过冲后返回 duty 限幅 DRIVE”。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16272 B`、`data=164 B`、`bss=1588 B`，总 `18024 B`。 |
| HEX/ELF SHA256 | HEX `618f7f700926fa99017ac30d6fdfda561a7e5bcba2d210a8388e468984ad7f3f`；ELF `7f22c0f5a2a29deb3a9ac9dfa2bc6f7317fd71505a6dc45960b3db7aebb9ef46`。 |
| 下载结果 | 首次下载因 DAPLink `already open` 失败；通过 Parallels USB 断开/重连 DAPLink 后重新下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003cbd`，返回 `Successfully resumed device`。 |
| 实时曲线 | 下载后 DAPLink 再次残留 open 状态，已重新断开/连接 DAPLink 并启动 `start_servo_curve_live_macos.sh`。验证 `servo_curve_live.csv` 已有 `49` 行，`servo_curve_live.html/svg/csv` 正在更新。 |
| 下一步验证点 | 做 `1000->1500`、`1500->1000`、`1500->2000`、`2000->1500`。重点看：1. 最大 `overshoot_adc` 是否从约 `100..160` 降低；2. 过冲后是否出现 `event=9`；3. 过冲后返回段 duty 是否被压到约 `+/-180`，不再出现 `+/-600` 级别强反打；4. 如果过冲变小但回正太慢，再把 `OVERSHOOT_RETURN_DUTY` 从 `180` 微升到 `220`。 |

2026-06-13 过冲后嗞嗞声修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “这一版改后效果非常好，请继续看，有嗞嗞声，虽然不抖。” |
| 数据结论 | 读取实时 CSV 后确认：不抖但嗞嗞时，位置已稳定，误差只有约 `15..17 count`，速度接近 `0`，但 `event=9` 一直输出 `duty=180`，例如 `target_adc=2196`、`feedback_adc=2180/2181`、`overshoot_adc=13`、`duty=180` 连续出现。声音来自过冲返回限幅逻辑持续 DRIVE，而不是大幅正反抖动。 |
| 修改内容 | 新增 `OVERSHOOT_QUIET_BAND=24`、`OVERSHOOT_QUIET_VEL_Q4=12`。当本次动作已经过冲、PWM 目标未变化、误差进入 `24 count` 内且速度很低时，直接清零速度/轨迹/参考/输出，进入 `HOLD + HBridge_Brake()`，不再继续 `event=9 duty=180`。新增 `event=10` 表示“过冲后接近目标且速度低，进入安静 HOLD”。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16332 B`、`data=164 B`、`bss=1588 B`，总 `18084 B`。 |
| HEX/ELF SHA256 | HEX `24c8a5ea88c3706c269eaeae4b3bb6e6d35c0e899048038a4fe07a10d12a3236`；ELF `8b1932779b5b593b8d007f8bfd9c2d1ffeffb3261ca1be4fd50d386a9e0d63e2`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003cf9`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，CSV 已有 `49` 行。稳定状态数据显示 `event=2`、`duty=0`、误差约 `0..2 count`，例如 `target_adc=2197`、`feedback_adc=2196/2197`、`duty=0` 连续出现，符合消除嗞嗞声的预期。 |
| 下一步验证点 | 用户继续做大步进动作，重点看过冲后的尾段是否先出现少量 `event=9`，随后快速进入 `event=10/2` 且 `duty=0`。如果声音仍在但曲线显示 `duty=0`，则声音不是 H 桥持续驱动，需要检查 H 桥 Brake 模式或机械回弹；如果仍长时间 `event=9 duty=180`，把 `OVERSHOOT_QUIET_BAND` 从 `24` 提到 `30`。 |

2026-06-13 头尾嗞嗞声和外力锁止修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “确认了，中间还好，头跟尾有，请查阅数据。但是中间是过一段时间后会有。请依数据重新更新修改程序” / “后面波动更大的是我用了外力来转动舵板，说明锁止力变差，不能强大。” |
| 数据结论 | 读取 `build/servo_curve_live/servo_curve_live.csv` 后确认：低端约 `1007us` 时多次长期停在 `event=9 duty=180`，典型 `target_adc=1268`、`feedback_adc=1095..1110`、`error=160..173`、速度接近 `0`；高端约 `2009/2010us` 也有 `event=9 duty=+/-180`，误差约 `95..130`。中位长时间后也会出现 `event=9 duty=-180`，误差约 `54..56`。外力拨动后的数据还发现 `target_adc≈3121`、`feedback_adc≈3168`、`error≈-47` 时程序仍大量输出 `event=2/6/7 duty=0`，说明上一版静音逻辑会把外力造成的偏差误判为可保持，锁止力被软件压掉。 |
| 我的判断 | 嗞嗞声和锁止力不能用同一个“扩大静音带”解决。正确做法是：过冲后如果低速且误差连续不再改善，认为继续 `180 duty` 只是在顶机械/摩擦，进入安静 HOLD；但进入 HOLD 时必须记录实际停住的 ADC 作为锚点。之后 PWM 没变而舵板被外力拨离锚点时，按锚点偏差立即反向输出锁止 duty，不能继续用 `duty=0` 静音。 |
| 修改内容 | 1. 新增过冲无进展静音参数：`OVERSHOOT_STUCK_BAND=180`、`OVERSHOOT_STUCK_VEL_Q4=16`、`OVERSHOOT_STUCK_COUNT=18`、`OVERSHOOT_STUCK_IMPROVE=3`。2. 新增锚点 HOLD：进入安静 HOLD 时把当前 `feedback_adc` 作为 `last_control_feedback_adc`，曲线 `event=10`。3. HOLD 状态不再用理论目标误差判断是否继续静音，改用锚点误差；锚点偏差超过 `HOLD_ANCHOR_BAND=12` 并确认后，直接给反向锁止输出，事件 `event=11`。4. 新增稳定大偏差锁止门限 `STATIC_LOCK_ERROR=36`，当 PWM 目标未变、轨迹到位、误差超过该值时，低 duty 不再被 Brake/HLD 吞掉，而是至少给 `HOLD_DRIVE_MIN_DUTY=420`。5. `TERMINAL_LOW_DUTY_BRAKE` 从 `70` 提到 `90`，用于压住中位静态的小 duty。6. 修正 `cmd_sensitive`：不再因为 PWM 原始 `1508/1509us` 的 1us 抖动一直重置，只有换算后的 `target_adc` 变化超过 `CMD_HOLD_EXIT_EXTRA` 才认为是新命令。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=17060 B`、`data=168 B`、`bss=1584 B`，总 `18812 B`。 |
| HEX SHA256 | 最终下载版本 HEX `2b17008cfd3e8db5f9ea2e998045d13c70043084c000cb8733cc972839bbef50`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003fd5`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，因 DAPLink 残留 `already open` 做了一次 Parallels USB 断开/重连。随后 CSV 正常更新，静态中位采样 `33` 行全部 `event=6`、`duty=0`，误差约 `13..18 count`，说明 1us PWM 抖动不会再让程序持续 DRIVE。 |
| 下一步验证点 | 用户实测 `1000/1500/2000` 三点：1. 头尾如果仍嗞嗞，看曲线是否还长期 `event=9 duty=180`，若是可继续缩短 `OVERSHOOT_STUCK_COUNT` 或放宽 stuck 条件；2. 用手拨舵板时应看到 `event=11` 和明显反向 duty，松手后回到 `event=6/10/2 duty=0`；3. 若头尾锁止点停在机械极限外的偏差过大，需要重新校正 PWM-ADC 映射，而不是持续用 duty 顶死。 |

2026-06-13 固定目标重复定位修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不嗞嗞声了，但过冲和抖双明显了，请查看数据并作相应修正。而且每次三个位置都不一个点上。理想状态是1000、1500、2000三个点，无论什么时候都是在一个点上，现在会经常变化，证明数据还是不严谨。” |
| 数据结论 | 读取实时 CSV 和周期文件确认：同样 `1508/1509us` 下，曲线 `target_adc` 曾从约 `2154` 跳到 `2235`，并且 `cycle_004/005/006` 的稳定段停点不一致。根因不是 PWM 源大幅变化，而是上一版“安静锚点 HOLD”把实际停住的 `feedback_adc` 当成 HOLD 目标发布和锁止，导致目标随过冲/回弹位置漂移。该方法能消除嗞嗞声，但破坏 `1000/1500/2000` 固定重复定位。 |
| 我的判断 | 舵机产品必须以 PWM 映射目标为唯一目标点，不能把当前停点变成新目标。安静策略只能是“目标附近小误差 Brake”，不能改变目标；外力或偏差超过门限时，必须回到 PWM 固定目标，而不是回到上次停点。 |
| 修改内容 | 1. HOLD 分支取消 `hold_anchor` 作为目标，全部恢复使用 `s_lcdm_servo_cmd_target_adc`。2. 曲线 `target_adc` 永远发布 PWM 固定目标，不再发布 `feedback_adc/hold_anchor`。3. 过冲无进展进入 `event=10` 时，轨迹保持 `cmd_q4`，不再改成 `feedback<<4`。4. 新增固定目标静音带 `FIXED_HOLD_BAND=18`；误差在该范围内 `Brake/HOLD duty=0`。5. 静态锁止从单一 `420 duty` 改成分级：`STATIC_LOCK_ERROR=28` 开始回拉，`28..95 count` 用 `STATIC_LOCK_MIN_DUTY=220`，`>=96 count` 才用 `HOLD_DRIVE_MIN_DUTY=420`，减少目标附近反打造成的过冲和抖。6. `event=11` 含义改为“固定目标锁止 DRIVE”。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16972 B`、`data=168 B`、`bss=1584 B`，总 `18724 B`。 |
| HEX SHA256 | HEX `4936fc657ac59c1e67031b8dbc7f9dffba78c281200cfd893c4a36ed076bb0a2`。 |
| 下载结果 | 首次下载遇到 DAPLink `already open`；手动停止实时曲线、清理 Ubuntu pyOCD、Parallels 断开/重连 DAPLink 后重新下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003f7d`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后启动 SWD 实时曲线，低端静态 `33` 行数据显示 `pwm_us=1007/1008`、`target_adc` 固定 `1271`、`feedback_adc=1268..1271`、`duty=0`、全部 `event=2`。说明目标线不再跟停点漂移。 |
| 下一步验证点 | 用户分别切到 `1000/1500/2000` 多次，重点看同一 PWM 下 `target_adc` 是否固定、`feedback_adc` 最终是否回到同一小范围。如果仍过冲/抖，下一步只调刹车和分级 duty，不再改目标点逻辑。 |

2026-06-13 固定 ADC 目标和运动期 event11 抖动修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请查阅数据，更明显了哦，抖抖抖，加油修正！” |
| 数据结论 | 读取最新实时 CSV 后确认：大动作期间 `event=11` 多次出现，且 duty 可到 `+/-420`，例如多个短 block 中 `feedback` 从低端冲到高端后又被强回拉，造成明显反打和抖动。另一个问题是 `target_adc` 仍会随供电/动作变化：同一低端或中位 PWM 下，目标 ADC 曾出现几十 count 的变化，说明用 `target_mv` 再按实时 `VDD` 换 ADC 会把马达动作时的供电波动带进目标点。 |
| 我的判断 | 舵机目标位置应是 PWM 到 ADC raw 的固定映射，不能随实时 VDD 改变。电位器和 ADC 是比例量，控制目标用固定 raw count 更稳定。`event=11` 强锁止只能用于已经 HOLD 后被外力拨开，不能在运动尾段作为静态补偿，否则会把过冲放大。 |
| 修改内容 | 1. 新增 `lcdm_pwm_to_target_adc()`，使用 `LCDM_SERVO_TARGET_ADC_VDD_MV=3300` 作为固定参考，把 PWM/mV 校准值转换为固定 ADC raw。2. 控制环目标从 `lcdm_mv_to_adc(target_mv, vdd_mv)` 改为 `lcdm_pwm_to_target_adc(control_pulse_us)`，不再跟实时 VDD 漂移。3. P50 目标 ADC 显示也使用同一个固定目标函数，屏幕和控制一致。4. 删除运动过程中的 static lock 强制 `event=11` 分支；`event=11` 只保留在 HOLD 后外力偏离固定目标时使用。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16844 B`、`data=168 B`、`bss=1592 B`，总 `18604 B`。 |
| HEX SHA256 | HEX `034aefac20758e61d4b3cf97d6e24d3d9c6ffd54f2fa9b0a359b64125d0a7492`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003efd`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后启动 SWD 实时曲线，静态中位 `33` 行数据显示 `pwm_us=1508/1509`、`target_adc` 固定 `2064`、`feedback_adc=2060..2061`、`error=1..5`、`duty=0`，全部 `event=2`，没有 `event=11`。 |
| 下一步验证点 | 用户重新切 `1000/1500/2000`。若仍有过冲，下一步只调整轨迹末端速度/过冲返回 duty；如果锁止力不足，再只在 HOLD 后外力偏离时增强 event11，不再让 event11 参与正常运动尾段。 |

2026-06-13 关闭 HOLD 主动回拉修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请查阅数据，更明显了哦，抖抖抖，加油修正！” |
| 数据结论 | 再次读取下载后的实时 CSV：固定目标已经生效，静态段 `target_adc` 固定，但大动作尾段仍有 `event=11` 和 `+/-220..420 duty` 出现。例如目标 `2934` 附近反馈过冲到 `3039` 后，HOLD 分支给 `-420`，随后又给 `+220`，形成尾段反打；低端也有类似 `+220/-220` 来回拉。说明 event11 不应在刚到位/回弹阶段主动出力。 |
| 我的判断 | 现在第一优先级是消除抖动。HOLD 状态内只要偏差不大，应当短刹静音，不应主动回拉；偏差很大时退出 HOLD，由正常位置环重新处理。这样先切断尾段反打源。锁止力以后再用“稳定后外力检测”单独加回来。 |
| 修改内容 | 新增 `HOLD_RELEASE_BAND=140`。在 `hold_active` 且 PWM 未变时：若误差 `<=140 count`，只 `HBridge_Brake()`、`duty=0`、发布 `event=10/2`，不再输出 event11；若误差超过 `140 count`，退出 HOLD 交给正常控制流程。删除 HOLD 分支中的主动 `event=11` 输出路径。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16540 B`、`data=168 B`、`bss=1592 B`，总 `18300 B`。 |
| HEX SHA256 | HEX `d30c294878cdaab68a3b32e94ec50fd8ca2dc83252e13a8aa039fcb88df9ef69`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003dc9`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后启动 SWD 实时曲线，静态中位 `33` 行全部 `event=2`、`duty=0`，`target_adc=2061` 固定，`feedback_adc=2055..2058`，没有 `event=11`。 |
| 下一步验证点 | 用户重新做 `1000/1500/2000` 大步进。若抖动明显下降但停点偏差变大，下一步调末端速度/刹车窗口；若锁止力不足，再做“稳定后外力偏移才启动锁止”的独立逻辑，不能再让锁止参与运动尾段。 |

2026-06-13 行程距离相关刹车和静态误触发修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请查阅数据，可以稳定一些，但不同的行程位置偏差较大，比如从1500-1000的位置比2000-1000的位置近一点，也就是说长距离跑的更远一些。还有就是静态时，还是会触发，可能ADC取样数量不够，要多次判定有变化才处理，减少干扰杂讯，加油修正！” |
| 数据结论 | 读取 `build/servo_curve_live/servo_curve_live.csv`：最新 8178 行中 `event=2` 5663 行、`event=6` 1591 行、`event=10` 658 行，`duty` 静态为 0。目标 ADC 固定，但长行程后可停在较大误差内，例如低端块 `target_adc≈1193`、`feedback_adc≈1144`、误差约 `49 count`，历史数据中也有 `event=10` 允许 `90..150 count` 偏差停住的情况。静态中位点误差只有 `4..9 count`，但仍大量显示 `event=6`，说明是低 duty 刹车事件显示/状态切换，不是 H 桥实际持续输出。 |
| 我的判断 | 上一版 `HOLD_RELEASE_BAND=140` 太宽，能消抖但会牺牲重复定位；同时低 duty 分支把目标附近的小误差发布为 `event=6`，导致静态看起来仍被触发。需要把“ADC 噪声不处理”和“偏差过大必须回目标”分开。 |
| 修改内容 | 1. `HOLD_RELEASE_BAND 140 -> 42 count`，`HOLD_EXIT_COUNT 3 -> 6`，超过门限需连续多次成立才退出 HOLD。2. 记录 `motion_start_error`，按本次行程距离选择末端减速曲线：长行程 `>=1200 count` 使用 `TERMINAL_BAND=180`、`FINE=64`、`VREF=56/18`；中行程 `>=700 count` 使用 `150/56`、`64/20`；短行程保持上一版 `120/48`、`72/24`。3. HOLD 后若因连续偏离重新修正，把本次修正距离设为当前偏差，不沿用上一动作长行程参数。4. 进入 HOLD 的误差带 `14 -> 18 count`，与固定静音带一致。5. 低 duty 刹车分支在 PWM 目标未变且误差 `<=42 count` 时直接归位轨迹、发布 `event=2 HOLD`，不再让静态目标附近显示 `event=6`。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16656 B`、`data=168 B`、`bss=1592 B`，总 `18416 B`。 |
| HEX/ELF SHA256 | HEX `9f941beffcabe5d48c6b6b4065a5fa5cf52b8bc39376cd33de28f0d07f14a804`；ELF `b8282e682e930c2b4ecebf8e772b5b28863608e5b948df293778397146aacf91`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003e3d`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线。首次 16 行全部 `event=2`、`duty=0`、`target_adc=2061`、`feedback_adc=2058..2060`、误差 `1..3 count`。继续 15 秒后共 255 行，最后 160 行仍全部 `event=2`、`duty=0`，`target_adc=2061` 固定，`feedback_adc=2057..2061`，误差 `1..5 count`。 |
| 下一步验证点 | 用户实测 `1500->1000`、`2000->1000`、`1000->2000`、`2000->1500`。重点看同一目标点最终 `feedback_adc` 是否更接近固定 `target_adc`，以及长行程是否不再比短行程多冲很多；若长行程仍远，下一步只继续扩大长行程末端减速区或降低长行程末端 `VREF`。 |

2026-06-13 尾段偏差最小修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “越来越接近了，继续加油，误差还是大，请核对数据就能明白偏移量。” |
| 数据结论 | 读取最新 `build/servo_curve_live/servo_curve_live.csv` 和 `cycles/*.csv`：静态中位已经稳定，最后长时间 `event=2`、`duty=0`、误差约 `10..14 count`；但动作后仍会停在较大偏移。例如 `1508->1007` 尾段 `target_adc=1189`、`feedback_adc≈1162/1163`，误差约 `26 count`；`1509->1508` 尾段一段时间停在 `target_adc=2061`、`feedback_adc≈2014/2015`，误差约 `46 count`，且长期 `event=6 duty=0`。原因是速度环在尾段算出的 duty 小于 `TERMINAL_LOW_DUTY_BRAKE=90` 后被直接刹掉，误差没有继续收敛。 |
| 我的判断 | 上一版已经把静态误触发压住，但 `19..80 count` 的尾段偏差不能继续被低 duty 刹车吞掉。这里需要小力慢慢贴近目标，而不是立刻 HOLD。 |
| 修改内容 | 新增 `FINE_CORRECT_BAND=80 count`、`FINE_CORRECT_DUTY=110`。当 PWM 目标未变、误差大于固定静音带 `18 count` 且小于等于 `80 count`，如果速度环 duty 小于 `110`，强制给目标方向 `+/-110 duty`，并发布 `event=12`。这样 `26/46 count` 的停点偏差不会被 `event=6` 刹住；`<=18 count` 仍保持静音 HOLD，避免静止嗞嗞声。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16696 B`、`data=168 B`、`bss=1592 B`，总 `18456 B`。 |
| HEX/ELF SHA256 | HEX `835cf0fe10095e4f9f35ad42d5ce4749fe1471f2715653b86878c9e7b42742c2`；ELF `9af9a8f26cf5a88f6186ef1561fc745abcbfcd911beb4a6ec4d33f50d0749ae0`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003e65`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，静态 34 行全部 `event=2`、`duty=0`、`target_adc=2061`、`feedback_adc=2056..2059`，误差 `2..5 count`，说明新增最小修正没有造成静态抖动。 |
| 下一步验证点 | 用户重新做 `1500->1000`、`1000->1500`、`1500->2000`、`2000->1500`。重点看尾段是否出现短暂 `event=12 duty=+/-110` 后收敛到 `event=2`，最终误差应从 `26..46 count` 下降到接近 `<=18 count`。如果出现嗞嗞声或来回小摆，再把 `FINE_CORRECT_DUTY` 从 `110` 降到 `95` 或缩小 `FINE_CORRECT_BAND`。 |

2026-06-13 撤销 event12 尾段慢修正并收紧停住判定下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “是不是方向又错了，不旦没解决问题，反而速度变慢很多，请核对数据并分析，越来越不行了。上一次的偏位是否解决到位，即角度每次在三个点都是一致的，如果ADC本身数值会偏，可能带来角度偏，所以要综合分析数据才行。” |
| 数据结论 | 读取最新实时 CSV 后确认：上一版 `event=12` 出现 `472` 次，尾段长时间 `duty=+/-110`，动作明显拖慢，但停点仍可偏到 `30..60 count`。例如 `cycle_006_1508_to_1509` 中 `target_adc=2065` 时尾段 `feedback_adc≈2000..2003`、误差约 `61..65 count`，并有大量 `event=9/12`；`cycle_007_1509_to_1509` 中同一中位目标可停到 `feedback_adc≈2098..2100` 后又跳到 `2030..2032`，说明三点重复性没有真正解决。符号检查显示 `event=12` 的 duty 方向不属于整机接反，但该策略太弱、太慢，且破坏整体节奏。 |
| 我的判断 | 上一版不是 H 桥方向整体错，而是调试策略错：不能用固定 `110 duty` 慢慢拖尾。真正问题是程序把 `26/33/46 count` 这种仍明显偏离目标的位置当作可 HOLD/低 duty 刹车，导致三个点每次不一致。 |
| 修改内容 | 1. 删除 `FINE_CORRECT_BAND/FINE_CORRECT_DUTY` 和 `event=12` 输出逻辑。2. `HOLD_RELEASE_BAND 42 -> 24 count`，超过 24 count 不再被 HOLD 分支吞掉。3. `TERMINAL_SETTLE_BAND 96 -> 24 count`，超过 24 count 不再因小 duty 被末端 settle 判定为到位。4. 低 duty 刹车范围从 `<=160 count` 改为只在 `<=HOLD_RELEASE_BAND(24 count)` 内生效；`26..60 count` 的偏差交回正常速度环继续修正。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16648 B`、`data=168 B`、`bss=1592 B`，总 `18408 B`。 |
| HEX/ELF SHA256 | HEX `1874c04af7e952ea17aa274a9b18679c3b6ba6b7b855a79890d7fcaea590f864`；ELF `fc15ece0199532c34718312610ced11d48222a66c31804d351593c32819ddc03`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003e35`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，静态 33 行全部 `event=2`、`duty=0`、`target_adc=2061`、`feedback_adc=2056..2058`，误差 `2..5 count`。 |
| 下一步验证点 | 用户重新做 `1000/1500/2000` 三点往返。重点不是只看感觉速度，而是看每个固定 `target_adc` 的最终 `feedback_adc` 是否重复落在同一小范围内；如果同一目标仍随来向偏移，下一步要按目标点分别统计停点偏差和 ADC 原始漂移，判断是控制提前放手、机械回弹，还是电位器/ADC 本身在运动后有滞后偏移。 |

2026-06-13 过冲返回力和远偏位静音误判修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “差太远了，越来越不行了，看着数据来分析和修改哦” |
| 数据结论 | 读取最新实时 CSV：静态 ADC 不差，中位静态仍可稳定在 `feedback_adc≈2055..2059`；真正差的是动作后 `event=9` 过冲返回段。低端目标 `target_adc≈1191` 曾停在 `feedback_adc≈1093`，误差约 `98 count`；中位目标 `target_adc≈2060` 曾停在 `feedback_adc≈2124`，误差约 `-65 count`。这些段大量出现 `event=9 duty=180/0`，说明过冲返回限幅太小，且 `OVERSHOOT_STUCK_BAND=180` 会把很远的位置也当成“无进展可静音”。 |
| 我的判断 | 问题不是 ADC 静态漂移，也不是整机方向反；是过冲后返回目标的力不足，并且远离目标时允许进入 stuck/quiet HOLD。这样会造成看起来“停住了”，但角度离目标很远。 |
| 修改内容 | 1. `OVERSHOOT_RETURN_DUTY 180 -> 360`，远离目标时给更强返回力。2. 新增 `OVERSHOOT_RETURN_NEAR_BAND=48`、`OVERSHOOT_RETURN_NEAR_DUTY=180`：进入 `48 count` 内后仍降回 `180`，避免靠近目标反打太猛。3. `OVERSHOOT_STUCK_BAND 180 -> 24`，只有真正接近目标才允许无进展静音；`60..110 count` 这种大偏位不再被 stuck HOLD 吞掉。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16648 B`、`data=168 B`、`bss=1592 B`，总 `18408 B`。 |
| HEX/ELF SHA256 | HEX `1e397f95cf533201c7521a7d389a2732b41d2f1c03fbbb169884ac4529404049`；ELF `d0ed99d19436dac98459364f0eeaf9d4f328404146087c8bdfb6e671984683bc`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003e35`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，静态 33 行全部 `event=2`、`duty=0`、`target_adc=2064`、`feedback_adc=2066..2069`，误差 `-5..-2 count`。 |
| 下一步验证点 | 用户重新做 `1000/1500/2000` 三点往返。重点看 `event=9` 时若误差 `>48 count`，duty 是否能到约 `360` 而不是卡在 `180/0`；最终低端/中位/高端是否不再出现 `60..100 count` 的远偏位。 |

2026-06-13 尾段细刹车过早导致软弱修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “又回到第一版本来了，就是从另一个方向转到当前的控制方式的时候了，到点会来回摆动一下再停，软绵绵的没力的感觉，转不动。” |
| 数据结论 | 读取最近实时数据时，主要停留在 `1508/1509us` 中位附近；板上当时可出现目标 `target_adc≈2065`、反馈 `feedback_adc≈2014..2023`、误差 `43..46 count`、`event=1`、`duty=60..90`、`ref_velocity≈1` 的状态。这个误差还没有真正贴到目标，但旧参数已进入 `TERMINAL_FINE_BAND=48` 的细刹车区，导致速度参考和前馈被压得过低，体感就是软、慢、转不动。 |
| 我的判断 | `40..50 count` 对当前机械来说仍属于需要正常推动的尾段，不能提前按极细刹车处理。细刹车窗口太宽会把“需要补力靠近目标”的阶段误判成“马上停住”，造成到点前无力和来向相关的摆动。 |
| 修改内容 | 1. `TERMINAL_FINE_BAND 48 -> 24 count`。2. 长行程 `LONG_TERMINAL_FINE_BAND 64 -> 32 count`。3. 中行程 `MID_TERMINAL_FINE_BAND 56 -> 28 count`。保留普通末端区和过冲返回参数不变，让 `40..50 count` 误差重新走正常末端推动，而不是进入零前馈细刹车。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16648 B`、`data=168 B`、`bss=1592 B`，总 `18408 B`。 |
| HEX/ELF SHA256 | HEX `fc286b15078912d8ab6c29121afa78d28aae300a9c5d99f31ecc763a4a34dc19`；ELF `f315cbe2765c89c08965fe60f85971e161167990fba1db7c56ccc9e1d7cffe80`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003e35`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后启动 SWD 实时曲线。静态中位 `1508/1509us`，目标 `2064`，反馈 `2057..2059`，误差 `5..8 count`，全部 `event=2 HOLD`、`duty=0`，说明新版本已运行且静态没有乱驱动。 |
| 下一步验证点 | 用户重新做 `1000->1500`、`1500->1000`、`1500->2000`、`2000->1500`。重点看尾段 `40..60 count` 时 `duty` 是否不再只有 `60..90`，以及到点后是否从来回摆动变成一次收敛。 |

2026-06-13 SWD 曲线 HTML 重复开窗口修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “修改一下SVG的显示,不要每次都重新开启一个HTML，这样越来越多窗口” |
| 数据结论 | 检查 `tools/start_servo_curve_live_macos.sh` 和 `tools/live_servo_curve_on_mac.py` 后确认有两处自动打开浏览器：启动脚本执行 `open servo_curve_live.html`，Python 启动时也调用 `open_html(html_path)`。每次重启实时曲线都会新增 HTML 窗口。 |
| 修改内容 | 1. `start_servo_curve_live_macos.sh` 默认不再自动 `open` HTML；只有设置 `SERVO_CURVE_OPEN_HTML=1` 时才打开一次。2. `live_servo_curve_on_mac.py` 增加 `--open-html` 参数，默认不打开浏览器，只持续更新同一个 `servo_curve_live.html` 和 SVG/CSV。 |
| 验证结果 | `bash -n tools/start_servo_curve_live_macos.sh` 通过；`/opt/homebrew/bin/python3 -m py_compile tools/live_servo_curve_on_mac.py` 通过。 |
| 使用方法 | 后续正常运行 `./tools/start_servo_curve_live_macos.sh` 不会再新增窗口；第一次需要看图时手动打开 `build/servo_curve_live/servo_curve_live.html`，或用 `SERVO_CURVE_OPEN_HTML=1 ./tools/start_servo_curve_live_macos.sh` 只打开一次。 |

2026-06-13 曲线帧同步和速度预判刹车修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “看数据，晃的更明显了，看你好像找不到修正方向了” |
| 数据结论 | 读取最新曲线时先发现一个必须修正的数据问题：部分行出现不可能成立的数据组合，例如 `target_adc=1191`、`feedback_adc=1198`，但 `error_adc=1733`、`traj_adc=2119`。这是 SWD 逐个读取 `g_servo_curve_*` 变量时撞上固件正在更新，读到了半新半旧的一帧。继续用这种撕裂数据调控制会把方向带偏。物理动作上，大行程仍可看到过目标后再回摆，典型过冲约 `80..150 ADC count`，说明只调固定 `TERMINAL_FINE_BAND` 不够。 |
| 我的判断 | 当前主要方向应分两步：1. 先保证曲线数据是一致帧，避免误判。2. 控制上不能只靠固定 fine band，到点前应按实际 ADC 速度提前刹车；速度越高，刹车越早，减少过点后再反向拉回。 |
| 修改内容 | 1. 固件新增 `g_servo_curve_seq_head/g_servo_curve_seq_tail`，`lcdm_servo_curve_publish()` 发布数据前后写入奇偶序号。2. `tools/live_servo_curve_swd.py` 改为整块读取曲线变量，并检查 `seq_head == seq_tail` 且为偶数，同时校验 `error ~= target-feedback`；不一致则重试。3. 新增速度预判刹车参数 `APPROACH_BRAKE_VEL_Q4=40`、`APPROACH_BRAKE_GAIN=4`、`APPROACH_BRAKE_MAX_BAND=520`。当轨迹已到目标、反馈仍朝目标高速运动且进入速度相关刹车距离时，先 `HBridge_Brake()` 并发布 `event=5`，减少惯性过点。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=16968 B`、`data=168 B`、`bss=1608 B`，总 `18744 B`。 |
| HEX/ELF SHA256 | HEX `8d0759a16e3308019018fdc387a3e6a4259d041626a8ac2fdd16402c76522ff0`；ELF `2faf0334c55285ada0fbd989ff7e633278248e6b74d51eb6ad5ef0a199a85842`。 |
| 下载结果 | 第一次下载因 DAPLink `already open` 失败；已用 Parallels 将 `DAPLink CMSIS-DAP` 从 Ubuntu VM 断开再接回。随后 Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08003f75`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后启动 SWD 实时曲线，最近 `200` 行一致性检查 `bad_recent=0`，没有再出现 `target/error/traj` 撕裂。静态 `1007/1008us` 下 `target_adc=1191`、`feedback_adc=1190..1192`、`error=-1..1`、`event=2`、`duty=0`。 |
| 下一步验证点 | 用户重新做 `1000<->1500`、`1500<->2000`、`1000<->2000`。重点看新增的 `event=5` 是否在接近目标前出现，最大过冲是否从 `80..150 count` 降低；若速度明显变慢但过冲降低，下一步减小 `APPROACH_BRAKE_GAIN`；若仍过冲，继续加大速度预判刹车距离，而不是再只调 fine band。 |

2026-06-13 长行程晃动数据分析和轨迹到位判断修正下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “短行程好像还可以，就是有点慢的感觉，但这个暂时不要动，先看长行程，还是会晃。请分析数据” |
| 数据结论 | 新数据中短行程最终误差基本在 `十几 count` 内；长行程仍明显过冲。典型 `2010->1007` 过冲约 `148..151 count`，`1007->2010` 过冲约 `106..126 count`。位置曲线显示长行程在接近目标时仍然带较高速度越过目标，再被反向拉回。另一个数据问题是旧的分散变量采样仍会出现 `event/duty` 不匹配，说明必须把调试帧做成固定结构体，否则不能可靠分析 duty/ref/event。 |
| 我的判断 | 长行程尾段刹车漏掉的主要原因之一是轨迹位置用 Q4 小数，但旧逻辑要求 `s_lcdm_servo_traj_pos_q4 == cmd_q4` 完全相等才进入末端限速/预判刹车。实际曲线里长行程尾段 `traj_adc` 经常与 `target_adc` 差约 `1 count`，导致末端逻辑被跳过，在目标前仍给较大推动力。短行程暂不改速度和主参数，只修这个“到位判断过严”的逻辑。 |
| 修改内容 | 1. 新增 `LCDM_SERVO_TRAJ_TARGET_TOL_Q4=24`，把轨迹到位条件改成 Q4 容差判断 `traj_at_cmd`，替换原来三个精确等于目标的判断。2. 新增 packed 结构体 `g_servo_curve_frame`，字段顺序固定为 `seq_head + 曲线数据 + seq_tail`；旧的 `g_servo_curve_*` 变量保留兼容。3. `tools/live_servo_curve_swd.py` 优先读取 `g_servo_curve_frame`，只接受头尾序号一致且 `error ~= target-feedback` 的帧。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=17496 B`、`data=168 B`、`bss=1640 B`，总 `19304 B`。 |
| HEX/ELF SHA256 | HEX `180e26ffc2f3d326b0841052d8aa5338944697d93a798a78faa0124f1d151456`；ELF `bb98a27af370c2a459cc82517f2949605424171c848aed72638383e79410146f`。 |
| 下载结果 | Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004185`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后启动 SWD 实时曲线，结构体采样正常，`884` 行静态数据中最近 `300` 行 `bad_err_recent=0`、`bad_event_duty_recent=0`。高端静态 `2010/2011us` 时 `target_adc=2935`、`feedback_adc≈2936..2940`、`event=2`、`duty=0`。 |
| 下一步验证点 | 用户只测长行程：`1000->2000`、`2000->1000` 各多做几次。重点看最大过冲是否从 `100..150 count` 下降，以及 `event=5/4` 与 `duty` 是否现在完全可信。短行程速度暂时不动。 |

2026-06-13 长行程复测数据核对和轨迹容差加大下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完，请核对数据” |
| 数据结论 | 读取 `build/servo_curve_live/servo_curve_live.csv` 共 `56888` 行，结构体帧数据可信：`bad_err=0`、`bad_event_duty=0`。最近长行程仍有明显过冲和回拉：典型 `1000->2000` 过冲 `180..211 ADC count`，过点后反向 duty 可到 `540..693`；典型 `2000->1000` 过冲 `144..154 ADC count`，过点后反向 duty 可到 `360..612`。短行程暂不作为本轮修改目标。 |
| 我的判断 | 长行程仍会漏掉一部分末端预判刹车。曲线里 `traj_adc` 有时离 `target_adc` 约 `2 count`，旧的 `LCDM_SERVO_TRAJ_TARGET_TOL_Q4=24` 约等于 `1.5 ADC count`，可能导致“轨迹已基本到目标，但程序仍未认定到位”，从而错过速度预判刹车窗口。 |
| 修改内容 | 只修改轨迹到位容差：`LCDM_SERVO_TRAJ_TARGET_TOL_Q4 24 -> 48`，约等于 `3 ADC count`。未调整短行程速度、主速度参数、H 桥 duty 或 ADC 映射。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=17496 B`、`data=168 B`、`bss=1640 B`，总 `19304 B`。 |
| HEX/ELF SHA256 | HEX `a66771685e798964357e0335fac4250358748f6b901eae6d9e0e93fa46b58db0`；ELF `bd57be56fecc9f5c98a65a1fa72562b0217dda8257ceb7bc136d89eeafee120c`。 |
| 下载结果 | 下载前已停止 SWD 实时曲线服务。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004185`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，`954` 行静态数据中最近 `300` 行 `bad_err_recent=0`、`bad_event_duty_recent=0`。低端静态 `1007us` 时 `target_adc=1193`、`feedback_adc=1188..1191`、`event=2`、`duty=0`。 |
| 下一步验证点 | 只测长行程：`1000->2000` 和 `2000->1000` 各多做几次。重点看新增容差后 `event=5` 是否更早出现、最大过冲是否低于上一版的 `180..211 / 144..154 count`，同时确认短行程体感没有被拖慢。 |

2026-06-13 高端二次晃原因分析和尾段补力抑制下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “好像还可以，高端出现二次晃” / “查下是什么原因” |
| 数据结论 | 读取最新 `servo_curve_live.csv` 后确认帧数据可信：最近数据 `bad_err_recent=0`、`bad_event_duty_recent=0`。高端过冲已明显比上一版小，很多 `1000/1500 -> 2000` 动作 `over=0..16 count`，不是大过冲问题。二次晃的典型数据在 `2064->2934`：先 `event=5` 预判刹车，误差从约 `234 -> 143 count`；随后到 `error=53` 时程序退出刹车并重新进入 `event=1`，给 `duty=190`；下一帧又出现 `duty=-42`。结论是刹车后尾段又补了一脚较大的驱动力，激发高端机械弹性/惯性，形成二次晃。 |
| 我的判断 | 不能再整体提前刹车，否则速度会变慢；也不应调整低端/短行程。应只处理“预判刹车后进入最后几十个 ADC count 时的补力”，让刹车后的尾段继续消耗惯性，避免重新强驱。 |
| 修改内容 | 新增 `s_lcdm_servo_approach_brake_tail_count` 和尾段参数：`TAIL_COUNT=12`、`TAIL_BAND=72 count`、`TAIL_VEL_Q4=8`、`TAIL_DUTY=80`。`event=5` 预判刹车后保留短时尾段状态；当误差进入 `72 count` 内且反馈仍朝目标滑动时继续 `HBridge_Brake()` 并发布 `event=5`；如果不再滑动但还需补力，则把朝目标补力限制到 `80 duty` 并发布 `event=11`。新 PWM 目标、ADC 无效、无信号、超范围释放和确认 HOLD 时都会清掉这个尾段状态，避免跨动作残留。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=17732 B`、`data=168 B`、`bss=1640 B`，总 `19540 B`。 |
| HEX/ELF SHA256 | HEX `44852ee9c418601e8b938af628de6776384e551b6bb8f3d34c70ba3299fe2a17`；ELF `ee2f6111f0f743e978b371df6185eec1e431703e5fcfac0311af76f8ebfd10b3`。 |
| 下载结果 | 下载前已停止 SWD 实时曲线服务。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004271`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，`917` 行静态数据中最近 `300` 行 `bad_err_recent=0`、`bad_event_duty_recent=0`。高端附近静态 `pwm_us=1975`、`target_adc=2874`、`feedback_adc=2858..2865`、`event=2`、`duty=0`。 |
| 下一步验证点 | 重点测高端：`1000->2000`、`1500->2000`、`2000->1500`。看二次晃是否减少；曲线中应看到尾段更多 `event=5` 或少量 `event=11`，且不应再出现高端尾段 `error≈50 count` 时 `duty≈190` 的补力。 |

2026-06-14 高端不到位数据核对记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请参考当前数据，看是否有发现高端扭不到位” |
| 数据结论 | 读取当前 `build/servo_curve_live/servo_curve_live.csv` 共 `97198` 行，最近 `30000` 行 `bad_err_recent=0`、`bad_event_duty_recent=0`，数据可信。高端目标多为 `target_adc≈2934/2935`，但长时间 HOLD 后常停在 `feedback_adc≈2910..2918`，误差约 `+16..24 count`。最后一段长时间静态：`target_adc=2934`、`feedback_adc=2912..2918`、平均误差约 `19 count`、最后误差 `18 count`、`event=2`、`duty=0`。按 `1000->2000` 约 `1743 count = 90 度` 估算，`18..24 count` 约等于 `0.9..1.2 度`。 |
| 我的判断 | 有轻微高端不到位。不是马达完全扭不上去，因为动作中可接近目标甚至部分段有过冲；主要是软件把高端少 `18..24 count` 的位置当成到位停住。当前 `HOLD_BAND=18`、`HOLD_RELEASE_BAND=24`、`TERMINAL_SETTLE_BAND=24`，再叠加昨天为消二次晃加入的尾段限力，会让最后约 `20 count` 的偏差被 HOLD/低 duty 刹停吞掉。 |
| 建议下一步 | 下一版不要动前段速度和高端预判刹车主逻辑，先只收紧“到位/HOLD”判断或给高端最后 `20..30 count` 一个受限的慢补偿。目标是把高端最终误差从 `18..24 count` 压到 `8..12 count`，同时避免恢复昨天的二次晃。 |

2026-06-14 高端不到位 HOLD 窗口收紧下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请改善” |
| 修改依据 | 上一段数据证明高端常在 `target_adc≈2934/2935`、`feedback_adc≈2910..2918` 时进入 `event=2 duty=0`，误差约 `18..24 count`。这是到位判定太宽，不是前段速度问题。 |
| 修改内容 | 只收紧到位/HOLD 窗口，不改前段速度、预判刹车、尾段刹车时间和 H 桥最大 duty：`LCDM_SERVO_HOLD_BAND 18 -> 12`、`LCDM_SERVO_FIXED_HOLD_BAND 18 -> 12`、`LCDM_SERVO_HOLD_RELEASE_BAND 24 -> 12`、`LCDM_SERVO_TERMINAL_SETTLE_BAND 24 -> 12`。目标是高端差 `18..24 count` 时不再直接 HOLD，而是继续小幅修正；误差约 `<=12 count` 才允许停住。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=17696 B`、`data=168 B`、`bss=1640 B`，总 `19504 B`。 |
| HEX/ELF SHA256 | HEX `1195de9d8f3f5dc37df247b0e5e9b5484ccf4afb5a7f97167c811bdffca20158`；ELF `695c7909d81b1e4c7957858913ffaa38748a8b79db085e0da419fc766a034d6d`。 |
| 下载结果 | 下载前已停止 SWD 实时曲线服务。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800424d`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，`1663` 行静态数据中最近 `300` 行 `bad_err_recent=0`、`bad_event_duty_recent=0`。中位静态 `target_adc=2065`、`feedback_adc=2051..2056`、误差 `9..14 count`，最近 `299/300` 行为 `event=2 duty=0`。 |
| 下一步验证点 | 重点测高端 `1500->2000`、`1000->2000`、`2000` 保持。看高端最终误差是否从 `18..24 count` 降到约 `8..12 count`，同时确认没有恢复二次晃或静态嗞嗞声。 |

2026-06-14 HOLD 窗口收紧版本因嗞嗞声回退记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有嗞嗞声，退回上一版” |
| 问题结论 | 刚才把 HOLD/到位窗口从 `18/24 count` 收到 `12 count` 后，静态误差会更频繁退出 HOLD 并尝试微调，用户实测出现嗞嗞声。说明 `12 count` 对当前 ADC/机械静态噪声和齿隙太窄。 |
| 回退内容 | 只撤销上一版 4 个窗口参数，恢复到高端二次晃抑制版本：`LCDM_SERVO_HOLD_BAND 12 -> 18`、`LCDM_SERVO_FIXED_HOLD_BAND 12 -> 18`、`LCDM_SERVO_HOLD_RELEASE_BAND 12 -> 24`、`LCDM_SERVO_TERMINAL_SETTLE_BAND 12 -> 24`。保留 `event=5/event=11` 高端尾段抑制逻辑。 |
| 编译结果 | Ubuntu 编译通过，P50-only LTO 版 `text=17732 B`、`data=168 B`、`bss=1640 B`，总 `19540 B`。 |
| HEX/ELF SHA256 | HEX `44852ee9c418601e8b938af628de6776384e551b6bb8f3d34c70ba3299fe2a17`；ELF `ee2f6111f0f743e978b371df6185eec1e431703e5fcfac0311af76f8ebfd10b3`。 |
| 下载结果 | 下载前已停止 SWD 实时曲线服务。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004271`，返回 `Successfully resumed device`。 |
| 实时验证 | 下载后重启 SWD 实时曲线，`1077` 行静态数据中最近 `300` 行 `bad_err_recent=0`、`bad_event_duty_recent=0`。高端静态 `target_adc=2932`、`feedback_adc=2928..2937`、误差 `-5..4 count`，最近 `300/300` 行为 `event=2 duty=0`。 |
| 后续判断 | 高端不到位不能通过简单收紧全局 HOLD 窗口解决，否则会带回静态嗞嗞声。下一步若继续改善高端不到位，应改成更有条件的策略，例如只在“刚到高端且误差连续稳定大于某阈值”时给一次短小补偿，而不是全局缩小 HOLD 窗口。 |

2026-06-14 SVG/HTML 增加 H 桥 PWM 与 ADC 响应关系曲线记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “另外，是否可以在原有的VSG的HTML上再增加一个H桥的驱动PWM、ADC变化的对应关系曲线图，这样可以更清晰的看到给出的H桥PWM驱动后马达带动电位器转动后的AD值，以及反向H桥PWM刹车信号的三者关系，请修改” |
| 修改内容 | 只修改 SWD 曲线显示工具，不改固件控制。`tools/live_servo_curve_swd.py` 的 SVG 高度从 `760` 增加到 `1040`，在原 `Position`、`Control` 两个面板下新增第三个面板 `H-Bridge PWM -> ADC Response`。该面板在同一时间轴上显示 signed H 桥 duty/PWM 柱状图、`ADC step x16`、`ADC velocity q4`、`ref velocity`，并用淡色竖线标出 `event=4/5/6/9/11`，用于观察驱动、ADC 变化、反刹/返回/尾段修正之间的关系。 |
| 兼容性 | CSV 字段不变，固件变量不变。`adc_step` 和 `adc_step_x16` 只在画 SVG 时由相邻 `feedback_adc` 差值派生，不影响实时采样、历史 CSV 或周期 CSV。`tools/live_servo_curve_on_mac.py` 继续复用同一个 `write_svg()`，所以 live 图和 `cycles/*.svg` 都会带第三面板。 |
| 验证结果 | `python3 -m py_compile tools/live_servo_curve_swd.py tools/live_servo_curve_on_mac.py` 通过。Demo 输出 `/tmp/servo_curve_svg_test/servo_curve_live.svg` 成功，SVG 内有 `H-Bridge PWM -> ADC Response`、`ADC step x16` 和 `event marks`。 |
| 实时验证 | 已重启 `./tools/start_servo_curve_live_macos.sh`，Mac 端重新上传新绘图脚本到 Ubuntu。真实 `build/servo_curve_live/servo_curve_live.svg` 已更新为 `viewBox=0 0 1120 1040`，包含第三面板；实时 CSV 已重新采样 `830` 行，最后一行高端静态 `target_adc=2934`、`feedback_adc=2931`、`error=3`、`duty=0`、`event=2`。 |
| 使用方法 | 继续打开同一个 `build/servo_curve_live/servo_curve_live.html`，不会新增浏览器窗口。第三面板中蓝色柱为 signed H 桥 PWM/duty，红色虚线为每个采样点 ADC 变化量，紫色为固件估算 ADC 速度，橙色为目标速度，淡色竖线是刹车/返回/尾段事件。 |

2026-06-14 H 桥 PWM/ADC 响应独立大图页面记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “看得不够清晰，将H-BRIDGE PWM和ADC RESPONSE独立一个界面，放到最大来看” |
| 修改内容 | 新增独立大图页面 `build/servo_curve_live/hbridge_response.html` 和大图 `hbridge_response.svg`。`hbridge_response.svg` 使用 `1440x860`、`viewBox=0 0 1440 860`，只显示 `H-Bridge PWM -> ADC Response` 一张大图；主页面 `servo_curve_live.html` 顶部增加 `H-Bridge detail` 链接。 |
| 显示内容 | 独立页面保留同一时间轴上的 signed H 桥 duty/PWM 柱状图、`ADC step x16`、`ADC velocity q4`、`ref velocity`，并用淡色竖线标出 `event=4/5/6/9/11`。顶部显示当前 `PWM/target/feedback/error/duty/adc_step/velocity_q4/event`，用于快速判断 H 桥输出后 ADC 是否有对应响应，以及反刹/尾段修正是否及时。 |
| 兼容性 | 固件和 CSV 字段不变。Mac 端 `tools/live_servo_curve_on_mac.py` 与 Ubuntu/SWD 端 `tools/live_servo_curve_swd.py` 都同步生成/刷新 `hbridge_response.html/svg`。不新增浏览器窗口，仍由用户手动打开或从主页面链接进入。 |
| 验证结果 | `python3 -m py_compile tools/live_servo_curve_swd.py tools/live_servo_curve_on_mac.py` 通过。Demo 输出 `/tmp/servo_curve_hbridge_test/hbridge_response.html/svg` 成功，主 HTML 有 `hbridge_response.html` 链接。 |
| 实时验证 | 已重启 `./tools/start_servo_curve_live_macos.sh`。真实目录已生成 `build/servo_curve_live/hbridge_response.html` 和 `hbridge_response.svg`；主页面有 `H-Bridge detail` 链接。实时 CSV 已重新采样 `927` 行，最后一行低端静态 `target_adc=1193`、`feedback_adc=1184`、`error=9`、`duty=0`、`event=2`。 |
| 使用方法 | 打开 `build/servo_curve_live/hbridge_response.html` 看最大化 H 桥/ADC 响应图；或从 `servo_curve_live.html` 顶部点击 `H-Bridge detail`。 |

2026-06-14 H 桥响应页改为触发快照并拉宽时间轴记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “把H桥收到的信息资料，每侦测到一次就暂停一次，同时把时间轴拉大，方便看，每更新一次刷新一次” |
| 修改内容 | H 桥独立页改为触发快照模式。新增 `HBridgeSnapshot`：检测到 `duty != 0` 或 `event=4/5/6/9/11` 时开始抓图，包含触发前约 `260ms` 和触发后数据；若进入 `event=2 duty=0` 稳定约 `220ms` 就停止，最长抓约 `1400ms`。抓完后 `hbridge_response.svg` 停住不再滚动，直到下一次 H 桥动作再刷新。 |
| 时间轴调整 | 独立大图从 `1440x860` 改为 `2400x900`、`viewBox=0 0 2400 900`，页面用横向滚动显示，让时间轴更宽，方便看 PWM 输出、ADC 响应和反刹之间的前后关系。 |
| 文件修改 | `tools/live_servo_curve_swd.py` 增加 `HBridgeSnapshot`、触发快照绘图和更宽的 `hbridge_response.svg`；`tools/live_servo_curve_on_mac.py` 同步使用同一个快照逻辑，Mac live 服务也按触发快照刷新 H 桥独立页。 |
| 验证结果 | `python3 -m py_compile tools/live_servo_curve_swd.py tools/live_servo_curve_on_mac.py` 通过。Demo 输出 `/tmp/servo_curve_hbridge_pause_test/hbridge_response.svg` 为 `2400x900`，标题为 `H-Bridge PWM and ADC Response - Trigger Snapshot`。 |
| 实时验证 | 已重启 `./tools/start_servo_curve_live_macos.sh`。真实 `build/servo_curve_live/hbridge_response.svg` 已更新为 `2400x900` 触发快照图，`hbridge_response.html` 支持横向滚动；实时 CSV 已重新采样 `1067` 行，最后一行低端静态 `target_adc=1193`、`feedback_adc=1191`、`error=2`、`duty=0`、`event=2`。 |
| 使用方法 | 继续打开 `build/servo_curve_live/hbridge_response.html`。每次拨动 PWM 造成 H 桥动作后，该页会刷新为新的一次动作快照并停住，便于逐次分析。 |

2026-06-14 H 桥快照页改为真实 AD 曲线、0ms 触发时间轴和无触发不刷新记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前AD曲线看上去是不对的，为何会一高一低变化?理论上是应该线性从低到高或从高到低。另外，请以PWM启动瞬间为0时间起始点来计时显示轴，并标注幅值，包插PWM和AD值” / “没有触发不要更新界面” |
| 问题解释 | 之前 H 桥独立页把 `ADC step x16` 和 `velocity_q4` 放在主视觉上，显示的是相邻采样点的 AD 变化量/导数，所以会一高一低跳动；这不是实际电位器 AD 位置曲线。 |
| 修改内容 | H 桥独立 SVG 改为上下两层：上层显示真实位置曲线 `target_adc`、`feedback_adc`、`raw_adc`、`traj_adc`，下层显示 signed H 桥 duty/PWM 柱状图、`ADC step x16`、`velocity_q4`、`ref_velocity`。时间轴改为相对触发时间：第一次 H 桥动作或刹车事件为 `0ms`，触发前为负时间，触发后为正时间，并在图中标出 `0 ms PWM/H-Bridge trigger`。 |
| 幅值标注 | 上层左侧标 AD count 刻度，并在右上显示 `AD count min..max`；下层标 PWM duty 正负刻度，并显示 `PWM duty +/-N | ADC-rate +/-N`。顶部状态行保留当前 `PWM/target/feedback/error/duty/adc_step/velocity_q4/event`。 |
| 无触发不刷新 | `HBridgeSnapshot` 改为只有一次快照完成后才置 dirty 并写 `hbridge_response.svg/html`。`hbridge_response.html` 不再每秒强制换图，而是轮询 `hbridge_response.version`；只有新快照写入、版本号变化时才更新图片和 `Snapshot updated` 时间。无触发时界面不换图、不更新时间戳。 |
| 验证结果 | `python3 -m py_compile tools/live_servo_curve_swd.py tools/live_servo_curve_on_mac.py` 通过。Demo 输出 `/tmp/servo_curve_ad_axis_test2/hbridge_response.svg` 成功，包含 `target AD`、`feedback AD`、`raw AD`、`AD count`、`PWM duty`、`0 ms PWM/H-Bridge trigger` 和 `2400x900` 大图。 |
| 实时验证 | 重启 live 服务时 DAPLink 曾被 pyOCD 残留占用，日志为 `RuntimeError: already open`；已断开/重连 `DAPLink CMSIS-DAP` 到 Ubuntu 并重启服务。实时 CSV 恢复采样 `1104` 行，最后一行低端静态 `target_adc=1191`、`feedback_adc=1188`、`error=3`、`duty=0`、`event=2`。无触发期间 `hbridge_response.svg` 修改时间保持 `1781407035` 不变，确认不会无触发刷新。 |

2026-06-14 H 桥快照页占空比标注、50ms 时间轴和中文备注记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “看不出H桥的PWM在哪里变化，能否标注出占空比的量在显示上。另外，我说的每一个触发要从0ms开始，这样好看，同时ms第隔50ms标注一个，这样可以看到变化量。” / “备注全部改成中文显示” |
| 修改内容 | H 桥独立快照页从触发点开始截取显示，第一点即 `0ms`，不再显示触发前负时间。横轴改为每 `50ms` 标注一次。H 桥 signed duty/PWM 柱状图上直接标注 `占空比+80 +8%` 这类数值，负方向显示为 `占空比-xxx -xx%`。 |
| 中文化 | H 桥独立页和主曲线中可见备注改为中文：标题、图例、顶部状态行、0ms 触发点、AD/PWM 幅值、事件说明、快照刷新文字、主曲线链接说明等全部中文显示。保留 `PWM/AD/q4` 这类工程缩写。 |
| 显示逻辑 | 上半图继续显示真实 `目标AD/反馈AD/原始AD/轨迹AD`；下半图显示 `H桥正反PWM占空比/AD步进x16/AD速度q4/目标速度`。无触发时 `hbridge_response.html/svg` 不刷新；只有新快照完成才更新 `.version` 并刷新页面。 |
| 验证结果 | `python3 -m py_compile tools/live_servo_curve_swd.py tools/live_servo_curve_on_mac.py` 通过。Demo 输出 `/tmp/servo_curve_cn_test/hbridge_response.svg` 成功，包含中文标题、`0ms PWM/H桥触发点`、50ms 时间标注、`占空比-260 -26%`、`占空比+260 +26%`、中文事件说明。 |
| 实时验证 | 已重启 `./tools/start_servo_curve_live_macos.sh`。真实 `hbridge_response.html/svg` 已更新中文备注；当前快照中可见 `占空比+80 +8%` 标注。实时 CSV 正常采样 `956` 行，最后一行 `pwm_us=1511`、`target_adc=2061`、`feedback_adc=2039`、`error=22`、`duty=0`、`event=2`。 |

2026-06-14 H 桥快照页 0ms 右移和同向/反向 PWM 分轴显示记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “改二个点，一是将0点后移50ms，可看到起点；二是将同向的PWM放在同几，反向的PWM放在反向，这样较容易看到反向刹车的轨迹” / “备注全部改成中文显示” |
| 修改内容 | 只修改曲线工具，不改固件控制算法。`hbridge_response.svg` 的相对时间轴在左侧预留 `50ms` 空白，`0ms PWM/H桥触发点` 不再贴左边，便于看起点。横轴仍按 `50ms` 标注。 |
| 同向/反向显示 | `rows_with_adc_step(..., relative_to_hbridge_trigger=True)` 根据触发帧的 `target_adc - feedback_adc` 判断本次目标方向，把 H 桥 duty 转成 `hbridge_rel_duty`。同向驱动显示为正半轴，反向刹车/回拉显示为负半轴；占空比标注改为 `同向+xxx +xx%` 或 `反向-xxx -xx%`。 |
| 中文备注 | 图例改为 `H桥PWM：同向为正/反向为负`。底部备注改为 `0ms左侧预留50ms`、`同向在正半轴，反向刹车在负半轴`，避免再用英文或含糊说明。 |
| 验证结果 | `python3 -m py_compile tools/live_servo_curve_swd.py tools/live_servo_curve_on_mac.py` 和 `/opt/homebrew/bin/python3 -m py_compile ...` 均通过。Demo 输出 `/tmp/servo_curve_dir_test/hbridge_response.svg` 成功，包含 `0ms PWM/H桥触发点`、`50 ms`、`同向+260 +26%`、`反向-260 -26%`、`H桥PWM：同向为正/反向为负`。 |
| 实时验证 | 已停止并重启 `./tools/start_servo_curve_live_macos.sh`，未打开新 HTML 窗口。清除 `tools/__pycache__` 后真实 `build/servo_curve_live/hbridge_response.svg` 于 `2026-06-14 11:47:08` 更新；可见 `0 ms` 位于 x≈250，`50 ms` 位于 x≈440，`0ms PWM/H桥触发点` 位于 x≈256，并显示 `同向+219 +22%`、`同向+80 +8%`、`H桥PWM：同向为正/反向为负`、`0ms左侧预留50ms`。实时 CSV 采样 `1563` 行，最后一行 `pwm_us=1007`、`target_adc=1191`、`feedback_adc=1206`、`error=-15`、`duty=0`、`event=2`。 |

2026-06-14 H 桥快照触发源改为外部 PWM 输入变化记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前程序有自己因为位置不对引起触PWM，所以改成外界输入PWM变化来触发取样，自身触发不再变化显示刷新。另外，AD步进16是什么意思。最后，没有看到反刹车的波形，是否目前没有刹车介入。” |
| 问题结论 | 用户判断正确。上一版 `HBridgeSnapshot` 以 `duty != 0` 或 `event=4/5/6/9/11` 触发快照，导致 PWM 没变但位置偏差触发 H 桥自修正时也会刷新图，容易误判成一次外部命令动作。 |
| 修改内容 | H 桥独立快照触发源改为外部命令变化：只有 `pwm_us` 相邻变化大于等于 `4us`，或 `target_adc` 相邻变化大于等于 `3 count` 时才从该点作为 `0ms` 开始记录。H 桥 `duty/event` 不再触发刷新，只作为被记录和显示的数据。 |
| 方向和刹车显示 | `0ms` 和同向/反向判断改为基于外部 PWM/目标变化点。图上新增顶部统计：`同向峰值`、`反向峰值`、`刹车/反打事件`。若 `反向峰值=0` 且事件数为 `0`，说明该快照没有明显反向刹车；若有负向蓝柱或 `反向峰值>0`，说明存在反向输出/回拉。 |
| AD单次变化说明 | 原 `AD步进x16` 改名为 `AD单次变化x16`。含义是相邻两次采样的 `feedback_adc` 差值乘以 `16`，只是为了把很小的单次 AD 变化放大到图上看得见；它不是实际 ADC 位置值，也不是 16 位 ADC。 |
| 验证结果 | `python3 -m py_compile tools/live_servo_curve_swd.py tools/live_servo_curve_on_mac.py` 和 `/opt/homebrew/bin/python3 -m py_compile ...` 均通过。Demo 输出 `/tmp/servo_curve_pwm_trigger_test/hbridge_response.svg` 成功，包含 `PWM输入触发快照`、`只在外部输入PWM/目标位置变化时从0ms开始记录`、`AD单次变化x16`、`同向峰值 260`、`反向峰值 260`。 |
| 实时验证 | 已重启 `./tools/start_servo_curve_live_macos.sh`，未打开新窗口。重启后采样 `1503` 行，`external_pwm_change_count=0`、`hbridge_activity_count=0`，最后一行 `pwm_us=1007`、`target_adc=1189`、`feedback_adc=1196`、`error=-7`、`duty=0`、`event=2`。因为外部 PWM 未变化，正式 `hbridge_response.svg` 没有生成新快照，仍保留上一张旧图；下一次外部 PWM 切换后才会按新规则刷新。 |

2026-06-14 成熟舵机调机模板照片评估记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请参考MOTOR目录下的调机模板目录内的所有照片，这是一个成熟舵机生产厂家的调试舵机控制器，可以直接连舵机通过PWM口进行下载主要参数和读取参数。照片里所有具体参数都考虑了舵机的相互系之间的牵作功能，比哪《过冲走电流回中不准》项目里的内容，齿轮紧与虚位大是相对的，等等。这些在规划整个程序架构时全盘考虑。因此，请参照这些照片给出的可调参数，重新评估一下我们的舵机设计方案及方向，请评估。” |
| 已检查照片 | 已读取 `调试模板/1.jpg` 到 `14.jpg`。识别出测试/编程卡入口、主参数菜单、信号范围对应舵机角度、过冲走电流回中不准、堵转保护设置、舵机模式设置、中位微调、其它参数设置、读取舵机参数和镜像文件等参数族。 |
| 评估结论 | 当前硬件和“PWM 输入 -> ADC 目标 -> H 桥闭环”的基本方向正确，但当前控制体系还不够产品化。成熟厂家不是只调 PID，而是把映射、轨迹、速度估算、齿隙、锁力、反压、保护、参数读写和镜像模板组成完整系统。 |
| 关键判断 | `过冲不准/回中不准/走电流/虚位大/锁力` 必须作为同一组互相牵制的调机现象处理，不能继续单独改某个过冲常数。电位器方向和电机方向也应拆成两个独立可配项。 |
| 文档输出 | 新增评估文档 `docs/servo_tuning_template_assessment.md`，记录照片参数、与 `ServoParams v2` 的差距、建议的 v3 参数分组和下一步重构方向。 |

2026-06-14 ServoParams V3 第一阶段落地记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “可以，开始V3” |
| 本次范围 | 先落地 V3 参数结构、协议长度、文档和最小控制连接；不重写控制状态机，不调整当前已验证的运动曲线。 |
| 参数变化 | `ServoParams.version` 升到 `3`，结构体大小为 `128 B`，仍放入最后一页 `128 B` Flash 参数页。新增 `servo_angle_deg`、`pot_angle_deg`、`lock_gain_q8`、`counter_emf_gain_q8`。 |
| 标志变化 | 新增 `SERVO_PARAM_FLAG_POT_REVERSE` 和 `SERVO_PARAM_FLAG_MOTOR_REVERSE`。`POT_REVERSE` 已接入目标 ADC 映射；`MOTOR_REVERSE` 已接入 H 桥 signed duty 方向。旧 `INVERTER` 暂时保留为目标映射反向兼容项。 |
| 通讯变化 | `SERVO_COMM_MAX_PAYLOAD` 改为 `128`，`SERVO_COMM_MAX_FRAME` 改为 `136`，完整参数包仍按一包读写。V2 Flash 参数因版本和长度不同会失效，固件自动回退 V3 默认参数。 |
| 防错措施 | `servo_params.c` 新增编译期断言，防止后续字段继续增加导致 `ServoParams` 超过一页 Flash。 |
| 预留说明 | `lock_gain_q8` 和 `counter_emf_gain_q8` 本次只作为一等参数放入结构体和校验，尚未参与 HOLD 锁力和反压/反刹算法；下一步控制状态机重构时接入。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17752 B`、`data=168 B`、`bss=1648 B`，总 `19568 B`。 |
| HEX/ELF SHA256 | HEX `c7623f10a2b8a101df1ecc69af0009554d72cdd39c6c1f183b74361ce7c05898`；ELF `1a0cfca2ed77fd05b646b2a21f10b629f7da1451ce2421798a977dfd50210a6c`。 |
| 下载结果 | 本次未下载到仿真板；当前板上仍是上一版稳定高端尾段抑制固件。 |

2026-06-14 V3 控制架构第一版重写下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请开始V3新控制架构重写工作” |
| 关键判断 | 当前实物调试固件走的是 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST` 下的 P50 控制环，也就是 `src/app_entry.c` 的 `lcdm_servo_loop_update_fast()`；只改 `servo_control.c` 不会影响现在板上的 P50 调试效果。 |
| 修改内容 | 重写 P50 控制环为 V3 状态机第一版：`ACCEL/TRACK/BRAKE/RETURN/HOLD/LOCK`。命令变化先启动轨迹规划；接近目标按速度预测提前进入 BRAKE；过点或刹停不足时进入受限 RETURN；HOLD 后 PWM 未变的小漂移只刹车，大偏移才进入 LOCK 反向锁止。 |
| 参数接入 | `lock_gain_q8` 已用于 HOLD 外力锁止输出比例；`counter_emf_gain_q8` 已用于过点 RETURN duty 限幅比例。`POT_REVERSE/MOTOR_REVERSE` 参数层已在上一阶段接入，LCDM 临时页仍使用当前 `Inv` 项控制实测方向。 |
| 曲线事件 | 新增 event：`12=V3 DRIVE`、`13=V3 DECEL`、`14=V3 BRAKE`、`15=V3 RETURN`、`16=V3 LOCK`。旧 `2/8/10` 仍保留为 HOLD/报警/安静 HOLD。 |
| 程序量 | 删除多段旧临时末端/过冲/低 duty 分支后，P50/LCDM 调试构建从上一阶段约 `19568 B` 降到 `18944 B`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17136 B`、`data=164 B`、`bss=1644 B`，总 `18944 B`。 |
| HEX/ELF SHA256 | HEX `a05c6b4921c91512b1043c2a0b7eac9698683ecf719c7896bf1c92d870f20342`；ELF `d4007b78442b910fdea84d0c3efc5e07bb5877b32a63e56b7119c66da35abfc4`。 |
| 下载结果 | 未成功下载。已先停止 SWD 实时曲线服务；Ubuntu pyOCD 和 macOS pyOCD 均未返回 `Successfully resumed device`。现象包括 `already open`、`expected DAP_SWJ_CLOCK`、`Operation timed out`，Ubuntu `dmesg` 出现 Parallels xHCI 异常。已尝试 Parallels 软件断开/重连 DAPLink，仍无法稳定打开 commander。需要手动拔插一次 DAPLink 后再重新下载。 |
| 下一步验证点 | 下载后先测 `1000->1500`、`1500->2000`、`1000->2000` 和反向三组。曲线重点看 event 是否按 `12/13/14/15/2` 收敛，最大 `O` 是否下降，以及 HOLD 后是否仍有嗞嗞声。 |

2026-06-14 V3 控制架构第一版拔插后重试下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已拨插，请重试” |
| 重试前处理 | 已停止 `com.py32servo.curve.live`，确认 macOS 无 `pyocd/live_servo/openocd` 占用进程。Ubuntu 侧可枚举 `0d28:0204 NXP ARM mbed`。 |
| DAPLink 问题定位 | 拔插后仍先后出现 `already open`、`expected DAP_SWJ_CLOCK`。进一步打开 pyOCD debug 日志确认：DAPLink 经 Parallels 的 HID 通讯会在临时 DAP_INFO 板卡探测后残留一包响应，导致正式会话响应错位；同时会出现全 0 空包。 |
| 工具链临时补丁 | 在 Ubuntu `/opt/py32-venv` 的 pyOCD 内做本地兼容补丁：临时打开失败时强制关闭 HID interface；禁用 DAPLink board-name probing；HID 读线程跳过全 0 空包；`MAX_PACKET_SIZE/CAPABILITIES` 异常时使用 64 字节包长和 SWD fallback。该补丁只影响 Ubuntu 下载工具，不改舵机固件。 |
| 固件校验 | 本地和 Ubuntu 待烧录文件 SHA256 一致：HEX `a05c6b4921c91512b1043c2a0b7eac9698683ecf719c7896bf1c92d870f20342`；ELF `d4007b78442b910fdea84d0c3efc5e07bb5877b32a63e56b7119c66da35abfc4`。构建大小 `text=17136 B`、`data=164 B`、`bss=1644 B`，总 `18944 B`。 |
| 下载结果 | Ubuntu pyOCD 下载成功。启动向量 `SP=0x20000c00`、`PC=0x08004021`，执行 `load build/py32f002_servo.hex` 后写入 `sp/pc` 并 `go`，返回 `Successfully resumed device`。 |
| 当前板上版本 | 已是 V3 控制架构第一版，P50 控制环事件包含 `12=V3 DRIVE`、`13=V3 DECEL`、`14=V3 BRAKE`、`15=V3 RETURN`、`16=V3 LOCK`。 |
| 实时曲线状态 | 下载后尝试重启 `com.py32servo.curve.live`，HTML 未自动开新窗口；但 CSV 末尾连续为全 0，判断 live sampler 当前未读到有效 V3 遥测数据。为避免占用 SWD，已停止实时曲线服务；后续需要看曲线时先修采样器再启动。 |

2026-06-14 V3.2 落点稳定和短脉冲修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “会嗞嗞声，来回摆，每次停也是有偏差，不准” |
| 问题判断 | V3 第一版到点附近仍使用连续 `RETURN/LOCK` 输出，`RETURN_MIN_DUTY=120` 和静态锁止最小 duty 偏高，容易在目标点附近连续正反拉回，形成嗞嗞声和来回摆；`MOTION_HOLD_BAND=24 count` 也会让停点允许偏差过大。 |
| 修改内容 | 把过点/停短后的连续回拉改为 V3.2 落点流程：先刹车等待速度下降，再用低 duty 短脉冲微调，脉冲后继续刹车间隔；不再在目标附近连续输出回拉 duty。 |
| 参数变化 | 真正到位带从 `24 count` 收紧到 `10 count`，到位速度从 `20 q4` 收紧到 `12 q4`；静止 HOLD 释放带从 `24 count` 放宽到 `44 count`，连续确认从 `6` 次增加到 `12` 次，减少 ADC/机械回弹引起的误触发。 |
| duty 变化 | 静态锁止最小 duty 从 `220` 降到 `160`，HOLD 外力锁止最小 duty 从 `420` 降到 `320`；落点微调 duty 改为 `65 + error*3`，最高 `155`，并按 `2` 次输出、`7` 次刹车间隔执行。 |
| 曲线事件 | 新增 `17=V3 SETTLE` 表示落点刹车等待/脉冲间隔，`18=V3 TRIM` 表示低 duty 短脉冲微调。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17308 B`、`data=164 B`、`bss=1644 B`，总 `19116 B`。 |
| HEX/ELF SHA256 | HEX `f9cea23b30bee9b9c5c006e870de262e25ecb4f129b386c5ee36ffe8406114e3`；ELF `0ea0a49e93b2dfb9d1e30fa07f6ca58a03d5dcb1d6837edb5da38fbb3ec218d2`。 |
| 下载结果 | 下载前已停止实时曲线服务并确认无 `pyocd/live_servo/openocd` 占用。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080040c9`，返回 `Successfully resumed device`。 |

2026-06-14 V3.2 作废并退回 V3 第一版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “退回上一版再重新分析吧，这一版本走不到位，角度都没转够就停下来了。” |
| 问题结论 | V3.2 的“刹车等待 + 低 duty 短脉冲微调”进入过早，并且等待时间和微调 duty 太保守，导致目标尚未到位就进入静止/刹车流程，实际角度不够。该方向本次作废，不继续叠加修改。 |
| 退回内容 | 手工反向恢复 `src/app_entry.c` 中 V3.2 改过的控制常数和 RETURN 分支；删除 `17=SETTLE`、`18=TRIM` 曲线事件说明。 |
| 退回后校验 | 重新构建后大小回到 V3 第一版：`text=17136 B`、`data=164 B`、`bss=1644 B`，总 `18944 B`。HEX/ELF SHA256 回到 V3 第一版：HEX `a05c6b4921c91512b1043c2a0b7eac9698683ecf719c7896bf1c92d870f20342`；ELF `d4007b78442b910fdea84d0c3efc5e07bb5877b32a63e56b7119c66da35abfc4`。 |
| 下载结果 | 已停止实时曲线服务并确认无 SWD 占用。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004021`，返回 `Successfully resumed device`。当前板上已退回 V3 第一版。 |
| 重新分析方向 | 下一步不再采用过早停机短脉冲方案。应基于 V3 第一版保留足够行程和动力，重点分析“为什么到点后还摆和嗞嗞”：需要区分目标未到、已过点、机械回弹、ADC估算速度滞后、H桥刹车模式是否有效，再决定是调整速度估算/刹车提前量，还是修改 HOLD 后锁止策略。 |

2026-06-14 V3.3 尾段连续性修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “无论正反转，最后一小段是像慢走过去的，也就是停顿一下再转到点，同时会晃，是否算法架构还是不对？” / “也就是说连续性不好，最后一小段里。” |
| 问题判断 | 用户判断正确，现象来自架构里的硬切换：V3 第一版提前 BRAKE 窗口最小 `150 count`，速度越快窗口越大，容易在目标前先刹停；进入 `terminal_band` 后又把 `R` 直接压到 `56/64/72 q4`，并把前馈降为三分之一，所以尾段会出现“停顿一下，再慢慢走过去”。 |
| 修改内容 | 保留 V3 第一版大行程动力和 RETURN 能力，但取消大距离提前纯刹车。提前 BRAKE 只作为近目标高速保护：`MIN_BRAKE_BAND 150 -> 32`、`BRAKE_GAIN 5 -> 1`、`BRAKE_MAX 560 -> 96`，并新增 `BRAKE_MIN_VEL_Q4=96`，速度不够高时不刹停。 |
| 状态流变化 | 如果处在 `BRAKE` 状态但还没有过目标，且速度已经降下来，不再进入 `RETURN` 慢爬，而是回到 `TRACK` 速度环继续连续逼近目标。`RETURN` 只保留给真正过点后的受限回拉。 |
| 尾段速度曲线 | `terminal_band` 内不再把 `R` 硬压成固定低速，而是按剩余误差连续插值：在末端窗口边缘接近 `max_vel_q4=416`，越接近目标越降到 `terminal_fine_vref`。前馈只在 very fine 区降到一半，不再整个 terminal 区直接降到三分之一。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17164 B`、`data=164 B`、`bss=1644 B`，总 `18972 B`。 |
| HEX/ELF SHA256 | HEX `ad0d3027133f74879b0454ba6ca23890c42c09a382b9416e3c85193978f1582c`；ELF `a044db03ee5f1e2d42c99f9ca3f74f39840773b447b573973cd005d7130af246`。 |
| 下载结果 | 已停止实时曲线服务并确认无 SWD 占用。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800403d`，返回 `Successfully resumed device`。 |
| 验证重点 | 看最后一小段是否还会先停顿再慢走；P50 第 5 行 `R` 应该连续下降，而不是突然变很小。若连续性改善但过冲变大，再只调近目标高速刹车阈值，不再恢复大范围提前刹车。 |

2026-06-14 V3.4 回摆阻尼修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “连续性较好，现在是来回摆动几下才停的问题了” |
| 问题判断 | V3.3 已改善尾段连续性，剩余问题集中在过目标点后的 `RETURN`：过点后仍按固定较强最低 duty 连续反向回拉，容易拉过目标再形成第二、第三次回摆。 |
| 修改内容 | 不改 V3.3 的 TRACK/terminal 连续逼近曲线，只修改 `RETURN` 分支。新增 `RETURN_DAMP`：当已经在回目标方向运动、剩余误差进入按速度计算的阻尼窗口时，H 桥先刹车吸能，不继续反向加力。若刹停后仍未到位，仍保留 RETURN 小力补回，避免 V3.2 那种走不到位。 |
| 参数变化 | `RETURN_MIN_DUTY 120 -> 105`，`RETURN_NEAR_DUTY 160 -> 135`；新增 `RETURN_DAMP_BASE_BAND=18`、`RETURN_DAMP_VEL_DIV=3`、`RETURN_DAMP_MAX_BAND=92`、`RETURN_DAMP_MIN_VEL_Q4=24`。 |
| 曲线事件 | 新增 `17=V3 RETURN_DAMP`，表示过点回目标途中近目标速度吸能刹车。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17268 B`、`data=164 B`、`bss=1644 B`，总 `19076 B`。 |
| HEX/ELF SHA256 | HEX `722faadc5dcf5c6ca93de805e1ee145b8ee2609466ca645c62b2181e3bd585f2`；ELF `a175c3910257e923bae29b75e31c1ebb228b5c05716905f561949bf7656fddae`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080040a5`，返回 `Successfully resumed device`。 |
| 验证重点 | 测 `1000/1500/2000` 三点来回切换，观察是否从“来回摆几下”变成“最多一次回摆后停”。若出现停短，下一步只放宽 `RETURN_DAMP` 窗口或略加 RETURN near duty，不恢复大范围提前刹车。 |

2026-06-14 V3.5 过点反打抑制修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “无明显改善，可能只有小许改进” |
| 问题判断 | V3.4 只在已经回目标方向时做近点阻尼，改善有限，说明主要能量来自刚过目标点时的处理。代码里还有一个更关键的问题：`lcdm_servo_apply_slew()` 只限制同方向 duty 增大，正转到反转/反转到正转时可能一拍直接翻转输出，容易在过点后形成反打和二次回摆。 |
| 修改内容 | 增加 signed duty 翻转限斜率：当前 duty 未回到 0 前，不允许直接输出反方向 duty。这样过目标点后先降到 0/刹车，再进入反向补偿。 |
| 过点处理 | 新增 `CROSS_BRAKE`：如果刚过目标点且速度仍沿原运动方向向外冲，在 `220 count` 以内先 H 桥刹车吸能，不立刻给反向 RETURN duty。 |
| 参数变化 | 新增 `CROSS_BRAKE_MAX_BAND=220`、`CROSS_BRAKE_MIN_VEL_Q4=48`。保留 V3.4 的 RETURN near duty 和 RETURN_DAMP 设置。 |
| 曲线事件 | 新增 `18=V3 CROSS_BRAKE`，表示刚过目标点仍向外冲时的吸能刹车。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17412 B`、`data=164 B`、`bss=1644 B`，总 `19220 B`。 |
| HEX/ELF SHA256 | HEX `c4e535754578c383ac07ddba73a0b666aa5a5595bfcbb9af1ad88e6318ec5caa`；ELF `da68b265d8a27dce79d671cff9ac6b5703dd41213513ea0f418e4c52964483ba`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004135`，返回 `Successfully resumed device`。 |
| 验证重点 | 若这版有效，应看到过点后的第二、第三次回摆明显减少；如果动作变软或停短，说明 `CROSS_BRAKE_MAX_BAND` 或 duty 翻转斜率过保守，下一步只缩小刹车窗口或提高翻转斜率。 |

2026-06-14 V3.6 运动反馈估算加速和事件显示下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “无改善” |
| 问题判断 | V3.4/V3.5 分别加强 RETURN 阻尼和过点刹车，但用户反馈无改善，说明单纯改变过点后的分支不是主因。更可能是控制看到反馈位置/速度太晚，或 H 桥 Brake 实际制动力弱。当前 P50 又没有直接显示事件号，无法确认回摆时是否进入 `13/14/15/17/18`。 |
| 修改内容 | 不再继续堆 RETURN 分支。把 P50 控制环的 alpha-beta 反馈估算改成运动自适应：静止仍用位置残差 `/3`、速度残差 `/12`；运动、刹车、回拉或残差超过 `8 ADC count` 时改为位置残差 `/2`、速度残差 `/7`，让过点和速度变化更早被控制环看到。 |
| 显示变化 | P50 第 5 行增加事件号显示，格式类似 `D+320 V+8 e13` 或 `BRK V+6 e18`。`e13=DECEL`、`e14=BRAKE`、`e15=RETURN`、`e17=RETURN_DAMP`、`e18=CROSS_BRAKE`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17480 B`、`data=168 B`、`bss=1648 B`，总 `19296 B`。 |
| HEX/ELF SHA256 | HEX `2e2590aa369559760a61055e2745b6a0374f73e823ae33a2b30c97679e6fc188`；ELF `d44a8bcde3c540c0e6a02284e244a748bdcad8ca365e6e27f69c063de68d379a`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004179`，返回 `Successfully resumed device`。 |
| 验证重点 | 观察回摆时 P50 第 5 行的 `e` 值。如果主要是 `e13`，说明速度环/末端曲线仍在驱动；如果是 `e14/e18` 但仍不减摆，说明 H 桥 Brake 组合或制动力需要硬件级确认；如果是 `e15`，说明 RETURN 回拉仍过强。 |

2026-06-14 V3.7 末端减速触发条件修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “e值最大12” |
| 问题判断 | 这是关键证据。`e12` 是 V3 DRIVE，说明回摆时基本没有进入 `e13` 末端减速、`e14/e18` 刹车或 `e15` 回拉。前几版改 RETURN 和刹车无明显改善，是因为实际运行路径没有触发这些分支。 |
| 根因定位 | V3.3/V3.6 的末端减速和预测刹车都依赖 `traj_at_cmd`，也就是轨迹规划器完全到达命令目标后才允许介入。实物反馈已经接近目标甚至开始回摆时，轨迹变量可能仍未满足这个条件，导致控制一直保持 `e12 DRIVE`。 |
| 修改内容 | 去掉末端控制对 `traj_at_cmd` 的硬依赖。现在只要实际反馈误差进入 terminal 区，就允许 `e13` 限速；只要实际反馈还朝目标运动且进入速度相关 brake 区，就允许 `e14` 预测刹车。 |
| 保留内容 | 保留 V3.6 的运动快速反馈估算和 P50 事件号显示；保留 V3.5 的 duty 翻转限斜率和 CROSS_BRAKE；不新增复杂状态。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17476 B`、`data=168 B`、`bss=1648 B`，总 `19292 B`。 |
| HEX/ELF SHA256 | HEX `c40020987df1d357a00677ce3f4112be5b287fa6d73379e2bb3c6500811dbc76`；ELF `dd0aa5557dcd7366dc6c025bff7d96ce86121d17cea175a556d2382a19c9be36`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004175`，返回 `Successfully resumed device`。 |
| 验证重点 | 再看 P50 第 5 行 `e` 值：如果现在能看到 `e13/e14`，说明末端控制真正介入；若仍只有 `e12`，下一步要查 `abs_error/terminal_band` 和 P50 刷新是否漏掉瞬态；若看到 `e14` 但回摆仍大，则要确认 H 桥 `Brake()` 组合是否真实强刹车。 |

2026-06-14 V3.8 预测刹车提前修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有看到e13” |
| 问题判断 | `e13` 出现说明 V3.7 已经让末端减速真正介入。若仍有回摆，说明单纯限速还不够，需要让 `e14` 预测刹车更早介入吸收惯性。 |
| 修改内容 | 调整 V3 brake 触发参数：`MIN_BRAKE_BAND 48`、`BRAKE_VEL_GAIN 2`、`BRAKE_MAX_BAND 180`、`BRAKE_MIN_VEL_Q4 56`。相比 V3.7，较低速度和更大误差范围内也允许 `e14`。 |
| 保留内容 | 保留 V3.7 去掉 `traj_at_cmd` 硬依赖的修改；保留 P50 事件号显示。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17476 B`、`data=168 B`、`bss=1648 B`，总 `19292 B`。 |
| HEX/ELF SHA256 | HEX `5a96b5d2807714848d9f91ae0aa45fe79f5c64ecce70b60fec74e11f3b022c56`；ELF `76c948417186330ed8a60fdd91c6121da11b9fe1ed4dfe334f7ec93d1300e0d3`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004175`，返回 `Successfully resumed device`。 |
| 验证重点 | 看回摆时是否能看到 `e14`。若 `e14` 出现且回摆改善，继续微调 brake band；若 `e14` 出现但仍不刹，优先核对 H 桥 `Brake()` 输出组合。 |

2026-06-14 V3.9 预测刹车连续介入修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “偶尔会出现e14，e16更少” |
| 问题判断 | `e14` 偶尔出现，说明预测刹车路径已接通但触发还不够稳定；`e16` 是 HOLD 后外力锁止事件，运动到点回摆阶段不应主要依赖它，少出现不是当前主因。 |
| 修改内容 | 继续提前并放宽 V3 brake：`MIN_BRAKE_BAND 48 -> 64`、`BRAKE_VEL_GAIN 2 -> 3`、`BRAKE_MAX_BAND 180 -> 260`、`BRAKE_RELEASE_VEL_Q4 48 -> 28`、`BRAKE_MIN_VEL_Q4 56 -> 32`。这样较低速度下也能判定为正在朝目标运动，并允许 `e14` 连续介入。 |
| 保留内容 | 保留 V3.7 的末端控制去 `traj_at_cmd` 硬依赖；保留 V3.8 的事件显示和 brake 路径。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17484 B`、`data=168 B`、`bss=1648 B`，总 `19300 B`。 |
| HEX/ELF SHA256 | HEX `883a08f384ec4d16f0ce22ca92583dfbe5e0e691ac7d27a6da61d3581703fa67`；ELF `77f67262f99b07a7246a32e0969999e195c08faca489fa37ebd5892b8a882c58`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800417d`，返回 `Successfully resumed device`。 |
| 验证重点 | 看 `e14` 是否从偶尔出现变为目标附近连续出现，且回摆是否减少。若 `e14` 明显增多但回摆无改善，下一步不要再调软件刹车窗口，应先核对 `HBridge_Brake()` 对四个 H 桥脚的实际刹车组合。 |

2026-06-14 V3.10 短刹释放修正版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “e14偶尔有，但停止位置有改着呢，不再摆了，但速度变慢许多，这种改进是否有相互牵制或有办法再完善” |
| 问题判断 | 用户判断正确：停止稳定和速度存在牵制。V3.9 通过更早、更久的 `e14` 把惯性压住，因此不摆，但 BRAKE 状态会一直刹到速度很低，造成后半段明显变慢。 |
| 修改内容 | 保留 V3.9 的 `e14` 进入条件，但把“进入刹车”和“持续刹车”分开。新增 `BRAKE_HOLD_VEL_Q4=68`，只有速度仍高于该值时才持续保持 `BRK e14`；速度降到中等后立即释放回 `TRACK/e13`，让舵机继续较快走到目标。 |
| 参数变化 | 新增 `LCDM_SERVO_V3_BRAKE_HOLD_VEL_Q4 68`；`BRAKE_MIN_VEL_Q4=32` 仍用于进入 `e14`，不再用于持续刹到很慢。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17484 B`、`data=168 B`、`bss=1648 B`，总 `19300 B`。 |
| HEX/ELF SHA256 | HEX `ec3d5c966b0b2fa2099027570a7c9b996d906b6c3bdefa6151e9d2a690366feb`；ELF `cbd986aa8872eeb1b69cb56afc3923ec7ce2237b28dfa52abdd8967f2c91170d`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800417d`，返回 `Successfully resumed device`。 |
| 验证重点 | 看速度是否比 V3.9 恢复，同时确认“不再摆”的改善是否保留。若又开始轻微摆，说明 `BRAKE_HOLD_VEL_Q4=68` 太高，需要降到 `56` 左右；若仍慢但稳定，说明还可以继续提高 terminal 速度曲线。 |

2026-06-14 V3.11 上行方向轻微过冲补偿下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前发现从1500-1000或2000-1000就1000这个点是停的比较稳定，从1000-1500的1500点和1000-2000或1500-2000的2000点都会有轻微的过冲回摆动作，不是很明显，是否程序是完全对称的，还是因为高低AD值的变化量影响到控制系数值？” |
| 问题判断 | 程序主控制公式和默认 PWM->mV 映射基本对称，但实物不对称：H 桥两方向导通损耗、马达正反力矩、齿轮受力、电位器线性和高低端机械阻力都会让 `ADC 增加方向` 与 `ADC 减少方向` 的过冲不同。当前反馈明确是低往高走时轻微过冲，往低端 `1000` 点稳定。 |
| 修改内容 | 增加很小的上行方向补偿，只在 `s_lcdm_servo_move_dir > 0` 即目标 ADC 大于当前反馈时生效：terminal 区提前 `32 count`，brake 区提前 `28 count`。下行方向不变，避免破坏已经稳定的 `1500/2000 -> 1000`。 |
| 参数变化 | 新增 `LCDM_SERVO_V3_UP_TERMINAL_EXTRA=32`、`LCDM_SERVO_V3_UP_BRAKE_EXTRA=28`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17504 B`、`data=168 B`、`bss=1648 B`，总 `19320 B`。 |
| HEX/ELF SHA256 | HEX `2635d37b441bf20bc1a1a149a8e65e8984b3c1d0d68c71b82e318ecd53ee5571`；ELF `8a7bddb0ddb9e617ba3b1f72c9f78991e14c4f0b72d54c489dde236e23668ede`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004191`，返回 `Successfully resumed device`。 |
| 验证重点 | 对比 `1000->1500`、`1000->2000`、`1500->2000` 是否减少轻微回摆，同时确认 `1500->1000`、`2000->1000` 没有变慢或停短。 |

2026-06-14 V3.12 对称控制基线诊断版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我认为高端补偿没什么用，主要还是检查刚开始从低往上速度可以，多转几次后，低往上逐渐变慢，而从高往下一直都不会变慢，是什么原因？” |
| 问题判断 | 用户反馈重点不是高端位置补偿，而是低往高方向多次动作后逐渐变慢。代码检查后，P50 当前控制路径没有明显只对低往高方向累积的积分量；目标变化时会重置 `phase/move_dir/traj/motion_start_error` 等主要运动状态。若对称基线仍复现，优先怀疑单方向 H 桥/马达/供电热效应，或控制环实际进入 `e13/e14` 导致软件提前减速。 |
| 修改内容 | 取消 V3.11 的上行方向补偿，移除只对 `s_lcdm_servo_move_dir > 0` 生效的 terminal/brake 额外提前量，恢复 V3.10 的对称控制参数作为诊断基线。 |
| 当前关键参数 | `MIN_BRAKE_BAND=64`、`BRAKE_VEL_GAIN=3`、`BRAKE_MAX_BAND=260`、`BRAKE_RELEASE_VEL_Q4=28`、`BRAKE_MIN_VEL_Q4=32`、`BRAKE_HOLD_VEL_Q4=68`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17484 B`、`data=168 B`、`bss=1648 B`，总 `19300 B`。 |
| HEX/ELF SHA256 | HEX `ec3d5c966b0b2fa2099027570a7c9b996d906b6c3bdefa6151e9d2a690366feb`；ELF `cbd986aa8872eeb1b69cb56afc3923ec7ce2237b28dfa52abdd8967f2c91170d`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800417d`，返回 `Successfully resumed device`。 |
| 验证重点 | 复测低往高多次动作变慢时，观察 P50 第 5 行 `D/V/e`：如果 `D+` 已经很大但 `V` 变小，偏硬件单方向力矩/供电/热效应；如果主要显示 `e13/e14` 且 duty 很小，偏软件提前减速/刹车；如果 `D` 本身逐次变小，则继续查控制状态残留或参数刷新路径。 |
建议不要马上改控制输出，而是按下面顺序整新，否则又会陷入“改一点、试一下、越改越乱”。

  第一步：先定新架构
  把控制程序拆成 6 个清楚层级：

  1. 输入层
     PWM 捕获、失信号、输入滤波、输入范围。
  2. 映射层
     PWM us -> 舵机角度 -> ADC目标值。
     这里处理 500-2500us、1000-2000us、90度/180度、电位器 220度、低/中/高三点校准。
  3. 运动规划层
     目标变化后，不直接追 ADC 目标，而是生成轨迹：加速、匀速、减速、尾段慢速。
  4. 速度环层
     用 ADC 变化量计算实际速度，判断马达是否够力、是否接近目标、是否需要提前刹车。
  5. 输出层
     根据位置误差、速度误差、锁力、反压系数，输出 H 桥 duty。
  6. HOLD/保护层
     到位后进入锚点 HOLD；PWM 不变时不继续追理论目标。只有外力超过齿隙/噪声阈值才反向锁住。堵转、失信号也在这一层处
     理。

  第二步：先扩展参数，不急着调算法
  新增 ServoParams v3，至少先补这些核心参数：

  - servo_angle_deg
  - pot_angle_deg
  - low_trim_count
  - mid_trim_count
  - high_trim_count
  - pot_reverse
  - motor_reverse
  - motion_max_speed
  - motion_accel
  - motion_decel
  - brake_zone_count
  - reverse_brake_gain
  - counter_emf_gain
  - lock_gain
  - lock_min_duty
  - lock_max_duty
  - backlash_count
  - stall_output_mode
  - lost_signal_mode

  先把参数体系搭好，后面调机才有抓手。

  第三步：增强曲线数据
  现在曲线已经有基础数据，但还不够。下一版应增加一个 state_detail/event_detail，让图上能直接看到当前处于：

  - 加速
  - 匀速
  - 减速
  - 纯刹车
  - 反向刹车
  - 到位 HOLD
  - 外力锁止
  - 堵转保护

  这样每次你说“晃”“反刹太早”“高端不到位”，我们能从图上直接判断是哪一段逻辑错了。

  第四步：重写控制状态机
  控制主流程按这个来：

  PWM变化
    -> 生成目标ADC
    -> 建立运动轨迹
    -> 加速/匀速
    -> 按剩余距离和当前ADC速度提前减速
    -> 进入刹车区
    -> 必要时短反刹
    -> 到位后记录HOLD锚点
    -> PWM不变时只锁锚点，不追理论目标
    -> 外力拨动超过阈值才反向锁力

  这个才符合成熟舵机逻辑。

  第五步：最后才开始调参数
  调试顺序建议固定：

  1. 校准低/中/高 ADC 点。
  2. 确认 1000/1500/2000us 三点重复一致。
  3. 调空载速度。
  4. 调过冲和刹车。
  5. 调 HOLD 嗞嗞声。
  6. 调外力锁止力。
  7. 调堵转保护。
  8. 调失信号策略。
  9. 做参数镜像模板。

  我建议下一步先做两件事：

  1. 先把 ServoParams v3 参数表和文档建出来。
  2. 再改曲线数据，增加 state_detail，让后续重写控制状态机时可以看清每一段动作。

2026-06-14 五步计划当前进度复核记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “机械和驱动发热问题我先留着后面查找。现在还有二个问题要解决，一个是嗞嗞声，一个是速度不够快。这二个问题如果是可以调之前讨论的参数的话，那也可以留到后面去解决。现在接下来要做第几步？之前我们有定了五步计划。” |
| 当前判断 | 嗞嗞声和速度都属于第五步调参数里的项目，但现在不宜直接进入最终调参，因为控制状态机虽然已重写到 V3.12，关键参数仍有一部分写死在 P50 临时代码中，LCDM/参数表还不能完整调这些值。 |
| 五步进度 | 第一步新架构已定义；第二步 ServoParams v3 已部分完成；第三步曲线数据已增强到 event/D/V/AD/H桥曲线，但还缺可长期使用的完整 state_detail 和参数对照；第四步 V3 状态机已运行并调到可用基线；第五步正式调参数尚未完全开始。 |
| 下一步建议 | 先做第四步收尾和第五步准备：把 V3.12 里影响嗞嗞声、速度、刹车、锁力的硬编码常数参数化，接到 ServoParams/LCDM 调参入口，并在曲线图上显示这些关键参数当前值。完成后再按第五步顺序调：三点一致性 -> 空载速度 -> 过冲刹车 -> HOLD 嗞嗞声 -> 锁止力。 |
| 本次固件动作 | 本次只复核计划并记录，不改固件、不构建、不下载。 |

2026-06-15 第四步收尾/第五步准备：V3.12 LCDM 运行时参数化记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请开始继续第四步和第五步，因今天没有带DPLINK，所以先自行检查核对，不下载。” |
| 执行原则 | 今天不烧录、不声称板上已更新。只做源码参数化、文档记录和 Ubuntu 构建检查。 |
| 问题判断 | 嗞嗞声和速度不够快都属于第五步调参问题，但 V3.12 仍有多项关键值写死在 P50 控制代码中，直接继续改数字会再次变成反复试错。先把这些值放到 LCDM 运行时调参页，后续带 DAPLink 后可以在屏上调整并配合曲线观察。 |
| LCDM 页面变化 | `P30` 改为运动参数：`Vmax/Acc/FF/Kv/Slew`；原 `P13` 改为 `P31` 刹车参数：`Bmin/Bg/Bmax/Bv/Bhd`；`P70` 改为 HOLD/锁力参数：`Hbd/Hvel/Rel/Lmin/Lmax`。 |
| 控制代码变化 | P50 快速控制环现在从 LCDM 参数读取轨迹最大速度、轨迹加速度、运动前馈、速度环增益、H 桥输出斜率、预测刹车窗口、刹车保持速度、HOLD 到位带、HOLD 速度阈值、HOLD 释放带和外力锁止 duty 上下限。旧宏只作为默认值和回退值保留。 |
| 暂不改内容 | 未扩展 `ServoParams` Flash 结构。原因是 v3 当前结构已为 `128 B`，正好占满最后一页参数区；若继续加入字段，需要同步升级协议/参数页设计，不能在今天的“只检查不下载”中仓促处理。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17624 B`、`data=288 B`、`bss=1648 B`，总 `19560 B`。 |
| HEX/ELF SHA256 | HEX `121ef2b836fc80e0ff9d03857879c93a93834c6c7e8ff12301106a3a6e7b6313`；ELF `66763b1810ded86733b7304b114a084c625573dd55ffcc4c18fa15aee31bd64a`。 |
| 下载结果 | 按用户要求，今天没有 DAPLink，不下载。当前板上仍是上一版已下载的 V3.12 对称控制基线；本条记录对应的是本地已构建但未上板的参数化准备版。 |
| 下次验证重点 | 带 DAPLink 后先下载此版，再只小幅调 P30/P31/P70。建议先保持默认值验证行为等同 V3.12，再按第五步顺序调：三点一致性 -> 空载速度 -> 过冲刹车 -> HOLD 嗞嗞声 -> 外力锁止力。 |

2026-06-15 V3 架构对照成熟调机模板和五步计划自主审查记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “可以自主再检查一下，目前的架构是否完全按之前提供的图片和规划的计划五步进行的，是否存在哪些漏洞？在没调试之前好好检查检查。” |
| 总体结论 | 当前主方向符合五步计划：已有输入层、映射层、轨迹/速度环、输出层、HOLD/保护层雏形，并已把部分 V3.12 关键调参项接入 LCDM。但还没有完全达到成熟调机模板的产品化闭环，主要缺口在生产校准、参数持久化、完整保护、齿隙/虚位、曲线状态细分和 PA4 配置物理层。 |
| 已符合部分 | P50 当前按 `PWM -> 目标ADC -> 轨迹 -> 速度估算 -> H桥 duty -> HOLD/LOCK` 工作；P30/P31/P70 已能运行时调整速度、刹车、HOLD 和锁力；曲线已有 `target/feedback/raw/traj/duty/velocity/ref/event/O`；ADC 丢失会关闭 H 桥。 |
| 主要漏洞 1 | P20/P30/P31/P70 目前是 LCDM 运行时 RAM 参数，不会保存到 SOP8 `ServoParams` Flash。断电、重刷或切到最终目标板后会丢失。V3 参数结构已满 `128 B`，后续若要保存这些参数，需要设计 V4 或扩展参数页。 |
| 主要漏洞 2 | P50 调试路径和正式 `servo_control.c` 参数路径仍是两套控制逻辑。P50 使用 `s_lcdm_*` 临时参数和 mV 映射；正式控制使用 `ServoParams` 的 ADC/脉宽/保护字段。调试成熟后必须合并或迁移，否则调出来的 P50 行为不能自动等于最终 SOP8 固件行为。 |
| 主要漏洞 3 | 几何校准还没闭合。成熟模板要求 `PWM us -> 舵机角度 -> ADC`，并支持低/中/高当前点写入；当前 P50 仍主要用 `Lo/Md/Hi mV` 和固定 3300mV 换算 ADC，没有真正用 `servo_angle_deg/pot_angle_deg/adc_min/center/max` 做统一角度映射。 |
| 主要漏洞 4 | 齿隙/虚位没有独立建模。当前 HOLD 释放带、HOLD band、方向报警等会间接覆盖虚位问题，但没有 `backlash_count/hysteresis/reversal compensation`。这会影响反向死区、回中一致性和“从不同方向到同一点”的偏差分析。 |
| 主要漏洞 5 | 保护链路不完整。P50 有 ADC LOST、PWM 超范围 lock/release、方向报警，但堵转保护、失信号多模式、堵转后限力/间歇输出还主要存在正式 `servo_control.c` 或参数文档中，P50 V3 快速环没有完整接入。 |
| 主要漏洞 6 | 曲线事件还不够细。现在 `e12/e13/e14/e15/e16/e17/e18` 能看大状态，但不能直接区分加速、匀速、减速、纯刹、反向回拉、HOLD 锚点等待、外力锁止确认、堵转保护等细状态。调参数时仍可能误判。 |
| 主要漏洞 7 | H 桥 Brake 组合仍是软件假设。若实际驱动芯片的 Brake/Coast/Forward/Reverse 真值表与当前 GPIO 组合不完全一致，过冲和嗞嗞声会被误判为算法问题。正式调刹车前应单独确认四脚组合。 |
| 代码风险 | `lcdm_servo_apply_hold_output()` 仍引用旧的 `s_lcdm_reg_params[5]`，但 P30 已改为 5 个参数 `0..4`。该函数目前未使用，所以不影响构建和当前运行；但若以后重新调用会越界，后续应删除或改成新 P70/P30 参数。 |
| 调试前建议 | 下一次带 DAPLink 后，先不要大幅调参。先下载参数化准备版，默认参数跑一次确认等同 V3.12；然后先修/确认以上架构风险中最关键的三项：P50 与最终 ServoParams 的参数迁移方案、H 桥 Brake 真值表、曲线 event_detail/state_detail。再按第五步调速度、过冲、嗞嗞声和锁力。 |
| 本次固件动作 | 本次只做架构审查和记录，不改固件、不构建、不下载。 |

2026-06-16 V3.12 LCDM 参数化准备版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “今天继续，已接PADLINK” |
| 下载前修正 | 删除未使用的旧函数 `lcdm_servo_apply_hold_output()`，该函数仍引用旧的 `s_lcdm_reg_params[5]`，而 P30 已改为 5 个参数 `0..4`。函数当前未被调用，删除后不改变生成镜像，只消除后续误调用越界风险。 |
| 本版内容 | 下载的是 2026-06-15 已构建的 V3.12 LCDM 运行时参数化准备版：P30=`Vmax/Acc/FF/Kv/Slew`，P31=`Bmin/Bg/Bmax/Bv/Bhd`，P70=`Hbd/Hvel/Rel/Lmin/Lmax`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=17624 B`、`data=288 B`、`bss=1648 B`，总 `19560 B`。 |
| HEX/ELF SHA256 | HEX `121ef2b836fc80e0ff9d03857879c93a93834c6c7e8ff12301106a3a6e7b6313`；ELF `66763b1810ded86733b7304b114a084c625573dd55ffcc4c18fa15aee31bd64a`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 通过 DAPLink 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080041f5`，返回 `Successfully resumed device`。 |
| 验证重点 | 先不要改参数，确认默认参数行为是否接近 V3.12 对称基线；P50/P30/P31/P70 是否能正常显示和触摸；再按第五步顺序调：三点一致性 -> 空载速度 -> 过冲刹车 -> HOLD 嗞嗞声 -> 外力锁止力。 |

2026-06-16 恢复 LCDM 参数页入口下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “你可能忘记了之前有屏蔽P30等只留P50界面，请确认并恢复” / “继续” |
| 问题确认 | 确认存在问题。`run_lcdm_runtime_ui_test()` 启动后直接 `lcdm_draw_p50(0U)`，读到触摸后只丢弃 `touch`，没有调用页面状态机，所以实际只剩 P50，无法进入 P30/P31/P70。 |
| 第一次恢复尝试 | 直接恢复完整 `lcdm_draw_current()` 和 `lcdm_handle_touch()` 会把所有旧页面重新链接进固件，Flash 溢出 `1808 B`，不能使用。 |
| 最终修改 | 增加轻量导航：开机仍进 P50；P50 底部左/中进入菜单，右进入 P40；菜单只恢复当前需要的 P31/P30/P40/P50/P70；参数页底部左/右调小调大，中间返回菜单。只有在 P50 页面才刷新实时 PWM/ADC/HBR 数值，后台电机控制持续运行。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19244 B`、`data=672 B`、`bss=1648 B`，总 `21564 B`。该版接近 20KB Flash 上限，但链接通过。 |
| HEX/ELF SHA256 | HEX `ca1da74e3579f37da42b79c1b7c20ee36fd4a864361d799ad7a04698b3f0d08a`；ELF `810a9e6da215632b5f2fd6198ef2077b3e2961903211249a652748eff2af5fd3`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 通过 DAPLink 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004789`，返回 `Successfully resumed device`。 |
| 验证重点 | 先确认 P50 底部能进入菜单；菜单中 P31/P30/P40/P50/P70 能进入；P30/P31/P70 参数左右调整能立即改变数值；再回 P50 确认实时显示和电机控制仍正常。 |


记录重新打V3架构事宜

2026-06-16 恢复 LCDM 参数页入口复核重下记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “继续” |
| 本次判断 | 延续上一条“恢复 LCDM 参数页入口”问题，不开启新问题。复核当前 `run_lcdm_runtime_ui_test()` 已走轻量页面状态机，P50 底部可进菜单，菜单只开放 P31/P30/P40/P50/P70，后台电机控制继续运行。 |
| 本次代码动作 | 未修改源码。只复核当前代码、重新构建、重新下载，确保板上运行的是恢复参数页入口后的同一镜像。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19244 B`、`data=672 B`、`bss=1648 B`，总 `21564 B`。 |
| HEX/ELF SHA256 | HEX `ca1da74e3579f37da42b79c1b7c20ee36fd4a864361d799ad7a04698b3f0d08a`；ELF `810a9e6da215632b5f2fd6198ef2077b3e2961903211249a652748eff2af5fd3`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 通过 DAPLink 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004789`，返回 `Successfully resumed device`。 |
| 板上验证重点 | 先测 P50 底部左/中是否进入菜单；菜单是否能进入 P31/P30/P40/P50/P70；P30/P31/P70 触摸左右调参是否有效；回 P50 后实时 PWM/ADC/HBR 和电机控制是否仍工作。 |

2026-06-16 LCDM 参数页触摸闪屏改良下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已验证如下，请继续下一步……但在点参数时可闪屏，请改良” |
| 问题判断 | P31/P70 和默认动作已验证，当前问题集中在参数页触摸时整页闪屏。原因是轻量 runtime 触摸处理每次参数加减或选中行变化后都返回整页重画，导致 `cls`、标题、所有行和底部按钮重新发送。 |
| 本次修改 | 参数页触摸改为局部刷新：加减参数只重画当前选中行；切换选中行只重画旧行和新行；只有切页/返回菜单才整页刷新。控制算法、默认参数、P30/P31/P70 参数值均未改动。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19372 B`、`data=672 B`、`bss=1648 B`，总 `21692 B`。 |
| HEX/ELF SHA256 | HEX `5b787238d9ea07b111e71bdaa805e1a3b2a781df5fd24f3a6785198859701088`；ELF `4e3c2c19b0c2bfc2b5987df6a3e26b1ea15cca3c9b3bf47b72b130996ac3c43f`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 通过 DAPLink 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004809`，返回 `Successfully resumed device`。 |
| 验证重点 | 在 P30/P31/P70 连续点选行、点左/右调参数，观察是否不再整页闪屏；确认返回菜单和切页仍正常；确认 P50 实时显示和马达动作未受影响。 |

2026-06-16 第五步调参基线采样启动记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不闪屏了，请进行下一步” |
| 当前判断 | LCDM 参数页触摸闪屏已解决，P31/P70 和默认动作已验证，进入第五步调参。第五步不先盲目改参数，而是先采集 1000/1500/2000 三点动作曲线，确认三点一致性、过冲、回摆、HOLD 状态和 H 桥 duty。 |
| 本次代码动作 | 未修改固件、未重新构建、未下载。 |
| 曲线采样动作 | 已启动 `com.py32servo.curve.live`，HTML 路径为 `build/servo_curve_live/servo_curve_live.html`，日志路径为 `build/servo_curve_live/live.log`。 |
| 当前静态基线 | CSV 已正常更新。当前静态点约 `PWM=1508/1509us`、`target_adc=2061`、`feedback_adc=2061-2063`、`error=0~-2`、`duty=0`、`state=1`、`event=2`，说明中点 HOLD 静态基线干净。 |
| 下一步实测指令 | 请按 `1500 -> 1000 -> 1500 -> 2000 -> 1500 -> 1000 -> 2000` 切换外部 PWM，每点停 `1-2s`。完成后根据新 CSV/周期 SVG 判断先调 P31 刹车参数还是 P30 速度参数。 |

2026-06-16 第五步第一次曲线数据作废/参考记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” / “对了，是没有调到1000-2000，是我的错” |
| 数据确认 | 这次 SWD 曲线实际读到的外部 PWM 约为 `701us / 1509us / 2335us`，不是计划中的 `1000us / 1500us / 2000us`。因此不能作为标准三点调参结论。 |
| 可参考现象 | `1509->701`、`701->1509`、`1508->2335`、`2334->1509`、`1508->700`、`701->2335` 等大行程基本能停到目标附近，过冲很小或为 0；说明大行程刹车方向不是当前最差点。 |
| 暂定风险 | 高端小幅 PWM 抖动如 `2336->2338us` 会造成目标 ADC 在 3499/3503 附近来回变化，并持续触发小 duty 正反修正约十几秒，这可能对应高端细抖或嗞声。该问题后续需要通过输入 PWM 死区/目标锁存/静止 HOLD 判定处理。 |
| 下一步 | 保持曲线服务运行，重新按标准三点 `1500 -> 1000 -> 1500 -> 2000 -> 1500 -> 1000 -> 2000` 测一次，每点停 `1-2s`。 |

2026-06-16 标准三点曲线分析与 HOLD 输入小抖动锁定下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “之前在调回1000-2000，前面的记录不采集，现在开始再试” / “已试完” |
| 数据来源 | 只分析 `2026-06-16 16:32:11` 标记后的新周期：`1509->1007`、`1007->1508`、`1508->2010`、`2010->1508`、`1509->1007`、`1007->2012`。 |
| 曲线结论 | 标准三点大动作表现已明显接近可用：`1509->1007` 过冲约 `4 count`、`1007->1508` 过冲约 `9 count`、`1508->2010` 过冲约 `3 count`、`2010->1508` 过冲约 `4 count`、`1007->2012` 过冲约 `0 count`；最终误差约 `1-8 count`。 |
| 发现的问题 | 静止或小幅输入抖动仍会触发动作。典型 `1508/1509/1510us` 会造成目标 ADC `2060/2065` 附近跳变，超过旧 `target_changed > 3 count` 阈值，导致 HOLD 退出并持续出现 `event=13` 小 duty 修正。这是中点或高端细抖/嗞声风险，不应先继续调 P31 大刹车。 |
| 本次修改 | 增加已接受命令脉宽缓存 `s_lcdm_servo_cmd_pulse_us`；当已在 HOLD 且当前反馈仍在 HOLD 释放带内时，若新目标相对上次目标只变化 `<= LCDM_SERVO_STATIC_HOLD_EXIT_EXTRA`，则认为是输入小抖动，保持原 `cmd_target_adc/cmd_pulse_us`，不退出 HOLD，不触发 H 桥。 |
| 不改内容 | 未修改 P30/P31/P70 默认参数，未改变大行程轨迹/速度/刹车参数。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19484 B`、`data=676 B`、`bss=1644 B`，总 `21804 B`。 |
| HEX/ELF SHA256 | HEX `6772d563454d49ad974b42ed7d19f596787ee7eb4d094be3a018ba189c0130d6`；ELF `aa1b4e6ed2364836cff976a46d913f03fd33c4112ccc9fa36c2e81e598f9b385`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 通过 DAPLink 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004879`，返回 `Successfully resumed device`。 |
| 复测重点 | 重新按 `1500 -> 1000 -> 1500 -> 2000 -> 1500 -> 1000 -> 2000` 测试，并额外在 1500 附近静止 `5-10s`；重点看是否还会出现长时间 `event=13` 小 duty 正反修正。 |

2026-06-16 PWM 捕获链路核查与诊断下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我有个疑问，你说的是1008-1009-1010这个范转会有4-5个ADCOUNT的值变化，这是怀疑外部输入PWM变化而引起的问题而改HOLD时外部微小变化不动作这个做法，我觉得还没找到问题时不能这么做，你倒是可以再检查一下，外部输入PWM的计算方式和过滤情况，会不会因为采样时间不够快才有误差呢？因为我看了PWM提供方变化不大，请再核查。” |
| 用户补充 | “因为你这样过滤后，将来测试灵敏度时会有冲突，因为灵敏度要2-3个us就要有舵机转动，这是茅盾的” |
| 核查结论 | 用户判断正确。不能用 HOLD 内部吞掉小目标变化来掩盖问题，否则会和 `1-3us` 灵敏度测试冲突。当前 PA4 输入不是硬件定时器捕获，而是 EXTI 双边沿加 SysTick 计时：上升沿记录时间，下降沿直接计算高电平宽度。PWM 周期只作为粗略有效帧检查，不参与宽度换算。 |
| 本次撤销 | 撤销上一版 HOLD 内 `<= LCDM_SERVO_STATIC_HOLD_EXIT_EXTRA` 的小目标变化屏蔽逻辑，删除对应 `s_lcdm_servo_cmd_pulse_us` 使用。控制层恢复为：只要捕获/锁存后的 PWM 造成目标变化超过原 `LCDM_SERVO_CMD_HOLD_EXIT_EXTRA`，就按真实命令处理。 |
| 本次诊断 | `pwm_input.c/.h` 增加原始测得脉宽 `raw_pulse_us`、候选脉宽 `candidate_pulse_us`、候选计数 `candidate_count`、原始 PWM 窗口抖动读取。P50 第一行临时改为 `Rxxxx Pxxxx Wxx`：`R` 是原始测得高电平宽度，`P` 是过滤后控制使用宽度，`W` 是原始高电平宽度窗口抖动。 |
| 捕获改进 | 新增 `Board_MicrosQ4()`，内部用 SysTick 以 `1/16us` 固定小数计时；PA4 EXTI 宽度计算改为 q4 时间戳相减后四舍五入为 us，减少 1us 整数量化误差。没有改变 PWM 过滤阈值，没有增加 HOLD 死区。 |
| 曲线工具 | SWD 曲线 CSV/SVG 增加 `pwm_raw_us`、`pwm_period_us`、`pwm_candidate_us`、`pwm_candidate_count`，图上显示“控制PWM/原始PWM/周期/候选”。周期字段只用于诊断 EXTI 计时是否异常，不作为舵机目标宽度计算依据。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19332 B`、`data=672 B`、`bss=1664 B`，总 `21668 B`。 |
| HEX/ELF SHA256 | HEX `0b61548ba9425bdd4ce34f10efa7487a96edd9ee57792b780ac57c05ebbc46ba`；ELF `d2345b1480891e1fac6b55a735dde60ace0f687e441c15ba7ca0b0853a7ef444`。 |
| 下载结果 | 第一次 1 MHz 下载时 DAPLink 初始化报 `expected DAP_INFO / expected DAP_SWJ_CLOCK`，未进入擦写；改用 100 kHz 手动 pyOCD session 后下载成功，启动向量 `SP=0x20000c00`、`PC=0x080047e1`，返回 `Successfully resumed device`。 |
| 初步数据 | 重启 SWD 曲线后，CSV 已能读出新增字段。静态高端附近样本中 `pwm_raw_us` 多数在 `2009-2013us`，`pwm_us` 在 `2010-2012us`，`target_adc` 保持不跳。后续需要标准三点测试和固定 PWM 静止测试，判断原始跳动来自信号源还是 EXTI 软捕获。 |
| 用户再次提醒 | “特别提醒一下，PWM的周期你可以大体计算，不要把整个周期的时间来分高低电平再算调电平PWM有效值的宽度，只要PWM的周期在范围内，重点计宽度就可以了，请核对一下你的程序是怎样计的，因我有看到你说周期也不稳定，实际周期无所谓的，主要是宽度。” |
| 对应确认 | 已确认程序是直接测高电平宽度，不是周期占空比反推：上升沿保存时间戳，下降沿用当前时间戳减上升沿时间戳得到高电平宽度。`s_last_period_us` 只用于 `3000-30000us` 的粗略有效帧范围判断，周期不参与 `pulse_us -> target_adc` 映射。 |

2026-06-16 PWM 捕获过滤继续改良记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “R和P都跳，P相差0-2，R相差0-5” |
| 问题判断 | `R` 是原始高电平宽度，`P` 是控制使用宽度；两者都跳说明问题仍在 PWM 捕获/过滤链路，不能改 HOLD 死区去掩盖。`P` 已从 `R` 的 0-5us 压到 0-2us，但静止点仍可能触发小目标变化。 |
| 本次源码修改 | `pwm_input.c` 增加 6 帧 q4 高电平宽度移动平均，小范围随机抖动进入平均后再给控制层；大于 `12us` 的真实命令大步进不走慢平均，但必须先在候选附近连续确认，避免单次 `1010us` 这类异常宽度污染平均窗口。候选确认允许相邻 `1us` 聚类，避免真实大步进附近因 1us 采样跳动迟迟不能接受。 |
| 保留原则 | 周期仍只做有效帧范围判断，不参与宽度计算；HOLD 不吞小 PWM；持续 2-3us 命令变化仍可通过多帧平均/候选确认进入控制层。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19596 B`、`data=672 B`、`bss=1680 B`，总 `21948 B`。 |
| HEX/ELF SHA256 | HEX `8129a4ecc5a8f19a8decbd82366ae7060e3df14aee5828d7f993e5149b739317`；ELF `c654c2b188c1e8c2affe4d6aac381ff5ece48830f1500a0dc421dec26606b20d`。 |
| 下载状态 | 用户拔插 DAPLink 后，Ubuntu 侧重新枚举为 `0d28:0204`。默认后端仍曾超时，改用 `PYOCD_USB_BACKEND=hidapiusb`、`--frequency 10000`、`connect_mode=under-reset` 后下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004825`，返回 `Successfully resumed device`。为避免后台 SWD 再占住 DAPLink，下载后已停止 `com.py32servo.curve.live`。 |
| 验证重点 | 先不看 SWD 曲线，直接看 LCDM P50 第一行 `R/P/W`：固定 PWM 不动时，目标是 `R` 仍可反映原始测量抖动，但 `P` 应明显比之前稳定，理想为 0-1us 内；若 `P` 仍有 0-2us 跳动，再继续调 PWM 输入滤波深度或候选确认策略。 |

2026-06-16 PWM 输入分级确认下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “P还是0-2” / “但2出现的概率相对看起来低了点，说明采样方法或硬件还是有问题？” |
| 问题判断 | `2us` 出现概率降低说明 6 帧平均有效，但 `P` 仍能跳 0-2us，说明根因仍在 PA4 前端捕获链路，而不是 HOLD 控制层。可能来源包括：EXTI+SysTick 软捕获的边沿时序误差，或 PA4 输入边沿不够干净。当前外部 100K 串联、330K 下拉使输入阻抗偏高，边沿经过 GPIO 阈值时更容易受噪声/阈值漂移影响。 |
| 本次源码修改 | `accept_pulse_width()` 改成分级确认：相对当前控制宽度 `<=1us` 直接当测量噪声保持，不更新 `P`；`2-3us` 小命令必须连续确认 `4` 帧才接受；`>=4us` 正常变化连续确认 `2` 帧接受。继续保留 q4 计时和 6 帧平均。 |
| 设计意图 | 固定 PWM 时避免偶发 2us 抖动进入控制目标；真正的 2-3us 灵敏度测试仍可通过，只是需要连续稳定几个 PWM 帧，不会被永久吞掉。HOLD 死区和控制算法未改。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19640 B`、`data=672 B`、`bss=1680 B`，总 `21992 B`。 |
| HEX/ELF SHA256 | HEX `709a53358330ce2e6d7f885ca7d5eec36a30615230d6a6c6a2155164519663b2`；ELF `854239d1b270c3560ec9ebaeb3e9ac075caa4f4d401eb13f250d6e40065d51ba`。 |
| 下载结果 | 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `10kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004915`，返回 `Successfully resumed device`。 |
| 下一步验证 | 看 P50 `R/P/W`：固定 PWM 不动时重点看 `P` 是否从 0-2us 降到 0-1us 或更少；再测试真正改变 2-3us 时，舵机是否在短暂确认后按方向轻微动作。若 `R` 仍 0-5us，建议做硬件对比：临时把 PA4 前端串阻从 100K 降到 1K-10K 或直接用更低阻抗信号源，观察 `R` 是否明显变稳。 |

2026-06-16 1kHz 方波 PWM 原始测宽临时版下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “没有明显的改善变化。现在这样做，我要确认PWM信号是否信号源有问题，临时改一版本只测PWM的宽度，其它任何不变，测完再恢复原状，现在的信号源只有一个1K的正负半周是1:1，测试后显示出PWM上面，其它不动，请试” |
| 本次目标 | 临时确认信号源/PA4 前端测宽是否稳定。外部输入为 `1kHz`、正负半周 `1:1`，理论高电平宽度约 `500us`，周期约 `1000us`。 |
| 本次修改 | `pwm_input.c` 把原始诊断 `R/W` 的有效周期范围临时放宽到 `500-30000us`，因此 1kHz 方波会显示在 P50 第一行 `Rxxxx Pxxxx Wxx` 的 `R/W` 上。控制用 `P` 仍只接受普通舵机 PWM 周期 `3000-30000us`，所以 1kHz 测试方波不会更新控制目标，不会驱动舵机跑到端点。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19620 B`、`data=672 B`、`bss=1680 B`，总 `21972 B`。 |
| HEX/ELF SHA256 | HEX `850e76e3a5635eb0398f59b35d489e543fb921b23bcf87e3966e78e957ce1b26`；ELF `0645421d112c7101676c1cc0d0c3fc6e1ed3e20d1c48a819a2383e6684213ac9`。 |
| 下载结果 | 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `10kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004901`，返回 `Successfully resumed device`。 |
| 验证方法 | 接入 1kHz 50% 方波后看 P50 第一行：`R` 应接近 `500us`，`W` 是约 150ms 窗口内原始宽度峰峰值；若 `R/W` 仍抖动大，优先查信号源输出、PA4 前端 100K/330K 阻抗和接地/线长。测完后需恢复普通舵机 PWM 捕获范围。 |

2026-06-16 1kHz 方波诊断显示修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不对哦，一直在闪跳，一下子R501P500,一下子有个1000字样，说明没有追踪到PWM哦” / “应该P50第一行是很稳定的才对” |
| 问题判断 | 用户判断正确。1kHz、50% 方波理论上高电平宽度应约 `500us`、周期约 `1000us`，P50 第一行应稳定。上一版仍把控制用 `P` 和原始 `R` 放在同一行，容易造成 `P500` 这种误解；并且切换到 1kHz 的首帧存在沿用前一舵机周期而误入控制确认的风险。 |
| 本次修改 | 临时诊断版把 P50 第一行改为固定格式 `Hxxxx Txxxx Wxx`，其中 `H` 为 PA4 直接测得高电平宽度，`T` 为上升沿到上升沿周期，`W` 为本刷新窗口内高电平宽度峰峰值。诊断期间不再显示控制用 `P`。 |
| 控制隔离 | `pwm_input.c` 增加 `PWM_INPUT_1KHZ_WIDTH_DIAG_ONLY=1` 临时开关，1kHz 测试期间只更新原始诊断值，不更新舵机控制脉宽，避免测试方波污染目标位置；同时增加连续舵机周期确认计数，后续恢复普通舵机 PWM 时也可防止非舵机周期首帧误判。 |
| 下一步 | 编译并下载后，用 1kHz 50% 信号看 P50 第一行：理想显示应接近 `H0500 T1000 W00`，允许 `H` 有少量 1us 级变化；若 `H/W` 仍大幅跳动，再集中查 PA4 输入边沿、信号源、100K 串阻/330K 下拉和接地。 |
| 用户下载要求 | 用户补充：“下载速度提到100K”。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19296 B`、`data=676 B`、`bss=1668 B`，总 `21640 B`。 |
| HEX/ELF SHA256 | HEX `5b27942963e76e7f0e1ba012ae91b2089fe1032b49d7e7b86e6431b3e56ef028`；ELF `88e7be40bba62a78688bbbfdbb915c421d20ed137a5dde191a43c58b38815891`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080047b5`，返回 `Successfully resumed device`。 |

2026-06-16 1kHz 方波 SWD 实测数据记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前可以看到LCDM，T1003-1004变化，W0-3变化，现在的信号是相当稳定的，可以证明不是硬件有问题就是软件计算有问题。请查看实际的数据” |
| 用户补充 | “而且数据偏大，我的输入是标准1KHZ,但实际算出来是1003” |
| SWD 原始读取 | 直接读取 `pwm_input.c` 静态变量，不依赖曲线发布：`s_raw_pulse_us=0x2000007e`、`s_last_period_us=0x2000035c`、`s_edge_count=0x20000364`、`SystemCoreClock=0x20000084`。 |
| 第一轮数据 | 200 个样本，读数间隔约 20ms 但受 SWD 速度影响总耗时约 `21.732s`。`H` 最小/最大/均值为 `500/501/500.85us`；`T` 有效值主要为 `1003/1004us`，首样本出现一次 `3us` 异常，判断为 attach/读取瞬间旧值或竞争值，不代表稳定输入。`edge_count` 增加 `43464`，约等于 `2000 edge/s`，符合 1kHz 双边沿输入。 |
| 第二轮数据 | 80 个样本，只读 `H/T/edge/SystemCoreClock`。`SystemCoreClock=24000000`；`H=500-501us`、均值 `500.8875us`；`T=1003-1004us`、均值 `1003.4359us`。 |
| 结论 | PA4 没有丢边沿，输入也不是随机抖。当前误差是稳定偏大约 `0.34%`，更符合 PY32 内部 HSI 24MHz 实际频率偏慢或 HSI trim 标定偏差。原始 `W` 偶尔读到大值是因为 SWD 同时读取 LCDM 会清零的窗口变量，存在读写竞争，不能作为本次稳定性判断。 |
| HSI 临时扫描 | 读取 `RCC->ICSCR=0x010D9304`，24MHz 出厂校准字 `0x1FFF0F10=0x9304`，即 `HSI_FS=0x8000`、`trim=4868`。临时把 trim 增加 `+16/+32/+48...` 后，`T` 反而变为 `1017/1030/1044...us`，说明 trim 增大方向会让当前计时更慢，修正应扫负方向。 |
| 当前阻塞 | 正准备恢复 `ICSCR` 并扫负方向时，DAPLink 返回 `expected DAP_INFO / expected DAP_SWJ_CLOCK / open failed`。需要拔插 DAPLink 后继续；若板上仍未复位，上一轮临时 trim 可能暂时停在较大偏移，断电/复位会回到固件默认出厂 trim。 |

2026-06-16 HSI 负方向校准与固化记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “继续” |
| 本次动作 | 增加临时工具 `tools/scan_hsi_trim.py`，从 Ubuntu 通过 SWD attach 直接扫描 `RCC->ICSCR` 的 HSI trim，扫描结束会恢复出厂 `0x010D9304`。 |
| 扫描结果 | 出厂 `trim=4868` 时 `H=501-502us`、`T=1003-1005us`；`trim-3` 时 `H=500-501us`、`T=1001-1002us`；`trim-4` 时 `H=499-500us`、`T=1000-1001us`，均值 `1000.933us`；`trim-5` 时 `H=499-500us`、`T=1000-1001us`，均值 `1000.5us`；`trim-6` 后 `T=999-1000us`，开始略偏快。 |
| 判断 | 选择 `trim-5` 作为当前板子的最佳临时校准点。个别档位的 `H=1498/1499/1500us` 是扫描改 trim 瞬间或 SWD 读取竞争下的异常样本，周期统计和稳定区间不支持 PA4 丢边沿结论。 |
| 本次源码修改 | `board.c` 增加 `BOARD_HSI_TRIM_OFFSET=-5` 和 `Board_Hsi24MHzCalibrationValue()`，系统时钟初始化时在 Puya 24MHz 出厂校准值基础上把 HSI trim 减 5。 |
| 验证目标 | 下载后 1kHz 50% 输入时，P50 第一行应接近 `H0499/0500 T1000/1001`，明显优于原 `T1003/1004`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19300 B`、`data=676 B`、`bss=1668 B`，总 `21644 B`。 |
| HEX/ELF SHA256 | HEX `abcf6bad6073e3913a8b58ae81a378d401de5c4ed51fa5487fd5b924e32637fa`；ELF `7b48d8e206f2c1bc24c672b03013e13039dcf92cf6c7d10d76c05df38e5f3953`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080047b9`，返回 `Successfully resumed device`。 |

2026-06-16 W 窗口抖动继续核查记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “T显示是在999-1001之间变化，但我看到W最大时有4的变化” / “你核查过侦测没问题，只是频偏问题，但为什么会有那么大的误差？” |
| 回答判断 | 频偏只解释原先 `T1003/1004` 的固定比例误差，不能解释 `W=4` 或偶发更大的窗口。`W` 反映的是单次边沿测量抖动/配对错误，重点在软捕获方式，而不是 HSI 平均频率。 |
| SWD 实测 | `trim -5` 版下直接读原始变量 200 次：`T=999/1000/1001us`、均值 `999.985us`，`edge_count` 约 `2000 edge/s`，证明频率已校正且整体没有丢边沿。`H` 主要在 `498/499/501us`，但抓到一次 `1499us` 异常。 |
| 异常解释 | `1499us` 约等于 `500us + 1000us`，说明某一次上升沿未正确更新 `s_last_rise_q4` 或 ISR 读当前 GPIO 电平时已经过了对应边沿，下一次下降沿拿上一轮上升沿计算，形成物理上不成立的高电平宽度。当前 PA4 是 EXTI+SysTick 软捕获，不是硬件输入捕获，边沿时间没有硬件锁存，单次 ISR 延迟会直接变成微秒级误差。 |
| 中断核查 | PA4 EXTI 优先级为 `0`，USART1 为 `1`，TIM1 为 `2`，所以不是 EXTI 优先级过低。根因方向仍是软捕获的边沿分类/配对和时间戳延迟。 |
| 本次源码修改 | `PWM_Input_EdgeISR()` 进入中断后先直接读 `GPIOx->IDR` 锁定当前电平，再读取 `Board_MicrosQ4()`，减少先算时间再读电平造成的边沿归类延迟；同时增加 `width < s_last_period_us` 物理约束，拒绝高电平宽度大于周期的错误配对，避免 `1499us` 进入 `H/W`。 |
| 后续方向 | 若下载后 `W` 仍常见 `3-4us`，则说明 EXTI+SysTick 软捕获的 ISR 延迟抖动仍不够满足 1-3us 灵敏度，下一步应评估 PA4 是否能改硬件定时器输入捕获；若 PA4 不能映射输入捕获，就需要用更高分辨率自由运行定时器读取计数并进一步缩短 ISR 路径。 |
| 用户提醒 | “后面还会遇到反应速度问题，也就是说PWM连续快或慢变化，舵机要跟着同步转动，目前我们之前试的程序都是无法实现的，这点要记得。” |
| 对应要求 | 后续控制架构不能只优化静态 PWM 宽度显示，也必须支持 PWM 连续快/慢变化时目标位置同步更新、舵机连续跟随。PWM 输入层不能用过重确认/滤波吞掉 1-3us 灵敏度或动态变化；若软捕获抖动无法兼顾稳定和响应，应优先改捕获架构而不是继续加死区。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19308 B`、`data=676 B`、`bss=1668 B`，总 `21652 B`。 |
| HEX/ELF SHA256 | HEX `1070e95abd331b5b2bec1d507ece3cdac4371f8de33485f20b84dcdf1d00a9fe`；ELF `66b4d72fde4754543daa05640fde3236eee38712ab3815f720f2c0c5218d64ea`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080047c1`，返回 `Successfully resumed device`。 |

2026-06-16 W 偶发 4us 复测记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前观察相对较稳，在999-1000变化，而W01-03偶尔会出现4” |
| 现场现象 | `T` 已从校准前 `1003/1004us` 改善到 `999/1000us` 附近，说明 HSI trim 和周期计时已经基本到位。`W` 仍偶发 `4us`，说明单次高电平测宽还有尾部抖动。 |
| SWD 复测 | 下载修正版后，直接读 300 个样本，约 `28.515s`。`H` 只分布在 `498/499us`，计数 `{498:113, 499:187}`；`T` 分布 `{997:1, 998:2, 999:233, 1000:64}`，均值 `999.2us`；`edge_count` 增加 `57031`，符合 1kHz 双边沿。未再抓到 `1499us` 这类错误配对。 |
| W 异常 | 300 个样本期间窗口变量出现 1 次宽窗口：`W=5`，窗口 `min496/max501`。这说明 `W=4/5` 不是连续不稳，而是极低概率单次软捕获抖动。 |
| 原因解释 | `T` 是相邻上升沿间隔，两次同类边沿的 ISR 延迟相近时会相互抵消；`H` 是上升沿到下降沿，两个不同边沿进入 ISR 的延迟差会直接变成宽度误差。因此会出现 `T` 很稳但 `W` 仍偶发 3-4us 的情况。 |
| 后续约束 | 因用户要求 1-3us 灵敏度，以及 PWM 连续快/慢变化时舵机同步跟随，不能靠重滤波或大死区把偶发抖动盖掉。下一步应继续查 PA4 是否可硬件输入捕获；若 PA4 无法映射硬件捕获，则至少改为更短 ISR 路径和自由运行定时器计数，避免 SysTick/HAL 路径带来的尾部抖动。 |

2026-06-16 静态锁止力恢复和持续嗞声修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有时还是会一点点嗞嗞声，自己动目前没有发现。另外，还发现一个问题，现在一直转动都会以前那样，转久一点向高端2000处会没有力转不动的现象，是什么原因呢，机构电路都还是原来的，会不会还是程序哪里出错了？” |
| 用户补充 | “以前嗞嗞声时，手动转一下就到位了没有声了，现在的不行，一直在嗞嗞声。” / “我发现了，原因是PWM没有那么大的力锁止了，在静止状态下，现在直接可以用手转动，电流只有7百mA,而正常会达到几A电流” |
| 判断 | 上一版为了避免误动和 HOLD 嗞声，把 HOLD 退出做得过保守，静止小到中等误差时更多停在 `HBridge_Brake()`，实际表现为安静但不是主动反推锁止；用户测到约 700mA，符合“刹车/弱锁”而不是强 PWM 反驱。 |
| 本次源码修改 | 不改移动速度和刹车曲线。`LCDM_SERVO_HOLD_EXIT_COUNT` 从 `12` 改为 `4`，让 PWM 未变而 ADC 被外力推离目标后更快退出 HOLD 进入原有 `LOCK` 分支；HOLD 低速安静区从 `Rel+16` 收紧为 `Rel+4`；`LCDM_SERVO_STATIC_LOCK_MIN_DUTY` 从 `220` 提高到 `420`，恢复静态锁止最小反推力度。 |
| 容量处理 | 首次新增显式 LOCK 分支导致 Flash 超限，已改为复用原有 LOCK 分支和参数阈值，不再增加控制分支；同时把 P00 旧演示动态内容压缩为静态占位页，P50 诊断和各参数页不受影响。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19672 B`、`data=680 B`、`bss=1680 B`，总 `22032 B`。 |
| HEX/ELF SHA256 | HEX `947f8814ec34d44e73fc898c216000b800f6c61f30786db256e4a903b5f0817e`；ELF `c66dd44fc3d9e806dbf4add105657051e4f0234328c54bb358d2b25f2bdc46db`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004931`，返回 `Successfully resumed device`。 |
| 测试重点 | 静止时轻拨舵板应更快从 `HLD/BRK` 进入 `D+`/`D-` 主动反推，P50 HBR 行应看到占空比明显高于上一版；若高端 2000us 转久后仍无力，优先看 P50 HBR 行：若仍是大 duty 但无力，转向电源/H桥/马达发热或机械负载；若显示 `HLD/BRK/RETURN` 或小 duty，则继续查控制限幅。 |

2026-06-16 强外力锁止测试版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我们是可以继续改锁止力吗？” / “还有就是，如果我故意大力正反扭动，它会一直来回摆而无法找到停止点，是否在这个时候没有看外界的PWM了，还是其它原因” / “换句话说，直接电流爆，都不应该转动舵板” / “顶多转动一点点后，一松手马上回位，在应用时也需要这样子测试” |
| 判断 | 固定 PWM 时程序仍持续读取 PWM，目标没有变；大力正反扭后无法停止，问题在外力扰动恢复路径：静态锁止不应沿用普通运动控制的温和找点方式，而应当在 ADC 离开锁定点后立即强反推，并在 ADC 高速回目标时刹车，避免松手后反打来回摆。 |
| 本次源码修改 | `LCDM_SERVO_HOLD_EXIT_COUNT` 保持更快的 `3`；`LCDM_SERVO_STATIC_LOCK_MIN_DUTY` 为 `560`、`LCDM_SERVO_V3_LOCK_MAX_DUTY` 为 `950`。LOCK 分支改为强锁优先：进入 LOCK 后直接按 `Lmax` 反推；若 ADC 已经高速向目标回位且误差在静态锁范围内，则先 `HBridge_Brake()`，避免继续加油推过目标。 |
| 未采用版本 | 曾尝试渐进锁止曲线，但 Flash 超限且逻辑偏温和，不符合“外力强扭时应直接大电流顶住”的测试标准，已改为更小的强锁逻辑。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19664 B`、`data=680 B`、`bss=1680 B`，总 `22024 B`。 |
| HEX/ELF SHA256 | HEX `da519eb8b18f301027c06bf23ec6d8e1193f37b9c09afaf70441584c19cc6e7c`；ELF `b19027ef82fb8c6434ea4cd3b6422f166c16b750edd2b1b29351136bb9568f85`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004929`，返回 `Successfully resumed device`。 |
| 测试重点 | 固定 1000/1500/2000us 时分别大力正反扭舵板：理想现象是最多偏一点，松手立即回位；P50 HBR 应显示 `D+950` 或 `D-950` 一类强反推，回位瞬间可能显示 `BRK/HLD`。若仍可大幅转动且 HBR 已是大 duty，则继续查供电/H桥/马达电流能力；若 HBR 没有进入大 duty，则继续查 HOLD/LOCK 触发条件和 ADC 速度判断。 |

2026-06-16 快速强锁版本记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “电流有改善，但反应不够快，大力扭直接会抖” / “继续” |
| 判断 | 上一版电流改善说明 LOCK 强反推方向有效，但反应仍慢：HOLD 退出仍要多个控制周期，且 LOCK 输出仍经过 `lcdm_servo_apply_slew()`，强锁 duty 不是瞬间到位。大力扭时反推来晚，舵板先偏开再被大力拉回，容易形成抖。 |
| 本次源码修改 | `LCDM_SERVO_HOLD_EXIT_COUNT` 从 `3` 改为 `1`；LOCK 分支去掉 `lcdm_servo_apply_slew()`，进入强锁后直接输出 `Lmax`；同时把“高速回目标就刹车”的范围从 `STATIC_LOCK_FULL_ERROR=96` 收窄到 `hold_release_band+4`，避免外力仍在时过早刹车导致抖。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19648 B`、`data=680 B`、`bss=1680 B`，总 `22008 B`。 |
| HEX/ELF SHA256 | HEX `b715f571f3bb6d24ccfb3e73494b582d9819513b6ac6a1cb7396637017f510d7`；ELF `3a474c9324ff52c592ed2f1e45d46c1776a3da5766e280199536424c827fbf7f`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004919`，返回 `Successfully resumed device`。 |
| 测试重点 | 大力扭动时看 P50 HBR：应更快进入 `D+950` 或 `D-950`。若仍抖，需记录抖动瞬间 HBR 是持续大 duty、还是在 `D/BRK/HLD` 间切换；前者偏硬件/机械弹性或电源电流响应，后者继续改锁止状态机。 |

2026-06-16 快速锁止斜率恢复版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “更抖了” |
| 判断 | 去掉 LOCK 输出斜率后，外力偏移即瞬间满 duty 反打，机械弹性、齿隙和 ADC 延迟被放大，形成更明显的 bang-bang 抖动。 |
| 本次源码修改 | 保留 `LCDM_SERVO_HOLD_EXIT_COUNT=1`，让外力扰动后快速进入 LOCK；恢复 LOCK 分支的 `lcdm_servo_apply_slew()`，让强锁电流上升有斜率，不再瞬间满力硬切；继续保持“高速回目标就刹车”的范围为 `hold_release_band+4`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19656 B`、`data=680 B`、`bss=1680 B`，总 `22016 B`。 |
| HEX/ELF SHA256 | HEX `7cf9e4dbba830a3064aad4fb42cfff0bf97be430f8a616233ba45b98d1a61067`；ELF `f1f120728d6de36119f0ee062b31f63e73019efd78fea2e173eb7ccbbd745de1`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004921`，返回 `Successfully resumed device`。 |
| 测试重点 | 重点对比上一版：若抖动减轻但反应仍慢，下一步应提高 LOCK 专用斜率，而不是直接满 duty；若仍比“强外力锁止测试版”更差，则回退 `HOLD_EXIT_COUNT` 或锁止最大 duty。 |

2026-06-16 回退到截图基线版本记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “越改越差了，抖得更严重，我建议恢复到上面这个版本，先不动，我再试，试完了我们再来改。” |
| 执行结论 | 已恢复到用户截图所指版本：`HOLD_EXIT_COUNT=4`、静止安静区为 `Rel+4`、`STATIC_LOCK_MIN_DUTY=420`、`LOCK_MAX_DUTY=850`；取消后续版本的 `HOLD_EXIT=1`、`Lmin=560`、`Lmax=950`、LOCK 固定打满和快速强锁改法。 |
| 校验确认 | 恢复后 HEX SHA256 与截图版记录一致：`947f8814ec34d44e73fc898c216000b800f6c61f30786db256e4a903b5f0817e`；ELF `c66dd44fc3d9e806dbf4add105657051e4f0234328c54bb358d2b25f2bdc46db`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19672 B`、`data=680 B`、`bss=1680 B`，总 `22032 B`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004931`，返回 `Successfully resumed device`。 |
| 下一步 | 用户先复测该基线版。复测前不再继续修改锁止逻辑，避免继续引入混合变量。 |

2026-06-16 结束基线版本遗留问题记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “记录这个版本存在问题：1，自动会动；2，锁止力不够，可不停转动；” |
| 对应版本 | 当日结束基线版，HEX SHA256 `947f8814ec34d44e73fc898c216000b800f6c61f30786db256e4a903b5f0817e`，即已恢复截图基线版本。 |
| 已确认问题 1 | 固定输入条件下仍可能出现自动动作，需要后续继续区分是 PWM 输入 `P/R/W` 跳变、ADC 反馈漂移/跳变，还是 HOLD/LOCK 状态机误退出。 |
| 已确认问题 2 | 静态锁止力不足，外力可持续转动舵板；说明当前 `LOCK` 输出力度、触发速度、或 H 桥实际电流响应仍达不到应用要求。 |
| 后续约束 | 后续修改必须从此基线重新分析，不再叠加当天后续试过且更抖的快速强锁版本。优先用 P50 HBR/PWM/ADC 行判断自动动作和锁止不足的直接触发原因。 |

2026-06-16 PA4 换脚可行性问题记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “按目前的规划，有哪个IO脚可以与PA4换用” |
| 初步判断 | 当前 SOP8 最小风险方案不是把 PWM 信号线换到另一个物理脚，而是确认 SOP8 第 2 脚 `PA4/PA10` 能否在固件中按 `PA10/TIM1_CH3` 复用为硬件输入捕获。这样外部 3PIN 信号线、单线写参规划和 PCB 脚位都不变，只改变 MCU 复用功能和 PWM 捕获驱动。 |
| 风险点 | 若必须换到其它物理 IO，SOP8 剩余脚都已被关键功能占用：`PA3` 是电位器 ADC，`PA1/PA2/PA13/PA14` 是 H 桥四路输出，其中 `PA13/PA14` 还兼 SWD。换脚会牵连 H 桥走线或 SWD 下载能力，需重新评估 PCB。 |

2026-06-16 恢复普通舵机 PWM 控制记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “也就是说暂时不换IO吗？恢复之前的PWM控制，保留已调整的检测PWM方法，对吗” / “好的，请切换回普通舵机，再进行测试” |
| 本次判断 | 暂时不换 IO，继续使用 `PA4`。保留已验证有效的 `HSI trim -5`、P50 `H/T/W` 显示、EXTI 入口先读 GPIO 电平、`width < period` 错误配对过滤。 |
| 本次源码修改 | `pwm_input.c` 把临时 `PWM_INPUT_1KHZ_WIDTH_DIAG_ONLY` 从 `1` 改为 `0`，恢复普通舵机 PWM 周期进入过滤和控制链路。1kHz 方波仍可作为原始诊断显示，但不再作为当前测试重点。 |
| 测试目标 | 固定 `1000/1500/2000us` 和慢速连续扫 PWM，观察 `W` 偶发 3-4us 是否会导致目标 ADC 跳动、HBR 从 HOLD 误入 DRV、嗞嗞声或动态跟随迟滞。 |

2026-06-16 普通舵机恢复版构建和下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “好的，请切换回普通舵机，再进行测试” |
| 执行结论 | 已切回普通舵机 PWM 控制：继续使用 `PA4`，不换 IO；保留 `HSI trim -5`、P50 `H/T/W` 原始诊断显示、EXTI 入口先读 GPIO 电平、`width < period` 错误配对过滤。 |
| 容量处理 | 恢复普通 PWM 控制后，首次链接超出 Flash 32 字节；未改控制算法，只在 P50 显示层删除未显示的 `feedback_mv` 缓存/换算刷新触发，第三行仍显示 `R/C/W`，第二行仍显示目标 `ADC/mV`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19620 B`、`data=676 B`、`bss=1676 B`，总 `21972 B`。 |
| HEX/ELF SHA256 | HEX `dceda2c6998590db696dae9a3cce00cafe0f6102da4edb57cdec1ef7eb390e19`；ELF `3cc261f490c740efb99ee18dec33a5c68e7ee310726a9fe2765c79298a824760`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048f9`，返回 `Successfully resumed device`。 |
| 下载后读数 | 下载后尝试用 SWD stream 读取运行变量。第一次 3 秒窗口只拿到 CSV 表头；第二次 attach 时 DAPLink 返回 `expected DAP_INFO`，判断为探针 USB 通讯层未重新打开，不代表固件下载失败。若需要继续 SWD 曲线采样，先拔插 PADLINK 后再读。 |
| 用户测试重点 | 请接普通舵机 PWM 后测 `1000/1500/2000us` 和慢速连续扫 PWM：P50 第一行应显示普通舵机周期约 `T20000` 附近，`H` 为实际输入宽度；重点观察 `W` 偶发值是否引起目标 ADC 跳、HBR 离开 HOLD、嗞嗞声或跟随迟滞。 |

2026-06-16 P50 PWM 显示澄清和控制周期收紧记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “LCDM的T显示不正常啊，实际PWM是1000,却显示19987，1500，2000都显示一样，而且有时瞬间舵机会转，什么情况？” / “顺便把几个参数显示隔远一点点，不要粘在一起” / “顺便来一个所有行的参数都是左对齐” |
| 原因解释 | P50 旧第一行 `Hxxxx Txxxx Wxx` 中，`T` 是 PWM 周期，不是高电平宽度；普通舵机 50Hz 时周期约 `20000us`，所以输入 1000/1500/2000us 时 `T` 都接近 `19987us` 是正常周期读数，但显示命名容易误导。真正的宽度是 `H`。 |
| 本次显示修改 | P50 第一行改成 `Pxxxx  Rxxxx  Wxx`：`P` 为已经进入控制链路的宽度，`R` 为 PA4 原始测得高电平宽度，`W` 为窗口抖动。其它 P50 多参数行增加间隔，例如 `R/C/W`、`E/O`、`HBR V e`。 |
| 对齐修改 | `lcdm_row_ex()` 和 `lcdm_value_cell()` 的参数值区域从右对齐改为左对齐，普通参数页和 P50 实时刷新行统一左对齐显示。 |
| 控制保护 | 将普通舵机控制有效周期从原来的 `3000-30000us` 收紧为 `15000-25000us`。原始诊断仍允许更宽周期显示，但只有接近 50Hz 的普通舵机周期才进入控制链路，降低 1kHz 测试信号、错误周期或异常配对造成瞬间转动的风险。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19664 B`、`data=680 B`、`bss=1680 B`，总 `22024 B`。 |
| HEX/ELF SHA256 | HEX `542ce342720c4ca67dcc6a8f533c3ab02659a105a88cbbc32896bbff2823de25`；ELF `4351602725193c944556da4dfd568d9ed348409e8e1751ab37e5ef17a9859840`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800491d`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 用普通舵机信号测试时，P50 第一行应直接看到 `P` 随 1000/1500/2000us 变化，`R` 也应接近输入宽度；若 `R` 对而 `P` 不跟，查确认/滤波；若 `R` 自身跳，继续查 PA4 软捕获抖动。 |

2026-06-17 PWM 输入假跳 SWD 记录和输入保护版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我看得眼花，在1500中间,R有时会跳2000字样，一闪而过，我觉得你还是自己检测记录一下吧，这样会来得准确一点，究竟是哪里触发到了。” |
| 自测结论 | 通过 SWD 直接读取 `g_servo_curve_frame`，确认不是视觉错觉：固定中位附近时，原始 `R` 会出现 `~503us`、`~2503us` 这类假宽度。早期抓取 `raw_frame_1500_20260617_201816.csv` 中，`P` 曾被接受到 `2505us`，`target_adc` 跳到 `3785`，能直接解释无命令自动动作。 |
| 触发链复核 | 第一版只收紧周期后，仍发现一次 `R=503` 进入 6 帧平均，把候选拖成 `1336`，后续正常 `1502/1503` 反而继续确认这个假候选，导致 `P=1336`、`target_adc=1762`、H 桥出力。说明大跳变原始宽度不能进入平均滤波缓存。 |
| 本次源码修改 | `pwm_input.c` 控制链路新增 `PWM_INPUT_CONTROL_FRAME_MIN_US=19500`、`PWM_INPUT_CONTROL_FRAME_MAX_US=20500`，原始诊断 `R/W` 仍保留 `500-30000us` 范围；对相对当前 `P` 超过 `12us` 的大跳变，不再进入 6 帧平均滤波，只走连续候选确认；同时控制链路只接受当前 `ServoParams` 的 `pulse_lower_us..pulse_high_us` 范围内宽度，避免 `503/2504` 这类超出当前参数范围的假宽度进入 `P`。 |
| 重要限制 | SWD 连续读 RAM 可能会放大软捕获延迟，因此最终确认应以 LCDM P50 和必要时的固件内部异常记录为准；但多次 SWD 采样已经证明 PA4 EXTI 软件捕获层存在错误配对/延迟宽度，后续若仍有自动动，优先考虑改为 PA4/PA10 的硬件定时器输入捕获或增加内部环形异常记录。 |
| 最终采样文件 | `build/pwm_glitch_debug/verify_after_range_gate_1500_raw.csv`。该段数据里既有 `R=503/2503` 的假跳，也有连续稳定的 `1003us/2003us` 段；连续稳定段会被视为真实输入命令。若测试时信号源确实固定 1500，则需继续查信号源/PA4 软捕获为何会连续多帧测得 1000/2000。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19616 B`、`data=680 B`、`bss=1680 B`，总 `21976 B`。 |
| HEX/ELF SHA256 | HEX `e478c194dcb0f71d707c3b657a18efb0f27b3b0b5b54bff4a3cfbcdd72fe1501`；ELF `b5c73fee6ccca6200cd6c4dd9d54e41105a2a9493dffe19baa7dd91e0bba3d4f`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048f9`，返回 `Successfully resumed device`。 |
| 下一步测试 | 固定 1500us 不动，重点看 P50 第一行：允许 `R` 偶发闪 `503/2503`，但 `P` 不应跟着大跳，目标 AD 不应跳到上下端；若 `P` 仍连续跳到 `1000/2000`，说明输入捕获层连续多帧测到该宽度，下一步应改硬件 TIM 输入捕获或先做固件内部异常环形记录。 |

2026-06-17 固定 1500us 不触碰复测记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我有动一下，不好意思，请继续，我不再动” / “你不能光屏蔽2500以上吧，下次会更宽呢，怎么办” |
| 干净采样结果 | 用户确认不再动后，重新抓取 `build/pwm_glitch_debug/verify_fixed_1500_no_touch_20260617.csv`，共 `1764` 行。全段 `R` 仍有 `503/504/505/2503/2504/2505` 假跳；5 秒后 `R` 超出 `1450-1550us` 共 `25` 次，但 `P` 始终 `1504us`，`target_adc` 始终 `2055`，`duty=0`，`state=1`，`event=2`，没有进入驱动。 |
| 结论 | 当前下载版在默认 `pulse_lower_us=900`、`pulse_high_us=2100` 范围下，能阻断固定 1500us 时的 `R` 假跳，不让它进入 `P/target_adc/H桥`。 |
| 风险修正 | 用户指出不能只靠挡 `2500us` 以上，这是正确的。当前范围门限只是安全挡板，不是根治；将来若开放 `500-2500us` 输入范围，`2500us` 假跳会再次变成合法值。根因仍是 PA4 EXTI 软件捕获在中断延迟时用“进入 ISR 的时间”代替“边沿发生时间”，可能把 1500us 量成 2500us 或 500us。 |
| 后续方向 | 根治方案应切到硬件定时器输入捕获，让边沿瞬间锁存计数；如果 PA4/PA10 复用或定时器资源受限，至少要增加独立异常环形记录和大跳变二级判定，不能只依赖上下限。当前 TIM1 已用于 20us H 桥软件 PWM tick，不能直接用同一 20us 周期计数器量 1500us PWM，需继续确认 TIM3/TIM14/TIM16/LPTIM 与 PA4/PA10 的可用复用关系。 |

2026-06-16 防自动误动和 HOLD 嗞声安全版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有时还是会突然转动，不知是哪里出错了。另外，到点后还是偶尔会有嗞嗞声” / “刚才已经自动动了二下来回了” |
| 本次判断 | 自动来回动优先按安全问题处理。可能路径有两条：一是 PWM 输入偶发错误宽度被接受，导致 `P`/目标 ADC 跳变；二是 PWM 未变但 ADC/观察器噪声使 HOLD 退出到 LOCK/DRIVE，H 桥短时正反驱动。 |
| 输入侧修改 | 普通舵机控制有效周期进一步从 `15000-25000us` 收紧到 `18000-22000us`；`PWM_INPUT_NORMAL_CONFIRM` 从 `2` 提到 `4`，大步 PWM 宽度变化必须连续 4 帧确认才进入控制；P60 `Stb` 默认从 `2` 改为 `4`。 |
| HOLD 侧修改 | `LCDM_SERVO_HOLD_EXIT_COUNT` 从 `6` 提到 `12`；HOLD 后若误差只比 release band 大一点且速度很低，继续保持刹车，不再立即进入 LOCK/DRIVE，减少到点后偶发嗞嗞声。 |
| 显示折中 | 为通过 20KB Flash 边界，P50 多参数显示保留左对齐，双空格改回单空格；参数仍不会右靠粘连。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19668 B`、`data=680 B`、`bss=1680 B`，总 `22028 B`。 |
| HEX/ELF SHA256 | HEX `03509c95c17674b6fe929f1164f6e5b8f75321453c3c59095ab02b8a3753b230`；ELF `e38d7b9ea8f0d8578ec49fbbd3a47b587180ddc1ae6c1378e6ac00575e06ddc2`。 |
| 下载结果 | 已停止 `com.py32servo.curve.live`。Ubuntu pyOCD 使用 `PYOCD_USB_BACKEND=hidapiusb`、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800492d`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 如果还出现无命令自动动，立刻看 P50 第一行：若 `P` 同时跳变，继续查 PA4/PWM 捕获；若 `P` 不跳但 HBR 离开 HOLD，继续查 ADC/观察器/HOLD 退出条件。若到点仍嗞嗞，看 HBR 行是 `HLD` 还是 `D+/D-/BRK`。 |

2026-06-17 第二问题点：锁止力调试启动版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “好的，记得记录，到什么时候改一定要提醒。现在进行第二个问题点，锁止力调试，请开始” |
| 本次提醒点 | 继续调锁止力前，先把 PWM 捕获问题记为后续必修项：以后只要准备把输入范围放开到 `500-2500us`，或开始验证 `1-3us` 灵敏度/死区，就必须先处理 PA4 PWM 捕获根因，不能只靠当前 `900-2100us` 范围门限挡住假跳。 |
| 本次判断 | 本轮只进入第二问题点“锁止力”，不再继续扩大 PWM 输入范围，也不改速度和刹车曲线。之前已经试过瞬间满 duty 强锁，结果会变抖，所以这次采用提高锁止力下限/上限、保留输出斜率的方式。 |
| 本次源码修改 | `LCDM_SERVO_STATIC_LOCK_MIN_DUTY` 从 `420` 提高到 `520`；`LCDM_SERVO_V3_LOCK_MAX_DUTY` 从 `850` 提高到 `930`；HOLD 退出等待只在 `abs_error < STATIC_LOCK_ERROR` 时执行，外力把误差推到 `>=28 count` 时不再等满 `HOLD_EXIT_COUNT`，更快进入 LOCK。LOCK 分支仍保留 `lcdm_servo_apply_slew()`，避免瞬间满力切换造成更严重摆动。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19628 B`、`data=680 B`、`bss=1680 B`，总 `21988 B`。 |
| HEX/ELF SHA256 | HEX `8e939e28579f1117880ee5c56bc8ae0f58983395b7414e78c3123014be5da890`；ELF `7e3f0f83a878488cb0d3dc8c02fc0e55742e5a183f780feef4e0590571f85bc1`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004905`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`、`1000us`、`2000us` 后用手外力扭舵板，看 P50 的 HBR 行是否比上一版更快进入反向 `D+/D-` 锁住；电流/锁力应增强，但不应明显变成连续大幅抖动。若锁力仍慢，下一步优先调 LOCK 专用斜率或 P70 `Lmin/Lmax`；若抖动变大，先降 `Lmin` 或锁止斜率，不回到瞬间满 duty。 |

2026-06-17 SWD 采集和 Mac/Ubuntu 互通固定流程记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “你把处理采集的各种理由和系统互通的情况汇总一起，不要每次都问MAC或UBUNTU系统等，只直处理即可。” |
| 固定处理流程 | 后续采集/下载不再反复询问系统选择：优先用 Parallels `Ubuntu Linux` 直连 PADLINK/DAPLink；若 `/mnt/hgfs/qtprj` 共享目录不可用，则使用项目既有临时目录 `/tmp/py32f002_servo/PY32F002_SERVO`；Mac 侧只负责查看/分析/记录文件。 |
| 探针异常处理 | 若 pyOCD 报 `expected DAP_INFO`、`expected DAP_SWJ_CLOCK`、能力识别异常或 CSV 只有表头，先在 Ubuntu 内执行 `usbreset 0d28:0204` 复位 DAPLink，再重试采集；只有软复位失败或 USB 不枚举时才提示用户拔插 PADLINK。 |
| 数据有效性标准 | 采集数据必须有足够行数，且 `time_ms` 持续变化，才用于修改控制算法；若只有少量重复行，视为 SWD attach/运行状态问题，不作为锁止力或抖动原因判断。 |
| 当前锁止力采集状态 | 第一次锁止力抖动采集只得到 3 行，`time_ms` 相同，不能代表真实抖动过程；继续确认 attach 后 MCU 是否运行，再重新采集。 |

2026-06-17 锁止力外力抖动数据分析和近目标分段锁力版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “力度够，会抖，我想还是像之前的方法，你自己采集数据看，是哪里出问题了吧” / “要知道，你前面测的是没有经过我用力转的吧” / “你可以屏蔽一些暂时不用的字段和程序” |
| 采集文件 | `build/lock_force_forced/servo_curve_live.csv`，共 `505` 行。用户用力扭动后数据有效。 |
| 数据结论 | `pwm_us` 全程 `1504us`，`target_adc` 全程 `2055`，说明这次抖动不是 PWM 目标跳变。外力扭动后 `feedback_adc` 在 `1930..2347` 间变化，`error_adc` 最大约 `-292..+123`，H桥 duty 最大约 `-890..+848`。在 `LOCK event=16` 中，误差只有 `+30..+60` 或 `-30..-60 count` 时仍输出约 `600..680 duty`，并且 duty 正负切换 `34` 次，判断为锁止回拉近目标时出力过大，冲过目标后反向再打，形成来回抖。 |
| 本次源码修改 | 不改 PWM 捕获和目标映射。屏蔽旧 `g_servo_curve_*` 单变量的重复刷新，实时/屏幕事件改读 `g_servo_curve_frame`；LOCK 分支去掉 `lock_gain_q8` 旧缩放，近目标锁力改为从固定 `160 duty` 起步，按误差线性升到 `Lmax`，保留远离目标时的大锁力。暂时保留 P70 `Lmin` 显示项，但本版 LOCK 近目标起步不再使用 `Lmin`，后续确认方向正确后再恢复成有效参数。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19640 B`、`data=680 B`、`bss=1680 B`，总 `22000 B`。 |
| HEX/ELF SHA256 | HEX `65522f4bd8dc79932c6e0027fbdae8841929a59941b1f9bdbb44e404e35bf257`；ELF `8b5d8b1c5780efc04b5d20cca6923d05d76196230cb86e38f9ed078355e853ba`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004911`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，先轻微再大力正反扭舵板并松手。重点观察：锁力是否仍够；松手后是否还出现来回摆；P50 HBR 是否仍在小误差附近频繁 `D+/D-` 切换。若锁力变软但不抖，下一步提高近目标起步 duty；若仍抖，继续降低近目标斜率或增加小误差回目标刹车分支，同时需要继续屏蔽无用字段腾 Flash。 |

2026-06-17 LOCK_BASE 300 锁力补偿版构建记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不抖，但没力” / “你可以屏蔽一些暂时不用的字段和程序” |
| 本次判断 | 上一版 `LOCK_BASE=160` 已证明抖动方向正确，但近目标锁力太小。本次只提高近目标起步锁力，不改 PWM 捕获、不改目标映射、不改远端大锁力。 |
| 本次源码修改 | 新增 `LCDM_SERVO_LOCK_BASE_DUTY=300`，LOCK 近目标 duty 从 `300` 起步，按误差线性升到 `Lmax`；继续屏蔽旧 `g_servo_curve_*` 单变量重复刷新，P50 事件读 `g_servo_curve_frame.event`。P70 `Lmin` 暂时仍不参与本版近目标起步。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19644 B`、`data=680 B`、`bss=1680 B`，总 `22004 B`。 |
| HEX/ELF SHA256 | HEX `963c189768afae8993ebe3541f60e31670de846aecc3a69eced81b0e4555ab2f`；ELF `0fc0c762cb236e1eadd7e99c950bc539202c028bcb374225fec585446e98e890`。 |
| 下载状态 | 初次未下载成功：PADLINK/DAPLink 在自动 `usbreset` 后仍连续返回 `expected DAP_INFO` / `expected DAP_SWJ_CLOCK`，且 pyOCD 返回码会误报成功；必须以输出出现 `Successfully resumed device` 才算下载成功。用户拔插 PADLINK 后，Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004915`，返回 `Successfully resumed device`。 |

2026-06-17 LOCK_BASE 240 半斜率减冲击版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “力度够，但不停的左右转，瞬间冲击，会破坏平衡走到别的位置” |
| 本次判断 | `LOCK_BASE=300` 证明锁止力够，但近目标小误差区输出过猛，外力释放后会左右反打并把平衡点冲走。本轮继续只调锁止段，不改 PWM 捕获、目标映射、行程和速度。 |
| 本次源码修改 | `LCDM_SERVO_LOCK_BASE_DUTY` 从 `300` 降到 `240`；LOCK 近目标线性斜率从 `>>7` 改为 `>>8`，等效把误差到 duty 的增幅减半。远离目标仍允许升到 `Lmax=930`，目标是在固定 1500us 外力扭动时比 `300` 版少冲击，同时比 `160` 版有更明显锁力。P70 `Lmin` 仍显示但当前 LOCK 近目标起步暂不使用它。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19640 B`、`data=680 B`、`bss=1680 B`，总 `22000 B`。 |
| HEX/ELF SHA256 | HEX `e6ae072983d65d043c14efa8767047590ef366722f41a26dd22409d694156eff`；ELF `5f95f2c79ad39d5d24b225bac6fa3e4ca1c43935735918f9193793f9af4d5f66`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004911`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，用手正反扭舵板后松手：确认是否还会不停左右反打、瞬间冲击是否明显降低、锁力是否仍够。如果仍冲击，下一步优先继续降低近目标起步到 `220` 或加入“小误差短刹车/回零等待”；如果不冲击但力弱，则在 `240` 和 `300` 之间继续微调。 |

2026-06-17 作废 LOCK_BASE 240 并退回 LOCK_BASE 300 基线记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不对，更差，没有上一版好，要退回上一版再改，现在又开始抖了” |
| 本次判断 | `LOCK_BASE=240` 且斜率 `>>8` 的减冲击方向错误，实际让锁止状态重新出现抖动，不能继续在该版本上叠改。 |
| 本次源码修改 | 退回上一版锁止基线：`LCDM_SERVO_LOCK_BASE_DUTY` 从 `240` 恢复为 `300`；LOCK 近目标 duty 斜率从 `>>8` 恢复为 `>>7`。其余 PWM 捕获、目标映射、行程、速度和显示不动。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19644 B`、`data=680 B`、`bss=1680 B`，总 `22004 B`。 |
| HEX/ELF SHA256 | HEX `963c189768afae8993ebe3541f60e31670de846aecc3a69eced81b0e4555ab2f`；ELF `0fc0c762cb236e1eadd7e99c950bc539202c028bcb374225fec585446e98e890`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004915`，返回 `Successfully resumed device`。 |
| 下一步原则 | 先以该 `300/>>7` 版重新确认现象。后续要减“瞬间冲击”时，不再简单降低整体锁力和斜率；应考虑只在跨过目标点或短时间连续反向切换时限幅/短刹车，保留静态反扭锁力。 |

2026-06-17 LOCK 保持锁力并加快外力响应版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我觉得是刹车不够快，反应不灵敏，可以改响应度更快的处理方向，锁止力目前看来够了。优化放手或反向扭舵板时的反应速度，这个可能与ADC采集有关系。” / “现在会过一段时间会嗞嗞声” |
| 本次判断 | 不再继续加大锁止力，也不沿 `240/>>8` 作废方向改。当前问题拆成两类：外力扭动/放手时 LOCK 反向出力太慢；静止一段时间后慢漂越过 HOLD 边界导致 H 桥重新进入 LOCK 嗞嗞。 |
| 本次源码修改 | 保持 `LOCK_BASE=300`、LOCK 斜率 `>>7` 和 `Lmax=930` 不变；LOCK 分支取消通用 `lcdm_servo_apply_slew()` 限速，直接输出计算 duty，让外力反向扭动时 H 桥更快反驱。新增 `LCDM_SERVO_HOLD_QUIET_DRIFT_BAND=44`、`LCDM_SERVO_HOLD_QUIET_DRIFT_VEL_Q4=16`，静止低速度的小慢漂继续保持 brake，不进入 LOCK 嗞嗞。曾尝试加入 HOLD/LOCK ADC 观测器快档，但因 Flash 超限先取消；后续若还需从 ADC 采集端提速，需要先再屏蔽更多 LCDM/调试字段或重整代码空间。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19640 B`、`data=680 B`、`bss=1680 B`，总 `22000 B`。 |
| HEX/ELF SHA256 | HEX `f14573a175562482750f5f31dc485eb85f4a04b666a9b06716bfd2577fc5059b`；ELF `2f5c95961db777712c56df2a02ca388363e1bffbb3f5e4f684502e840c9f3074`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004911`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us` 静止等待，观察是否还会过一段时间嗞嗞；再用手正反扭舵板后松手，看反向锁止是否更快、是否还会连续左右摆。若反应快但冲击过大，下一步不降锁力，而是在 LOCK 直接输出前加“跨零短刹车/一拍限幅”。 |

2026-06-17 轻扭抖动分段锁止版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还要继续优化，现在的情况是这样，用力扭感觉可以，锁止力也够，但轻扭时就不行，稍微用点力就开始抖了，是什么原因？” |
| 原因判断 | 强扭正常说明大误差锁止力足够。轻扭抖动的原因是 HOLD/brake 到 LOCK/drive 之间存在输出台阶：小误差刚跨过 HOLD 边界时，旧公式 `300 + error*(Lmax-300)/128` 会让 45 count 左右直接超过约 500 duty，从 0/brake 突然跳到大反驱，容易冲过目标后反向再打。 |
| 本次源码修改 | 保持强扭锁力上限不变，新增 LOCK 小误差分段：`LCDM_SERVO_LOCK_LIGHT_BAND=96`、`LCDM_SERVO_LOCK_LIGHT_BASE_DUTY=180`、`LCDM_SERVO_LOCK_LIGHT_GAIN=3`。`abs_error<=96` 时 duty 从 180 随误差线性增加；超过 96 后再从该段顶点继续拉到 `Lmax`。目标是轻扭时不突然大电流反打，重扭时仍能进入大锁力。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19668 B`、`data=680 B`、`bss=1680 B`，总 `22028 B`。 |
| HEX/ELF SHA256 | HEX `67461d0ba0d388d493d621860c44db39568ad29f5182fb32ffe248f75ccfef7f`；ELF `ac0cf1148e51f44e792316c2d9e045d1685ae0064483af2a96eecf8a5aec95a9`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800492d`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，先轻轻扭到刚有阻力的位置，看是否还会一用力就抖；再大力扭确认锁止力是否仍够。若轻扭不抖但感觉松，下一步只提高 `LIGHT_BASE` 或 `LIGHT_GAIN`；若仍抖，继续降低小误差斜率或加一拍 brake。 |

2026-06-17 外力锁止 ADC 快速采样版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “理论上1度都不能有，所以我还是觉得反应速度问题” / “别人的一动不动，正转不动，反转不动，用力马上大电流锁死” / “也许可以在静止没有PWM变化时一种ADC采样，有PWM变化时一种ADC采样，这样就不会冲突，这是我个的认为。” / “换句话说是外力引起的ADC做法不一样的采集方式” |
| 本次判断 | 用户判断成立：不能只靠提高最终锁止力，否则轻扭和嗞嗞声会互相冲突。应区分两种 ADC 场景：PWM 命令运动时要抗噪声平均；无 PWM 变化且处于 HOLD/LOCK 时，应视为外力扰动，使用更快 ADC 采样并更早反驱。 |
| 本次源码修改 | `adc_feedback.c` 新增 `ADC_Feedback_ReadFastRaw()`，快速路径只做一次 ADC 转换；ADC 采样时间从 `239.5 cycles` 缩短到 `41.5 cycles`。P50 控制环在 `HOLD/LOCK` 状态调用快速 ADC，其他运动状态仍调用原 `ADC_Feedback_ReadRaw()` 平均采样。HOLD 免费活动区同步收窄：`HOLD_EXIT_COUNT 4->1`、`HOLD_RELEASE_BAND 24->14`、`HOLD_QUIET_DRIFT_BAND 30->18`、`STATIC_LOCK_ERROR 28->18`、`MOTION_HOLD_BAND 24->16`。轻扭锁止公式压缩为 `E<=96: 220+4E`，`E>96: 412+2E`，仍受 `Lmax` 限制。 |
| 空间处理 | 由于 20KB Flash 已接近上限，压缩了 LOCK 分段公式，并让快速 ADC 路径沿用已选电位器通道，避免重复通道配置。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19672 B`、`data=680 B`、`bss=1680 B`，总 `22032 B`。 |
| HEX/ELF SHA256 | HEX `6d0b3758f711343fe7f32c4c06b47ebd0cd9934095b24f99ae96c23ba6ffe6f8`；ELF `e42c06696001b93bc206e355fdb68aecac7644482d4c459034d06010cf9403ef`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004931`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us` 不变，先轻轻正反扭，看是否能明显小于上一版约 5 度的可扭角；再逐渐加大外力，看是否能快速大电流锁死。若仍让位明显，下一步要继续把控制环从 1ms 降到更短周期或在 HBridge 定时中断中做锁止快速分支，而不是继续单纯加 LOCK duty。 |

2026-06-17 外力瞬间锁止曲线加陡版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有改善，还是反应慢了点，再优化变快，基本上是转不动为止” / “会不会现在反应快了，但锁止曲线太斜了，是否再陡一点，在扭动的瞬间？” |
| 本次判断 | 上一版外力 ADC 快速采样后已有改善，说明采样方向有效；剩余让位更像锁止曲线初段不够陡。原本小误差 duty 约为 `220+4E`，外力刚开始时给力仍偏慢。 |
| 本次源码修改 | 保持外力 ADC 快速采样、PWM 运动平均采样、HOLD 边界不变；LOCK 小误差曲线改为单一陡曲线 `duty = min(Lmax, 256 + 8*abs_error)`。这样 `E=18` 附近已约 `400 duty`，`E=48` 约 `640 duty`，`E=84` 附近接近 `Lmax`，目标是扭动瞬间更快进入大电流锁止。 |
| 空间处理 | 原计划 `260+6E` 因 20KB Flash 差 4 字节未通过，改用 `256+8E` 移位公式，体积更小且曲线更陡。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19660 B`、`data=680 B`、`bss=1680 B`，总 `22020 B`。 |
| HEX/ELF SHA256 | HEX `755603902528d13ceb0c3eb33c2ff8d61a3dd4737a38e5f4b98fb40628810dc2`；ELF `f0e6db244dbb50e57aee304fc83a6904520f0f843c1f591dbb907144a709102f`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004925`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us` 正反轻扭，观察是否接近“转不动”。若反应明显变快但开始嗞嗞或轻微抖，下一步不回退采样方式，而是在 `256+8E` 前增加一小段速度判定或把曲线降到 `256+6E`，同时需再腾出 Flash 空间。 |

2026-06-17 轻搭舵板临界抖动阻尼版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “情况有显著改善，但轻碰正反转还是会抖，要改良” / “就相当于我搭在舵板上似动非动，会一直抖，找不到点” / “也许这个处理好，也是可以处理在临界状态下找不到点的问题，就是嗞嗞声问题” |
| 本次判断 | 现象集中在固定 PWM、目标点附近、外力轻搭造成 ADC 刚好越过 LOCK 边界。上一版 `256+8E` 让外力瞬间锁止更快，但小误差低速区仍可能正反驱动来回切换，形成轻碰临界抖动和到点嗞嗞。本版不改 PWM 捕获、不改目标 ADC 映射、不降大误差锁止力，只在 LOCK 近零低速时直接刹停 H 桥，避免继续正反找点。 |
| 本次源码修改 | `src/app_entry.c` 的 LOCK 分支新增近零阻尼：当 `abs_error <= 36` 且 `abs_velocity_q4 <= 28` 时，执行 `HBridge_Brake()`，清零当前 duty/参考速度，并发布 `LCDM_SERVO_V3_EVENT_RETURN_DAMP`；其余外力偏移仍沿用上一版强锁曲线 `duty = min(Lmax, 256 + 8*abs_error)`。曾尝试再加“回目标方向小误差刹车”分支，但 Flash 超限 36B，最终保留最核心低速刹停分支。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19672 B`、`data=680 B`、`bss=1680 B`，总 `22032 B`。 |
| HEX/ELF SHA256 | HEX `b87b8bf5623caaeb5e49dddd8f0aa0bc710363577da1639c9aef63d27e6bdb9a`；ELF `466cc7b3b9eae73cd9f6ba638efdc5b5614fab843f671c1fa324ac737f8b1259`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004931`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，轻轻把手搭在舵板上让它处于似动非动的临界状态，观察是否还会一直抖、是否还会找不到点产生嗞嗞声；再稍微加大外力确认是否仍能快速进入大电流锁止。若仍轻碰抖，下一步需要做 LOCK 状态滞回/方向保持，避免在临界误差附近每 1ms 反复翻方向。 |

2026-06-17 撤销低速刹停并降低临界起步冲击版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不行，没改好” / “还是一样” / “更严重” |
| 本次判断 | 上一版在 `abs_error<=36` 且速度低时直接刹停，实际没有解决临界找点，且用户反馈更严重。说明问题不能用“同向小误差低速切断输出”处理，否则轻搭时会在 Brake/LOCK 之间来回进出。 |
| 本次源码修改 | 撤销上一版 LOCK 近零低速刹停分支；曾尝试加入“跨零不立刻反打”的方向滞回，但当前 20KB Flash 超限 16-40B，未下载。最终采用不增加代码体积的曲线修正：`LCDM_SERVO_LOCK_LIGHT_BASE_DUTY` 从 `256` 降到 `224`，保留 `+ 8*abs_error` 斜率，目标是降低临界轻碰起步冲击，同时保留较快进入大误差锁止的能力。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19656 B`、`data=680 B`、`bss=1680 B`，总 `22016 B`。 |
| HEX/ELF SHA256 | HEX `40a358c8eedf2a3df0ddde158aaaed50f3c4d7c51cc3e39f807e1e232766deed`；ELF `c994b47e6921a86091e307cb6433a3ebce1b4964ac9cec55af7817acbee0f71a`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004921`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，重复上一轮轻搭舵板临界测试，先确认是否比低速刹停版减轻；再用稍大外力确认锁止力是否仍够。如果抖动仍无改善，下一步不能继续只调基准 duty，应先腾出 Flash 空间，把真正的 LOCK 方向滞回/反向确认计数写进去。 |

2026-06-17 外力偏移后打满锁止力版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “暂时不抖，锁止力也有，但还是能扭得动，说明扭动时没有用最大的扭止H桥PWM输出力” |
| 本次判断 | `224+8E` 版解决了临界抖动，但外力继续扭动时没有尽快拉到最大 H 桥 PWM。本轮保持轻碰临界不抖的起步曲线，只在偏移超过小范围后直接打满 `Lmax`。 |
| 本次源码修改 | LOCK 分支改为两段：`abs_error <= 24` 时仍用 `224 + 8*E`；`abs_error > 24` 时直接 `lock_max`。HOLD 门槛暂未收窄。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19632 B`、`data=680 B`、`bss=1680 B`，总 `21992 B`。 |
| HEX/ELF SHA256 | HEX `8e8fb4789bb7f7f355d7efc83d163a1e5ac56661e5583e77f852a28f99ba914e`；ELF `1faa952fe3ac60debf99578936b874ad4a4a89702979b40d0ed800e4ed978ec8`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004909`，返回 `Successfully resumed device`。 |
| 后续状态 | 用户进一步明确目标是“一点扭动都不可能，立即锁死”，因此该版已被下一版 `E>8` 极早锁止版替代。 |

2026-06-17 极早最大锁止版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “要做到，一点扭动都不可能，立即锁死的效果” / “别家的样品就是这种效果” |
| 本次判断 | 只把大误差打满不够，HOLD 状态本身还允许 14-18 count 左右的偏移才进入 LOCK。要接近样品效果，必须同时收窄 HOLD 自由区，并把 LOCK 打满阈值前移到接近 ADC 噪声上方。 |
| 本次源码修改 | `LCDM_SERVO_HOLD_RELEASE_BAND 14->6`，`LCDM_SERVO_HOLD_QUIET_DRIFT_BAND 18->8`，`LCDM_SERVO_STATIC_LOCK_ERROR 18->8`，`LCDM_SERVO_V3_MOTION_HOLD_BAND 16->8`，P70 `Rel` 调整范围改为最小 `4`、步进 `1`；LOCK 分支 `abs_error > 8` 即直接 `lock_max`，否则仍用 `224 + 8*E`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19632 B`、`data=680 B`、`bss=1680 B`，总 `21992 B`。 |
| HEX/ELF SHA256 | HEX `d0606680722f46d393a0d1225e4a067e50395af7688ae3bae5649d5bd6b551ec`；ELF `e08888e6f8a0eb78fd4eb4b91cf387f535cca8fc29464cfad85e27b55e7432b8`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004909`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，先不要大幅扭动，轻碰确认是否重新嗞嗞/抖；再逐渐加力测试是否接近“一点扭动都不可能、立即大电流锁死”。如果这版重新抖，说明阈值低于实际 ADC/机械噪声，需要回到 `E>24` 或加真正的方向滞回；如果不抖但仍能扭动，则需继续提高 `Lmax` 或检查 H 桥/供电实际电流限制。 |

2026-06-17 今日结束退回 E24 版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不行，不用动都在抖了，退回到上一版，今天到此为止” |
| 本次判断 | `E>8` 极早最大锁止版把 HOLD/LOCK 门槛压到实际 ADC/机构噪声以下，导致静止不动也抖，方向错误，必须退回上一版。 |
| 本次源码修改 | 恢复上一版参数：`LCDM_SERVO_HOLD_RELEASE_BAND=14`，`LCDM_SERVO_HOLD_QUIET_DRIFT_BAND=18`，`LCDM_SERVO_STATIC_LOCK_ERROR=18`，`LCDM_SERVO_V3_MOTION_HOLD_BAND=16`，P70 `Rel` 最小 `8`、步进 `2`；LOCK 打满阈值恢复为 `abs_error > 24`，保留 `abs_error<=24` 使用 `224 + 8*E`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19632 B`、`data=680 B`、`bss=1680 B`，总 `21992 B`。 |
| HEX/ELF SHA256 | HEX `8e8fb4789bb7f7f355d7efc83d163a1e5ac56661e5583e77f852a28f99ba914e`；ELF `1faa952fe3ac60debf99578936b874ad4a4a89702979b40d0ed800e4ed978ec8`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004909`，返回 `Successfully resumed device`。 |
| 今日结论 | 今天结束版本固定为 `E>24` 打满锁止力版。下一次继续时不要再直接把 HOLD/LOCK 阈值压到 `8`；应先查实际 ADC 静止噪声、H 桥真实输出 duty/电流，再考虑用方向滞回或短时间确认计数处理“一点扭动即锁死”。 |

2026-06-18 V4 标准模式分层预编译版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “太赞了，就是要有这样的魄力来改善。今天没有带实物，请先开始大改V4架构，等有硬件了，立马测试。” / “不要忘记，一定要按市面上标准的做法来完成程序，不要走变路” / “弯路” |
| 本次判断 | 前面局部调参已经证明不能继续把运动、静止保持、外力锁止混在一个误差公式里。V4 按市面舵机常见结构先拆模式：PWM 变化时走 MOVE 位置/速度控制；PWM 不变且到位时走 HOLD，不追 ADC 小漂移；外力导致 ADC 连续偏移时进入 DISTURB/LOCK，快速反向锁止，并用方向保持/反向滞回避免轻微跨零正反抖。 |
| 本次源码修改 | `src/app_entry.c` 新增 V4 扰动门槛：`LCDM_SERVO_V4_DISTURB_ENTER_BAND=24`、`LCDM_SERVO_V4_DISTURB_CONFIRM=2`、`LCDM_SERVO_V4_DISTURB_REV_BAND=48`。HOLD 分支改为先忽略 release/noise 范围内的偏移，超过扰动门槛后需连续确认 2 次才进入 LOCK。LOCK 分支改为 DISTURB 模式：保留 `E<=24` 的 `224+8E` 轻锁曲线，`E>24` 直接 `lock_max`；若 ADC 轻微跨过目标反向且误差小于 48，则只刹车不马上反向打，防止轻触时正反翻方向。 |
| 显示/数据修改 | P50 HBR 行在 LOCK/DISTURB 驱动时显示 `DST`，普通运动仍显示 `D+`/`D-`，HOLD 显示 `HLD`，以后看屏幕或 HTML 曲线能区分 MOVE/HOLD/DISTURB。曲线事件仍使用已有事件号，`LOCK` 对应外力锁止，`CROSS_BRAKE` 对应跨零刹车。 |
| 未做事项 | 今天无实物，未下载。未改变 PWM 捕获、目标 ADC 映射、H 桥引脚、ADC 快速采样函数和 P70 参数表默认范围。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19644 B`、`data=680 B`、`bss=1680 B`，总 `22004 B`。 |
| HEX/ELF SHA256 | HEX `47814d08a122883d2cf9f422346d473184e28b34b85c7891c94fd756c86b53dc`；ELF `e2dcef97c3f271f1459d4bf33a806be0ba9d38cb257df5380f234da533c4b873`。 |
| 下载状态 | 未下载，等待用户带硬件后再刷入测试。下一次测试优先看固定 `1500us` 时 P50 是否稳定显示 `HLD`，轻扭是否进入 `DST`，轻微跨零是否显示刹车而不是正反抖。 |

2026-06-18 V4 架构复核及参数化版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “重复验证一下，这样的V4架构是否可以彻底解决锁止力和抖动问题？而且将来有很多细调参数是否兼容？所有变量应该在写程序时都会变成是可调的变量。” |
| 复核结论 | V4 架构方向正确，但未经过实物验证前不能承诺“彻底解决”。它能针对根因处理：HOLD 不追 ADC 小噪声，DISTURB/LOCK 单独负责外力锁止，跨零小误差先刹车不马上反打。这样比 V3 单一误差公式更适合同时解决锁止力和轻触抖动。 |
| 本次参数化修改 | 为兼容后续细调，V4 主调参数接入现有 P70：`Hbd` 推导扰动进入门槛，实际 `disturb_enter_band = Hbd + 8`；`Lmin` 从原未使用的静态锁止最小值改为轻锁起步 duty，默认 `224`；`Lmax` 继续作为最大锁止 duty；`disturb_rev_band = disturb_enter_band * 2`。保留 `DISTURB_CONFIRM=2` 为内部默认，后续若 Flash 空间允许再独立成 P70/P31 可调项。 |
| 当前可调项 | P30: `Vmax/Acc/FF/Kv/Slew` 调 MOVE 速度环；P31: `Bmin/Bg/Bmax/Bv/Bhd` 调 MOVE 刹车；P70: `Hbd/Hvel/Rel/Lmin/Lmax` 调 HOLD/DISTURB/LOCK；P40: `Inv/Dir/Hz/Max/Sta/Stm` 调 H 桥方向、频率、最大 duty。 |
| 仍需实物确认 | 静止 ADC 噪声窗口、H 桥实际输出 duty/电流、`DST` 进入是否及时、跨零刹车是否消除轻触正反抖、`Lmax` 是否真的能产生足够堵转力。若硬件电流限制或 H 桥保护限制住，即使软件打满也不会达到样品锁止力。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19664 B`、`data=680 B`、`bss=1680 B`，总 `22024 B`。 |
| HEX/ELF SHA256 | HEX `c308cb189480515f88b7da34d05458c1fd07560930b10f20e419dd69ccd6d0eb`；ELF `299866e18d82bce867cde37db4884b350dc789d64f9b6cb7fb54ba8a62287691`。 |
| 下载状态 | 用户接入 DAPLink 后已下载。Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004925`，返回 `Successfully resumed device`。下一步现场测试优先看固定 `1500us` 时 P50 是否稳定 `HLD`，轻扭是否进入 `DST`，以及锁止时 `Lmax` 是否能产生足够实际电流。 |

2026-06-18 V4 弹簧阻尼锁止版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “二种情况，一种是当你可以扭动时就很容易正反扭转锁力不够。另外一种是锁力大时，又会不停的正反抖。还有就是正常1000-1500之间切换都会来回抖。” |
| 本次判断 | V4 参数化版仍存在两种相互牵制的失败模式：小锁力时外力能扭动，大锁力时硬阈值反打会正反抖；普通 `1000us` 到 `1500us` 的目标切换也会因回程/反打过强出现来回摆。根因仍是 LOCK/DISTURB 直接用硬阈值和打满输出，缺少速度阻尼。 |
| 本次源码修改 | `src/app_entry.c` 的 LOCK/DISTURB 分支由硬切换改为“位置误差弹簧力 - ADC 速度阻尼”的 PD 锁止：误差决定反向锁力，ADC 速度项抵消冲过目标的动能；当小误差下计算结果准备反向打时，先 `HBridge_Brake()` 而不是立刻反向驱动。同步降低 RETURN 回拉力度：`RETURN_MIN_DUTY 105->80`、`RETURN_MAX_DUTY 300->220`、`RETURN_NEAR_DUTY 135->105`，目标是降低 `1000-1500` 切换后的回摆。 |
| 代码空间处理 | 新增公共 `lcdm_servo_stop_publish()`，合并多处刹车/释放/曲线发布重复代码；`Makefile` 链接改为裸机常规 `-nostartfiles`，并在 `app_entry.c` 提供空 `_init/_fini`，去掉不需要的 GCC 启动文件占用。P50 HBR 行去掉未显示的 ref_velocity 缓存比较，不影响曲线数据里的 `ref_velocity`。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19608 B`、`data=668 B`、`bss=1652 B`，总 `21928 B`。 |
| HEX/ELF SHA256 | HEX `bea707cd0a2dfc1cde5e628fa61ca50e72e580dc767eaed613b51bcba7d9afbb`；ELF `bde4b63d23bf614dd5488925f6ce5f424993d70102ffbf32e40a139490ee1f5c`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004905`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 先固定 `1500us` 不碰，确认是否稳定 `HLD`、是否还有嗞嗞；再轻扭看是否进入 `DST` 后有阻尼锁住而不是正反抖；再做 `1000us <-> 1500us`，观察是否从“来回抖几下”变成一次过冲或直接停。如果仍弱锁，可提高 P70 `Lmin/Lmax`；如果仍正反抖，优先加大 P31 `Bg` 阻尼或降低 P70 `Lmin`，不要再直接把阈值压低。 |

2026-06-18 V4 静止杂讯去嗞嗞版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前基本可以，但还有一点眯嗞嗞声，应该检测到杂讯” |
| 本次判断 | 现在大动作基本可用，剩余轻微嗞嗞更像 HOLD/DISTURB 边界被 ADC 零星杂讯触发。代码检查发现 HOLD 安静分支在 `abs_error <= disturb_enter_band` 且速度低时会继续 HOLD，但没有清零 `s_lcdm_servo_hold_deviation_count`；零星杂讯可能累计到 2 次后误入 LOCK。另外一旦误入 LOCK，原逻辑只有回到 `Rel` 内才 HOLD，`Rel..Hbd+8` 之间仍可能输出小 duty 找点。 |
| 本次源码修改 | `src/app_entry.c`：HOLD 安静分支增加 `s_lcdm_servo_hold_deviation_count = 0`，避免零星杂讯累积；LOCK 分支的安静退出条件从 `abs_error <= hold_release_band && vel<=Hvel` 放宽到 `abs_error <= disturb_enter_band && vel<=Hvel+8`，如果只是杂讯带入 LOCK，会立刻回 HOLD，不再小电流嗞嗞。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19612 B`、`data=668 B`、`bss=1652 B`，总 `21932 B`。 |
| HEX/ELF SHA256 | HEX `c858ad8aa8c0da2afd66d7c02e794bee2c5f4f4b81207684e96c743d6c4ea1f3`；ELF `768d841cd1bbb5e1a53a615cfc9337fbb5d44c7aab454c1adb94e6dc9cb2bfd5`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004909`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us` 和 `1000/1500/2000us` 到位后各等 30-60 秒，观察 P50 是否长期保持 `HLD`、HBR 行是否还偶发 `DST` 或小 duty。如果嗞嗞明显减少但锁止轻扭变软，再把 P70 `Hbd` 降 1-2 或 `Lmin` 升一点；如果仍偶发嗞嗞，下一步记录 P50 的 ADC `W` 和 HBR 事件，按真实 W 值设置独立噪声门槛。 |

2026-06-18 V4 静止外力强锁无低 PWM 试探版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “处力干扰还是没有解决好，轻加小力在舵板上就嗡嗡叫，别人的样子是没有任何一点声音，也转不了一点，只要一出力直接锁死，不会提前嗡嗡发声，应该是马达反向PWM，所以这个静止算法还是没有达到别人样机的境界。” / “解决了这个问题，也许就解决了嗞嗞声” / “嗞嗞声通常是因为外力转动后留下的。正常用外信号PWM控制，比较不容易嗞嗞声” |
| 本次判断 | 用户补充说明问题主要来自外力扰动后的静止锁止/回位收尾，不是 PWM 命令运动段。上一版仍有“确认外力后按误差逐渐给反向 PWM”的路径，小外力时会先给低 duty 试探，表现为嗡嗡声。样机行为应是静止无声，确认外力后直接形成强锁墙，不经过低 PWM 嗡嗡区。 |
| 本次源码修改 | `src/app_entry.c` 只改静止 `LOCK/DISTURB` 分支：删除 `lock_pd = error*P - velocity*D` 渐进锁止输出，删除 `lock_min_duty/Lmin` 在本分支的低 PWM 试探作用；安静区仍直接回 `HLD`，靠近目标且正向回位时仍 `HBridge_Brake()` 防止过冲；确认外力后直接输出 `+/-Lmax` 强锁。运动段 `MOVE/TRACK/BRAKE/RETURN` 未改。 |
| 当前参数含义 | 这一版静止锁止主要看 P70 `Hbd/Hvel/Rel/Lmax` 和 P40 `Max`。P70 `Lmin` 在本版静止 LOCK 中暂不生效，保留显示项但不参与低 PWM 试探；若本方向确认正确，后续可把 `Lmin` 改名或改成强锁确认/保持相关参数。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19472 B`、`data=668 B`、`bss=1652 B`，总 `21792 B`。 |
| HEX/ELF SHA256 | HEX `498b2407543357d1e512659bbe867d399a8a1900c6707bada899c1083ae4a967`；ELF `e37e8b67d411fbf9095a32c0e1215b003d1bc6e2247eadc9e9706d28868ef010`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800487d`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，轻轻加力到舵板：理想表现是不提前嗡嗡，越过确认门槛后直接强力顶住；松手后应回到 `HLD`，不留下持续嗞嗞。再测试外部 PWM 正常 `1000/1500/2000us` 切换，确认运动段没有被本次静止分支修改影响。如果这版出现强锁冲击后反弹，下一步只在强锁回目标附近加“短刹车保持时间”，不要恢复低 PWM 试探。 |

2026-06-18 V4 静止强锁自由区修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不对呢，又回复了之前可以正反转动了，出什么问题了呢” |
| 本次原因 | 上一版为了去掉外力后的低 PWM 嗡嗡，把 `abs_error <= Hbd+8` 且速度低的区域全部作为安静 `HLD` 区。这样虽然减少小 duty 声音，但等于给舵板留下 `Rel..Hbd+8` 的自由可扭动区，所以外力轻推会回到“可以正反转动”的旧问题。 |
| 本次源码修改 | `src/app_entry.c`：HOLD 自由静音区恢复只看 P70 `Rel`，超过 `Rel` 后不再因为低速度和 `Hbd+8` 放行，而是经过 `DISTURB_CONFIRM=2` 确认后进入静止强锁；LOCK 回 `HLD` 的条件也恢复为 `abs_error <= Rel && vel <= Hvel`。仍保留上一版“确认外力后直接 `Lmax` 强锁，不走低 PWM 试探”的方向。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19452 B`、`data=668 B`、`bss=1652 B`，总 `21772 B`。 |
| HEX/ELF SHA256 | HEX `5ed8efb36dd7352daf6811457b7cfad386de708463646d37c20e14f465858547`；ELF `817da467a9df4d09ab95b4317deb179efcc114eccce97d4c69bb620523ca8dfa`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004869`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，轻轻左右加力，确认不再有一段明显自由可转区域；如果能顶住但出现短促强锁冲击，下一步只加“强锁后短刹车保持/回 HLD 延时”，不再放宽自由区到 `Hbd+8`。 |

2026-06-18 退回 V4 静止杂讯去嗞嗞稳定基线记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “越改越坏了，会自动摆了” |
| 本次原因 | 最近两版“确认外力后直接 `Lmax` 强锁”的方向过激。第一版为了消嗡嗡放宽了 `Hbd+8` 自由区，导致舵板可正反转动；第二版收回自由区后，`Lmax` 强锁又使静止环路过激，出现自动摆。说明在当前机构/ADC/驱动条件下，不能直接从 `HLD` 跳到满力强锁。 |
| 本次源码修改 | 明确作废最近两版强锁尝试，恢复到用户反馈“目前基本可以，但还有一点嗞嗞声”的 V4 静止杂讯去嗞嗞版：HOLD 安静区保留 `disturb_enter_band` 低速回 `HLD` 并清零扰动计数；LOCK 分支恢复 `lock_pd = error*P - velocity*D` 的阻尼锁止；小误差反向趋势先 `HBridge_Brake()`，不直接反打；恢复 P70 `Lmin` 在 LOCK PD 中参与计算。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19612 B`、`data=668 B`、`bss=1652 B`，总 `21932 B`。 |
| HEX/ELF SHA256 | HEX `c858ad8aa8c0da2afd66d7c02e794bee2c5f4f4b81207684e96c743d6c4ea1f3`；ELF `768d841cd1bbb5e1a53a615cfc9337fbb5d44c7aab454c1adb94e6dc9cb2bfd5`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004909`，返回 `Successfully resumed device`。当前板上版本已退回该稳定基线。 |
| 下一步原则 | 先确认自动摆已消失。后续若继续处理外力后的嗞嗞，不能再直接满力强锁；应在此基线基础上做很小改动，例如只调 P70 `Lmin/Hbd/Rel` 或增加短时间 HOLD 延时/事件记录，且每次只改一个变量。 |

2026-06-18 V4 马达 PWM 低频验证版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有一个想法不知道是否正确，在外力作用下时，PWM频率是否可以调到最低频率，是否能解决嗞嗞声，同理，在没有达到位置一直在找位置时也是发出嗞嗞声这个情况下是否也可以降低PWM轻转到位？这只是我个的想法” / “这个是马达PWM” |
| 本次判断 | 用户指的是 H 桥马达驱动 PWM，不是外部输入 PWM。这个想法值得做临时验证：低频可能改变马达低速力矩和声音，但如果根因是目标点附近反复正反输出，只改频率不一定根治。因此本版只在嗞嗞相关状态临时降频，不动正常 PWM 指令运动段。 |
| 本次源码修改 | `src/app_entry.c` 新增 `LCDM_SERVO_QUIET_HBRIDGE_HZ=330` 和 `lcdm_hbridge_set_frequency_cached()`。正常 `MOVE/TRACK/BRAKE` 输出前仍使用 P40 `Hz`；外力锁止 `LOCK/DST` 和到点后回找 `RETURN` 输出前临时切到 `330Hz`。如果上一周期已经在 `LOCK/RETURN`，本周期开头也保持低频；普通运动输出前会切回 P40 正常频率。 |
| 未改事项 | 不改输入 PWM 捕获；不改 ADC 过滤；不改 `LOCK` 的 PD 阻尼公式；不改 `RETURN` duty 曲线；不新增菜单项。P40 `Hz` 仍控制正常运动频率，本版 330Hz 是固定验证值。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19516 B`、`data=664 B`、`bss=1656 B`，总 `21836 B`。 |
| HEX/ELF SHA256 | HEX `d5765965a8a132b78966a26ac945be17ed0c212d7962adcb4eb6e498a48db090`；ELF `6b369ef69ffe500370f70040e53ded6d151d5162a8318e4986504f697403fecd`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048a9`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us` 后外力轻扭，听嗞嗞是否变小或变成低频声音；再测试到点后小范围找位置是否更柔和。必须同时观察锁止力是否变差、是否更容易能扭动、是否又出现自动摆。如果低频只改变声音但不解决动作，下一步要回到控制逻辑；如果明显改善，再把低频值做成 P40/P70 可调参数。 |

2026-06-18 V4 整机马达 PWM 100Hz 试验版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “如果要改程序，顺便干脆把马达PWM改成100HZ试一下” |
| 本次判断 | 前一版只在 `LOCK/RETURN` 临时降到 330Hz，用户测到一次外力扭动后失衡并找到松动点，可能是状态切频和低频脉冲共同引入副作用。为把变量单纯化，本版取消状态切频，直接把整机马达 H 桥 PWM 默认改为 100Hz。 |
| 本次源码修改 | `src/app_entry.c`：P40 `Hz` 默认值从 `1000` 改为 `100`，范围从 `330..1000` 改为 `100..1000`、步进 `100`；删除上一版 `LOCK/RETURN` 专用低频切换 helper 和所有 `quiet_frequency_hz` 调用。控制环恢复统一按 P40 `Hz` 设置 H 桥频率。 |
| 未改事项 | 不改输入 PWM；不改 ADC；不改 HOLD/LOCK/RETURN 控制公式；不改 duty；不改方向。当前只验证马达 H 桥 PWM 频率从 1kHz 改 100Hz 的效果。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19612 B`、`data=668 B`、`bss=1652 B`，总 `21932 B`。 |
| HEX/ELF SHA256 | HEX `6557f16084a6f95db14d2644f2a7ae7946b25ab9b29160bc7c36ac24cffc0130`；ELF `dd39d917b09419982c159b99bcf827ea6397784b5f285e48becf53d67ab782bb`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004909`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 先不要调其它参数，确认 P40 `Hz` 显示为 `100`。测试外力轻扭、到点后找位、正常 `1000/1500/2000us` 切换三项；重点观察声音是否只是变低频、锁力是否下降、是否还会失衡或自动摆。 |

2026-06-18 恢复马达 PWM 1000Hz 并确认频率非根因记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不关马达的频率，还是逻辑问题，可以恢复了。然后再找原因。别人的样板都是一动不动的，没有任何声音，说明他的感知反应能力是相当快的反顶住而不反推。请重新理清逻辑。” |
| 本次判断 | 100Hz/330Hz 马达 PWM 低频试验没有解决根因，反而引入失衡、松动点和可来回转动的副作用。根因仍在静止锁止逻辑：外力触发后要快速感知并反顶住，但不能变成主动反推、追点或来回找点。 |
| 本次源码修改 | `src/app_entry.c`：P40 `Hz` 默认/范围恢复为 `1000, 330, 1000, 670`；确认上一版临时 `LOCK/RETURN` 低频切换 helper 和 `quiet_frequency_hz` 调用均已移除。除恢复 100Hz 试验外，不改 HOLD/LOCK/RETURN 控制公式。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19612 B`、`data=668 B`、`bss=1652 B`，总 `21932 B`。 |
| HEX/ELF SHA256 | HEX `c858ad8aa8c0da2afd66d7c02e794bee2c5f4f4b81207684e96c743d6c4ea1f3`；ELF `768d841cd1bbb5e1a53a615cfc9337fbb5d44c7aab454c1adb94e6dc9cb2bfd5`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004909`，返回 `Successfully resumed device`。当前板上版本已恢复到 V4 静止杂讯去嗞嗞稳定基线。 |
| 下一步原则 | 从该稳定基线继续处理静止锁止：不能再直接跳满 `Lmax`，也不能放宽自由区；应改成“快速识别外力位移/速度、同方向建立阻尼墙、停止时立刻刹住并退出输出”，避免低 PWM 嗡嗡和反向追点。 |

2026-06-18 V4 静止反顶不反推小改版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不关马达的频率，还是逻辑问题，可以恢复了。然后再找原因。别人的样板都是一动不动的，没有任何声音，说明他的感知反应能力是相当快的反顶住而不反推。请重新理清逻辑。” |
| 本次判断 | 稳定基线的 `LOCK` 静止段仍按 `error*P - velocity*D` 做回位控制，外力后容易把小偏差当成运动目标去追，表现为低 PWM 嗞嗞、反推、过目标再找点。样机更像是：轻微扰动先用 H 桥刹车形成无声阻尼，只有持续被推离目标才输出反顶力。 |
| 本次源码修改 | `src/app_entry.c`：只改静止 `LOCK` 分支。保留原 `HLD` 进入条件和原 `lock_pd` 公式；新增 `quiet_lock_band=2*Hbd`、`quiet_lock_vel=Hvel+8`，当偏差在二级小范围且速度低时直接 `HBridge_Brake()`，不输出马达 PWM；当 ADC 已朝目标回来的小范围内，也刹车而不是继续推。未改马达 PWM 频率，P40 `Hz` 仍为 `1000`。 |
| 特别说明 | 之前“直接 `Lmax` 强锁”和“放宽自由区”都已证明方向不对，本版不走这两条路。它是小步验证：用刹车保持替代小 duty 嗡嗡，观察是否更接近“反顶住、不反推”。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19680 B`、`data=668 B`、`bss=1652 B`，总 `22000 B`。 |
| HEX/ELF SHA256 | HEX `8789d46aa915b887e699f9a83fbeaefdf53bb4ada5c36e945184b931d8022bdd`；ELF `747f8fca31ee3d3cc2f933db486a75a5293609acb2bbeec8b24da27c941b5736`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800494d`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us` 先不碰 20-30 秒，听是否无声；再轻搭舵板似动非动，确认是否由嗞嗞变成无声刹车阻尼；然后稍大力左右扭，确认是否仍会反顶并回到原点。如果变软或能转动角度变大，说明刹车窗口太宽，需要缩小 `quiet_lock_band`；如果仍嗞嗞，说明进入的是超过二级门槛后的 `lock_pd` 输出，需要改强锁/回位状态拆分。 |

2026-06-18 V4 H 桥锁止刹车衰减验证版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “失败的，轻动舵板就嗞嗞响” / “这里面的逻辑还是不对” |
| 失败结论 | 上一版“小偏差刹车窗口”失败，原因是把 `HBridge_Brake()` 当成锁止力使用。刹车只是短接阻尼，不是主动反顶；轻动舵板时仍会在刹车和重新进入 `LOCK` 输出之间切换，所以嗞嗞没有消失。该版本逻辑已撤回。 |
| 本次判断 | 重新检查 H 桥底层后发现：当前软件 PWM 在 on 段驱动马达，在 off 段一直 `Coast` 滑行。这样静止反顶时每个 PWM 周期都“推一下、放一下”，锁止会软，也容易让 ADC 继续变化并反复触发嗞嗞。标准锁止更应使用刹车衰减或同步衰减，让 off 段也保持阻尼。 |
| 本次源码修改 | `inc/hbridge.h`、`src/hbridge.c`：新增 `HBridge_SetPwmOffBrake(uint8_t enabled)`，当 enabled=1 时，PWM off 段从 `apply_coast()` 改为 `apply_brake()`；默认仍是滑行。`src/app_entry.c`：仅静止 `LOCK` 输出前设置 off=刹车；正常 `RETURN` 和 `MOVE/TRACK` 输出前设置 off=滑行。未改变马达 PWM 频率、ADC 过滤、LOCK 的 `lock_pd` 公式。 |
| 编译结果 | Ubuntu 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19684 B`、`data=668 B`、`bss=1652 B`，总 `22004 B`。 |
| HEX/ELF SHA256 | HEX `81db9101eab3002a965f4768e4726716610f14a198d259dd4e5fe2902ab0436e`；ELF `94ba73c85b08ebeba5173ae94f17f201484ece981326c8d0380899111abc1e2b`。 |
| 下载结果 | Ubuntu pyOCD 使用 SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004951`，返回 `Successfully resumed device`。 |
| 用户测试重点 | 固定 `1500us`，轻动舵板测试是否从“嗞嗞找点”变成更硬的反顶阻尼；再看是否有发热、电流明显升高或自动摆。如果这版仍轻动嗞嗞，下一步应集中在 `LOCK` 进入/退出迟滞和最低 duty 门槛，不再改 H 桥频率或刹车窗口。 |

2026-06-18 SVG/SWD 统计后修正静止 LOCK 迟滞记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我老是试到刚开始可以正反扭得动，然后再试几下扭不动。但轻扭还是抖。我建议你还是用回之前的SVG观察统计模式，自行核查发生了什么事。不然这个调试一直又像之前没改V4时一样，没有任何进展。” / “这里面的逻辑还是不对” |
| 本次数据结论 | 已恢复并改良 `tools/live_servo_curve_swd.py` 的 SVG/SWD 采集：原 H桥大图只按外部 PWM/目标变化触发，已改为外部 PWM 变化或静止外力触发 H桥输出都能抓图。第一批统计 `3145` 行显示：`pwm_us` 稳定为 `1504`，`target_adc` 稳定为 `2055`，但 `feedback_adc=2028..2077`，误差 `-22..+27`；其中 `HLD/event2` 为 `3017` 行，真正 `LOCK/event16` 只有 `3` 行。结论：程序把太大的外力偏移仍当成安静区放过，所以刚开始可正反扭动。 |
| 第一次源码修改 | `src/app_entry.c`：去掉 HOLD 内 `abs_error <= Hbd+8` 且低速就直接回 `HLD` 的大安静区；LOCK 退出条件从 `abs_error <= Hbd+8` 收回到 `abs_error <= Rel`。目的：超过 `Rel` 后必须进入 `LOCK`，不能在大范围里放手。 |
| 第一次编译/下载 | 构建通过，`text=19664 B`、`data=668 B`、`bss=1652 B`。HEX `bae2ffda2537a86cc4138867e3366b33fd88d784a540f6d4863c155bbe9242f3`；ELF `a45eb4f8179b57dc97269aadd5f1191c6cd8ebb455377d2b30f87c4eb18ee73a`。该版本已下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800493d`，返回 `Successfully resumed device`。 |
| 第二批数据结论 | 新版采集 `2532` 行显示：`pwm_us=1503`、`target_adc=2052` 稳定；`event16` 从 `3` 行增加到 `9` 行，但仍有大量 `event18`/`event17` 且 duty=0。关键片段出现 `error=+16..+26`、`velocity_q4=-64..+2`，即反馈正被外力推离目标，程序却因方向翻转保护进入纯刹车，不给反顶力。 |
| 第二次源码修改 | `src/app_entry.c`：LOCK 里的方向翻转刹车条件增加“ADC 正朝目标回来”限制。若 ADC 正被外力推离目标，不再触发 `event18` 纯刹车，而允许进入 `event16` 反顶 duty。`inc/hbridge.h`、`src/hbridge.c` 保留上版的 LOCK PWM off 段刹车衰减能力。 |
| 第二次编译结果 | 构建通过，`text=19684 B`、`data=668 B`、`bss=1652 B`。HEX `311991a813aa339671a2803a4465a878b14febd1842231b0102e667b31376c9f`；ELF `2fbc4cd92a6e4cb494346f730fc8cdc7431df3015c288b866405de029075eb8f`。 |
| 第二次下载状态 | 未下载成功。pyOCD/DAPLink 报 `expected DAP_INFO`、`expected DAP_SWJ_CLOCK`，Ubuntu `dmesg` 同时出现 `xhci_hcd ... HC probably busted`。已执行 Ubuntu `usbreset 0d28:0204`、Parallels `--device-disconnect/--device-connect`，仍未恢复。当前实物板上仍是第一次已成功下载的 `bae2...` 版本，不是最新 `3119...`。 |
| 工具脚本修正 | `tools/flash_run_in_ubuntu.sh` 已修正：以后 pyOCD 即使返回 exit code 0，也必须在输出中看到 `Successfully resumed device` 才认为下载成功，避免把 DAPLink 初始化错误误判成下载成功。 |
| 下一步 | 需要物理拔插 DAPLink 或重启 Ubuntu USB 通道后，先下载 `3119...` 版本，再重新开 SVG 采集。测试重点看：轻扭时 `event16` 是否明显增加，`event18` 是否减少，`feedback_adc` 是否不再允许几十 count 空转。 |

2026-06-18 V4 SVG 数据后修正静止小误差 event13 记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次数据结论 | 读取 `build/servo_curve_live/servo_curve_live.csv` 共 `5033` 行，外部 PWM 基本稳定：`pwm_us=1503..1505`，`target_adc=2052..2056`。但静止段仍有 `event13=100` 行，关键片段在 `pwm=1504`、`target=2055` 不变时，`feedback=2067..2070`、`error=-12..-15`、`velocity_q4=-5..4`，H 桥仍输出 `duty=-57..-93` 并持续约 `3091ms`。结论：不是外部 PWM 突变触发，而是目标不变的小误差被运动减速段 `event13` 接管，造成轻微嗞嗞/抖动。 |
| 本次源码修改 | `src/app_entry.c`：没有采用前一版会超 Flash 的新增大分支；改为在轨迹到达目标后的 HOLD 判定中，用 P70 `Rel` 释放带替代 P70 `Hbd` 运动保持带。目标是：外部 PWM 没变、轨迹已到位、速度很低且误差仍在 `Rel` 内时，直接进入 `HLD/event2`，不再继续进入 `event13` 输出小 duty。 |
| 同步保留 | 保留上一版 H 桥 `LOCK` off 段刹车衰减；保留 `LOCK` 方向翻转刹车条件中“ADC 正朝目标回来才纯刹车”的修正；保留 SWD/SVG 外力触发采集。未改马达 PWM 频率、未改 ADC 采样方式、未改 LOCK 的主要 PD 公式。 |
| 构建结果 | Ubuntu LTO 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19680 B`、`data=668 B`、`bss=1652 B`，Flash 仅余约 `4 B`。非 LTO 调试配置会超 Flash，后续此调试固件必须继续使用 `LTO=1`。 |
| HEX/ELF SHA256 | HEX `43ed88936fee5a63405706642e3ef329ec1ecdc7c653573dd87f4dfe593ca8a3`；ELF `5a1405735e6360af01cd929c379158e0d01e4a9377d4e697fc44cdbc0ea0318`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800494d`，返回 `Successfully resumed device`。 |
| 后续测试重点 | 固定 `1500us`，先不碰看是否仍会过一段时间嗞嗞；再轻搭舵板似动非动，观察 P50 是否主要停在 `HLD e2`，不要长时间出现 `D+/D- e13`。若仍嗞嗞，下一步直接分析新 CSV 中 `event13/event16/event17/event18` 的占比和片段，不再凭感觉改。 |

2026-06-18 H 桥大电流安全回退版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “大电流了，程序把H桥搞成有冲突了。” / “反转一点就大电流” |
| 本次判断 | 这是 H 桥危险问题，优先级高于嗞嗞声和锁止力。上一阶段加入的 `LOCK` PWM off 段刹车衰减，以及原 `HBridge_Brake()` 的主动短接组合，在当前硬件上可能形成桥臂冲突或过大短接电流；必须先撤到安全态。 |
| 本次源码修改 | `src/hbridge.c`：`apply_brake()` 临时退化为 `apply_coast()`，即所有刹车请求都变成四脚关闭滑行；`HBridge_SetPwmOffBrake()` 保留 API 但强制写 `s_pwm_off_brake=0`，上层即使请求 off-brake 也不会生效。`src/app_entry.c` 仍保留小误差 `event13` 修正，没有恢复到危险旧版。 |
| 影响 | 这版锁止力和刹车会变弱，但不应再由软件刹车组合引起大电流。下一步必须重新确认 H 桥四个输入组合的真实安全表后，才能恢复任何主动刹车/同步衰减。 |
| 构建结果 | Ubuntu LTO 构建通过，TSSOP20/P50/LCDM 调试配置 `text=19624 B`、`data=668 B`、`bss=1652 B`。 |
| HEX/ELF SHA256 | HEX `878bb1f5e23f4e82fec72f022d57af76cca8c2f79a10c96fc94a23536cfc48d6`；ELF `679f785dc0afaa9631fc7c49b63b8cce6335f8ba4e4c322530147db2113dc049`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004915`，返回 `Successfully resumed device`。 |
| 测试要求 | 先限流上电，只验证空载和轻微正反动作电流是否恢复正常；不要先做大力扭舵板锁止测试。若仍反转一点就大电流，则问题不是刹车组合，而要立即检查 `apply_forward/apply_reverse` 两个方向组合与实物 H 桥输入定义是否相反或冲突。 |

2026-06-18 疑似 H 桥烧坏后的高阻安全版与 PID 参考记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “好像H桥烧了” / “这样吧，我再给一个网站给你参考，看是否有参考价值，里面介绍的PID你看下是否按这样来写的程序。” |
| 安全判断 | H 桥疑似损坏后，不能再让 MCU 继续输出任何 H 桥驱动组合。上一版虽然把刹车退成滑行，但 `forward/reverse` 仍会输出驱动信号；若 H 桥已经损坏或真值表不明，继续驱动有二次损坏风险。 |
| 本次源码修改 | `Makefile` 新增 `SERVO_HBRIDGE_FORCE_HIZ` 开关；`src/hbridge.c` 在该开关为 1 时，H 桥四个控制脚配置为 `GPIO_MODE_ANALOG`、`GPIO_NOPULL` 高阻输入，`HBridge_SetSignedDuty()` 强制清零，`HBridge_TickISR()` 直接返回，所有 H 桥 API 不再写 IO。 |
| 构建结果 | Ubuntu LTO 构建通过，TSSOP20/P50/LCDM 调试配置加 `SERVO_HBRIDGE_FORCE_HIZ=1`，`text=19436 B`、`data=668 B`、`bss=1652 B`。 |
| HEX/ELF SHA256 | HEX `8ed16ff51301348fecaf499e7e6b9d3a87e0715c0951578a46103b122e520369`；ELF `b627cc2f6e52a005fa8656dcbd234ad2c478c68eb58265751211194cd5bd3df5`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004859`，返回 `Successfully resumed device`。当前板上是高阻安全版，不会驱动马达/H 桥。 |
| PID 参考判断 | 用户给的 PID 框图有参考价值：目标位置 `r(t)` 与反馈 ADC 位置 `y(t)` 做误差 `e(t)`，控制输出 `u(t)=P+I+D`。但舵机不能直接套普通 PID；必须在 PID 外层加输入 PWM 滤波/目标轨迹、H 桥真值表安全层、输出限幅/斜率、抗积分饱和、死区/静止保持、过流/失控保护。当前最紧急问题不是 PID 参数，而是先确认 H 桥真值表和硬件是否损坏。 |
| 下一步 | 断开马达电源，只给逻辑电源/用万用表测 H 桥：VM-GND 是否短路、马达两端是否短路、各 H 桥输出到 VM/GND 是否击穿。若 H 桥更换或确认正常，再写独立 H 桥真值表测试程序，每次只打开一个安全组合并测电流，确认 forward/reverse/brake/coast 后，才能恢复闭环控制和重新设计 PID。 |

2026-06-18 WildWingServoDriver STM32 开源舵机源码评估记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “你可以记录这个，但我想再给你一个GITHUB链接，另外一个人的STM32程序源代码，你再看看这个人写的是否合理，然后你可以折中选优秀的来参考，这样会更好。https://github.com/huangshuang853071569/WildWingServoDriver” |
| 参考来源 | GitHub `https://github.com/huangshuang853071569/WildWingServoDriver`，本地临时克隆 `/tmp/WildWingServoDriver`，当前查看提交 `702a671b424d2b485755a12f2f9b0ccb7f29c7a9`。README 描述为 STM32F103 “位置式串级 PID 算法”：输入 PWM 脉宽映射为目标位置/电压，电位器 ADC 作为当前位置，PID 输出给 H-Bridge。 |
| 源码主线 | `Code/main.c` 主循环为 `data_process()`、`pid_control(except_pos,current_pos,5)`、`Motor_Control()`、`delay_ms(1)`，结构很短，控制周期约 1ms。`data_process.c` 把 `1000..2000us` 映射为 `-16500..16500`，ADC 电压 `0..3.3V` 也映射为 `-16500..16500`。`pid.c` 是外环位置误差 P 控制生成目标速度，内环用当前位置微分得到实际速度，再做 PID/PD 输出。`motor.c` 根据 PID 输出正负决定 H 桥方向和 PWM duty。 |
| 值得参考 | 这份代码最有价值的是架构方向：外环位置环 + 内环速度环 + 单一 signed duty 输出。它比我们前面 V4 里多个状态分支互相抢控制权更干净，也更符合标准伺服控制思路。它还包含输出整形：小输出归零防振荡，中等输出跳到固定最小驱动力防啸叫，积分抗饱和，位置死区。 |
| 不能照搬 | 该项目是 STM32F103、浮点运算、资源较宽；我们的 PY32F002 SOP8/Flash/RAM 更紧，不能直接用 float 大结构。它的 H 桥只用两个方向脚加一个 PWM，且没有我们这次遇到的四输入真值表安全验证、桥臂互锁、损坏保护、过流保护。PWM 输入捕获滤波也较简单，ADC 平均滤波代码也比较粗，不能直接作为量产安全代码。 |
| 与 DroneBot PID 参考对比 | DroneBot 视频/文章是普通单环 PID：目标位置和 ADC 位置直接算 PID 输出，适合教学和低要求验证。WildWing 是串级思路：位置外环给速度目标，速度内环控制 H 桥输出，更接近高性能舵机需要。我们后续应以 WildWing 的串级结构为主，结合 DroneBot 的清晰 PID 数据流，不沿用当前复杂状态机。 |
| 对我们项目的结论 | 后续正常控制架构应重写成一条主线：`PWM 捕获/滤波 -> 目标位置/目标轨迹 -> ADC 快速滤波 -> 当前位置和速度 -> 位置外环生成目标速度 -> 速度内环生成 signed duty -> 输出整形/死区/最低启动力/限幅/斜率 -> H 桥安全层`。静止锁止也不应另写一套互相冲突的逻辑，而应是同一串级环在目标不变时的高刚性模式，参数可调。 |
| 当前安全状态 | 现在实物板疑似 H 桥损坏，板上已下载的是 `SERVO_HBRIDGE_FORCE_HIZ=1` 高阻安全固件，H 桥引脚不输出。未确认 H 桥芯片和真值表之前，不恢复任何正常马达驱动，不下载普通舵机控制固件。 |
| 下一步建议 | 先做独立 H 桥真值表/电流安全验证；硬件确认后，基于当前代码新建简化串级控制分支，把参数全部做成可调：位置 P、速度 P/D/I、死区、最小启动 duty、最大 duty、静止刚性、输出斜率、目标轨迹速度/加速度、ADC 滤波强度、PWM 输入滤波强度。SVG/SWD 曲线继续保留，重点看 `target_adc/current_adc/velocity/error/duty/state` 五类数据。 |

2026-06-18 H 桥低 duty 正反步进测试版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “现在恢复H桥，可以单步试动作是否会大电流，PWM逐级试加，然后反过来试，请测试” |
| 本次判断 | 不恢复普通舵机闭环控制，也不使用之前高 duty ramp。当前目标只验证 H 桥正反两个驱动组合是否会大电流，因此必须使用短脉冲、低占空比、长间隔，并且保持刹车/同步衰减关闭。程序没有电流采样，不能自动判断大电流，测试时必须用限流电源人工观察电流。 |
| 本次源码修改 | `src/app_entry.c` 的 `SERVO_ENABLE_HBRIDGE_IO_TEST` 测试路径从原来 `0..900` duty 连续 ramp 改为安全阶梯短脉冲：`SERVO_HBRIDGE_IO_TEST_MAX_DUTY=100`、`STEP_DUTY=20`、`PULSE_MS=120`、`REST_MS=900`、`CYCLE_REST_MS=2000`。流程为 1kHz 马达 PWM：滑行 2 秒，正向 duty `20/40/60/80/100` 各输出 120ms 且每步滑行 900ms，再滑行 2 秒，反向同样测试，然后循环。 |
| 安全限制 | `HBridge_SetPwmOffBrake(0)`，所有 off 段滑行；`apply_brake()` 当前仍退化为滑行；本版没有 LCDM、没有 ADC/PWM 闭环、没有普通舵机动作逻辑。若任一步电流异常，立即断电，下一步只查 H 桥真值表/硬件，不继续加大 duty。 |
| 构建命令 | `./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_HBRIDGE_IO_TEST=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_ENABLE_PA0_POT_POWER=1 SERVO_HBRIDGE_FORCE_HIZ=0` |
| 构建结果 | Ubuntu LTO 构建通过，`text=4148 B`、`data=20 B`、`bss=1244 B`。 |
| HEX/ELF SHA256 | HEX `26b0aa65a9c8c27f63fcb48803c19f6942f4c79ad60376a45c44c3dc4235dec0`；ELF `4a3cbd9c155151ed2a8da16dfa48453d4d3c973c182542187b1822af61f3cafc`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08000f8d`，返回 `Successfully resumed device`。当前板上是 H 桥低 duty 正反步进测试版。 |
| 用户测试重点 | 使用限流电源。看正向第一步 `duty=20` 是否已经异常大电流；若正常，再看 `40/60/80/100`。正向一轮后等反向一轮，观察反向第一步是否异常。记录是“正向异常、反向异常、两个方向都异常、还是都正常”，这直接决定下一步是改方向组合、查 H 桥损坏，还是恢复更高一级 duty 测试。 |

2026-06-18 H 桥中低 duty 正反步进测试版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “动作有点小，看不到” |
| 本次判断 | 上一版 `2%..10%` 动作太小，不方便观察，但用户未反馈大电流异常，因此按同一安全测试框架提高一级。仍不恢复闭环舵机，不启用刹车/同步衰减。 |
| 本次源码修改 | `src/app_entry.c`：`SERVO_HBRIDGE_IO_TEST_MAX_DUTY` 从 `100` 提高到 `300`，`STEP_DUTY` 从 `20` 改为 `50`，`PULSE_MS` 从 `120ms` 改为 `180ms`。测试流程变为 1kHz：正向 duty `50/100/150/200/250/300`，每步 180ms 后滑行 900ms；再滑行 2 秒，反向同样测试并循环。 |
| 安全限制 | off 段仍滑行，`HBridge_SetPwmOffBrake(0)`；`apply_brake()` 仍退化为滑行；无 LCDM、无 ADC/PWM 闭环、无普通舵机控制。若 5% 或 10% 就出现异常电流，立即断电，不继续观察后续步进。 |
| 构建命令 | `./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_HBRIDGE_IO_TEST=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_ENABLE_PA0_POT_POWER=1 SERVO_HBRIDGE_FORCE_HIZ=0` |
| 构建结果 | Ubuntu LTO 构建通过，`text=4152 B`、`data=20 B`、`bss=1244 B`。 |
| HEX/ELF SHA256 | HEX `f514101adce774554bf0c785f56764779f7f6c49613012c13e12d61a02138dda`；ELF `74d4a03bb8dffd218cf04e900109432c1fed7466259a25c4839fb8243a3fea45`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08000f91`，返回 `Successfully resumed device`。当前板上是 H 桥中低 duty 正反步进测试版。 |
| 用户测试重点 | 观察是否能清楚看到正向和反向动作；同时看每一级电流，特别是 `50/100/150` 低三级是否已经异常。如果正反都正常但动作仍小，下一版再加到 `50%`；如果某一方向明显大电流或不动，先停，改查 H 桥真值表或硬件损坏。 |

2026-06-18 V5 串级控制架构第一版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “正常了，请接着重新打架构，直到完成我们的目标任务为止，加油” |
| 前置结论 | H 桥低 duty 正反步进测试正常，说明基本正反驱动组合没有立即短路大电流。可以从高阻/IO 测试版恢复到普通 LCDM 调试固件，但仍保持刹车/同步衰减关闭，避免再次触发大电流风险。 |
| 本次架构判断 | 旧 V4/V3 在 `lcdm_servo_loop_update_fast()` 内同时存在运动、刹车、回位、锁止、反打等多个状态分支，调参时互相牵制，容易越改越乱。V5 第一版改为一条主线：PWM 目标 -> ADC 位置/速度观测 -> 目标轨迹限速/限加速 -> 位置外环生成速度目标 -> 速度内环生成 signed duty -> 输出限幅/斜率 -> H 桥。静止锁止也走同一串级环，不再另写一套互相冲突的反推逻辑。 |
| 本次源码修改 | `src/app_entry.c`：重写 LCDM 实际运行路径 `lcdm_servo_loop_update_fast()` 的核心控制段。保留 PWM 新鲜度、PWM 范围保护、ADC LOST、目标映射、P50/SWD 曲线发布、方向报警和现有 LCDM 参数页；删除旧的运动中分支刹车、RETURN 回位、小误差 event13 反复输出等复杂路径。新增静止 `static_lock_mode`：目标不变且轨迹到位时，如果误差超过 P70 `Rel`，直接进入同一串级速度环反顶，输出受 P70 `Lmin/Lmax` 和 P40 `Max` 限制。 |
| 当前可调关系 | P30 `Vmax/Acc/FF/Kv/Slew` 分别对应轨迹最高速度、加速度、运动前馈、速度环增益、输出斜率；P31 `Bg` 暂作速度阻尼/刹车系数；P40 `Inv/Hz/Max` 仍控制方向、马达 PWM 频率和总 duty 上限；P70 `Hbd/Hvel/Rel/Lmin/Lmax` 控制到位静区、速度静区、静止释放带、静止最低反顶 duty 和静止最高 duty。 |
| 安全限制 | `HBridge_SetPwmOffBrake(0)`，H 桥 PWM off 段仍滑行；`apply_brake()` 当前仍退化为滑行。V5 第一版不启用主动刹车/同步衰减，先验证控制主线是否比旧架构稳定。 |
| 构建命令 | `./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_TJC_SOFT_TX=1 SERVO_HBRIDGE_FORCE_HIZ=0` |
| 构建结果 | Ubuntu LTO 构建通过，`text=18524 B`、`data=668 B`、`bss=1644 B`，比之前复杂状态机版本 Flash 更小。 |
| HEX/ELF SHA256 | HEX `f92e0b128534eb20ae57ee55fedafcc48ed2ff7a1d54c09961ce74e882ccf8d9`；ELF `2b29005a1abd8d7a3092cd2be9eea594befb88c04a744ab57569403d82388fbf`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080044c9`，返回 `Successfully resumed device`。当前板上是 V5 串级控制架构第一版，不是 H 桥测试版。 |
| 用户测试重点 | 先限流固定 `1500us` 上电，确认不自动乱跑、不大电流；再做 `1000/1500/2000us` 切换，看是否还会到点来回摆；最后固定 `1500us` 轻扭舵板，看是否进入反顶但不持续嗞嗞。若速度慢，优先调 P30 `Vmax/Acc/FF/Kv`；若锁止软，优先调 P70 `Rel/Lmin/Lmax`；若到点过冲，优先调 P30 `Acc/Kv` 和 P31 `Bg`。 |

2026-06-18 V5 断电后重下同版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “拨电了再下载一次” |
| 本次操作 | 用户断电/重新上电后，未改代码、未重编译，直接重新下载上一条记录的 V5 串级控制架构第一版，避免引入新变量。 |
| HEX/ELF SHA256 | HEX `f92e0b128534eb20ae57ee55fedafcc48ed2ff7a1d54c09961ce74e882ccf8d9`；ELF `2b29005a1abd8d7a3092cd2be9eea594befb88c04a744ab57569403d82388fbf`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080044c9`，返回 `Successfully resumed device`。当前板上仍是同一个 V5 第一版。 |

2026-06-18 V5 LCDM PA7 软发送修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “这版没LCDM显示么” |
| 本次原因 | 上一版确实包含 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1`，但 runtime 模式下 `pa7_tjc_write_cmd()` 强制走硬件 USART1 发送，没有使用编译参数 `SERVO_TJC_SOFT_TX=1`。当前 LCDM 接线使用 PA7 软发送时，屏幕收不到命令，所以表现为无显示。同时默认 USART1 MSP 会把 PA3 配成 UART RX，PA3 又是 ADC 电位器脚，存在 ADC 被抢脚的风险。 |
| 本次源码修改 | `src/app_entry.c`：新增/使用 `SERVO_TJC_SOFT_TX` 判断，runtime 模式下也可调用 `pa7_tjc_write_byte()` 通过 PA7 bit-bang 发送 TJC 命令；新增 `pa7_tjc_soft_tx_init()`，在 LCDM runtime 启动前把 PA7 配成上拉推挽输出。`src/app_hal_msp.c`：当 `SERVO_TJC_SOFT_TX=1` 时，USART1 只配置 PB2 作为 RX，不再配置 PA2/PA3，避免 PA3 ADC 被 UART 占用。 |
| 未改事项 | V5 串级控制算法未改；H 桥 off 段仍滑行；LCDM 波特率仍为 `230400`；P50/P30/P40/P70 页面逻辑未改。 |
| 构建命令 | `./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_TJC_SOFT_TX=1 SERVO_HBRIDGE_FORCE_HIZ=0` |
| 构建结果 | Ubuntu LTO 构建通过，`text=18176 B`、`data=672 B`、`bss=1648 B`。 |
| HEX/ELF SHA256 | HEX `425b927b9f05b60e71c416fb8e4bba61ffc8cbf1ae00359eda58914908726e3e`；ELF `241ece1ba0fc6bb71319dd381b2f8d7120c881c5d752a89c75ca18539e02696d`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004371`，返回 `Successfully resumed device`。当前板上是 V5 LCDM PA7 软发送修正版。 |
| 用户测试重点 | 先看 LCDM 是否恢复显示 P50/P01 等页面；若有显示，再测试 `1500us` 静止和 `1000/1500/2000us` 切换。若仍无显示，下一步只做 PA7 方波/串口命令验证，不再改控制算法。 |

2026-06-18 V5 LCDM 硬件 USART PA7/PB2 修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “没显示呢” / “动到哪里了” / “我的天哪，终于有的，你究竟改了什么” |
| 本次原因 | PA7 软串口在 `230400` 下仍未让 LCDM 显示，判断为软发送时序/中断/控制循环干扰风险较高。改为使用硬件 USART1 的复用引脚，PA7 作为硬件 TX，PB2 作为硬件 RX；同时保持 PA3 不被 USART 占用。 |
| 本次操作 | 使用编译参数 `SERVO_TJC_UART_ALT_PINS=1`，不再使用 `SERVO_TJC_SOFT_TX=1`。控制算法未改，仅切换 LCDM 通讯物理层。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=18544 B`、`data=668 B`、`bss=1644 B`。 |
| HEX/ELF SHA256 | HEX `65b05e5a399b4b26fa9e8194c7b17bdfd2613df4983fd8ee8e48c997a8aa96cf`；ELF `0c48159a0981685225483069f8bf189cefaa393b6edba66e329356df81326fb4`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080044dd`，返回 `Successfully resumed device`。用户确认 LCDM 终于有显示。 |

2026-06-18 V5 每个点自抖止抖保守版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不行，每个点都在来回自己抖” |
| 本次判断 | V5 第一版静止锁止太激进：目标不变且误差刚超过 P70 `Rel` 时，立即给静止最低 duty `Lmin`，同时静止速度目标使用 `error/2`，还会叠加运动前馈 `FF`。这会导致到点后自己推过目标，再反向，形成每个点来回抖。 |
| 本次源码修改 | `src/app_entry.c`：P70 `Rel` 默认从 `14` 提高到 `24`；新增 `static_quiet_band=Rel+Hbd`，目标不变且轨迹到位时，小误差/低速直接进入 `HLD`；静止锁止速度目标从 `track_error/2` 改为 `track_error/16`；静止锁止时关闭 `FF`；只有误差达到 `Rel+2*Hbd` 级别时才启用 `Lmin` 最低强推。 |
| 影响 | 这版优先压住自抖和嗞嗞，静止小误差会更安静，但初始锁止力会比激进版弱。若不再自抖，下一步再逐步调小 `Rel` 或提高 `Lmin/Lmax` 恢复锁止刚性。 |
| 构建命令 | `./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0` |
| 构建结果 | Ubuntu LTO 构建通过，`text=18608 B`、`data=668 B`、`bss=1644 B`。 |
| HEX/ELF SHA256 | HEX `c6846f389dda58e42788907f44ecc96e11afb8eef24caa16717d18e4ad6c78e5`；ELF `e5b200e644ded9c1a27438fc8cf5f9bf7213e0d3dc044f6cc91cc3c87b125a39`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800451d`，返回 `Successfully resumed device`。当前板上是 V5 止抖保守版。 |
| 用户测试重点 | 固定 `1500us` 先看是否还会自己来回抖；再测试 `1000/1500/2000us` 三点是否每点都能停住。如果自抖消失但锁力变软，下一步只调静止锁止参数；如果仍自抖，要马上抓 SWD/SVG 数据看 `error/velocity/duty/event`。 |

2026-06-18 V5 静止小 duty 静音版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有嗞嗞声” / “暂时不抖” |
| 本次判断 | 止抖保守版已经把自抖压住，说明大方向正确。剩余嗞嗞声来自静止锁止中间带仍输出小 duty：误差超过安静带但未达到强锁带时，速度环算出低于 `Lmin` 的小 PWM，马达进入临界嗞嗞区。 |
| 本次源码修改 | `src/app_entry.c`：在 `static_lock_mode` 中，如果误差未达到 `static_strong_band` 且计算 duty 低于 P70 `Lmin`，直接令 `duty_calc=0`，不再输出小 PWM 试探；只有误差达到强锁门槛才补到 `Lmin`。 |
| 影响 | 目标是减少到位后的嗞嗞声并保持不抖。代价是强锁介入前的自由区可能略大，若用户反馈“又能扭动”，下一步不能简单恢复小 PWM，而要缩小 `static_quiet_band/static_strong_band` 或提高静止速度环比例。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=18632 B`、`data=668 B`、`bss=1644 B`。 |
| HEX/ELF SHA256 | HEX `85ef86a78a586e660032410f75ac6564fea24de219cf32e5976a858debb2e23c`；ELF `f7f52574fcc06af9b1b4b061c31179d226f5e4d1251cbfda05d37b0279812976`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004535`，返回 `Successfully resumed device`。当前板上是 V5 静止小 duty 静音版。 |
| 用户测试重点 | 固定 `1500us`，听嗞嗞声是否减少；再轻扭舵板，看是否从安静变成明显能扭动。如果嗞嗞减少但锁止软，下一版只收窄静止门槛，不恢复低 duty 嗞嗞区。 |

2026-06-18 V5 正反切换死区/滑行保护版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不有嗞嗞声，但我感觉正反马达PWM没有死区一样，因为我看电流表只要一反向转动非常大电流” / “证明H桥一直正反正反作用” |
| 本次判断 | 嗞嗞声已被上一版压住，但反向转动时电流瞬间变大，说明控制输出在零点附近可能正反翻转过快；旧逻辑虽然用 `apply_slew()` 把 duty 拉到 0，但下一次 1ms 控制循环已经允许反向输出，等效上缺少 H 桥正反切换死区。 |
| 本次源码修改 | `src/app_entry.c`：新增 `LCDM_SERVO_REVERSE_DEAD_MS=35ms` 和 `LCDM_SERVO_REVERSE_MIN_DUTY=180`；新增当前输出方向、反向死区到期时间和死区状态。最终 H 桥输出前，如果请求方向与上一输出方向相反，先强制 `HBridge_Coast()`，大于反向门槛时进入 35ms 反向等待；等待期间只滑行不输出正反 PWM。所有 HOLD/STOP/无 PWM/ADC LOST/越界释放出口同步清除输出方向保护状态。 |
| 未改事项 | LCDM 通讯仍使用硬件 USART1 复用引脚 `PA7 TX / PB2 RX`；PA3 仍保留 ADC；H 桥 off 段仍滑行，未启用主动刹车/同步衰减；V5 串级控制参数未重调。 |
| 构建命令 | `./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0` |
| 构建结果 | Ubuntu LTO 构建通过，`text=18932 B`、`data=668 B`、`bss=1660 B`。 |
| HEX/ELF SHA256 | HEX `1076ff01840d74b5c79a86d47cc7096c057e53c353036eab5ce37e4d6d34c6cb`；ELF `9693e593fc76cb825b98ead0dc46e5e5d7d29284097eed04226cd2be9ec48476`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004661`，返回 `Successfully resumed device`。当前板上是 V5 正反切换死区/滑行保护版。 |
| 用户测试重点 | 先固定 `1500us` 静止轻扭，观察电流是否还会一反向就冲大；再做 `1000/1500/2000us` 切换，看是否因 35ms 死区导致速度明显变慢。如果电流冲击下降但动作变钝，下一步把 35ms 下调；如果仍大电流，必须继续抓 SVG/SWD 数据确认 duty 符号是否仍在抖动，或 H 桥真值表是否存在硬件方向冲突。 |

2026-06-18 V5 运动中禁止主动反打刹车版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “有改善，但从一个点到另一个点时，还是会有大电流的情况，左右扭不会大电流了。同时转动的太慢，而且到点后还会抖。” |
| 本次判断 | 上一版正反切换死区对静止外力左右扭有效，说明 H 桥方向保护方向正确。但点到点切换仍大电流，说明正常运动过程中速度环还会在未过目标点前算出反向 duty 做主动刹车；在没有真实电流环和已验证刹车真值表前，这种反打刹车会产生瞬间大电流和到点回摆。 |
| 本次源码修改 | `src/app_entry.c`：反向死区从 `35ms` 降到 `20ms`，减少正常动作变慢；在非静止锁止模式下，如果速度环计算出的 `duty_calc` 方向与当前位置到目标点的方向相反，则把 `duty_calc` 置 `0` 并标记为减速事件，让马达滑行/降 duty，不再主动反向刹车。静止锁止模式仍允许按误差方向反顶。 |
| 未改事项 | LCDM 通讯、PWM 输入、ADC 观测、V5 串级主结构未改；H 桥 off 段仍滑行；未启用主动刹车。 |
| 构建命令 | `./tools/build_in_ubuntu.sh LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0` |
| 构建结果 | Ubuntu LTO 构建通过，`text=18992 B`、`data=668 B`、`bss=1660 B`。 |
| HEX/ELF SHA256 | HEX `4d315379a1aba1a30d8ea766c5c6bd7c996fdaa692684459ce04f0bf08f76138`；ELF `236e2ddd60d88c8d1d39cbec5d1db5af69ed578c55c1150598a19410cac253f9`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800469d`，返回 `Successfully resumed device`。当前板上是 V5 运动中禁止主动反打刹车版。 |
| 用户测试重点 | 先看 `1000/1500/2000us` 点到点切换时电流是否仍有瞬间大冲击；再看速度是否比 35ms 版本恢复一些；最后看到点回摆是否减少。如果大电流消失但过冲变大，下一步要用分段减速/轨迹限速解决，而不是恢复反向刹车。 |

2026-06-18 回退到 V5 正反切换死区/滑行保护基线记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还有一点闪电流，应该还是在正反马达PWM切换时或刹车时有冲突。另外，锁止力完全没有了。” / “越改越差” |
| 本次判断 | “运动中禁止主动反打刹车版”虽然试图降低点到点大电流，但把速度环反向输出一刀切为 0，导致静止/到点后的锁止力被明显削弱，方向错误。该版本作废，不能继续叠加调整。 |
| 本次源码修改 | 回退上一版改动：`LCDM_SERVO_REVERSE_DEAD_MS` 从 `20ms` 恢复为 `35ms`；删除非静止模式下 `duty_calc` 与目标方向相反就置 0 的限制。保留上一版正反切换滑行保护逻辑。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=18932 B`、`data=668 B`、`bss=1660 B`，与 V5 正反切换死区/滑行保护版一致。 |
| HEX/ELF SHA256 | HEX `1076ff01840d74b5c79a86d47cc7096c057e53c353036eab5ce37e4d6d34c6cb`；ELF `9693e593fc76cb825b98ead0dc46e5e5d7d29284097eed04226cd2be9ec48476`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004661`，返回 `Successfully resumed device`。当前板上已恢复到 V5 正反切换死区/滑行保护基线。 |
| 后续方向 | 下一步不要再简单禁止反向 duty。应先用 SVG/SWD 抓点到点切换时的 `target/adc/velocity/duty/event`，确认“闪电流”发生在正反切换、到点刹车、还是 H 桥 PWM off/on 边沿；再做分段限幅或限反打斜率，而不是直接取消锁止输出。 |

2026-06-18 VSG/SVG 静态正反强打定位与小目标分级修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “我看你还是把VSG拉出来检测吧，你自己看，哪里出问题了” |
| VSG 启动情况 | 先用旧 `start_servo_curve_live_macos.sh` 发现 Ubuntu 侧报 `No connected debug probes`，原因是当前 DAPLink 在 macOS 侧。改用 macOS 本机 `/tmp/pyocd-mac-venv/bin/python tools/live_servo_curve_swd.py --pack /tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack` 直接采样，实时生成 `build/servo_curve_live/servo_curve_live.html` 和 `hbridge_response.html`。 |
| 数据结论一 | 在固定 `1503us` 静止时，PWM 目标基本稳定：`pwm_us=1503..1505`、`target_adc=2052..2056`，但旧版本反馈 ADC 在 `1946..2146` 大幅摆动，1000 行中 duty 非零 589 行，最大 `774`，duty 正负翻转 `545` 次，事件主要是 `event=12`。这证明闪电流/抖动直接来自静止附近被误判成正常 DRIVE 后的正反强打，不是 PWM 大幅跳变。 |
| 源码原因 | 旧逻辑只要目标 ADC 小跳超过 `LCDM_SERVO_CMD_HOLD_EXIT_EXTRA=3 count` 就认为是新动作，重置轨迹并进入正常 `event=12 DRIVE`。1503/1505us 的小抖动约 4 count，就足以触发大动作分支。第一次修正后又发现小目标更新时 `cmd_target_adc` 已变，但 `traj_adc` 仍停在旧值，导致误差接近 0 时仍按旧轨迹输出小负 duty。 |
| 本次源码修改 | `src/app_entry.c`：新增 `LCDM_SERVO_CMD_MOTION_EXIT_EXTRA=12`，把目标变化分成两级。`target_changed >3 count` 只更新 `cmd_target_adc`，保留 1-3us 灵敏度；`target_motion_changed >12 count` 才重置轨迹、清 overshoot、进入快速运动。小目标变化但未达到大动作门槛时，同时把 `s_lcdm_servo_traj_pos_q4` 同步到新目标并清 `traj_vel`，避免旧轨迹残留造成静态小 duty。 |
| 构建结果 | Ubuntu LTO 构建通过，最终版本 `text=18956 B`、`data=668 B`、`bss=1660 B`。 |
| HEX/ELF SHA256 | 中间版 HEX `d5ee726d2cba6c513990b5f928c72bec5d60650427bfa502a8396b14b0c61bb0` 仍有旧轨迹残留小 duty；最终版 HEX `5cf1f37bc2d29b5423d737bfea5fc61371f247c5bfc12143636700366b56a30a`，ELF `d487f983c201128eca61a4d8989020a4ed0efe344e3248bbb4d22ec626774b34`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，最终版启动向量 `SP=0x20000c00`、`PC=0x08004679`，返回 `Successfully resumed device`。 |
| VSG 静态复测 | 下载最终版后重新采样，固定 `1503us` 共 73 行：`target_adc=2052` 固定，`feedback_adc=2040..2056`，`error=-4..12`，`duty非0=0`，事件 `event=2` 70 行、`event=16` 2 行、`event=13` 1 行。最后 12 行全部 `event=2`、`duty=0`、`traj_adc=2052`。静止正反强打已消失。 |
| 下一步 | 保持当前后台 VSG 采样。用户做 `1000 -> 1500 -> 2000 -> 1500 -> 1000` 后，再读取 CSV/快照分析点到点大电流、到点抖动是否还存在；若还存在，下一步只针对运动尾段 duty 斜率/限幅处理，不再动静态小目标分级逻辑。 |

2026-06-18 V5 板上 ring-buffer 曲线采集版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “完成” |
| 本次判断 | 之前 macOS 实时 SWD 读取会出现同一帧重复、`unique_time=1` 的冻结现象，不能继续作为运动曲线依据。改为固件在 RAM 内部记录关键帧，测试完成后再通过 SWD 暂停读取，可避免实时读取干扰和边写边读乱序。 |
| 本次源码修改 | `src/app_entry.c`：新增 `ServoCurveLogEntry g_servo_curve_log[32]`、`g_servo_curve_log_head/count`，在 `lcdm_servo_curve_publish()` 中记录 `time_ms/pwm_us/target_adc/feedback_adc/error_adc/duty/state/event`。日志触发条件为每 20ms、目标变化、事件变化、duty 方向变化或 duty 幅度变化超过约 160，避免同一方向小幅 duty 抖动刷满缓冲。 |
| 本次工具修改 | `tools/dump_servo_curve_log.py`：新增/更新 SWD 回读脚本，读取前先 `halt()` MCU，读取完成后再 `resume()`，并按 ring-buffer 顺序导出 `build/servo_curve_live/servo_curve_ring.csv`。同步容量为 32 帧。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=19044 B`、`data=672 B`、`bss=2184 B`。`.bss` 中包含 1KB 栈预留，当前 RAM 仍可链接运行，但后续若再加功能要注意 RAM 余量。 |
| HEX/ELF SHA256 | HEX `7fae825db48eadcdd0981d7f12869e14d74003e238ae61ff13c9e7e815e0283b`；ELF `94176c4d56ff95b8275dc2ec9277e4b37d4af966d1ba0616b274c52279dd030f`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080046d1`，返回 `Successfully resumed device`。当前板上是 V5 板上 ring-buffer 曲线采集版。 |
| 回读验证 | 执行 `/tmp/pyocd-mac-venv/bin/python tools/dump_servo_curve_log.py --elf build/py32f002_servo.elf --target py32f002ax5 --pack /tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack --frequency 100000 --output build/servo_curve_live/servo_curve_ring.csv` 成功，输出 `head=18 count=32 rows=32`。时间轴已按顺序输出，不再出现实时 SWD 同一帧冻结问题。 |
| 初步数据观察 | 固定约 `1004us` 时，目标 `target_adc=1186`，反馈约 `1144..1168`，多数为 `duty=0/event=2`；但有一次 `error=18`、`duty=-80`、`event=13` 的小脉冲。后续若用户反馈轻触嗞嗞或小抖，应优先查这类小误差减速/尾段脉冲，而不是盲目提高锁止力。 |
| 下一步 | 请只做一个受控动作后回报“已试完”：例如 `1000us -> 2000us` 一次，停 1 秒；我再直接倒出 `servo_curve_ring.csv` 分析 `target/feedback/duty/event`，再决定尾段刹车或锁止参数如何改。 |

2026-06-18 V5 ring-buffer 采集修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 回读数据 | 第一次回读 `servo_curve_ring.csv` 得到 32 行，但没有保留到 `1000 -> 2000` 运动段；日志被最后 `1004us` 静止状态覆盖。关键数据：`target_adc=1186` 固定，`feedback_adc=1188..1190`，`error=-4..-2`，仍出现 `duty=-9..24` 与 `event=12/3` 反复切换。 |
| 本次判断 | 两个问题同时存在：1. ring-buffer 连续记录静止数据，用户停 1 秒后运动段被覆盖；2. 误差只有 2-4 个 ADC count 时仍进入 `DRIVE event=12`，会形成轻微嗞嗞/小电流脉冲。 |
| 本次源码修改 | `src/app_entry.c`：新增 `LCDM_SERVO_MICRO_HOLD_BAND=6`，目标没有大变化且误差不超过 6 count 时直接进入 `HOLD event=2/duty=0`；新增 `SERVO_CURVE_LOG_CAPTURE_MS=900`、采集窗口状态和 `lcdm_servo_curve_log_trigger()`，只有外部 PWM 目标出现大变化时清空并开启采集窗口，窗口结束后冻结，不再被后续静止覆盖；`s_lcdm_servo_last_target_adc` 初始化为 `0xFFFF`，避免上电首次目标初始化误触发采集。 |
| 工具状态 | `tools/dump_servo_curve_log.py` 保持暂停式读取：读前 `halt()`，读后 `resume()`。 |
| 验证结果 | 中间版证明小误差已经压住：静止时 `error=-2..-4` 全部为 `duty=0/state=HOLD/event=2`。最终版下载后静止等待 2 秒再回读，输出 `head=0 count=0 rows=0`，确认日志不会被上电初始化或静止噪声填满。 |
| 构建结果 | 最终 Ubuntu LTO 构建通过，`text=19180 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `a00041e1512536b667796e748e2dacd346049ea7c99c0677ec6fa0d443dbfba9`；ELF `1d001bf72bc34846168a11514c4dfb52535625bcbc79a86b10a6029faffc5f83`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004759`，返回 `Successfully resumed device`。当前板上是 V5 ring-buffer 采集修正版。 |
| 下一步 | 用户再做一次受控动作：`1000us -> 2000us`，停约 1 秒后回报“已试完”。下一次回读应能保留动作窗口，不会再只看到静止段。 |

2026-06-19 V5 运动尾段不误入静态锁止修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | 成功抓到 `1000 -> 2000` 后的高端动作窗口：`pwm_us=2003..2005`、`target_adc=2921..2925`。反馈从 `2410` 上升到 `2954`，误差从 `+515` 变成 `-33`，duty 范围 `0..224`，无正反翻转，事件为 `2/3/16`。 |
| 数据结论 | 目标 PWM 已正确进入，不是 PWM 输入问题。主要问题是运动尾段被当成静止 LOCK/HOLD：大误差阶段一直是 `event=16 LOCK` 且 duty 多数只有 `224`，速度偏慢；到 `error=38` 时已经进入 `HOLD/event=2`、`duty=0`，随后靠惯性过目标到 `error=-33`。这解释了“软、慢、到点偏差/过冲”的现象。 |
| 本次源码修改 | `src/app_entry.c`：新增 `static_phase` 判定。`static_lock_mode` 和 `static_quiet_band` 只允许在当前相位已经是 `HOLD/LOCK` 时生效；正常 PWM 大动作后的尾段即使轨迹到目标，也继续走运动闭环，不再误入静态锁止/静态安静区。 |
| 采集修改 | `SERVO_CURVE_LOG_CAPTURE_MS` 从 `900ms` 改为 `620ms`，32 帧约覆盖 640ms，避免用户停 1 秒后只保留尾部静止段。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=19180 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `4aed7945bd9ab6149152043c859c5e16bf23c7e0007ab6097a7b2bc052b802b9`；ELF `035a94054ac241caaa92be0a932790a234fcd9329d9a8fcbea632ef7ea220182`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004759`，返回 `Successfully resumed device`。下载后静止 1 秒回读 `head=0 count=0 rows=0`，日志不会被静止填充。 |
| 下一步 | 再做一次 `1000us -> 2000us`，停约 1 秒后回读。重点验证：动作段是否从 `event=16 LOCK` 改为运动闭环事件、duty 是否高于 224、到 `error=30..50` 时是否还会提前 HOLD。 |

2026-06-19 V5 速度相关提前滑行刹车修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | 新版已进入运动尾段 `event=13`，不再全程固定 `event=16 LOCK`。数据：`pwm_us=2002`、`target_adc=2920`；反馈从 `2246` 上升到 `3034`；误差从 `+674` 变到 `-114`，duty 范围 `-408..318`，出现 1 次正负翻转。 |
| 数据结论 | 上一版“运动尾段不误入静态锁止”有效，但刹车仍太晚。`error=101` 时仍输出同向 `duty=318`，`error=35` 时仍输出 `129`；随后 `error=5` 被 `MICRO_HOLD_BAND` 直接 HOLD，但此时反馈仍高速前进，最终过冲到 `-114`，再反向 `duty=-408` 拉回。根因是小误差 HOLD 没看速度，且尾段没有按 ADC 速度提前撤同向驱动。 |
| 本次源码修改 | `src/app_entry.c`：1. `MICRO_HOLD_BAND=6` 增加低速条件，只有 `abs_velocity_q4 <= Hvel` 才可 HOLD，避免高速穿过目标时误停。2. 新增读取 P31 `Bmin/Bmax/Bv`，在非静态、轨迹到目标的运动尾段，如果 ADC 仍朝目标高速前进，且误差进入 `Bmin + velocity*Bg/4` 到 `Bmax` 限幅后的接近区，则将同向 `duty_calc` 清零，标记 `event=14 BRAKE`，让马达提前滑行，不继续推过头。3. duty 为 0 时若来自提前刹车，日志保留 `event=14` 而不是统一写 `event=3`。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=19384 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `9643eb441658c6a8aa5c43230959d0cae0ba6f2314af72c3621de493ef44bcaf`；ELF `80c240c70bbe1bc6aabb77783571a0efb5e90c2c1571de2a4fba1fd700fc8d99`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004825`，返回 `Successfully resumed device`。下载后静止 1 秒回读 `head=0 count=0 rows=0`。 |
| 下一步 | 再做一次 `1000us -> 2000us`。重点看是否出现 `event=14` 提前滑行，最大过冲是否从 `-114` 明显下降，是否避免 `duty=-408` 反向大拉回。 |

2026-06-19 V5 提前滑行条件放宽修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | 上一版没有出现预期 `event=14`，事件只有 `3/12`。数据：`target_adc=2920`，反馈 `2684 -> 3036 -> 2799 -> 2955`，误差范围 `+236..-116`，duty 范围 `-720..+720`，正反翻转 2 次。 |
| 数据结论 | 提前滑行没有命中，反而出现明显来回摆。原因是上一版把提前滑行条件绑在 `traj_at_cmd` 上，但实际数据全程是 `event=12 DRIVE`，说明轨迹尚未判定到点时机械位置已经高速接近目标。算法等 `traj_at_cmd` 才刹车太晚。 |
| 本次源码修改 | `src/app_entry.c`：提前滑行刹车条件从 `else if (traj_at_cmd)` 放宽为普通非静态运动段 `else`。现在只要 ADC 反馈正高速朝目标移动、误差进入速度相关接近区、并且计算 duty 仍为同向推动，就清零同向 duty 并标记 `event=14`。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=19380 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `d01fbf370f0e543fba8a599471c8308b6a9bda55221701e72636d5b0dcdca093`；ELF `5a8f5ad8a14ae13fdc8ae84b3591e74ef0a6ccb196115f5b9e08c705f217516b`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004821`，返回 `Successfully resumed device`。下载后静止 1 秒回读 `head=0 count=0 rows=0`。 |
| 下一步 | 再测 `1000us -> 2000us`。这次必须先看是否出现 `event=14`；若出现但仍过冲，调 P31 `Bmin/Bg/Bmax/Bv`；若仍不出现，则说明速度估计方向或单位不符合判断，需要把速度也加入 ring-buffer 记录。 |

2026-06-19 V5 静止强锁边界抖动修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | 本次没有抓到 `1000 -> 2000` 高端动作，而是抓到低端 `1003us` 静止/扰动窗口：`target_adc=1184`，`feedback_adc=1238..1240`，误差固定 `-54..-56`，事件在 `event=3` 和 `event=16` 之间反复切换，duty 在 `0` 和 `-80` 之间跳。 |
| 数据结论 | 这是静止强锁边界抖动，不是高端运动刹车数据。根因是 `static_strong_band` 约等于 56 count，实际误差正好在 `54/56` 上下跳，导致一帧判定强锁、下一帧判定停输出，形成低端嗞嗞/小脉冲。 |
| 本次源码修改 | `src/app_entry.c`：`static_strong_band` 从 `static_quiet_band + Hbd` 改成 `static_quiet_band + Hbd/2`，并保证至少比 quiet band 大 4 count。目标是让 `54..56 count` 这种边界误差进入连续锁止，不再 `0/-80` 来回跳。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=19384 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `4e120e52100a97595deb02a3a30df2b6ed9d376d068e4ab06ff555e8c152e184`；ELF `53be6f507ae4a1146edf298dca7ae0abfcd921c4a8320eaaa90348250467d5b0`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004825`，返回 `Successfully resumed device`。下载后静止 1 秒回读 `head=0 count=0 rows=0`。 |
| 下一步 | 若要继续看高端过冲，请只做单方向 `1000us -> 2000us`，停 1 秒，不要再回 1000us。若要验证低端嗞嗞，则固定 `1000us` 轻触/等待后回读，看 `event=3/16` 是否还会 0/-80 反复跳。 |

2026-06-19 V5 静止 LOCK 采集触发与低端复测记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “今天开始我们继续” / “已试完” |
| 本次判断 | 上一版固定 `1000us` 测试后回读 `rows=0`，不是控制一定无动作，而是 ring-buffer 只由外部 PWM 大动作触发；静止外力锁止没有触发采集，因此无法确认低端轻触时是否还在 `event=3/16` 边界跳。 |
| 本次源码修改 | `src/app_entry.c`：新增 `s_servo_curve_static_lock_logged`。当静止状态首次进入 `static_lock_mode/event=16` 时触发一次 ring-buffer；目标大动作或回到 release band 后复位该触发锁存。这样低端固定 PWM 轻触/等待也能抓到静止锁止过程。 |
| 构建结果 | Ubuntu LTO 构建通过，`text=19460 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `1847ad04349d0616dd8734b3cf5bd8dd72cebd0e6d7f6f490360cdbc16d94362`；ELF `af7de6ff12e79695e881d5bd9f2cadf581606acc6553f419a58fdb087a1f40f4`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004871`，返回 `Successfully resumed device`。 |
| 低端复测数据 | 固定约 `1003us`，`target_adc=1184`，`feedback_adc=1178..1192`，`error=-8..+6`。32 行中 31 行为 `event=2/duty=0`，仅 1 行 `event=13/duty=80`，持续约 2ms。没有再出现上一版 `event=3/16` 与 `0/-80 duty` 的连续边界跳动。 |
| 结论 | 低端静止边界抖动明显改善。若用户实际听感没有嗞嗞，可先通过低端静止项；后续重点回到单方向高端过冲。 |
| 下一步 | 请只做 `1000us -> 2000us`，停 1 秒，不要回 1000us。回读后重点看 `event=14` 是否出现、最大过冲是否下降、是否还有反向大 duty。 |

2026-06-19 V5 高端静止小误差反向敲击修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | 回读到 16 行高端静止/轻扰动窗口，不是完整 `1000 -> 2000` 运动段。关键数据：`pwm_us=2003`、`target_adc=2921`、`feedback_adc=2835..2929`、`error_adc=+86..-8`。异常点为 `error=+15` 时出现 `duty=-80/event=13`，`error=+1` 时出现 `duty=-240/event=13`，说明在目标附近误差尚未真正反向时，速度/刹车项已经给出反向 H 桥 PWM，容易形成高端嗞嗞声和轻微摆动。 |
| 本次判断 | 外部 PWM 输入没有异常，目标点也正确在高端 `2921` 附近。问题集中在静止小误差区：进入 HOLD/LOCK 相位后，若速度估计还有残留，小误差仍可能被速度环反向驱动；这与“反顶住而不反推”的静止锁止要求相冲突。 |
| 本次源码修改 | `src/app_entry.c`：静止相位条件 `static_phase && !target_motion_changed && traj_at_cmd && abs_error <= static_quiet_band` 直接进入 `HOLD/event=2/duty=0`，不再要求 `abs_velocity_q4 <= motion_hold_vel_q4 + 16`。较大误差仍会进入 `static_lock_mode/event=16` 按误差方向锁止；本次不改运动速度曲线和锁止大力参数。 |
| 构建注意 | 曾误用默认 `./tools/build_in_ubuntu.sh` 生成非 LCDM 调试版，体积仅 `text=9884 B`，该产物未下载。随后按现场正确参数重新构建：`LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，`text=19476 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `6d1d5858c5df3eb609a2f270c856986857d45ca79cef28a2e496184e2baf4a97`；ELF `55a6b665ed5a890bee87a0beb40cb23a3f8b48a3d3daf6e3469ee33321bc6584`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004881`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=3 count=3 rows=3`。数据：`error=44 duty=80 event=16` 后立即变为 `error=27 duty=0 event=2`、`error=-3 duty=0 event=2`。没有再出现 `error` 仍为正时输出负 duty 的高端小误差反向敲击。 |
| 下一步 | 用户验证高端静止嗞嗞是否改善；若改善，再做一次完整 `1000us -> 2000us`，停 1 秒，回读运动段继续看过冲和尾段刹车。 |

2026-06-19 V5 运动尾段反推过大修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | 抓到 32 行高端动作尾段，`pwm_us=2002`、`target_adc=2920`、`feedback_adc=2608..2956..2846..2915`，`error_adc=+312..-36..+74..+5`，`duty=-674..+400`，事件 `12/16/3/2`。 |
| 数据结论 | PWM 目标正确，问题是尾段速度环反推过大。典型过程：`error=+29` 已经接近目标但仍未稳定进入 HOLD；随后在 `error=+17/+7/0` 附近出现反向 duty `-21/-181/-341`，过目标后继续到 `-674`，把舵板反拉到另一侧；再由 `event=16` 正向锁回，形成“到点后来回摆一下”。 |
| 本次源码修改 | `src/app_entry.c`：1. 提前计算 `static_quiet_band/static_strong_band`。2. 新增非静态尾段 HOLD：只要外部 PWM 目标没有变化且实际误差进入 `static_quiet_band`，立即进入 `HOLD/event=2/duty=0`，不再等待轨迹和速度完全满足，避免尾段速度环继续输出大反推。3. 修正提前滑行刹车事件被覆盖的问题：当 `drive_event == 14` 时不再被后续普通 `event=12/13` 覆盖，后续数据能看到真实刹车点。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19508 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `e694c0e00058a560fb14f9b679edd738f2a61d1150b619c2050bf08a226c5824`；ELF `e8b88f26fd3348bc9cf5e850a66728384c56f84203cd7faf4a86e9af4d9cba31`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048a1`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=0 count=0 rows=0`，没有被上电静止状态误触发。 |
| 下一步 | 用户重复做 `1000us -> 2000us`，停 1 秒后回报“已试完”。重点看是否不再出现 `error=0` 附近的 `-300` 以上大反推，以及过冲是否从 `-36/+74` 明显下降。 |

2026-06-19 V5 撤销运动中直接 HOLD、提前刹车窗口加大记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” / “感觉没什么进度” |
| 本次回读数据 | `pwm_us=2004`、`target_adc=2924`，反馈从 `2247` 上升并过冲到 `3051`，再回到 `2890`。误差范围 `+677..-127..+34`，duty 范围 `-400..+258`，事件为 `13/14/16/3/2`。 |
| 数据结论 | 用户反馈成立，上一版进度不明显。真实原因是：`event=14` 只在 `error=100` 才触发滑行刹车，此时太晚；随后我新增的“非静态小误差直接 HOLD”在 `error=40` 时把控制环切入 HOLD 并清零速度估计，但机械仍在高速前进，所以继续冲到 `error=-127`，再靠 `event=16` 反向锁回。 |
| 本次源码修改 | `src/app_entry.c`：撤销“运动中 `abs_error <= static_quiet_band` 直接 HOLD”的补丁，只保留静态相位的小误差安静 HOLD；把默认提前滑行刹车窗口 `Bmin` 从 `64 count` 提高到 `160 count`，`Bmax` 从 `260 count` 提高到 `360 count`；保留 `event=14` 不被覆盖的修正。目标是让 `error≈150` 时就撤掉同向驱动，不等到 `error=100` 才滑行。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19488 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `fefbc24d70eac191f0d01e152422d7a2fb62ff3aa458b154028be2290ebb5844`；ELF `a86f3527b03b3c419eb9f086d7d8549ab6a60a9249484a8fc02fed5d8667da76`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800488d`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=0 count=0 rows=0`，没有被上电静止状态误触发。 |
| 下一步 | 用户再做 `1000us -> 2000us`。重点看 `event=14` 是否提前到 `error≈150` 左右、最大过冲是否低于上一版 `127 count`。如果仍大，下一步不再只滑行，需要加入“受限反向动态刹车”并限制峰值，避免大电流冲击。 |

2026-06-19 V5 提前刹车有效但尾段重新加速修正版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | `pwm_us=2002`、`target_adc=2920`，反馈 `2683 -> 2974 -> 2959`，误差范围 `+237..-54`，duty 范围 `-402..+252`。`event=14` 已提前出现在 `error=213/202/192/118/55/19`，最大过冲从上一版 `127 count` 降到 `54 count`。 |
| 数据结论 | 提前刹车方向有效，但尾段仍有二次摆动根因：进入 `event=14` 后，后续几帧又重新给了同向 `+80/+240`，随后在 `error=+3/-9` 给出反向 `-223/-402`，造成一次小反摆。需要让刹车状态保持到真正稳定，而不是每帧重新回到 TRACK。 |
| 本次源码修改 | `src/app_entry.c`：新增 `brake_latch_active`。当 `s_lcdm_servo_v3_phase == BRAKE`、外部 PWM 未变化、误差仍在 `Bmax` 刹车窗口内时，强制 `duty_calc=0` 并保持 `event=14`，不允许尾段重新同向加速，也不允许目标点附近立即大反推。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19520 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `53225c5bf971b89c5f1a395d8631825d7a10560cce3582375c1534640a01b000`；ELF `2f04179eccf7bedab9c099554fb31fd4736d53ec1ae9879b5047c19214bd9485`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048ad`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=14 count=14 rows=14`。大部分为 `error=16..18/duty=0/event=2`；其中 1 帧 `feedback=2793/error=128/duty=80/event=16` 后立即回到 `error=39/17/duty=0`。判断为一次静止 ADC/观测瞬态触发，不是持续驱动，后续需留意是否对应实际嗞嗞声。 |
| 下一步 | 用户再做 `1000us -> 2000us`。重点看刹车后是否不再出现 `+80/+240` 同向再加速和 `-223/-402` 大反推，最大过冲是否进一步低于 `54 count`。 |

2026-06-19 V5 刹车锁存过强、改为受限反拉记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | `pwm_us=2002`、`target_adc=2920`，反馈 `2260 -> 3114`，误差范围 `+660..-194`。进入 `event=14` 后一直 `duty=0`，从 `error=208` 滑行到 `error=-194`，没有任何反向拉回。 |
| 数据结论 | 上一版“刹车锁存”过强。它成功禁止了尾段重新加速和大反打，但过目标后仍保持 `duty=0`，导致舵板靠惯性冲过后停在目标外约 `190 count`，无法回位。正确逻辑应是：未过目标时禁止同向推；过目标且误差仍大时，允许受限反向制动。 |
| 本次源码修改 | `src/app_entry.c`：新增 `s_lcdm_servo_motion_dir` 记录本次运动原始方向；新增 `LCDM_SERVO_V3_BRAKE_REVERSE_MAX_DUTY=160`。刹车锁存状态下，如果尚未过目标，保持 `duty=0/event=14`；如果已经过目标且 `abs_error > static_quiet_band`，按误差方向给受限反拉，最小 `80`、最大 `160`，避免回到之前 `-402` 的硬反打。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19648 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `0e538f287b72b77cab27634d4e738b72b0d4a9bc0f363601523a0d6b596f5e7e`；ELF `8c562f36807935e82406060489974ec8d1e4158e41b49573d4ef00c355c80470`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800492d`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=9 count=32 rows=32`，内容为 `pwm_us=2004`、`target_adc=2924`、`feedback_adc=2935..2939`、`error=-11..-15`，全部 `duty=0/event=2`，没有静止锁止输出。 |
| 下一步 | 用户再做 `1000us -> 2000us`。重点看过目标后是否出现受限反拉 `80..160`，最大过冲是否小于 `194 count` 且能回到目标附近；若仍回摆，再把受限反拉峰值或释放窗口微调。 |

2026-06-19 V5 受限反拉不足、峰值提高记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | `pwm_us=2003`、`target_adc=2921`，反馈 `2247 -> 3034`，误差范围 `+674..-113`，duty 范围 `-160..+398`。过目标后出现受限反拉 `-80/-160`，没有回到之前 `-402` 的大反打。 |
| 数据结论 | 两段式刹车方向正确，但 `-160` 反拉力不足。过目标后反馈停在 `3031..3034`，误差约 `-110`，说明小反拉无法把机械拉回目标附近。需要增加反拉峰值，但仍限制在低于原先失控反打的范围。 |
| 本次源码修改 | `src/app_entry.c`：`LCDM_SERVO_V3_BRAKE_REVERSE_MAX_DUTY` 从 `160` 提高到 `260`。其它刹车窗口、刹车锁存和运动方向记录逻辑不变。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19652 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `4ea4ee9d53cc057aa53b1f68028b230259ac733390cce04a62fc7988d305427e`；ELF `dee0722920275e00ff09dc77b118016ebd460c19167af6b396fb4157947b0f39`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004931`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=16 count=16 rows=16`，`pwm_us=2001`、`target_adc=2917`、`feedback_adc=2911..2915`、`error=2..6`，全部 `duty=0/event=2`。 |
| 下一步 | 用户再做 `1000us -> 2000us`。重点看受限反拉 `-260` 是否能把 `error=-110` 拉回目标附近，并观察是否出现新的回摆。 |

2026-06-19 V5 反拉有效、提前刹车再提前记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | `pwm_us=2001`、`target_adc=2917`，反馈 `2237 -> 3011 -> 2925`，误差范围 `+680..-94..-8`，duty 范围 `-260..+282`。`event=14` 在 `error=208/136/71/16/-35...` 持续，过目标后受限反拉 `-80/-207/-260` 将位置拉回到 `error=-8` 并进入 `HOLD/event=2`。 |
| 数据结论 | 受限反拉 `260` 有效：上一版停在 `error≈-110` 的问题解决，最终回到目标附近；也没有出现之前 `-402` 的大反打。剩余问题是最大过冲仍有 `94 count`，原因是 `error=243` 时还在同向推 `+204`，到 `error=208` 才开始滑行。 |
| 本次源码修改 | `src/app_entry.c`：提前刹车窗口再提前，`LCDM_SERVO_V3_MIN_BRAKE_BAND` 从 `160` 提高到 `240`，`LCDM_SERVO_V3_BRAKE_MAX_BAND` 从 `360` 提高到 `420`；受限反拉上限 `260` 保持不变。目标是让 `error≈240` 时先撤同向驱动，减少惯性过冲。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19652 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `9b8ebe22cb7a72fba2a595eb0befeee5f7d09ab8c78f22fcbf6f1b9da6bd49df`；ELF `22efd3de3090625a12616bc5fd6690a920ebff953140923e0bf62dee79663d9c`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004931`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=0 count=0 rows=0`，没有上电静止误触发。 |
| 下一步 | 用户再做 `1000us -> 2000us`。重点看 `event=14` 是否提前到 `error≈240`，最大过冲是否低于 `94 count`，以及最终是否仍能回到 `±10 count` 附近。 |

2026-06-19 V5 过冲降低但最终偏差仍大、反拉释放改窄记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” |
| 本次回读数据 | `pwm_us=2002`、`target_adc=2920`，反馈 `2111 -> 2986 -> 2953`，误差范围 `+809..-66..-33`，duty 范围 `-198..+324`。`event=14` 从 `error=293` 开始，最大过冲从上一版 `94 count` 降到 `66 count`。 |
| 数据结论 | 提前刹车有效，过冲继续下降；但最终停在 `error=-33`，没有进入目标附近。原因是受限反拉在 `abs_error <= static_quiet_band`，约 `40 count`，就停止输出，因此无法继续把位置拉到 `±10..±24 count` 内。 |
| 本次源码修改 | `src/app_entry.c`：过目标后的受限反拉释放条件从 `abs_error > static_quiet_band` 改为 `abs_error > hold_release_band`。即不再在 `40 count` 停反拉，而是继续拉到 `Rel` 设定附近，约 `24 count`，再释放。提前刹车窗口 `Bmin=240/Bmax=420` 和反拉上限 `260` 不变。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19648 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `e3dd7f9c42cbf924205fe919b15aff8163078325e13629d259ecf6c1c619a61e`；ELF `b100eb4e61736ff9b13febe94059507069a4817d07a0829e312399f28ca245d4`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800492d`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | 抓到低端 `1004us` 静态锁止窗口：`target_adc=1186`、`feedback_adc=1127..1162`、`error=59..24`。先有数帧 `event=16 duty=224/240`，随后 `event=2 duty=0`，最终约 `error=24..25`。这不是高端运动数据，后续若低端有嗞嗞声需单独看。 |
| 下一步 | 用户再做 `1000us -> 2000us`。重点看最终误差是否从 `-33` 收到 `±24` 以内，并确认没有引入新的回摆。 |

2026-06-19 V5 用户质疑试验次数、修正 BRAKE 过早进入 HOLD 记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” / “还要试多少次才能妖正哦” |
| 本次回读数据 | `pwm_us=2002`、`target_adc=2920`，反馈 `2283 -> 2983 -> 2955`，误差范围 `+637..-63..-35`，duty 范围 `-300..+372`。数据中 `error=23` 时进入 `HOLD/event=2`，随后惯性继续冲到 `error=-40/-63`，之后静态 `LOCK/event=16` 接管并输出 `-80/-300/-224`。 |
| 数据结论 | 用户质疑合理；不能继续盲调参数。这次不是单纯参数问题，而是状态机问题：BRAKE 相位还没真正稳定时，通用 HOLD 条件过早生效；随后惯性过目标，程序已经脱离 BRAKE，导致静态 LOCK 介入，形成多余反打和最终偏差。 |
| 本次源码修改 | `src/app_entry.c`：禁止 `s_lcdm_servo_v3_phase == BRAKE` 时走通用 HOLD 条件。这样接近目标后的惯性段仍留在 BRAKE，由原有 `event=14` 滑行/受限反拉处理，不再提前切到 HOLD 后被静态 LOCK 接管。曾尝试加入“BRAKE 稳定 20 周期再 HOLD”的更完整实现，但完整 LCDM 调试版 Flash 超出 20 字节；已改为当前省空间实现。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19656 B`、`data=672 B`、`bss=2192 B`，Flash 空间已非常紧张。 |
| HEX/ELF SHA256 | HEX `65bb6a44e41de5da225db3d7f8c056337dcd46ab97e514ac4e7d5c894eaf6924`；ELF `919ea70ee6fbe51c334549e68b87c22dacd2942153cf204f09e6aa800974ab2c`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004935`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=0 count=0 rows=0`，没有上电静止误触发。 |
| 下一步 | 用户再做一次 `1000us -> 2000us`。这不是继续盲试，而是验证状态机修正是否消除 `event=16` 在尾段接管；若数据仍不理想，下一步应减少 LCDM/调试代码体积，给控制状态机留更多 Flash 空间。 |

2026-06-19 V5 旧产物误用纠正、重新构建下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已试完” / “你怎么老犯这种错呢，调来调去你还是在用旧程序和参数” |
| 问题确认 | 用户批评成立。检查发现 `src/app_entry.c` 修改时间为 `2026-06-19 13:01`，但 `build/py32f002_servo.elf/hex` 仍是 `2026-06-19 12:28` 的旧产物；上一轮数据读取和后续判断存在“源码已改、板上仍旧”的风险。 |
| 纠正动作 | 重新执行干净构建；首次链接因完整 LCDM 调试版 Flash 超出 `4 bytes` 失败。为释放空间，只压缩 P50 第一行显示格式，从 `P%04u R%04u W%02u` 改为 `P%u R%u W%u`；不改变伺服控制参数、BRAKE/HOLD 状态机和 H 桥输出逻辑。 |
| 当前关键源码核对 | `Bmin=240`、`Bmax=420`、`BRAKE_REVERSE_MAX_DUTY=260` 保持不变；两处 HOLD 条件均带有 `s_lcdm_servo_v3_phase != LCDM_SERVO_V3_PHASE_BRAKE`，包括普通 HOLD 和 MICRO_HOLD。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19680 B`、`data=672 B`、`bss=2192 B`，Flash 使用 `20352/20480 B`，剩余 `128 B`。 |
| HEX/ELF SHA256 | HEX `c0a1d9304d30f501e2a140b6a4bd08d4bca09a45aa3fc9465c580984eaa2b56e`；ELF `bbae5f8eea28de24d9da8bf65d7cb940b2543a06376afa1734cbf757275f1846`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004951`，返回 `Successfully resumed device`。 |
| 下载后回读 | `head=15 count=15 rows=15`。当前板上 `pwm_us=2002`、`target_adc=2920`、`feedback_adc=2920..2927`、`error=-7..0`、`duty=0`、`event=2`，确认新固件已运行。 |
| 后续硬性流程 | 每次下载前必须重新构建并核对 `build/*.hex` 时间戳、`arm-none-eabi-size` 和 SHA256；记录后再下载。若构建失败，不允许继续用旧 `hex` 测试。 |
| 下一步 | 用户只做一次受控动作，例如 `2000us -> 1000us` 或 `1000us -> 2000us`，动作后停 1 秒再回报“已试完”。我随后立即回读 `servo_curve_ring.csv`，重点看 `event=14` 是否保持到过目标后、`event=16` 是否不再提前接管、最大过冲和最终误差。 |

2026-06-19 V5 第三处 HOLD 漏排除 BRAKE 修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已完成” |
| 本次回读数据 | `servo_curve_ring.csv` 共 `32` 行，目标固定 `pwm_us=2002`、`target_adc=2920`。动作尾段反馈 `2456 -> 2971`，误差 `+464 -> -51`，duty 范围 `-80..+288`，事件分布：`event13=6`、`event14=6`、`event2=3`、`event3=11`、`event16=6`。 |
| 数据结论 | `event=14` 从 `error=289` 开始滑行，直到 `error=25` 都保持刹车；但下一帧 `error=-3` 直接进入 `HOLD/event=2`，机械惯性继续冲到 `error=-51`，随后静态 `LOCK/event=16 duty=-80` 和 `event=3 duty=0` 反复切换，造成高端残余嗞嗞/轻抖。根因不是 PWM 抖动，而是还有第三处尾段 HOLD 条件没有排除 BRAKE 相位。 |
| 本次源码修改 | `src/app_entry.c`：在 `abs_error <= motion_hold_band && abs_velocity_q4 <= motion_hold_vel_q4 && traj_at_cmd` 的尾段 HOLD 条件中，补上 `s_lcdm_servo_v3_phase != LCDM_SERVO_V3_PHASE_BRAKE`。至此三处 HOLD 入口都禁止在 BRAKE 相位提前接管；不改 `Bmin/Bmax/反拉上限` 等参数。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19676 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `fb5af7abc06479c2bd68042225f60c8c147f8a52babba0a68c8302357bce602c`；ELF `edcc1b2648af1c46aa82f32917a49b850adf9f165ebaf5fe4e97b1229d629a38`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800494d`，返回 `Successfully resumed device`。 |
| 下载后回读 | 下载后 1 秒回读 `head=0 count=0 rows=0`，说明上电静止没有误触发采集，当前版本已运行。 |
| 下一步 | 用户再做一次同样动作并停 1 秒后回报。重点确认 `error` 过零后是否继续保持 `event=14`，由受限反拉接住，而不是提前进入 `event=2` 后再由 `event=16` 反复小力拉。 |

2026-06-19 V5 BRAKE 反拉斜率提高记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已完成” |
| 本次回读数据 | `servo_curve_ring.csv` 共 `32` 行，目标固定 `pwm_us=2003`、`target_adc=2921`。动作尾段 `feedback_adc=2100 -> 2967`，误差 `+821 -> -46`。`event=14` 已经保持到过零后，不再提前进入 `event=2/16`；但过目标后 `error=-43..-46` 时持续输出 `duty=-129..-138/event=14` 约 `180ms`，反馈基本停在 `2964..2967`，说明反拉力不足以把高端位置拉回目标。 |
| 数据结论 | 上一版“禁止 BRAKE 相位提前 HOLD”的方向正确，状态机没有再掉到静态 LOCK；当前问题转为 BRAKE 过目标后的反拉曲线太平，`error≈45 count` 只有约 `13% duty`，容易变成持续通电声而没有有效回位。 |
| 本次源码修改 | `src/app_entry.c`：BRAKE 锁存且已经过目标后的受限反拉由 `abs_error * 3` 改为 `abs_error * 5`，上限仍保持 `LCDM_SERVO_V3_BRAKE_REVERSE_MAX_DUTY=260`，最小 `80` 不变。其它刹车窗口和 HOLD 条件不变。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19676 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `8bd9f01328e6d6837560d10a8704f838c59b2d5987a8ef34b01edd983836b552`；ELF `f580a569613d732d879ef477f6c967bf9453e84e7f8b77b192be3cf89ccc0285`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800494d`，返回 `Successfully resumed device`。 |
| 下载后回读 | 下载后低端静止回读 `head=11 count=11 rows=11`。出现两次短暂 `event=16 duty=80` 将 `error=48/49` 拉回，随后稳定在 `event=2 duty=0`、`error=-5..-8`。 |
| 下一步 | 用户再做同样动作并停 1 秒后回报。重点看 `error=-40` 附近时 `duty` 是否提高到约 `-200`，反馈是否能回到 `±20 count` 内，以及是否引入新的回摆。 |

2026-06-19 V5 BRAKE 低速释放到 HOLD 记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已完成” |
| 本次回读数据 | `servo_curve_ring.csv` 共 `32` 行，目标固定 `pwm_us=2001`、`target_adc=2917`。`abs_error*5` 后反拉有效：尾段从 `error=-33` 收到 `-22..-24`，比上一版 `-43..-46` 明显改善。但 `event` 全程仍停在 `14`，在 `error=-24/-25` 附近出现 `duty=0` 与 `duty=-80` 来回切换，说明 BRAKE 释放条件过死。 |
| 数据结论 | 当前主要问题不是反拉力，而是 BRAKE 相位没有在低速、已过目标、误差进入 `Rel` 后释放回 HOLD。继续让 `event=14` 常驻会造成尾端轻微通电/声响。 |
| 本次源码修改 | `src/app_entry.c`：将 BRAKE 释放逻辑合并进已有 HOLD 条件。原 `abs_error <= hold_release_band && abs_velocity_q4 <= motion_hold_vel_q4 && traj_at_cmd` 条件现在允许两种情况进入 HOLD：非 BRAKE 相位，或 BRAKE 相位但已经过目标。保留 `abs_error * 5` 反拉和 `BRAKE_REVERSE_MAX_DUTY=260`。 |
| 体积处理 | 直接新增释放分支会导致完整 LCDM 调试版 Flash 超出 `60 bytes`。为保留控制逻辑，删除旧的未使用 LCDM 函数，并停止刷新实时帧中的辅助字段 `ref_velocity/traj_adc/traj_vel_q4/overshoot/pwm_candidate`；ring log 和核心帧字段 `time/pwm/raw/target/feedback/error/duty/velocity/state/event` 保留。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19592 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `3e39671ff5a682bed855b6f0068afa05d2cf4e585e25a70fe757c5a537a3b58f`；ELF `13cd1fd74384e4ccab57c28acdb504ba1286e9c5cf43ec84c79ffcea76af1376`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048f9`，返回 `Successfully resumed device`。 |
| 下载后回读 | 下载后 1 秒回读 `head=0 count=0 rows=0`，没有上电误触发采集。 |
| 下一步 | 用户再做同样动作并停 1 秒后回报。重点看尾段是否从 `event=14` 正常释放到 `event=2`，最终误差是否保持在 `±20..24 count` 内，且不再出现 `duty=0/-80` 来回切换。 |

2026-06-19 V5 BRAKE 过零不立即释放、需反拉/延迟确认记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已完成” |
| 本次回读数据 | `servo_curve_ring.csv` 共 `32` 行，目标固定 `pwm_us=2002`、`target_adc=2920`。上一版确实能从 `event=14` 释放到 `event=2`，但释放太早：`error=2 -> -1` 过零后立即 HOLD，之后惯性继续到 `error=-46`，再触发 `event=16 duty=-80`。 |
| 数据结论 | 单纯“已过目标 + 误差小 + 速度低”仍不够，因为速度估算在过零瞬间可能偏低，实际机械还有惯性。必须禁止刚过零立即释放；只有已经过目标一段确认时间，或已经超出 `Rel` 并启动过反拉，再回到 `Rel` 内，才允许 HOLD。 |
| 本次源码修改 | `src/app_entry.c`：复用 `s_lcdm_servo_approach_brake_tail_count` 作为 BRAKE 过零确认标志。BRAKE 状态下，HOLD 释放需要 `s_lcdm_servo_approach_brake_tail_count >= 40`；若过目标且 `abs_error > Rel`，立即置 `40` 并进行 `abs_error*5` 反拉；若过目标但仍在 `Rel` 内，则逐周期累加，避免刚过零立即 HOLD。 |
| 体积处理 | 为容纳确认逻辑，保留上一版体积优化：旧 LCDM 未用函数删除；实时帧核心字段和 ring log 保留，辅助字段 `ref_velocity/traj_adc/traj_vel_q4/overshoot/pwm_candidate` 不再刷新。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；`text=19632 B`、`data=672 B`、`bss=2192 B`。 |
| HEX/ELF SHA256 | HEX `6c198bf0f00ce5fd73402f2b9a10f1019fb2e29de6333da56ebc590b53f3cee2`；ELF `4f16839712955cf82b1ec33429df773f774879d5a69a078af750e4c37a143fc6`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004921`，返回 `Successfully resumed device`。 |
| 下载后回读 | 下载后 1 秒回读 `head=0 count=0 rows=0`，没有上电误触发采集。 |
| 下一步 | 用户再做同样动作并停 1 秒后回报。重点看 `error=-1` 附近是否继续保持 `event=14`，是否等到反拉/延迟确认后再进入 `event=2`，最终是否不再冲到 `-40` 以上。 |

2026-06-19 调试方法纠偏记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “四个小时也找不到规律，这个办法不可取” / “实际正真抓到问题点做法，是不需要一直调试，而是一针见血的完成要素，而只是将可调参数做一点点微调即可。” |
| 结论 | 用户判断正确。当前围绕尾段 `BRAKE/HOLD/LOCK` 小步试参数，效率低且只是在补局部现象，没有形成稳定控制规律。后续不能继续用反复下载试错代替架构判断。 |
| 后续原则 | 先用数据找结构性根因，再改状态机；参数只用于最后微调。尾段控制必须变成互斥状态：运动、滑行、反刹、稳定确认、保持，不能让 HOLD、LOCK、BRAKE 多个分支同时抢控制权。 |
| 暂停动作 | 未下载新的临时小改；保持最后已下载基线版本作为对照。下一步应先整理尾段状态机规则，再做一次明确代码改动。 |

2026-06-19 V6 架构重写启动记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “也就是说，如果基数和逻辑设计是没问题的话，就逄换马达从1万2转换成1万8的转速，或者，减速齿轮比从19：1变成25：1，也就是调一点点参数就能搞定，这样的逻辑程序才是我们想要的。不然就是逻辑不正确。” / “请开始做V6版本吧” / “我就不信你搞不定，你可以的，我相信你” |
| 架构结论 | V6 不再继续 V5 的局部参数试错；必须把目标变化运动控制、过目标反刹、稳定确认、静止外力锁止拆成互斥状态。HOLD、BRAKE、LOCK 不能在同一尾段抢控制权。 |
| 本次执行原则 | 保留现有 PWM 输入、ADC observer、TJC/LCDM、曲线回读和安全保护；重写 `lcdm_servo_loop_update_fast()` 后半段 H 桥决策逻辑。参数只作为曲线斜率/阈值，不能再用参数补逻辑漏洞。 |

2026-06-19 V6 首版状态机落地和下载记录：

| 项目 | 内容 |
|---|---|
| 本次源码修改 | `src/app_entry.c`：新增 V6 默认参数 `SETTLE_COUNT/BRAKE_CONFIRM_COUNT/LOCK_RELEASE_COUNT/LOCK_GAIN/BRAKE_VEL_DUTY_DIV/DRIVE_ERR_GAIN`；将 `lcdm_servo_loop_update_fast()` 后半段改为互斥状态机：`HOLD`、`LOCK`、`ACCEL/TRACK`、`BRAKE`、`RETURN(settle)`；目标变化进入运动状态，静止外力才进入锁止状态。 |
| 关键结构变化 | 删除旧的内部轨迹积分/加减速段，V6 直接用 `目标ADC - 反馈ADC` 和 ADC observer 速度闭环决定驱动、滑行、反刹和保持。这样避免旧轨迹造成尾段慢走，也减少 Flash 占用。 |
| 静止噪声修正 | 首次 V6 下载后回读发现单点 ADC 跳变 `feedback_adc 2930 -> 2981` 会立即触发 `event=16`。已增加静止锁止入口连续确认：非 `LOCK` 状态下需连续 2 次超出释放带才进入外力锁止，避免单点毛刺触发 H 桥。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过，参数为 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`；最终版本 `text=19468 B`、`data=672 B`、`bss=2192 B`。 |
| 产物时间和 SHA256 | 产物时间 `2026-06-19 13:53:54`；HEX `48f76bcb69d2d0238bb477b3ac7b4ede2991c1351d8393f5baa6a21068be7775`；ELF `9ae50b38caf4d189086cdfa0724b08122b82c41154b034b931b226b1fea2c1bc`；BIN `d12fe64d01827d7e5d332f9d83d687616f631acb417173701bd4b5af213f1567`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800487d`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=0 count=32 rows=32`；`pwm_us=2004`、`target_adc=2924`、`feedback_adc=2927..2932`、`error=-3..-8`、`duty=0`、`state=1`、`event=2`。没有上电静止误触发，没有 `event=16`。 |
| 下一步测试 | 先只做一次受控动作：建议 `2000us -> 1000us`，停 1 秒后回报“已试完”。我会回读 ring log，看 V6 的 `12/13/14/15/2/16` 状态切换是否符合预期，再决定是否只调参数，或继续修结构。 |

2026-06-19 V6 首测 BRAKE 漏洞修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “已完成” |
| 本次回读数据 | `pwm_us=1003`、`target_adc=1184`。动作从 `feedback_adc=2500` 往低端运行，过目标后 `error=+31/+47` 已进入 `event=14` 反刹，但随后反馈继续冲到 `feedback_adc≈93`，最终 `error≈+1090`、`duty=0`、`state=9/event=14`。事件分布：`event12=6`、`event14=26`。 |
| 数据结论 | 这是 V6 首版状态机漏洞，不是参数微调问题：过目标后反刹上限仍沿用旧 `260`，且当误差超过 `Bmax` 后，`BRAKE` 分支不再输出，导致过目标后一路滑到低端。 |
| 本次源码修改 | `src/app_entry.c`：`BRAKE` 状态下只要已经 `crossed_target`，不再受 `Bmax` 限制；过目标后的反向制动上限从旧的 `LCDM_SERVO_V3_BRAKE_REVERSE_MAX_DUTY=260` 改为 `min(lock_max_duty, loop_max_duty)`，避免过目标后丢输出。 |
| 构建结果 | 完整 LCDM 调试版 Ubuntu LTO 构建通过；`text=19468 B`、`data=672 B`、`bss=2192 B`。 |
| 产物时间和 SHA256 | 产物时间 `2026-06-19 14:15:23`；HEX `79da72c5368058ead8183b06f117fdd89b7fdf54c711a8ee4d1cc2c6f27d8184`；ELF `c5f3bb6442d442dbe623cc746e281d7f2bb28451dd1270a852de2ee0c81ebfe3`；BIN `5b8d5098861c81391ba24459a721e4277759dc28b76c8e0a7155fd92980608a2`。 |
| 下载结果 | macOS pyOCD、DAPLink、SWD `100kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x0800487d`，返回 `Successfully resumed device`。 |
| 下载后静止回读 | `head=0 count=0 rows=0`，没有上电误触发。 |
| 下一步测试 | 再做一次同样方向动作，例如 `2000us -> 1000us`，停 1 秒后回报。重点看过目标后 `event=14` 是否持续反向输出，不再丢成 `duty=0`。 |

2026-06-19 V6 内部阶跃自测方法记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “下一步，我建义你可以自行转动舵板再记录参数，换句话说，无需外界PWM，直接自行定义从1000-2000，再从2000到1000，最后记录参数，就清楚要改什么，这样效率会更高。直到所有参数达标后，我再来试外界PWM信号控制。” |
| 调试结论 | 接受该方法。后续先使用固件内部阶跃命令自动生成 `1500 -> 2000 -> 1000` 循环目标，再用 SWD 直接抓 `target/feedback/error/duty/velocity/event` 曲线。这样能排除外部 PWM 源和手动切换节奏干扰，先把控制逻辑本身调到稳定，再恢复外部 PWM 给用户验证。 |
| 已有源码支持 | `src/app_entry.c` 已加入 `SERVO_ENABLE_INTERNAL_STEP_TEST`：编译打开时，`lcdm_internal_step_test_sample()` 每 `1200ms` 自动切换一次目标脉宽，周期为 `1500us`、`2000us`、`1000us`；`Makefile` 已支持 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 编译宏。 |
| 本次源码修改 | 根据上一轮数据，`LCDM_SERVO_V3_BRAKE_MAX_BAND` 从变差的 `700` 退回到表现较好的 `620`；保持 `Bmin=300`、预刹车上限 `RETURN_MAX_DUTY=220`。`Bmax=700` 会让长行程过冲扩大，不继续沿用。 |
| 下一步 | 构建并下载内部自测固件，抓取一段自动 `1500/2000/1000` 曲线。若 `Bmax=620` 数据恢复到过冲约 `44..52 count` 的基线，再做结构性速度刹车曲线，而不是继续盲目加大反刹力。 |

2026-06-19 V6 内部自测链路修正和数据记录：

| 项目 | 内容 |
|---|---|
| 发现的问题 | 内部自测最初会卡在 `2000us`：原因是 `lcdm_internal_step_test_sample()` 把 `pulse_count` 当成“脉宽变化计数”，而真实 PWM 输入里的 `pulse_count` 是每个 PWM 周期都会递增。输入稳定确认因此无法累计，目标会锁在旧值。 |
| 本次源码修改 1 | `src/app_entry.c`：内部自测改为独立维护 `edge_count/pulse_count`，并且每次模拟采样都递增 `pulse_count`；`1500 -> 2000 -> 1000` 循环已通过 live SWD 验证。 |
| 发现的问题 2 | 原 `32` 条 ring log 容量太小，只能看到尾段；扩大到 `96` 会 RAM 溢出。 |
| 本次源码修改 2 | ring log 改为 `35` 条，并加入 `velocity_q4` 字段；`tools/dump_servo_curve_log.py` 同步改为 `ENTRY_SIZE=20`、`CAPACITY=35`，以后能直接看到误差、H 桥 duty 和 ADC 速度关系。 |
| 发现的问题 3 | 静止 HOLD 时原程序使用 `ADC_Feedback_ReadFastRaw()` 单次采样，live/ring 数据看到过一次静止反馈瞬间跳约 `60 count`，会被 observer 当成真实速度，触发一次修正。 |
| 本次源码修改 3 | HOLD 状态改用 `ADC_Feedback_ReadRaw()` 多次去极值平均；LOCK 状态仍保留 `ADC_Feedback_ReadFastRaw()`，兼顾外力锁止响应速度。 |
| 本次源码修改 4 | BRAKE 尾段如果 `velocity_q4` 已低、误差仍超过释放带，反拉 duty 下限提升到 `lock_min_duty`，避免停在 `error≈30` 但长期 `duty≈150` 推不动、持续嗞嗞。 |
| 构建/下载记录 | 最终内部自测版构建通过：`text=19628 B`、`data=672 B`、`bss=2392 B`；产物时间 `2026-06-19 15:20:31`；HEX SHA256 `0469e940044f6686f72015f46a67047c52fb55fa92eec344e848434a207a8f3f`；macOS DAPLink、SWD `100kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 数据文件 | `build/servo_curve_live/live_internal_hold_avg_adc_8s.csv`；辅助 ring 文件包括 `internal_ring_velocity_bmax620.csv`、`internal_ring_tail_min_lock.csv`、`internal_ring_hold_avg_adc.csv`。 |
| 数据结论 | 8 秒 live 连续采样中，内部目标正常循环 `1500/2000/1000`。两次 `2000->1000` 长行程过冲约 `48/53 count`，最终误差 `-17/-10 count`；`1000->1500`、`1500->2000` 末端最终误差约 `9..13 count`。HOLD 平均采样后，未再看到静止单点跳几十 count 的触发。 |
| 当前状态 | 该固件仍是 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 内部自测版，不适合直接接外部 PWM 验证。若要恢复普通舵机模式，需重新构建时去掉 `SERVO_ENABLE_INTERNAL_STEP_TEST=1`。 |
2026-06-19 内部自测 Rel=14 待验证记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “要快点解决，过冲和慢转到位等问题” |
| 当前判断 | 不能继续盲调。上一轮数据说明高速提前刹车窗口加大到 `+220` 方向会更差，已经回退；当前应验证默认 `Rel=14` 是否能减少停点偏差和尾段慢找位。 |
| 已有源码状态 | `src/app_entry.c` 中 `P70 Rel` 默认已经改为 `LCDM_SERVO_HOLD_RELEASE_BAND=14`，且默认范围为 `6..160`、步进 `1`。内部自测仍开启，目标自动循环 `1500 -> 2000 -> 1000`，用于排除外部 PWM 和手动切换干扰。 |
| 下一步 | 下载当前内部自测版并抓 8 秒 SWD 曲线。重点看长行程过冲是否低于约 `35 count`、最终误差是否进入 `±10..14 count`、尾段是否仍长期 `event=14` 或 duty 非零慢找位。 |

2026-06-19 内部自测 Rel=14 首轮数据和方向切换修正：

| 项目 | 内容 |
|---|---|
| 数据文件 | `build/servo_curve_live/live_internal_rel14_8s.csv`，共 `222` 条有效 SWD 采样。 |
| 数据结论 | `Rel=14` 后停点最终误差大多已进入 `±10 count` 左右，说明停点释放窗口比 `Rel=24` 明显更合理。但 `2000 -> 1000` 等长行程仍有约 `49..62 count` 过冲，个别采样段可见更大；到 HOLD 时间约 `470..630ms`。 |
| 根因判断 | 接近目标需要反向刹车时，数据经常出现 `event=3 duty=0`，说明方向切换保护期间 H 桥处于滑行；在高速接近目标时，这个滑行窗口直接扩大过冲，也会造成尾段慢找位。 |
| 本次源码修改 | `src/app_entry.c`：`LCDM_SERVO_REVERSE_DEAD_MS 35 -> 18`。这不是调 PID，而是减少刹车方向切换时的无驱动滑行时间；仍保留方向死区保护，避免正反桥臂直接冲突。 |
| 下一步 | 构建并下载内部自测版，再抓同样 8 秒数据，对比 `event=3 duty=0` 持续时间、长行程过冲和到 HOLD 时间。 |

2026-06-19 内部自测 RevDead18 数据和预反刹修正：

| 项目 | 内容 |
|---|---|
| 数据文件 | `build/servo_curve_live/live_internal_revdead18_8s.csv`，共 `159` 条有效 SWD 采样。 |
| 对比结果 | 相比 `Rel=14/RevDead35`，1000 方向过冲最大值从该轮数据的 `100 count` 降到 `61 count`，平均值从约 `41 count` 降到约 `36 count`；但平均到 HOLD 时间仍约 `443ms`，说明缩短反向死区只改善部分滑行损失，没有解决预刹车力不足。 |
| 根因判断 | 过冲大的段落在目标前已经进入 `event=14`，但未过目标时的反刹上限仍是 `LCDM_SERVO_V3_RETURN_MAX_DUTY=220`；速度高、误差还有 `300..600 count` 时反刹力偏小，等过点后才用更大 duty 拉回，天然会形成过冲和回摆。 |
| 本次源码修改 | `src/app_entry.c`：`LCDM_SERVO_V3_RETURN_MAX_DUTY 220 -> 360`。只提高“未过目标、正在高速接近目标”的预反刹上限，不扩大刹车距离窗口，避免再走 `Bmax=700` 那种过早刹车变慢的方向。 |
| 下一步 | 构建并下载内部自测版，抓 `live_internal_revdead18_prebrake360_8s.csv`，重点看 1000 长行程过冲是否从 `50..60 count` 降到 `35 count` 附近，同时 HOLD 时间不能明显变长。 |

2026-06-19 预反刹 360 数据和公式修正：

| 项目 | 内容 |
|---|---|
| 数据文件 | `build/servo_curve_live/live_internal_revdead18_prebrake360_8s.csv`，共 `121` 条有效 SWD 采样。 |
| 对比结果 | 1000 方向过冲平均值从上一轮约 `36.4 count` 降到约 `10.4 count`，最大值 `52 count`；2000 方向过冲约 `0..5 count`；最终停点误差约 `±12 count` 内。该方向是有效的。 |
| 剩余问题 | 唯一明显异常段：`1000us` 目标，`feedback=1444`、`error=-265`、`velocity=-278 q4` 时，预反刹 duty 约 `290`，仍冲到 `feedback=1127`，过冲约 `52 count`。说明不是刹车窗口太晚，而是高速接近目标时预反刹公式只按速度给力，略偏保守。 |
| 本次源码修改 | `src/app_entry.c`：未过目标且高速接近目标的预反刹公式从 `reverse_abs = abs_velocity_q4` 改为 `reverse_abs = abs_velocity_q4 + abs_error / 8`，上限仍保持 `360`。目标是只在剩余误差仍较大时补一点反刹力，不改变前段驱动和 HOLD 释放。 |
| 下一步 | 构建下载内部自测版，抓 `live_internal_prebrake360_err8_8s.csv`，确认 1000 方向最大过冲是否进一步压低，且动作不变慢。 |

2026-06-19 预反刹 err/8 数据和尾段慢动作修正：

| 项目 | 内容 |
|---|---|
| 数据文件 | `build/servo_curve_live/live_internal_prebrake360_err8_8s.csv`，共 `116` 条有效 SWD 采样。 |
| 对比结果 | 1000 方向 5 次有效段过冲全为 `0 count`；2000 方向最大过冲 `1 count`；1500 方向最大过冲 `12 count`；最终误差约 `±11 count`。过冲逻辑明显优于前版。 |
| 剩余问题 | 多个段落在尾段出现 `event=3 duty=0`，例如 2000 方向 `error=246/85 count` 时仍在滑行，导致最后一段到 HOLD 约 `400..500ms`。这说明慢转到位主要来自反向保护时间和多次方向切换滑行，而不是前段驱动不足。 |
| 本次源码修改 | `src/app_entry.c`：`LCDM_SERVO_REVERSE_DEAD_MS 18 -> 8`。保持预反刹公式不变，先减少尾段反向 PWM 切换时的无驱动滑行时间。 |
| 下一步 | 构建下载内部自测版，抓 `live_internal_revdead8_prebrake360_err8_8s.csv`。若过冲保持低、HOLD 时间下降，则该方向成立；若出现明显电流冲击或过冲回摆，立刻退回 `18ms`。 |

2026-06-19 RevDead8 内部自测结论和普通 PWM 切换：

| 项目 | 内容 |
|---|---|
| 数据文件 | `build/servo_curve_live/live_internal_revdead8_prebrake360_err8_8s.csv`、`build/servo_curve_live/live_internal_revdead8_prebrake360_err8_b_8s.csv`。 |
| 数据结论 | 两轮 `8ms` 内部自测均未破坏过冲。第二轮统计：1000 方向 7 次有效段过冲全为 `0 count`、HOLD 平均约 `380ms`；1500 方向最大过冲 `1 count`、HOLD 平均约 `422ms`；2000 方向最大过冲 `2 count`、HOLD 平均约 `476ms`。整体比 `18ms` 版本略快或接近，1000 方向明显更快。 |
| 当前取舍 | 暂保留 `LCDM_SERVO_REVERSE_DEAD_MS=8`、`RETURN_MAX_DUTY=360`、预反刹 `velocity + error/8`。该版本过冲数据明显好于此前版本，但实物测试时仍需关注反向切换瞬间电流，若出现闪大电流，再改为动态死区而不是直接退回旧算法。 |
| 工具修正 | `tools/live_servo_curve_swd.py` 新增 `--duration-sec`，后续 SWD 抓 8 秒数据可自动退出，不再靠外部 `kill`，减少调试等待。 |
| 下一步 | 构建去掉 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 的普通外部 PWM 版本并下载，交给用户做 `1000/1500/2000us` 实测。 |

2026-06-19 普通外部 PWM 版下载记录：

| 项目 | 内容 |
|---|---|
| 构建参数 | `LTO=1 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1 SERVO_ENABLE_PA0_POT_POWER=1 BOARD_TSSOP20_DEBUG_PINS=1 SERVO_TJC_UART_ALT_PINS=1 TJC_LCDM_BAUD_OVERRIDE=230400 SERVO_TJC_RX_ENABLE=1 SERVO_HBRIDGE_FORCE_HIZ=0`，未开启 `SERVO_ENABLE_INTERNAL_STEP_TEST`。 |
| 构建结果 | Ubuntu 构建通过；`text=19572 B`、`data=672 B`、`bss=2376 B`。 |
| 产物 SHA256 | HEX `14dee98063875874ca4c995b59ad2c6c86ec6f2ddc37fab34fcf7d936a3cb83f`；ELF `334cc5c8100ddca6f920b027372bee59dad1dde7ef3ee351fcfa166d0ab3e0c8`；BIN `4e210a74a9cc452a6e4e17bc58b60d09d76860887e6f095b1704c4b77572be59`。 |
| 下载结果 | macOS DAPLink、SWD `50kHz`、`connect_mode=under-reset` 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048e5`，pyOCD 返回 `Successfully resumed device`。 |
| 下载后确认 | `Makefile` 默认 `SERVO_ENABLE_INTERNAL_STEP_TEST ?= 0`，普通版构建未传入内部自测宏；短抓 `build/servo_curve_live/live_normal_pwm_postflash_3s.csv` 未出现内部 `1500/2000/1000` 自动阶跃数据。 |
| 用户测试重点 | 先用外部 PWM 做 `1000/1500/2000us` 切换，观察是否仍有到点来回摆和尾段慢走；同时注意反向切换瞬间电流。如出现明显闪大电流，下一步把固定 `8ms` 改成按 duty 大小动态死区。 |

2026-06-19 用户要求复测内部阶跃记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “刚离开了一会，你再试我看看，没达标就不能下一步” |
| 执行动作 | 重新构建并下载内部阶跃自测版，参数为 `SERVO_ENABLE_INTERNAL_STEP_TEST=1`，自动循环 `1500 -> 2000 -> 1000`。下载使用 macOS DAPLink、SWD `50kHz`、`connect_mode=under-reset`，pyOCD 返回 `Successfully resumed device`。 |
| 自测产物 | HEX `a4a622019d08c0fa7018addfd2ca0d60c6e203b5f6c77ba83128547cb25ba27b`；ELF `e121fc22452f6e64da71c2b1a78899aa66de8781aff12dcac0598312a15b090c`；BIN `3f2fb5003b06c34b728dfe781e9536fa966c407fd5fc63d18bfcda7497bfc427`。 |
| 数据文件 | `build/servo_curve_live/live_internal_retest_a_10s.csv`、`build/servo_curve_live/live_internal_retest_b_10s.csv`。 |
| 两轮合并统计 | `1000us`：6 次有效段，过冲平均/最大 `0/0 count`，HOLD 平均/最大约 `374.7/496ms`，最终误差最大 `10 count`。`1500us`：6 次有效段，过冲 `0/0 count`，HOLD 平均/最大约 `429.8/497ms`，最终误差最大 `7 count`。`2000us`：6 次有效段，过冲平均/最大约 `1.2/2 count`，HOLD 平均/最大约 `490.2/532ms`，最终误差最大 `2 count`。 |
| 达标判断 | 按当前内部判定门限：过冲 `<=20 count`、最终误差 `<=14 count`、首次 HOLD `<=550ms`，两轮数据 `bad_count=0`，内部阶跃达标。 |
| 当前板子状态 | 为便于用户观察动作，暂时保留内部阶跃自测版在板子上；如果要接外部 PWM 继续人工测试，需要重新下载普通外部 PWM 版本。 |

2026-06-19 尾段慢滑逻辑漏洞修正：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前看不够丝滑，最后一段还是慢滑过去的，也就是说不存在加速与刹车的矛盾” / “还存在，不是不存在” |
| 数据复查 | 复测数据虽然过冲和最终误差达标，但多段尾段存在 `event=3 duty=0`。典型点：`1500us` 段 `error=240/205/79/58 count` 时进入 `event=3`；`2000us` 段 `error=187/158/112/95 count` 时进入 `event=3`。这正对应用户看到的最后一段慢滑。 |
| 根因判断 | 方向切换保护分支中，如果反向请求 `requested_abs_duty < LCDM_SERVO_REVERSE_MIN_DUTY(180)`，程序只关 H 桥一拍，但没有清掉 `s_lcdm_servo_output_dir` 旧方向标志。下一拍小反向请求仍会被判断为反向切换，于是可能反复 `event=3 duty=0`，形成尾段滑行。 |
| 本次源码修改 | `src/app_entry.c`：在方向切换保护中，若 `requested_abs_duty < LCDM_SERVO_REVERSE_MIN_DUTY` 不启动 8ms dead time，则立即调用 `lcdm_servo_clear_output_guard()`，保证下一拍可以进入新方向的小反向刹车/修正，不再被旧方向标志卡住。 |
| 下一步 | 构建并下载内部自测版，抓两轮数据。重点看 `event=3` 次数和 HOLD 时间是否下降，同时过冲仍需保持在当前达标范围。 |

2026-06-19 尾段过早刹车修正：

| 项目 | 内容 |
|---|---|
| 数据复查 | 小反向切换修正后，`event=3` 明显减少，但 `event=14` 增多，HOLD 时间没有下降。明细显示低速尾段在 `error≈200 count` 甚至更大时已经进入 `event=14` 小反刹/滑行，例如 `1500us` 段 `error=230/214/193/161 count`，`2000us` 段 `error=233/237/226/205 count`。 |
| 根因判断 | `Bmin=300` 对低速尾段过大，导致还没接近目标就进入刹车相。这样虽然过冲小，但最后一段会慢滑过去，用户看到“不够丝滑”。 |
| 本次源码修改 | `src/app_entry.c`：`LCDM_SERVO_V3_MIN_BRAKE_BAND 300 -> 160`。保留速度项 `abs_velocity_q4 * Bg / 3`，所以高速接近目标仍会提前刹车；低速时则更晚进入刹车，继续驱动到更靠近目标。 |
| 下一步 | 构建下载内部自测版，抓两轮数据。目标是减少尾段 `event=14` 过早介入，HOLD 时间下降，同时过冲不能明显回升。 |

2026-06-19 Bmin160 内部自测达标记录：

| 项目 | 内容 |
|---|---|
| 构建/下载 | 内部自测版构建通过，`text=19640 B`、`data=672 B`、`bss=2392 B`；HEX `78ca84e84370942c5d0a9926867cefe4bb29c987506d3db3feade843c3b892a1`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 数据文件 | `build/servo_curve_live/live_internal_bmin160_a_10s.csv`、`build/servo_curve_live/live_internal_bmin160_b_10s.csv`。 |
| 对比基线 | 对比 `before_retest`：1000 HOLD 平均 `374.7ms -> 348.5ms`；1500 HOLD 平均 `429.8ms -> 349.3ms`；2000 HOLD 平均 `490.2ms -> 373.2ms`。尾段慢滑明显改善。 |
| 过冲/误差 | 1000：6 次，过冲 `0/0 count`，最终误差最大 `11 count`。1500：6 次，过冲平均/最大 `1.8/10 count`，最终误差最大 `9 count`。2000：6 次，过冲平均/最大 `0.2/1 count`，最终误差最大 `2 count`。 |
| 达标判断 | 按当前门限，`bad_count=0`。该版同时改善了丝滑度和尾段到位时间，且没有明显牺牲过冲。 |
| 当前板子状态 | 板子保留内部阶跃自测版，便于用户目视确认。若确认动作手感可以，再重新构建并下载普通外部 PWM 版本。 |

2026-06-19 尾段轻抖修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还有二个问题，一是到位有小小的慢过去，二是到位会来回轻抖” |
| 数据/代码判断 | 当前 Bmin160 版虽然过冲数据达标，但尾段仍存在 `event=14` 小修正。代码中运动刹车分支在低速且误差超过 `Rel=14` 时，会把反拉 duty 下限强制抬到 P70 `Lmin=224`。因此 `20..30 count` 小误差也可能被 224 duty 反打，容易造成到位轻抖。 |
| 本次源码修改 | `src/app_entry.c`：运动刹车尾段的 `lock_min_duty` 下限不再对所有 `abs_error > Rel` 生效，而是改为 `abs_error > static_quiet_band(Rel+Hbd)` 才启用。也就是小误差尾段不再用锁止力打，较大偏差仍保留强拉能力。 |
| 下一步 | 构建下载内部自测版，抓两轮数据并目视观察。重点看尾段是否少了轻微来回抖，同时不能重新出现明显过冲。 |

2026-06-19 尾段轻抖多方案测试结论：

| 项目 | 内容 |
|---|---|
| 尝试 1 | 小误差尾段取消 `Lmin=224` 下限，只在 `abs_error > Rel+Hbd` 时启用。结果变差：1000 方向 HOLD 超过 `555ms`，`bad_count` 增多，已回退。 |
| 尝试 2 | 小误差尾段下限改 `140`。结果会卡在 `17..23 count` 附近反复 `event=14 duty=140`，无法稳定进 HOLD，已放弃。 |
| 尝试 3 | 小误差尾段下限改 `180`。结果比 140 稳，但仍有 1000 方向 HOLD `636ms` 的坏段，已放弃。 |
| 当前下载版本 | 恢复原 Bmin160 尾段力 `Lmin=224`，但将到位确认从 `BRAKE_CONFIRM=8/SETTLE=5` 缩短为 `2/2`。内部自测 HEX `3981bf656206984a5907f8edcee1dfae4994fbd09b583f3654edc5775949a6a6` 已下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 当前数据 | `build/servo_curve_live/live_internal_origfloor_settle2_a_10s.csv`、`live_internal_origfloor_settle2_b_10s.csv`：1000 方向 6 次，过冲 `0`，HOLD 平均/最大 `483.8/537ms`，最终误差最大 `12`；1500 方向 6 次，过冲 `0`，HOLD 平均/最大 `290.7/481ms`，最终误差最大 `11`；2000 方向 5 次，过冲 `0`，HOLD 平均/最大 `352.4/474ms`，最终误差最大 `7`；`bad_count=0`。 |
| 判断 | 当前版消除了近目标 `tail_big` 大反打，但 1000 方向比 Bmin160 慢。是否保留要看目视效果：如果 1000 慢明显，应回到 Bmin160 基线并改成方向/区间独立尾段处理；如果轻抖明显改善且 1000 可接受，则继续在此版上微调。 |
| 当前板子状态 | 板子保留内部自测版，便于用户直接观察。 |

2026-06-19 尾段嗞嗞声继续修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “比较好了，但最后一小小节还是静态转动过去的，而并不像正真的PID算法预算精准到位，听那个嗞嗞声就知道，转动和刹车不够干脆的那种，请继续修善” |
| 先回退项 | 上一轮试验的低端目标 `/12` 预反刹数据已证明变差，本次先恢复全局 `reverse_abs = abs_velocity_q4 + abs_error / 8`。 |
| 作废试验 | 试过把 BRAKE 尾段 `15..120 count` 小误差改成较弱同向收口，结果 `error=15..17` 会长期卡住，HOLD 时间出现 `9999ms` 未完成段，数据文件 `live_internal_finishsmall_a_10s.csv`、`live_internal_finishsmall_b_10s.csv`，该方案作废并已回退。 |
| 本次保留修改 | `src/app_entry.c`：保持 Bmin160、原尾段力 `Lmin=224`、预反刹 `/8`，只把运动到位速度门限 `LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4 20 -> 28`。目的不是削弱尾段力，而是在误差已进入释放带附近且速度已经很低时更快关 H 桥，减少最后几拍嗞嗞。 |
| 构建/下载 | 内部阶跃自测版构建通过：`text=19652 B`、`data=672 B`、`bss=2392 B`；HEX `e6961e65eeaebd17dc7fbf8d59d72b14c6e10f7d7ddfe9e9969fb68632f8e7c1`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 数据文件 | `build/servo_curve_live/live_internal_hvel28_a_10s.csv`、`build/servo_curve_live/live_internal_hvel28_b_10s.csv`。 |
| 数据结论 | 排除采样截断的未完成尾段后，共 16 个完整内部阶跃段，`bad_count=0`。`1000us`：6 次，过冲 `0/0 count`，HOLD 平均/最大 `414.8/512ms`，最终误差最大 `12`；`1500us`：6 次，过冲平均/最大 `1.8/5 count`，HOLD 平均/最大 `326.3/331ms`，最终误差最大 `4`；`2000us`：4 次，过冲平均/最大 `1.5/3 count`，HOLD 平均/最大 `285.2/328ms`，最终误差最大 `3`。 |
| 当前板子状态 | 板子保留 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 内部阶跃自测版，便于直接观察尾段嗞嗞和到位动作。若要接外部 PWM 测试，需要重新构建普通版并下载。 |

2026-06-19 六行程对比与 Rel=16 记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “从2000-1500停的相对较好，是什么原因？其它都还是有点最后一小节慢过去的现象” / “不好意思，我说错了，是1000-1500点较好。” |
| 自测序列修改 | 原内部自测只有 `1500 -> 2000 -> 1000` 循环，无法单独比较 `1000->1500`。本次改为六行程循环：`1000 -> 1500 -> 2000 -> 1500 -> 1000 -> 2000 -> 1000`，覆盖短行程、长行程、上行和下行。 |
| 原因判断 | `1000->1500` 好的主要原因不是单独方向算法，而是它属于低端到中点短行程，最大速度较低，尾段通常只需一次 `event=14` 修正，无 `event=3` 空滑；长行程如 `1000->2000`、`2000->1000` 最大速度更高，进入刹车后还会残留较大误差，所以会多一两次尾段修正，听起来就是最后一小节慢过去。 |
| 本次参数修改 | `src/app_entry.c`：`LCDM_SERVO_HOLD_RELEASE_BAND 14 -> 16`。作用是让误差已经很小、速度已低时更快进入 HOLD，减少尾段继续打 H 桥的次数；不是改变前段速度和预刹公式。 |
| 构建/下载 | 内部六行程自测版构建通过：`text=19676 B`、`data=672 B`、`bss=2392 B`；HEX `2d66f97c600b4fcc7edd68ec6bfd820c715c69bf98c96c93e1f306549713c58a`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 数据文件 | `build/servo_curve_live/live_internal_6step_hvel28_a_16s.csv`、`live_internal_6step_hvel28_b_16s.csv` 为 `Rel=14` 对比；`live_internal_6step_rel16_a_16s.csv`、`live_internal_6step_rel16_b_16s.csv` 为当前 `Rel=16`。 |
| Rel=14 对比数据 | 26 个完整段，按 `over>20/end>14/hold>550` 有 3 个 bad。`1000->1500`：过冲平均/最大 `5.8/16`，HOLD 平均/最大 `341.8/398ms`；`1000->2000`：`11.8/36`，`411/494ms`；`2000->1500`：`17.6/35`，`385/476ms`。 |
| Rel=16 当前数据 | 26 个完整段，按 `over>20/end>16/hold>550` 为 `bad_count=0`。`1000->1500`：过冲 `0/0`，HOLD `282/335ms`；`1000->2000`：过冲 `4.8/11`，HOLD `407.5/499ms`；`1500->1000`：过冲 `0/0`，HOLD `283.5/335ms`；`1500->2000`：过冲 `3/8`，HOLD `359.2/478ms`；`2000->1000`：过冲 `0/0`，HOLD `398.2/478ms`；`2000->1500`：过冲 `0/0`，HOLD `301/371ms`。 |
| 当前板子状态 | 板上保留 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 六行程 `Rel=16/Hvel=28` 自测版，便于用户目视比较各行程尾段。 |

2026-06-19 Bg=2 试验作废与恢复记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “是的，明显更差” |
| 试验目的 | 为解决“最后一小小节慢摆过去”，尝试把速度相关刹车窗口从 `Bg=3` 降到 `Bg=2`，让高速接近目标时不要太早进入刹车。 |
| 数据结论 | `Bg=2` 方向错误。`build/servo_curve_live/live_internal_6step_rel16_bg2_a_16s.csv` 显示 13 个完整段中 8 个 bad，最大过冲达到 `93 count`；各行程过冲明显放大，用户目视也确认明显更差。 |
| 处理动作 | 立即回退 `LCDM_SERVO_V3_BRAKE_VEL_GAIN 2 -> 3`，并确认 `Rel=16`、`Hvel=28` 保持稳定基线。 |
| 恢复下载 | 内部六行程自测稳定版重新构建并下载成功：`text=19676 B`、`data=672 B`、`bss=2392 B`；HEX `2d66f97c600b4fcc7edd68ec6bfd820c715c69bf98c96c93e1f306549713c58a`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 当前板子状态 | 板上已恢复为 `Rel=16/Bg=3/Hvel=28` 六行程内部自测版，不再是明显变差的 `Bg=2` 版。 |

2026-06-19 负载/换马达达标复核记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “现在的做法和方向，能符合将来随便换马达或减速齿轮比的要求吗？因为我用力顶住舵版，它并不能完成到达目标点，一直在嗞嗞声中，说明这个马达力度配速等均是不达标，或方法不对的。因为真正的产品是要用尽最大的力气把位置转到位，不然在使用时就失去意意了。何况将来要进行吊重测试，塑胶齿在5.8KG，转动自如到位，金属齿在8.2KG，转动自如到位，不存在嗞嗞声转不到或转不动的现象，请再复核这个程序逻辑是否达标。” |
| 达标结论 | 当前 `Rel=16/Bg=3/Hvel=28` 六行程版本只说明空载/轻载过冲和尾段稳定性接近可用，不能证明产品级负载达标，也不能保证随便换马达或齿轮比后只微调即可。 |
| 主要原因 1 | 当前控制逻辑偏向空载运动曲线：速度环、预刹车、尾段 HOLD 都在抑制过冲；缺少“目标未到、速度很低、负载顶住时必须持续增大 duty 推向目标”的独立负载/堵转到位分支。 |
| 主要原因 2 | `src/hbridge.c` 中主动 Brake 仍然被安全关闭；`HBridge_Brake()` 当前等同 `HBridge_Coast()`，PWM off 段也是滑行。也就是说现在的 BRAKE/HOLD 不是市售舵机常见的主动短刹/同步衰减锁止，因此仅靠软件 duty 曲线很难达到“顶住也必须到位”的锁力表现。 |
| 主要原因 3 | 目前 LCDM/P50 调试参数仍是运行时 RAM 参数，很多核心曲线没有进入最终可保存的 `ServoParams` Flash 参数表；将来换马达、换减速比，需要一组明确的可调参数：最大 duty、静态锁止最大 duty、负载推力阈值、速度观测增益、预刹车距离、反刹上限、HOLD 释放带、堵转保护时间/电流近似门限。 |
| 本次源码处理 | 未下载新算法。先清理 H 桥中已经失效的 off-brake 接口：删除 `HBridge_SetPwmOffBrake()` API 和调用；`HBridge_Brake()` 明确改为调用 `HBridge_Coast()`，保持当前安全滑行行为，避免误以为已有主动刹车。曾尝试加入紧凑负载推力分支，但调试版 Flash 余量不足，未保留，避免仓库处于不可编译状态。 |
| 构建记录 | 本地 Ubuntu 构建通过，未下载：`text=19612 B`、`data=672 B`、`bss=2392 B`；HEX `7491f5737b8d33b5ebb886ca5e615df17cd40c3b506b5116b1fa0b3929373b97`；ELF `d8f00e5443b03484f83da43ab03e8e09bc31e14061ecf91dce0a64dbf993baa8`；BIN `88f2f4eb23c14f84afdeced4dbdacccd0ad6710dc8cb1265bb2c4e5c134fd412`。 |
| 下一步判断 | 要达到 5.8kg/8.2kg 吊重转动自如，需要把下一阶段重点从“继续微调空载过冲”切到“负载闭环”。第一步先用 SWD 曲线加入/确认 H桥 duty、反馈速度、误差、event 在外力顶住时的记录；第二步压缩或拆分 LCDM 调试代码释放 Flash；第三步实现负载推力分支：命令目标未变且误差持续存在、ADC 速度接近 0 时，duty 必须推到 `Lmax/Max`，直到接近目标或保护条件触发；第四步单独安全验证 H 桥主动刹车/同步衰减真值表，再决定是否恢复主动 Brake。 |

2026-06-19 V7 架构跳版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “直接跳到V7吧，不要浪费时间” / “不是改速度就完事了，方向又感觉偏了，应该改善刹车方法和最点斜率问题，现在这个看起来都没力” |
| V7 原则 | 不再沿用 V6 的局部试参。V7 把“正常运动同向驱动、速度/距离预刹、过点反拉、静止外力锁止、负载推力”拆开。原则是：目标未到且负载拖慢时必须加同向力；只有速度相对剩余距离过高或已经过目标时才允许刹车/反拉；小残差不能触发强锁。 |
| V7 初版数据 | `live_internal_v7_a_16s.csv` 显示过激：`event18=43`、`event16=35`，duty 达 `-1000..1000`，典型为 `event16` 负载推力在无负载运动中提前触发，随后 `event18` 全幅反拉，形成 ping-pong。 |
| V7.1 修改 | `LOAD_PUSH` 加 30 次确认，避免无负载运动刚变慢就进入负载推力。下载成功，pyOCD 返回 `Successfully resumed device`。数据 `live_internal_v7_1_a_16s.csv`：`event16` 已被压住，但 `event18` 仍有 26 次，小误差区继续反拉，仍不合格。 |
| V7.2/V7.3 结论 | V7.2 把静止残差误当外力，`error 30..40 count` 就按 `Lmin+8E` 打到 `400..500 duty`，更差；V7.3 抑制了小误差强锁，但正常 DRIVE 仍因位置项过猛出现大过冲和反拉。两版均作为失败数据保留，不作为基线。 |
| V7.4 结论 | 正常运动改为速度环为主、位置误差有限补偿；预刹改为“速度是否超过剩余距离允许值”。数据 `live_internal_v7_4_a_16s.csv`：`event18` 从 19-32 次降到 2 次，`event16=0`，duty 从 `±1000` 降到 `-640..775`，方向正确，但动作偏软，部分长行程在内部周期内未完全收完。 |
| V7.5 系列 | 根据用户反馈“不只是速度、要改刹车方法和终点斜率”，加入正常运动同向推力地板，并调终点释放带。V7.5a 无 `event16/18`，但释放带 64 count 太宽；V7.5b 收到 32 count 后 `event18` 回来；V7.5c 释放 48 count 且预刹提前，`event18` 归零但误差仍常在 30 多；V7.5d 折中到 40 count。 |
| 当前下载版本 | V7.5d 内部阶跃版已下载成功，pyOCD 返回 `Successfully resumed device`；构建 `text=19640 B`、`data=672 B`、`bss=2392 B`。 |
| 当前数据 | `build/servo_curve_live/live_internal_v7_5d_16s.csv`：事件 `event2=66`、`event12=17`、`event14=5`、`event18=2`、`event16=0`；duty 范围 `-582..753`；多数段最终误差 `±15..29 count`，HOLD 约 `305..520ms`。高端 2000 仍有几段过冲 `38..40 count`，其中一个长行程 HOLD `624ms`，说明下一步应专门改“高端/长行程停车距离估算和终点斜率”，不是再单纯加速度。 |
| 当前板子状态 | 板上是 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 的 V7.5d 内部阶跃自测版，不是普通外部 PWM 版。若要用户接 PWM 测试，必须重新构建去掉 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 后下载。 |

2026-06-20 V7.6 刹车斜率继续修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “不是改速度就完事了，方向又感觉偏了，应该改善刹车方法和最点斜率问题，现在这个看起来都没力” / “继续” |
| V7.6 修改 | 将预刹触发从“必须高速朝目标移动”改成“只要朝目标移动，且速度相对剩余距离过高就预刹”。也就是用动态停车距离判断，而不是只看 `fast_toward_target`。同时删除无用的 `fast_toward_target` 变量，避免构建警告。 |
| V7.6 数据 | `live_internal_v7_6_16s.csv`：`event18=0`、`event16=0`，高端 2000 过冲由 V7.5d 的 `38..40 count` 明显下降到多为 `7..19 count`；但低端 1000 出现 `26..36 count` 停点偏差，原因是过目标后进入 HOLD 太早。 |
| V7.6b 试验 | 尝试收窄过目标 HOLD 窗口。结果 `event18` 回来，且有一段 `e=-36` 时连续 `duty=-108` 拉不动，形成典型嗞嗞/慢找点，作废。 |
| V7.6c 试验 | 在 V7.6b 基础上给小误差反拉加入最低 `180 duty`。结果出现不干净混段和低端仍有 `40 count` 偏差，且不如 V7.6d 方向清晰，作废。 |
| V7.6d 保留修改 | 不再靠过点后反拉修正，改为在预刹分支中增加末段刹车提升：当 `abs_error <= Bmin` 且速度仍明显高于 HOLD 速度时，`reverse_abs += abs_velocity/2 + FF/2`。这属于刹车斜率修正，不是单纯加快马达。 |
| V7.6d 数据 | `live_internal_v7_6d_16s.csv`：`event18=0`、`event16=0`；最大过冲 `26 count`，最大最终误差 `26 count`，平均最终误差 `9.8 count`；duty 范围 `-389..480`，比 V7.6 的 `-720..720` 更收敛。相对 V7.6：最大过冲 `36 -> 26`，平均最终误差 `17.2 -> 9.8`。 |
| V7.6e 试验 | 试把默认 `Slew 80 -> 100`，希望提高刹车建立速度。结果出现异常混段，最大过冲回到 `30 count`，且平均最终误差被异常点拉大，作废并回退。 |
| 当前源码状态 | 保留 V7.6d：动态预刹 + 末段刹车提升；`Slew` 已恢复 `80`；H 桥主动刹车仍未启用，`HBridge_Brake()` 仍为安全滑行。 |
| 当前下载版本 | 内部阶跃自测版已重新构建并下载成功，构建 `text=19664 B`、`data=672 B`、`bss=2392 B`，pyOCD 返回 `Successfully resumed device`。 |
| 当前板子状态 | 板上是 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 的 V7.6d 内部阶跃自测版，不是普通外部 PWM 版。下一步应在 V7.6d 上继续看实物声音和末段动作，再决定是否只做小幅刹车建立速度参数化，而不是回到过点后强反拉。 |

2026-06-20 V7.7 末段慢走修正试验记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “还要继续改加速度和刹车，最后一小点点不是慢走过终点的。” |
| 问题判断 | V7.6d 的数据仍可看到部分小误差窗口内的同向驱动，例如 `error 70/74 count` 时仍给 `duty 432..455`，用户听感/目视会像最后一点点带电慢走。试验目标是减少末端带电同向驱动，但不能牺牲停点误差和过冲。 |
| V7.7a 试验 | 试图在 `event14` 预刹阶段单独提高输出斜率，避免全局 `Slew` 变大。该做法新增代码后 Flash 溢出 `48 bytes`，未下载。随后改成不增代码的刹车补偿加强：末段补偿从 `velocity/2 + FF/2` 改为 `velocity + FF/2`。数据 `live_internal_v7_7a_16s.csv` 变差：最大过冲 `33 count`、平均最终误差 `15.4 count`，不如 V7.6d。已回退。 |
| V7.7b 试验 | 将低速预刹触发门槛从 `Hvel+16` 收到 `Hvel`，目标是避免低速末端重新进入普通同向驱动。数据 `live_internal_v7_7b_16s.csv` 变差：最大过冲 `37 count`，并出现 `event18=1`。已回退。 |
| V7.7c 试验 | 尝试小窗口提前断输出/靠惯性收尾：当接近目标且仍朝目标移动时提前 HOLD。数据 `live_internal_v7_7c_16s.csv` 证明方向不对：虽然末端带电同向驱动减少，但最大最终误差扩大到 `52 count`，说明当前 `HBridge_Brake()` 实际为 Coast 时，惯性收尾不可控。已回退。 |
| 当前结论 | 三个 V7.7 方向都没有超过 V7.6d。当前最稳仍是 V7.6d：最大过冲 `26 count`、平均最终误差 `9.8 count`、`event18=0`、`event16=0`。若继续解决“最后一点慢走”，不能再靠简单调预刹阈值/补偿/提前断电，下一步应进入结构性改法：压缩调试代码释放 Flash 后实现真正的运动轨迹/刹车状态机，或者安全验证 H 桥主动刹车/同步衰减后再做末段主动刹停。 |
| 当前板子状态 | 已重新构建并下载回 V7.6d 内部阶跃自测版：`text=19664 B`、`data=672 B`、`bss=2392 B`，pyOCD 返回 `Successfully resumed device`。 |

2026-06-20 V7.8/V7.9 速度斜率与末段反刹修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “那是因为你的斜率没做好的原因，也许可能还未到目标之前就开始小的反刹，整个PID设计瞎了眼在问题” / “比如，从1500到2000，有可能1000开始是满PWM全速，到1300开始减小速和开始加刹车，斜线一直这样比例下去，直到2000刹停，这样就精准了” |
| V7.8 试验 | 先按 `Bmax` 全区线性限速：`Bmax` 外全速，进入 `Bmax` 后目标速度按剩余误差下降，并把未到目标前反刹条件改为“实际速度超过允许速度 + 高速余量”。该版构建下载成功，数据文件 `live_internal_v7_8_16s.csv`。 |
| V7.8 失败结论 | `Bmax=620` 处开始拉低速度太早，斜率太浅，速度环为了追上/修正反而打出 `±1000 duty`；`event18=3`、最大最终误差 `37 count`、平均最终误差 `21.1 count`，明显差于 V7.6d，作废。 |
| V7.9 修改 | 改为动态接近区斜率：`abs_error >= approach_band` 时保持全速；进入 `approach_band` 后才按剩余误差线性降低目标速度；反刹只在实际速度超过该斜率允许速度加 `Bhd` 余量时触发；同向 duty 地板也只在接近区外有效，避免末端继续硬推或小反刹打架。 |
| V7.9 数据 | 数据文件 `live_internal_v7_9_16s.csv`、`live_internal_v7_9_b_16s.csv`。按 `target_adc` 正确分段后，两组均 `event18=0`、`event14=0`、无 bad 段；最大最终误差 `20/19 count`，平均最终误差 `10.5/10.2 count`；平均 HOLD 时间约 `299/291ms`，比 V7.6d 的约 `449ms` 更快进入稳定；末段带电采样从 V7.6d 的 `13` 次降到 `5-6` 次。 |
| 注意事项 | V7.9 个别行程 duty 最高到 `1000`，属于为了避免末段慢爬而让前段/中段更干脆。实物测试时需重点观察电流尖峰、齿轮冲击和是否仍有尾段嗞嗞；如果电流尖峰过大，下一步应限制运动最大 duty 或给速度斜率加独立上限，而不是恢复 V7.8 的远距离浅斜率。 |
| 构建/下载 | 内部阶跃自测版构建通过：`text=19672 B`、`data=672 B`、`bss=2392 B`；HEX SHA256 `273ac59c1755770928042e1fb0b2e52003c57c1be057037c9e0e251eda9ac86d`；ELF SHA256 `9fae0ca7a1cebc06ee8165d8d1bf91405e309592a6e77d28e3afbc32643f9a2a`；BIN SHA256 `6699e7d9ed87e5b79b4df2273cd2eb8cf6d222326daa5efc77e29f461c22fc56`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 当前板子状态 | 板上保留 V7.9 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 内部阶跃自测版，用于用户直接观察加速、刹车和最后一小段是否还慢爬。 |

2026-06-20 V7.10 提速和外部 PWM 临时屏蔽记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请继续V7.9,又要提速，又要精准刹车到点。” / “同时可以保留记录一下V7.8，虽然速度不够，但效果相对好一些。” / “不要因为空间不够来压缩程序的发挥。必要时把不用的程序先屏蔽掉，大胆改写程序。” / “比如，现在可以先屏蔽外部PWM输入部分。” |
| V7.8 保留结论 | V7.8 不作为当前下载版，但保留为参考：它是“较保守刹车感”的试验，速度和误差不达标，数据 `live_internal_v7_8_16s.csv` 为 `event18=3`、最大最终误差 `37 count`、平均最终误差 `21.1 count`。后续若需要找“刹车感偏稳但偏慢”的方向，可回看该数据。 |
| V7.10 修改 | 在 V7.9 的动态接近区斜率基础上，提高远离刹车区的运动 duty 地板：`floor = Lmin + FF + (abs_error - approach_band) / 2`，并被 `Max` 限幅。进入接近区后仍由 V7.9 的速度斜率接管，不恢复 V7.8 的远距离浅斜率。 |
| 外部 PWM 屏蔽 | 在 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 调机固件中，`pwm_input.c` 编译为桩函数；`PWM_Input_Init()` 不配置 PA4 外部中断，`EXTI4_15_IRQHandler()` 不调用 PWM 捕获；控制循环和 P50 使用内部 1000/1500/2000us 阶跃，不再受 PA4 输入影响。 |
| V7.10 数据 | 数据文件 `build/servo_curve_live/live_internal_v7_10_16s.csv`。按 `target_adc` 分段：`event18=0`、`event14=0`、无 bad 段；最大最终误差 `19 count`、平均最终误差 `7.2 count`；平均 HOLD 时间 `281.5ms`，比 V7.9 的约 `291-299ms` 略快；末段带电采样 `5` 次；最大 duty `960`，`>=900` 共 `3` 个采样。 |
| 构建/下载 | 内部阶跃自测版构建通过：`text=18952 B`、`data=664 B`、`bss=2336 B`，外部 PWM 屏蔽后释放约 700B Flash；HEX SHA256 `d1d0e88f95d2ed26f65b8141a5263498371f9c96390be1c9431d69e4869733d7`；ELF SHA256 `ba97e8a3f158d913c2d238d7ab942d69a3aff21eef1f7a8223c9bfb1a0878005`；BIN SHA256 `85d3416e556d7a260df239bdb0a449f7793593486e856355baf8bf5081348725`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 当前板子状态 | 板上保留 V7.10 内部阶跃自测版。该版暂时不读外部 PWM，只用于观察内部阶跃下的速度、刹车、终点精度和声音。 |

2026-06-20 V7.10 外部 PWM 恢复下载记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “目前转动不算可以，现在可以用V7.10版本来打开外部PWM再试了” |
| 本次构建 | 保留 V7.10 控制算法和提速 duty 地板；去掉 `SERVO_ENABLE_INTERNAL_STEP_TEST=1`，恢复 PA4 外部 PWM 输入捕获、滤波和 P50 PWM 显示。 |
| 构建/下载 | 普通外部 PWM 版构建通过：`text=19556 B`、`data=672 B`、`bss=2376 B`；HEX SHA256 `a6c03f7fcac9c8353797fcb349317c6162de1ca04d4834fc4f23432c30e8dd18`；ELF SHA256 `065c4245d1013e0bae1e30317f706994cc9f9d4e02f8f2cafcdb872a1b9d57af`；BIN SHA256 `5aaa62a9d2aa0c74a77e99e20cebfe8ec9f56d5bffe4ff810cc4757eba29983f`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 下载后短抓 | `build/servo_curve_live/live_external_v7_10_postflash_4s.csv` 显示外部 PWM 为 `1503us`，目标 `target_adc=2052`，全程 `HOLD/event=2`，未出现内部 1000/1500/2000 自动阶跃。 |
| 当前板子状态 | 板上为 V7.10 普通外部 PWM 版，可用外部 PWM 信号源测试 1000/1500/2000us 和连续变化。 |

2026-06-20 V7.10 外部 PWM 强制恢复检查记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “没有切到外部信号PWM哦” / “你这有点乱来，没有屏蔽之前是好好的，为什么取消屏蔽就没有外部PWM功能了呢” / “把内部自己调试跃式的程序部分屏蔽掉先，然后再检查” |
| 本次判断 | 上一版普通构建虽然没有传 `SERVO_ENABLE_INTERNAL_STEP_TEST=1`，但内部阶跃调试代码仍保留在构建开关里，容易被命令行误开。为避免后续再把内部阶跃版误当外部 PWM 版下载，本次先从 Makefile 层硬屏蔽内部阶跃入口。 |
| 本次修改 | 新增 `SERVO_ALLOW_INTERNAL_STEP_TEST ?= 0`。以后即使构建命令误传 `SERVO_ENABLE_INTERNAL_STEP_TEST=1`，也不会定义内部阶跃宏；只有同时显式传 `SERVO_ALLOW_INTERNAL_STEP_TEST=1 SERVO_ENABLE_INTERNAL_STEP_TEST=1` 才允许编译内部 1000/1500/2000us 阶跃调试版。 |
| 屏幕标记 | P50 右上角新增输入源标记：普通外部 PWM 版显示 `EXTPWM`；只有显式允许并打开内部阶跃调试时才会显示 `STEP`。 |
| 构建/下载 | 普通外部 PWM 版重新构建通过：`text=19556 B`、`data=672 B`、`bss=2376 B`；ELF 校验无 `lcdm_internal_step_test_sample` 符号，字符串可见 `EXTPWM`；HEX SHA256 `78940be6336ff7963f94260360ccd1cb6a673609f5a8b46e6477c7645efad3b8`；ELF SHA256 `c75d989445af793c4d7524fdea60f3ce4b95e03eafa3b30802ab6ac05ced4487`；BIN SHA256 `ba637f849be731bd4172d713d31a96e31807698a3c3febbabbb25c6baf0ca47b`；macOS DAPLink、SWD `50kHz` 下载成功，pyOCD 返回 `Successfully resumed device`。 |
| 当前板子状态 | 板上为 V7.10 普通外部 PWM 版。上电后 P50 右上角应显示 `EXTPWM`，可继续用外部 PWM 信号源测试 1000/1500/2000us。 |

2026-06-20 V7.11 终点斜率修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “duty到1000意是满载马达PWM吗？” / “继续” |
| 说明 | `HBRIDGE_DUTY_MAX=1000`，因此控制输出 `duty=+1000/-1000` 表示 H 桥两个方向的马达 PWM 100% 占空比；它不是输入舵机 PWM 的 `1000us`。 |
| 本次判断 | V7.10 的内部数据中，末端可见小反向刹车 duty，例如接近目标时出现 `error` 约几十 count、`duty` 约 `190` 的短输出。该输出不是同向慢推，但因为当前 `HBridge_Brake()` 仍为安全滑行，听感上仍可能像最后一小段带电找点。 |
| 本次修改 | `src/app_entry.c`：增加 V7.11 末端斜率处理。接近区仍按剩余误差线性降低允许速度；当进入 `Rel + Hbd + Bmin/8` 的终点小窗口时，刹车触发余量从 `Bhd` 降到 `Hvel`，并禁止速度环继续给同向小 duty 推向目标。目标是让最后小段更快进入 HOLD 或明确刹车，减少零散小 duty 慢爬。 |
| 未改内容 | 不改 PWM 捕获、不改 LCDM 菜单、不打开内部阶跃；普通版仍显示 `EXTPWM`。H 桥主动短刹仍未启用，`HBridge_Brake()` 继续保持安全滑行。 |
| 构建/下载 | 普通外部 PWM 版构建通过：`text=19656 B`、`data=672 B`、`bss=2376 B`；HEX SHA256 `15424b36f3c7704c4c97a2b61f74b473ecdec2660318f77f3d554940270a76fa`；ELF SHA256 `966a331b1c92dd2a074a73c74a98a36aaa1a8215ef398135b7d2307856801b8b`；BIN SHA256 `d258e18e2a3d6ef46b57a9bcb00fa15379d275134bec477ff465e75981ba3f28`。macOS DAPLink、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004939`，pyOCD 返回 `Successfully resumed device`。 |
| 下载后短抓 | `build/servo_curve_live/live_external_v7_11_postflash_4s.csv` 共 `393` 行数据；外部 PWM 只看到 `1004us/1502us`，没有内部阶跃；全程 `event=2/HOLD`、`duty=0`，确认下载后没有自动乱动。 |
| 当前板子状态 | 板上为 V7.11 普通外部 PWM 版。下一步实测重点：1000/1500/2000 切换时最后小段是否还慢爬，是否出现新增过冲或电流尖峰。 |

2026-06-20 外部 PWM 接收修正记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “没有把外部PWM接收方式改过来，检测不到” |
| 根因 | `pwm_input.c` 的 ISR 接收层仍用 `ServoParams` 默认 `pulse_lower/high=900/2100us` 去过滤控制用脉宽，和 LCDM 调机页 `P20 InL/InH=500/2500us` 不一致；同时大步变化虽然绕过平均滤波，但仍要等 `accept_pulse_width()` 的 4 帧确认，导致外部 PWM 变化响应慢或被误认为没接收。 |
| 本次修改 | 1. 控制捕获周期范围从 `3000-20500us` 放宽为 `3000-30000us`，周期只作为合法性检查，重点取高电平宽度。2. ISR 接收层不再用 `ServoParams` 的 `900-2100us` 过滤宽度，合法捕获范围按 `300-2700us`。3. 大于 `12us` 的外部 PWM 大步变化立即锁存，并重置输入平均滤波，小变化继续走滤波/确认。 |
| 构建/下载 | 普通外部 PWM 版构建通过：`text=19616 B`、`data=672 B`、`bss=2376 B`；HEX SHA256 `a68830e1a4b289bdace8bc1894e0cacaef6385e7713c19fa1f9af6e08c67bf15`；ELF SHA256 `b51250017ccb33a0a183ac55026c1f0fa733fe42c3ed776fb43e3aa894f6b711`；BIN SHA256 `a5ff2b26de2f078e22d39324e5203cdc42e1cbe822ec7784193a924a4a1c0e6f`。macOS DAPLink、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004911`，pyOCD 返回 `Successfully resumed device`。 |
| 下载后短抓 | `build/servo_curve_live/live_external_pwm_accept_fix_postflash_4s.csv` 共 `717` 行有效数据；外部 PWM 已读到 `1003/1004/1502/1503/2002us`，原始宽度含 `2003us`，周期约 `19945-20020us`；事件包含 `HOLD/event=2`、`DRIVE/event=12`、`DECEL/event=13`，duty 范围 `-80..80`。确认外部 PWM 接收已恢复，且不是内部阶跃。 |
| 当前板子状态 | 板上为 V7.11 控制算法 + 外部 PWM 接收修正版，P50 应显示 `EXTPWM` 并能看到外部 `1000/1500/2000us` 变化。 |

2026-06-20 V7.10b 外部 PWM 重来版记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “请回复到这个位置重来，V7.10版本，‘目前转动不算可以，现在可以用V7.10版本来打开外部PWM再试了’，在这个基础上，打开外部PWM信号输入检测功能，同时关联外部PWM信号并进行联动测试舵机。把内部测试程序关闭，不要再跟进SETP test里面了。且回恢LCDM显示界面并关联，其它任何PID参数暂时不要动！” / “版本先变V7.10b吧” |
| 本次处理 | 回到 V7.10 控制基线，不继续 V7.11 终点斜率方向；仅保留外部 PWM 接收修正和 Makefile 内部 STEP 硬屏蔽。P50 标题显示 `P50 V7.10b`，右上角显示 `EXTPWM`。 |
| 未改内容 | 不改 V7.10 PID/刹车/速度/锁止参数；不打开内部 `1000/1500/2000us` STEP test；不进入 `SETP/STEP` 调试流程。 |
| 构建校验 | 普通外部 PWM 版构建通过：`text=19528 B`、`data=672 B`、`bss=2376 B`；固件字符串可见 `EXTPWM`、`P50 V7.10b`；ELF 符号表无 `lcdm_internal_step_test_sample`。HEX SHA256 `553bc8cc8975f8966174ca3010e2032a9b2f13261f49f908ecb13adeb2f082bb`；ELF SHA256 `fb2e8dab3a3dae04d63c8a5ee6b084292178b800f56831ee4851129683b64d94`；BIN SHA256 `f990713cd323e15539a0682a7aebfd563292afa07da9d939170561c060924c57`。 |
| 下载结果 | macOS DAPLink、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048ad`，pyOCD 返回 `Successfully resumed device`。 |
| 下载后短抓 | 数据文件 `build/servo_curve_live/v7_10b_postflash/servo_curve_live.csv` 共 `679` 行；外部 PWM 捕获到 `1503/1504us`，并记录到一次 `2503us` 跳变；`target_adc` 随 PWM 从约 `2052/2055` 联动到 `3785`，确认 PA4 外部 PWM 到控制目标链路已打开，不是内部 STEP。 |
| 当前板子状态 | 板上为 `V7.10b` 普通外部 PWM 版。应读取 PA4 外部 PWM，P50 显示 `P50 V7.10b` 和 `EXTPWM`，舵机目标由外部 PWM 变化联动。 |

2026-06-20 V7.10b 外部 PWM 毛刺抑制与内/外源切换记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “每次一段时约二分钟后就会无法检测外部PWM信号” / “时不时又自己在转，更像是内部和外部PWM在打架一样” |
| 根因判断 | 不是内部 STEP 编进普通版，而是外部 PWM 捕获层之前对大跳变单帧立即锁存。短抓曾出现 `1503us -> 2503us -> 1504us` 单点假值，足以让目标 ADC 突然跳到高端再回中间，看起来像自己转动。 |
| 本次修改 | `pwm_input.c` 去掉大跳变单帧立即接受，所有外部 PWM 变化统一走原有平均/连续确认，单帧毛刺只进入 `R` 原始显示，不进入控制用 `P`。P50 底部中键从 `BK` 改为 `IN`，按一次在 `EXTPWM/INTPWM` 间切换；默认仍为 `EXTPWM`。P50 标题右上角显示当前源。 |
| 空间处理 | 20K flash 已接近满。LCDM 调试构建用 `SERVO_ENABLE_COMM=0`，并且只在 `SERVO_ENABLE_TJC_LCDM=1` 时编译正式 `tjc_lcdm.c`，避免与 P50 运行时调试界面重复占空间。 |
| 构建校验 | 构建参数包含 `SERVO_ENABLE_COMM=0 SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1`；构建通过：`text=19624 B`、`data=672 B`、`bss=2376 B`。固件字符串可见 `P50 V7.10b`、`EXTPWM`、`INTPWM`；ELF 符号表无 `lcdm_internal_step_test_sample` 和 `Servo_Comm`。HEX SHA256 `f63048f55a0507b4ceca43ff052a62240f16c03e482b7b689cb7cd7a7c725eff`。 |
| 下载结果 | macOS DAPLink、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004905`，pyOCD 返回 `Successfully resumed device`。 |
| 下载后短抓 | `build/servo_curve_live/v7_10b_src_toggle_postflash/servo_curve_live.csv` 共 `748` 行；控制用 `pwm_us` 全程稳定 `1003us`，但原始 `pwm_raw_us` 出现过 `2003us` 单帧毛刺；`target_adc` 全程稳定 `1184`。确认单帧假宽度已被挡在控制链外。 |
| 当前板子状态 | 板上为 `V7.10b` 外部 PWM 默认版。P50 中键 `IN` 可切到内部 `1000/1500/2000us` 循环测试；再按一次回外部 PWM。 |

2026-06-20 V7.10b 外部 PWM 恢复二次修正记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “内部可以转动，但有些晃的明显……现在外部PWM还是动不了” / “外部PWM之前不是一直很好能动的吗？” |
| 根因复核 | 上一版把外部 PWM 所有变化都送入 6 帧平均滤波，再做连续确认。对 1000/1500/2000 这类大步变化，平均滤波会输出一串中间值，导致确认逻辑迟迟吃不到稳定新值，表现为外部 PWM 不动。 |
| 本次修改 | `pwm_input.c` 删除控制链路的 6 帧平均滤波；外部 PWM 控制值改用原始宽度做连续确认。普通大步变化连续 `2` 帧确认后生效，`1-3us` 微小变化仍需 `4` 帧确认；单帧毛刺只会进入 `R` 原始显示，不会进入控制 `P`。 |
| 构建/下载 | 构建通过：`text=19492 B`、`data=672 B`、`bss=2352 B`；HEX SHA256 `020ff95f9be1993e259d91d4b5e5405cd2effde638bf4fab1e4a1c5570036e3a`。macOS DAPLink、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004881`，pyOCD 返回 `Successfully resumed device`。 |
| 下载后短抓 | `build/servo_curve_live/v7_10b_pwm_confirm2_postflash/servo_curve_live.csv` 共 `268` 行；外部源下控制用 `pwm_us` 稳定 `1003us`，原始 `pwm_raw_us` 为 `1002-1004us`，目标 `target_adc=1184`。 |
| 当前板子状态 | 板上为 `V7.10b` 外部 PWM 默认版，P50 中键仍可切 `INTPWM/EXTPWM`。本版重点验证外部 PWM 1000/1500/2000 是否恢复动作，以及是否还会被单帧毛刺带动自己转。 |

2026-06-20 V7.10b PWM W 跳变与内部自检起步晃动修正记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “切换到内部自检时开始的大概10次是一直在晃，外部就不晃。而外部PWM测了也是一样，开始一段时间会晃，外部就稳定下来。P50第一行PWM的最后一个W变化有时是0直接变成1000，之前处理好了只有0，1变化。” |
| 原因判断 | `W=1000` 不是正常 PWM 漂移，而是 PA4 软捕获偶发测到单帧假宽度，例如真实约 `1003us` 时偶发读到 `2003us`。控制用 `P` 已经过连续确认，不应跟着跳；但 P50 的 `W` 仍直接统计原始窗口，所以把这类单帧坏值显示成 `1000us` 抖动。内部自检刚切入时按系统运行时间直接进入 1000/1500/2000 相位，没有先对齐当前舵板/锁存命令，容易前几次大阶跃晃动。 |
| 本次修改 | `pwm_input.c`：P50 的 PWM `W` 只统计贴近控制已确认宽度或候选宽度的原始值，单帧远跳仍可在 `R` 上暴露，但不再把 `1000us` 跨度计入 `W`。`app_entry.c`：运行时切到 `INTPWM` 后先保持当前锁存 PWM 或中位约 `0.8s`，再按 1000/1500/2000 循环，避免切换瞬间随机大跳。 |
| 未改内容 | 没改 V7.10b PID、刹车、速度、锁止参数；没打开编译期 STEP test；默认仍为 `EXTPWM`，P50 中键 `IN` 只做运行时内/外源切换。 |
| 构建/下载 | LCDM 调试配置构建通过：`text=19612 B`、`data=672 B`、`bss=2360 B`；HEX SHA256 `fbc218cd3fa8ae34ce38bf16cc675ea9d8321061f92c636127651113b0e23f60`；ELF SHA256 `41c960d84240376eaa0f11e7e1e105902ef8231a799155f78f0e64722c6ce5bf`；BIN SHA256 `9cbb97282945a49f61912f1d8f925db855666c0c38aa26c0d9f43a045da80d0d`。macOS DAPLink、SWD `2MHz`、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048f9`，pyOCD 返回 `Successfully resumed device`。 |
| 测试重点 | 固定外部 PWM 不动时看 P50 第一行：`W` 应回到 `0-几 us`，不应再从 `0` 直接跳 `1000`；若 `R` 偶发闪远值但 `P/W/目标` 不动，说明毛刺已隔离在显示原始层。切到 `INTPWM` 时，前 0.8 秒应先保持当前点，再开始循环，观察前 10 次是否明显减少晃动。 |

2026-06-20 V7.10b 静止外部 PWM 自转根因修正记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “在外部PWM静止一个点的状态下，舵机还是会时不时的自转，是什么原因？” / “刚转了二个，看到没” / “一直在自动转了” |
| 实测抓取 | 使用 macOS pyOCD/SWD 抓取 `build/servo_curve_live/static_external_watch_20260620_151621/servo_curve_live.csv`。自动转动期间保存出多个周期：`cycle_002_1504_to_1504` 到 `cycle_007_1504_to_1504`。虽然文件名首尾都是 `1504us`，但周期内部看到控制用 `P` 被误确认到 `2504/2505us` 或 `504us`，目标 AD 跳到 `3785` 或 `318`，H 桥随即满力追目标，导致舵机自转。 |
| 根因 | PA4 当前仍是 EXTI 双边沿 + `Board_MicrosQ4()` 软捕获，不是硬件定时器输入捕获。LCDM/电机/中断负载下会偶发错配边沿，原始 `R` 出现 `1504 -> 2504` 或 `1504 -> 504` 这类约 `1000us` 假宽度。上一版只要求大步变化连续 `2` 帧确认，挡不住短串假宽度。 |
| 本次修改 | `pwm_input.c` 增加大跳变独立确认：`delta >= 750us` 时必须连续 `8` 帧才更新控制 `P`；普通中等变化仍为 `2` 帧，`1-3us` 微小灵敏度仍为 `4` 帧。`app_entry.c` 修正 SWD 曲线帧，正式写入 `pwm_candidate_us/pwm_candidate_count`，以后能看见假宽度停在候选层还是进入控制层。 |
| 验证结果 | 下载后重新抓取 `build/servo_curve_live/static_external_after_large_filter_20260620_152033/servo_curve_live.csv`，2 分钟共 `21451` 行。控制 `pwm_us` 全程固定 `1503`，`target_adc` 全程固定 `2052`；原始 `pwm_raw_us` 仍可见 `502/503/2502/2503/2504` 假宽度，但 `pwm_candidate_count` 最大只到 `2`，没有进入控制 `P`，没有保存新的自转周期。 |
| 剩余现象 | 抓取中仍有少量 `error=14-15`、`duty=80`、`event=13` 的短促小修正，共 `14` 行，不是目标跳变型自转。为避免削弱锁止力，本次暂不继续改 HOLD/锁止参数。若用户仍能看到小嗞动，下一步只针对静态小误差 HOLD 门限微调。 |
| 构建/下载 | LCDM 调试配置构建通过：`text=19636 B`、`data=672 B`、`bss=2360 B`；HEX SHA256 `30e4037b8d52669e7c8cad113458d082608eb0882de56d6ff28c435ba8ff2d62`；ELF SHA256 `8d1443c65f12a581b5225ed2aeadd637e885a66cc4d730cc67f3020ae0f8167f`；BIN SHA256 `bf66b3e8cf2a09f05584c0e25644885c81e586394dd1bb7c623839714e3441ff`。macOS DAPLink、SWD `2MHz`、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x08004911`。 |
| 后续方向 | 这个版本先验证静止外部 PWM 是否还会大幅自转。长期根治 PA4 假宽度，应把 PWM 输入从 EXTI 软捕获切到硬件 TIM 输入捕获，或在最终 SOP8 IO 规划中优先给 PWM 输入可捕获定时器通道。 |

2026-06-20 V7.10c PA3 硬件捕获换脚记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “PA4长远会是换到哪个有TIM捕获的脚呢，我觉得是不是现在提前改?以防后患” / “好的，现在我马上更改硬件，同步修改程序定义，二分钟后再试。” / “硬件已改好” |
| 根因延续 | 上一轮 SWD 数据已证明 PA4 EXTI 软捕获会偶发把固定约 `1504us` 误算成 `504/2504us`，导致控制目标跳到端点并自转。继续在 PA4 上加滤波只是补救，长期方案应改成硬件 TIM 输入捕获。 |
| 本次硬件定义 | 外部舵机 PWM 输入从 `PA4` 改到 `PA3/TIM1_CH1`；电位器 ADC 反馈从 `PA3/ADC3` 改到 `PA4/ADC4`。 |
| 本次程序修改 | `pwm_input.c` 改为 TIM1 1MHz 自由计数，PA3 硬件捕获 CCR1，上升沿计算周期、下降沿计算高电平宽度；保留原有大跳变确认和 `R/P/W` 诊断逻辑。`board.c/app_it.c` 将 H 桥 tick 从 TIM1 挪到 TIM16，并改为寄存器直接初始化/中断，避免 TIM1 与输入捕获冲突。 |
| 未改内容 | 不改 V7.10b 控制算法、PID/刹车/速度/锁止参数；只把 LCDM 显示标记改成 `V7.10c`，便于确认已下载到新捕获版。 |
| 空间处理 | LCDM 调试版 Flash 很紧。移除当前未使用的旧 LCDM 表/入口，TIM1/TIM16 改寄存器方式，并从构建中移除 HAL TIM/TIMEx 源文件。 |
| 构建/下载 | LCDM 调试配置构建通过：`text=19604 B`、`data=672 B`、`bss=2328 B`；HEX SHA256 `4db5ffa0edfff1a38b72467b063a9a59d391d5827f877623a41e22bacb3ac04f`；ELF SHA256 `ee6eed1ad4691024f7c5fb06e2d9c7f1346abddb093add5a63cce38da9d877b2`；BIN SHA256 `fe0f6ba534e73e6281fd73f388788e175c5c0103ffba7c5331b8e4aefa67233f`。macOS DAPLink、SWD `2MHz`、under-reset 下载成功，启动向量 `SP=0x20000c00`、`PC=0x080048f1`，pyOCD 返回 `Successfully resumed device`。 |
| 当前板子状态 | 板上为 `V7.10c`：PWM 输入请接 `PA3`，电位器反馈请接 `PA4`。下一步先看 P50 第一行 PWM 固定点是否稳定，尤其是否还出现 `504/2504us` 这种假宽度；若 PWM 已稳定，再回到过冲、锁止和嗞嗞声算法调试。 |

2026-06-20 V7.10c PA3 输入无真实 PWM 显示与 Ubuntu DAPLink 记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “你要通过UBUNTU来进行链接DAPLINK” / “只要用示波器探头碰触一下PA3,LCDM的PWM就有数字显示出来，但不是真实的，也不会变” / “请继续试” |
| 当前源码状态 | `BOARD_PWM_IN_PIN=PA3`，`BOARD_POT_ADC_PIN=PA4/ADC4`。为先证明 PA3 信号是否真正到达 MCU，`pwm_input.c` 当前是 PA3 `GPIO_MODE_IT_RISING_FALLING` + TIM1 1MHz 自由计数的 EXTI 诊断抓边沿版，不是上一条记录中的 TIM1_CH1 CCR 硬件捕获版；H 桥 tick 仍在 TIM16。 |
| Ubuntu DAPLink 处理 | DAPLink 已切到 `Ubuntu Linux`，`lsusb` 可见 `0d28:0204 NXP ARM mbed`，`pyocd list --probes` 可见 UID `070000011a5a5cc500000000000c00b6a5a5a5a597969908`。pyOCD 默认 `pyusb` 后端会在 `DAP_INFO` 写入时 USB timeout，因此后续必须强制 `PYOCD_USB_BACKEND=hidapiusb`。 |
| 链路测试结果 | 使用 Ubuntu HIDAPI pyOCD、`py32f002ax5`、`--frequency 10000`、`under-reset` 读 CPUID，进入 SWD 后失败为 `Unexpected ACK '0'`。Ubuntu OpenOCD 使用 `interface/cmsis-dap.cfg` 也确认 CMSIS-DAP 本体正常，但 `Error connecting DP: cannot read IDR`。结论：Ubuntu 到 DAPLink USB 链路正常，当前失败在 DAPLink 到目标 MCU 的 SWD 物理/目标状态层，不是固件构建问题。 |
| 构建结果 | 使用 Ubuntu 构建脚本重新构建通过：`text=19588 B`、`data=676 B`、`bss=2324 B`；HEX SHA256 `14c62b63966df51fd0f10a66d2963d20985daccbf927b932e749a16c34732041`；ELF SHA256 `ea6063cc48e725eed088887831a4b8cc1a0f16f272aeaf5c6c5aedfc547526e5`；BIN SHA256 `ab79f4db1dbb3d5e66ecb37edf4748bbd3d7b41f9118ef74d759d5da701c2a50`。 |
| 下载状态 | 未成功下载。下载脚本进入 HIDAPI under-reset 后长时间无返回，已中止；不得把这次构建误记为已烧录到板上。下一步先恢复 SWD 目标回应：确认目标 VCC/GND、SWDIO/SWCLK/NRST、`CON3 / PB6-PF4-BOOT0` 是否按之前成功下载要求接到 VCC，再重新用 Ubuntu HIDAPI 下载。 |

2026-06-20 V7.10c 转回 macOS 短命令下载记录：

| 项目 | 内容 |
|---|---|
| 用户提醒 | “之前也是发生过这种问题，在我的这个qtpj里面，以后都不用这个方向，直接转回MACOS里再试吧” / “而且我记得每次下载时有一堆的问题，后来还特别提醒减短语句，直接下了” |
| 本次处理 | DAPLink 已从 `Ubuntu Linux` 退回 macOS host，`Connected-To-Vm: NO`。不再走 Ubuntu 下载，也不再用长脚本调试链路；改用 macOS `/tmp/pyocd-mac-venv/bin/pyocd flash` 最短命令直接下载当前 `build/py32f002_servo.hex`。 |
| macOS 结果 | macOS 能枚举 `DAPLink CMSIS-DAP`，但 `pyocd flash -t py32f002ax5 --pack /tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack -f 10000/100000 -O connect_mode=under-reset build/py32f002_servo.hex` 均在 `DebugPortSetup` 返回 `SWD/JTAG communication failure (Unexpected ACK '0')`。 |
| 判断 | 这次已经排除 Ubuntu、长 commander 命令、复杂脚本。失败点回到目标 SWD 状态，和 2026-06-03 记录的 `CON3 / PB6-PF4-BOOT0` 未接 VCC 时的 ACK0 现象一致。下一步只检查目标板 SWD/BOOT0/复位/供电，不改固件。 |
| 用户确认 | 用户确认当前 `CON3` 是接地状态，而之前成功下载时为接 `VCC`。后续下载前必须先确认 `CON3=VCC`，否则优先判断为 BOOT0/调试入口状态错误，不再先怀疑 Ubuntu、macOS、pyOCD 或固件。 |
| 最终下载 | 用户重新确认 `CON3=VCC` 后，按旧方法用 macOS 最短命令下载成功：`pyocd flash -t py32f002ax5 --pack /tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack -f 100000 -O connect_mode=under-reset build/py32f002_servo.hex`。pyOCD 完成擦写：擦除 `4096 bytes`、编程 `4096 bytes`、跳过 `16256 bytes`，速度 `2.93 kB/s`。随后用户提醒 `CON3=VCC` 时单纯 `reset/go` 可能从 BOOT0 启动入口运行，不一定进入用户 Flash；已改为短 commander 直接写用户向量 `SP=0x20000c00`、`PC=0x080048e1` 后 `go`，返回 `Successfully resumed device`。当前板上已运行本条记录中的 V7.10c PA3 EXTI 诊断版。 |
| 后续固定做法 | 下载前 `CON3=VCC` 保证 SWD 能连接；下载后若仍保持 `CON3=VCC`，必须用 `wreg sp <用户向量SP>` + `wreg pc <用户向量PC>` + `go` 启动用户程序，不能只 `reset/go`。若需要上电自然运行用户程序，应把 `CON3` 恢复到正常启动状态后重新上电/复位。 |

2026-06-20 V7.10d 外部 PWM 输入处理修正记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “过滤大范围做法有问题，变成我要从1000转到2000时无法动作；再一个，外部PWM信号输入时，还是会抖动，证明与PID无关；请改善外部输入PWM处理模式。” |
| 根因判断 | 问题在外部 PWM 输入链路，不在 PID。上一版为挡住 `504/2504us` 这类假宽度，把 `delta >= 900us` 的大跳变确认提高到 `32` 帧，真实 `1000 -> 2000us` 会被长时间卡住。静止外部 PWM 抖动则说明原始宽度毛刺仍会干扰显示/候选层。 |
| 本次修改 | `pwm_input.c` 增加轻量“两帧成对”预过滤：一帧大跳只记住，不直接送控制确认；下一帧仍在 `4us` 内才送入后级。大跳变确认从 `32` 帧降到 `3` 帧，真实 `1000 -> 2000us` 可通过；单帧错宽度不会进入控制目标。小步进 `1-3us` 仍保留确认，但确认从 `4` 帧改为 `6` 帧，减少静止随机抖动误触发。候选聚类容差从 `1us` 放宽到 `2us`，避免真实稳定输入因 1-2us 测量抖动反复清零。 |
| P50/控制 | 固件标记改为 `P50 V7.10d`。没有修改 PID、H 桥控制、刹车、锁止参数。P50 自身 `Stb=4` 二次锁存仍保留，用于防止输入目标抖动直接进控制环。 |
| 构建结果 | Ubuntu 构建通过：`text=19632 B`、`data=676 B`、`bss=2332 B`。HEX SHA256 `03f708f15220a07ecaa7f030b31129e5cd7b33ff91f7df1e0fc2a4e873def173`；ELF SHA256 `45a1bf30946c5f6fc532574469085a382e87129e6b29f9cbefa888c1f0b249fb`；BIN SHA256 `08873ee13de6b81d9cbc96ceb980d5dc421e4c106a1a6bd143f5fe76407afb74`。 |
| 下载结果 | 按旧方法 macOS 最短命令下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x0800490d` 并 `go`，返回 `Successfully resumed device`。当前板上运行 V7.10d。 |
| 验证重点 | 固定外部 PWM 不动时，P50 第一行看 `P` 是否稳定，`R/W` 可反映原始测量但不应带动目标抖动；从 `1000 -> 2000us` 和 `2000 -> 1000us` 必须能动作，不能再被大范围过滤卡住。 |

2026-06-20 V7.10e 外部 PWM 输入检测重构记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “我认为你要重新构建外部PWM输入检测程序，否则目前这个处理方式是非常不妥的” |
| 架构调整 | 放弃继续在 PA3 EXTI 软边沿捕获上叠过滤，改为 PA3 `GPIO_AF13_TIM1` + `TIM1_CH1` 硬件输入捕获。TIM1 以 `1MHz` 自由计数，CCR1 锁存边沿时间；上升沿记录周期并切换到下降沿，下降沿计算高电平宽度，再切回上升沿。周期只做合法帧检查，不参与宽度计算。 |
| 输入处理 | 保留最小连续确认：`<=1us` 视为静止测量噪声不更新；`1-3us` 小步进需连续确认 `5` 帧，以兼顾灵敏度和静止抗抖；大于 `3us` 的真实变化只需 `2` 帧确认。取消上一版 `delta >= 900us` 的长确认，所以 `1000 <-> 2000us` 不应再被过滤卡住。 |
| 硬件输入设置 | PA3 设为 `GPIO_MODE_AF_PP`、`GPIO_AF13_TIM1`、`GPIO_NOPULL`。这里没有采用 SDK 示例的内部上拉，因为当前输入前端已有外部下拉，内部上拉会把低电平抬高，可能造成误触发。 |
| 未改内容 | 不改 PID、H 桥、刹车、锁止参数；只改外部 PWM 输入检测层。固件标记为 `P50 V7.10e`。 |
| 构建结果 | Ubuntu 构建通过：`text=19656 B`、`data=676 B`、`bss=2324 B`。HEX SHA256 `de58c773f97e21b8e18e0b3acc00bba5c1767a87c3b6fbf944b25c0b5cd815ff`；ELF SHA256 `12ebc97349a6ce44aa047b8858db24548fe3cc7c506b5498acc145572b0eee65`；BIN SHA256 `77b36102dc187f65c1bf63cc627f48e4c003140ca05b5f4ca6e94eb1f56d2b66`。 |
| 下载结果 | 按旧方法 macOS 最短命令下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x08004925` 并 `go`，返回 `Successfully resumed device`。当前板上运行 V7.10e。 |
| 验证重点 | 先只看外部 PWM 输入：P50 第一行固定 PWM 时 `P` 应稳定，`R/W` 应明显比 EXTI 版本小；再测 `1000 -> 2000us`、`2000 -> 1000us` 是否能动作。若 P50 无 PWM 数值，优先查 PA3 输入幅值/接线，因为程序已经改到 TIM1_CH1 硬件捕获路径。 |

2026-06-20 V7.10f PWM W 显示窗口修正记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “W的误差有时会跑到1000，我在1000，1500，2000之间切换，不会因为瞬间检测就把误差计算出来吧，逻辑应该是抓到了真实的才会再反应W误差，会不是这样的原因走到最后会晃？” |
| 核对结果 | `W` 只用于 P50 显示，不参与 PID、目标位置或 H 桥输出，所以它不会直接让舵机晃。但上一版 `W` 是显示刷新窗口内原始 PWM 的最大最小差，若 150ms 显示窗口里刚好包含切换前 `1000us` 和切换后 `2000us`，就会显示 `W1000`，这不是同一固定 PWM 点的真实抖动。 |
| 本次修改 | `pwm_input.c` 增加 `raw_window_reset_to()`。首次确认有效 PWM、或 `P` 确认切换到新命令时，立即把 `W` 统计窗口重置到当前确认宽度，避免把旧命令和新命令一起算成误差。硬件 TIM1_CH1 捕获架构、PID、H 桥和目标映射均未改。 |
| 构建结果 | Ubuntu 构建通过：`text=19668 B`、`data=676 B`、`bss=2324 B`。HEX SHA256 `bd23dec599f7540e09c50814872e5a76b4e6f5bb066553622a5a21226c89ce88`；ELF SHA256 `6b29f02942f9b8c37cc2e4f047549f54ff64cec8e842931fa28d7227f1dc57ee`；BIN SHA256 `2d873da2b15e0e6adef43e23c7bca027d0ada6bf8c2dc8d68dced5c19f0000b8`。 |
| 下载结果 | 按旧方法 macOS 最短命令下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x08004931` 并 `go`，返回 `Successfully resumed device`。当前板上运行 V7.10f。 |
| 验证重点 | 切换 `1000/1500/2000us` 时，`W` 不应再因为命令切换本身显示 `500/1000`；固定某一点不动时再看 `W`，这个值才代表同一输入点的真实测宽波动。若舵机末端仍晃，应看 `P` 是否真的变化，以及 ADC/控制事件，而不是单独看切换瞬间的 `W`。 |

2026-06-20 V7.10g 外部 PWM 连续扫宽跟随修正记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “我现在外部PWM进行了一个不断变化的宽度进行输入，但你的检测根本反应不过来” / “20US一侦，理论上可以检测很多很多侦再做决定都不会影响反应速度才对啊” |
| 关键澄清 | `20us` 是 H 桥软件 PWM tick，不是外部舵机 PWM 输入帧。外部标准舵机 PWM 为约 `50Hz`，即 `20ms` 才有一次有效高电平宽度可确认；4 帧确认就是约 `80ms`，连续扫宽时若要求同一宽度重复出现，会一直确认不到。 |
| 根因判断 | V7.10f 的 TIM1 硬件捕获本身已经比 EXTI 可靠，慢的主要原因在输入层仍保留候选确认。快速连续变化时，候选值每帧变化，确认计数会被反复重置，导致控制用 `P` 滞后或不更新。 |
| 本次修改 | `pwm_input.c` 的 `accept_pulse_width()` 改为：只要周期和宽度合法，PA3/TIM1_CH1 硬件捕获到的高电平宽度立即更新为控制 `P`，同时更新 `pulse_count`。静止抗抖和锁止判断后续放到目标锁存/保持层处理，不再在输入捕获层卡住连续变化。 |
| 未改内容 | 没有修改 PID、H 桥、刹车、速度、锁止参数；没有重新打开内部 STEP test。固件标记为 `P50 V7.10g`。 |
| 构建结果 | Ubuntu 构建通过：`text=19456 B`、`data=672 B`、`bss=2320 B`。HEX SHA256 `6fd83e504bfdb5f72e4ac73099d15ac2845a09b56006e4274d26e5b3a1c2ba39`；ELF SHA256 `476619924968901b02266611f543f5fffbfa3be866e112ea6ebe5e01c933ef66`；BIN SHA256 `0d689f59900b08b121817e56fc57061482c31a26ff9b7adc84a8a5ead60d10f6`。 |
| 下载结果 | 按旧方法 macOS 最短命令下载成功：擦除 `20480 bytes`、编程 `20224 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x0800485d` 并 `go`，返回 `Successfully resumed device`。当前板上运行 V7.10g。 |
| 验证重点 | 外部 PWM 连续从低到高、从高到低扫宽时，P50 第一行 `P` 应每个 PWM 周期快速跟随，不应卡在旧值。固定在 `1000/1500/2000us` 时再观察 `P/R/W` 是否稳定，以及舵机是否仍有末端晃动。 |

2026-06-20 V7.10h 外部 PWM 关联马达目标记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “目前看还行，请关联马达转动” |
| 根因判断 | V7.10g 已让输入层 `P` 每个合法外部 PWM 周期更新，但控制层仍用目标 ADC 变化量判断是否退出 HOLD/LOCK。连续扫宽或小步进时，单帧 PWM 映射到 ADC 可能小于 `LCDM_SERVO_CMD_HOLD_EXIT_EXTRA=3 count`，导致 P50 上 `P` 已变化而马达目标不一定同步刷新。 |
| 本次修改 | `app_entry.c` 增加 `s_lcdm_servo_last_control_pulse_us`。只要控制用脉宽 `control_pulse_us` 与上一帧不同，就视为命令更新，刷新 `s_lcdm_servo_cmd_target_adc` 并退出 HOLD/LOCK 进入运动逻辑。这样外部 PWM 连续扫宽会直接联动马达目标。 |
| 未改内容 | 没有修改 PA3/TIM1_CH1 捕获、PID、H 桥、刹车、速度、锁止参数；内部 STEP test 仍关闭。固件标记为 `P50 V7.10h`。 |
| 构建结果 | Ubuntu 构建通过：`text=19512 B`、`data=676 B`、`bss=2324 B`。HEX SHA256 `c642ce7b24242cdbfa2d2ec05fb42393c5032f801fdad9796d74b724aeda3a13`；ELF SHA256 `6b9267da7dfdee07e544dbe7e8672af3dbe667b3593a65ecbdee74022f3e468b`；BIN SHA256 `a8486fd4a38ee21aa5a219effcaf22dde31067f53f50143256515416522a2514`。 |
| 下载结果 | 按旧方法 macOS 最短命令下载成功：擦除 `20480 bytes`、编程 `20224 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x08004895` 并 `go`，返回 `Successfully resumed device`。当前板上运行 V7.10h。 |
| 验证重点 | 外部 PWM 连续变化时，P50 第一行 `P` 应跟随，同时 `Tgt/Err/HBR` 应随之变化，马达应跟着转动。若 `P` 变但 `HBR` 不变，则继续查目标/状态门限；若 `HBR` 变但马达不动，则查 H 桥输出和方向/限流。 |

2026-06-20 V7.10i 外部 PWM 微抖防误触发记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “又返回老问题了，刚开始1分钟又乱晃了，过一段时间后又正常了” |
| 根因判断 | V7.10h 把任意 `pulse_delta != 0` 都当作命令更新，导致 `1us` 级输入测量抖动也会刷新目标并退出 HOLD/LOCK。刚上电或刚运行阶段 ADC/机构/输入窗口还未完全平稳时，这会把微小毛刺放大成马达乱晃；一段时间后杂讯变小又表现正常。 |
| 本次修改 | 增加外部 PWM 命令门控：`>=3us` 变化立即接受；`1-2us` 变化必须连续同方向 `2` 个外部 PWM 帧才接受；来回跳动的 `1-2us` 不刷新马达目标。确认计数绑定 `sample.pulse_count`，避免 1ms 控制循环把单个 20ms PWM 帧内的毛刺快速确认。 |
| 未改内容 | 没有修改 PA3/TIM1_CH1 捕获、PID、H 桥、刹车、速度、锁止参数；只修正外部 PWM 到马达目标的命令确认层。固件标记为 `P50 V7.10i`。 |
| 构建结果 | Ubuntu 构建通过：`text=19656 B`、`data=680 B`、`bss=2328 B`。HEX SHA256 `d560234a4d2fc73ddd4cfcd62d85ed851af9ea790252cfee9773965d2298706e`；ELF SHA256 `82192c2d8461677a425b5cbd730c19ca01585f3006d6d61e387f1631eb1b33e6`；BIN SHA256 `05bdaf56e836234c1f07e7e0151793181747734951d129789faebf7e0e623cfa`。 |
| 下载结果 | 按旧方法 macOS 最短命令下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x08004925` 并 `go`，返回 `Successfully resumed device`。当前板上运行 V7.10i。 |
| 验证重点 | 固定外部 PWM 时，P50 的 `P/R/W` 可以显示微小波动，但 `Tgt/Err/HBR` 不应因为来回 `1-2us` 跳动而乱变。连续同方向扫宽时，`P` 和马达目标仍应跟随。 |

2026-06-20 P50J 外部 PWM 与内部自测差异收尾记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “这样说有点不对，那为什么内部定点自测又不存在问题呢？肯定哪里衔接不对。” / “你先记录吧，今天到此止，明天继续” |
| 今日关键结论 | 用户判断正确，不能只把外部 2000 点晃动归因到刹车参数。对比内部自测和外部 PWM 数据后，根因更像是衔接问题：外部 PWM 在运动过程中会有 `2002 -> 2001/2003` 一类小跳变，旧逻辑把这种小变化也当作新的 `command_step`，导致运动相位被重置到 `ACCEL`；内部自测是理想 `1000/1500/2000`，运动中没有这些小命令扰动，所以表现更稳。 |
| SWD 数据依据 | 旧外部采样 `build/servo_curve_live/latest_stream.csv` 中，高端运动段可见 `pwm_us` 在 `2000-2003us` 之间变化，`target_adc` 在 `2916-2921` 之间小跳，同时反馈在接近目标后仍有回摆；内部采样 `live_internal_v7_10_16s.csv` 的目标点为固定 `1000/1500/2000us`，无运动中小幅命令重置。 |
| 已做修正 | `app_entry.c`：增加 `motion_phase_active` 判断。外部 PWM 小变化仍可更新目标值，但只有在静止/HOLD 时才作为新动作触发；运动过程中小于 `LCDM_SERVO_CMD_MOTION_EXIT_EXTRA` 的微小变化不再重置加速/刹车流程。此前试探性的提前刹车与提高反刹上限改动已撤回，避免混合验证。 |
| 输入门控状态 | 保留 `LCDM_SERVO_PWM_CMD_DIRECT_US=8`，`LCDM_SERVO_PWM_CMD_REPEAT_COUNT=6`。`>=8us` 立即接受；`1-7us` 必须同一个脉宽连续 6 个外部 PWM 帧才接受，防止 `2001/2002/2003` 来回跳被累计成真实命令。 |
| 显示压缩 | 由于 Flash 空间很紧，P50 标题仍为 `P50J`；P50 状态文字进一步压缩，例如 `HLD -> H`、`BRK -> B`、`D+/D- -> +/-`，仅为节省空间，不影响控制逻辑。 |
| 构建结果 | Ubuntu 构建通过：`text=19668 B`、`data=680 B`、`bss=2328 B`。HEX SHA256 `40410e37c5b7a7ff6cb6f24bde0d5ec5532a9137f47a0042fd23e9e4d5821e31`；ELF SHA256 `35013ddebde97bde5d9c1c56c275a234d3291bbb2186cab52133a5a984ce56ef`；BIN SHA256 `ba42b46ec3b8526810dc7069a5cc6df17d00087c0cf4d15e407bc6fa524d96a7`。 |
| 下载结果 | macOS DAPLink 短命令下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x0800495d` 并 `go`，返回 `Successfully resumed device`。当前板上已运行本条记录的 `P50J` 衔接修正版。 |
| 明天验证重点 | 先不改刹车参数，先验证外部 PWM 下 `1000 -> 2000`、`1500 -> 2000`、固定 `2000` 是否接近内部自测表现。若仍晃，重新抓 SWD 数据，重点看运动中 `command_step/target_adc/event/duty/feedback_adc` 是否还被 `1-3us` 外部 PWM 小跳变打断；若目标不再重置但仍过冲，再回到刹车曲线。 |
| 用户追加反馈 | 当前下载版仍“还有一点点晃”；`2000us` 端仍有过冲现象，用户判断它与外部 PWM 信号处理仍有关联。明天不能直接跳去调刹车参数，需先用 SWD 对比外部 PWM 与内部自测在 `2000us` 端的目标锁存、运动相位、事件码是否一致。 |

2026-06-21 P50K 外部 PWM 命令层二次整理记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “请继续处理外部PWM信号程序，还是不太科学” |
| 本次原则 | 只处理外部 PWM 到控制命令的衔接，不改 PID、刹车、H 桥和锁止参数。目标是把原始捕获值、已接受控制命令、运动相位触发分开。 |
| 本次修改 | `app_entry.c`：去掉冗余的二次锁存函数，`pulse_count` 只在命令接受层防止同一外部 PWM 帧被重复确认；`P50` 第一行第一个数改为显示已接受的控制命令，第二个数仍显示原始捕获宽度，第三个数显示窗口抖动；`1-7us` 小变化确认增加候选簇判断，防止同方向小噪声长期累计成假命令。 |
| 保留逻辑 | PA3/TIM1_CH1 硬件捕获每个合法 50Hz 外部 PWM 帧；`>=8us` 真实变化立即接受；`3-7us` 小变化需同方向、同簇 2 帧；`1-2us` 灵敏度变化需同方向、同簇 6 帧；运动中小于 `LCDM_SERVO_CMD_MOTION_EXIT_EXTRA` 的命令扰动不重置加速/刹车相位。 |
| 构建结果 | Ubuntu 构建通过：`text=19668 B`、`data=680 B`、`bss=2320 B`。HEX SHA256 `9356e92da4d255c79ed211c27059b2dec84b57d0ef8d7ea76dc73bf767bf8e19`；ELF SHA256 `bf514d80ec8dd1f8922ba887d2c1dcc5a7904ab448deb17eb05d76ab791a4c3a`；BIN SHA256 `29f0008226363350e0dfb28af519c49b5b383cc9c4a6b413ffbd473e20222b92`。 |
| 下载结果 | macOS DAPLink 下载成功：擦除 `4096 bytes`、编程 `3968 bytes`、跳过 `16384 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x0800497d` 并 `go`，返回 `Successfully resumed device`。当前板上运行 `P50K`。 |
| 验证重点 | 固定外部 `1000/1500/2000us` 时，P50 第一行第一个数应稳定代表控制命令；第二个数若有 1-2us 原始抖动，不应频繁带动目标和 H 桥。继续测 `1000 -> 2000`、`1500 -> 2000`、固定 `2000` 和连续扫宽；若还晃，下一步抓 SWD 数据重点看控制命令是否已经稳定。 |

2026-06-21 P50L 外部 PWM 运动衔接与 HOLD 误差修正记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “高端2000会慢过去的动作。同时，又还有测试一段时间又会每个点乱摆……连接SVG看看？” / “这样乱摆我认为还是外部PWM的处理问题多一些” |
| SVG/SWD 抓取 | Ubuntu 侧实时脚本找不到探针，已改用 macOS 直接 SWD 抓取 `build/servo_curve_live/latest_stream.csv`，并生成同一页面 `servo_curve_live.html/svg`、`hbridge_response.html/svg`。 |
| 数据结论 | 90 秒共 `888` 行。外部 PWM 原始宽度范围 `1002..2005us`，控制命令范围 `1002..2005us`；`raw-control` 大差值只出现 `2` 行，且没有进入控制命令，说明 `500/2500us` 类假宽度已基本挡住。主要问题转为外部 PWM 连续切换时，控制环运动中仍被新命令重置相位，以及到位 HOLD 容许误差过宽。 |
| 典型现象 | 高端可见 `target=2924`、`feedback≈2952`、`error≈-28 count` 时已经进入 `HOLD/event=2 duty=0`，因此表现为高端慢过去或停点偏差；外部 PWM 90 秒内接受命令跳变 `166` 次，若运动未收住就不断重启相位，会表现为每个点乱摆。 |
| 本次修改 | `app_entry.c` 固件标记改为 `P50L`。外部 PWM 命令在运动中继续变化时，只更新目标值，不再反复把运动相位重置为 `ACCEL`；只有当前不在运动相位时，新命令才作为新的运动起点。`v7_settle_band` 改为等于 `hold_release_band`，不再叠加 `motion_hold_band` 放宽 HOLD，避免二三十个 AD count 误差就停住。 |
| 构建结果 | Ubuntu 构建通过：`text=19660 B`、`data=680 B`、`bss=2320 B`。HEX SHA256 `18e03cd9bab80cbbbe2b5a15357a63c2673a5f7ef6f7cc085dc55088257ef3a0`；ELF SHA256 `eefc17843b9d3e2789a6b906738c1c05c2a6d2ca312de1da710a9e14fc2c086a`；BIN SHA256 `858bfda3a9c74d11660415bf6a46fb393d5f09f1edba2286b5e78b057439a61b`。 |
| 下载结果 | macOS DAPLink 下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x08004975` 并 `go`，返回 `Successfully resumed device`。当前板上运行 `P50L`。 |
| 下载后短抓 | `build/servo_curve_live/p50l_postflash_4s.csv` 共 `41` 行；固定 `2003us` 时 `target_adc=2921`、`feedback_adc=2918..2919`、误差 `2..3 count`、`event=2`、`duty=0`，确认下载后静态高端正常且 SWD 可读。 |
| 验证重点 | 先试固定 `2000us` 是否还慢慢贴过去；再试快速 `1000/1500/2000` 切换一段时间是否还演变成每个点乱摆。若仍乱，下一轮抓 P50L 的 `latest_stream.csv`，重点看控制命令是否稳定而 `event/duty` 自己反复触发。 |

2026-06-21 P50M 大阶跃外部 PWM 恢复记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “改得更糟了，反应也变慢了，突然从2000-1000瞬间切换，是慢慢的走过去了。目前千万不要改PID参数” / “一样还是摆” |
| 根因确认 | P50L 的“运动中不重启相位”做得过头。它能挡住小 PWM 抖动打断运动，但把 `2000->1000` 这种大阶跃也当成运动中跟随目标，导致相位没有重新进入正常大行程启动，表现为慢慢走过去。 |
| 本次修改 | `app_entry.c` 固件标记改为 `P50M`。不改 PID 参数、不改 H 桥参数。外部 PWM 若造成目标变化超过 `LCDM_SERVO_CMD_SETTLE_BAND=96 AD count`，即使当前正在运动，也立即作为新大行程命令重启运动相位；小扰动仍沿用 P50L 逻辑，不打断当前运动。为压 Flash，P50 最后一行显示去掉 event 数字；SWD 曲线仍保留完整 event 字段。 |
| 构建结果 | Ubuntu 构建通过：`text=19668 B`、`data=680 B`、`bss=2320 B`。HEX SHA256 `99c6a82dc9d0561d0c1061b61e6e93777cef4388ea10244131ce2acdae3b31cf`；ELF SHA256 `84b28355cc00f5e3ca686a36857509dc4b4c69a4e189c627f58dbb943adca928`；BIN SHA256 `445a9b494de53b42e4a7a4c4be67ab9a981fd617b143bf999f1199944032b1f8`。 |
| 下载结果 | macOS DAPLink 下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x0800497d` 并 `go`，返回 `Successfully resumed device`。当前板上运行 `P50M`。 |
| 下载后短抓 | `build/servo_curve_live/p50m_postflash_6s.csv` 共 `60` 行；固定高端时 `pwm_us=2001..2002`、`pwm_raw_us=2000..2003`、最终 `target_adc=2920`、`feedback_adc=2914`、误差 `6 count`、`event=2`、`duty=0`。 |
| 验证重点 | 先验证突然 `2000->1000` 是否恢复快速响应；再看各点是否仍摆。如果仍慢，必须抓切换瞬间数据，而不是继续调 PID。 |

2026-06-21 P50N 大阶跃提前反刹记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “现在可以，反应快。但摆得也大。” |
| SWD 数据 | 抓取 `build/servo_curve_live/p50m_swing_20s.csv`，捕到 `2003us -> 1004us`。典型序列：`error=-1708 duty=-1000 event=12`，下一拍 `error=-925 duty=-644 event=12`，随后已过目标到 `feedback=1115 target=1186 error=+71` 才开始 `event=18 duty=+243/+360`，最低冲到 `feedback=812`。结论：大阶跃恢复速度后，提前反刹太晚，导致过冲摆大。 |
| 本次修改 | `app_entry.c` 固件标记改为 `P50N`。没有修改 PID 参数表、H 桥最大 duty、PWM 捕获。只把 `v7_need_prebrake` 改为动态刹车区：当正在朝目标高速移动，且距离进入 `brake_max_band + max_vel_q4` 范围时提前进入反刹，避免继续同向大 duty 冲过目标。 |
| 构建结果 | Ubuntu 构建通过：`text=19672 B`、`data=680 B`、`bss=2320 B`。HEX SHA256 `ff04e3c5ea574bebaeaa164c3dfa9a2d7a36b03bd82994ee689e6edc37227db3`；ELF SHA256 `b4e695f4284a244ccef5a4e232ae02bbacbeeab42a42d4ad3cb30faa780709b5`；BIN SHA256 `451972dac4dd82bf15ca694e43adce5dc44423c74d2b73acc840d3bccdf94c24`。 |
| 下载结果 | macOS DAPLink 下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x08004981` 并 `go`，返回 `Successfully resumed device`。当前板上运行 `P50N`。 |
| 下载后短抓 | `build/servo_curve_live/p50n_postflash_8s.csv` 共 `78` 行；未捕到切换，只确认静态中位正常：`pwm_us=1501..1503`、`pwm_raw_us=1502..1504`、`duty=0`、最后 `target_adc=2051`、`feedback_adc=2059`、误差 `-8 count`。 |
| 验证重点 | 用户继续实测 `2000->1000` 和 `1000->2000`。如果仍摆大，下一步必须抓 P50N 的切换瞬间，看提前反刹是否在 `error≈900` 时已介入，不能再盲改 PID 参数。 |

2026-06-21 P50O 撤销 P50N 无反应版本记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “没反应了” |
| SWD 数据 | 抓取 `build/servo_curve_live/p50n_no_response_10s.csv`。外部 PWM 和目标均正常变化：`pwm_us=1002..2002`、`target_adc=1183..2920`；但反馈卡在 `1748..1751`，软件输出长期只有 `duty=-80..80`，事件全为 `event=12`。结论：不是外部 PWM 捕获失败，而是 P50N 的提前反刹改动导致控制输出被压住，舵机无力动作。 |
| 本次处理 | 立即作废 P50N。`app_entry.c` 固件标记改为 `P50O`，撤销 P50N 的动态提前反刹条件，恢复 P50M 的 `v7_need_prebrake` 判断；保留 P50M 的大阶跃外部 PWM 立即重启运动相位逻辑。PID 参数表、H 桥参数未修改。 |
| 构建结果 | Ubuntu 构建通过：`text=19668 B`、`data=680 B`、`bss=2320 B`。HEX SHA256 `f65a9c9201464fc9c9ae52c2cf79767f7b0ce388472c90d4407d59ae49af29af`；ELF SHA256 `2f659f11eb7f53d32ebd74b61c9897c2b5b5e54b2367dc71c78c7395fe54ca15`；BIN SHA256 `95072b62b04b1013e17fba5c0f8d3461c5077a20a4c78034dbeecc2a8b9aadf7`。 |
| 下载结果 | macOS DAPLink 下载成功：擦除 `20480 bytes`、编程 `20352 bytes`。随后写用户向量 `SP=0x20000c00`、`PC=0x0800497d` 并 `go`，返回 `Successfully resumed device`。当前板上运行 `P50O`。 |
| 下载后短抓 | `build/servo_curve_live/p50o_restore_8s.csv` 共 `645` 行；未捕到外部 PWM 切换，只确认静态中位正常：`pwm_us=1503..1504`、`pwm_raw_us=1502..1505`、最后 `target_adc=2052`、`feedback_adc=2056`、误差 `-4 count`、`event=2`、`duty=0`。 |
| 下一步 | 请用户先确认 P50O 是否恢复动作。如果仍没反应，必须抓切换瞬间数据；如果动作恢复但仍摆大，再重新设计提前反刹，不能复用 P50N 的条件。 |

2026-06-21 P50O 外部无动作与内部强制测试记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “无切换，LCDM上有显示，但不动作” |
| 外部模式 SWD | 抓取 `build/servo_curve_live/p50o_no_motion_15s.csv` 和 `p50o_switch_watch_30s.csv`。两次外部模式下 `pwm_us` 始终约 `1503us`，`pwm_raw_us=1502..1505`，没有任何 `1000/2000us` 切换进入 MCU；`target_adc` 固定约 `2052`，因此程序保持中位 HOLD、不驱动马达是合理结果。 |
| 内部强制测试 | 通过 SWD 临时写 `s_lcdm_pwm_source=1`，抓取 `build/servo_curve_live/p50o_internal_forced_12s.csv`。内部自测目标在 `1000/1503/2000us` 循环，H 桥 `duty` 可到 `±1000`，反馈 ADC 大幅跟随，说明马达控制链路、H 桥输出、ADC 反馈都能动作。 |
| 恢复状态 | 内部强制测试后已通过 SWD 写 `s_lcdm_pwm_source=0` 切回外部 PWM 输入。当前板上仍为 `P50O`。 |
| 结论 | 当前“LCDM 有显示但不动作”不是 PID 参数或 H 桥输出问题，而是外部 PWM 变化没有进入 PA3/TIM1_CH1 捕获链路。下一步应核对外部 PWM 是否实际接到 PA3、P50 第一行 raw 数值是否随信号源变化；若 raw 固定在 1503us，软件不会动作。 |

2026-06-21 P50O 内部输入抖动与 PID 改动核对记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “内部输入也会抖了，确认一下有没有改过PID?” / “确认一下，之前有一个版本是内部输入是测试通过的。” |
| 核对结果 | 记录中确认内部输入测试通过版是 `V7.10` 内部阶跃自测版，数据文件 `build/servo_curve_live/live_internal_v7_10_16s.csv`；该版 `event18=0`、`event14=0`、无 bad 段，最大最终误差 `19 count`、平均最终误差 `7.2 count`、平均 HOLD 时间约 `281.5ms`。 |
| 当前版本差异 | 当前板上记录为 `P50O`，不是纯 `V7.10` 内部阶跃版。`P50K/P50L/P50M/P50O` 记录均注明没有修改 PID/刹车/速度/锁止参数表；但 `P50L` 改过运动中命令衔接和 `v7_settle_band`，`P50M/P50O` 保留了大阶跃目标变化时重启运动相位的状态机逻辑。因此若内部输入也抖，应优先按状态机衔接差异排查，而不是先调 PID 参数。 |
| 处理原则 | 若要验证内部基线，应回到 `V7.10` 控制基线或临时用内部源抓 SWD 曲线，对比 `command_step/event/duty/feedback_adc`；在确认差异前不修改 PID 参数。 |

2026-06-21 V7.10I 内部通过版重建失败与回退记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | “你还是把内部输入通过的版本让我确认一下吧” / “我确认后，你再改外部输入的处理程序” / 实测反馈：“这个不对哦，会摆动哦” |
| 事实核对 | 当时真正通过的 `V7.10` 内部阶跃固件 HEX 未保留，仓库只保留了 CSV 数据和 SHA256 记录。尝试用当前源码重建 `P50 V7.10I / STEP` 内部自测版，但这不是原始二进制。 |
| SWD 失败证据 | 重建版 `build/servo_curve_live/v7_10i_internal_confirm2_10s.csv` 出现 `event18=18`、`event16=1`、`event3=1`，duty 达 `-1000..989`，与旧通过数据 `build/servo_curve_live/live_internal_v7_10_16s.csv` 的 `event18=0`、`event14=0`、无 bad 段不一致。 |
| 处理结果 | 该 `V7.10I` 重建版作废，不能作为内部通过基准。已立即刷回 `P50O` 外部普通版，HEX SHA256 `f65a9c9201464fc9c9ae52c2cf79767f7b0ce388472c90d4407d59ae49af29af`，启动向量 `SP=0x20000c00`、`PC=0x0800497d`。 |
| 后续原则 | 不能再让用户试“看起来像”的内部版本。必须先离线从代码差异恢复到旧 CSV 指标，至少 SWD 统计达到 `event18=0/event14=0` 后，才允许下载给用户确认。若无法恢复旧指标，应明确说明旧 HEX 丢失，转为基于当前数据重新建立基线，而不是冒充 V7.10。 |

2026-06-21 自有 GitHub 查找 V7.10 记录：

| 项目 | 内容 |
|---|---|
| 用户纠正 | 用户说明“上 GitHub 找一下”是找自己的仓库历史，不是找别人参考代码。 |
| 查找范围 | 查询 `origin=https://github.com/andrewlau2080/PY32F002_SERVO.git` 的远端 refs、GitHub API branches/tags/releases/commits/actions/artifacts/matching-refs。 |
| 结果 | GitHub 远端只有 `refs/heads/main -> 8211f445b0789f0227c235e22842a5638bfa5973`，无 tag、无 release、无 GitHub Actions run、无 artifacts；公开 commit 列表只到 2026-06-01。2026-06-20 的 `V7.10` 内部调试版未提交也未推送到 GitHub。 |
| 结论 | 不能从自有 GitHub 找回旧 `V7.10` HEX 或源码快照。后续若要恢复，只能继续查本机本地备份/构建缓存/系统快照，或根据旧 CSV 指标重新恢复算法。 |

2026-06-21 V7.10R 系列内部自测恢复记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “那你慢慢找，找没有改输入信号处理前的几个，应该是可以的” / “我要离开一会，你自己找了，自己开启SVG对一下，内测不会摆和抖的就是对的，没找到就一直找，找到为止，我回来要看到有好的PID不抖的。” |
| 找回的旧二进制 | Ubuntu `/tmp/servo_curve_v710b/build/py32f002_servo.elf` 与文档中 `V7.10b` 哈希完全一致，已归档到 `build/archive/v7_10b_pre_input_filter/`。该版是普通外部 PWM 版，HEX SHA256 `553bc8cc8975f8966174ca3010e2032a9b2f13261f49f908ecb13adeb2f082bb`，ELF SHA256 `fb2e8dab3a3dae04d63c8a5ee6b084292178b800f56831ee4851129683b64d94`。真正旧 `V7.10` 内部 STEP 二进制仍未找到。 |
| R1 修改和结果 | `P50 V7.10R1 / STEP`：内部自测时禁用 `crossed_target -> event18` 强反打，保留负载推力 `event16`。16 秒数据较好，`event18=0`、尾段最大误差 `28 count`、尾段最大摆幅 `2 count`；但 30 秒长测后段仍出现完整段尾段摆幅 `45..153 count`，不能作为最终稳定版。 |
| R2 作废 | 尝试在内部 STEP 中禁用 `event16` 负载推力，结果变差：尾段平均误差 `30.1 count`、尾段平均摆幅 `53.1 count`，说明当前框架里 `event16` 对终点拉回有帮助，不能简单去掉。 |
| R3 作废 | 尝试进入 HOLD 时不清零速度估计，结果出现一段 `maxE=759 count`、`osc=873 count` 的严重异常，作废。 |
| R4 当前板上版本 | `P50 V7.10R4 / STEP`：保留 R1 的“内部自测禁用 event18 强反打”，恢复进入 HOLD 清零速度，同时把 `v7_settle_band` 从 `Rel + Hbd` 收紧为 `Rel`，避免误差 `30..40 count` 就进 HOLD。 |
| R4 下载结果 | macOS DAPLink 下载成功：擦除 `20480 bytes`、编程 `19328 bytes`；随后 `reset halt`、写用户向量 `SP=0x20000c00`、`PC=0x08004521`、`go`，返回 `Successfully resumed device`。当前板上保持 `P50 V7.10R4 / STEP`。 |
| R4 数据 | 30 秒 SWD 文件 `build/servo_curve_live/live_internal_v7_10r4_30s.csv`；刷新后的曲线为 `build/servo_curve_live/servo_curve_live.html` 和 `build/servo_curve_live/hbridge_response.html`。排除最后一个被采集窗口截断的未完成动作后，完整段尾段误差约 `0..23 count`，大多数尾段摆幅 `1..14 count`；比 R1 后段 `45..153 count` 摆幅明显改善。 |

2026-06-21 V7.10 打开外部 PWM 前基准确认：

| 项目 | 内容 |
|---|---|
| 用户原话 | “这就是在打开外部PWM前的最后一个版本” |
| 确认基准 | 用户指的是 `2026-06-20 V7.10 提速和外部 PWM 临时屏蔽记录`，也就是 `SERVO_ENABLE_INTERNAL_STEP_TEST=1` 的内部 1000/1500/2000us STEP 自测版。该版在 STEP 构建中把外部 PWM 输入编译为桩函数，不读取外部 PWM。 |
| 禁止混淆 | 不能把后面的 `V7.10 外部 PWM 恢复下载记录`、`V7.10b`、`P50K/L/M/O` 或 `V7.10R` 系列当作这个基准版本。它们已经加入外部 PWM、输入滤波、内外源切换或状态机衔接修改。 |
| 目标校验值 | 原始基准固件记录为：HEX SHA256 `d1d0e88f95d2ed26f65b8141a5263498371f9c96390be1c9431d69e4869733d7`；ELF SHA256 `ba97e8a3f158d913c2d238d7ab942d69a3aff21eef1f7a8223c9bfb1a0878005`；BIN SHA256 `85d3416e556d7a260df239bdb0a449f7793593486e856355baf8bf5081348725`。 |
| 当前事实 | 本机归档只找到了 `V7.10b` 外部 PWM 版和 `V7.10R4` 内部恢复版二进制，未找到上述 `d1d0e88...` 原始内部 STEP HEX。若后续要“恢复原版”，必须先重建并用 SHA 或 SWD 曲线指标确认，不能下载看起来相似的版本冒充。 |
| R4 归档 | 固件和数据已归档到 `build/archive/v7_10r4_internal_best/`。HEX SHA256 `8cd489e120c6bfc75ec0b7f0e0c3fca4a51435adef2f45122061aa4154f058e0`；ELF SHA256 `0efc0727207933b0ce4943f19e285521b2f3ef86ac67299fe1ddf0a78250b1ac`；BIN SHA256 `d813efd39794a734959dadfd0c88d3fdb4261c9d16bedfa6ae798fa4acd20e14`。 |
| 后续原则 | R4 只能作为“内部自测恢复基线”，不是最终产品 PID。下一步若用户确认目视不摆，再以 R4 为基础恢复外部 PWM 输入；若外部仍摆，先处理输入命令衔接，不再修改 R4 的核心刹车/末端参数。 |

2026-06-21 V7.10h 外部 PWM 关联马达目标恢复记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “这版能恢复吗？” / “如果可以恢复，那我就可以在这个版本内切换IN还是外部模式” / “下次只要改版本，就上GITHUB” |
| 核对结论 | 2026-06-20 的原始 `V7.10h` 二进制仍未在本机归档目录或 GitHub 中找回，因此不能说“原始 HEX 完全恢复”。但当天 session 记录证明该版 ELF 字符串包含 `EXTPWM`、`INTPWM`、`P50 V7.10h`，说明 `V7.10h` 当时已经支持 P50 内外 PWM 源切换。 |
| 本次恢复 | `app_entry.c` 恢复 `P50 V7.10h` 标记；恢复 `s_lcdm_servo_last_control_pulse_us` 和锁存脉宽逻辑；撤销后续 `P50K/L/M/O` 的 `accept_control_pulse` 多帧门控、运动中小命令不重启相位逻辑；恢复 `pulse_delta != 0` 即视为命令更新，并恢复 `command_step = target_changed`；`v7_settle_band` 恢复为 `Rel + Hbd`。 |
| 保留内容 | 保留 PA3/TIM1_CH1 硬件捕获、运行时 `IN` 键切换 `EXTPWM/INTPWM`、P50 LCDM 诊断界面、当前 H 桥/ADC/参数页框架。未修改 PID、刹车、速度、锁止参数表。 |
| 构建结果 | Ubuntu 构建通过：`text=19444 B`、`data=676 B`、`bss=2324 B`。字符串校验可见 `EXTPWM`、`INTPWM`、`P50 V7.10h`。 |
| SHA256 | HEX `87a45405175706ca26638631b72a68cd3e38237d1b76875f0f403a3c5d2c491a`；ELF `2f1f3660567a5f9f83a8f92fc5968121a1fee5e3d6f0eb5c5171f26ccdee7fd7`；BIN `762d06c4c209bc1319e4139e3751cc7772e4e3320b34d5b1163cd73868aec79e`。这些与原始 V7.10h 记录 `c642ce7b.../6b9267.../a8486...` 不一致，所以本版命名为“V7.10h 逻辑恢复版”，不能冒充原始二进制。 |
| 归档位置 | `build/archive/v7_10h_recovered_20260621/` 已保存本次 `.hex/.elf/.bin/.map`。 |
| GitHub 规则 | 从下一个可测试版本开始，任何版本号变化都必须在本地 Git 形成清晰 commit，并 push 到 `origin/main` 或打 tag；不再只依靠 Markdown 记录、Codex session 或 build 缓存恢复版本。当前工作区已有大量历史未提交变更，本次先记录和归档，后续需先整理一次基线提交。 |
| 下载状态 | 本次只完成源码恢复、构建、hash 和归档，尚未下载到板。若要实测，需再执行 macOS DAPLink 下载，并以 pyOCD `Successfully resumed device` 作为成功依据。 |
| 下载更新 | 用户要求“请下载来测试一下”后，使用 macOS DAPLink/pyOCD 下载当前 `build/py32f002_servo.hex`，SWD `2MHz`、`connect_mode=under-reset`。启动向量 `SP=0x20000c00`、`PC=0x0800488d`，pyOCD 返回 `Successfully resumed device`。当前板上运行本条记录的 `V7.10h 逻辑恢复版`。 |

2026-06-21 V7.10h 内部自检 PID/SVG 验证记录：

| 项目 | 内容 |
|---|---|
| 用户原话 | “以这一版本重新验证PID，请开启VSG，进行自检，就像之前说的，他会自己转一段时间后会晃的很厉害，有时又会好，究竟出在哪里问题。注意，这个检测是不试外部PWM，只试内部的。” / “一直搜集，直到会晃” / “开始晃了” / “又不晃了” |
| 执行动作 | 不接外部 PWM，不改 PID。通过 SWD 写 `s_lcdm_pwm_source=1` 切到 `INTPWM` 内部自检；用 macOS pyOCD 直接读取 `g_servo_curve_frame`，生成 `build/servo_curve_internal_10h_check/` 和 `build/servo_curve_internal_10h_long/` 的 CSV/SVG/HTML。 |
| 第一轮数据 | `build/servo_curve_internal_10h_check/servo_curve_live.csv`，约 `150s`，`12076` 行。最后 30 秒窗口 `event18` 从前面个位数/十几次突增到 `108` 次，说明确实存在“跑一段时间后突然晃”的间歇性状态。最坏周期 `cycle_068_1000_to_1000.csv` 实际跨了完整行程，反馈 `1157..2957`，尾段跨度 `1799 count`，`event13=33`，duty 到 `1000`。 |
| 第二轮数据 | `build/servo_curve_internal_10h_long/servo_curve_live.csv`，约 `250s`，`19718` 行、`201` 个周期。第 7、8 个 30 秒窗口 `event18` 分别突增到 `121/94` 次。最坏周期包括 `cycle_115_1000_to_1000.csv`、`cycle_196_2000_to_1500.csv`、`cycle_200_1500_to_1500.csv`。 |
| 关键现象 | 不是外部 PWM 捕获造成，因为控制用 `pwm_us` 来自内部源。异常周期里会出现同名 `1000_to_1000` 或 `1500_to_1500`，但周期内部目标已经跨过 `1000/2000/1500`，说明上一个动作还没真正稳定，下一段内部命令又叠进来了。 |
| 典型片段 | `cycle_115_1000_to_1000.csv`：`1000` 稳定后切到 `2000`，到 `target=2916 feedback=2925 error=-9` 就进 `HOLD/event=2`；随后反馈漂到 `2951 error=-35` 又触发 `event12 duty=-240/-392`，再反复出现 `+41/-33/+23 count` 小误差驱动。然后内部命令切回 `1000`，系统以 `duty=-1000` 追赶，形成肉眼可见的大晃。 |
| 另一片段 | `cycle_138_1000_to_2000.csv`：接近高端后先 `event14` 反刹，过点到 `feedback=3022 error=-106` 后连续 `event18` 反打 `11` 次，再回到 HOLD。这个说明反打退出条件和 HOLD 进入条件互相打架，能量被反复注入。 |
| 当前判断 | 根因不在外部 PWM。10H 的问题集中在末端状态机：过点/HOLD 后还允许小误差重新进入 `DRIVE/event12`，并且 `event18` 强反打在某些速度/位置组合下会连续触发；当内部下一目标按时间到来时，未收敛的余振与新命令叠加，表现为“突然晃得厉害，过一会又好”。 |
| 下一步建议 | 下一版不要先调 PID 参数。应先改 3 点：1. 内部自检生成器用于 PID 验证时改为“等 HOLD 稳定若干 ms 后再跳下一点”，避免测试工具把未收敛动作叠加；2. HOLD 后的小误差只做锁止或静默保持，不允许立即用 `event12` 小 duty 反复找点；3. `event18`/反刹退出后设置短暂阻尼/禁止反向重入窗口，防止过点后连续反打。 |

2026-06-23 V9.2-V9.4 内部两点 PWM 间歇抖动追踪记录：

| 项目 | 内容 |
|---|---|
| 用户反馈 | 当前在内部 PWM 下，正常运行时看起来可以接受，不抖不晃；但运行一段时间后会在到点位置来回晃和抖，再过一段时间又恢复。用户判断该问题不只是新 PID，因为改 PID 前也出现过类似现象，要求从 ADC、算法状态和时序从头核查。 |
| 排查边界 | 本轮只检查和修改控制逻辑、ADC 反馈读取、末端 HOLD/LOCK/过点状态机。明确不改 LCDM、触屏、UART、引脚、外部 PWM 下载方式和 UI 初始化。内部测试仍使用运行时写 `s_lcdm_pwm_source=1` 切到 `INTPWM`，外部因素先排除。 |
| 历史关联 | 2026-06-21 的 V7.10h 内部长抓已经证明：异常不是外部 PWM 捕获造成，而是末端状态机在 HOLD 后允许小误差重新进入 `event12`，并且过点后 `event18` 强反打可能连续触发。内部测试固定时间切下一个点时，未收敛余振会和新命令叠加，表现为“有时晃、有时又好”。 |
| V9.0 基线 | `30ba855 Create V9.0 customer PID baseline` / tag `v9.0-customer-pid-baseline` 是按客户 logicdata 修改后的基础版。该版主要处理客户驱动/反刹/尾段逻辑，但末端 HOLD/LOCK 抖动路径仍保留旧问题。 |
| V9.1 中间状态 | 为减少客户逻辑曲线中“正转刹车后还有很长绿色正转 PWM”，客户 profile 调整为 drive `12ms/930`、brake `4ms/360`、tail `3ms/220`、coast gate `24ms`，并保留客户 profile 结束后的短暂 coast。该版解决了客户波形靠近问题，但不是专门处理间歇抖动。 |
| V9.2 第一轮抖动修正 | 发现 LOCK 阶段使用 `ADC_Feedback_ReadFastRaw()` 单次采样，且观测器会因为 ADC residual 过大进入 fast observer，静止时 ADC 噪声可被积分成速度；`static_lock_candidate` 只确认 2ms，容易被短噪声触发。V9.2 改为控制环始终用 `ADC_Feedback_ReadRaw()` 平均采样；移除“仅由 ADC residual 触发 fast observer”的路径；LOCK 候选确认从 `2` 次增加到 `8` 次。 |
| V9.2 意义 | 这一步针对 ADC 尖峰和观测器误判，降低静止时假速度、假 LOCK 的概率。它说明问题可能不是 PID 增益本身，而是反馈噪声经观测器和 LOCK 条件被放大为 H 桥输出。 |
| V9.3 第二轮抖动修正 | V9.3 在 V9.2 基础上进一步收敛 HOLD/LOCK：进入 HOLD 时清 `s_lcdm_servo_hold_deviation_count`；`static_lock_candidate` 不再由“观测速度超过阈值”触发，只由误差超过静态锁止带触发；HOLD/LOCK 灰区改成静默保持，不再轻易掉回 `event12` 小 duty 找点。版本字符串改为 `P50 V9.3`。 |
| V9.3 下载与短抓 | V9.3 构建通过，使用刘氏下载法下载成功，pyOCD 返回 `Successfully resumed device`。随后通过 SWD 写 `s_lcdm_pwm_source=1`，读回 `0x01`，确认进入内部 PWM。短抓目录 `build/vsg_latest_v93/`。 |
| V9.3 数据结论 | V9.3 的 539 行有效数据覆盖约 `63.1s`，`pwm_us` 只在 `1000/2000` 间切换，说明确为内部两点循环。多数周期尾段已进入 `event=2, duty=0`，但第 5 个 2000us 周期在过点后仍出现 `event18 -> event12 -> event13` 反复：典型点包括 `feedback=3194 error=-278 duty=-360 event18`，随后 `feedback=2794 error=118 duty=1000 event12`，再 `feedback=2815 error=101 duty=308 event18`。这说明 V9.3 已改善静止 HOLD 噪声，但过点后的 90-120 count 区域仍可能重新打大 duty。 |
| V9.4 最终当前修正 | V9.4 不再增加复杂计时窗口，采用更小的末端状态机修正：新增 `LCDM_SERVO_V3_OVERSHOOT_HOLD_BAND=128`；`v7_static_lock_band` 从 `Rel + Hbd*3` 扩为 `Rel + Hbd*7`，在当前默认 `Rel=16/Hbd=16` 下为 `128 count`；只要已经发生过 overshoot 且误差回到 `128 count` 内、速度低于 `brake_high_vel_q4+32`，直接进入 HOLD；HOLD/LOCK 灰区也按 `v7_static_lock_band` 静默保持。 |
| V9.4 同时保留的修正 | 保留 V9.2/V9.3 的 ADC 平均采样、移除 residual-only fast observer、LOCK 8 次确认、LOCK 只由持续误差触发、进入 HOLD 清确认计数、客户 profile 的 `12/4/3ms` 与 `930/360/220` 输出及 `24ms` coast gate。删除未使用的 `s_lcdm_servo_approach_brake_tail_count` 死变量，避免 Flash 空间不足。 |
| V9.4 编译下载 | V9.4 构建通过：`text=19680`、`data=672`、`bss=2336`，ELF 字符串确认 `P50 V9.4`。第一次下载遇到 DAPLink 通信层 `expected DAP_INFO` / `expected DAP_SWJ_CLOCK`，按现有刘氏下载法自动重试；最终 pyOCD 输出 `Successfully resumed device`，确认 V9.4 已下载。 |
| V9.4 有效 VSG | 先前 `build/vsg_latest_v94/` 出现全 0 数据，判定为无效采集，不作为验证依据。随后重新按 V9.4 reset vector resume，读 `g_servo_curve_frame` 确认 RAM 帧更新，再写 `s_lcdm_pwm_source=1` 并读回 `0x01`，重新采集到有效数据目录 `build/vsg_latest_v94_fresh/`。 |
| V9.4 数据结论 | `build/vsg_latest_v94_fresh/servo_curve_live.csv` 有效 536 行，覆盖约 `62.9s`。`pwm_us` 只在 `1000/2000` 间切换；完整 16 个动作段中，动作开始 `>1000ms` 的尾段全部为 `event=2, duty=0`；尾段非零 duty 数为 `0`；`event18=0`、`event16=0`。这直接对比 V9.3 第 5 段尾部仍有 `event12/event18`，说明 V9.4 已把“到点后过点反打再重新大 duty 找点”的主要通道压住。 |
| V9.4 长测复现 | 后续持续 VSG 目录 `build/vsg_watch_v94/` 中，V9.4 运行到约 900s 后复现来回摆动。数据确认仍是内部 `1000/2000`，不是外部 PWM；`raw_adc` 与 `feedback_adc` 同步跨动，不是单点 ADC 假跳。异常段在目标附近 `abs(error)=150..220 count` 时重新进入 `event12/event13`，并出现大量 `±1000 duty`，形成近目标限幅振荡。结论是 V9.4 的 `128 count` 末端 HOLD/静默带仍太窄。 |
| V9.5 小范围修正 | 按用户确认的小范围方案，只把末端静默带扩大：`LCDM_SERVO_V3_OVERSHOOT_HOLD_BAND` 从 `128` 改为 `240`；`v7_static_lock_band` 从 `Rel + Hbd*7` 改为 `Rel + Hbd*14`，默认同样为 `240 count`；版本字符串改为 `P50 V9.5`。其余客户 profile、ADC 平均采样、fast observer 修正、LOCK 8 次确认、LCDM/触屏/UART/引脚均不改。 |
| V9.5 当前结果 | V9.5 编译通过，ELF 字符串为 `P50 V9.5`，大小 `text=19680 data=672 bss=2336`。下载后内部两点 VSG 最近 60 秒统计中，`abs(error)<=240` 的近目标大 duty 次数为 `0`，用户目测反馈“目前没发现抖晃现象”。因此 V9.5 暂定为内部 PWM 稳定基准，下一步打开外部 PWM 输入测试。 |
| 当前判断 | 这次间歇性抖动的主因不是客户 PID 曲线本身，而是末端状态机对 ADC 噪声、观测速度、过点误差和 HOLD/LOCK 重新激活条件处理过激。ADC 噪声会通过单次采样或 fast observer 变成速度；过点后误差在几十到百余 count 时，旧逻辑仍可能 `event12/event18` 注入能量；内部固定 4 秒切点会在某些未完全收敛周期上叠加新命令，于是表现为运行一段时间后突然晃，再过一段时间恢复。 |
| 后续观察点 | 当前 V9.5 是内部 PWM 暂定稳定基准，但外部 PWM 测试必须继续看 VSG。若再次出现到点晃，优先检查 VSG 中 `event12/event18/event16` 是否重新出现在动作尾段；同时看 `raw_adc/feedback_adc/velocity_q4` 是否有异常跳动。如果尾段仍全是 `event=2, duty=0` 而机械仍晃，则要转向机械齿隙、电位器噪声、电源/PA0 供电稳定性检查，而不是继续改 PID。 |
| 文件和原则 | 本轮源码只改 `src/app_entry.c`。不要把无效全 0 的 `build/vsg_latest_v94/` 当验证数据；有效内部参考目录包括 `build/vsg_latest_v94_fresh/` 和 `build/vsg_watch_v94/`。VSG 后续固定覆盖 `build/vsg_watch_v94/`，不再每次新开目录/窗口。V9.5 应作为“内部 PWM 间歇抖动修正基准版”提交 GitHub 并打 tag。 |
