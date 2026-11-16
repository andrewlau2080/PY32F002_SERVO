#!/usr/bin/env python3
"""Build a V10.x image with the pinned Ubuntu toolchain and selected profile.

This command builds only. Flashing remains a separate, explicitly selected ELF
operation through download_py32_ubuntu.sh.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
FLAGS = [
    'LTO=1', 'BOARD_TSSOP20_DEBUG_PINS=1',
    'SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1', 'SERVO_ENABLE_PA0_POT_POWER=1',
    'SERVO_TJC_UART_ALT_PINS=1', 'TJC_LCDM_BAUD_OVERRIDE=230400',
    'SERVO_TJC_RX_ENABLE=1', 'SERVO_ENABLE_COMM=0',
]
GENERIC_FLAGS = [
    'LTO=0', 'BOARD_TSSOP20_DEBUG_PINS=0', 'BOARD_SOP8_PINOUT=1',
    'SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=0', 'SERVO_ENABLE_TJC_LCDM=0',
    'SERVO_ENABLE_PA0_POT_POWER=0', 'SERVO_ENABLE_COMM=1',
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='andrew@10.211.55.4')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--profile', choices=['lcdm', 'generic'], default='lcdm',
                        help='lcdm: TSSOP20 field build; generic: SOP8 pinout build')
    args = parser.parse_args()
    source = (ROOT / 'src/app_entry.c').read_text()
    match = re.search(r'#define LCDM_SERVO_FW_TAG " (V10\.[0-9]+)"', source)
    if not match:
        parser.error('Expected the current V10.x version in src/app_entry.c')
    version = match.group(1)
    flags = FLAGS if args.profile == 'lcdm' else GENERIC_FLAGS
    default_name = version if args.profile == 'lcdm' else version + '_generic'
    out = args.output or ROOT / 'build/releases' / default_name
    out.mkdir(parents=True, exist_ok=False)
    sdk_dir = ROOT / 'firmware/toolchains'
    sdk_meta = json.loads((sdk_dir / 'manifest.json').read_text())
    sdk = sdk_dir / sdk_meta['archive']
    if hashlib.sha256(sdk.read_bytes()).hexdigest() != sdk_meta['sha256']:
        raise SystemExit('Pinned SDK checksum mismatch')
    files = [ROOT / 'Makefile']
    for folder in ['src', 'inc', 'ld']:
        files += sorted(p for p in (ROOT / folder).rglob('*') if p.is_file())
    hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    archive = out / 'source.tar.gz'
    with tarfile.open(archive, 'w:gz') as tar:
        for path in files:
            tar.add(path, arcname=str(path.relative_to(ROOT)))
    ssh = ['ssh', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=8', args.host]
    remote = subprocess.check_output(ssh + ['mktemp -d /tmp/servo_v10_build.XXXXXX'], text=True).strip()
    if not re.fullmatch(r'/tmp/servo_v10_build\.[A-Za-z0-9]+', remote):
        raise SystemExit('Unexpected Ubuntu build directory')
    for path, name in [(archive, 'source.tar.gz'), (sdk, 'sdk.tar.gz')]:
        subprocess.run(['scp', str(path), args.host + ':' + remote + '/' + name], check=True)
    command = (
        f'cd {shlex.quote(remote)} && mkdir project sdk && '
        'tar -xzf source.tar.gz -C project && tar -xzf sdk.tar.gz -C sdk && '
        'arm-none-eabi-gcc --version && cd project && '
        f'make -j4 PY32_SDK={shlex.quote(remote + "/sdk")} ' + ' '.join(flags)
    )
    print(f'Building {version} ({args.profile}) in Ubuntu: {remote}', flush=True)
    with (out / 'build.log').open('w') as log:
        result = subprocess.run(ssh + [command], stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((out / 'build.log').read_text()[-4000:])
        raise SystemExit(result.returncode)
    artifacts = {}
    for ext in ['elf', 'bin', 'hex', 'map']:
        name = 'py32f002_servo.' + ext
        subprocess.run(['scp', args.host + ':' + remote + '/project/build/' + name, str(out / name)], check=True)
        data = (out / name).read_bytes()
        artifacts[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    image = (out / 'py32f002_servo.bin').read_bytes()
    sp, pc = struct.unpack_from('<II', image)
    if not (8 <= len(image) <= 20096 and sp == 0x20000c00 and pc & 1
            and 0x08000000 <= (pc & ~1) < 0x08000000 + len(image)):
        raise SystemExit('Invalid PY32F002A application size/vector')
    versions = re.findall(rb'P50 V[0-9A-Za-z.]+', image)
    expected_version = ('P50 ' + version).encode()
    if (args.profile == 'lcdm' and versions != [expected_version]) or (
            args.profile == 'generic' and versions not in ([], [expected_version])):
        raise SystemExit('Unexpected firmware version strings: ' + repr(versions))
    report = dict(version=version, date=datetime.datetime.now().isoformat(), target='py32f002ax5',
                  profile=args.profile, flags=flags, sdk=sdk_meta, source_files=hashes, artifacts=artifacts,
                  lcdm_version_string_present=bool(versions),
                  initial_sp=hex(sp), reset_pc=hex(pc), remote_build_dir=remote,
                  built=True, flashed=False, hardware_stability_verified=False,
                  provenance='Reconstructed V9.6 behavior; not a byte-exact recovery of the original C source')
    (out / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    print((out / 'build.log').read_text()[-650:])
    print('Release saved:', out)
    print('Build passed; not flashed.')


if __name__ == '__main__':
    main()
