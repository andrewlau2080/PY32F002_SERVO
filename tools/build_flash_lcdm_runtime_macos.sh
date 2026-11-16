#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYOCD="${PYOCD:-/tmp/pyocd-mac-venv/bin/pyocd}"
TARGET="${TARGET:-py32f002ax5}"
FREQUENCY="${FREQUENCY:-100000}"
PACK="${PACK:-/tmp/py32-pack/Puya.PY32F0xx_DFP.1.2.2.pack}"
BUILD_DIR="${PROJECT_DIR}/build"
LOG_DIR="${BUILD_DIR}/logs"
BUILD_LOG="${LOG_DIR}/lcdm_runtime_build.log"
FLASH_LOG="${LOG_DIR}/lcdm_runtime_flash.log"
RUN_LOG="${LOG_DIR}/lcdm_runtime_run.log"

mkdir -p "${LOG_DIR}"

MAKE_ARGS=(
  LTO=1
  SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1
  SERVO_LCDM_DIAG_ONLY=1
  SERVO_ENABLE_PA0_POT_POWER=1
  BOARD_TSSOP20_DEBUG_PINS=1
  SERVO_TJC_UART_ALT_PINS=1
  TJC_LCDM_BAUD_OVERRIDE=230400
  SERVO_TJC_RX_ENABLE=1
  SERVO_HBRIDGE_FORCE_HIZ=0
  SERVO_ENABLE_COMM=0
)

require_file() {
  local path="$1"
  if [ ! -e "${path}" ]; then
    echo "Missing required file: ${path}" >&2
    exit 2
  fi
}

require_log_pattern() {
  local pattern="$1"
  local description="$2"
  if ! grep -Fq -- "${pattern}" "${BUILD_LOG}"; then
    echo "Build preflight failed: missing ${description} (${pattern})" >&2
    echo "Refusing to flash because the LCDM board configuration is not proven in the build log." >&2
    exit 3
  fi
}

if [ ! -x "${PYOCD}" ]; then
  echo "pyOCD not found: ${PYOCD}" >&2
  exit 2
fi
require_file "${PACK}"

echo "Building LCDM runtime firmware with fixed field wiring:"
printf '  %s\n' "${MAKE_ARGS[@]}"

"${PROJECT_DIR}/tools/build_in_ubuntu.sh" "${MAKE_ARGS[@]}" 2>&1 | tee "${BUILD_LOG}"

require_log_pattern "-DSERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1" "LCDM runtime UI"
require_log_pattern "-DBOARD_TSSOP20_DEBUG_PINS=1" "TSSOP20 debug pin map"
require_log_pattern "-DSERVO_TJC_UART_ALT_PINS=1" "LCDM PA7/PB2 UART alternate pins"
require_log_pattern "-DLCDM_TJC_BAUD=230400U" "LCDM 230400 baud"
require_log_pattern "-DSERVO_ENABLE_PA0_POT_POWER=1" "PA0 potentiometer power"

require_file "${BUILD_DIR}/py32f002_servo.bin"
require_file "${BUILD_DIR}/py32f002_servo.hex"

eval "$(
  python3 - "${BUILD_DIR}/py32f002_servo.bin" <<'PY'
import re
import shlex
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
sp = int.from_bytes(data[0:4], "little")
pc = int.from_bytes(data[4:8], "little")
match = re.search(rb"P50 V[0-9A-Za-z.]+", data)
if not match:
    raise SystemExit("Cannot find P50 version string in binary")
version = match.group(0).decode("ascii")
version_addr = 0x08000000 + match.start()
print(f"INITIAL_SP=0x{sp:08x}")
print(f"RESET_PC=0x{pc:08x}")
print(f"VERSION_ADDR=0x{version_addr:08x}")
print(f"VERSION_TEXT={shlex.quote(version)}")
PY
)"

echo "Preflight OK:"
echo "  LCDM UART: PA7 TX / PB2 RX, 230400 bps"
echo "  Vector: SP=${INITIAL_SP} PC=${RESET_PC}"
echo "  Version: ${VERSION_TEXT} @ ${VERSION_ADDR}"

"${PYOCD}" flash \
  -t "${TARGET}" \
  --pack "${PACK}" \
  -f "${FREQUENCY}" \
  -O connect_mode=under-reset \
  "${BUILD_DIR}/py32f002_servo.hex" 2>&1 | tee "${FLASH_LOG}"

printf 'halt\nread8 %s 16\nwreg sp %s\nwreg pc %s\ngo\nexit\n' \
  "${VERSION_ADDR}" "${INITIAL_SP}" "${RESET_PC}" \
  | "${PYOCD}" commander \
      -t "${TARGET}" \
      --pack "${PACK}" \
      --frequency "${FREQUENCY}" \
      -O connect_mode=under-reset \
      -O cmsis_dap.prefer_v1=true \
      -O cmsis_dap.limit_packets=true \
      -O cmsis_dap.deferred_transfers=false 2>&1 | tee "${RUN_LOG}"

if ! grep -Fq "${VERSION_TEXT}" "${RUN_LOG}"; then
  echo "Run verification failed: version readback did not contain ${VERSION_TEXT}" >&2
  exit 4
fi

if ! grep -Fq "Successfully resumed device" "${RUN_LOG}"; then
  echo "Run verification failed: pyOCD did not resume the device" >&2
  exit 5
fi

echo "Download verified: ${VERSION_TEXT} is flashed and resumed with PA7/PB2 LCDM settings."
