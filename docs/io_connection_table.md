# PY32F002 SOP8 PCB Layout IO 表

版本：v0.3

用途：给最终 SOP8 舵机控制板画原理图和 PCB Layout 使用，可直接发给 PCB/PAC Layout 工程师。TSSOP20 只作为调试板/工装对照，不作为最终产品板。

## 1. SOP8 最终板引脚表

| SOP8 脚 | MCU 脚名 | PCB 网名 | 连接对象 | Layout 要点 |
|---:|---|---|---|---|
| 1 | VCC | `VCC_3V3` | MCU 3.3V 电源 | 0.1uF 去耦贴近 1/8 脚；同时引出 VTref 测试点 |
| 2 | `PA4 / PA10` | `PWM_IN_CFG` | 舵机信号线 S | 正常 PWM 输入；后续单线写参数也用此脚 |
| 3 | `PA3` | `POT_ADC` | 电位器滑臂 | ADC 线短、远离马达线；RC 滤波靠近 MCU |
| 4 | `PA14-SWC / PB3` | `HB_NG2_SWCLK` | H 桥 `CTR_NG2` | 同时是 SWCLK；测试点接 MCU 近端，H 桥支路串阻后再出去 |
| 5 | `PA13-SWD` | `HB_CP2_SWDIO` | H 桥 `CP2 / CPG1` | 同时是 SWDIO；测试点接 MCU 近端，H 桥支路串阻后再出去 |
| 6 | `PA2 / PF2-NRST` | `HB_NG1` | H 桥 `CTR_NG1` | 最终按 `PA2` GPIO 用；不要再接强复位电路 |
| 7 | `PA1` | `HB_CP1` | H 桥 `CP1 / CPG2` | H 桥右上臂 P-MOS 控制 |
| 8 | VSS | `GND` | 系统地 | MCU、电位器、接收机、马达驱动、SWD 共地 |

SOP8 最终只有 6 个可用 IO，已经全部分配：`PA4`、`PA3`、`PA1`、`PA2`、`PA13`、`PA14`。不要再给最终板增加 LCD、按键、LED 或额外通讯脚。

## 2. H 桥控制网名

| 固件信号 | SOP8 脚 | MCU 端口 | PCB 网名 | H 桥网名 | MOS 位置 |
|---|---:|---|---|---|---|
| `BOARD_HB_IN1` | 7 | `PA1` | `HB_CP1` | `CP1 / CPG2` | 右上臂 P-MOS |
| `BOARD_HB_IN2` | 6 | `PA2` | `HB_NG1` | `CTR_NG1` | 左下臂 N-MOS |
| `BOARD_HB_IN3` | 5 | `PA13` | `HB_CP2_SWDIO` | `CP2 / CPG1` | 左上臂 P-MOS |
| `BOARD_HB_IN4` | 4 | `PA14` | `HB_NG2_SWCLK` | `CTR_NG2` | 右下臂 N-MOS |

Layout 注意：

| 项目 | 要求 |
|---|---|
| SWD 共脚 | `PA13/PA14` 既是 H 桥控制，又是 SWD 下载脚；必须保留 SWD 测试点 |
| 测试点位置 | `SWDIO/SWCLK` 测试点放在 MCU 脚近端；H 桥控制支路经过串阻后再接驱动电路 |
| 默认状态 | H 桥输入网络上电默认不能误导通；必要时用电阻保证关断 |
| 马达电流 | 马达电流回路不要经过 MCU 地、ADC 地；电源地回流要短而粗 |
| ADC 抗干扰 | `POT_ADC` 不要与马达两端、H 桥栅极控制线长距离平行 |

## 3. 调试点与生产测试点

### 3.1 必留测试点

| 测试点丝印 | 连接位置 | 类型 | 用途 | Layout 指示 |
|---|---|---|---|---|
| `VCC` | SOP8 1 脚 `VCC_3V3` | 必留 | SWD VTref、电源测量 | 放在 SWD 测试点旁边 |
| `GND` | SOP8 8 脚 `GND` | 必留 | SWD GND、示波器地 | 至少 1 个靠近 SWD，最好另留 1 个靠近马达/电源 |
| `SWDIO` | SOP8 5 脚 `PA13` 近端 | 必留 | 下载、读写 Flash、在线调试 | 必须接在 MCU 脚近端，位于 H 桥串阻之前 |
| `SWCLK` | SOP8 4 脚 `PA14` 近端 | 必留 | 下载、读写 Flash、在线调试 | 必须接在 MCU 脚近端，位于 H 桥串阻之前 |
| `PWM` | SOP8 2 脚 `PWM_IN_CFG` | 必留 | PWM 输入、单线写参数、量产校准 | 可与舵机信号线同网，方便工装顶针接触 |
| `ADC` | SOP8 3 脚 `POT_ADC` | 必留 | 测电位器电压、ADC 噪声、端点校准 | 放在 RC 滤波后、MCU ADC 输入近端 |

建议 SWD 顶针排布：

| 顺序 | 丝印 | 信号 |
|---:|---|---|
| 1 | `VCC` | 目标板 3.3V / VTref |
| 2 | `GND` | 地 |
| 3 | `DIO` | `SWDIO / PA13` |
| 4 | `CLK` | `SWCLK / PA14` |

若板面允许，`PWM` 和 `ADC` 放在 SWD 四个测试点附近，形成 6 点工装区：`VCC, GND, DIO, CLK, PWM, ADC`。

### 3.2 可选测试点

| 测试点丝印 | 连接 | 用途 | 备注 |
|---|---|---|---|
| `CP1` | `HB_CP1` | 看右上臂 P-MOS 控制波形 | 空间不足可不留 |
| `NG1` | `HB_NG1` | 看左下臂 N-MOS 控制波形 | 空间不足可不留 |
| `CP2` | `HB_CP2_SWDIO` 串阻后 | 看左上臂 P-MOS 控制波形 | 不能替代 `SWDIO` 测试点 |
| `NG2` | `HB_NG2_SWCLK` 串阻后 | 看右下臂 N-MOS 控制波形 | 不能替代 `SWCLK` 测试点 |

### 3.3 调试点 Layout 规则

| 项目 | 要求 |
|---|---|
| 丝印 | 必须有清楚丝印：`VCC`、`GND`、`DIO`、`CLK`、`PWM`、`ADC` |
| 位置 | SWD 四点放在同一边或同一区域，便于弹簧针一次压接 |
| 焊盘 | 用圆形或长圆形裸铜测试 pad；不要盖阻焊；尺寸按工厂顶针能力确定 |
| 顺序 | 不要把 `DIO` 和 `CLK` 交换；若空间允许，按 `VCC-GND-DIO-CLK` 顺序排 |
| 共脚保护 | `PA13/PA14` 测试点必须在 MCU 侧，H 桥支路必须经过串阻，避免驱动电路影响下载 |
| 地参考 | `ADC` 测试时需要就近 `GND`；若 SWD 区离 ADC 很远，ADC 附近再加一个 `GND` 测试点 |
| 量产写参 | `PWM` 测试点必须能被工装接触，用于后续 PA4 单线写参数和读回 CRC |

## 4. TSSOP20 调试板对照

| 功能 | SOP8 最终脚 | TSSOP20 调试脚 | 说明 |
|---|---|---|---|
| PWM 输入/配置 | `PA4`，SOP8 2 脚 | `PA4`，TSSOP20 15 脚 | 同一功能 |
| 电位器 ADC | `PA3`，SOP8 3 脚 | `PA3`，TSSOP20 14 脚 | 同一功能 |
| H 桥 `CP1 / CPG2` | `PA1`，SOP8 7 脚 | `PA1`，TSSOP20 12 脚 | 同一功能 |
| H 桥 `CTR_NG1` | `PA2`，SOP8 6 脚 | `PA2`，TSSOP20 13 脚 | 同一功能 |
| H 桥 `CP2 / CPG1` | `PA13`，SOP8 5 脚 | `PA6`，TSSOP20 6 脚 | 调试板临时代替，避免占 SWDIO |
| H 桥 `CTR_NG2` | `PA14`，SOP8 4 脚 | `PB0`，TSSOP20 17 脚 | 调试板临时代替，避免占 SWCLK |
| LCDM TX/RX | SOP8 不接 | `PA7/PB2` | 只给调试板/工装使用 |
| SOP8 写参输出 | 接 SOP8 `PA4` | `PB1`，TSSOP20 19 脚 | 工装经 1k-4.7k 串阻接 SOP8 `PA4` |

最终 SOP8 Layout 以第 1、2、3 节为准；第 4 节只用于调试板和工装接线。
