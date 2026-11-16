#!/usr/bin/env bash
# Run once from a normal terminal, outside the existing Codex sandbox.
set -euo pipefail

WORKSPACE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
CONFIG_DIR="$WORKSPACE_DIR/.codex"
CONFIG_FILE="$CONFIG_DIR/config.toml"

# Do not leave a partial configuration or overwrite unrelated settings.
mkdir -p "$CONFIG_DIR"
STAGED_CONFIG="$(mktemp "$CONFIG_DIR/network-config.XXXXXX")"
trap 'rm -f "$STAGED_CONFIG"' EXIT
cat > "$STAGED_CONFIG" <<'CONFIG'
# PY32 downloads use SSH to the Ubuntu VM.
sandbox_mode = "workspace-write"
approval_policy = "on-request"

[sandbox_workspace_write]
network_access = true
CONFIG

if [ -e "$CONFIG_FILE" ]; then
    if ! cmp -s "$STAGED_CONFIG" "$CONFIG_FILE"; then
        echo "已有其他工程配置，未覆盖：$CONFIG_FILE" >&2
        echo "请让 Codex 检查并合并此配置。" >&2
        exit 1
    fi
else
    cp -n "$STAGED_CONFIG" "$CONFIG_FILE"
fi
if ! cmp -s "$STAGED_CONFIG" "$CONFIG_FILE"; then
    echo "配置安装未通过检查：$CONFIG_FILE" >&2
    exit 1
fi

echo "已安装工程持久配置：$CONFIG_FILE"
echo "当前 Codex 会话的权限不会原地改变。"
echo "请退出当前会话，在此工程目录重新运行 codex，开启新会话。"
echo "新会话中说：按 AGENTS.md 下载固件，并确认 SSH 网络权限已生效。"

printf '恢复本次项目：cd "%s/PY32F002_SERVO" && codex resume 01a1a12c-ddd7-7722-8ee9-35d8c50d17d3\n' "$WORKSPACE_DIR"
