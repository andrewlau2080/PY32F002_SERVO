# TJC 陶晶驰串口屏接入说明

版本：v0.1

目标：开发板使用 TJC 陶晶驰串口 HMI 作为 LCDM，显示舵机遥测、位置图示和部分参数。最终 SOP8 目标板默认不启用 LCDM。

参考：用户指定的 TJC 工程创建页面 `http://wiki.tjc1688.com/start/create_project/create_project_9.html#id1`。本工程按常见 TJC/串口屏指令方式实现：MCU 发送控件赋值 ASCII 指令，每条指令以 `0xFF 0xFF 0xFF` 结束。

## 1. 固件开关

默认目标板构建不启用 TJC LCDM：

```bash
./tools/build_in_ubuntu.sh
```

开发板启用 TJC LCDM 时：

```bash
./tools/build_in_ubuntu.sh SERVO_ENABLE_TJC_LCDM=1
```

当前验证结果：

| 构建 | Flash | RAM | 说明 |
|---|---:|---:|---|
| 默认目标板 | 8364 B | 1368 B | TJC 模块为空函数，不占 UART |
| TJC 开发板 | 14592 B | 1524 B | 启用 USART1、UART HAL、TJC 刷新逻辑 |

## 2. 硬件连接

| TJC 屏 | 开发板 PY32F002A | 说明 |
|---|---|---|
| TX | PA3 / USART1_RX | 开发板专用；最终 SOP8 板 PA3 仍是电位器 ADC |
| RX | PA2 / USART1_TX | 开发板专用；最终 SOP8 板 PA2 仍是 H 桥控制 |
| GND | GND | 必须共地 |
| 5V/VCC | 屏幕电源 | 不建议由小 LDO 直接硬供背光 |

串口参数：

| 项目 | 值 |
|---|---:|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验 | None |
| 停止位 | 1 |
| 刷新周期 | 100 ms |

## 3. 页面规划

当前固件先支持两个页面：

| 页面 | page id | 用途 |
|---|---:|---|
| 首页 | 0 | 实时遥测、位置条、电机输出、状态 |
| 参数页 | 1 | 显示常用输入/ADC/控制参数 |

屏幕端切页后，建议发送以下 ASCII 指令给 MCU，并以 `0xFF 0xFF 0xFF` 结束：

```text
page=0
page=1
refresh
```

固件也兼容 TJC 触摸事件中常见的 `0x65 page_id component_id event` 帧，用于更新当前页面并强制刷新。

## 4. 首页控件命名

TJC 工程中请按下表建立控件。名称必须一致，否则 MCU 发出的赋值命令不会生效。

| 控件名 | 类型建议 | MCU 发送内容 |
|---|---|---|
| `n0` | 数字 | 当前输入脉宽 `input_us` |
| `n1` | 数字 | 目标 ADC `target_adc` |
| `n2` | 数字 | 反馈 ADC `feedback_adc` |
| `n3` | 数字 | 误差 `error_adc` |
| `n4` | 数字 | 电机输出 `motor_duty`，正负表示方向 |
| `n5` | 数字 | 估算 VDD，单位 mV |
| `j0` | 进度条 | 目标位置百分比 |
| `j1` | 进度条 | 电位器反馈位置百分比 |
| `j2` | 进度条 | 电机输出绝对值百分比 |
| `t0` | 文本 | 控制状态：`NO SIG`、`HOLD`、`DRIVE`、`STALL` |
| `t1` | 文本 | 参数状态：`CRC OK` 或 `CRC BAD` |

位置条建议：

| 条形图 | 显示 |
|---|---|
| `j0` | 输入目标位置 |
| `j1` | 电位器实际反馈位置 |
| `j2` | 马达驱动力度 |

这样可以直接看到“目标在哪里、反馈电位器转到哪里、马达正在用多大力追”。

## 5. 参数页控件命名

| 控件名 | 类型建议 | MCU 发送内容 |
|---|---|---|
| `n10` | 数字 | `pulse_lower_us` |
| `n11` | 数字 | `pulse_center_us` |
| `n12` | 数字 | `pulse_high_us` |
| `n13` | 数字 | `adc_min_count` |
| `n14` | 数字 | `adc_center_count` |
| `n15` | 数字 | `adc_max_count` |
| `n16` | 数字 | `deadband_us` |
| `n17` | 数字 | `deadband_count_min` |
| `n18` | 数字 | `max_duty` |
| `n19` | 数字 | `boost_duty` |

本版先做显示和页面刷新，写参数流程下一步再接：屏幕输入数值后，MCU 需要构造完整 `ServoParams`，校验 CRC32，再调用 `Servo_Params_Save()`，然后读回确认。

## 6. MCU 发送指令示例

固件会发送类似以下指令，每条后面都自动追加 `0xFF 0xFF 0xFF`：

```text
n0.val=1500
n1.val=2048
n2.val=2052
n3.val=-4
j0.val=50
j1.val=51
t0.txt="HOLD"
t1.txt="CRC OK"
```

## 7. 已接入代码

| 文件 | 内容 |
|---|---|
| `inc/tjc_lcdm.h` | TJC LCDM 接口和编译开关 |
| `src/tjc_lcdm.c` | USART1 初始化、TJC 指令发送、遥测刷新、屏幕回包解析 |
| `src/app_entry.c` | 初始化 TJC LCDM，并在主循环中刷新 |
| `src/app_it.c` | 开启 TJC 时增加 `USART1_IRQHandler()` |
| `src/app_hal_msp.c` | 开启 TJC 时配置 PA2/PA3 USART1 |
| `inc/py32f0xx_hal_conf.h` | 开启 TJC 时启用 HAL UART |
| `Makefile` | 增加 `SERVO_ENABLE_TJC_LCDM` 开关和 UART HAL 源文件 |

## 8. 注意事项

| 项目 | 要求 |
|---|---|
| 不要全屏高频刷新 | 当前按 100 ms 刷新变量，屏幕本身负责重绘 |
| 不要在目标 SOP8 板启用 | PA2/PA3 在目标板已有 H 桥/ADC 用途 |
| 先做显示再做写参数 | 防止未验证 UI 时误写 Flash |
| 写参数必须读回 | 后续保存参数必须 `WRITE_PARAMS` 后 `READ_PARAMS` 比对 CRC |
| 串口电平确认 | TJC 屏 RX/TX 必须与 PY32 3.3V 逻辑兼容 |
| 背光供电独立 | 屏幕供电和 MCU 共地，避免背光电流污染 ADC |
