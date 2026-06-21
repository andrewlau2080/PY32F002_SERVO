#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYOCD="${PYOCD:-/tmp/pyocd-mac-venv/bin/pyocd}"
TARGET="${TARGET:-py32f002ax5}"
FREQUENCY="${FREQUENCY:-2000000}"
ELF="${ELF:-${PROJECT_DIR}/build/py32f002_servo.elf}"
HEX="${HEX:-${PROJECT_DIR}/build/py32f002_servo.hex}"
PROBE_UID="${PROBE_UID:-}"
PACK="${PACK:-/tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack}"

if [ ! -x "${PYOCD}" ]; then
  echo "pyOCD not found: ${PYOCD}" >&2
  echo "Install it with: python3 -m venv /tmp/pyocd-mac-venv && /tmp/pyocd-mac-venv/bin/pip install pyocd" >&2
  exit 2
fi

if [ ! -f "${ELF}" ] || [ ! -f "${HEX}" ]; then
  echo "Missing build outputs. Build or copy ${ELF} and ${HEX} first." >&2
  exit 2
fi

probe_list="$("${PYOCD}" list || true)"
if ! printf '%s\n' "${probe_list}" | grep -q 'CMSIS-DAP'; then
  echo "No CMSIS-DAP probe visible to macOS." >&2
  echo "Disconnect DAPLink from the Ubuntu VM in Parallels, then retry." >&2
  exit 3
fi

pack_arg=()
if [ -f "${PACK}" ]; then
  pack_arg=(--pack "${PACK}")
fi

target_list="$("${PYOCD}" list --targets "${pack_arg[@]}" || true)"
if ! printf '%s\n' "${target_list}" | grep -q "${TARGET}"; then
  echo "pyOCD target ${TARGET} is not installed in this macOS pyOCD environment." >&2
  echo "Install/copy the Puya PY32F0xx CMSIS-Pack or set PACK=/path/to/Puya.PY32F0xx_DFP.pack." >&2
  exit 4
fi

vector_line="$(arm-none-eabi-objdump -s -j .isr_vector "${ELF}" | awk '$1 == "8000000" || $1 == "08000000" { print $2 " " $3; exit }')"
if [ -z "${vector_line}" ]; then
  echo "Cannot read .isr_vector from ${ELF}" >&2
  exit 1
fi

set -- ${vector_line}

le32_to_hex() {
  value="$1"
  printf '0x%s%s%s%s\n' "${value:6:2}" "${value:4:2}" "${value:2:2}" "${value:0:2}"
}

initial_sp="$(le32_to_hex "$1")"
reset_pc="$(le32_to_hex "$2")"

probe_arg=""
if [ -n "${PROBE_UID}" ]; then
  probe_arg="--uid ${PROBE_UID}"
fi

echo "Flash image: ${HEX}"
echo "Run vector: SP=${initial_sp} PC=${reset_pc}"

TEMP_HEX="/tmp/py32f002_servo_flash.hex"
cp "${HEX}" "${TEMP_HEX}"

run_pyocd_session() {
  local frequency="$1"
  local mode="$2"

  echo "Trying pyOCD frequency=${frequency} connect=${mode}"
  printf 'halt\nload %s\nwreg sp %s\nwreg pc %s\ngo\nexit\n' "${TEMP_HEX}" "${initial_sp}" "${reset_pc}" \
    | "${PYOCD}" commander -t "${TARGET}" --frequency "${frequency}" ${probe_arg} \
        "${pack_arg[@]}" \
        -O connect_mode="${mode}" \
        -O cmsis_dap.prefer_v1=true \
        -O cmsis_dap.limit_packets=true \
        -O cmsis_dap.deferred_transfers=false
}

run_pyocd_session "${FREQUENCY}" under-reset || \
run_pyocd_session 1000000 under-reset || \
run_pyocd_session 500000 under-reset || \
run_pyocd_session 100000 attach
