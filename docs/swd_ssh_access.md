# Ubuntu SSH 仿真器读写路径

## 一、当前结论

| 项目 | 配置／状态 |
|---|---|
| 路径 | macOS → SSH → Ubuntu → HID 兼容脚本 → DAPLink → PY32F002A |
| 来源 | DQ110 的 `tools/download_dq110.sh`（PY32F002B 已验证路径） |
| Ubuntu | `andrew@10.211.55.4` |
| Python | `/home/andrew/ARTERY/.tools/pyocd-venv/bin/python` |
| HID 兼容入口 | `/tmp/dq110_pyocd_hid_compat.py` |
| 固件包 | `/home/andrew/ARTERY/DQ110_FW/1.2.8.pack` |
| 目标 | `py32f002ax5`，本项目使用 A 型，不沿用 DQ110 的 B 型 |
| USB 后端 | `PYOCD_USB_BACKEND=hidapiusb`；自动识别唯一探针 |
| SWD | 默认 1MHz、attach；读取不复位，下载后 reset halt / go |
| 2026-11-03 实测 | 当前 Codex 会话 SSH 返回 `Operation not permitted`，尚未完成板上读写验证 |

## 二、使用方法

### 2.1 在 Ubuntu 内直接操作（无 SSH 依赖）

在 Ubuntu 终端进入共享项目目录：

```bash
cd "/mnt/hgfs/qtprj/Steering Engine/MOTOR/PY32F002_SERVO"
python3 tools/servo_swd_ssh.py check --local
python3 tools/servo_swd_ssh.py read --local

# 明确选定固件后才执行此命令：
python3 tools/servo_swd_ssh.py flash build/py32f002_servo.bin --local
```

`check` 只检查依赖和探针可见性，不代表目标 MCU 已能读取。`read` 成功后报告才会包含完整 Flash 的版本、向量、SHA256。

首次找到原有 `/tmp/dq110_pyocd_hid_compat.py` 时，工具把该文件复制到 Ubuntu 的 `~/.local/share/servo_swd/dq110_pyocd_hid_compat.py`。以后临时文件消失时自动使用此持久副本；若两个位置都不存在，会明确退出，不猜测兼容补丁。该复制逻辑已模拟验证，尚未在本次 Ubuntu 会话实际部署。

可通过 `--python`、`--compat`、`--pack` 指定 Ubuntu 中的实际依赖路径；多探针时使用 `--uid`。`--output` 可指定新的备份目录，工具拒绝覆盖已有目录。

### 2.2 从 Mac 连接 Ubuntu

在 `PY32F002_SERVO` 目录运行：

```bash
# 读取全部 20KB Flash，包含最后 128 字节参数页；不下载固件。
python3 tools/servo_swd_ssh.py read

# 仅在明确选定待测试固件后下载，不自动编译或选择版本。
python3 tools/servo_swd_ssh.py flash build/py32f002_servo.bin
```

| 操作 | 记录／判定 |
|---|---|
| 读取 | `build/board_readback/<时间>/before.bin`、`report.json`、`pyocd.log`、`ssh.log` |
| 版本识别 | 报告记录版本字符串及地址、启动 SP/PC、SHA256、字节数 |
| 下载前 | 检查 BIN 长度和向量；先读取全部 Flash，读取失败则不下载；在 Ubuntu `~/servo_swd_backups/` 保留备份 |
| 下载后 | 保存 `after.bin`；逐字节核对所选 BIN，核对末尾参数页未变，并要求 resumed 成功文本 |
| 路径变化 | 可用 `SERVO_REMOTE_HOST`、`SERVO_REMOTE_PYTHON`、`SERVO_REMOTE_COMPAT`、`SERVO_REMOTE_PACK` 覆盖默认值 |
| 临时目录失效 | 首次可用时保存 HID 兼容入口至 Ubuntu `~/.local/share/servo_swd/`；以后优先按指定路径查找，再使用持久副本 |

脚本的模拟校验不代表硬件验证。只有报告 `verified: true` 才表示本次板上操作通过检查。

## 三、连接限制

`ssh ...: Operation not permitted` 表示本机执行环境拒绝建立网络连接，尚未到 SSH 认证或 SWD 阶段。更换芯片型号、降低 SWD 频率或拔插 DAPLink不能修复这一层限制。

当前会话不允许申请沙箱外执行，项目脚本也不能改变宿主应用权限。需在允许访问本机局域网的终端／会话执行上述读取命令，才能完成实际板上验证。不要为此关闭 Ubuntu SSH 安全设置或修改目标板 Flash。
