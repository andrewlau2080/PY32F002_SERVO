#!/usr/bin/env python3
"""Keep the customer raw-channel vs six live VSG move comparison page current."""
from __future__ import annotations

import argparse
import csv
import html
import re
import statistics
import struct
import time
from collections import Counter
from pathlib import Path

from analyze_customer_vcd import parse_vcd

TICK_HZ = 250_000_000
CHANNEL_RE = re.compile(r"\b(?:Channel\s*|Ch\s*)([2367])\b", re.IGNORECASE)
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


def customer_vcd_tracks(path: Path):
    timescale, seconds_per_tick, signals, changes = parse_vcd(path)
    if seconds_per_tick is None:
        raise SystemExit(f"Unsupported or missing VCD timescale: {timescale}")
    selected = []
    for code, sig in signals.items():
        match = CHANNEL_RE.search(sig.name)
        if match and sig.width == 1 and changes.get(code):
            selected.append((int(match.group(1)), sig, changes[code]))
    selected.sort(key=lambda item: item[0])
    if not selected:
        raise SystemExit("VCD has no single-bit Channel 2/3/6/7 signals")
    missing = sorted({2, 3, 6, 7} - {channel for channel, _, _ in selected})
    if missing:
        raise SystemExit(f"VCD is missing digital channel(s): {', '.join(map(str, missing))}")

    activity = sorted({tick for _, _, seq in selected for tick, value in seq
                       if value[-1:].lower() in ("0", "1")})
    if not activity:
        raise SystemExit("VCD channels contain no known digital levels")
    max_gap = max(1, 0.015 / seconds_per_tick)
    clusters: list[list[int]] = []
    for tick in activity:
        if not clusters or tick - clusters[-1][-1] > max_gap:
            clusters.append([tick])
        else:
            clusters[-1].append(tick)
    start_tick, end_tick = clusters[-1][0], clusters[-1][-1]
    tracks = []
    for channel, sig, seq in selected:
        prior = [(t, v[-1:].lower()) for t, v in seq if t < start_tick and v[-1:].lower() in ("0", "1")]
        current = prior[-1][1] if prior else None
        points = [(start_tick, current)] if current is not None else []
        for tick, value in seq:
            value = value[-1:].lower()
            if tick < start_tick or tick > end_tick:
                continue
            if value in ("x", "z"):
                points.append((tick, "x"))
                current = None
            elif value in ("0", "1"):
                if (current != value) or (not points):
                    points.append((tick, value))
                current = value
        tracks.append((channel, sig.name, points))
    duration_ms = max(0.001, (end_tick - start_tick) * seconds_per_tick * 1000.0)
    return timescale, seconds_per_tick, start_tick, end_tick, duration_ms, tracks


def customer_csv_tracks(path: Path):
    with path.open(newline="", errors="replace") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise SystemExit("Customer CSV has no header")
        time_col = next((name for name in reader.fieldnames
                         if re.match(r"^\s*time(?:\s*\[([^]]+)\])?\s*$", name, re.IGNORECASE)), None)
        if not time_col:
            raise SystemExit("Customer CSV needs a Time column (optionally [s], [ms], [us], or [ns])")
        unit_match = re.search(r"\[([^]]+)\]", time_col)
        unit = unit_match.group(1).strip().lower() if unit_match else "s"
        scale = {"s": 1.0, "sec": 1.0, "ms": 1e-3,
                 "us": 1e-6, "µs": 1e-6, "ns": 1e-9}.get(unit)
        if scale is None:
            raise SystemExit(f"Unsupported CSV time unit: {unit}")
        columns = []
        for name in reader.fieldnames:
            match = CHANNEL_RE.search(name)
            if match:
                columns.append((int(match.group(1)), name))
        columns.sort()
        if not columns:
            raise SystemExit("Customer CSV has no Channel 2/3/6/7 columns")
        missing = sorted({2, 3, 6, 7} - {channel for channel, _ in columns})
        if missing:
            raise SystemExit(f"CSV is missing channel(s): {', '.join(map(str, missing))}")
        samples = []
        for row in reader:
            try:
                seconds = float(row[time_col]) * scale
            except (TypeError, ValueError):
                continue
            samples.append((seconds, row))
    if not samples:
        raise SystemExit("Customer CSV has no timestamped samples")
    times = [t for t, _ in samples]
    max_gap = 0.015
    clusters: list[list[float]] = []
    for sec in times:
        if not clusters or sec - clusters[-1][-1] > max_gap:
            clusters.append([sec])
        else:
            clusters[-1].append(sec)
    start_sec, end_sec = clusters[-1][0], clusters[-1][-1]
    tracks = []
    for channel, name in columns:
        points = []
        state = None
        for sec, row in samples:
            if sec < start_sec or sec > end_sec:
                continue
            value = (row.get(name) or "").strip().lower()
            if value not in ("0", "1", "x", "z"):
                continue
            if value != state:
                points.append((sec, value))
                state = value
        tracks.append((channel, name, points))
    duration_ms = max(0.001, (end_sec - start_sec) * 1000.0)
    return "CSV seconds", 1.0, start_sec, end_sec, duration_ms, tracks


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


def render_svg(customer: list[list[int]], rows: list[dict[str, str]], limit_ms: float,
               vcd_data=None) -> str:
    w, left, right = 1480, 190, 45
    plotw = w - left - right
    move_h = 112
    top_h = 220
    height = 105 + top_h + len(MOVES) * move_h + 40
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{height}" viewBox="0 0 {w} {height}">',
             '<rect width="100%" height="100%" fill="#fff"/>',
             '<text x="24" y="34" font-size="23" font-weight="700">CUSTOMER 原始波形 vs 我们的双向对称 PWM</text>',
             '<text x="24" y="58" font-size="13" fill="#444">客户采集与VSG曲线分别按各自动作起点归零；两组数据不是同一时钟的同步实测</text>']
    parts.append('<text x="24" y="88" font-size="16" font-weight="700">客户标准原始通道</text>')
    if vcd_data:
        timescale, seconds_per_tick, start_tick, end_tick, cust_ms, tracks = vcd_data
        parts.append(f'<text x="24" y="108" font-size="12" fill="#555">{html.escape(timescale)} Channel 2/3/6/7 数字电平 · 最后活动段起点为 t=0</text>')
    else:
        parts.append('<text x="24" y="111" font-size="12" fill="#555">Channel 2 · 250 MHz 时间戳 · Edge Finder标记；未解码电平极性/脉宽</text>')
        start_tick = customer[0][0] if customer else 0
        cust_ms = max(15.0, max(((b[-1] - start_tick) / TICK_HZ * 1000 for b in customer), default=15.0) + 0.5)
    def cx(ms: float) -> float: return left + max(0, min(cust_ms, ms)) / cust_ms * plotw
    base_y = 188
    colors = ["#187a3c", "#c43b33", "#df8a19", "#1f77b4"]
    if vcd_data:
        lane_y = [125, 147, 169, 191]
        for tick_ms in range(0, int(cust_ms) + 1, 1 if cust_ms < 22 else 2):
            xx = cx(tick_ms)
            parts.append(f'<line x1="{xx:.1f}" y1="116" x2="{xx:.1f}" y2="199" stroke="#eee"/>')
            parts.append(f'<text x="{xx-8:.1f}" y="214" font-size="10" fill="#666">{tick_ms}</text>')
        for i, (channel, name, points) in enumerate(tracks):
            y = lane_y[i]
            color = colors[i % len(colors)]
            parts.append(f'<text x="24" y="{y+4}" font-size="12" fill="{color}">Channel {channel}</text>')
            parts.append(f'<line x1="{left}" y1="{y}" x2="{left+plotw}" y2="{y}" stroke="#ddd"/>')
            d_parts = []
            state = None
            prev_x = left
            for tick, value in points:
                x = cx((tick - start_tick) * seconds_per_tick * 1000.0)
                if value == "x":
                    if state is not None:
                        old_y = y - 7 if state == "1" else y + 7
                        d_parts.append(f'M {prev_x:.1f} {old_y:.1f} H {x:.1f}')
                    state = None
                    continue
                level_y = y - 7 if value == "1" else y + 7
                if state is None:
                    d_parts.append(f'M {x:.1f} {level_y:.1f}')
                else:
                    old_y = y - 7 if state == "1" else y + 7
                    d_parts.append(f'M {prev_x:.1f} {old_y:.1f} H {x:.1f} V {level_y:.1f}')
                state = value
                prev_x = x
            if state is not None:
                final_y = y - 7 if state == "1" else y + 7
                d_parts.append(f'M {prev_x:.1f} {final_y:.1f} H {left+plotw:.1f}')
            if d_parts:
                parts.append(f'<path d="{" ".join(d_parts)}" fill="none" stroke="{color}" stroke-width="1.5"/>')
    else:
        parts.append(f'<line x1="{left}" y1="{base_y}" x2="{left+plotw}" y2="{base_y}" stroke="#777"/>')
        for tick in range(0, int(cust_ms) + 1, 1 if cust_ms < 22 else 2):
            xx = cx(tick)
            parts.append(f'<line x1="{xx:.1f}" y1="118" x2="{xx:.1f}" y2="{base_y+5}" stroke="#eee"/>')
            parts.append(f'<text x="{xx-8:.1f}" y="211" font-size="10" fill="#666">{tick}</text>')
        for bi, block in enumerate(customer):
            c = colors[bi % len(colors)]
            first, last = block[0], block[-1]
            s = (first - start_tick) / TICK_HZ * 1000
            e = (last - start_tick) / TICK_HZ * 1000
            parts.append(f'<rect x="{cx(s):.1f}" y="119" width="{max(1,cx(e)-cx(s)):.1f}" height="69" fill="{c}" opacity=".08"/>')
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
    digital_export = args.customer_vcd or args.customer_csv
    blocks=[] if digital_export else customer_blocks(args.customer)
    vcd_data = (customer_vcd_tracks(args.customer_vcd) if args.customer_vcd else
                customer_csv_tracks(args.customer_csv) if args.customer_csv else None)
    args.out_dir.mkdir(parents=True,exist_ok=True)
    svgname='customer_logic_compare.svg'
    (args.out_dir/svgname).write_text(render_svg(blocks,rows,args.window_ms,vcd_data),encoding='utf-8')
    source_label = ('VCD 导出的客户数字通道' if args.customer_vcd else
                    'CSV 导出的客户数字通道' if args.customer_csv else
                    'CUSTOMER.logicdata 客户边沿')
    html_text=f'''<!doctype html><html><head><meta charset="utf-8"><title>客户波形与双向 PWM 对照</title><style>body{{margin:0;background:#f5f5f5;font:14px Arial,"PingFang SC",sans-serif;color:#222}}header{{position:sticky;top:0;background:#fff;padding:10px 14px;border-bottom:1px solid #ddd;z-index:2}}main{{max-width:1480px;margin:auto}}img{{width:100%;height:auto;background:white;display:block}}</style></head><body><header>{source_label}在上｜VSG 正反向占空比包络在下（非同步实测）｜实时更新 <span id="stamp"></span></header><main><img id="cmp" src="{svgname}" alt="客户与双向 PWM 对照"></main><script>function refresh(){{let n=Date.now();document.getElementById('cmp').src='{svgname}?t='+n;document.getElementById('stamp').textContent=new Date(n).toLocaleTimeString()}}setInterval(refresh,1000);refresh();</script></body></html>'''
    (args.out_dir/'customer_logic_compare.html').write_text(html_text,encoding='utf-8')
    print(f"updated rows={len(rows)} customer_blocks={len(blocks)} page={args.out_dir/'customer_logic_compare.html'}",flush=True)


def main() -> None:
    p=argparse.ArgumentParser()
    p.add_argument('--customer',type=Path,default=Path('../PY32F002_SIGNAL_CAPTURE/CUSTOMER.logicdata'))
    p.add_argument('--customer-vcd',type=Path,help='optional Logic 1 VCD export; plots digital Channel 2/3/6/7 levels')
    p.add_argument('--customer-csv',type=Path,help='optional digital CSV with Time [s/ms/us/ns] and Channel 2/3/6/7 columns')
    p.add_argument('--ours',type=Path,default=Path('build/servo_curve_live/servo_curve_live.csv'))
    p.add_argument('--out-dir',type=Path,default=Path('build/customer_logic_compare'))
    p.add_argument('--window-ms',type=float,default=1200.0)
    p.add_argument('--watch',action='store_true',help='refresh output as the live CSV grows')
    p.add_argument('--refresh-sec',type=float,default=1.0)
    a=p.parse_args()
    if a.customer_vcd and a.customer_csv:
        p.error('choose only one of --customer-vcd or --customer-csv')
    if a.customer_vcd or a.customer_csv:
        a.customer = None
    while True:
        write_page(a)
        if not a.watch: break
        time.sleep(max(0.25,a.refresh_sec))


if __name__=='__main__': main()
