# R21 Flash 模型工况适配核对

更新：2026-11-13  
对象：V10.6 model-store/adaptive 启动路径；固件仍未下载  
结论：**启动已按 model/profile/configuration fingerprint/supply 匹配；供电身份未知时 fail-closed。R21 仍未通过，因为实际供电映射、试配行为和多工况数据尚未验收。**

## 本次源码推进

- 新增 `Servo_ModelStore_InitFor()`，只有 model ID、profile ID、配置指纹和电机供电读数均匹配的 Flash 槽才可作为当前模型；双槽中只在匹配槽之间按 generation 选择最新记录。
- 供电为 0、model ID 未配置、配置指纹为 0 或字段不匹配时，不加载该模型。两个生产入口均调用此筛选初始化。
- 配置指纹当前用 `Servo_Params_Crc32()` 计算整组参数。供电读数明确传 0：现有 VREFINT 只能测 MCU VDD，尚未证明它等于电机电源轨，因此不会拿它冒充 motor supply。
- `Servo_Adaptive_Init()` 在无匹配模型时使用现有默认/空 RAM 模型，不会将旧记录直接注入当前运行。

## 2026-11-13 离板验证

- mock Flash 主机测试 23/23 通过：覆盖匹配模型加载、不同 model/profile/fingerprint/supply 拒绝、供电未知和无上下文初始化失败关闭、CRC 回退、双槽 generation、写入/擦除失败和 IRQ 平衡。证据见 `firmware/evidence/r21_model_match_20261113_final/`。
- Ubuntu V10.6 内部小步构建通过，BIN 18,704 B，SHA256 `ac76262a374ab095632991f83fa131c39837d8b56f0cbc363cad6aec97c2dfe8`；manifest `build/releases/V10.6_R21_FINAL_20261113/` 标记 `flashed=false`、`remote_build_cleaned=true`。

## 未通过原因与后续所需

- 目前供电身份缺失会使启动拒绝所有 Flash 模型；这避免错误加载，但还没有供电测量到“电机电源”语义的可信映射。
- 没有跨马达、齿轮箱、负载和电源工况的模型档案，也没有低风险试配、渐进启用和模型漂移实测。
- 后续需确认板上电机电源测量点/分压映射、实际舵机/齿轮箱/负载标签；待 R01 同步与方向安全门槛解除后再用 DAPLink 做分工况实测。当前不需要用户立即操作。
