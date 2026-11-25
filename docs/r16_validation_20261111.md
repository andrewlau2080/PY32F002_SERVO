# R16 客户原始波形参考核对

## 2026-11-25 USBee同步波形接入与源码差距

- 四份 `2026/*.usbeecomp` 的同步PWM/Analog1分析见 [`customer_usbeecomp_four_moves_analysis_20261125.md`](customer_usbeecomp_four_moves_analysis_20261125.md)。主驱动过程中反馈已朝目标变化；输出转到相反方向后，反馈仍继续朝目标变化，支持“提前反向制动，之后低能量收敛”的样机策略。`1500-2000`元数据采样率不同，载波频率仍待Suite时间光标复核，但不影响先定义按反馈切阶段的策略。
- 离板源码核查：`src/app_entry.c` 的customer profile分支按 `7ms drive + 2ms reverse brake + 3ms reverse tail + 24ms coast` 划阶段（第1363–1368行定义常数），不由误差/预测停车距离决定阶段转换；反向占空会经 `LcdmCustomerBrake_ScaleDuty()` 按沿原方向的反馈速度缩放，速度落入stop band时归零。该分支在customer-active期间直接给signed duty并绕过普通轨迹分支。
- 新USBee样品首个代表性主驱动段约35–203ms，明显长于现有7ms固定驱动阶段。记录和通道标签不足以证明旧7/2/3ms属于这些USBee动作，因此不能把其直接当样品时长；但也不能声称当前固定profile已复现新曲线。应把阶段转换改为反馈/停车距离触发，时间只作为独立硬超时保护。
- Analog1量化码估算的主驱动→反方向输出切换位置分别约为全行程的64%、55%、77%、82%（切换后还剩36%、45%、23%、18%）。这四个跨度/方向差异不支持固定百分比门槛；应由当前速度、学习制动能力和剩余距离预测切换。
- `HBridge_Brake()`在低层确实调用`HBridge_Coast()`，但这不等于customer profile的反向PWM不存在：profile在驱动期后将`s_lcdm_servo_customer_dir`取反并输出带符号duty，是反方向驱动/主动制动候选；`HBridge_Brake()`用于零输出制动接口时实际为coast。两者概念必须分开记录，最终电气制动行为仍需实板双路波形/电机电流确认。
- 本轮离板推进目标：抽出一个可主机验证的阶段决策函数，以位置误差方向、速度方向/大小、预测停车距离、剩余行程以及限时/限占空作输入，输出 DRIVE / REVERSE_BRAKE / SETTLE / COAST 和 signed duty；对噪声、速度反号、无效反馈、超时、距离不足的边界做向量验证。不得把客户样品约78–90%占空或35–203ms时长直接当作本机安全默认值。

### 2026-11-25 观察器接入后的闭环压力结果（更新）

- `inc/lcdm_motion_phase.h`补入`BRAKE_COAST`：若反向制动后观察到速度已越过零点并朝制动方向运动，立刻撤销反向duty，coast至低速带后才允许位置修正；模型未训练时另用`untrained_drive_duty_permille`限制驱动，配置为0即不驱动。纯决策层仍未接入`app_entry.c`。
- 阶段决策边界测试以C99、C11分别使用`-Wall -Wextra -Werror -fsanitize=undefined`运行，**22/22通过**。覆盖故障关闭、硬超时、限幅、未训练回退、速度反号撤刹、BRAKE_COAST持续到低速、驱动恢复及全阶段序列。
- 更新后的13组对象扫掠调用生产`lcdm_velocity_observer.h`，含1 ADC量化、1/2/4ms采样和确定性±1 ADC噪声；结果**7/13收敛、6/13超时**。失败集中于低增益对象（3项）和4ms+噪声（3项）。不得再引用上一版理想速度观察器的9/9作为当前结果。这个合成扫掠不代表真实马达参数，但足以证明现有观察器+阶段决策在这些压力条件下还未稳健，尚不能进入生产集成/下载。
- 对生产快观察器做斜坡起停表征：真实合成输入为+2 ADC/ms，速度输出达到理论值90%需**14ms**；停止后降至8 Q4/ms停止带需**7ms**。工具`tools/r03_host_sim/characterize_velocity_observer_latency.c`，实际运行结果见本次验证记录；这是滤波器自身离线表征，不等于机械停转时间。
- 当前源哈希：`inc/lcdm_motion_phase.h`=`1b355bb91bc91a1496c06651adafa46c559c2aea95dfa1effa184b124ebbfab7`；边界测试=`a2f10ce83e7c58714712b0d2c87b685cbc660c5111a2cd310964aa35097b5c0a`；闭环扫掠=`14c25f1545db4a7f04bb6dc1562d8884f3b9dc5558696ded119a227e1e02e7cf`；观察器表征=`b036de66378f7ebdb39b499b7219da98fc66103e195ca7a7c03bd8ee6edec22d`。
- **结论：R16仍未通过**。客户曲线已提取出归一化阶段特征，阶段函数边界向量通过；但阶段/观察器联合压力仿真失败6组，且决策函数未生产集成，客户输出与本机物理桥脚未同步对照。下一步先找出低增益欠驱与4ms噪声下的决策振荡/误判原因，保持合成参数为测试假设，不伪造产品默认值。

### 2026-11-25 离板修正复测（更新）

- 失败转移轨迹显示：固定驱动占空在接近目标时仍会维持高输出，导致制动后反向再加速、在目标两侧形成大幅来回。阶段决策层现加入两项可配置逻辑：剩余距离缩短时线性降低DRIVE duty；进入SETTLE带后按误差相对停止带的距离线性降低纠偏duty，确保误差仍高于停止带时请求不为零。仿真器还记录失败时的阶段切换/估算速度/对象速度，避免只看到“超时”而不知模式。
- 更新后C99/C11 + UBSan阶段边界测试 **25/25通过**。13种固定合成对象工况（目标20/50/100 ADC；1/2/4ms采样；含±1 ADC噪声；已训练模型及未训练回退）现**13/13于500ms内进入±2 ADC、低速带并连续coast 10ms**。在这组仿真参数中，DRIVE上限200‰、未训练上限120‰、SETTLE上限70‰，靠近目标的斜坡宽度80 ADC；这些是模型实验条件，不能拷贝为产品默认值或安全电流承诺。
- 当前复测哈希：`inc/lcdm_motion_phase.h`=`d7ad2768974229ec4981eb596bdbc4e457c91990b50a1c7a0e5707f10b6b9324`；边界测试=`64653ef474c12b281e3cfe190379df935ff9487571c84d4335f3781733cac5a2`；仿真/失败轨迹=`b46ee03ded22edccf8eebe4f5b1ccf71619fb178c48175f51d6d9da72ad5f02e`；观察器表征工具=`b036de66378f7ebdb39b499b7219da98fc66103e195ca7a7c03bd8ee6edec22d`。
- **仍未通过R16**：离板决策层和当前列明的合成对象集通过，但客户样机参数尚未拟合为本机模型，函数未接生产控制路径，也没有仿真板/实体H桥的同步输出、速度和位置闭环证据。下一步先将四份USBee曲线做时间归一化重放边界，再检查生产集成入口与输出互锁；未满足这些前置条件前不下载运行。

### 2026-11-25 USBee归一化边界向量及生产路径安全审查

- 合成对象现在直接调用生产`inc/lcdm_brake_predictor.h`的停车距离函数，而不是在仿真器中复制公式；C99/C11 + UBSan重跑仍为**13/13收敛**。
- 新增`tools/r16_host_sim/test_usbee_brake_boundary_vectors.c`，使用四份USBee归一化首段切换点约64%、55%、77%、82%作边界向量。每组在预测距离尚差1Q4时保持同向DRIVE，到达预测边界立即反向制动；正反方向各自的符号均核对。C99/C11 + UBSan共**8/8通过**。这验证planner不依赖固定完成百分比；不是对USBee原始逐采样数据的完整重放，也不拟合客户马达系数。
- 生产接入审查发现`src/app_entry.c`的`customer_profile_direct`分支会绕开普通分支的8ms换向空档、duty slew及方向报警检查。新决策器不能直接接到该直驱分支；需进入带换向guard的控制路径，并保证“guard等待时间不计为主动制动时间”、超时/反馈失效立即零输出。当前仅静态审查，未改生产调用点或构建固件。
- **连板前状态：**可以准备首个低能量的观察性板测候选，但在接入guard、复核HBridge duty限幅/硬截止并完成Ubuntu构建前不下载。上板首步应只在中段做限时、限占空的单向运动，采同步速度/位置/signed-duty/两桥脚；先确认换向空档和输出映射，再试目标前主动刹车。不得跳到端点或长行程。
- 本轮哈希：`tools/r16_host_sim/test_usbee_brake_boundary_vectors.c`=`a22e65b092c114c01e8b9ebaf7ddc16113740d706a0c2288aa8609a9bdcb9771`；更新后的仿真器=`fb73edaf77a578ffaa782935073c0b8af33bd17e295c78b10a19f55920dfdd48`。决策层/基础向量哈希分别见上节复测记录。R10/R16整项仍未通过，完整物理门槛见各项下一步。

### 2026-11-25 普通LCDM输出guard已抽取并构建

- 8ms换向空档与slew现由`inc/lcdm_output_guard.h`提供，并已用于普通`app_entry.c`输出路径；C99/C11 + UBSan 9/9，Ubuntu TSSOP20 production-LCDM build通过，BIN SHA256=`a9541b7b7011d9e01156d68f443145b356e1a951e1698ef334f909211adde1b0`，未下载。
- 这是连板前工程门槛的一部分：guard无需再从普通路径重写，但新planner尚未接入它，customer timed-direct仍绕过它。R16尚缺planner生产集成、真实速度/制动标定与我方实体桥脚同步波形；本轮没有操作仿真板。

### 2026-11-25 离板阶段决策器初版

- 新增`inc/lcdm_motion_phase.h`纯决策层：输入有效性、位置误差Q4、速度Q4/ms、预测停车距离、上一阶段、模型资格和硬超时；输出`DRIVE`、`REVERSE_BRAKE`、`SETTLE`、`COAST`或失败/超时状态及signed duty。只在速度与误差方向相反、或合格停车距离已覆盖剩余行程、或未训练模型进入配置的保守回退距离时开始反向制动；制动阶段在速度进入停止带前保持，避免预测距离随速度下降时阶段抖动。到位后小误差收敛，输入失效/配置不合法/硬超时则输出零duty。
- 模块不设客户样品的固定占空/时长；drive/brake/settle duty、停车裕量和回退距离由调用方配置。新增`untrained_drive_duty_permille`：制动模型未训练时只允许调用方指定的较低驱动上限，设为0时coast；硬超时仍独立生效。反向制动duty按速度缩放并限制在1000‰以内。
- `tools/r16_host_sim/test_lcdm_motion_phase.c`以C99/C11、`-Wall -Wextra -Werror -fsanitize=undefined`分别编译并运行，均**19/19通过**且无UBSan报告：故障关闭、超时、到位、驱动/刹车/收敛阶段、未训练回退、反向速度、占空限幅、整数极值、停车距离触发单调性及制动duty随速度下降单调性。首次运行发现测试样例未重置previous phase；显式传入上一周期状态后重跑通过。
- 新增`tools/r16_host_sim/simulate_lcdm_motion_phase.c`做9组简化对象扫掠，C99 `-Wall -Wextra -Werror -fsanitize=undefined`编译通过。对象只含一阶加速度/粘性阻尼、1 ADC位置量化、理想速度观测和人工设置的停车减速度；目标跨度20/50/100 ADC，合格模型/未训练回退均有覆盖。当前配置9/9在500ms内进入±2 ADC、低速并连续coast 10ms；峰值过冲约0–2.56 ADC。该数值是合成对象结果，不是客户曲线拟合或实机参数。没有模型时不同回退距离会改变时长与过冲，回退设置必须待首台测量，不设为产品默认值。
- 哈希：`inc/lcdm_motion_phase.h`=`10d24755b0c75eb21897e88bff78d9ed73cafac0188dc4db79e319bc8b2829a0`；测试源=`a3ca8357e5f6ca917a3d8dbd6d3b6542906a326c859e44b7f9c59ac6c7f4095d`；合成扫掠源=`f0ed4b74ac8edd11e190d0d9bf74fc11e5a05a25eaca09bb3bc2ea4bf9b37961`。
- **范围限制：**两个文件均为隔离离板层，未接入`app_entry.c`，未构建产品固件、未下载、未驱动仿真板。19/19及9/9仅证明决策边界与明确假设下的合成闭环可收敛，不证明真实电机的制动/角度性能。下一步需把客户四组相对阶段和误差/速度曲线接入R17对照，复核采样率差异；硬件输出仍待安全短行程实测。

> 2026-11-22用户新增指定三份客户样品：`../../2026/1000-1500.logicdata`、`../../2026/1500-2000.logicdata`、`../../2026/2000-1000.logicdata`。本次更新以这三份为动作样品补充分析；此前唯一 `CUSTOMER.logicdata` 的旧结论仍作为历史记录保留，不再将“仅 CUSTOMER”当作当前全部客户样品范围。

## 2026-11-22 三动作样品初步分析

- 三个文件均标注 Channel 2/3/6/7、Edge Finder、State 1；大小及输入SHA256分别为：1000-1500，190,568B，`f28e1c20b8cd06b185f1d1de132d2710be00f0dda751776b7ab8c07abeeade97`；1500-2000，220,480B，`27a8f440cff76f384c0309f3db72b9c27f25eb03ce9900315c38fed8182f9483`；2000-1000，335,143B，`f63170f3df9e5905e986bcbdfac91a17b936eda1f1f0e1a10be2ff4df22b745b`。
- 用既有`tools/analyze_logicdata_motor_pwm.py`处理三份均为`edge_blocks=0`。这是脚本固定记录布局不匹配，并非没有信号。本机没有Logic 1，故还没有精确边沿/电平解码。精确何时加速、减速、刹车和补偿仍待导出通道数字CSV/VCD；不能根据文件大小或Edge Finder标签补造毫秒数、占空比或桥真值表。
- 机理初步解释及数据界限见`docs/customer_logicdata_three_moves_analysis_20261122.md`。根据用户对波形的观察，加速/行进/预刹/补偿是合理结构；到底是反向转矩、动态短路制动还是coast后补偿，需要逐路电平、接线映射及最好同步位置ADC确认。
- `2000-1000.logicdata`的文件名表示高到低，但用户强调“起点1000µs”。报告暂保留该说明并标为语义待确认，不擅自把采样首值当成指令起点。
- 最小后续输入：Logic 1导出三文件的Channel 2/3/6/7数字VCD或按变化逐行记录的时间戳CSV；无需重新采集。R16仍未通过，当前没有依据修改/下载我方刹车逻辑。

### 用户确认的每文件往返解释（2026-11-22补充）

- 每份样品的两路波形分别代表正向驱动和反向驱动；前段按文件名顺序运行，接近该目标时施加反向驱动作为主动刹车，之后再按回程方向运行（例如1000→1500，临近1500反向制动，然后1500→1000）。回程靠近终点时也需按同一逻辑识别反向刹车/补偿。
- 此解释作为波形分段和事件分类的权威语义，不再将两路当作独立无关的PWM例子。通道到正/反向的实际映射、脉宽、开始刹车时刻、死区和补偿参数仍需数字电平导出后计算；精确波形解码仍未完成。
- 已补PID/轨迹控制解释：位置误差驱动前进，速度反馈/制动距离预测在剩余距离不足时使净转矩反号，主动反向制动；停稳后再用小幅位置纠正补残差。I项要限幅并anti-windup，不能代替高速刹车。源码核对确认当前`HBridge_Brake()`仍调用`HBridge_Coast()`，故当前R66并未实现客户样品所示反向主动刹车；未修改固件。分析文档对这些内容标为控制机理解释，未伪称为波形测量结论。
- 用户进一步明确分析目的：学会客户的整条运动曲线，作为我方将来点动/段动的主要控制策略，再由本机反馈修正；不能只使用恒定前进PWM后coast。分析文档已加入可复用的起动、加速、行进、预刹、零速捕获、补偿状态与长短行程曲线压缩原则。R66 coast后的超count正是缺少减速/主动刹车段的实测例子。

日期：2026-11-11  
唯一客户参考：`../PY32F002_SIGNAL_CAPTURE/CUSTOMER.logicdata`  
结论：**客户文件选择与时间边沿提取已完成；H 桥逻辑原因仍未充分解码，R16 未通过。**

## 已核对证据

- 历史解析只使用用户指定的 `CUSTOMER.logicdata`，未将 `2026/PA7串口*.logicdata` 混入客户参考。
- Saleae官方说明：旧`.logicdata`需Logic 1.x打开，Logic 2使用不同的`.sal`格式；`.logicdata`由复杂Boost序列化生成，官方建议通过软件导出功能取数。[打开旧`.logicdata`文件](https://www.saleae.com/support/logic-software/saving-and-exporting-data/open-logicdata-files)、[`.logicdata`格式说明](https://www.saleae.com/support/logic-software/saving-and-exporting-data/can-i-create-or-edit-logicdata-files)。
- 重新检查原始二进制的Boost序列化元数据，除 `Channel 2` 外还明确含 `Channel 3`、`Channel 6`、`Channel 7`、`Edge Finder` 和 `State 1` 标签。旧 `customer_blocks()` 只搜索32字节格式的Edge Finder事件记录并将其解释为Channel 2时间戳，**没有解出其它数字通道的逻辑值**。因此此前“原档只有一个通道”的说法错误，现撤回。
- 从 CUSTOMER 文件 Channel 2 恢复约 49.89–49.91 kHz 重复边沿标记；4 个动作窗口跨度约 11.2–13.3 ms。最清晰末组时间段为 7.195、2.006、3.166 ms，活动边沿数分别 360、101、159。
- 这些结果仍只给出Edge Finder事件时标。文件使用Saleae Logic 1 `.logicdata`格式，内部Boost二进制序列化；Mac的`/Applications`/PATH及Ubuntu构建机的PATH、`/opt`、`/usr/local`与用户目录均未找到Logic 1导出程序。需在有Logic 1的电脑打开既有文件并导出数字通道CSV或VCD，才能逐路计算边沿极性、高低电平脉宽、占空比和方向对应关系。原文件SHA256=`d9fd4d9b12a5e90310a55cef58a3b5bb369e9c5b84befa89f976c1585db59c2c`。7/2/3ms只是当前事件分组结果；930/360/220及制动机理不能由这些事件计数推出。已新增`tools/analyze_customer_vcd.py`：收到VCD后自动生成边沿CSV、脉宽汇总CSV和manifest，manifest保留输入哈希、timescale及时间范围，并明确VCD未必含采样率/触发偏移。语法、CLI和临时VCD端到端检查通过；验证多行timescale、带空格通道名、初始电平标记、边沿计数、完整脉宽µs换算及未知电平间断不跨段计宽；捕获开头的局部脉宽也不计入统计。没有真实VCD输入，尚无客户数字逻辑解码结论。
- 既有对照页将 CUSTOMER 原始边沿置于上方；我方下方是 signed duty 到正/反方向的逻辑包络，不是实际引脚采样。SWD 采样也无法解析约 20µs 载波，更不能推导客户两路 MOSFET 真值表。

## 未通过原因与所需协助

- 未解出Channel 2/3/6/7哪个对应H桥两路、方向和PWM；未解出反向吸能/短路制动/滑行逻辑，尚不足以把客户电路作为确证基线。
- 离板所需输入已缩小为一个文件转换：用Saleae Logic 1打开既有 `CUSTOMER.logicdata`，把Channel 2/3/6/7全部导出为VCD（或每通道时间/电平CSV），保留原始采样率和触发时间，不需重新采集。通道到电路的接线映射若文件无标签，需再从原接线图或捕获记录确认。
- 我方要完成同源比较，需 DAPLink/仿真板及示波器或逻辑分析仪同时采集 MCU 两路 H 桥物理引脚，并按动作起点对齐；不能用估算占空包络冒充引脚波形。

R01–R15及R17–R22状态按对应记录；R16新发现已更正，本轮只做原始文件元数据检查并准备VCD解码器，没有马达动作。唯一待用户协助项是用Logic 1转换现有文件；不需重新采集。若原文件/导出没有接线标注，还需对应的通道到H桥测点映射。

## 2026-11-19 VCD 对照工具接通

- `tools/render_customer_channel_compare.py`新增`--customer-vcd`输入，可从Logic 1导出的VCD提取Channel 2/3/6/7单比特数字轨迹，并显示最后一段连续活动；有VCD时不再尝试解析原`.logicdata`。原始Edge Finder显示路径仍保留。
- 同一工具新增`--customer-csv`，支持`Time [s/ms/us/µs/ns]`和Channel 2/3/6/7列的数字电平CSV；VCD与CSV只能择一。合成`Time [ms]`、`Time [ns]`输入均能渲染四路，非法时间单位或缺失通道会明确拒绝；VCD的`x/z`区间会在图上断开，不会当作已知电平延伸。
- 用户需要导出时，在Logic 1的`Options → Export data`选CSV、时间戳（秒）、每个数字通道单独一列、仅在状态变化时写行，并导出Channel 2/3/6/7。Saleae [Logic 1用户指南](https://www.saleae.com/support-assets/Legacy%20Logic%201.x%20User%20Guide.pdf)说明数字CSV可按时间戳、每bit一列、每次变化一行导出；解析器兼容`Channel 2`和`Ch2`名称。原始数据文件未改动。
- 合成四通道VCD端到端页面检查通过：四路阶跃均生成SVG轨迹；HTML/SVG明确说明客户采集与我方VSG曲线分别归零、不是同步实测。`Channel 2`/`Ch2`两种标签形式均已在VCD/CSV输入验证。`py_compile`、`--help`、`git diff --check`通过。测试输出在临时目录，未写入项目证据目录。
- 再用唯一真实`../PY32F002_SIGNAL_CAPTURE/CUSTOMER.logicdata`运行旧版Edge Finder路径成功：识别3个连续事件块并生成SVG；未修改原文件，也没有把Edge Finder事件冒充数字电平。
- 这只是导入/绘图准备，不是客户文件解码。当前没有真实VCD，客户高低脉宽、探头极性、桥臂真值表仍未确认；R16未通过。待用户在Logic 1导出既有文件，无需重新采集。

## 2026-11-25 快环阶段器接入状态

- `src/app_entry.c`现有显式编译开关接入阶段器，使用快环signed速度、制动学习资格与停车距离估计；输出导入普通换向保护路径，旧固定客户时序profile只在开关关闭时生效。候选关闭触摸RX。
- 修正构建过程的两个误判源：错误的`--production-lcdm`配置未编译位置控制快环；新增Makefile宏映射和构建日志校验，防止参数存在于manifest却没有进入C编译定义。
- 首版快环候选超Flash 628B。移除实验不需的触摸菜单/接收路径并保留只读诊断页后，候选`build/releases/V10.6_R10_R16_MOTION_PHASE_CANDIDATE_20261125_R10/`构建通过，BIN 17,452B，SHA256=`04b773f82bb6c8ef3e73dd415e21c50f34eab952fa110e55f05d4a9fc753ddef`，代码区余量2,644B，manifest=`flashed:false`。候选仅在SWD单次arm后响应≥400µs命令变化；限200‰、8 ADC count位移及500ms绝对截止，尚未下载。
- LCDM覆盖遵守已验证顺序`page→等待→ref_stop→vis 255,0→tsw 255,0→sendxy=1→自绘→ref_star→立即ref_stop`；试验运行时按变化字段更新，不编译触摸菜单。
- 离板回归结果：阶段决策C99/C11 + UBSan各25/25，输出guard各9/9，阶段器+guard联合各6/6，USBee归一化切换边界8/8，合成对象13/13。它们不等同于完整客户波形重放或实机验证。
- **R16仍未通过**：候选可构建但未下载；客户参考完整逐采样回放和我方ADC/速度/双路H桥同步波形未测。下一步先识别并备份板上当前镜像，再做单次受限动作；现阶段无需用户操作。
