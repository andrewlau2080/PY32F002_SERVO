# R10 反向 PWM 与滑行/制动核对

日期：2026-11-11  
对象：工作区 V10.6 candidate07 LCDM/H 桥源码（板上仍为 V10.5）  
结论：**R10 未通过；当前反向片段是定时反向驱动，没有按实时速度误差收敛至零。**

## 源码核对

- `HBridge_SetSignedDuty()` 把 duty 符号直接映射到正/反方向；PWM 每个周期在相应方向输出和全脚关闭（coast）之间切换。`HBridge_Brake()` 直接调用 `HBridge_Coast()`，所以该 API 的“brake”实际是四脚关闭滑行，不是电气短路制动。
- 普通 LCDM 分支的预刹反向 duty 有 80–360 限幅，且按估算速度/误差生成；但没有统一基于实测制动模型和零速的连续归零控制。方向改变保护会先 coast 并等待 dead-time，但该安全保护不能证明反向 duty 是零速前的受限吸能。
- 客户波形 profile 固定 drive 7ms、反向 brake 2ms、反向 tail 3ms、随后 coast 24ms；对应反向 duty 固定 360/220，不依据实时速度变化，并绕过普通输出 slew/reverse guard。若转子在片段结束前降到零，余下反向输出可能成为反向驱动。

## 未通过原因与限制

- 客户固定时序不按速度误差降低反向 duty，也没有零速附近提前归零。
- `HBridge_Brake()` 等同 coast；当前 PWM 输出的关断段是 coast。没有示波器/逻辑分析仪同步观测两路实际 H 桥输入及电机速度，不能推断板外波形、能量回收或电气制动效果。
- 板上 V10.5 尚未用本候选行为采集；没有证据证明没有反向再加速。

## 需要用户协助

- 接入 DAPLink 并分配给 Ubuntu；仿真板接舵机/电位器并保证转臂可观察。内部 PWM 可触发，不需外部 PWM 信号源。
- 测量时同步记录两路 H 桥输入、位置/速度估计、signed duty、事件阶段和时间戳；重点捕捉 drive→reverse→coast、零速附近和反向速度是否再次增大。建议使用示波器/逻辑分析仪观察两路输出。

R01–R09 未通过项留待后续统一复核，本轮未重跑。

## 2026-11-16 客户反向制动按实测速度收敛（TSSOP20源码）

- 新增`inc/lcdm_customer_brake.h`，客户profile的2ms reverse-brake和3ms reverse-tail均按沿原运动方向的实测速度缩放。反馈速度进入`motion_hold_vel_q4`低速带后输出归零；若反馈速度已经变成反方向，也立即归零，避免固定尾段继续反向驱动。最大反向占空仍受既有profile duty和`loop_max_duty`双重限制。
- `src/app_entry.c`仅在customer reverse阶段调用缩放函数；前段drive时序、H桥映射及`HBridge_Brake()==coast`行为未改。TSSOP20方向符号沿用已有结论。
- 新增主机边界测试：低速归零、比例缩放、最大占空封顶、正反方向符号、已反转归零、无效输入及完整32位速度极值共 **12/12** 通过。报告与结果见`firmware/evidence/r10_customer_brake_20261116/`；报告SHA256=`1b5c00cefa50ac06e1184d081ce91ec9e91a3bfad3d37490586c6b6779e6198b`。
- **R10仍未通过**：主机测试只验证算术边界，不代表真实速度符号或两路H桥引脚波形。2026-11-16同一源码已纳入TSSOP20正式LCDM构建`build/releases/V10.6_R15_VDD_GUARD_20261116/`（BIN 16,108B，未下载）；需上板同步采集速度、signed duty、事件和双路H桥实际波形，确认零速切断时机及没有反向再加速。

## 2026-11-25 离板进展

- 新增的`inc/lcdm_motion_phase.h`把反向制动开始/结束从customer profile固定阶段时间改为决策接口：由signed位置误差、反馈速度、qualified stopping distance/保守回退距离选择阶段，并在速度进入停止带前锁住BRAKE阶段；硬超时独立coast。该层尚未接入生产路径。
- 阶段决策器边界向量现为C99/C11及UBSan **25/25通过**，覆盖反号撤刹、BRAKE_COAST、距离相关驱动斜坡和残差按误差收敛。先前固定驱动版本压力扫掠7/13收敛；增加两级斜坡后同一13组固定合成工况13/13收敛，参数仅用于该合成模型，不是产品默认值；详见`docs/r16_validation_20261111.md`。
- **R10仍未通过**：离板决策器及合成对象子测试通过，但算法尚未接入生产路径，也没有本候选构建/下载、实体双路桥脚波形和真实零速附近速度证据。下一步先完成客户波形回放边界与生产接入审查，再安排受限实板验证；不是重新怀疑已确认的H桥硬件方向。

### 2026-11-25 输出路径安全门复核

- `src/app_entry.c`中customer-active分支设`customer_profile_direct=true`，因此跳过普通输出分支的`LCDM_SERVO_REVERSE_DEAD_MS=8`换向空档、`lcdm_servo_apply_slew()`和方向报警检查。它直接用定时profile换向，不适合作为新反馈决策器的接入点。
- 新决策器若进入板测，必须走普通输出guard；反向dead-time期间要继续保留BRAKE阶段状态，但不得把这段零输出等待计入主动制动持续时间。接入前需有主机测试覆盖FWD→deadtime→REV、REV→deadtime→FWD、超时/无效反馈coast及duty限幅。
- 这轮未修改生产`app_entry.c`、未构建或下载。USBee四个归一化制动边界向量8/8通过，见`docs/r16_validation_20261111.md`；这只验证决策阈值及正反方向符号，不验证物理空档。

## 2026-11-25 输出guard抽取与Ubuntu构建

- 新增`inc/lcdm_output_guard.h`，把原普通LCDM路径的方向空档、tick回绕和同向duty slew抽为生产/主机共用helper；`src/app_entry.c`普通输出路径改用该helper。实际行为沿用原规则：大于等于180‰的反向请求先coast 8ms，小请求沿原有slew规则降至0；customer直驱分支仍未改，也未接入新motion phase planner。
- `tools/r10_host_sim/test_lcdm_output_guard.c`以C99/C11、`-Wall -Wextra -Werror -fsanitize=undefined`分别运行，**9/9通过**：正反换向、deadtime等待/到期、tick回绕、slew斜率、低幅换向及大elapsed饱和。
- Ubuntu TSSOP20生产LCDM构建通过：`build/releases/V10.6_R10_OUTPUT_GUARD_20261125_R1/`；BIN 16,360B，SHA256=`a9541b7b7011d9e01156d68f443145b356e1a951e1698ef334f909211adde1b0`。manifest逐文件记录SDK、flags和源码哈希；`flashed=false`。这次只构建，没有下载或动作。
- helper SHA256=`38afa26bc22e398222fe1b3060d7a89542ce0084f8de77cf75a71c33ce9a28d4`；测试SHA256=`de1d5286d8e9537a69eba7907fff9976283a6ec84277c295f911a29f8497a5c0`；`src/app_entry.c` SHA256=`a8b2587abce0faab1c2837cd87cb8985b3785f93637aabd909c492781b4bc1cc`。
- **R10仍未通过**：输出guard生产构建和离板测试通过，但customer profile仍绕过该guard；新反馈阶段planner尚未接入，实体双路桥脚/速度、零速切断和无反向再加速还未测。下一步为新planner建立受保护的生产入口并构建显式候选；之后才适合首个中段低能量板测。

### 2026-11-25 快环接入检查与镜像容量

- `src/app_entry.c`增加默认关闭的`SERVO_ENABLE_LCDM_MOTION_PHASE_TEST`路径：复用反馈速度、已学习的制动样本/停车距离，阶段输出走普通方向换向空档、duty slew和方向报警；使用500ms绝对H桥截止。未训练模型时驱动请求为0。试验占空配置200‰/制动200‰/收敛80‰，仅用于受限候选构建，不是标定参数或产品默认值。
- `Makefile`增加特性开关并要求LCDM运行快环；`tools/build_v10_ubuntu.py`增加独立构建选项、关闭本试验不需要的触摸RX，并校验编译日志中的运行快环与阶段器宏。曾误用`--production-lcdm`构建，结果不包含位置控制快环且BIN与普通镜像相同；已识别并修正，不能把该次构建当成阶段器验证。
- 首次快环镜像超Flash 628字节。随后为阶段测试关闭触摸菜单/接收路径、保留只读诊断页并重建，得到可链接候选：`build/releases/V10.6_R10_R16_MOTION_PHASE_CANDIDATE_20261125_R10/`，BIN 17,452B，SHA256=`04b773f82bb6c8ef3e73dd415e21c50f34eab952fa110e55f05d4a9fc753ddef`，manifest=`flashed:false`。构建日志确认快环和阶段器宏均实际进入编译；代码区余量2,644B。镜像通过SWD写`g_lcdm_motion_phase_arm_request=0x50484153`单次arm，等待≥400µs命令变化后运行；200‰占空上限、8 ADC count位移锁存和500ms绝对截止。LCD页显示PWM/目标/反馈/误差/duty/阶段/arm状态。未下载。
- LCDM初始化与每次整页绘制落实固定顺序：切`page 0`并等待，`ref_stop`，`vis 255,0`，`tsw 255,0`，`sendxy=1`，自绘，`ref_star`后立即`ref_stop`；试验页按字段变化更新，避免高频重绘。R54既有触屏防花屏确认仍适用。
- 离板检查：客户制动算术12/12；阶段器C99/C11 + UBSan各25/25；输出guard C99/C11 + UBSan各9/9；阶段器+guard联合C99/C11 + UBSan各6/6；USBee切换边界8/8；合成对象13/13。均不构成R10实机通过。
- 联合用例覆盖反馈方向需反刹、反向前8ms coast、到期后允许反向输出、500ms截止和反馈无效零输出；源文件`tools/r16_host_sim/test_lcdm_motion_phase_guard.c`。
- 本轮输入哈希：`src/app_entry.c`=`6225fa6cbad1480e6af0012ae2b94eb2ef1e37a5591032ac50c60e793fb83408`；`Makefile`=`c505e7a81734ec5f999dab05d38e8908863c229ed9d6cd64bcc7f7f4eb15fd6c`；`tools/build_v10_ubuntu.py`=`54dc29bc8b1a52312ee6612128f62a374791322ac9d7749572d9410f92af049c`；联合测试=`e0ce34711df355526d4062d46c8d4d877fd7fb2ec2762e026afa22e3ccefde1a`；最终候选BIN=`04b773f82bb6c8ef3e73dd415e21c50f34eab952fa110e55f05d4a9fc753ddef`。
- **R10未通过**：候选已能构建，但仍未下载；还缺板上目标—反馈—速度—signed duty与双桥脚同步证据，以及零速归零/不反向再加速核验。无需用户操作；下一步先只读识别并备份板上整片，再选择这份候选做单次中段受限动作。

### 2026-11-25 R10候选下载与只读状态

- 下载前板为R66：20,480B整片SHA256=`e7c4147165c6b115c2fd735a0994c04fe61f86d25b39b3d626633f090cb80429`；应用与R66 BIN逐字节匹配，末384B与此前归档一致。备份`build/board_readback/20261125_motion_phase_preflash/before.bin`。
- R10候选按2MHz under-reset下载；读回20,480B整片SHA256=`971f9f28554dde101284d6bd2f2e9d59c66c692ae36d7552d8ee807d98570995`，前17,452B与R10 BIN逐字节匹配，末384B保持一致，版本`P50 V10.6R10R16P`，SP=`0x20000c00`、PC=`0x08004165`、核心Running。读回和报告在`build/board_readback/20261125_motion_phase_postflash/`。
- SWD只读3帧显示：phase=COAST、arm request=0、armed=0、customer active=0、signed duty=0、反馈约2390 ADC、VDD约3176–3182mV。没有触发马达；输入命令脉宽仍为无效初值65535，确认这版缺内部测试源，无法自行生成命令。这也是换成R11自生1500/1550µs源的原因。
- R10板上图像尚无用户目视确认；R10没有执行任何运动。此后R11候选增加SWD可控的内部命令源和一次性arm，输出仍限制200‰/8 ADC/500ms。

### 2026-11-25 R11动作复盘与R12候选

- R11内部源确有1500→1550µs切换，目标ADC约2048→2144；当前反馈约2392ADC，未观察到输出duty或位置响应。静态映射中心偏差约344ADC，导致该次动作不能用于判断H桥或阶段算法是否工作。
- R12在首次有效中立样本把1500µs目标锚定到当前反馈，并增加阶段signed-duty请求、最终HBridge duty、误差和反馈有效标志。候选BIN 17,476B SHA256=`e9b517d74e7060d3b5c63bd57f5dc742f0d87a4e0b27598a1c6be577dff188d6`，Ubuntu构建通过；默认内部源关闭、arm=0。
- R12以2MHz under-reset下载；20,480B整片SHA256=`791138f224a68a555c95aacefbcbe09bc77a0d9ebb3c4c1e62465aab989513d0`，17,476B应用与BIN匹配，尾384B与R11相同，核心Running，PC=`0x08004229`。只读快照确认arm=0、内部PWM=0、active=0、signed duty=0，反馈约2392ADC。当前板为R12 idle，尚未在R12运行动作。
- 先核实R12的中立锚定与新遥测字段后，再决定同一小幅限幅动作。不得直接放大PWM或位置范围。R10实板验收仍未通过；桥脚波形需要示波器/逻辑分析仪实测。

### 2026-11-25 R13–R16分级驱动结果（当前板R16）

- R13–R16均使用1500/1550µs内部命令、live反馈锚定、8 ADC count位移保护、500ms硬截止；只逐级调整启动占空请求下限40→160→200‰，上限始终200‰。每轮仅一次arm，结束后HBridge `s_duty=0`、state=COAST、核心Running。
- R14（请求峰值137‰）：501个有效阶段周期，阶段DRIVE→TIMEOUT，误差93–99 ADC；raw反馈2390–2394。正向PWM ISR计数增加3490、反向0。
- R15（请求峰值184‰）：501个有效周期，误差93–97 ADC；raw反馈2390–2394。正向PWM ISR计数增加4982、反向0。
- R16（请求峰值200‰）：501个有效周期，阶段位图含DRIVE与TIMEOUT，误差95–100 ADC；raw反馈2390–2395（动作监控raw总范围2389–2396）；正向PWM ISR计数增加4982、反向0。VDD约3158–3207mV。反馈变化仍落在原始静态波动范围内，不能确认位置改变。ISR计数证明固件执行了正向输出函数，不等价于示波器确认PA6/PB0电气波形。
- R16 BIN 17,772B SHA256=`d681df196f8ee06b0db11f1d309264cfb53940cd5c0f39afe611b98b79f83c0e`；下载后全片SHA256=`14c3f78c67b3f2afb346fe3c0b326603cd09743957baf41f8e6eeecae2a11fc6`，应用逐字节匹配、末384B与R15一致，核心Running。当前板R16，内部源仍启用但一次性arm已消费，active=0、实际HBridge duty=0/state=coast。
- **R01/R10未通过**：三档小范围正向请求都未获得可区分于静止噪声的ADC位移；反向制动、速度过零、真实H桥针脚波形和客户曲线仍未验证。先核对PA6/PB0波形或电机端PWM/电流，并确认机构实际响应；不继续盲目加大PWM。
