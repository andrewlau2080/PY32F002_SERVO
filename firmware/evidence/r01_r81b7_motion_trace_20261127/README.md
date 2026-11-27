# R81B7 单次受限测试（2026-11-27）

Ubuntu构建BIN 18,496B，SHA256 `3f2b5c6eba82a4178b624d93dd458bf664c3979b569fd7139c2233c0ec41e815`。under-reset下载后整片读回SHA256 `2dffcb448b6d1ae400c82e020be25b29b9ebbe25039755a9e7d801795dc71aa1`，应用与BIN匹配，尾384B与下载前备份相同。

arm前确认内部1500/1550µs命令源、active=0、duty=0、桥tick=0。仅做一次200‰、80ms硬截止、8-count保护动作。结果：phase mask=38（DRIVE、反刹相位、COAST均出现），有效阶段循环25次，requested duty峰值200‰；正向PA6/PB0匹配tick累计238，错配0，反向tick=0。observer反馈曾在起点2519附近下降到2498（约21 count变化），随后结束在反馈2523附近。动作结束active=0、arm=0、H桥duty=0、core Running。故启动门槛修复有效，但反刹相位没有转化为可观测的反向桥输出；不判为主动制动通过，R01仍未通过。

轨迹24帧和最终快照分别见`trace.csv`、`post_motion_snapshot.jsonl`。没有再次arm或重试。
