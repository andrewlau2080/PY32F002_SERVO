#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_NAME="${VM_NAME:-Ubuntu Linux}"
REMOTE_DIR="${REMOTE_DIR:-/tmp/py32f002_servo}"
MAKE_ARGS="$*"

tar -C "${PROJECT_DIR}/.." --exclude 'PY32F002_SERVO/build' -cf - PY32F002_SERVO \
  | prlctl exec "${VM_NAME}" "rm -rf '${REMOTE_DIR}' && mkdir -p '${REMOTE_DIR}' && tar -C '${REMOTE_DIR}' -xf -"

prlctl exec "${VM_NAME}" "cd '${REMOTE_DIR}/PY32F002_SERVO' && make clean && make -j\$(nproc) ${MAKE_ARGS}"

mkdir -p "${PROJECT_DIR}/build"
for artifact in py32f002_servo.elf py32f002_servo.hex py32f002_servo.bin py32f002_servo.map; do
  prlctl exec "${VM_NAME}" "base64 -w 0 '${REMOTE_DIR}/PY32F002_SERVO/build/${artifact}'" \
    | base64 -D > "${PROJECT_DIR}/build/${artifact}"
done
