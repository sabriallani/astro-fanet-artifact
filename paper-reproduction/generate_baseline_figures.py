#!/usr/bin/env python3
"""Generate comparison figures and LaTeX tables from the baseline campaign CSV.

Consumes ``baseline_campaign_summary.csv`` (produced by
``aggregate_baseline_campaign.py``) and emits, for each reported metric:

* a grouped bar chart with 95 % confidence error bars (SVG for GitHub, vector
  PDF for the manuscript);
* a LaTeX table body with mean +/- CI per cell.

Every number is read from the summary CSV. Nothing is smoothed, imputed or
hand-edited: cells whose runs all failed are rendered as gaps and listed in the
generated notes, because a missing simulation must stay visibly missing.

Dependency-free by design (no matplotlib/numpy): the environment has neither.
"""

from __future__ import annotations

import argparse
import csv
import html
import pathlib

# Display order and labels. `astro` is the contribution; the rest are the local
# re-implementations of the classical suppression families.
PROTOCOL_ORDER = ["astro", "sf", "pr", "cb", "sba"]
PROTOCOL_LABEL = {
    "astro": "A3D-BSM",
    "sf": "SF",
    "pr": "PR",
    "cb": "CB",
    "sba": "SBA",
}
PROTOCOL_COLOR = {
    "astro": "#E45756",
    "sf": "#4C78A8",
    "pr": "#72B7B2",
    "cb": "#F58518",
    "sba": "#54A24B",
}
PROTOCOL_RGB = {
    "astro": (0.894, 0.341, 0.337),
    "sf": (0.298, 0.471, 0.659),
    "pr": (0.447, 0.718, 0.698),
    "cb": (0.961, 0.522, 0.094),
    "sba": (0.329, 0.635, 0.294),
}

# metric key -> (axis label, figure title, filename stem)
METRIC_SPECS = {
    "pdr": ("PDR (%)", "Packet delivery ratio", "cmp_pdr"),
    "redundancyRatio": ("RR (%)", "Redundancy ratio (duplicate receptions)", "cmp_rr"),
    "savedRebroadcastRatio": ("SP (%)", "Saved rebroadcast ratio", "cmp_sp"),
    "broadcastPathLength": ("BL (hops)", "Mean broadcast path length", "cmp_bl"),
    "avgDelay": ("Delay (ms)", "Mean end-to-end delay", "cmp_delay"),
    "emergencyNonSuppressionRate": (
        "ENSR (%)", "Emergency non-suppression rate", "cmp_ensr"),
}


def read_summary(path: pathlib.Path) -> list[dict]:
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def cell_label(row: dict) -> str:
    return f"{row['nUavs']} {row['mobility'].upper()}"


def collect(rows: list[dict], metric: str):
    """Return (ordered cell labels, {protocol: [(mean, ci) or None per cell]})."""
    labels: list[str] = []
    for row in rows:
        label = cell_label(row)
        if label not in labels:
            labels.append(label)
    labels.sort(key=lambda s: (s.split()[1], int(s.split()[0])))

    index = {(r["protocol"], cell_label(r)): r for r in rows}
    series: dict[str, list] = {}
    for protocol in PROTOCOL_ORDER:
        column = []
        for label in labels:
            row = index.get((protocol, label))
            mean = row.get(f"{metric}_mean", "") if row is not None else ""
            if not mean:
                column.append(None)
                continue
            ci = row.get(f"{metric}_ci95") or "0"
            try:
                column.append((float(mean), float(ci)))
            except ValueError:
                column.append(None)
        if any(v is not None for v in column):
            series[protocol] = column
    return labels, series


def nice_bounds(series: dict, cap: float | None = None):
    """Return (lo, hi, step) with a round tick step, honouring an optional cap."""
    import math as _math

    values = [m + c for col in series.values() for v in col if v for m, c in [v]]
    values += [m - c for col in series.values() for v in col if v for m, c in [v]]
    if not values:
        return 0.0, 1.0, 0.5
    lo, hi = min(values), max(values)
    lo = min(0.0, lo)
    if cap is not None:
        # Bounded percentage metrics must never expose a negative axis caused
        # solely by a confidence interval around a zero mean.
        lo = max(0.0, lo)
    if hi <= lo:
        hi = lo + 1.0
    hi *= 1.12
    if cap is not None:
        hi = min(hi, cap)

    # Pick a round step giving roughly 5-6 intervals, then snap the top to it.
    span = hi - lo
    raw = span / 5.0
    mag = 10 ** _math.floor(_math.log10(raw)) if raw > 0 else 1.0
    for mult in (1, 2, 2.5, 5, 10):
        if mag * mult >= raw:
            step = mag * mult
            break
    else:
        step = mag * 10
    hi = min(cap, _math.ceil(hi / step) * step) if cap is not None else _math.ceil(hi / step) * step
    return lo, hi, step


def _ticks(lo, hi, step):
    """Tick values from lo up to hi inclusive, tolerant of float error."""
    out, i = [], 0
    while lo + i * step <= hi + step * 1e-9:
        out.append(lo + i * step)
        i += 1
    return out


def svg_grouped_bars(path, title, labels, series, ylabel, cap=None):
    W, H, L, R, T, B = 1000, 560, 95, 30, 70, 95
    pw, ph = W - L - R, H - T - B
    lo, hi, step = nice_bounds(series, cap)

    def y(v):
        return T + (hi - v) * ph / (hi - lo)

    n_groups = max(len(labels), 1)
    group_w = pw / n_groups
    n_series = max(len(series), 1)
    bar_w = group_w * 0.72 / n_series

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{W/2}" y="30" text-anchor="middle" font-family="Arial" '
        f'font-size="20" font-weight="bold">{html.escape(title)}</text>',
    ]

    for val in _ticks(lo, hi, step):
        yy = y(val)
        parts.append(f'<line x1="{L}" y1="{yy:.1f}" x2="{W-R}" y2="{yy:.1f}" stroke="#dddddd"/>')
        parts.append(
            f'<text x="{L-10}" y="{yy+5:.1f}" text-anchor="end" font-family="Arial" '
            f'font-size="12">{val:g}</text>')

    parts += [
        f'<line x1="{L}" y1="{T}" x2="{L}" y2="{H-B}" stroke="#222"/>',
        f'<line x1="{L}" y1="{H-B}" x2="{W-R}" y2="{H-B}" stroke="#222"/>',
    ]

    for gi, label in enumerate(labels):
        gx = L + gi * group_w
        parts.append(
            f'<text x="{gx+group_w/2:.1f}" y="{H-B+24}" text-anchor="middle" '
            f'font-family="Arial" font-size="13">{html.escape(label)}</text>')
        for si, protocol in enumerate(series):
            point = series[protocol][gi]
            if point is None:
                continue
            mean, ci = point
            bx = gx + group_w * 0.14 + si * bar_w
            top = y(max(mean, lo))
            base = y(lo)
            parts.append(
                f'<rect x="{bx:.1f}" y="{min(top, base):.1f}" width="{bar_w*0.9:.1f}" '
                f'height="{abs(base-top):.1f}" fill="{PROTOCOL_COLOR[protocol]}"/>')
            if ci > 0:
                cx = bx + bar_w * 0.45
                parts.append(
                    f'<line x1="{cx:.1f}" y1="{y(mean-ci):.1f}" x2="{cx:.1f}" '
                    f'y2="{y(mean+ci):.1f}" stroke="#222" stroke-width="1.6"/>')
                for edge in (mean - ci, mean + ci):
                    parts.append(
                        f'<line x1="{cx-4:.1f}" y1="{y(edge):.1f}" x2="{cx+4:.1f}" '
                        f'y2="{y(edge):.1f}" stroke="#222" stroke-width="1.6"/>')

    lx = L + 5
    for j, protocol in enumerate(series):
        ly = H - 30 + (j // 5) * 18
        off = lx + (j % 5) * 150
        parts.append(
            f'<rect x="{off}" y="{ly-10}" width="16" height="12" fill="{PROTOCOL_COLOR[protocol]}"/>')
        parts.append(
            f'<text x="{off+22}" y="{ly}" font-family="Arial" font-size="13">'
            f'{html.escape(PROTOCOL_LABEL[protocol])}</text>')

    parts.append(
        f'<text transform="translate(22 {H/2}) rotate(-90)" text-anchor="middle" '
        f'font-family="Arial" font-size="14">{html.escape(ylabel)}</text>')
    parts.append("</svg>")
    path.write_text("\n".join(parts))


def pdf_grouped_bars(path, title, labels, series, ylabel, cap=None):
    W, H, L, R, T, B = 720, 430, 74, 22, 46, 78
    pw, ph = W - L - R, H - T - B
    lo, hi, step = nice_bounds(series, cap)

    def y(v):
        return B + (v - lo) * ph / (hi - lo)

    n_groups = max(len(labels), 1)
    group_w = pw / n_groups
    n_series = max(len(series), 1)
    bar_w = group_w * 0.72 / n_series

    def esc(text):
        return text.replace("(", "[").replace(")", "]")

    cmds = [f"BT /F1 13 Tf {W/2-len(title)*3.2:.0f} {H-26} Td ({esc(title)}) Tj ET", "0.8 w"]
    for val in _ticks(lo, hi, step):
        yy = y(val)
        cmds.append(f"0.85 0.85 0.85 RG {L} {yy:.1f} m {W-R} {yy:.1f} l S")
        cmds.append(f"0 0 0 RG BT /F1 8 Tf {L-34} {yy-3:.1f} Td ({val:g}) Tj ET")
    cmds += [f"0 0 0 RG {L} {B} m {L} {H-T} l S", f"{L} {B} m {W-R} {B} l S"]

    for gi, label in enumerate(labels):
        gx = L + gi * group_w
        # Reset the fill colour: the previous group's bars left it coloured.
        cmds.append(f"0 0 0 rg BT /F1 8 Tf {gx+group_w/2-14:.1f} {B-16} Td ({esc(label)}) Tj ET")
        for si, protocol in enumerate(series):
            point = series[protocol][gi]
            if point is None:
                continue
            mean, ci = point
            bx = gx + group_w * 0.14 + si * bar_w
            r, g, b = PROTOCOL_RGB[protocol]
            height = y(mean) - y(lo)
            cmds.append(f"{r} {g} {b} rg {bx:.1f} {y(lo):.1f} {bar_w*0.9:.1f} {height:.1f} re f")
            if ci > 0:
                cx = bx + bar_w * 0.45
                cmds.append(
                    f"0 0 0 RG 1 w {cx:.1f} {y(mean-ci):.1f} m {cx:.1f} {y(mean+ci):.1f} l S")
                for edge in (mean - ci, mean + ci):
                    cmds.append(f"{cx-3:.1f} {y(edge):.1f} m {cx+3:.1f} {y(edge):.1f} l S")

    for j, protocol in enumerate(series):
        r, g, b = PROTOCOL_RGB[protocol]
        lx = L + (j % 5) * 108
        ly = 22
        cmds.append(f"{r} {g} {b} rg {lx} {ly} 12 9 re f")
        cmds.append(f"0 0 0 rg BT /F1 8 Tf {lx+16} {ly+1} Td ({PROTOCOL_LABEL[protocol]}) Tj ET")

    cmds.append(f"0 0 0 rg BT /F1 9 Tf 14 {H/2-30:.0f} Td ({esc(ylabel)}) Tj ET")

    stream = "\n".join(cmds).encode()
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + f"{W} {H}".encode()
        + b"] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>",
        b"<< /Length " + str(len(stream)).encode() + b" >>\nstream\n" + stream + b"\nendstream",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    ]
    out = b"%PDF-1.4\n"
    offs = [0]
    for i, obj in enumerate(objs, 1):
        offs.append(len(out))
        out += f"{i} 0 obj\n".encode() + obj + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 6\n0000000000 65535 f \n"
    out += b"".join(f"{v:010d} 00000 n \n".encode() for v in offs[1:])
    out += f"trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode()
    path.write_bytes(out)


def latex_table(rows: list[dict], metric: str, label: str) -> str:
    labels, series = collect(rows, metric)
    lines = [
        "% Generated by generate_baseline_figures.py -- do not edit by hand.",
        "\\begin{tabular}{l" + "r" * len(labels) + "}",
        "\\hline",
        "Scheme & " + " & ".join(labels) + " \\\\",
        "\\hline",
    ]
    for protocol in PROTOCOL_ORDER:
        if protocol not in series:
            continue
        cells = []
        for point in series[protocol]:
            cells.append("--" if point is None else f"${point[0]:.2f} \\pm {point[1]:.2f}$")
        lines.append(f"{PROTOCOL_LABEL[protocol]} & " + " & ".join(cells) + " \\\\")
    lines += ["\\hline", "\\end{tabular}", f"% metric: {metric} ({label})"]
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--summary", required=True, type=pathlib.Path)
    parser.add_argument("--figures", required=True, type=pathlib.Path)
    parser.add_argument("--tables", required=True, type=pathlib.Path)
    args = parser.parse_args()

    rows = read_summary(args.summary)
    if not rows:
        print(f"ERROR empty summary: {args.summary}")
        return 1

    args.figures.mkdir(parents=True, exist_ok=True)
    args.tables.mkdir(parents=True, exist_ok=True)

    produced = []
    for metric, (ylabel, title, stem) in METRIC_SPECS.items():
        labels, series = collect(rows, metric)
        if not series:
            print(f"SKIP {metric}: no data in summary")
            continue
        # Percentage metrics are bounded by the protocol definition.
        cap = 100.0 if metric in (
            "pdr", "redundancyRatio", "savedRebroadcastRatio",
            "emergencyNonSuppressionRate",
        ) else None
        svg_grouped_bars(args.figures / f"{stem}.svg", title, labels, series, ylabel, cap)
        pdf_grouped_bars(args.figures / f"{stem}.pdf", title, labels, series, ylabel, cap)
        (args.tables / f"{stem}.tex").write_text(latex_table(rows, metric, ylabel))
        produced.append(stem)
        print(f"generated {stem}.svg/.pdf/.tex over {len(labels)} cells, {len(series)} schemes")

    print(f"total figures: {len(produced)}")
    return 0 if produced else 1


if __name__ == "__main__":
    raise SystemExit(main())
