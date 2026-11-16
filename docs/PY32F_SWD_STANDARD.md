# PY32F 系列统一读写规范

版本：2026-11-03。依据：用户明确指定，所有 PY32F 项目复用最近 DQ110/PY32F002B 已验证的 Ubuntu 路径。

## 一、执行摘要

统一沿用 Ubuntu + DAPLink + pyOCD HID 兼容入口。新项目只配置芯片和固件，不重新选择平台或安装工具。本次脚本更新经过模拟检查；当前会话无法建立 SSH，硬件尚未复验。全局安装完成前，本文件是待推广规范，不代表工作区外文件已经更新。

## 二、唯一默认路径

### 2.1 pickupbit 已验证经验（本次必须继承）

| 经验 | 本项目执行方式 |
|---|---|
| 配置是第一层 | pickupbit 的 `.codex/config.toml` 启用 workspace-write 网络访问；当前 MOTOR 缺该配置。更新配置后须在受信任项目中开启新会话；不能靠当前受限会话反复重试来生效 |
| 下载路径固定 | pickupbit `tools/download_py32_ubuntu.sh` 在 2026-11-03 以 `2M / under-reset` 首次真实烧录成功；本项目的同名脚本从该文件改成接收目标和 ELF，未再发明连接路径 |
| SSH 空参数 | 必须逐参数转义，保留空 UID，防止后续频率及模式参数错位 |
| USB 无需每次重连 | 默认复用 Ubuntu 已连接的 DAPLink；转接后枚举最多等待十次；没有证据不要求拔插或切 Mac |
| 下载重试 | `2M under-reset → 1M under-reset → 500k under-reset → 100k attach`；下载和读取的连接模式分别定义 |
| 验收分层 | 编译成功、Flash 下载成功、核心 Running、产品功能通过必须分别记录；不能用模拟验证代替真实烧录 |
| 不再重复阻塞排查 | 已确定是沙箱/SSH 权限问题时只报告一次；按 pickupbit 配置流程处理新会话，不再换下载工具 |
| 公共入口状态 | pickupbit 指定的公共 `download_py32_ubuntu.sh` 当前确实尚不存在；本次安装包已补上，但工作区外部署尚未完成 |

来源：`/Users/andrewlau/qtprj/pickupbit/AGENTS.md`、`pickupbit_py32_v0_1/tools/download_py32_ubuntu.sh`、`tools/setup_codex_network.sh`、`docs/始原网面版转.md`。不得把 pickupbit 的 B 型号固件拿来下载本项目的 A 型号芯片。

### 2.2 固定依赖

| 项目 | 固定规则 |
|---|---|
| 执行端 | Ubuntu；从 Mac 经 SSH 调用，或 Ubuntu 终端直接运行 |
| Ubuntu | `andrew@10.211.55.4` |
| Python | `/home/andrew/ARTERY/.tools/pyocd-venv/bin/python` |
| HID 后端 | `PYOCD_USB_BACKEND=hidapiusb` |
| 原有兼容入口 | `/tmp/dq110_pyocd_hid_compat.py` |
| 持久入口 | Ubuntu `~/.local/share/servo_swd/dq110_pyocd_hid_compat.py`，从已验证原文件保存，禁止猜写兼容补丁 |
| DFP | `/home/andrew/ARTERY/DQ110_FW/1.2.8.pack` |
| 探针 | 自动识别唯一 DAPLink；多探针才指定 UID |
| 下载入口（安装后） | `~/qtprj/PY32F002_COMMON/scripts/download_py32_ubuntu.sh -t <target> <当前ELF>` |
| 读取入口（安装后） | `~/qtprj/PY32F002_COMMON/scripts/py32_swd.py read --target <target>` |
| 项目区别 | 芯片型号、固件路径、保留参数区；不得为不同项目复制出不同连接方案 |

## 三、读写要求

| 操作 | 要求 |
|---|---|
| check | 检查 Ubuntu 依赖及探针可见性；不能宣称 MCU 已可读写 |
| read | attach 读取整个 Flash，不复位、不下载；保存原始 BIN、SHA256、SP/PC、可识别版本字符串及日志 |
| flash | 显式选择 BIN；检查长度及向量；完整备份通过后才下载；Ubuntu 保留下载前镜像；写后逐字节读回校验并确认运行 |
| 参数区 | 项目明确配置 `--reserve-tail`；PY32F002A 伺服当前保留尾部 384 字节（两个 128B 学习模型槽 + 一个 128B ServoParams 页），其他项目按实际布局，不套用伺服参数区 |
| MCU | 002A=`py32f002ax5` / 20KB；002B=`py32f002bx5` / 24KB。其他型号查对应 Pack 后扩展，不猜测容量或冒用 A/B |
| 日志 | 操作结果必须区分“脚本检查通过”“探针可见”“Flash 读取成功”“下载并运行成功” |

统一工具从 Mac 调用示例（下载使用 pickupbit 已验证的 ELF 流程；先读取备份，下载后再次读取核验）：

```bash
python3 ~/qtprj/PY32F002_COMMON/scripts/py32_swd.py read --target py32f002ax5
python3 ~/qtprj/PY32F002_COMMON/scripts/py32_swd.py read --target py32f002bx5
bash ~/qtprj/PY32F002_COMMON/scripts/download_py32_ubuntu.sh -t py32f002ax5 /path/to/firmware.elf
```

Ubuntu 内直接读取可从共享目录运行 Python 读取工具，加 `--local`；读取默认 1MHz attach，不复位。下载入口按 pickupbit 固定为 2MHz under-reset 起步，下载后 `reset halt / g / status`。Python 工具的 BIN 下载分支是新增且仅模拟验证的辅助功能，不替代已经真实验证的 ELF 下载入口。

Codex 配置依据：[官方配置参考](https://learn.chatgpt.com/docs/config-file/config-reference) 中的 `sandbox_workspace_write.network_access`，以及[工程配置与信任要求](https://learn.chatgpt.com/docs/config-file/config-advanced)。配置仍受宿主管理策略限制，不能保证仅写入文件就解除当前会话限制。

## 四、固定排错顺序

| 层级 | 证据及处理 |
|---|---|
| 执行权限 | SSH 建连即 `Operation not permitted`：当前执行环境阻止网络；停止 USB/SWD 调整。需要允许访问 Ubuntu 的会话，项目脚本不能解除宿主限制 |
| SSH | 超时、拒绝连接、认证失败：分别处理 VM／服务／认证，不调整 MCU 程序 |
| Ubuntu 依赖 | 缺 Python、Pack 或兼容脚本：检查已知固定位置和持久副本，不默认重装 |
| USB | Ubuntu 中无探针：查 Parallels 是否仍把 DAPLink 分配给 Ubuntu；只有证据到此层才处理 USB |
| SWD | 探针可见但目标连接失败：查目标供电、型号、接线、BOOT 和连接模式；不混用其他型号 |
| 固件 | 读取成功后再判断版本、控制程序和运行状态 |

禁止把不同层级错误混在一起反复试；禁止只凭 pyOCD 退出码 0 宣称成功；禁止把模拟测试当作板上实测。

## 五、项目接入

所有新 PY32F 项目先读本规范，引用公共工具。既有、已验证的产品下载脚本保留构建参数及项目流程，后续逐个验证后才把 SWD 部分转接公共入口。不得为了统一而立即替换未经复验的生产下载脚本。用户已授权的读写操作继续完成，不重复询问是否可以使用既定 Ubuntu 通道。
