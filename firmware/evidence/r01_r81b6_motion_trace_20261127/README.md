# R81B6 单次arm轨迹（2026-11-27）

R81B6已under-reset下载并整片读回匹配。只读确认内部源包含1500/1550µs且active/duty均为0后，执行一次单次arm。

结果：arm前core Running、arm=0、armed=0、active=0、duty=0。采集24帧；motion phase全程COAST（phase=5），有效控制循环仅1次，误差仅1 ADC count，阶段器请求duty始终0；H桥最终duty最高36‰但只记录到1个PA6/PB0匹配的正向PWM tick，反向0 tick。随后active=0、arm=0、duty=0、core Running，raw/feedback约2519/2520；无可确认机械位移或制动。故本次不构成R01运动或制动通过。

全量只读快照见`post_motion_snapshot.jsonl`，24帧轨迹见`trace.csv`。没有再次arm或重试。
