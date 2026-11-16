#!/usr/bin/env python3
"""Install the prepared common PY32F tool and qtprj-wide agent rule.

Run only in a session that permits writes to ~/qtprj and PY32F002_COMMON.
Does not access or program hardware. Existing files are backed up.
"""
import datetime
from pathlib import Path
import shutil

ROOT = Path('/Users/andrewlau/qtprj')
PROJECT = Path(__file__).resolve().parents[1]
RULE = '''
## PY32F 系列统一读写规则（用户指定，2026-11-03）

- 所有 PY32F 系列读写先读 `/Users/andrewlau/qtprj/PY32F002_COMMON/docs/PY32F_SWD_STANDARD.md`；下载优先复用 pickupbit/DQ110 的公共 `scripts/download_py32_ubuntu.sh`，读取用 `scripts/py32_swd.py`，不得每个项目重新探索连接方式。
- 默认 Ubuntu SSH `andrew@10.211.55.4` → 已验证 DQ110 HID 兼容入口 → DAPLink；Ubuntu 内可用 `--local`。不默认切回 Mac、Windows，不重复安装，不无依据要求拔插。
- 必须明确型号：002A 为 `py32f002ax5`，002B 为 `py32f002bx5`；参数保留区按项目设置，不把 A/B 型号和容量混用。
- read 用 attach，完整备份并记录版本／向量／hash；flash 前备份通过，之后读回校验并确认运行。check 或模拟测试不能称为硬件读写成功。
- 固定排错顺序：执行权限 → SSH → Ubuntu 依赖 → USB → SWD → 固件。SSH 建立即 `Operation not permitted` 时停止 USB/SWD 试错，明确报告执行环境限制。
- HID 临时兼容脚本先保存至 Ubuntu 持久目录；缺失时查原有副本，不猜写补丁。既有已验证产品构建／下载流程保留，SWD 转接公共工具后必须复验。
- 本规则优先于项目历史中选用 macOS／prlctl 的旧连接说明；用户新的明确指示优先。
- 必须吸收 pickupbit 的配置经验：工程网络配置须在新会话加载；受信任项目的 `.codex/config.toml` 才生效，不能在网络受限的旧会话反复 SSH。设置被管理策略拒绝时不得绕过。
'''


def main():
    common = ROOT / 'PY32F002_COMMON'
    agents = ROOT / 'AGENTS.md'
    readme = common / 'README.md'
    # Preflight every destination before making changes. Permission failures are
    # reported, never routed around the host execution policy.
    import os
    for path in (ROOT, common, common / 'scripts', common / 'docs'):
        if not os.access(path, os.W_OK):
            raise SystemExit(f'Global installation not permitted: {path}')
    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    writes = {
        common / 'scripts/py32_swd.py': (PROJECT / 'tools/servo_swd_ssh.py').read_text(),
        common / 'scripts/download_py32_ubuntu.sh': (PROJECT / 'tools/download_py32_ubuntu.sh').read_text(),
        common / 'docs/PY32F_SWD_STANDARD.md': (PROJECT / 'docs/PY32F_SWD_STANDARD.md').read_text(),
    }
    original = agents.read_text()
    if '## PY32F 系列统一读写规则（用户指定，2026-11-03）' not in original:
        writes[agents] = original.rstrip() + '\n\n' + RULE.strip() + '\n'
    original = readme.read_text()
    marker = '## 2026-11-03 统一 Ubuntu 读写入口'
    if marker not in original:
        writes[readme] = original.rstrip() + '\n\n' + marker + '\n\n后续读写以 [PY32F_SWD_STANDARD.md](docs/PY32F_SWD_STANDARD.md) 为准，使用 `scripts/py32_swd.py`。上文 macOS／旧脚本说明保留作历史参考，新的执行路径统一使用 Ubuntu。\n'
    for path, content in writes.items():
        if path.exists():
            shutil.copy2(path, path.with_name(path.name + '.backup_' + stamp))
        path.write_text(content)
        print('Updated:', path)
    print('Global rule installed. Hardware access still requires an actual check/read.')


if __name__ == '__main__':
    main()
