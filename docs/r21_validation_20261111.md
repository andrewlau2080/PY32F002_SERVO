# R21 Flash 模型工况适配核对

更新：2026-11-13  
对象：V10.6 model-store/adaptive 启动路径；固件仍未下载  
结论：**项目统一电机工作范围已由用户确认为4.8–8.4V（合并CYS-S3047 4.8–6.0V与S0100 6.0–8.4V）。model/profile/参数指纹/供电筛选骨架存在，但生产供电身份仍传0，且无安全试配和多工况数据，R21未通过。**

## 本次源码推进

- 新增 `Servo_ModelStore_InitFor()`，只有 model ID、profile ID、配置指纹和电机供电读数均匹配的 Flash 槽才可作为当前模型；双槽中只在匹配槽之间按 generation 选择最新记录。
- 供电为 0、model ID 未配置、配置指纹为 0 或字段不匹配时，不加载该模型。两个生产入口均调用此筛选初始化。
- 配置指纹当前用`Servo_Params_Crc32()`计算整组参数。项目统一电机工作范围已知为4.8–8.4V；其来源分别是CYS-S3047 4.8–6.0V和S0100 6.0–8.4V，旧版`C8051F850 SCH.pdf`标称DC6V给H桥/马达、ME6209-3V3生成逻辑VDD。VREFINT只测MCU VDD，不用于推断电机电压。固件model-store供电身份仍传0，缺口是将所用供电工况配置/测量并纳入模型身份，而不是缺少产品工作电压范围。
- `Servo_Adaptive_Init()` 在无匹配模型时使用现有默认/空 RAM 模型，不会将旧记录直接注入当前运行。

## 2026-11-13 离板验证

- mock Flash 主机测试 23/23 通过：覆盖匹配模型加载、不同 model/profile/fingerprint/supply 拒绝、供电未知和无上下文初始化失败关闭、CRC 回退、双槽 generation、写入/擦除失败和 IRQ 平衡。证据见 `firmware/evidence/r21_model_match_20261113_final/`。
- Ubuntu V10.6 内部小步构建通过，BIN 18,704 B，SHA256 `ac76262a374ab095632991f83fa131c39837d8b56f0cbc363cad6aec97c2dfe8`；manifest `build/releases/V10.6_R21_FINAL_20261113/` 标记 `flashed=false`、`remote_build_cleaned=true`。

## 未通过原因与后续所需

- 当前启动上下文供电身份仍传0，因此拒绝所有Flash模型；这避免错误加载。项目工作范围4.8–8.4V已知，旧版图标称DC6V。缺少的是把实际使用的供电工况编码并纳入模型身份，以及不同电压/负载下模型能否共用的验证；不能把MCU VDD当作电机供电。
- 没有跨马达、齿轮箱、负载和电源工况的模型档案，也没有低风险试配、渐进启用和模型漂移实测。
- 后续把4.8–8.4V列为项目允许电机供电范围；模型仍需按实际选用的电压工况和马达/齿轮箱/负载标签隔离。若需要区分运行时具体供电档，再使用电机电源测点；VREFINT不参与该判断。待R01动作安全门槛解除后做分工况实测。

## 2026-11-21 工况profile失败关闭补强

- `src/servo_model_store.c` 加载时现在拒绝`expected_profile_id=0`，保存时拒绝`model->profile_id=0`。这样未配置工况不能被存档或匹配为有效模型；不会伪造电机供电电压，也不改变已知的4.8–8.4V项目范围。
- 修改后Ubuntu TSSOP20生产LCDM构建通过，归档`build/releases/V10.6_TSSOP20_R13_R21_PROFILE_GUARD_20261121/`，BIN 16,240B，SHA256=`9a6d151bfbdf7df2ec61039727a38c79dc5e0744bbe277228dea627de06f50f4`；manifest含完整源码/SDK/flags哈希，`flashed=false`。未运行测试、未下载。仍需定义实际profile编码（型号、供电使用档、马达/齿轮箱/负载），并接通可信模型快照/资格/保存调用；R21未通过。
