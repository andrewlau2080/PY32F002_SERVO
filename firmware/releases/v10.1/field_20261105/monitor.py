import runpy,json,time,zlib,struct
from unittest.mock import patch
with patch('runpy.run_module'):runpy.run_path('/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py')
from pyocd.core.helpers import ConnectHelper
symbols={'s_params': (536872264, 128), 'uwTick': (536871644, 4), 's_lcdm_servo_current_duty': (536871668, 2), 's_lcdm_servo_current_state': (536871672, 1), 's_edge_count': (536871712, 4), 's_last_edge_ms': (536871728, 4), 's_pulse_count': (536871732, 4), 'g_servo_curve_frame': (536871752, 44)}
s=ConnectHelper.session_with_chosen_probe(target_override='py32f002ax5',blocking=False,auto_open=False,options={'frequency':1000000,'connect_mode':'attach','pack':'/home/andrew/ARTERY/DQ110_FW/1.2.8.pack','resume_on_disconnect':False,'cache.enable_memory':False,'cache.enable_register':False,'cmsis_dap.prefer_v1':True,'cmsis_dap.limit_packets':True,'cmsis_dap.deferred_transfers':False})
s.open();t=s.target
try:
 for i in range(60):
  data={k:bytes(t.read_memory_block8(a,n)) for k,(a,n) in symbols.items()}
  params=data['s_params'];frame=data['g_servo_curve_frame']
  out={'sample':i,'wall':time.time(),'core':str(t.get_state()),'params_crc_ok':zlib.crc32(params[:124]+bytes(4))==int.from_bytes(params[124:128],'little'),'input_timeout_ms':int.from_bytes(params[54:56],'little'),'params_hex':params.hex(),'time_ms':int.from_bytes(frame[4:8],'little'),'pwm_us':int.from_bytes(frame[8:10],'little'),'target_adc':int.from_bytes(frame[16:18],'little'),'feedback_adc':int.from_bytes(frame[18:20],'little'),'error_adc':int.from_bytes(frame[20:22],'little',signed=True),'duty':int.from_bytes(frame[22:24],'little',signed=True),'event':frame[37]}
  for k,b in data.items():
   if len(b)<=4:out[k]=int.from_bytes(b,'little',signed=k.endswith('duty'))
  print(json.dumps(out),flush=True)
  time.sleep(1)
finally:s.close()
