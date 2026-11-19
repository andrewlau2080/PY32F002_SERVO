# R04 快速环响应与 HOLD 监测验证记录

日期：2026-11-10；续记：2026-11-15  
对象：V10.5历史实板稀疏HOLD轨迹及V10.6 R04 force-coast scheduler诊断候选  
结论：**R04 未通过。** 现有记录只证明长时间平均发布约 1 kHz；命令到占空比延迟、逐周期时序和 UI 压力影响尚无证据。

## 已完成的核对

- 重跑 `tools/analyze_r04_hold_trace.py` 分析 V10.5 现场轨迹：430 个帧序号和误差一致、采样时为 HOLD 的帧；429 个相邻区间内推算发布 49,904 次，覆盖 49,904 ms，平均 **1000.0 次/秒**。复核报告：`firmware/evidence/r04_current_recheck_20261110/report.json`。
- 稀疏采样帧间距 **82–307 ms**（中位数 103 ms），不能推断这些区间内每个控制周期的抖动、漏周期或中间状态。采集时无受控指令跳变，不能测命令到 duty 延迟。原始轨迹及首轮分析见 `firmware/releases/v10.5/r01_validation_20261110/field03/trace.csv` 和 `firmware/evidence/r04_hold_20261110/`。
- 源码中 `lcdm_runtime_service_motor_control()` 以 1 ms 为调度条件调用 fast loop；它在 UI 主循环和带控制的等待函数中被调用。若积压达到 4 次，会把调度基准重置为当前 tick，丢弃更多积压；因此不能仅凭源码声称阻塞期间每周期无遗漏。TJC UART 发送与 UI 重绘的占用影响仍需实测。
- `Servo_Adaptive_Update()` 位于 fast loop 内同步运行；源码核对未显示其中有等待或 Flash 擦写，但这不能证明它、ADC 采样和显示通信在目标板上的最坏执行时间。

## 未通过原因与所需协助

- **未通过原因**：没有逐周期 GPIO/时间戳波形，没有受控命令变化，也没有 UI 更新压力下的命令到 duty 轨迹；已有 SWD 采样间隔过稀。
- **我已完成**：重新核对当前调度源码并复算既有 HOLD 轨迹。无需外部 PWM 发生器，P50 `INTPWM` 可自动触发 1000/2000 µs 变化。
- **当前无需用户立即操作**：DAPLink现已可用，Ubuntu读写链路已验证。本次R04用force-coast固件完成板上观察，不需要用户转动机构。若后续要完成UI压力验收，需要开发板LCDM屏保持连接并可接受连续触摸/翻页；高分辨率逐周期时序还需逻辑分析仪或可读的硬件时序引脚。
- **下一步**：探针可见后读取备份并运行选定 V10.x 固件；用板上高速事件记录或 GPIO 时序标记记录每次控制开始、命令更新时间和 duty 首变时刻，在 HOLD 期间同时记录反馈/模型状态；重复无 UI 操作与连续 LCDM 页面重绘两种工况，分别比较周期、漏周期和最大命令到输出延迟。没有逐周期证据前 R04 保持未通过。

## 2026-11-15 调度积压诊断遥测（离板）

- LCDM runtime UI诊断配置新增 `g_r04_scheduler_overrun_count`、`g_r04_scheduler_dropped_tick_count` 和 `g_r04_scheduler_max_dropped_ticks`。单次服务补跑4周期后若仍有积压，累计记录被跳过的tick；超限次数与最大值饱和于16位，累计丢失tick为32位。调度/控制行为未改变，遥测仅在 `SERVO_ENABLE_LCDM_RUNTIME_UI_TEST` 启用，并接入 `tools/monitor_r01_swd.py` CSV。
- Ubuntu TSSOP20 `--internal-step-test --force-coast` 构建通过，BIN 18,884B，SHA256=`d32efb7846ac982475dcd94ac6a6c593f1402fc1e23af326b198425cfc4b8bf4`；map确认 `_ebss=0x20000600`，恰好保留1536B栈余量；未下载，未触发控制动作。监控脚本 `py_compile` 通过。
- 实板force-coast采集161样本/40.065秒，PWM 1503→1506→回中同步检查通过。首次/末次SWD样本的 scheduler 计数均为 overrun=8、dropped_ticks=30、max_dropped=10，采集窗口未增加；但无法确定此前8次何时发生。序号一致153/161，误差字段一致83/161，实际SWD轮询远慢于请求20ms，因此不是逐周期证据。力桥强制coast，软件请求峰值40不代表实体输出。常规LCDM诊断页150ms刷新运行，未施加连续触摸重绘负载。证据 `firmware/evidence/r04_scheduler_telemetry_20261115/`。
- 代码、构建和这一窗口数据只证明积压可以被观察、该窗口内计数稳定；连续UI重绘对照、准确高分辨率逐周期时序和命令到输出延迟仍缺。**R04仍未通过。**实测后已恢复R5 force-coast，整片20KB回读与下载前一致，SHA256=`5ad1ea0521c1b03bc99b9a62f9d3b7836a6e2df20299ea90cf21c26bb3a7f51b`。

## 2026-11-15 连续重绘根因与修复

- 新增自动每250ms重绘P50诊断页的force-coast压力构建。修复前40.078秒/159样本中PWM始终为1503µs，1503→1506→回中序列没有发生；控制帧仍在发布。源码追到 `lcdm_draw_p50()` 每次页面重绘都会调用 `lcdm_reset_pwm_live_cache()`，该函数重置控制状态和内部PWM测试起始tick，导致连续重绘反复重置扫动计时器。
- 修复将 `lcdm_reset_pwm_live_cache()` 移到runtime UI初始化，只在启动和显式切换PWM源时重置；普通页面绘制不再修改控制状态。相同250ms自动重绘压力下，R6 force-coast构建40.127秒/161样本，1503→1506→回中同步检查通过；scheduler上下文/丢失计数在采集窗未增长。此前R3/R5采集定位到累计事件来自等待/UART调用，正常压力窗口期间未继续增长。
- R6 app读回与18,904B BIN逐字节匹配，整片SHA=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`，尾384B保持FF，核心Running。随后R5 force-coast恢复并读回：整片SHA=`5ad1ea0521c1b03bc99b9a62f9d3b7836a6e2df20299ea90cf21c26bb3a7f51b`。读回、失败复现和修复后采集见`firmware/evidence/r04_ui_redraw_reset_fix_20261115/`及相邻R3/R5对照目录。
- 这解决了连续页面重绘重置控制状态这一R04源码缺陷，并通过命令更新压力对照；R04仍未整体通过，因为SWD没有逐周期分辨率，也未取得真实PWM命令到H桥引脚输出延迟。

## 2026-11-15 SysTick时序遥测诊断（R8，未通过）

- 为缩小逐周期时序证据缺口，增加仅用于force-coast压力固件的SysTick微秒时间戳、8档循环起始间隔直方图、最大间隔、最大fast-loop执行时间和>1000µs执行计数。额外RAM令日志环从9条减为6条；Ubuntu构建通过，BIN 19,220B，SHA256=`241a84a8588189c59d27ad488721334e4c5936d02c3c3dbe034981fb06b4f7a1`，保留栈断言通过。该计时方式依赖SysTick相位换算，尚未用示波器校准，数据暂作诊断线索。
- R8通过under-reset下载；整片Flash读回SHA256=`92780a0c0826ace603e7bddccccdf91919a41c3c324e8256ee789af5f69ce3f6`，核心Running。40.242秒SWD观察142样本；内部PWM同步校验失败：全部观测帧仍为1503µs，没有观察到上行或回中。计时器累计89,187个间隔样本，直方图最终计数依次为30,246/7,446/5,808/35,055/10,543/108/1/2，最大间隔13,121µs；最大单次fast-loop时间566µs，>1000µs为0，丢失tick累计31。采集CSV/JSON在`firmware/evidence/r04_timing_trace_redraw_fix_20261115/`。
- 这次采集不能用于R04通过结论：PWM扫动未触发，计时直方图显示间隔分布离散且尚未校准，SWD帧也不足以说明每个周期。需继续查清内部PWM启动计时基准/重置来源，并用逻辑分析仪或可读硬件标记校准周期和fast-loop时间。
- 之后使用under-reset恢复已验证的R6 force-coast构建并读回；恢复后整片SHA256=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`，核心Running。当前板上是R6 force-coast，不是R5；没有触发马达输出。

## 2026-11-15 同步触发R04时序采集（R9）

- R8同步失败根因已确认：中心扫动计时从复位启动，先等待30秒；R8监控连接时扫动已经结束，因此帧保持1503µs。R9新增仅限R04 timing-trace诊断的 `g_r04_sweep_start_request`，监控先attach，再写请求；固件以请求时刻重置扫动基准。新Ubuntu force-coast BIN 19,240B，SHA256=`8d8b964cfcea9395154a28803b6d96d207d765cd1fc859b269ee0fe2d1d62c62`，栈断言通过。
- R9 under-reset下载、整片20KB读回通过，读回SHA256=`051b393ea2f30027079df861611dd3d55fc89c60892bb4530f8d90450af1368e`，核心Running。监控器启动触发后采40.037秒/141样本；PWM同步`1503→1504→1505→1506→1505→1504→1503`通过。中心动作软件duty请求峰值40/1000；因为force-coast，不能视为电机实际输出。
- 用窗口起止计数差分：快环计数+39,687，HAL tick +39,688，scheduler丢tick +0，最大单次执行计数从513µs升至578µs，>1000µs执行增量为0。说明这段250ms自动重绘窗口内平均快环约999.97Hz，未观察到累计漏tick；SWD只采141次，不能证明每一个周期的间隔或命令到引脚延迟。
- 子毫秒间隔直方图窗口差分为`<500, 500–899, 900–1100, 1101–1500, 1501–2000, 2001–4000, 4001–10000, >10000µs`：`13674/3464/1664/16435/4424/28/0/0`。分布与预期1ms周期不符，SysTick相位换算尚未校准；最大间隔字段是启动以来最大值，不能归属于本采集窗口。直方图和最大时间不作为验收证据。
- 监控同步命令变化后采到duty 40，PWM变化与下一次250ms SWD抽样间隔不足以精确测延迟；R04仍部分通过而非整体通过。CSV/JSON：`firmware/evidence/r04_timing_trace_armed_20261115/`。采后under-reset恢复R6 force-coast，20KB读回SHA256=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`，核心Running。
- 下一步时序方法：停止依赖未经校准的SysTick子毫秒估算；以PWM硬件更新事件或可读GPIO作示波器/逻辑分析仪基准，或者用独立硬件定时器捕获。当前无须用户即时操作；后续若要测真实H桥输出，必须先完成R01方向/同步安全门槛。

## 2026-11-19 丢周期即时降级源码修正

- LCDM runtime scheduler在4次补跑后若仍丢弃tick，现立即`HBridge_Coast()`，并以无效样本通知adaptive learner；同时清零当前占空比/速度、observer基准和轨迹速度，取消未完成的customer profile阶段，状态置为BAD。该修正只在检测到实际丢弃tick时执行，不改正常周期或客户profile的7/2/3ms波形参数。
- 修改范围是`SERVO_ENABLE_LCDM_RUNTIME_UI_TEST`调度路径；它处理“发现丢周期后的即时安全降级”，不代表已完成所有生产控制路径的超时状态机，也不证明硬件输出波形或时序。
- 本机临时构建因缺少`/opt/py32/PY32F0xx_Firmware` HAL头文件，在编译`src/app_entry.c`时失败；未运行测试、未下载、未触发动作。需在项目规定的Ubuntu工具链环境重建后才能确认编译，再进行force-coast板上诊断。R04仍未通过。

## 2026-11-15 窗口计数器清零后的复测（R10）

- 为避免把启动后历史最大值混入本次窗口，R10在监控器arm时清零R04计时直方图、最大值、超时计数和scheduler丢tick统计。TSSOP20 force-coast BIN 19,148B，SHA256=`322ea25a8e3588dd67515fa55ac75583e64d2f6bb38364debbc5ac00f0a7b2302`；Ubuntu构建、栈保护通过。
- R10 under-reset下载和整片20KB读回成功，读回SHA256=`b68f3e82cd67f2bd2e035b9fa3cd69f8d1faccc3ac6da3d51d4da266c8e3af36`，核心Running。attach后arm采集40.231秒/129样本，中心PWM同步1503→1506→回中通过。
- 以首末SWD样本差分：快环间隔计数增加39,907、HAL tick增加39,904；scheduler丢失tick=0，最大执行时间574µs，>1000µs执行=0。快环计数与HAL tick仅差3，支持本窗口约1kHz运行且无累计漏tick。计数器在arm写入后、首个SWD行形成前已运行约250次，因此不把绝对计数直接当作CSV窗口长度。
- 子毫秒直方图仍不可信：窗口档位计数`<500/500–899/900–1100/1101–1500/1501–2000/2001–4000/>4000µs`约为`13889/2843/1845/16815/4480/35/0`，其形状与1ms主循环不一致；最大测得间隔2,519µs也未经独立时间基准校准。不能以此验收逐周期抖动。H桥处于force-coast，实际引脚输出及命令到引脚延迟未测。
- 采后under-reset恢复R6 force-coast并整片读回：SHA256=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`，核心Running。R10 CSV/JSON在`firmware/evidence/r04_timing_trace_window_20261115/`；R04继续保持部分通过。
