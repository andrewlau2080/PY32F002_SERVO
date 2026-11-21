# R19 版本、构建身份与唯一下载对象核对

历史基线：2026-11-16；最新状态见文末2026-11-19增量。

当时结论：历史构建可追溯，但尚无与完整工作树一致的新BIN/ELF；R19部分通过。

## 当前硬件与构建状态

- 最近归档的板况为 R6 `P50 V10.6SIMB2C` force-coast，整片 20KB SHA256=`89600fe63b3212efd5eaaf69503f06b06012296aae36e3cceef357b343098826`；本轮未重新读板。
- `build/releases/V10.6_R21_FINAL_20261113/` 是历史构建：实际 BIN **18,660 B**，SHA256=`ac76262a374ab095632991f83fa131c39837d8b56f0cbc363cad6aec97c2dfe8`。manifest 含35项源文件哈希、SDK、flags及4种产物哈希，标记`flashed=false`。旧文档曾误记18,704 B，已按实际文件与manifest更正。
- 最新完整LCDM force-coast候选`build/releases/V10.6_R11_STABILITY_TIME_COAST_R4_20261115/`，BIN 19,180 B，SHA256=`e2d6476a48a00d6e781e5b273c740ae759be8e0fa29c1d363b21dcc832e23e09`，manifest覆盖36项源文件且标记未下载。逐项与当前工作树比对，`src/servo_control.c`哈希不一致，原因是其后加入R03失联恢复速度基准修复。
- `V10.6_R11_STABILITY_TIME_PRODUCTION_R3_20261115`同样与当前`src/servo_control.c`不一致，且为非force-coast生产路径，不选作当前诊断下载对象。
- `V10.6_R14_PRIMASK_FORCE_COAST_20261115`比当前工作树有3个输入文件不同：`Makefile`、`src/app_entry.c`、`src/servo_control.c`，也不选作当前候选。
- 旧 `candidate07` 快照只含`src/app_entry.c`副本/哈希，其他构建输入缺失，不能重建验证；不再作为下载候选。

## 下载准备结论

- 正在准备受控诊断下载，但**当前没有与完整工作树一致的BIN/ELF**。本机 Homebrew `arm-none-eabi-gcc 15.2.0`缺少newlib/sysroot；`stdint.h`无法编译且`libc.a`不存在。诊断见`firmware/evidence/toolchain_macos_runtime_20261116/report.json`。
- 应通过固定Ubuntu工具链重建当前源码，保存全量源文件哈希、SDK/flags与产物哈希；第一版下载对象只选force-coast诊断镜像，用于非运动的MCU调度、显示和遥测检查。不能选生产BIN，也不能因force-coast镜像启动而宣称产品通过。
- PWM目标、电位器手动方向和马达方向沿用用户确认的既有定论；最近一次微步采集不重复不推翻该结论。当前动作固件的待验点是微步重复性、同步输出证据及报警行为。下载仍需同版构建、只读识别/备份、under-reset写入和整片读回。
- 当前无需用户按键或输入PWM。R19剩余验证是生成同版构建、唯一候选确认和板上读回；本轮没有下载。

## 2026-11-19 最新构建与板上状态更正

- 当前最新构建为`build/releases/V10.6_TSSOP20_R01_CENTER_RETURN_DIAG_20261119_R1/`，BIN 16,332B，SHA256=`2a204ad96fcbf37183948482a8566f3d7ce5e9989f19cb68f49d95b29ac438e6`，ELF SHA256=`695dd49cb92fc42d838327808e8832dae63374ac7b4d6ca0e568c11c846d2fd0`。manifest记录目标`py32f002ax5`、SDK、flags、全量参与源码和产物哈希，`flashed=false`。
- 当前板按R01停止记录为R16慢速诊断固件；SWD确认active=0、arm=2、HBridge duty=0/state=coast、核心Running。中心回归候选没有下载。以后任何下载仍须先实时识别并备份整片Flash，不能依靠旧板况摘要。
- R19仍部分完成：构建身份可追溯；候选板上写入/回读尚未进行，R01/R20产品验收均未通过。

## 2026-11-21 只读探针检查

- 经已配置的Mac→Ubuntu SSH路径运行`python3 tools/servo_swd_ssh.py check`；SSH可达，但Ubuntu返回`No available debug probes are connected`，未识别到DAPLink，因此没有读取目标芯片、没有下载或写Flash。检查日志在临时目录`/private/tmp/servo_daplink_check_20261121/`，没有在Mac/Ubuntu两侧重复保存镜像。
- 当前候选`V10.6_TSSOP20_R13_R21_PROFILE_GUARD_20261121`构建成功但未下载。探针重新出现后，下一步先只读整片识别/备份，再决定是否需要板测。

## 2026-11-21 用户报告已接板后的探针复查

- 用户报告仿真板已接好后，立即再次运行同一只读检查；Ubuntu仍返回`No available debug probes are connected`，DAPLink数量为0。未能访问MCU，未读取、复位、下载或写Flash；日志`/private/tmp/servo_daplink_check_20261121_recheck/ssh.log`。
- 目前阻塞点是Ubuntu侧没有枚举到DAPLink调试探针，不是固件构建问题。请确认连接的是仿真器/DAPLink的USB调试口并接到Ubuntu工具机（或其USB直通），而不只是给目标板供电；重新插拔探针USB后通知我，我再立即重试只读检查。恢复后先识别并备份全片。

## 2026-11-21 探针恢复后的只读板况确认

- 用户再次确认已接好后，Ubuntu枚举到唯一DAPLink，目标`py32f002ax5`，UID=`070000011a5a5cc500000000000c00b6a5a5a5a597969908`。只读整片读取20,480B成功，核心`Running`，SP=`0x20000c00`、PC=`0x08003bd5`；全片SHA256=`1d4058ff42797b7ef4671866118528a96fd730fe621005bf0541537c55186f24`。日志与唯一备份位于`/private/tmp/servo_board_read_20261121/`，未在Mac和Ubuntu重复留存。
- 读回前16,220B与归档`V10.6_TSSOP20_R01_1500_1550_SLOW_20261119_R16/py32f002_servo.bin`逐字节一致，BIN SHA256=`cc308efdc02ffae0a218eb1596767834263a625e09ae4de0897323a68c947c7e`；末尾384B为全`FF`。镜像内还能找到旧字符串`P50 V9.0`，属未覆盖残留，不作当前版本判断；R16前缀匹配是版本依据。
- 仅可确认核心运行及R16固件映像；本轮没有触发、复位或下载。尝试调用历史R01 RAM符号读取脚本时，本机缺少它要求的本地compat脚本，因此没有读取RAM运行状态；无板上写操作。当前板不应当作产品固件或R01验收通过。后续如需读R01状态，使用Ubuntu已验证的远程SWD兼容路径，先解决该工具路径差异。
- 随后修复`tools/read_r01_symbols_swd.py`：Mac端调用时将ELF符号地址作为小型JSON经SSH传入Ubuntu，由Ubuntu pyOCD/HID compat直接attach读取，不传输或保存第二份ELF/固件；增加只读多帧选项。8帧读取实际跨度约3.8s（SWD采样耗时使实际间隔大于请求的250ms）：核心全程`State.RUNNING`，arm=0、production active=0、输入1500µs但`input_valid=0`、实时raw ADC始终2256、signed duty=0、控制state=0、正反桥导通计数始终0；确认本次观测中处于coast且R16动作未运行。静止raw一致只说明这段采样期间反馈未变，不作角度/精度结论。工具脚本语法与`git diff --check`通过；未触发动作、未复位、未下载。
- 为继续R01，随后以既有200/1000限幅设置显式arm R16；10.38秒20帧内nonzero duty请求与正向桥on计数增加，但raw仅2253–2258，反向桥无新增计数。按无反馈位移保护规则立刻请求coast，停后3帧确认active=0、duty=0、核心Running。详细板测见`docs/r01_validation_20261110.md`。本轮无Flash改写；脚本SHA256=`9b13a75c14d66755da3fa30d7e32c52633f0db79c10e20d6d8a782cb47889bcf`。
- 此后构建/下载R17（duty350、step50µs、1s/direction）：BIN SHA256=`045558f3095ed9dc0bb876be15bbb82bde91ad8d6a36b2be0e670a8162d859fb`。与用户此前确认运行的R15 BIN逐字节相同。R15记录中的16,112B是text段大小，另有128B data，因此实际BIN为16,240B；原先据此怀疑R15镜像缺失的判断撤销。R17写后整片SHA256=`8ca4ffe7c371e99addd3bb60a5a5ed9943a9ff1371a886ac7b24fddd4451ca2b`，应用匹配、尾384B保持、核心Running。显式arm后反馈仍2253–2259无可确认位移；立即coast并确认active=0、duty=0、计数冻结。当前板R17已停。停机快照：输入有效1500µs，目标/反馈2256 ADC、误差0、HOLD，arm=2、active=0、位置锁=0；测试累计非零duty请求28,404次、峰值350。只证明软件闭环请求到位，不证明物理位移。
- 用户确认本轮LCDM、马达均无可见变化；R17已coast。LCD之前已验证，不重复检查。
