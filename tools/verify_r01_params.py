#!/usr/bin/env python3
"""Test R01 defaults and preservation of valid Flash calibration on the host.

Production C functions are extracted from the selected servo_params.c. A RAM
object stands in for Flash; the test does not erase or program a real device.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    pattern = r"^(?:static\s+)?[A-Za-z_][\w \t*]*\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{"
    match = re.search(pattern, source, re.MULTILINE | re.DOTALL)
    if not match:
        raise SystemExit("Missing production function: " + name)
    start = match.start()
    opening = source.find("{", start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise SystemExit("Unclosed production function: " + name)


HARNESS = r'''
static unsigned failures;
static void check(const char *name, int actual, int expected)
{
  printf("%s: %s (actual=%d expected=%d)\n",
         (actual == expected) ? "PASS" : "FAIL", name, actual, expected);
  if (actual != expected) { failures++; }
}
int main(void)
{
  ServoParams old;
  memset(&fake_flash, 0xff, sizeof(fake_flash));
  Servo_Params_Init();
  check("blank flash defaults lower endpoint to 500 us", Servo_Params_Get()->pulse_lower_us, 500);
  check("blank flash defaults center to 1500 us", Servo_Params_Get()->pulse_center_us, 1500);
  check("blank flash defaults upper endpoint to 2500 us", Servo_Params_Get()->pulse_high_us, 2500);
  check("new default block validates", Servo_Params_Validate(Servo_Params_Get()), 1);
  old = *Servo_Params_Get();
  old.pulse_lower_us = 900U;
  old.pulse_high_us = 2100U;
  old.crc32 = 0U;
  old.crc32 = Servo_Params_Crc32(&old);
  check("existing 900/2100 calibration remains valid", Servo_Params_Validate(&old), 1);
  fake_flash = old;
  Servo_Params_Init();
  check("existing lower calibration is retained", Servo_Params_Get()->pulse_lower_us, 900);
  check("existing upper calibration is retained", Servo_Params_Get()->pulse_high_us, 2100);
  check("existing calibration CRC is retained", Servo_Params_Validate(Servo_Params_Get()), 1);
  fake_flash.pulse_lower_us = 901U;
  check("modified calibration fails CRC", Servo_Params_Validate(&fake_flash), 0);
  printf("Flash is a simulated RAM object; no physical Flash write was performed.\n");
  return failures ? 1 : 0;
}
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path, default=ROOT / "src/servo_params.c")
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    source_bytes = args.source.read_bytes()
    source = source_bytes.decode()
    fragments = [
        '#include <stdint.h>', '#include <stdbool.h>', '#include <stdio.h>', '#include <string.h>',
        '#include "servo_params.h"', '#include "servo_config.h"',
        '#define FLASH_PAGE_SIZE 128U',
    ]
    for name in ("SERVO_PARAMS_MAGIC", "SERVO_PARAMS_VERSION"):
        match = re.search(r"^#define\s+" + name + r"\b[^\n]*", source, re.MULTILINE)
        if not match:
            raise SystemExit("Missing source macro: " + name)
        fragments.append(match.group(0))
    fragments += [
        'static ServoParams fake_flash;',
        '#define SERVO_PARAMS_FLASH_ADDR ((uintptr_t)&fake_flash)',
        'static ServoParams s_params;',
    ]
    for name in ("crc32_update", "Servo_Params_Crc32", "Servo_Params_GetDefaults",
                 "Servo_Params_Validate", "Servo_Params_Init", "Servo_Params_Get"):
        fragments.append(function(source, name))
    fragments.append(HARNESS)
    args.output.mkdir(parents=True, exist_ok=False)
    generated = args.output / "r01_params_harness.c"
    generated.write_text("\n\n".join(fragments) + "\n")
    binary = args.output / "r01_params_harness"
    build = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "inc"),
         str(generated), "-o", str(binary)], text=True, capture_output=True,
    )
    (args.output / "compile.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit("Host test did not compile; inspect " + str(args.output / "compile.log"))
    run = subprocess.run([str(binary.resolve())], text=True, capture_output=True)
    (args.output / "results.txt").write_text(run.stdout + run.stderr)
    report = {
        "production_source": str(args.source),
        "production_source_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "generated_c_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
        "pass_count": sum(line.startswith("PASS:") for line in run.stdout.splitlines()),
        "fail_count": sum(line.startswith("FAIL:") for line in run.stdout.splitlines()),
        "exit_code": run.returncode,
        "scope": "Production parameter defaults, validation and initialization with simulated Flash.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(run.stdout, end="")
    if run.returncode:
        raise SystemExit(run.returncode)


if __name__ == "__main__":
    main()
