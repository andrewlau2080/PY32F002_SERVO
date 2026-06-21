#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_NAME="${VM_NAME:-Ubuntu Linux}"
SHARED_PROJECT_DIR="${SHARED_PROJECT_DIR:-/mnt/hgfs/qtprj/Steering Engine/MOTOR/PY32F002_SERVO}"
PYOCD_PYTHON="${PYOCD_PYTHON:-/opt/py32-venv/bin/python}"
INTERVAL_MS="${INTERVAL_MS:-5}"
FREQUENCY="${FREQUENCY:-1000000}"

mkdir -p "${PROJECT_DIR}/build/servo_curve_live"
cat > "${PROJECT_DIR}/build/servo_curve_live/servo_curve_live.html" <<HTML
<!doctype html>
<html>
<head><meta charset="utf-8"><meta http-equiv="refresh" content="1"><title>Servo Live Curve</title></head>
<body style="font-family:Arial,sans-serif;padding:24px">
  <h3>Servo live curve is starting...</h3>
  <p>If this page does not update, check the terminal running live_servo_curve_in_ubuntu.sh.</p>
</body>
</html>
HTML

if command -v open >/dev/null 2>&1; then
  open "${PROJECT_DIR}/build/servo_curve_live/servo_curve_live.html" || true
fi

echo "Live page:"
echo "  ${PROJECT_DIR}/build/servo_curve_live/servo_curve_live.html"
echo "Stop with Ctrl+C."

prlctl exec "${VM_NAME}" "cd '${SHARED_PROJECT_DIR}' && '${PYOCD_PYTHON}' tools/live_servo_curve_swd.py --interval-ms '${INTERVAL_MS}' --frequency '${FREQUENCY}'"
