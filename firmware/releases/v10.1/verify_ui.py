import angr, logging, json, time
from pathlib import Path
from elftools.elf.elffile import ELFFile
logging.getLogger('angr').setLevel(logging.CRITICAL)
p=Path('PY32F002_SERVO/build/v10_recovery')
class Send(angr.SimProcedure):
 def run(self,ptr):
  self.state.globals['commands']=self.state.globals['commands']+(self.state.mem[ptr].string.concrete.decode('ascii',errors='replace'),)
  return 0
class Delay(angr.SimProcedure):
 def run(self,ms):return 0
with (Path('PY32F002_SERVO/build/releases/V10.1_qualified/py32f002_servo.elf')).open('rb') as f:
 e=ELFFile(f);data=e.get_section_by_name('.data').data();syms={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols() if s['st_size']}
original=Path('PY32F002_SERVO/firmware/baselines/v9.6_original')
projects=[angr.Project(str(x),auto_load_libs=False,arch='ARMCortexM') for x in [original/'original_v96_app.elf',Path('PY32F002_SERVO/build/releases/V10.1_qualified/py32f002_servo.elf')]]
for j,pr in enumerate(projects):
 if j==0:pr.hook(0x8003a1d,angr.SIM_PROCEDURES['libc']['snprintf']())
 pr.hook(0x80021d5 if j==0 else syms['pa7_tjc_write_cmd'],Send())
 pr.hook(0x8001e49 if j==0 else syms['pa7_tjc_delay_ms_with_led'],Delay())
 pr.hook(0x8001e1d if j==0 else syms['lcdm_runtime_wait_ms_with_control'],Delay())
results=[];start=time.time()
for page in [50,13,30,40,70,1]:
 outputs=[]
 for j,pr in enumerate(projects):
  s=pr.factory.call_state(0x80024b1 if j==0 else syms['lcdm_runtime_draw_page'],0x20000a00,ret_addr=0xbadf00d,add_options={angr.options.ZERO_FILL_UNCONSTRAINED_MEMORY,angr.options.ZERO_FILL_UNCONSTRAINED_REGISTERS})
  s.regs.sp=0x20000bf0;s.memory.store(0x20000000,bytes(3072));s.memory.store(0x20000000,(original/'original_v96_app.bin').read_bytes()[0x4cc0:0x4f60] if j==0 else data)
  s.memory.store(0x20000a00,bytes([page])+bytes(63));s.globals['commands']=()
  m=pr.factory.simgr(s);m.explore(find=0xbadf00d,n=50000,num_find=1)
  outputs.append(list(m.found[0].globals['commands']) if m.found else {'error':str(m.errored[:1]),'active':[hex(x.addr) for x in m.active]})
 outputs=[[v.replace('P50 V10.1','P50 V9.6') for v in seq] if isinstance(seq,list) else seq for seq in outputs]
 item=dict(page=page,equal=not isinstance(outputs[0],dict) and outputs[0]==outputs[1],original=outputs[0],candidate=outputs[1]);results.append(item);print('Page',page,'equal',item['equal'],'elapsed',round(time.time()-start,1),flush=True)
 Path('PY32F002_SERVO/build/releases/V10.1_qualified/ui_real_formatter_comparison.json').write_text(json.dumps(results,indent=2)+'\n')
 if isinstance(outputs[0],dict) or isinstance(outputs[1],dict):print(item,flush=True);break
