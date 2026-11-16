#!/usr/bin/env python3
"""Offline V9.6 Thumb disassembly, pseudocode and function comparison.

Run with the isolated Python environment recorded in firmware/toolchains/.
This never connects to the SWD probe. CFG/function matching is heuristic;
pseudocode is evidence for reconstruction, not recovered original C text.
"""
import argparse
import hashlib
import json
import logging
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--decompile', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/v10_reverse')
    args = parser.parse_args()
    import angr

    logging.getLogger('angr').setLevel(logging.CRITICAL)
    original = ROOT / 'firmware/baselines/v9.6_original'
    data = (original / 'original_v96_full_flash.bin').read_bytes()
    expected = '303995fd4a331f0911b1705b45a1e42abf56d72b5231d239126f97d7505de9ce'
    if hashlib.sha256(data).hexdigest() != expected:
        raise SystemExit('Original V9.6 baseline checksum mismatch')
    args.output.mkdir(parents=True, exist_ok=True)
    project = angr.Project(str(original / 'original_v96_app.elf'),
                           auto_load_libs=False, arch='ARMCortexM')
    cfg = project.analyses.CFGFast(normalize=True, resolve_indirect_jumps=False,
                                  data_references=True)
    functions = []
    destination = args.output / 'decompiled'
    destination.mkdir(exist_ok=True)
    for address, function in cfg.kb.functions.items():
        if not 0x08000000 <= address < 0x08004f80 or function.is_alignment:
            continue
        item = dict(address=hex(address), size=function.size, name=function.name,
                    blocks=len(list(function.blocks)))
        functions.append(item)
        assembly = []
        for block in sorted(function.blocks, key=lambda b: b.addr):
            assembly += [f'{i.address:08x} {i.mnemonic:8s} {i.op_str}' for i in block.capstone.insns]
        (destination / f'{address:08x}.asm').write_text('\n'.join(assembly) + '\n')
        if args.decompile:
            try:
                result = project.analyses.Decompiler(function, cfg=cfg.model)
                if result.codegen:
                    (destination / f'{address:08x}.c').write_text(result.codegen.text)
                else:
                    item['decompile_error'] = 'No pseudocode generated'
            except Exception as error:
                item['decompile_error'] = str(error)
    (args.output / 'functions.json').write_text(json.dumps(functions, indent=2) + '\n')
    if args.candidate:
        candidate = angr.Project(str(args.candidate), auto_load_libs=False, arch='ARMCortexM')
        other_cfg = candidate.analyses.CFGFast(normalize=True, resolve_indirect_jumps=False,
                                               data_references=True)
        diff = project.analyses.BinDiff(candidate, cfg_a=cfg.model, cfg_b=other_cfg.model)
        matches = []
        for a, b in sorted(diff.function_matches):
            fa, fb = cfg.kb.functions.get(a), other_cfg.kb.functions.get(b)
            if fa and fb:
                matches.append(dict(original=hex(a), candidate=hex(b), name=fb.name,
                                    original_size=fa.size, candidate_size=fb.size,
                                    probably_identical=diff.functions_probably_identical(a, b)))
        (args.output / 'function_matches.json').write_text(json.dumps(matches, indent=2) + '\n')
        print('Heuristic function matches:', len(matches), '; probably identical:',
              sum(x['probably_identical'] for x in matches))
    print('Offline analysis saved:', args.output)


if __name__ == '__main__':
    main()
