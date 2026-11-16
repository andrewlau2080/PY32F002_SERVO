#!/usr/bin/env python3
"""Check that arm-none-eabi-gcc has its target C headers and runtime libraries."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    return result.returncode, result.stdout.strip(), result.stderr.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="arm-none-eabi-gcc")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    compiler = shutil.which(args.compiler)
    report = {"compiler_requested": args.compiler, "compiler_path": compiler}
    if compiler is None:
        report.update(ok=False, reason="compiler not found")
    else:
        _, version, version_error = run([compiler, "--version"])
        _, sysroot, _ = run([compiler, "-print-sysroot"])
        _, libc_result, _ = run([compiler, "-print-file-name=libc.a"])
        libc_path = Path(libc_result)
        report.update(version=version.splitlines()[0] if version else version_error,
                      sysroot=sysroot,
                      libc_path=libc_result,
                      libc_exists=libc_path.is_absolute() and libc_path.is_file())

        with tempfile.TemporaryDirectory(prefix="arm_none_eabi_check_") as temp_dir:
            source = Path(temp_dir) / "check_stdint.c"
            object_file = Path(temp_dir) / "check_stdint.o"
            source.write_text("#include <stdint.h>\nuint32_t toolchain_header_check;\n")
            code, stdout, stderr = run([compiler, "-std=c11", "-Werror", "-c",
                                        str(source), "-o", str(object_file)])
            report.update(stdint_compile_ok=(code == 0),
                          stdint_compile_stdout=stdout,
                          stdint_compile_stderr=stderr)
        report["ok"] = report["stdint_compile_ok"] and report["libc_exists"]
        if not report["ok"]:
            report["reason"] = "target C library headers and/or libc.a are missing"

    rendered = json.dumps(report, indent=2) + "\n"
    print(rendered, end="")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    raise SystemExit(0 if report["ok"] else 1)


if __name__ == "__main__":
    main()
