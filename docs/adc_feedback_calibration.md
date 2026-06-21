# ADC 反馈实测记录

版本：v0.1

用途：记录 PA3 电位器反馈 ADC 的实测范围，后续闭环反馈程序不要假设 ADC 最小值为 0、最大值为 4095。

## 1. 当前测试固件

编译开关：

```bash
BOARD_TSSOP20_DEBUG_PINS=1 SERVO_ENABLE_HBRIDGE_IO_TEST=1 SERVO_ENABLE_ADC_LED_TEST=1 SERVO_IO_TEST_ENABLE_GPIOB=1 HBRIDGE_PWM_TICK_US_OVERRIDE=100
```

测试方式：

| PA3 电压范围 | H 桥 LED 表现 | 当前临时映射 |
|---|---|---|
| 实测最低端到中点 | 一端 LED 从灭到亮 | ADC raw `213-2056` 映射为反向 duty `0-1000`；`<=225` 强制关断 |
| 中点到实测最高端 | 另一端 LED 从灭到亮 | ADC raw `2056-3900` 映射为正向 duty `0-1000`；`>=3900` 满 duty |

注意：这是临时 LED 可视化测试映射。实际舵机闭环要使用实测 `adc_min_count`、`adc_center_count`、`adc_max_count`。

## 2. 已确认现象

LED 不能完全变暗，说明当前 PA3 输入端、外部电位器/分压/滤波电路或 ADC 偏置下，实测最小 ADC raw 不一定等于 0。

后续程序处理原则：

| 参数 | 说明 |
|---|---|
| `adc_min_count` | 取 PA3 实测最低机械位置 raw，不使用固定 0 |
| `adc_center_count` | 取电位器中点或舵机机械中点 raw，不固定写死 2048 |
| `adc_max_count` | 取 PA3 实测最高机械位置 raw，不使用固定 4095 |
| 电压换算 | `Vadc_mv = raw * VDD_mv / 4095` |

## 3. 本轮实测值

等待实测扫动后填写。当前测试固件已经加入以下 pyOCD 可读变量：

| 变量 | 含义 |
|---|---|
| `s_adc_led_test_raw` | 当前滤波后 ADC raw |
| `s_adc_led_test_duty` | 当前写入 H 桥的 signed duty |
| `s_adc_led_test_min_raw` | 本次上电运行以来采到的最小 raw |
| `s_adc_led_test_max_raw` | 本次上电运行以来采到的最大 raw |
| `s_adc_led_test_vdd_mv` | 当前按 VREFINT 估算的 VDD |
| `s_adc_led_test_min_mv` | 最小 raw 换算电压 |
| `s_adc_led_test_max_mv` | 最大 raw 换算电压 |

| 日期 | 条件 | VDD | min raw | min mV | center raw | max raw | max mV | 备注 |
|---|---|---:|---:|---:|---:|---:|---:|---|
| 2026-06-05 | TSSOP20 调试板，PA3 ADC LED 测试，当前输入未覆盖全量程 | 3331-3333mV | 207 | 168mV | 待测 | 1923 | 1563mV | 读取期间当前 raw 约 1917-1919；已确认低端不是 0，上端未扫到 3.3V |
| 2026-06-05 | 按提示固定最低端/最高端分别读取 | 3333-3336mV | 213 | 174mV | 待测 | 3900 | 3175mV | 最低端当前读数 `213`；最高端当前读数 `3900` |
| 2026-06-05 | 同一次运行中固件自动捕捉 min/max | 3333-3336mV | 189 | 154mV | 待测 | 3929 | 3195mV | 自动捕捉值可作为后续端点校准参考，闭环参数建议再留少量余量 |

结论：

- 当前已实测到 PA3 最低端当前 raw 为 `213`，约 `174mV`；同次运行自动捕捉最低 raw 为 `189`，约 `154mV`，所以后续反馈程序不能把 ADC 最低点当作 `0`。
- 当前已实测到 PA3 最高端当前 raw 为 `3900`，约 `3175mV`；同次运行自动捕捉最高 raw 为 `3929`，约 `3195mV`，所以后续反馈程序不能把 ADC 最高点简单当作 `4095`。
- 后续闭环建议先按实测机械行程写入 `adc_min_count / adc_center_count / adc_max_count`。LED 测试映射也应改为“实测 min 到 center、center 到 max”映射，而不是固定 `0/2048/4095`。
