#!/usr/bin/env bash
set -euo pipefail

# pickupbit：完全沿用 DQ110 的 PY32F Ubuntu 下载方式。

# Shared downloader adapted from pickupbit's verified DQ110 path.
# Transport and retry logic retained; no build or USB reassignment by default.
REMOTE_HOST="${PY32_REMOTE_HOST:-andrew@10.211.55.4}"
REMOTE_PYTHON="${PY32_REMOTE_PYTHON:-/home/andrew/ARTERY/.tools/pyocd-venv/bin/python}"
REMOTE_COMPAT="${PY32_REMOTE_COMPAT:-/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py}"
REMOTE_PACK="${PY32_REMOTE_PACK:-/home/andrew/ARTERY/DQ110_FW/1.2.8.pack}"
REMOTE_ELF="${PY32_REMOTE_ELF:-/tmp/py32_current_firmware.elf}"
TARGET="${PY32_TARGET:-}"
PROBE_UID="${PY32_PROBE_UID:-}"
SWD_FREQ="${PY32_SWD_FREQ:-2M}"
CONNECT_MODE="${PY32_CONNECT_MODE:-under-reset}"
while [ "$#" -gt 0 ]; do
    case "$1" in
        -t) TARGET="$2"; shift 2 ;;
        -u) PROBE_UID="$2"; shift 2 ;;
        -*) echo "Unknown option: $1" >&2; exit 2 ;;
        *) LOCAL_ELF="$1"; shift ;;
    esac
done
if [ -z "$TARGET" ] || [ -z "${LOCAL_ELF:-}" ] || [ ! -f "$LOCAL_ELF" ]; then
    echo "Usage: $0 -t py32f002ax5|py32f002bx5 firmware.elf" >&2
    exit 2
fi
case "$LOCAL_ELF" in *.elf) ;; *) echo "Expected current build ELF" >&2; exit 2 ;; esac
case "$TARGET" in py32*) ;; *) echo "Invalid PY32 target" >&2; exit 2 ;; esac
# Fixed remote filename must remain a simple shell-safe path for SCP and pyOCD.
case "$REMOTE_ELF" in *[!a-zA-Z0-9_./-]*) echo "Unsafe remote ELF path" >&2; exit 2 ;; esac

echo "PY32F系列下载方式"
echo "[2/3] 传输固件到 Ubuntu"
scp "$LOCAL_ELF" "$REMOTE_HOST:$REMOTE_ELF"

echo "[3/3] 自动识别 DAPLink、下载并运行"
# SSH 会把命令参数拼成字符串交给远端 shell；逐项转义以保留空 UID。
printf -v REMOTE_COMMAND '%q ' bash -s -- \
    "$REMOTE_PYTHON" "$REMOTE_COMPAT" "$REMOTE_PACK" "$REMOTE_ELF" \
    "$TARGET" "$PROBE_UID" "$SWD_FREQ" "$CONNECT_MODE"
ssh "$REMOTE_HOST" "$REMOTE_COMMAND" <<'REMOTE_SCRIPT'
set -euo pipefail

PYTHON="$1"
COMPAT="$2"
PACK="$3"
ELF="$4"
TARGET="$5"
PROBE_UID="$6"
REQUESTED_FREQ="$7"
REQUESTED_MODE="$8"

if [ -z "$PROBE_UID" ]; then
    # Parallels 转接成功后，Ubuntu 的 USB 枚举及 udev 权限可能尚未就绪。
    for attempt in {1..10}; do
        if LIST="$(PYOCD_USB_BACKEND=hidapiusb "$PYTHON" "$COMPAT" list 2>&1)"; then
            PROBE_UID="$(printf '%s\n' "$LIST" | grep -Eo '[[:alnum:]]{20,}' | head -n 1 || true)"
        fi
        if [ -n "$PROBE_UID" ]; then
            printf '%s\n' "$LIST"
            break
        fi
        if [ "$attempt" -lt 10 ]; then
            echo "等待 Ubuntu 识别 DAPLink（$attempt/10）..."
            sleep 1
        fi
    done
    if [ -z "$PROBE_UID" ]; then
        printf '%s\n' "$LIST"
    fi
fi
if [ -z "$PROBE_UID" ]; then
    echo "错误：没有识别到可用的 DAPLink 仿真器。" >&2
    exit 2
fi

echo "使用 DAPLink UID: $PROBE_UID"

flash_try() {
    local frequency="$1"
    local mode="$2"
    local output status
    echo "尝试 frequency=$frequency connect=$mode"
    set +e
    output="$(PYOCD_USB_BACKEND=hidapiusb "$PYTHON" "$COMPAT" commander \
        -u "$PROBE_UID" \
        -t "$TARGET" \
        --pack "$PACK" \
        -f "$frequency" \
        -M "$mode" \
        -O cmsis_dap.prefer_v1=true \
        -O cmsis_dap.limit_packets=true \
        -O cmsis_dap.deferred_transfers=false \
        -c "load $ELF" \
        -c "reset halt" \
        -c "g" \
        -c "status" 2>&1)"
    status=$?
    set -e
    printf '%s\n' "$output"
    if [ "$status" -eq 0 ] &&
       printf '%s\n' "$output" | grep -Fq 'Successfully resumed device' &&
       printf '%s\n' "$output" | grep -Eq 'Core .*Running' &&
       ! printf '%s\n' "$output" | grep -Eqi 'Error:|read thread exited unexpectedly|not halted as expected'; then
        return 0
    fi
    return 1
}

if flash_try "$REQUESTED_FREQ" "$REQUESTED_MODE" ||
   flash_try 1M under-reset ||
   flash_try 500k under-reset; then
    echo "下载完成，目标板已复位并进入运行状态。"
    exit 0
fi

echo "错误：pyOCD下载未完成。" >&2
echo "为避免部分擦写后继续以 attach 模式写入，本脚本仅重试 under-reset。" >&2
exit 3
REMOTE_SCRIPT

echo "下载完成，目标板已进入运行状态。"
