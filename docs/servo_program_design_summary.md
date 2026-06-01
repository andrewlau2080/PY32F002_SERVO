# PY32F002 舵机程序设计汇总

项目目录：`PY32F002_SERVO`

目标芯片：`PY32F002AL15S6TU`，SOP8，Cortex-M0+。

当前方案使用 `PY32F002A` 的 SOP8 型号，不是 `PY32F002B`。A 系列和 B 系列虽然都有 SOP8 料号，但 SOP8 引脚不同，不能作为同一 PCB 的直接替代料。

编译环境：Parallels `Ubuntu Linux`，`arm-none-eabi-gcc`，OpenPuya SDK `/opt/py32/PY32F0xx_Firmware`。

参考配置器资料：`KC9806 V2.5` 目录下的配置卡使用说明和配置参数说明。KC9806 的模式是电脑软件通过 USB 写入配置卡，配置卡再通过舵机 3PIN 信号线把参数写入舵机控制单片机。本工程按同样思路，把程序拆成固定底层和可变参数区。

## 0. 程序分层

| 层级 | 文件/模块 | 是否随产品固定 | 职责 | 后续升级方式 |
|---|---|---|---|---|
| 底层程序 | `board.c`、`adc_feedback.c`、`pwm_input.c`、`hbridge.c`、`app_it.c` | 固定 | ADC、PWM 输入、H 桥、定时器、中断、Flash 基础能力 | SWD 烧录整体固件 |
| 控制算法 | `servo_control.c` | 固定为主，参数驱动 | 目标映射、死区、增益、软启动、失信号、堵转保护 | 少量算法升级用 SWD |
| 可变参数区 | `servo_params.c/.h` | 可现场改变 | 保存不同马达、齿轮箱、机构阻力对应参数 | 未来配置卡或 LCD+按键模块写入 Flash 参数页 |
| 参数存储 | `ld/py32f002ax5_servo.ld` 预留最后 128B Flash | 固定 | 防止程序代码覆盖参数页 | 参数页独立擦写 |
| 外挂配置入口 | `Servo_Params_Save()` + `ServoComm` | 可扩展 | 单线配置协议、开发板 LCDM 或生产工装调用 | 通过 3PIN 信号线、USART1 LCDM 或专用工装写入 |
| 通讯包处理 | `servo_comm.c/.h` | 固定 | 处理 HELLO、读参数、写参数、读遥测、ACK/NACK | 后续接入 PA4 单线物理层 |
| 协议文件 | `docs/servo_config_protocol.md` | 固定跟踪 | 定义握手、帧格式、参数包、遥测包 | 上位机和 LCDM 模块按此实现 |
| 开发板 LCDM | `docs/lcdm_development_interface.md` | 开发/工装使用 | 用串口 HMI 显示遥测、编辑参数、生产写入 | 最终 SOP8 目标板不直接接 LCDM |
| TJC 屏适配 | `tjc_lcdm.c/.h` | 开发板启用 | USART1 连接陶晶驰串口屏，刷新控件和解析屏幕回包 | `SERVO_ENABLE_TJC_LCDM=1` 时编译启用 |

## 1. 控制目标

| 项目 | 当前设计值 | 说明 |
|---|---:|---|
| 输入 PWM 周期 | 约 20 ms / 50 Hz | 允许 3-30 ms 帧周期，用于兼容不同接收机 |
| 输入脉宽范围 | 1000-2000 us | 1000 us 左端，1500 us 中位，2000 us 右端 |
| 设计死区 | 约 1-3 us 对应 ADC 误差 | 程序用 ADC 死区实现，上板后按噪声调整 |
| 反馈方式 | 电位器 ADC | 不用马达运行时间估算位置 |
| 控制方式 | 位置闭环 | 输入脉宽映射目标 ADC，反馈 ADC 形成误差 |
| 保护方式 | 信号丢失、堵转保护 | 无有效 PWM 时停止输出；堵转时短暂停机恢复 |
| 参数调试方式 | Flash 参数块 | 先用默认值，后续配置器写入最后一页 Flash |

## 2. SOP8 引脚分配

| 芯片脚位 | MCU 功能 | 程序宏 | 当前用途 | 风险/备注 |
|---:|---|---|---|---|
| 1 | VCC | - | MCU 电源 | 3.3 V 或 5 V，需与仿真器 VTref 一致 |
| 2 | PA4 / PA10 | `BOARD_PWM_IN_PIN` | 外部 PWM 命令输入 | EXTI 双边沿测高电平宽度 |
| 3 | PA3 | `BOARD_POT_ADC_PIN` | 电位器 ADC 反馈 | ADC channel 3，需硬件低噪声布线 |
| 4 | PA14-SWC / PB3 | `BOARD_HB_IN4_PIN` | H 桥控制 4 | 与 SWCLK 共用，烧录时必须避免拖死 SWD |
| 5 | PA13-SWD | `BOARD_HB_IN3_PIN` | H 桥控制 3 | 与 SWDIO 共用，建议串阻和上电默认关断 |
| 6 | PA2 / PF2-NRST | `BOARD_HB_IN2_PIN` | H 桥控制 2 | 是否保留 NRST 需硬件确认 |
| 7 | PA1 | `BOARD_HB_IN1_PIN` | H 桥控制 1 | 软件 PWM 输出之一 |
| 8 | VSS | - | 地 | 接收机、马达驱动、电位器共地 |

## 2A. PY32F002A/B 兼容性记录

| 项目 | 当前 PY32F002A SOP8 | PY32F002B SOP8 | 对本项目影响 |
|---|---|---|---|
| 当前选型 | `PY32F002AL15S6TU` | 非当前目标 | 固件、链接脚本、pyOCD target 均按 A 系列 |
| SOP8 是否存在 | 有 | 有 | A 没有 SOP8 的判断不成立 |
| 引脚兼容性 | PA4/PA3/PA13/PA14/PA2/PA1 | PB1/PB0/PA2/PB6/PA6/PA7 | 不兼容，不能直接替换 |
| SWD | PA13/PA14 | PB6/PA2 | B 会改变调试脚和 H 桥脚规划 |
| Flash/SRAM | 20 KB / 3 KB | 24 KB / 3 KB | B Flash 稍大，但不抵消引脚不兼容问题 |
| 采购策略 | 第一选择 | 备选新 PCB | 若 A 供货/价格恶化，再单独开 B 版 PCB |

## 3. 程序模块表

| 模块 | 文件 | 主要职责 | 调试关注点 |
|---|---|---|---|
| 入口 | `src/app_entry.c` | 初始化板级、ADC、PWM 输入、H 桥、控制循环 | 确认启动后 H 桥默认无输出 |
| 板级 | `src/board.c` | HAL 初始化、微秒计时、TIM1 20 us 中断 | `Board_Micros()` 对 PWM 测量精度有直接影响 |
| PWM 输入 | `src/pwm_input.c` | PA4 双边沿中断，计算输入脉宽 | 示波器比对 1000/1500/2000 us |
| ADC 反馈 | `src/adc_feedback.c` | PA3 ADC 采样、多次平均、IIR 滤波 | 检查电位器噪声是否超过 1-3 us 等效范围 |
| H 桥 | `src/hbridge.c` | 四脚输出、1 kHz 软件 PWM、正反转/刹车/滑行 | 先空载限流验证方向，再接马达 |
| 控制 | `src/servo_control.c` | 脉宽到 ADC 映射、死区、比例输出、堵转保护 | 上板后调 `servo_config.h` 参数 |
| 参数 | `src/servo_params.c` | 默认参数、Flash 读取/校验/保存、KC9806 参数映射 | 后续配置卡协议会调用 `Servo_Params_Save()` |
| 通讯 | `src/servo_comm.c` | 参数帧、遥测帧、命令处理、CRC16 | PA4 物理收发确定后接入 |
| TJC LCDM | `src/tjc_lcdm.c` | 陶晶驰串口屏控件赋值、页面刷新、回包解析 | 开发板 USART1，目标 SOP8 默认关闭 |
| 中断 | `src/app_it.c` | SysTick、EXTI4_15、TIM1 update ISR | 中断优先级：PWM 输入高于马达 PWM |
| HAL MSP | `src/app_hal_msp.c` | ADC/TIM1 外设时钟和 NVIC | 与 SDK HAL 初始化配合 |
| 配置 | `inc/servo_config.h` | 所有可调控制参数 | 后续调试主要改此文件 |
| 编译 | `Makefile` | Ubuntu GCC 构建 `.elf/.hex/.bin` | 依赖 `/opt/py32/PY32F0xx_Firmware` |
| 链接 | `ld/py32f002ax5_servo.ld` | 预留 Flash 最后一页 | 参数区不被代码覆盖 |

## 4. 数据模型

| 数据 | 类型/范围 | 来源 | 用途 |
|---|---|---|---|
| `input_us` | 默认 900-2100 us 识别，中心 1500 us | PA4 外部 PWM | 代表目标位置 |
| `target_adc` | 默认 600-3495 counts，可带 Neutral/Range/Inverter | `Servo_Control_MapPulseToAdc()` | 目标电位器位置 |
| `feedback_adc` | 0-4095 counts | PA3 ADC | 实际电位器位置 |
| `error_adc` | signed ADC counts | `target_adc - feedback_adc` | 控制方向和速度 |
| `motor_duty` | -850 到 +850 | 比例控制输出 | 正负代表方向，绝对值代表 PWM 占空比 |
| `state` | `NO_SIGNAL/HOLD/DRIVE/STALL_PROTECT` | 控制器状态机 | 调试判断当前行为 |
| `ServoParams` | Flash 参数结构体 | 默认值或最后一页 Flash | 统一保存不同舵机型号的调机参数 |
| `ServoTelemetry` | 实时状态结构体 | `Servo_Comm_FillTelemetry()` | 上位机/LCDM 读回输入、位置、输出和参数 CRC |

## 5. 可调参数映射

| 调试项目 | 本工程字段/标志 | 默认值 | 当前作用 | 上板调试意义 |
|---|---|---:|---|---|
| Dead Band | `deadband_us`、`deadband_count_min` | 2 us / 8 counts | 最小响应变化，换算成 ADC 死区 | 决定 1-3 us 响应与抖动平衡 |
| Stretcher | `stretcher_q8` | 384 | 大误差区误差放大比例，Q8 格式 | 马达/齿轮阻力大时提高，振荡时降低 |
| Max.Duty | `max_duty` | 850 | 最大驱动占空比，满量程 1000 | 限制电机力度、电流和冲击 |
| Boost | `boost_duty` | 160 | 起动补偿占空比 | 克服静摩擦，过大易过冲 |
| Pulse Lower | `pulse_lower_us` | 900 us | 可识别最小输入脉宽 | 兼容配置器/接收机输入范围 |
| Pulse High | `pulse_high_us` | 2100 us | 可识别最大输入脉宽 | 兼容配置器/接收机输入范围 |
| Left Range | `left_range_count` | 0 | 调整中心到左端行程 | 适配齿轮箱角度和机械限位 |
| Right Range | `right_range_count` | 0 | 调整中心到右端行程 | 适配齿轮箱角度和机械限位 |
| Neutral | `neutral_offset_count` | 0 | 中位偏移 | 修正装配和电位器中点误差 |
| Drive Frequency | `drive_frequency_hz` | 1000 Hz | 软件 PWM 驱动频率 | 后续可试 330 Hz、较高频等马达声音/效率差异 |
| Speed Adj | `forward_speed_q8`、`reverse_speed_q8` | 256/256 | 正反转速度比例 | 修正左右速度不一致 |
| Inverter | `SERVO_PARAM_FLAG_INVERTER` | 关闭 | 目标方向反转 | 同一 PCB 适配相反机构 |
| OL Protect | `SERVO_PARAM_FLAG_OL_PROTECT` | 开启 | 堵转/过载保护 | 防止马达和驱动过热 |
| 100% PWM | `SERVO_PARAM_FLAG_PWM_100_PERCENT` | 关闭 | 预留满占空比策略 | 后续按驱动能力决定是否开放 |
| Narrow Band | `SERVO_PARAM_FLAG_NARROW_BAND` | 关闭 | 预留窄频模式标志 | 可映射为更窄输入范围或更高灵敏度 |
| Soft Start | `SERVO_PARAM_FLAG_SOFT_START`、`soft_start_step_count` | 开启 / 8 counts/ms | 上电或目标变化时目标缓慢移动 | 降低上电冲击和齿轮冲击 |
| Lose PPM Enable + Lock | `LOSE_PPM_ENABLE` + `LOSE_PPM_LOCK` | Lock 开启，Enable 关闭 | 失信号锁原位置；两者都开则回 1500 us | 按产品规格选择失信号行为 |
| 型号/Profile | `model_id`、`profile_id` | 0 / 0 | 区分金属齿、塑胶齿和客户型号模板 | 避免不同齿轮箱套用同一套参数 |
| 上电防乱跑 | `startup_delay_ms`、`startup_input_stable_count`、`startup_input_stable_us`、`startup_adc_stable_count`、`startup_adc_stable_band_count` | 200 ms / 5 / 3 us / 5 / 4 counts | 上电后等待输入和反馈稳定才进入闭环 | 解决插电突然跑到端点或跳动 |
| 上电追目标速度 | `startup_step_count` | 4 counts/ms | 第一次闭环追目标时使用更慢的目标斜坡 | 防止当前位和输入位差很大时冲击 |
| ADC 抗干扰 | `adc_sample_count`、`adc_filter_shift`、`adc_jump_limit_count`、`adc_noise_band_count` | 4 / 3 / 0 / 4 | 采样平均、IIR、跳变限幅和噪声评估 | 针对 AD 超过约 1V 后抖动的问题 |
| 电源监测 | `vdd_nominal_mv`、`vdd_warn_drop_mv`、`vdd_noise_band_mv`、`vdd_sample_interval_ms` | 3300 / 250 / 80 / 20 ms | 通过内部 VREFINT 估算 MCU/ADC 供电变化 | 判断 ADC 抖动是否来自 3.3V 跌落、地弹或电机干扰 |
| 到位保持 | `hold_mode`、`hold_exit_band_count`、`hold_brake_time_ms`、`hold_settle_ms` | 短刹后滑行 / 16 / 8 ms / 20 ms | 到位后用滞回和短刹车降低静态电流 | 解决到位 200-300mA、嗞嗞声、发热 |
| 近目标控制 | `close_error_count`、`close_stretcher_q8`、`close_boost_duty` | 24 / 192 / 60 | 小误差区低增益低起动补偿 | 减少锁附时正反抖动 |
| 接近目标控制 | `approach_error_count`、`approach_stretcher_q8`、`approach_boost_duty` | 120 / 320 / 120 | 接近目标区中等增益 | 改善末端慢跨和晃过去 |
| 反向保护 | `reverse_pause_ms`、`reverse_brake_ms` | 8 ms / 3 ms | 正反切换前短暂停顿/短刹车 | 降低未到位反向时的冲击电流 |

## 6. 当前默认参数

| 参数宏 | 当前值 | 作用 | 后续调试建议 |
|---|---:|---|---|
| `SERVO_INPUT_MIN_US` | 1000 | 输入最小脉宽 | 按规格和实测接收机调整 |
| `SERVO_INPUT_CENTER_US` | 1500 | 输入中位 | 机械中位校准后确认 |
| `SERVO_INPUT_MAX_US` | 2000 | 输入最大脉宽 | 按左右极限防撞调整 |
| `SERVO_ADC_MIN_COUNT` | 600 | 左端对应 ADC | 上板后实测电位器左极限 |
| `SERVO_ADC_CENTER_COUNT` | 2048 | 中位 ADC | 上板后实测机械中位 |
| `SERVO_ADC_MAX_COUNT` | 3495 | 右端对应 ADC | 上板后实测电位器右极限 |
| `SERVO_ADC_DEADBAND_COUNT` | 8 | 控制死区 | 目标 1-3 us，需按 ADC 噪声重算 |
| `SERVO_ADC_FILTER_SHIFT` | 3 | IIR 滤波强度 | 噪声大则加大，响应慢则减小 |
| `SERVO_CONTROL_KP_NUM/DEN` | 3/2 | 比例增益 | 先低速限流调，再提高响应 |
| `SERVO_CONTROL_MIN_DUTY` | 160 | 起动补偿 | 克服齿轮箱静摩擦 |
| `SERVO_CONTROL_MAX_DUTY` | 850 | 最大驱动 | 限制电流和冲击 |
| `SERVO_STALL_TIME_MS` | 300 | 堵转判定时间 | 需结合马达电流和齿轮箱强度 |
| `SERVO_STALL_RECOVERY_MS` | 500 | 堵转恢复等待 | 防止反复冲击 |

说明：上表是编译期保底参数；实际运行优先使用 `ServoParams`。如果最后一页 Flash 没有合法参数或 CRC 错误，程序自动使用默认参数。

## 6A. 参数区 v2 新增调试项

| 历史问题 | 对应参数 | 当前程序是否已接入 | 调试方法 |
|---|---|---|---|
| 上电乱跑 | `startup_delay_ms`、`startup_input_stable_count`、`startup_adc_stable_count`、`startup_step_count` | 已接入 | 先把等待和稳定计数调大，确认不上电乱动后再缩短 |
| AD 抖动 | `adc_sample_count`、`adc_filter_shift`、`adc_jump_limit_count` | 已接入 | 用遥测看 raw/filter ADC，先找硬件噪声，再调滤波 |
| 3.3V 供电波动 | `vdd_nominal_mv`、`vdd_warn_drop_mv`、`vdd_noise_band_mv`、`vdd_sample_interval_ms` | 已接入遥测 | 同步看 `feedback_adc_raw` 和 `vdd_mv`，区分电位器变化和供电/地线扰动 |
| 到位电流/嗞嗞声 | `hold_mode`、`hold_exit_band_count`、`hold_brake_time_ms` | 已接入 | 从短刹后滑行开始试，若保持力不足再增加刹车时间 |
| 锁附正反抖 | `close_error_count`、`close_stretcher_q8`、`close_boost_duty` | 已接入 | 降低小误差区增益和 Boost，避免在齿隙内反复打方向 |
| 末端慢跨 | `approach_error_count`、`approach_stretcher_q8`、`approach_boost_duty` | 已接入 | 调整接近目标区，让速度下降但不拖尾 |
| 反向冲击 | `reverse_pause_ms`、`reverse_brake_ms` | 已接入 | 先 8/3 ms 起步，按电流尖峰和速度再调 |
| 金属齿/塑胶齿差异 | `model_id`、`profile_id` | 参数已预留 | 同一固件按齿轮箱写入不同模板 |
| ADC 噪声评估 | `adc_noise_band_count` | 预留 | 后续可用于遥测报警或自动放宽死区 |
| 到位稳定时间 | `hold_settle_ms` | 预留 | 后续可用于更严格的到位判定 |

## 7. 状态机

| 状态 | 进入条件 | 输出动作 | 退出条件 |
|---|---|---|---|
| `NO_SIGNAL` | PWM 超过 100 ms 无有效脉宽 | H 桥滑行停止 | 恢复有效 PWM |
| `HOLD` | 反馈进入 ADC 死区 | 小误差刹车或滑行 | 误差超过死区 |
| `DRIVE` | 有信号且误差超过死区 | 按误差方向输出 PWM | 到位、失信号或堵转 |
| `STALL_PROTECT` | 大占空比但反馈长期不变 | 停止输出 500 ms | 恢复等待结束 |

## 8. 外挂配置器后续设计

| 阶段 | 设备形态 | 舵机侧工作 | 说明 |
|---|---|---|---|
| 当前阶段 | SWD + 固件默认参数 | 固件可读取默认参数和 Flash 参数 | 先完成舵机闭环控制和参数结构 |
| 配置卡阶段 | PC 软件 + USB 配置卡 MCU | 配置卡通过 3PIN 信号线进入参数写入流程 | 类似 KC9806 的 `Read Config` / `Write Config` |
| 独立模块阶段 | 小 MCU + LCD + 按键 | 模块直接读写 `ServoParams` | 不依赖电脑，适合生产线或售后 |
| 开发板 LCDM 阶段 | 开发板 + UART HMI 智能屏 | USART1 读取遥测、写入完整参数包 | 不受 SOP8 IO 限制，屏幕负责页面和条形图 |
| 量产阶段 | 工装批量写入 | 写入型号参数、校准 ADC 左中右点 | 每种马达/齿轮箱/机构保存一套参数 |

## 9. 编译与调试状态

| 日期 | 项目 | 结果 | 备注 |
|---|---|---|---|
| 2026-05-31 | 创建独立工程 `PY32F002_SERVO` | 完成 | 位于 MOTOR 目录下 |
| 2026-05-31 | Ubuntu GCC 编译 | 通过 | 生成 `.elf/.hex/.bin` |
| 2026-05-31 | 程序大小 | Flash 7004 B，RAM 1288 B | 已加入参数层和通讯包处理，仍适合 PY32F002A 20KB/3KB |
| 2026-05-31 | KC9806 参数参考 | 完成 | 已映射到 `ServoParams` |
| 2026-05-31 | Flash 参数页 | 完成 | 链接脚本预留最后 128B，CRC 校验 |
| 待仿真器到货 | SWD 在线烧录 | 待做 | 推荐 CMSIS-DAP/DAPLink 或 J-Link |
| 待样板 | ADC 噪声评估 | 待做 | 决定 1-3 us 死区是否可达 |
| 待样板 | H 桥方向与限流测试 | 待做 | 必须先限流、空载验证 |
| 待接口定义 | 配置卡单线协议 | 待做 | 需要定义 PA4 普通 PWM 与配置通信的进入条件 |
| 2026-05-31 | 配置通讯协议草案 | 完成 | 独立文件 `docs/servo_config_protocol.md` |
| 2026-05-31 | 通讯包处理模块 | 完成 | `servo_comm.c/.h`，物理层待接入 |
| 2026-06-01 | 开发板 LCDM 方案 | 完成草案 | LCDM 不受 SOP8 限制，推荐 USART1 串口 HMI 智能屏 |
| 2026-06-01 | TJC 陶晶驰串口屏驱动骨架 | 完成 | 默认构建关闭；开发板构建 `SERVO_ENABLE_TJC_LCDM=1`，Flash 14592 B，RAM 1524 B |

## 10. 关键风险

| 风险 | 影响 | 对策 |
|---|---|---|
| PA13/PA14 与 SWD 共用 | 可能影响烧录/在线调试 | H 桥输入加串阻，硬件默认关断，保留 SWD 测试点 |
| ADC 噪声 | 1-3 us 等效死区可能抖动 | 电位器滤波、模拟地布线、ADC 多采样和 IIR |
| 四脚 H 桥定义未最终确认 | 方向或刹车方式可能需改 | 上板前确认驱动芯片/分立桥逻辑表 |
| 马达静摩擦和齿隙 | 小误差可能无法动作或来回摆动 | 调 `MIN_DUTY`、死区和滤波 |
| 堵转电流 | 烧毁马达或驱动 | 先限流调试，必要时加电流检测或热保护 |
| 配置通信与普通 PWM 共用 PA4 | 可能误进入配置或影响接收机信号 | 后续协议必须有明确握手，例如超出普通 PWM 范围的长低/长高序列 |
| 裸屏图形代码过大 | 占用 Flash/RAM，影响控制程序 | LCDM 首选 UART HMI 智能屏，把页面和图形交给屏幕端 |
