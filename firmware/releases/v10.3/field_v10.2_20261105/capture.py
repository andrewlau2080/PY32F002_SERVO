import csv, json, time, zlib
from unittest.mock import patch

with patch('runpy.run_module'):
    import runpy
    runpy.run_path('/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py')
from pyocd.core.helpers import ConnectHelper

OUT = '/tmp/servo_v102_field.csv'
PARAM = 0x20000548
FRAME = 0x20000348
SYMS = {
    'uwTick': (0x200002dc, 4), 'duty_current': (0x200002f4, 2),
    'state_current': (0x200002f8, 1), 'edge_count': (0x20000320, 4),
    'pulse_count': (0x20000334, 4),
}
opts = {'frequency': 1000000, 'connect_mode': 'attach',
        'pack': '/home/andrew/ARTERY/DQ110_FW/1.2.8.pack',
        'resume_on_disconnect': False, 'cache.enable_memory': False,
        'cache.enable_register': False, 'cmsis_dap.prefer_v1': True,
        'cmsis_dap.limit_packets': True, 'cmsis_dap.deferred_transfers': False}
session = ConnectHelper.session_with_chosen_probe(target_override='py32f002ax5',
    blocking=False, auto_open=False, options=opts)
if session is None:
    raise SystemExit('No SWD probe found')
session.open()
target = session.target
fields = ['sample', 'wall_s', 'core', 'tick', 'time_ms', 'pwm_us', 'pwm_raw_us',
          'raw_adc', 'target_adc', 'feedback_adc', 'error_adc', 'duty',
          'velocity_q4', 'ref_velocity', 'traj_adc', 'traj_vel_q4',
          'overshoot_adc', 'state', 'event', 'seq_head', 'seq_tail',
          'params_crc_ok', 'input_timeout_ms', 'edge_count', 'pulse_count',
          'uwTick', 'duty_current', 'state_current']
try:
    target.resume()
    with open(OUT, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        start = time.monotonic()
        i = 0
        while time.monotonic() - start < 100:
            wall = time.time()
            b = bytes(target.read_memory_block8(FRAME, 44))
            u16 = lambda o: int.from_bytes(b[o:o+2], 'little')
            i16 = lambda o: int.from_bytes(b[o:o+2], 'little', signed=True)
            u32 = lambda o: int.from_bytes(b[o:o+4], 'little')
            row = {'sample': i, 'wall_s': wall, 'core': str(target.get_state()),
                'time_ms': u32(4), 'pwm_us': u16(8), 'pwm_raw_us': u16(10),
                'raw_adc': u16(14), 'target_adc': u16(16), 'feedback_adc': u16(18),
                'error_adc': i16(20), 'duty': i16(22), 'velocity_q4': i16(24),
                'ref_velocity': i16(26), 'traj_adc': i16(28), 'traj_vel_q4': i16(30),
                'overshoot_adc': u16(32), 'state': b[36], 'event': b[37],
                'seq_head': u32(0), 'seq_tail': u32(40)}
            for name, (addr, size) in SYMS.items():
                raw = bytes(target.read_memory_block8(addr, size))
                row[name] = int.from_bytes(raw, 'little')
            if i % 100 == 0:
                p = bytes(target.read_memory_block8(PARAM, 128))
                row['params_crc_ok'] = zlib.crc32(p[:124] + bytes(4)) == int.from_bytes(p[124:128], 'little')
                row['input_timeout_ms'] = int.from_bytes(p[54:56], 'little')
            else:
                row['params_crc_ok'] = ''
                row['input_timeout_ms'] = ''
            w.writerow(row)
            if i % 25 == 0:
                f.flush()
                print(json.dumps({'sample': i, 'time_ms': row['time_ms'],
                    'pwm': row['pwm_us'], 'target': row['target_adc'],
                    'feedback': row['feedback_adc'], 'error': row['error_adc'],
                    'duty': row['duty'], 'state': row['state'], 'event': row['event']},
                    separators=(',', ':')), flush=True)
            i += 1
            time.sleep(0.01)
    print(json.dumps({'complete': True, 'samples': i, 'csv': OUT}), flush=True)
finally:
    session.close()
