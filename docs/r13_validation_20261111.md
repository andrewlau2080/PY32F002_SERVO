# R13 学习模型持久化与漂移核对

更新：2026-11-13  
对象：工作区 V10.6 model store/adaptive 源码；板上当前为 force-coast `P50 V10.6SIMB2C`  
结论：**R13 部分实现但未通过；双槽生产代码的主机 Flash 故障注入现为 26/26 通过，但运行时仍没有保存调用，实体 Flash/掉电和漂移验证未完成。**

## 已完成源码核对

- `ServoLearnedModel` 包含模型、方向增益、制动/滑行摘要、动作统计、性能统计、供电参考、generation 和 CRC。
- 旧 `Servo_ModelStore_Init()` 仍只按通用有效性/CRC/generation 选槽；两个生产启动入口现调用 `Servo_ModelStore_InitFor()`，再按身份字段筛选。`Servo_Adaptive_Init()` 只会读取当前 store 选中的记录；在线 `Servo_Adaptive_Update()` 继续更新 RAM 估计。
- `Servo_ModelStore_Save()` 接受显式 `qualified_and_stationary` 条件，写入另一槽并回读验证，试图保留前一个有效槽。
- 全工程检索没有发现运行路径调用 `Servo_ModelStore_Save()`；`qualified_and_stationary` 资格如何形成、什么时机触发保存尚未接通。
- `Servo_ModelStore_Validate()` 本身只校验记录完整性；生产上下文匹配在 `Servo_ModelStore_InitFor()` 单独完成。该函数目前因 motor-supply 输入为 0 而 fail-closed。

## 2026-11-13 离板验证

- 新增主机 Flash HAL 故障注入，直接编译生产 `src/servo_model_store.c`：22/22 通过，覆盖空白 Flash、资格为 false 拒绝、generation 递增、工况身份匹配/拒绝、未知供电 fail-closed、CRC 损坏后旧槽回退、程序/擦除失败后保留有效活动槽，以及 IRQ 关/开计数平衡。
- 最终测试与生产源哈希见 `firmware/evidence/r21_model_match_20261113_final/`；测试使用 RAM 模拟 Flash 和 HAL，不是真实 Flash 时序或掉电测试。
- 为支持主机映射，生产源码增加可覆写的 `SERVO_MODEL_FLASH_PTR` 访问宏；嵌入式默认仍直接映射 Flash 地址。
- 当前最终 production source 已在 Ubuntu V10.6 内部小步配置重新构建成功；BIN 18,704 B，SHA256 `ac76262a374ab095632991f83fa131c39837d8b56f0cbc363cad6aec97c2dfe8`，manifest `build/releases/V10.6_R21_FINAL_20261113/` 标记未下载。

## 2026-11-15 增量

- 模型store主机故障注入现为26/26通过，覆盖双槽资格/身份/CRC/擦写失败旧槽恢复以及 PRIMASK 原始状态保存。测试和源码哈希见`firmware/evidence/r14_model_store_irq_state_20261115/`。
- TSSOP20 force-coast交叉构建通过（BIN 18,812B），但全工程依旧没有`Servo_ModelStore_Save()`生产调用；因为调用不存在，链接器会剔除该保存路径。R13仍无运行时模型提交、复位重载与漂移验证，不能判通过。

## 未通过原因与限制

- 目前没有从合格动作提交 Flash 的生产调用；运行时学习结果掉电会丢失。API 的资格参数仍是调用者布尔值。
- 没有板上读回字段、复位重载、掉电故障注入、generation/槽回退或擦写次数证据。
- 生产入口已校验 model/profile/参数指纹，并因供电身份未知拒绝加载；尚无多工况档案和模型漂移检测数据，不能证明工况间安全复用。

## 需要用户协助

- 代码可继续补齐保存资格/调用和配置指纹验证，不需要用户先操作；但必须先设计不在快环运行期间关闭中断的安全维护窗口。
- 板上验证时需要 DAPLink 接至 Mac 并连接 Ubuntu；在获得稳定硬件连接后，我会先保存当前 Flash，再验证双槽写入、重启读回和掉电/故障回退。不要在没有备份时覆盖参数页。

R01–R12 未通过项留待后续统一复核，本轮未重跑。

## 2026-11-16 保存内容自检补充

- `Servo_ModelStore_Save()`现拒绝缺少model_id、calibration fingerprint、motor supply、非零confidence、学习样本或成功运动计数的记录；这能拦截空/不完整模型，但不能代替运行时稳定性资格或静止证明。
- 生产双槽存储实现的mock Flash回归扩展为33/33，包含六类模型质量/身份字段拒绝；报告见`firmware/evidence/r13_model_save_quality_20261116/report.json`，SHA256 `0c43b8c948050289b53c61e8ee331ccd619544ce22f090ddafd5092a0f449f47`。
- TSSOP20生产LCDM Ubuntu构建通过，BIN 16,104B，与VDD保护候选哈希相同；Save仍没有生产调用，被链接器裁掉。R13仍未通过，真实保存、复位重载、掉电及漂移检测未验证。
