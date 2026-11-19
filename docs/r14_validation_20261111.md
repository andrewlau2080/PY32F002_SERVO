# R14 模型保存资格与掉电保护核对

更新：2026-11-15  
对象：工作区 V10.6 Flash model-store 源码；板上已恢复 force-coast V10.6SIMB2C，保存函数未链接进当前固件
结论：**R14仍未通过；已修正保存函数无条件开启全局中断的问题，并扩展主机注入到26/26；安全资格仍只是API布尔值，生产调用、限频和实体掉电测试未完成。**

## 已完成源码核对

- `Servo_ModelStore_Save(model, qualified_and_stationary)` 在资格参数为 false 或模型为空时拒绝写入。
- 保存时目标是非活动槽，擦除后写新模型并 CRC 回读验证；只有验证成功后才切换 RAM 中的活动槽指针，因此设计上保留旧槽。
- 全工程没有调用 `Servo_ModelStore_Save()`；目前没有由静止确认、安全指标、模型置信度、用户许可或保存间隔组成的资格判定器，也没有写入频率限制。
- Flash擦除/写入期间仍会关闭全局中断；源码现先保存PRIMASK，并在返回时仅恢复调用前已开启的中断状态，不会把原本关闭的中断提前打开。该修复不缩短Flash busy时段，也没有安排到安全维护窗口或确认马达输出为coast的调度逻辑。
- 未做真实板上断电/写入失败注入、擦写次数测试或 Flash 忙时控制时序测量。主机 HAL 注入不能证明真实掉电时序。

## 2026-11-13 离板故障注入

- 生产 `src/servo_model_store.c` 在 mock Flash HAL 上测试 26/26；资格 false 拒绝、工况身份匹配/拒绝、无上下文初始化失败关闭、页擦除/程序失败报告、失败后旧活动槽保留、CRC 损坏回退和 PRIMASK 状态恢复均通过。最终证据见 `firmware/evidence/r14_model_store_irq_state_20261115/`。
- 这是软件故障返回测试，不包含真实 Flash 中断时序、电源掉电、擦写次数寿命或控制任务阻塞测量。

## 2026-11-15 PRIMASK状态恢复修复

- `Servo_ModelStore_Save()`现在保存调用前PRIMASK，并在所有擦除/写入错误返回及正常返回时恢复原状态。主机mock增加“调用前中断已开/已关”两种情形，双槽模型store总回归26/26通过，报告和源码哈希见`firmware/evidence/r14_model_store_irq_state_20261115/`。
- Ubuntu TSSOP20 force-coast交叉构建通过，BIN 18,812B；未下载。由于生产代码当前没有调用Save，链接垃圾回收会剔除此保存函数，构建成功不代表板上调用链有效。

## 未通过原因与所需协助

- 任意调用者可传入 `true`，函数自身无法核实“模型稳定、静止、安全指标合格、明确允许”；这些条件尚未实现。
- 无保存间隔/变化门槛，无法证明不会高频擦写；host HAL 结果也不能代替实物掉电注入。
- 双槽保留旧记录仅有源码设计依据，尚无硬件故障注入和参数页读回证据。
- 没有生产资格状态机、最小保存间隔/变化阈值或coast安全窗口。当前函数仍接收调用者提供的`qualified_and_stationary`布尔值。
- **需要用户协助**：实板验证需 DAPLink 接入并连至 Ubuntu；我会先备份 Flash，再测试保存中断电/失败及旧槽恢复。无需用户提供额外信号源。

R01–R13 未通过项留待后续统一复核，本轮未重跑。

## 2026-11-16 保存API输入约束补充

- 保存函数除调用者`qualified_and_stationary`外，现在还拒绝model identity、calibration fingerprint、motor supply、confidence、学习样本和成功运动计数缺失的模型；这是记录自检，不代表完成了运行时资格状态机。
- 生产双槽mock Flash回归33/33；报告`firmware/evidence/r13_model_save_quality_20261116/report.json`。当前源码TSSOP20生产构建`build/releases/V10.6_TSSOP20_R13_R15_20261116/`通过，但Save仍无生产调用且被链接器裁掉。
- R14仍未通过：安全静止窗口、保存节流/变化门槛、生产调用和实体掉电注入尚未实现/验证。

## 2026-11-19 离板调用资格复核

- 生产启动上下文把预期电机供电传入0，且默认model/profile ID均为0；这会让旧模型加载保持fail-closed。当前VREFINT不能替代已确认的电机供电身份。
- `Servo_ModelStore_Save()`没有生产调用；仅传入布尔`qualified_and_stationary`并不足以形成可审计的资格证明。自动调用前还缺稳定静止时间、控制/ADC/VDD健康状态、模型置信度与变化门槛、最小保存间隔，以及有界的维护窗口。未启用自动保存，也未写Flash。
- 代码可继续实现fail-closed的资格状态机和节流逻辑；完成后仍需实板备份、写入/回读、复位及故障注入。Flash操作期间的真实控制时序不可由主机模拟替代。
