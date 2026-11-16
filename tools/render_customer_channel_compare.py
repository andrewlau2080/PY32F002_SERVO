#!/usr/bin/env python3
"""Keep the customer raw-channel vs six live VSG move comparison page current."""
from __future__ import annotations

import argparse
import csv
import html
import statistics
import struct
import time
from collections import Counter
from pathlib import Path

TICK_HZ = 250_000_000
MOVES = [("1000 → 1500", "low", "mid"), ("1500 → 2000", "mid", "high"),
         ("2000 → 1500", "high", "mid"), ("1500 → 1000", "mid", "low"),
         ("1000 → 2000", "low", "high"), ("2000 → 1000", "high", "low")]


def read_vsg(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def customer_blocks(path: Path) -> list[list[int]]:
    data = path.read_bytes()
    records: list[tuple[int, int]] = []
    for off in range(len(data) - 32):
        if (data[off + 4:off + 8] == b"\0" * 4 and
            data[off + 12:off + 16] == b"\0" * 4 and
            data[off + 20:off + 24] == b"\0" * 4 and
            data[off + 24:off + 28] == b"\x02\0\0\0" and
            data[off + 28:off + 32] == b"\0" * 4):
            records.append((off, struct.unpack_from("<I", data, off)[0]))
    blocks: list[list[int]] = []
    i = 0
    while i < len(records):
        j = i
        while j + 1 < len(records) and records[j + 1][0] == records[j][0] + 32:
            j += 1
        if j - i + 1 >= 10:
            blocks.append([x[1] for x in records[i:j + 1]])
        i = j + 1
    blocks.sort(key=lambda block: block[0])
    clusters: list[list[list[int]]] = []
    for b in blocks:
        if not clusters or (b[0] - clusters[-1][-1][-1]) / TICK_HZ * 1000 > 15:
            clusters.append([b])
        else:
            clusters[-1].append(b)
    # The final captured customer action is the clearest 7.195/2.006/3.166ms group.
    return clusters[-1] if clusters else []


def level(pwm: int) -> str:
    if pwm < 1250:
        return "low"
    if pwm < 1750:
        return "mid"
    return "high"


def locate_moves(rows: list[dict[str, str]]) -> dict[tuple[str, str], tuple[int, int] | None]:
    found: dict[tuple[str, str], tuple[int, int] | None] = {(a, b): None for _, a, b in MOVES}
    transitions: list[tuple[int, str, str]] = []
    prev = None
    for i, row in enumerate(rows):
        try:
            cur = level(int(float(row["pwm_us"])))
        except (KeyError, ValueError):
            continue
        if prev is not None and cur != prev:
            transitions.append((i, prev, cur))
        prev = cur
    wanted = [(a, b) for _, a, b in MOVES]
    complete: list[list[tuple[int, str, str]]] = []
    for start, event in enumerate(transitions):
        if (event[1], event[2]) != wanted[0]:
            continue
        sequence = [event]
        cursor = start + 1
        for pair in wanted[1:]:
            while cursor < len(transitions) and (transitions[cursor][1], transitions[cursor][2]) != pair:
                cursor += 1
            if cursor == len(transitions):
                break
            sequence.append(transitions[cursor])
            cursor += 1
        if len(sequence) == len(wanted):
            complete.append(sequence)
    if complete:
        for event, pair in zip(complete[-1], wanted):
            found[pair] = (event[0], int(float(rows[event[0]]["time_ms"])))
        return found
    for i, a, b in transitions:
        if (a, b) in found:
            found[(a, b)] = (i, int(float(rows[i]["time_ms"])))
    return found


def render_svg(customer: list[list[int]], rows: list[dict[str, str]], limit_ms: float) -> str:
    w, left, right = 1480, 190, 45
    plotw = w - left - right
    move_h = 112
    top_h = 220
    height = 105 + top_h + len(MOVES) * move_h + 40
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{height}" viewBox="0 0 {w} {height}">',
             '<rect width="100%" height="100%" fill="#fff"/>',
             '<text x="24" y="34" font-size="23" font-weight="700">CUSTOMER 原始波形 vs 我们的双向对称 PWM</text>',
             '<text x="24" y="58" font-size="13" fill="#444">只使用 CUSTOMER.logicdata；客户边沿在上，我们的正向/反向两路逻辑输出在下，按动作起点 t=0 对齐</text>']
    # Original customer edge train. Draw each recorded edge marker without inventing polarity or pulse width.
    y0 = 95
    parts.append('<text x="24" y="88" font-size="16" font-weight="700">客户标准原始通道</text>')
    parts.append('<text x="24" y="111" font-size="12" fill="#555">Channel 2 · 250 MHz 时间戳 · 边沿标记；当前提取未解码电平极性/脉宽</text>')
    start_tick = customer[0][0] if customer else 0
    cust_ms = max(15.0, max(((b[-1] - start_tick) / TICK_HZ * 1000 for b in customer), default=15.0) + 0.5)
    def cx(ms: float) -> float: return left + max(0, min(cust_ms, ms)) / cust_ms * plotw
    base_y = 188
    parts.append(f'<line x1="{left}" y1="{base_y}" x2="{left+plotw}" y2="{base_y}" stroke="#777"/>')
    colors = ["#187a3c", "#c43b33", "#df8a19", "#1f77b4"]
    for tick in range(0, int(cust_ms) + 1, 1 if cust_ms < 22 else 2):
        xx = cx(tick)
        parts.append(f'<line x1="{xx:.1f}" y1="118" x2="{xx:.1f}" y2="{base_y+5}" stroke="#eee"/>')
        parts.append(f'<text x="{xx-8:.1f}" y="211" font-size="10" fill="#666">{tick}</text>')
    for bi, block in enumerate(customer):
        c = colors[bi % len(colors)]
        first = block[0]
        last = block[-1]
        s = (first - start_tick) / TICK_HZ * 1000
        e = (last - start_tick) / TICK_HZ * 1000
        parts.append(f'<rect x="{cx(s):.1f}" y="119" width="{max(1,cx(e)-cx(s)):.1f}" height="69" fill="{c}" opacity=".08"/>')
        # Accurate edge-time raster from the native file; marks are not synthetic PWM high/low levels.
        marks = " ".join(f'{cx((t-start_tick)/TICK_HZ*1000):.1f},{base_y-34} {cx((t-start_tick)/TICK_HZ*1000):.1f},{base_y}' for t in block)
        parts.append(f'<polyline points="{marks}" fill="none" stroke="{c}" stroke-width="1.15"/>')
        dur = (last-first)/TICK_HZ*1000
        parts.append(f'<text x="{cx((s+e)/2):.1f}" y="132" text-anchor="middle" font-size="11" fill="{c}">{dur:.3f} ms / {len(block)} edges</text>')
    # Six user-selected command transitions below, all from the current live VSG file.
    parts.append('<text x="24" y="248" font-size="16" font-weight="700">我们的双向对称 PWM 输出逻辑（六个动作）</text>')
    parts.append(f'<text x="24" y="268" font-size="12" fill="#555">正 duty 映射正向通道，负 duty 映射反向通道；两路对称且互斥。曲线是 VSG duty 占空比包络，SWD 采样无法解析约 50 kHz 载波</text>')
    found = locate_moves(rows)
    ytop = 280
    xspan = max(1.0, limit_ms)
    def vx(ms: float) -> float: return left + max(0.0, min(xspan, ms)) / xspan * plotw
    maxrows = 0
    for n, (label, a, b) in enumerate(MOVES):
        yy = ytop + n * move_h
        panel_top = yy + 19
        plot_h = 62
        lane1 = panel_top + 19
        lane2 = panel_top + 49
        parts.append(f'<rect x="{left}" y="{panel_top}" width="{plotw}" height="{plot_h}" fill="#fbfbfb" stroke="#ddd"/>')
        parts.append(f'<text x="24" y="{yy+34}" font-size="14" font-weight="700">{label}</text>')
        hit = found[(a,b)]
        if hit is None:
            parts.append(f'<text x="{left+12}" y="{panel_top+35}" font-size="14" fill="#999">等待本方向实测数据</text>')
            continue
        idx, t0 = hit
        selected=[]
        for offset, row in enumerate(rows[idx:]):
            dt=float(row['time_ms'])-t0
            if dt > limit_ms: break
            if offset and level(int(float(row['pwm_us']))) != b: break
            if dt >= 0: selected.append((dt,row))
        maxrows=max(maxrows,len(selected))
        if not selected:
            parts.append(f'<text x="{left+12}" y="{panel_top+35}" font-size="13" fill="#999">采样窗口尚未形成</text>')
            continue
        p0=int(float(rows[idx-1]['pwm_us'])) if idx else int(float(rows[idx]['pwm_us']))
        p1=int(float(rows[idx]['pwm_us']))
        parts.append(f'<text x="24" y="{yy+53}" font-size="11" fill="#555">{p0}→{p1} µs · {len(selected)} 个 VSG 点 · duty 对称标尺 ±100%</text>')
        parts.append(f'<text x="24" y="{lane1+4}" font-size="10" fill="#2878b5">正向</text><text x="24" y="{lane2+4}" font-size="10" fill="#cf5638">反向</text>')
        for y in (lane1, lane2): parts.append(f'<line x1="{left}" y1="{y}" x2="{left+plotw}" y2="{y}" stroke="#ddd"/>')
        fwd=[]; rev=[]
        for dt,r in selected:
            try: d=max(-1000,min(1000,int(float(r['duty']))))
            except (KeyError,ValueError): continue
            x=vx(dt)
            fwd.append(f'{x:.1f},{lane1-(max(0,d)/1000*25):.1f}')
            rev.append(f'{x:.1f},{lane2-(max(0,-d)/1000*25):.1f}')
        if fwd: parts.append(f'<polyline points="{" ".join(fwd)}" fill="none" stroke="#2878b5" stroke-width="2"/>')
        if rev: parts.append(f'<polyline points="{" ".join(rev)}" fill="none" stroke="#cf5638" stroke-width="2"/>')
        # Event count is useful context, but the 10ms SWD poll can miss 7/2/3ms edges.
        events=Counter(int(float(r['event'])) for _,r in selected)
        parts.append(f'<text x="{left+8}" y="{yy+108}" font-size="10" fill="#555">events 12/14/17/18: {events[12]}/{events[14]}/{events[17]}/{events[18]} · t=0</text>')
    for tick in range(0, int(xspan)+1, 100):
        xx=vx(tick)
        for n in range(len(MOVES)):
            y=ytop+n*move_h+19
            parts.append(f'<line x1="{xx:.1f}" y1="{y}" x2="{xx:.1f}" y2="{y+62}" stroke="#ececec"/>')
        parts.append(f'<text x="{xx-10:.1f}" y="{ytop+len(MOVES)*move_h+4}" font-size="10" fill="#555">{tick}</text>')
    parts.append(f'<text x="{left+plotw/2:.1f}" y="{ytop+len(MOVES)*move_h+22}" text-anchor="middle" font-size="12" fill="#555">各行相对动作起点时间（ms）</text>')
    parts.append(f'<text x="{left+8}" y="{ytop+len(MOVES)*move_h+40}" font-size="11" fill="#777">VSG 帧数：{len(rows)} · 正负 duty 分别对应两路方向输出；图中为占空比包络，不是物理引脚载波采样</text>')
    parts.append('</svg>')
    return '\n'.join(parts)


def write_page(args: argparse.Namespace) -> None:
    rows=read_vsg(args.ours)
    blocks=customer_blocks(args.customer)
    args.out_dir.mkdir(parents=True,exist_ok=True)
    svgname='customer_logic_compare.svg'
    (args.out_dir/svgname).write_text(render_svg(blocks,rows,args.window_ms),encoding='utf-8')
    html_text=f'''<!doctype html><html><head><meta charset="utf-8"><title>CUSTOMER 与双向 PWM 对照</title><style>body{{margin:0;background:#f5f5f5;font:14px Arial,"PingFang SC",sans-serif;color:#222}}header{{position:sticky;top:0;background:#fff;padding:10px 14px;border-bottom:1px solid #ddd;z-index:2}}main{{max-width:1480px;margin:auto}}img{{width:100%;height:auto;background:white;display:block}}</style></head><body><header>CUSTOMER.logicdata 原始波形在上｜我们的正反向对称 PWM 逻辑在下｜实时更新 <span id="stamp"></span></header><main><img id="cmp" src="{svgname}" alt="客户与双向 PWM 对照"></main><script>function refresh(){{let n=Date.now();document.getElementById('cmp').src='{svgname}?t='+n;document.getElementById('stamp').textContent=new Date(n).toLocaleTimeString()}}setInterval(refresh,1000);refresh();</script></body></html>'''
    (args.out_dir/'customer_logic_compare.html').write_text(html_text,encoding='utf-8')
    print(f"updated rows={len(rows)} customer_blocks={len(blocks)} page={args.out_dir/'customer_logic_compare.html'}",flush=True)


def main() -> None:
    p=argparse.ArgumentParser()
    p.add_argument('--customer',type=Path,default=Path('../PY32F002_SIGNAL_CAPTURE/CUSTOMER.logicdata'))
    p.add_argument('--ours',type=Path,default=Path('build/servo_curve_live/servo_curve_live.csv'))
    p.add_argument('--out-dir',type=Path,default=Path('build/customer_logic_compare'))
    p.add_argument('--window-ms',type=float,default=1200.0)
    p.add_argument('--watch',action='store_true',help='refresh output as the live CSV grows')
    p.add_argument('--refresh-sec',type=float,default=1.0)
    a=p.parse_args()
    while True:
        write_page(a)
        if not a.watch: break
        time.sleep(max(0.25,a.refresh_sec))


if __name__=='__main__': main()
