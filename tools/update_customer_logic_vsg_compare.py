#!/usr/bin/env python3
"""Generate the single customer LOGICDATA vs VSG comparison page."""

from __future__ import annotations

import argparse
import csv
import html
import time
from dataclasses import dataclass
from pathlib import Path


EVENT_NAMES = {
    2: "HOLD",
    3: "BRAKE",
    12: "DRIVE",
    13: "DECEL",
    14: "PROFILE_BRAKE",
    16: "LOCK",
    17: "TAIL",
    18: "CROSS_BRAKE",
    19: "ADC_INVALID",
    20: "INPUT_LOST",
    21: "INPUT_RANGE_RELEASE",
}


@dataclass
class Segment:
    role: str
    start_ms: float
    end_ms: float
    duration_ms: float
    avg_duty: float
    max_duty: int
    events: str
    min_error: int
    max_error: int


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def to_int(row: dict[str, str], key: str, default: int = 0) -> int:
    value = row.get(key, "")
    if value == "":
        return default
    return int(float(value))


def load_customer_blocks(path: Path) -> list[dict[str, str]]:
    rows = read_csv(path)
    rows.sort(key=lambda r: (int(r["cluster"]), int(r["block"])))
    return rows


def clean_vsg_rows(rows: list[dict[str, str]]) -> tuple[list[dict[str, str]], int]:
    clean: list[dict[str, str]] = []
    dropped = 0
    for r in rows:
        all_zero_frame = (
            to_int(r, "time_ms") == 0
            and to_int(r, "pwm_us") == 0
            and to_int(r, "target_adc") == 0
            and to_int(r, "feedback_adc") == 0
            and to_int(r, "error_adc") == 0
            and to_int(r, "event") == 0
        )
        if all_zero_frame:
            dropped += 1
            continue
        clean.append(r)
    return clean, dropped


def vsg_valid(rows: list[dict[str, str]]) -> tuple[bool, str]:
    if not rows:
        return False, "VSG CSV has no rows"
    times = [to_int(r, "time_ms") for r in rows if r.get("time_ms")]
    if len(set(times)) < 2:
        return False, "VSG rows exist but time_ms does not move"
    consistency = 0
    checked = 0
    for r in rows[: min(len(rows), 400)]:
        if not {"target_adc", "feedback_adc", "error_adc"} <= set(r):
            continue
        checked += 1
        if to_int(r, "target_adc") - to_int(r, "feedback_adc") == to_int(r, "error_adc"):
            consistency += 1
    if checked and consistency < checked * 0.8:
        return False, "VSG error_adc does not consistently match target-feedback"
    return True, "valid"


def vsg_activity_note(rows: list[dict[str, str]], dropped: int) -> str | None:
    if not rows:
        return None
    duties = [abs(to_int(r, "duty")) for r in rows]
    active = sum(1 for d in duties if d)
    events = {to_int(r, "event") for r in rows}
    notes: list[str] = []
    if dropped:
        notes.append(f"dropped {dropped} all-zero startup/stale frames")
    if active == 0 and not (events & {12, 13, 14, 16, 17, 18}):
        notes.append("current VSG capture is real but contains no motor action; it only shows HOLD/coast")
    return "; ".join(notes) if notes else None


def find_vsg_start(rows: list[dict[str, str]]) -> int:
    if not rows:
        return 0
    prev_target = to_int(rows[0], "target_adc")
    prev_pwm = to_int(rows[0], "pwm_us")
    for i, r in enumerate(rows[1:], start=1):
        target_delta = abs(to_int(r, "target_adc") - prev_target)
        pwm_delta = abs(to_int(r, "pwm_us") - prev_pwm)
        duty = abs(to_int(r, "duty"))
        event = to_int(r, "event")
        if target_delta >= 3 or pwm_delta >= 4 or duty > 0 or event in (12, 13, 14, 16, 17, 18):
            return i
        prev_target = to_int(r, "target_adc")
        prev_pwm = to_int(r, "pwm_us")
    return 0


def event_role(row: dict[str, str]) -> str:
    event = to_int(row, "event")
    duty = to_int(row, "duty")
    if event == 2 and duty == 0:
        return "hold/coast"
    if event == 12:
        return "drive"
    if event in (13, 14, 18):
        return "brake/decel"
    if event == 17:
        return "tail"
    if event == 16:
        return "lock"
    if duty:
        return "drive" if duty > 0 else "reverse"
    return EVENT_NAMES.get(event, f"event{event}")


def compress_vsg_segments(rows: list[dict[str, str]]) -> tuple[list[Segment], float]:
    if not rows:
        return [], 0.0
    start_index = find_vsg_start(rows)
    t0 = to_int(rows[start_index], "time_ms")
    active = rows[start_index:]
    segments: list[Segment] = []
    current_role = event_role(active[0])
    current: list[dict[str, str]] = []

    def flush() -> None:
        if not current:
            return
        start = to_int(current[0], "time_ms") - t0
        end = to_int(current[-1], "time_ms") - t0
        duties = [to_int(r, "duty") for r in current]
        errors = [to_int(r, "error_adc") for r in current]
        events = sorted({to_int(r, "event") for r in current})
        segments.append(
            Segment(
                role=current_role,
                start_ms=float(start),
                end_ms=float(end),
                duration_ms=float(max(0, end - start)),
                avg_duty=sum(duties) / len(duties),
                max_duty=max((abs(d) for d in duties), default=0),
                events="/".join(EVENT_NAMES.get(e, str(e)) for e in events),
                min_error=min(errors, default=0),
                max_error=max(errors, default=0),
            )
        )

    for r in active:
        role = event_role(r)
        if current and role != current_role:
            flush()
            current = []
            current_role = role
        current.append(r)
    flush()

    merged: list[Segment] = []
    for seg in segments:
        if seg.duration_ms < 1 and merged:
            continue
        merged.append(seg)
        if len(merged) >= 12:
            break
    return merged, float(t0)


def customer_summary(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    result: list[dict[str, str]] = []
    for r in rows:
        result.append(
            {
                "cluster": r["cluster"],
                "role": r["role"],
                "start": f'{float(r["rel_start_ms"]):.3f}',
                "duration": f'{float(r["duration_ms"]):.3f}',
                "pulses": r["pulses"],
                "freq": f'{float(r["freq_khz"]):.2f}',
            }
        )
    return result


def fmt_ms(value: float) -> str:
    return f"{value:.1f}"


def generate_svg(customer: list[dict[str, str]], vsg: list[Segment], valid: bool, reason: str) -> str:
    width = 1320
    height = 620
    left = 140
    right = 40
    scale_ms = 50.0
    plot_w = width - left - right

    def x(ms: float) -> float:
        return left + max(0.0, min(ms, scale_ms)) / scale_ms * plot_w

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#f7f7f7"/>',
        '<text x="24" y="34" font-family="Arial" font-size="22" font-weight="700" fill="#222">CUSTOMER LOGICDATA vs OUR VSG</text>',
        f'<text x="24" y="60" font-family="Arial" font-size="13" fill="#555">Alignment: customer first drive edge = 0ms; VSG action start = 0ms. VSG status: {html.escape(reason)}</text>',
    ]
    for tick in range(0, 51, 5):
        xx = x(float(tick))
        parts.append(f'<line x1="{xx:.1f}" y1="86" x2="{xx:.1f}" y2="560" stroke="#ddd" stroke-width="1"/>')
        parts.append(f'<text x="{xx - 8:.1f}" y="582" font-family="Arial" font-size="11" fill="#666">{tick}</text>')
    parts.append('<text x="24" y="116" font-family="Arial" font-size="15" font-weight="700" fill="#222">Customer</text>')
    y = 136
    colors = {"drive": "#2ca02c", "reverse_brake": "#d62728", "brake": "#d62728", "tail": "#ff7f0e"}
    for r in customer[:16]:
        role = r["role"]
        start = float(r["start"])
        dur = float(r["duration"])
        c = colors.get(role, "#1f77b4")
        parts.append(f'<text x="24" y="{y+13}" font-family="Arial" font-size="12" fill="#333">C{html.escape(r["cluster"])} {html.escape(role)}</text>')
        parts.append(f'<rect x="{x(start):.1f}" y="{y}" width="{max(2.0, x(start+dur)-x(start)):.1f}" height="16" rx="2" fill="{c}" opacity="0.86"/>')
        parts.append(f'<text x="{x(start+dur)+5:.1f}" y="{y+13}" font-family="Arial" font-size="11" fill="#333">{dur:.2f}ms</text>')
        y += 22
    parts.append('<text x="24" y="392" font-family="Arial" font-size="15" font-weight="700" fill="#222">Our VSG</text>')
    y = 412
    for seg in vsg[:8]:
        c = "#2ca02c" if "drive" in seg.role else "#d62728" if "brake" in seg.role or "reverse" in seg.role else "#ff7f0e" if "tail" in seg.role else "#777"
        parts.append(f'<text x="24" y="{y+13}" font-family="Arial" font-size="12" fill="#333">{html.escape(seg.role)}</text>')
        parts.append(f'<rect x="{x(seg.start_ms):.1f}" y="{y}" width="{max(2.0, x(seg.end_ms)-x(seg.start_ms)):.1f}" height="16" rx="2" fill="{c}" opacity="{0.88 if valid else 0.35}"/>')
        parts.append(f'<text x="{x(seg.end_ms)+5:.1f}" y="{y+13}" font-family="Arial" font-size="11" fill="#333">{seg.duration_ms:.1f}ms duty max {seg.max_duty}</text>')
        y += 24
    parts.append('<text x="650" y="604" font-family="Arial" font-size="12" fill="#555">time after action start (ms)</text>')
    parts.append("</svg>")
    return "\n".join(parts)


def html_table(headers: list[str], rows: list[list[str]]) -> str:
    out = ["<table>", "<thead><tr>"]
    out += [f"<th>{html.escape(h)}</th>" for h in headers]
    out += ["</tr></thead><tbody>"]
    for row in rows:
        out.append("<tr>" + "".join(f"<td>{html.escape(cell)}</td>" for cell in row) + "</tr>")
    out.append("</tbody></table>")
    return "\n".join(out)


def generate_html(svg_name: str, customer: list[dict[str, str]], vsg: list[Segment],
                  valid: bool, reason: str, vsg_path: Path, invalid_note: str | None) -> str:
    customer_rows = [
        [r["cluster"], r["role"], r["start"], r["duration"], r["pulses"], r["freq"]]
        for r in customer
    ]
    vsg_rows = [
        [
            seg.role,
            fmt_ms(seg.start_ms),
            fmt_ms(seg.duration_ms),
            f"{seg.avg_duty:.0f}",
            str(seg.max_duty),
            seg.events,
            f"{seg.min_error}..{seg.max_error}",
        ]
        for seg in vsg
    ]
    verdict = "有效" if valid else "无效"
    note_html = f"<p class=\"warn\">{html.escape(invalid_note)}</p>" if invalid_note else ""
    return f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>CUSTOMER LOGICDATA vs VSG</title>
  <style>
    body{{margin:0;font-family:Arial,'PingFang SC',sans-serif;background:#f7f7f7;color:#222}}
    .bar{{position:sticky;top:0;z-index:2;padding:10px 14px;background:#fff;border-bottom:1px solid #ddd;font-size:14px}}
    main{{max-width:1320px;margin:0 auto;padding:12px}}
    img{{display:block;width:100%;background:#fff;border:1px solid #ddd}}
    .grid{{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px}}
    .panel{{background:#fff;border:1px solid #ddd;padding:10px}}
    h2{{font-size:16px;margin:0 0 8px}}
    p{{margin:6px 0;font-size:13px;color:#444}}
    .warn{{color:#b00020;font-weight:700}}
    table{{width:100%;border-collapse:collapse;font-size:12px}}
    th,td{{border:1px solid #ddd;padding:5px 6px;text-align:right;white-space:nowrap}}
    th:first-child,td:first-child,th:nth-child(2),td:nth-child(2){{text-align:left}}
    th{{background:#f0f0f0}}
    @media(max-width:900px){{.grid{{grid-template-columns:1fr}}}}
  </style>
</head>
<body>
  <div class="bar">CUSTOMER 对比 VSG 起始点对齐 | 只维护这个页面，不重复开窗口 | VSG: {verdict} | <span id="stamp"></span></div>
  <main>
    <img id="cmp" src="{html.escape(svg_name)}" alt="CUSTOMER logic compare VSG">
    <div class="panel">
      <h2>当前对比状态</h2>
      <p>VSG CSV: <code>{html.escape(str(vsg_path))}</code></p>
      <p>判定: <strong>{verdict}</strong>，原因: {html.escape(reason)}</p>
      <p>对齐: 客户第一段有效 drive = 0ms；我们 VSG 目标跳变或第一笔非零 H桥 duty = 0ms。</p>
      {note_html}
    </div>
    <div class="grid">
      <section class="panel">
        <h2>客户 LOGICDATA 分段</h2>
        {html_table(["cluster", "role", "start ms", "duration ms", "pulses", "kHz"], customer_rows)}
      </section>
      <section class="panel">
        <h2>我们 VSG 分段</h2>
        {html_table(["role", "start ms", "duration ms", "avg duty", "max duty", "events", "error"], vsg_rows)}
      </section>
    </div>
  </main>
  <script>
    setInterval(() => {{
      const now = Date.now();
      document.getElementById('cmp').src = '{html.escape(svg_name)}?t=' + now;
      document.getElementById('stamp').textContent = new Date(now).toLocaleTimeString();
    }}, 1000);
  </script>
</body>
</html>
"""


def update(args: argparse.Namespace) -> None:
    customer = customer_summary(load_customer_blocks(args.customer))
    raw_rows = read_csv(args.vsg) if args.vsg.exists() else []
    rows, dropped = clean_vsg_rows(raw_rows)
    valid, reason = vsg_valid(rows)
    selected = args.vsg
    invalid_note = vsg_activity_note(rows, dropped)
    if args.note:
        invalid_note = f"{invalid_note} {args.note}" if invalid_note else args.note
    if args.watch:
        stamp = time.strftime("%Y-%m-%d %H:%M:%S %Z")
        watch_note = f"Page generated at {stamp}; CSV capture runs separately and may be stale."
        invalid_note = f"{invalid_note} {watch_note}" if invalid_note else watch_note
    if not valid and args.fallback_vsg.exists():
        fallback_note = f"Requested VSG sample is invalid: {args.vsg} - {reason}. Showing fallback VSG sample instead until a new live capture replaces it."
        raw_rows = read_csv(args.fallback_vsg)
        rows, dropped = clean_vsg_rows(raw_rows)
        selected = args.fallback_vsg
        valid, reason = vsg_valid(rows)
        activity_note = vsg_activity_note(rows, dropped)
        invalid_note = f"{fallback_note} {activity_note or ''} {invalid_note or ''}".strip()

    vsg_segments, _start = compress_vsg_segments(rows)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    svg = generate_svg(customer, vsg_segments, valid, reason)
    svg_path = args.out_dir / "customer_logic_compare.svg"
    html_path = args.out_dir / "customer_logic_compare.html"
    svg_path.write_text(svg, encoding="utf-8")
    page = generate_html(svg_path.name, customer, vsg_segments, valid, reason, selected, invalid_note)
    if args.watch:
        page = page.replace("</head>", '<meta http-equiv="refresh" content="2"></head>', 1)
    html_path.write_text(page, encoding="utf-8")
    print(f"updated {html_path}")
    print(f"updated {svg_path}")
    print(f"vsg={selected} valid={valid} reason={reason}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--customer", type=Path, default=Path("build/customer_logic_compare/customer_logic_blocks.csv"))
    parser.add_argument("--vsg", type=Path, default=Path("build/servo_curve_live_v96l1_compare/servo_curve_live.csv"))
    parser.add_argument("--fallback-vsg", type=Path, default=Path("build/servo_curve_live_v96t_ext/servo_curve_live.csv"))
    parser.add_argument("--out-dir", type=Path, default=Path("build/customer_logic_compare"))
    parser.add_argument("--note", default="", help="Evidence note shown above the comparison tables.")
    parser.add_argument("--watch", action="store_true", help="Regenerate from CSV every second; capture must run separately.")
    args = parser.parse_args()
    while True:
        try:
            update(args)
        except (OSError, csv.Error, ValueError) as exc:
            if not args.watch:
                raise
            print(f"comparison update failed: {exc}", flush=True)
        if not args.watch:
            break
        time.sleep(1)


if __name__ == "__main__":
    main()
