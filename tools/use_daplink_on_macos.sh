#!/usr/bin/env bash
set -euo pipefail

DAPLINK_ID="${DAPLINK_ID:-1130000|0d28|0204|full|--|070000011a5a5cc500000000000c00b6a5a5a5a597969908}"

if ! command -v prlsrvctl >/dev/null 2>&1; then
  echo "prlsrvctl not found; this script is only for Parallels hosts." >&2
  exit 2
fi

prlsrvctl usb set "${DAPLINK_ID}" --autoconnect host
echo
prlsrvctl usb list -a | sed -n '/DAPLink CMSIS-DAP/,+10p'
echo
echo "If Connected-To-Vm is still YES, unplug and replug DAPLink once."
echo "After replug, macOS pyOCD should see it and ./tools/flash_run_on_macos.sh can be used."
