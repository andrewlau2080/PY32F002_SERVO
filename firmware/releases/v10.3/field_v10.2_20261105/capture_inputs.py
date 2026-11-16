import csv, time
from unittest.mock import patch
with patch('runpy.run_module'):
    import runpy
    runpy.run_path('/home/andrew/.local/share/servo_swd/dq110_pyocd_hid_compat.py')
from pyocd.core.helpers import ConnectHelper
o={'frequency':1000000,'connect_mode':'attach','pack':'/home/andrew/ARTERY/DQ110_FW/1.2.8.pack',
   'resume_on_disconnect':False,'cache.enable_memory':False,'cache.enable_register':False,
   'cmsis_dap.prefer_v1':True,'cmsis_dap.limit_packets':True,'cmsis_dap.deferred_transfers':False}
s=ConnectHelper.session_with_chosen_probe(target_override='py32f002ax5',blocking=False,auto_open=False,options=o)
s.open();t=s.target
syms={'tick':(0x200002dc,4),'state':(0x200002f8,1),'duty':(0x200002f4,2),
      'edge_count':(0x20000320,4),'valid':(0x2000032c,1),'candidate_count':(0x2000032d,1),
      'last_edge_ms':(0x20000330,4),'pulse_count':(0x20000334,4),
      'raw_pulse':(0x2000002c,2),'candidate_pulse':(0x20000030,2),'period_us':(0x20000328,4),
      'frame':(0x20000348,44)}
try:
 t.resume(); start=time.monotonic(); n=0
 with open('/tmp/servo_v102_inputs.csv','w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=['sample','wall_s']+list(syms)+['frame_time_ms','pwm_us','target_adc','feedback_adc','error_adc','frame_duty','frame_event','frame_seq']); w.writeheader()
  while time.monotonic()-start<35:
   row={'sample':n,'wall_s':time.time()}
   for k,(a,z) in syms.items():
    b=bytes(t.read_memory_block8(a,z))
    if k=='frame':
     row['frame_time_ms']=int.from_bytes(b[4:8],'little'); row['pwm_us']=int.from_bytes(b[8:10],'little')
     row['target_adc']=int.from_bytes(b[16:18],'little'); row['feedback_adc']=int.from_bytes(b[18:20],'little')
     row['error_adc']=int.from_bytes(b[20:22],'little',signed=True); row['frame_duty']=int.from_bytes(b[22:24],'little',signed=True)
     row['frame_event']=b[37]; row['frame_seq']=int.from_bytes(b[40:44],'little')
    else: row[k]=int.from_bytes(b,'little',signed=(k=='duty'))
   w.writerow(row); f.flush(); n+=1; time.sleep(.05)
 print('complete',n,flush=True)
finally:s.close()
