# TJC 陶晶驰串口屏接入说明

版本：v0.1

目标：TSSOP20 调试板使用 TJC 陶晶驰串口 HMI 作为 LCDM，显示舵机遥测、位置图示和部分参数。最终 SOP8 目标板默认不启用 LCDM；调好的参数后续通过 SOP8 的 PA4/PWM 输入口写入。

参考：用户指定的 TJC 工程创建页面 `http://wiki.tjc1688.com/start/create_project/create_project_9.html#id1`。本工程按常见 TJC/串口屏指令方式实现：MCU 发送控件赋值 ASCII 指令，每条指令以 `0xFF 0xFF 0xFF` 结束。

## 1. 固件开关

默认目标板构建不启用 TJC LCDM：

```bash
./tools/build_in_ubuntu.sh
```

TSSOP20 调试板启用 TJC LCDM 时：

```bash
./tools/build_in_ubuntu.sh SERVO_ENABLE_TJC_LCDM=1
```

TSSOP20 调试板如需避开 `PA2/PA3` 并同时保留右半桥调试，可使用临时接线：

```bash
./tools/build_in_ubuntu.sh SERVO_ENABLE_TJC_LCDM=1 SERVO_TJC_UART_ALT_PINS=1 BOARD_TSSOP20_DEBUG_PINS=1
```

当前验证结果：

| 构建 | Flash | RAM | 说明 |
|---|---:|---:|---|
| 默认 SOP8 目标板 | 8328 B | 1368 B | TJC 模块为空函数，不占 UART |
| TSSOP20 + TJC 调试板 | 14732 B | 1524 B | 启用 USART1、UART HAL、TJC 刷新逻辑 |

## 2. 硬件连接

完整 IO 接线见 `docs/io_connection_table.md`。

默认 USART1 接线：

| TJC 屏 | PY32F002A USART1 默认脚 | 说明 |
|---|---|---|
| TX | PA3 / USART1_RX | 仅用于早期串口验证；最终 SOP8 板 PA3 仍是电位器 ADC |
| RX | PA2 / USART1_TX | 仅用于早期串口验证；最终 SOP8 板 PA2 仍是 H 桥控制 |
| GND | GND | 必须共地 |
| 5V/VCC | 屏幕电源 | 不建议由小 LDO 直接硬供背光 |

TSSOP20 临时 LCDM 接线，编译时需加 `SERVO_TJC_UART_ALT_PINS=1`：

| TJC 屏 | TSSOP20 调试板 PY32F002A | 说明 |
|---|---|---|
| TX | PB2 / USART1_RX | 避开 PA3 电位器 ADC |
| RX | PA7 / USART1_TX | 避开 PA2 H 桥控制 |
| GND | GND | 必须共地 |
| 5V/VCC | 屏幕电源 | 屏幕背光不要由目标小 LDO 硬供 |

`PB0` 不作为 LCDM 串口脚使用；当前核对结果是它适合做 `TIM1_CH2N` 或普通 GPIO。为方便接线，TSSOP20 临时马达调试可以用 `PB0` 接右下臂 `CTR_NG2`，把 `PA7` 留给 LCDM 串口 TX。

串口参数：

| 项目 | 值 |
|---|---:|
| 波特率 | 230400 |
| 数据位 | 8 |
| 校验 | None |
| 停止位 | 1 |
| 常规页刷新周期 | 100 ms |
| R01诊断动态值刷新间隔 | 250 ms，且仅值/颜色改变时重绘 |

### 2.1 LCDM整页直绘刷新顺序（固定规则）

**高频易错项（开始任何LCDM工作前先核对）**：运行时自绘覆盖旧TJC页面时，必须隐藏旧页面控件并屏蔽其触摸事件；仅停`ref`刷新不够。此问题已在R54实板确认修复，未做此检查曾导致触屏后花屏。凡涉及LCDM页面、切页、触摸或刷新初始化，都要保留并复核这一点。

对于通过 USART 发送 `page/cls/fill/xstr` 直接绘制整页、覆盖屏端既有页面背景的诊断 UI，严格按以下顺序：

1. 发送 `page <id>` 切到目标页并等待页面切换完成。
2. 发送 `ref_stop` 停止**当前页**控件刷新。必须在切页之后发送，因为切页会恢复页面刷新。
3. 发送 `cls`、`fill`、`xstr` 等整页绘图命令。
4. 全部绘图命令完成后发送 `ref_star`，提交画面。
5. 若要遮住屏端原有控件/背景，在自绘前发送 `vis 255,0` 隐藏当前页所有控件、`tsw 255,0` 关闭原控件触摸，再启用`sendxy=1`接收自定义坐标；提交后立即再次发送 `ref_stop`，防止页面刷新覆盖直绘层。后续动态值仍通过 USART 直接写入对应区域。R54已在TSSOP20实板确认触摸后不再花屏。

不要在切页前发送`ref_stop`（换页后会失效）；也不要省略`ref_star`提交步骤。覆盖原页面时，`ref_stop`不能代替隐藏原控件；触摸事件可能使原控件外观重绘。R54按上述规则实现，已实板确认触摸显示正常。TJC指令语义参考[Nextion/TJC指令集](https://forum.arduino.ir/attachments/3083d1477725352-nmaishgrhaaye-hmi-nextion-instruction-set-itead-wiki.pdf)。

### 2.2 触摸期间的动态页刷新（固定规则）

- 整页、标题、背景、按钮只在首次显示或真正切页时绘制；禁止每次触摸或每个主循环重画整页。
- 遥测值缓存上次显示值，只在值或显示颜色改变时重绘对应数值区域；无变化时不发UART命令，避免每100ms无条件重发整组`xstr`。
- 诊断页采用有界刷新间隔；R52设置250ms最小间隔。触摸后只更新必要字段，不重画页面结构。新增页面元素或触摸反馈时也要保持局部更新。

依据：`docs/lcdm_screen_layout_plan.md` 2026-06-16参数页触摸闪屏修复记录；R49–R54原页背景覆盖及修正见`docs/r01_validation_20261110.md`。两类问题不同：触摸只局部刷新解决MCU串口重画过量；隐藏旧控件、停其触摸处理并抑制页面刷新，解决屏端原页面覆盖。

## 3. 页面规划

当前固件先支持两个页面：

正式调试界面层级和控件命名以 `docs/lcdm_screen_layout_plan.md` 为准。该文档综合了 `../2026/` 下旧下载器界面图片、测试问题记录、ADC/VDD 问题、PA7/PB2 串口联调结果和 `ServoParams v3` 参数结构。本文下方的两页表格保留为最小联调版本说明。

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
| `src/app_hal_msp.c` | 开启 TJC 时配置 PA2/PA3 USART1；`SERVO_TJC_UART_ALT_PINS=1` 时改用 PA7/PB2 |
| `inc/py32f0xx_hal_conf.h` | 开启 TJC 时启用 HAL UART |
| `Makefile` | 增加 `SERVO_ENABLE_TJC_LCDM`、`SERVO_TJC_UART_ALT_PINS` 开关和 UART HAL 源文件 |

## 8. 注意事项

| 项目 | 要求 |
|---|---|
| 不要全屏高频刷新 | 页面和静态文字放在 TJC 工程内；MCU 只更新变化过的控件值 |
| 不要在目标 SOP8 板启用 | PA2/PA3 在目标板已有 H 桥/ADC 用途 |
| 先做显示再做写参数 | 防止未验证 UI 时误写 Flash |
| 写参数必须读回 | 后续保存参数必须 `WRITE_PARAMS` 后 `READ_PARAMS` 比对 CRC |
| 串口电平确认 | TJC 屏 RX/TX 必须与 PY32 3.3V 逻辑兼容 |
| 背光供电独立 | 屏幕供电和 MCU 共地，避免背光电流污染 ADC |

## 8A. 刷新策略

TJC 屏本身保存 `.HMI/.tft` 工程中的页面、静态文字、按钮和默认控件属性。正式调试界面不要让 MCU 每次重画整页；MCU 只负责更新运行中会变的 `.val`、`.txt`。

当前 `tjc_lcdm.c` 已改成缓存刷新：

| 场景 | MCU 行为 |
|---|---|
| 上电初始化 | 清空 MCU 侧显示缓存，第一次刷新当前页时全量写入该页动态值 |
| 周期刷新 | 比较上次发送值，只发送变化过的控件 |
| 切页 | 清空缓存，进入新页后补发该页所有动态值 |
| 屏幕发 `refresh` | 作为强制刷新处理，清空缓存并重发当前页动态值 |

运行时绘图测试版仍会在切换逻辑页面时发送 `cls/fill/xstr` 重画当前画面，因为它没有真正的 TJC 页面工程可复用。2026-06-06 版本已把运行波特率提高到 230400，并用 `ref_stop/ref_star` 批量刷新、2 ms 命令间隔和 30 ms 清屏等待改善整页切换速度。正式版应把页面内容固化在 TJC 工程内，避免通过串口传整页文字和底色。

## 9. PA7 串口抓波记录

2026-06-05 分析 `../2026/PA7串口2.logicdata`：

| 项目 | 结论 |
|---|---|
| 文件内容 | 只有 Saleae 通道/Edge Finder 数据，没有保存 Async Serial 解码文本 |
| 可解析数据 | 从偏移 `0x0A06` 开始有 32 字节记录，第一列为 ns 时间戳 |
| 时间范围 | 有效边沿约 `9.46 ms` 到 `248.97 ms` |
| 直接解码结果 | 按 9600 8N1 不能解出 `dim=20` 或 `dim=100` |
| 判断 | 该文件不能证明 PA7 已输出正确 TJC 命令；更像是捕获窗口没有覆盖测试命令，或捕获通道不是实际 PA7 TX |

原视觉测试第一次发送会等待 1000 ms，而该 `.logicdata` 有效边沿只到约 249 ms，容易漏掉真正命令。固件已调整为视觉测试上电立即发送，并支持 `TJC_LCDM_VISUAL_TEST_MS_OVERRIDE` 缩短周期。当前推荐 PA7 TX-only 测试编译参数：

```bash
BOARD_TSSOP20_DEBUG_PINS=1 \
SERVO_ENABLE_TJC_LCDM=1 \
SERVO_TJC_RX_ENABLE=0 \
SERVO_TJC_VISUAL_TEST=1 \
SERVO_TJC_SOFT_TX=1 \
TJC_LCDM_BAUD_OVERRIDE=9600 \
TJC_LCDM_VISUAL_TEST_MS_OVERRIDE=200 \
HBRIDGE_PWM_TICK_US_OVERRIDE=100
```
