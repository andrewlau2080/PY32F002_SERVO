#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LABEL="com.py32servo.curve.live"
PLIST="/tmp/${LABEL}.plist"
OUT_DIR="${PROJECT_DIR}/build/servo_curve_live"
UID_NUM="$(id -u)"

mkdir -p "${OUT_DIR}"

launchctl bootout "gui/${UID_NUM}" "${PLIST}" >/dev/null 2>&1 || true

cat > "${PLIST}" <<PLIST_XML
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>${LABEL}</string>
  <key>WorkingDirectory</key>
  <string>${PROJECT_DIR}</string>
  <key>ProgramArguments</key>
  <array>
    <string>/opt/homebrew/bin/python3</string>
    <string>-u</string>
    <string>${PROJECT_DIR}/tools/live_servo_curve_on_mac.py</string>
    <string>--interval-ms</string>
    <string>10</string>
    <string>--window</string>
    <string>600</string>
    <string>--output-dir</string>
    <string>${OUT_DIR}</string>
  </array>
  <key>KeepAlive</key>
  <true/>
  <key>RunAtLoad</key>
  <true/>
  <key>StandardOutPath</key>
  <string>${OUT_DIR}/live.log</string>
  <key>StandardErrorPath</key>
  <string>${OUT_DIR}/live.err.log</string>
</dict>
</plist>
PLIST_XML

launchctl bootstrap "gui/${UID_NUM}" "${PLIST}"
launchctl kickstart -k "gui/${UID_NUM}/${LABEL}" >/dev/null 2>&1 || true
if [[ "${SERVO_CURVE_OPEN_HTML:-0}" == "1" ]]; then
  open "${OUT_DIR}/servo_curve_live.html" >/dev/null 2>&1 || true
fi

echo "${LABEL}" > "${OUT_DIR}/live.label"
echo "${PLIST}" > "${OUT_DIR}/live.plist"
echo "Started ${LABEL}"
echo "HTML: ${OUT_DIR}/servo_curve_live.html"
echo "Log:  ${OUT_DIR}/live.log"
echo "Open manually, or run with SERVO_CURVE_OPEN_HTML=1 to open the HTML once."
