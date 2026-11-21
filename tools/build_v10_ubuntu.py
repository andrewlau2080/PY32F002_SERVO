#!/usr/bin/env python3
"""Build a V10.x image with the pinned Ubuntu toolchain and selected profile.

This command builds only. Flashing remains a separate, explicitly selected ELF
operation through download_py32_ubuntu.sh.
"""
import argparse
import atexit
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
PRODUCTION_LCDM_FLAGS = [
    'LTO=1', 'BOARD_TSSOP20_DEBUG_PINS=1', 'SERVO_ENABLE_TJC_LCDM=1', 'SERVO_PRODUCTION_LCDM=1',
    'SERVO_ENABLE_PA0_POT_POWER=1', 'SERVO_TJC_UART_ALT_PINS=1',
    'TJC_LCDM_BAUD_OVERRIDE=230400', 'SERVO_TJC_RX_ENABLE=1',
    'SERVO_ENABLE_COMM=1',
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
    parser.add_argument('--production-lcdm', action='store_true',
                        help='build the TJC LCDM application path without the runtime-UI test harness')
    parser.add_argument('--production-loop-r01-test', action='store_true',
                        help='build production TJC LCDM and Servo_Control with an explicitly armed internal 1500/1510 us source')
    parser.add_argument('--r01-center-return-test', action='store_true',
                        help='single armed 1500 us return from a measured low-end position')
    parser.add_argument('--internal-step-test', action='store_true',
                        help='build the guarded center-only LCDM diagnostic sequence')
    parser.add_argument('--r01-test-max-duty', type=int, default=40,
                        help='maximum absolute R01 diagnostic duty in permille (1..350 step, 1..500 return)')
    parser.add_argument('--r01-position-limit-adc', type=int, default=8,
                        help='latched coast limit from the measured R01 start position (1..32 ADC counts)')
    parser.add_argument('--r01-anchor-pulse-us', type=int, choices=[1500, 1550], default=1500,
                        help='pulse mapped to the live ADC anchor; use 1550 to test the 1550-to-1500 direction')
    parser.add_argument('--r01-explicit-arm', action='store_true',
                        help='disable automatic R01 motion and wait for a live SWD arm request; capped at 150 permille')
    parser.add_argument('--r01-test-step-us', type=int, default=None,
                        help='internal R01 center-step size in microseconds (explicit-arm requires 10..50)')
    parser.add_argument('--r01-test-dwell-ms', type=int, default=1000,
                        help='time to hold each internal R01 pulse width before switching')
    parser.add_argument('--r01-test-staircase', action='store_true',
                        help='armed 1500..1550 us visible staircase; return to 1500 after feedback confirmation, otherwise coast')
    parser.add_argument('--r01-lcd-only-test', action='store_true',
                        help='show 1500/1550 us on the V9.x P50 LCD layout with the H-bridge held coast')
    parser.add_argument('--r04-ui-redraw-stress', action='store_true',
                        help='repaint the LCDM diagnostic page every 250 ms for R04 load testing')
    parser.add_argument('--force-coast', action='store_true',
                        help='force H-bridge outputs to coast for non-moving diagnostics')
    parser.add_argument('--disable-tjc-rx', action='store_true',
                        help='omit LCDM touch UART RX to reduce RAM/Flash for read-only diagnostics')
    parser.add_argument('--tjc-soft-tx', action='store_true',
                        help='drive LCDM TX on PA7 by GPIO bit-banging, matching the V9.x path')
    parser.add_argument('--direction-probe', action='store_true',
                        help='build explicitly armed hardware single-pulse diagnostic')
    parser.add_argument('--pa6-single-pulse', action='store_true',
                        help='build a one-shot direct PA6 GPIO pulse diagnostic (no servo/H-bridge logic)')
    parser.add_argument('--probe-pulse-us', type=int, choices=[80, 120, 160], default=80,
                        help='one timer pulse width for direction diagnostics')
    parser.add_argument('--vrefint-settle-fix', action='store_true',
                        help='wait for the PY32F002A VREFINT startup time before VDD sampling')
    parser.add_argument('--adc-pair-time-tag', action='store_true',
                        help='tag the paired ADC timestamp diagnostic image as P160H')
    parser.add_argument('--adc-subsample-diagnostic', action='store_true',
                        help='record the four position ADC conversions and Q4 timestamps')
    parser.add_argument('--adc-vdd-each-batch', action='store_true',
                        help='refresh VREFINT before every four-sample position ADC batch')
    parser.add_argument('--adc-long-sample-diagnostic', action='store_true',
                        help='use maximum ADC acquisition time for force-coast diagnostics')
    args = parser.parse_args()
    if args.r01_test_step_us is None:
        args.r01_test_step_us = 10 if (args.production_loop_r01_test or args.r01_explicit_arm) else 1
    if not 1 <= args.r01_test_max_duty <= (500 if args.r01_center_return_test else 350):
        parser.error('--r01-test-max-duty exceeds the selected R01 diagnostic range')
    if not 250 <= args.r01_test_dwell_ms <= 10000:
        parser.error('--r01-test-dwell-ms must be in 250..10000')
    if args.r01_center_return_test and not args.production_loop_r01_test:
        parser.error('--r01-center-return-test requires --production-loop-r01-test')
    if args.r01_test_staircase and (not args.production_loop_r01_test or args.r01_center_return_test):
        parser.error('--r01-test-staircase requires --production-loop-r01-test and excludes --r01-center-return-test')
    if args.r01_lcd_only_test and (not args.production_loop_r01_test or not args.tjc_soft_tx):
        parser.error('--r01-lcd-only-test requires --production-loop-r01-test --tjc-soft-tx')
    if args.internal_step_test and args.profile != 'lcdm':
        parser.error('--internal-step-test requires --profile lcdm')
    if args.pa6_single_pulse and (args.direction_probe or args.internal_step_test or args.force_coast or args.r04_ui_redraw_stress):
        parser.error('--pa6-single-pulse is an isolated GPIO test; do not combine with other diagnostics')
    if not 1 <= args.r01_test_step_us <= 50:
        parser.error('--r01-test-step-us must be in 1..50 us')
    if not 1 <= args.r01_position_limit_adc <= 32:
        parser.error('--r01-position-limit-adc must be in 1..32 ADC counts')
    if args.r01_explicit_arm and (not args.internal_step_test or args.force_coast or
                                  args.direction_probe or args.r04_ui_redraw_stress or
                                  args.adc_subsample_diagnostic or args.r01_test_max_duty > 150 or
                                  not 10 <= args.r01_test_step_us <= 50):
        parser.error('--r01-explicit-arm requires a 10 us LCDM internal step, no force-coast/direction/timing/subsample mode, and duty <= 150 permille')
    if args.production_lcdm and args.profile != 'lcdm':
        parser.error('--production-lcdm requires --profile lcdm')
    if args.production_lcdm and (args.internal_step_test or args.r04_ui_redraw_stress):
        parser.error('--production-lcdm cannot be combined with diagnostic test modes')
    if args.production_loop_r01_test and (args.profile != 'lcdm' or args.internal_step_test or args.production_lcdm or args.r04_ui_redraw_stress):
        parser.error('--production-loop-r01-test selects its own production LCDM and internal step flags')
    if args.adc_subsample_diagnostic and not (args.internal_step_test or args.production_loop_r01_test):
        parser.error('--adc-subsample-diagnostic requires --internal-step-test or a guarded R01 production-loop test')
    if args.adc_vdd_each_batch and not args.adc_subsample_diagnostic:
        parser.error('--adc-vdd-each-batch requires --adc-subsample-diagnostic')
    if args.adc_long_sample_diagnostic and not (
            args.adc_subsample_diagnostic and
            (args.force_coast or args.production_loop_r01_test)):
        parser.error('--adc-long-sample-diagnostic requires paired ADC diagnostics and either force-coast or guarded production-loop R01 test')
    if args.r04_ui_redraw_stress and not (args.internal_step_test and args.profile == 'lcdm'):
        parser.error('--r04-ui-redraw-stress requires --profile lcdm --internal-step-test')
    source = (ROOT / 'src/app_entry.c').read_text()
    match = re.search(r'#define LCDM_SERVO_FW_TAG " (V10\.[0-9]+)[A-Za-z0-9]*"', source)
    if not match:
        parser.error('Expected the current V10.x version in src/app_entry.c')
    version = match.group(1)
    flags = (PRODUCTION_LCDM_FLAGS if (args.production_lcdm or args.production_loop_r01_test) else
             (FLAGS if args.profile == 'lcdm' else GENERIC_FLAGS))
    if args.tjc_soft_tx:
        if args.profile != 'lcdm':
            parser.error('--tjc-soft-tx requires the TSSOP20 LCDM profile')
        flags = flags + ['SERVO_TJC_SOFT_TX=1']
    if args.production_loop_r01_test:
        flags = flags + ['SERVO_ALLOW_INTERNAL_STEP_TEST=1', 'SERVO_ENABLE_INTERNAL_STEP_TEST=1',
                         'SERVO_R01_EXPLICIT_ARM=1', 'SERVO_R01_PRODUCTION_LOOP_TEST=1',
                         f'SERVO_R01_TEST_POSITION_LIMIT_ADC={args.r01_position_limit_adc}',
                         f'SERVO_R01_TEST_ANCHOR_PULSE_US={args.r01_anchor_pulse_us}',
                         f'SERVO_R01_TEST_MAX_DUTY={args.r01_test_max_duty}',
                         f'SERVO_R01_TEST_STEP_US={args.r01_test_step_us}',
                         f'SERVO_R01_TEST_DWELL_MS={args.r01_test_dwell_ms}U']
        if args.r01_test_staircase:
            flags.append('SERVO_R01_TEST_STAIRCASE=1')
        if args.r01_lcd_only_test:
            flags.append('SERVO_R01_LCD_ONLY_TEST=1')
        if args.r01_center_return_test:
            flags.append('SERVO_R01_CENTER_RETURN_TEST=1')
    if args.pa6_single_pulse:
        flags = ['LTO=1', 'BOARD_TSSOP20_DEBUG_PINS=1', 'SERVO_ENABLE_LCDM_RUNTIME_UI_TEST=0',
                 'SERVO_ENABLE_TJC_LCDM=0', 'SERVO_ENABLE_COMM=0',
                 'SERVO_PA6_SINGLE_PULSE_TEST=1', 'SERVO_PA6_SINGLE_PULSE_WIDTH_US=100']
    if args.internal_step_test:
        # The diagnostic is armed and observed over SWD; touch RX is unused and
        # reserving it costs scarce RAM needed by the telemetry/stack budget.
        flags = [flag for flag in flags if flag != 'SERVO_TJC_RX_ENABLE=1']
        flags = flags + ['SERVO_TJC_RX_ENABLE=0']
        flags = flags + ['SERVO_ALLOW_INTERNAL_STEP_TEST=1', 'SERVO_ENABLE_INTERNAL_STEP_TEST=1',
                         'SERVO_R01_TEST_DUTY_INVERT=1',
                         f'SERVO_R01_TEST_MAX_DUTY={args.r01_test_max_duty}',
                         f'SERVO_R01_TEST_STEP_US={args.r01_test_step_us}']
        if args.r01_explicit_arm:
            flags = flags + ['SERVO_R01_EXPLICIT_ARM=1']
    if args.disable_tjc_rx:
        flags = [flag for flag in flags if not flag.startswith('SERVO_TJC_RX_ENABLE=')]
        flags = flags + ['SERVO_TJC_RX_ENABLE=0']
    if args.r04_ui_redraw_stress:
        flags = flags + ['SERVO_R04_UI_REDRAW_STRESS=1', 'SERVO_R04_TIMING_TRACE=1']
    if args.adc_subsample_diagnostic:
        flags = flags + ['SERVO_ADC_SUBSAMPLE_DIAGNOSTIC=1']
    if args.adc_vdd_each_batch:
        flags = flags + ['SERVO_ADC_VDD_EACH_DIAG_BATCH=1']
    if args.adc_long_sample_diagnostic:
        flags = flags + ['SERVO_ADC_LONG_SAMPLE_DIAGNOSTIC=1']
    if args.force_coast:
        flags = flags + ['SERVO_HBRIDGE_FORCE_COAST=1']
    if args.direction_probe:
        if not args.force_coast or not args.internal_step_test:
            parser.error('--direction-probe requires --internal-step-test --force-coast')
        flags = flags + ['SERVO_DIRECTION_PROBE=1', f'SERVO_DIRECTION_PULSE_US={args.probe_pulse_us}']
    if args.vrefint_settle_fix:
        if not args.direction_probe or args.probe_pulse_us != 160:
            parser.error('--vrefint-settle-fix currently requires the 160 us coast-only diagnostic')
        flags = flags + ['SERVO_VREFINT_SETTLE_FIX=1']
    if args.adc_pair_time_tag:
        if not args.vrefint_settle_fix or not args.direction_probe:
            parser.error('--adc-pair-time-tag requires --vrefint-settle-fix and --direction-probe')
        flags = flags + ['SERVO_ADC_PAIR_TIME_TAG=1']
    default_name = ('V10.6_TSSOP20_PA6_SINGLE_PULSE_100US' if args.pa6_single_pulse else
                    (version + '_internal_step' if args.internal_step_test else
                     (version if args.profile == 'lcdm' else version + '_generic')))
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

    def cleanup_remote_build():
        cleanup = subprocess.run(ssh + ['rm -rf -- ' + shlex.quote(remote)],
                                 capture_output=True, text=True)
        if cleanup.returncode:
            print('WARNING: Ubuntu temporary build directory cleanup failed:', remote)
            if cleanup.stderr:
                print(cleanup.stderr.strip())
        return cleanup.returncode == 0

    atexit.register(cleanup_remote_build)
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
    build_log = (out / 'build.log').read_text(errors='replace')
    if args.r01_lcd_only_test and '-DSERVO_R01_LCD_ONLY_TEST=1' not in build_log:
        raise SystemExit('Requested LCD-only test macro is absent from the compiler command; candidate rejected')
    if args.production_loop_r01_test and f'-DSERVO_R01_TEST_POSITION_LIMIT_ADC={args.r01_position_limit_adc}' not in build_log:
        raise SystemExit('Requested R01 ADC displacement guard is absent from the compiler command; candidate rejected')
    if args.production_loop_r01_test and f'-DSERVO_R01_TEST_ANCHOR_PULSE_US={args.r01_anchor_pulse_us}U' not in build_log:
        raise SystemExit('Requested R01 anchor pulse is absent from the compiler command; candidate rejected')
    if args.tjc_soft_tx and '-DSERVO_TJC_SOFT_TX=1' not in build_log:
        raise SystemExit('Requested PA7 software-TX macro is absent from the compiler command; candidate rejected')
    if args.adc_vdd_each_batch:
        if '-DSERVO_ADC_VDD_EACH_DIAG_BATCH=1' not in build_log:
            raise SystemExit('Requested VDD-per-batch macro is absent from the compiler command; candidate rejected')
    if args.adc_subsample_diagnostic and '-DSERVO_ADC_SUBSAMPLE_DIAGNOSTIC=1' not in build_log:
        raise SystemExit('Requested ADC sub-sample diagnostic macro is absent from the compiler command; candidate rejected')
    if args.adc_long_sample_diagnostic and '-DSERVO_ADC_LONG_SAMPLE_DIAGNOSTIC=1' not in build_log:
        raise SystemExit('Requested long ADC sample-time diagnostic macro is absent from the compiler command; candidate rejected')
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
    direction_suffix = ('SIMB2P' + str(args.probe_pulse_us) +
                        ('H' if args.adc_pair_time_tag else ('G' if args.vrefint_settle_fix else 'F')))
    expected_suffix = ('SIMB2PA6P' if args.pa6_single_pulse else
                       ('SIMB2R30D' if args.r01_explicit_arm else
                       (direction_suffix if args.direction_probe else
                       ('SIMB2C' if args.force_coast else ('SIMB2' if args.internal_step_test else '')))
                       ))
    expected_version_text = ('P50 V10.6LCDP50' if args.r01_lcd_only_test else
                             ('P50 V10.6R13LCD' if args.production_loop_r01_test else
                              'P50 ' + version + ('' if args.production_lcdm else expected_suffix)))
    expected_version = expected_version_text.encode()
    if (args.profile == 'lcdm' and not args.pa6_single_pulse and versions != [expected_version]) or (
            args.pa6_single_pulse and versions not in ([], [expected_version])) or (
            args.profile == 'generic' and versions not in ([], [expected_version])):
        raise SystemExit('Unexpected firmware version strings: ' + repr(versions))
    remote_build_cleaned = cleanup_remote_build()
    atexit.unregister(cleanup_remote_build)
    report = dict(version=args.pa6_single_pulse and 'V10.6_SIMB2PA6P' or version, profile=args.profile, pa6_single_pulse=args.pa6_single_pulse, pa6_pulse_width_us=100 if args.pa6_single_pulse else None, production_lcdm=(args.production_lcdm or args.production_loop_r01_test), production_loop_r01_test=args.production_loop_r01_test, r01_lcd_only_test=args.r01_lcd_only_test, tjc_soft_tx=args.tjc_soft_tx, r01_center_return_test=args.r01_center_return_test, r01_test_staircase=args.r01_test_staircase, internal_step_test=(args.internal_step_test or args.production_loop_r01_test), r01_explicit_arm=(args.r01_explicit_arm or args.production_loop_r01_test), r01_test_step_us=args.r01_test_step_us, r01_test_max_duty=args.r01_test_max_duty, r01_test_dwell_ms=args.r01_test_dwell_ms, r01_position_limit_adc=args.r01_position_limit_adc, r01_anchor_pulse_us=args.r01_anchor_pulse_us, r04_ui_redraw_stress=args.r04_ui_redraw_stress, adc_subsample_diagnostic=args.adc_subsample_diagnostic, adc_vdd_each_batch=args.adc_vdd_each_batch, adc_long_sample_diagnostic=args.adc_long_sample_diagnostic, force_coast=args.force_coast, direction_probe=args.direction_probe, probe_pulse_us=args.probe_pulse_us if args.direction_probe else None,
                  date=datetime.datetime.now().isoformat(), target='py32f002ax5',
                  flags=flags, sdk=sdk_meta, source_files=hashes, artifacts=artifacts,
                  lcdm_version_string_present=bool(versions),
                  initial_sp=hex(sp), reset_pc=hex(pc), remote_build_dir=remote,
                  remote_build_cleaned=remote_build_cleaned,
                  built=True, flashed=False, hardware_stability_verified=False,
                  provenance='Reconstructed V9.6 behavior; not a byte-exact recovery of the original C source')
    (out / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    print((out / 'build.log').read_text()[-650:])
    print('Release saved:', out)
    print('Build passed; not flashed.')


if __name__ == '__main__':
    main()
