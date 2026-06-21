#!/usr/bin/env bash
set -euo pipefail

VM_NAME="${VM_NAME:-Ubuntu Linux}"
REMOTE_DIR="${REMOTE_DIR:-/tmp/py32f002_servo}"
PROJECT_REMOTE="${PROJECT_REMOTE:-${REMOTE_DIR}/PY32F002_SERVO}"
ELF="${ELF:-build/py32f002_servo.elf}"
HEX="${HEX:-build/py32f002_servo.hex}"
PYOCD="${PYOCD:-/opt/py32-venv/bin/pyocd}"
TARGET="${TARGET:-py32f002ax5}"
FREQUENCY="${FREQUENCY:-2000000}"
PROBE_UID="${PROBE_UID:-}"

remote_cmd=$(cat <<EOF
set -euo pipefail

cd '${PROJECT_REMOTE}'

vector_line=\$(arm-none-eabi-objdump -s -j .isr_vector '${ELF}' | awk '\$1 == "8000000" || \$1 == "08000000" { print \$2 " " \$3; exit }')
if [ -z "\${vector_line}" ]; then
  echo "Cannot read .isr_vector from ${ELF}" >&2
  exit 1
fi

set -- \${vector_line}

le32_to_hex() {
  value="\$1"
  printf '0x%s%s%s%s\\n' "\${value:6:2}" "\${value:4:2}" "\${value:2:2}" "\${value:0:2}"
}

initial_sp=\$(le32_to_hex "\$1")
reset_pc=\$(le32_to_hex "\$2")

echo "Flash image: ${HEX}"
echo "Run vector: SP=\${initial_sp} PC=\${reset_pc}"

probe_arg=""
if [ -n '${PROBE_UID}' ]; then
  probe_arg="--uid '${PROBE_UID}'"
fi

run_pyocd_session() {
  backend="\$1"
  frequency="\$2"
  mode="\$3"

  echo "Trying pyOCD backend=\${backend:-default} frequency=\${frequency} connect=\${mode}"
  if [ -n "\${backend}" ]; then
    export PYOCD_USB_BACKEND="\${backend}"
  else
    unset PYOCD_USB_BACKEND || true
  fi

  # Keep load and run in one pyOCD process. Re-opening CMSIS-DAP v1 immediately
  # after a load is unreliable on Parallels Ubuntu and can leave Flash written
  # but the CPU still in BootROM. Some pyOCD/DAPLink failures still return exit
  # code 0, so require the success text before accepting the download.
  set +e
  output=\$(printf 'halt\\nload %s\\nwreg sp %s\\nwreg pc %s\\ngo\\nexit\\n' '${HEX}' "\${initial_sp}" "\${reset_pc}" \\
    | '${PYOCD}' commander -t '${TARGET}' --frequency "\${frequency}" \${probe_arg} \\
        -O connect_mode="\${mode}" \\
        -O cmsis_dap.prefer_v1=true \\
        -O cmsis_dap.limit_packets=true \\
        -O cmsis_dap.deferred_transfers=false 2>&1)
  status=\$?
  set -e
  printf '%s\\n' "\${output}"
  if [ "\${status}" -ne 0 ]; then
    return "\${status}"
  fi
  printf '%s\\n' "\${output}" | grep -q 'Successfully resumed device'
}

if run_pyocd_session hidapiusb '${FREQUENCY}' under-reset; then
  exit 0
fi

echo "First pyOCD session failed; resetting DAPLink and retrying safer modes." >&2
if command -v usbreset >/dev/null 2>&1; then
  usbreset 0d28:0204 || true
  sleep 1
fi

run_pyocd_session "" 1000000 under-reset || \\
run_pyocd_session hidapiusb 1000000 under-reset || \\
run_pyocd_session "" 100000 attach || \\
run_pyocd_session hidapiusb 100000 attach
EOF
)

prlctl exec "${VM_NAME}" "${remote_cmd}"
