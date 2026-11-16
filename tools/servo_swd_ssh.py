#!/usr/bin/env python3
"""PY32F002A/B read/flash through the verified DQ110 Ubuntu HID SWD path."""
import argparse
import base64
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import sys

REMOTE = r'''
import base64, json, os, pathlib, re, shutil, subprocess, sys, tempfile
cfg = json.loads(sys.argv[1])
env = dict(os.environ, PYOCD_USB_BACKEND="hidapiusb")
for name in (cfg["python"], cfg["pack"]):
    if not pathlib.Path(name).is_file():
        raise SystemExit("Missing Ubuntu dependency: " + name)
compat = pathlib.Path(cfg["compat"])
stable_compat = pathlib.Path.home() / ".local/share/servo_swd/dq110_pyocd_hid_compat.py"
if compat.is_file():
    if str(compat).startswith("/tmp/"):
        stable_compat.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(compat, stable_compat)
        compat = stable_compat
elif stable_compat.is_file():
    compat = stable_compat
else:
    raise SystemExit("Ubuntu HID compatibility script is missing: " + str(compat)
                     + "; restore the verified DQ110 script before continuing")
base = [cfg["python"], str(compat)]
listing = subprocess.run(base + ["list"], env=env, text=True, capture_output=True, timeout=30)
print(listing.stdout + listing.stderr, file=sys.stderr)
uids = re.findall(r"^\s*\d+\s+.+?\s+([A-Za-z0-9_-]{20,})\s+", listing.stdout, re.M)
if cfg.get("uid"):
    if cfg["uid"] not in uids:
        raise SystemExit("Requested probe is not present: " + cfg["uid"])
    uids = [cfg["uid"]]
if listing.returncode or len(set(uids)) != 1:
    raise SystemExit("Expected exactly one DAPLink probe; found " + str(len(set(uids))))
if cfg["action"] == "check":
    print(json.dumps({"uid": uids[0], "ok": True, "log": listing.stdout + listing.stderr,
                      "compat": str(compat), "python": cfg["python"], "pack": cfg["pack"]}))
    sys.exit(0)
with tempfile.TemporaryDirectory(prefix="servo_swd_") as tmp:
    root = pathlib.Path(tmp)
    before = root / "before.bin"
    after = root / "after.bin"
    # Flash can be read with the running core attached; do not reset for backup.
    args = base + ["commander", "--no-wait", "-u", uids[0], "-t", cfg["target"],
                   "--pack", cfg["pack"], "-f", cfg["frequency"], "-M", "attach",
                   "-O", "cmsis_dap.prefer_v1=true", "-O", "cmsis_dap.limit_packets=true",
                   "-O", "cmsis_dap.deferred_transfers=false"]
    def run(commands):
        call_args = list(args)
        for command in commands:
            call_args += ["-c", command]
        result = subprocess.run(call_args, env=env, text=True, capture_output=True, timeout=180)
        log = result.stdout + result.stderr
        bad = result.returncode != 0 or bool(re.search(
            r"Error:|read thread exited unexpectedly|not halted as expected|No connected debug probe", log, re.I))
        return log, bad
    log, bad = run(["savemem 0x08000000 " + str(cfg["flash_size"]) + " " + str(before), "status"])
    bad = bad or not before.exists() or before.stat().st_size != cfg["flash_size"]
    if cfg["action"] == "flash" and not bad:
        # Keep a durable Ubuntu copy before any programming, including if SSH drops.
        import datetime
        archive = pathlib.Path.home() / "servo_swd_backups"
        archive.mkdir(exist_ok=True)
        backup = archive / (datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".bin")
        backup.write_bytes(before.read_bytes())
        image = root / "firmware.bin"
        image.write_bytes(base64.b64decode(cfg["image"]))
        flash_log, bad = run(["halt", "load " + str(image) + " 0x08000000",
                              "savemem 0x08000000 " + str(cfg["flash_size"]) + " " + str(after),
                              "reset halt", "g", "status"])
        log += "\nUbuntu backup: " + str(backup) + "\n" + flash_log
    payload = {"uid": uids[0], "log": log, "ok": not bad}
    for key, path in (("before", before), ("after", after)):
        if path.exists():
            payload[key] = base64.b64encode(path.read_bytes()).decode()
    print(json.dumps(payload))
'''


def describe(data):
    if len(data) < 8:
        return {"bytes": len(data), "error": "incomplete readback"}
    sp, pc = struct.unpack_from("<II", data)
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "sp": f"0x{sp:08x}", "pc": f"0x{pc:08x}",
            "versions": [{"address": f"0x{0x08000000 + m.start():08x}",
                          "text": m.group().decode("ascii")}
                         for m in re.finditer(rb"P50 V[0-9A-Za-z.]+", data)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["check", "read", "flash"])
    parser.add_argument("image", nargs="?", type=Path, help="flash: explicitly selected BIN")
    parser.add_argument("--host", default=os.getenv("SERVO_REMOTE_HOST", "andrew@10.211.55.4"))
    parser.add_argument("--frequency", default="1M")
    common_entry = Path(__file__).name == "py32_swd.py"
    parser.add_argument("--target", choices=["py32f002ax5", "py32f002bx5"],
                        default=None if common_entry else "py32f002ax5", required=common_entry)
    parser.add_argument("--reserve-tail", type=int, default=None,
                        help="protected final Flash bytes (default: A=384, B=0)")
    parser.add_argument("--local", action="store_true", help="run directly inside Ubuntu; no SSH")
    parser.add_argument("--python", dest="python_path", default=os.getenv(
        "SERVO_REMOTE_PYTHON", "/home/andrew/ARTERY/.tools/pyocd-venv/bin/python"))
    parser.add_argument("--compat", default=os.getenv("SERVO_REMOTE_COMPAT", "/tmp/dq110_pyocd_hid_compat.py"))
    parser.add_argument("--pack", default=os.getenv("SERVO_REMOTE_PACK", "/home/andrew/ARTERY/DQ110_FW/1.2.8.pack"))
    parser.add_argument("--uid", help="select a specific probe when several are connected")
    parser.add_argument("--output", type=Path, help="backup/log directory (default: project build/board_readback/time)")
    args = parser.parse_args()
    cfg = {"action": args.action, "frequency": args.frequency,
           "python": args.python_path, "compat": args.compat, "pack": args.pack, "uid": args.uid}
    if args.local and not sys.platform.startswith("linux"):
        parser.error("--local is for Ubuntu; execute this command inside the Ubuntu VM")
    cfg["target"] = args.target
    cfg["flash_size"] = {"py32f002ax5": 20480, "py32f002bx5": 24576}[args.target]
    reserved = args.reserve_tail if args.reserve_tail is not None else (
        384 if not common_entry and args.target == "py32f002ax5" else 0)
    if reserved < 0 or reserved >= cfg["flash_size"] or reserved % 128:
        parser.error("--reserve-tail must be a multiple of 128 smaller than Flash")
    image_limit = cfg["flash_size"] - reserved
    image = None
    if args.action == "flash":
        if args.image is None:
            parser.error("flash requires an explicitly selected .bin file")
        image = args.image.read_bytes()
        if args.image.suffix.lower() != ".bin" or not 8 <= len(image) <= image_limit:
            parser.error(f"expected a BIN of 8..{image_limit} bytes for {args.target}; reserved tail={reserved}")
        sp, pc = struct.unpack_from("<II", image)
        if not (0x20000000 < sp <= 0x20000c00 and sp % 4 == 0 and pc & 1
                and 0x08000000 <= (pc & ~1) < 0x08000000 + len(image)):
            parser.error("invalid PY32F002A application vector")
        cfg["image"] = base64.b64encode(image).decode()
    elif args.image is not None:
        parser.error("check/read take no image argument")
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    out = args.output or Path(__file__).resolve().parents[1] / "build" / "board_readback" / stamp
    out.mkdir(parents=True, exist_ok=False)
    if args.local:
        argv = [sys.executable, "-", json.dumps(cfg)]
    else:
        command = "python3 - " + shlex.quote(json.dumps(cfg))
        argv = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", args.host, command]
    result = subprocess.run(argv, input=REMOTE, text=True, capture_output=True)
    transport_log = out / ("ubuntu.log" if args.local else "ssh.log")
    transport_log.write_text(result.stderr)
    (out / "transport_stdout.log").write_text(result.stdout)
    if result.returncode:
        print(result.stderr + result.stdout, file=sys.stderr)
        raise SystemExit(f"Ubuntu SWD access failed; log: {transport_log}")
    payload = json.loads(result.stdout)
    (out / "pyocd.log").write_text(payload["log"])
    report = {"host": "Ubuntu-local" if args.local else args.host, "target": args.target,
              "uid": payload["uid"], "action": args.action}
    if args.action == "check":
        report.update({k: payload[k] for k in ("compat", "python", "pack")})
        report["probe_visible"] = payload["ok"]
        report["hardware_read_verified"] = False
        (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
        print(f"Probe visible; run read to verify target access. Logs: {out}")
        return
    data = {}
    for key in ("before", "after"):
        if key in payload:
            data[key] = base64.b64decode(payload[key], validate=True)
            (out / (key + ".bin")).write_bytes(data[key])
            report[key] = describe(data[key])
    ok = payload["ok"] and len(data.get("before", b"")) == cfg["flash_size"]
    if image is not None:
        ok = (ok and len(data.get("after", b"")) == cfg["flash_size"]
              and data["after"][:len(image)] == image
              and data["after"][image_limit:] == data["before"][image_limit:]
              and "Successfully resumed device" in payload["log"])
    report["verified"] = bool(ok)
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    print(f"Backup and logs: {out}")
    if not ok:
        raise SystemExit("SWD operation did not pass verification; inspect pyocd.log")


if __name__ == "__main__":
    main()
