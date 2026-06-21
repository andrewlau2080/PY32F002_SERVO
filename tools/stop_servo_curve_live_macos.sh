#!/usr/bin/env bash
set -euo pipefail

LABEL="com.py32servo.curve.live"
PLIST="/tmp/${LABEL}.plist"
UID_NUM="$(id -u)"

launchctl bootout "gui/${UID_NUM}" "${PLIST}" >/dev/null 2>&1 || \
launchctl bootout "gui/${UID_NUM}/${LABEL}" >/dev/null 2>&1 || true

echo "Stopped ${LABEL}"
