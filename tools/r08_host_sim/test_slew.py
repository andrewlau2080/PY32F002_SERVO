#!/usr/bin/env python3
"""Exercise the production LCDM slew function with elapsed-time inputs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/app_entry.c").read_text()
start = source.index("static int16_t lcdm_servo_apply_slew(")
end = source.index("\nstatic void lcdm_servo_enter_hold", start)
prefix = """#include <stdint.h>
#include <assert.h>
static int16_t s_lcdm_servo_current_duty;
static int s_lcdm_reg_params;
#define LCDM_REG_PARAM_SLEW 4
#define LCDM_SERVO_OUT_SLEW 80
static unsigned lcdm_param_u16(int p,int k,int d) {(void)p;(void)k;return d;}
"""
checks = """
int main(void) {
  s_lcdm_servo_current_duty=0;
  assert(lcdm_servo_apply_slew(500,0)==0);
  assert(lcdm_servo_apply_slew(500,1)==80);
  assert(lcdm_servo_apply_slew(500,4)==320);
  s_lcdm_servo_current_duty=200;
  assert(lcdm_servo_apply_slew(50,0)==50);
  assert(lcdm_servo_apply_slew(-200,1)==120);
  assert(lcdm_servo_apply_slew(-200,4)==0);
  s_lcdm_servo_current_duty=-200;
  assert(lcdm_servo_apply_slew(200,1)==-120);
  return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="servo_slew_") as directory:
    test = Path(directory) / "slew.c"
    binary = Path(directory) / "slew"
    test.write_text(prefix + source[start:end] + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("LCDM slew: 7 checks passed")
