# R16 客户原始波形参考核对

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
