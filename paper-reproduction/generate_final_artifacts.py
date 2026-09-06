#!/usr/bin/env python3
"""Generate final paper tables and vector figures from published validation CSVs."""
from __future__ import annotations
import csv, html, math, pathlib, re

ROOT = pathlib.Path(__file__).resolve().parent
RESULTS = ROOT / "final-results"
OUT = ROOT / "final-artifacts"
FIG = OUT / "figures"
OUT.mkdir(exist_ok=True)
FIG.mkdir(exist_ok=True)


def read_csv(path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))

paired = read_csv(RESULTS / "paired-validation" / "progress_validation_summary.csv")
robust = read_csv(RESULTS / "robustness-validation" / "progress_robustness_summary.csv")

# Preserve source rows and add only mechanically derived paired differences.
def paired_rows(rows, keys):
    groups = {}
    for r in rows:
        groups.setdefault(tuple(r[k] for k in keys), {})[r["variant"]] = r
    out = []
    for key, variants in sorted(groups.items()):
        if "reference" not in variants or "experimental" not in variants:
            continue
        a, b = variants["reference"], variants["experimental"]
        row = {k: v for k, v in zip(keys, key)}
        row.update({"runs_reference": a["runs"], "runs_experimental": b["runs"]})
        for metric in ("pdr_mean", "pdr_ci95", "avgDelay_mean", "avgDelay_ci95", "broadcasts_mean", "suppressed_mean"):
            row[f"reference_{metric}"] = a[metric]
            row[f"experimental_{metric}"] = b[metric]
        row["pdr_difference_pp"] = f"{float(b['pdr_mean'])-float(a['pdr_mean']):.6f}"
        row["delay_difference"] = f"{float(b['avgDelay_mean'])-float(a['avgDelay_mean']):.6f}"
        row["broadcast_difference"] = f"{float(b['broadcasts_mean'])-float(a['broadcasts_mean']):.6f}"
        out.append(row)
    return out

paired_out = paired_rows(paired, ["nUavs", "mobility"])
robust_out = paired_rows(robust, ["nUavs", "mobility", "byzFraction"])


def write_csv(path, rows):
    if not rows:
        return
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader(); w.writerows(rows)

write_csv(OUT / "paired_validation_final.csv", paired_out)
write_csv(OUT / "robustness_validation_final.csv", robust_out)

# Minimal dependency-free SVG and PDF plotting. Values come directly from generated CSVs.
COLORS = {"reference": "#4C78A8", "experimental": "#E45756"}

def svg_plot(path, title, labels, series, ylabel, ymin=0, ymax=100):
    W,H,L,R,T,B = 1000,560,90,35,65,80
    pw, ph = W-L-R, H-T-B
    def x(i): return L + (0 if len(labels)==1 else i*pw/(len(labels)-1))
    def y(v): return T + (ymax-v)*(ph/(ymax-ymin))
    parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">', '<rect width="100%" height="100%" fill="white"/>']
    parts.append(f'<text x="{W/2}" y="28" text-anchor="middle" font-family="Arial" font-size="20" font-weight="bold">{html.escape(title)}</text>')
    for tick in range(int(ymin), int(ymax)+1, 20):
        yy=y(tick); parts.append(f'<line x1="{L}" y1="{yy:.1f}" x2="{W-R}" y2="{yy:.1f}" stroke="#dddddd"/>')
        parts.append(f'<text x="{L-10}" y="{yy+5:.1f}" text-anchor="end" font-family="Arial" font-size="13">{tick}</text>')
    parts += [f'<line x1="{L}" y1="{T}" x2="{L}" y2="{H-B}" stroke="#222"/>', f'<line x1="{L}" y1="{H-B}" x2="{W-R}" y2="{H-B}" stroke="#222"/>']
    for i, lab in enumerate(labels):
        xx=x(i); parts.append(f'<text x="{xx:.1f}" y="{H-B+25}" text-anchor="middle" font-family="Arial" font-size="13">{html.escape(lab)}</text>')
    parts.append(f'<text transform="translate(20 {H/2}) rotate(-90)" text-anchor="middle" font-family="Arial" font-size="14">{html.escape(ylabel)}</text>')
    for name, vals in series.items():
        pts=' '.join(f'{x(i):.1f},{y(v):.1f}' for i,v in enumerate(vals))
        parts.append(f'<polyline points="{pts}" fill="none" stroke="{COLORS[name]}" stroke-width="4"/>')
        for i,v in enumerate(vals): parts.append(f'<circle cx="{x(i):.1f}" cy="{y(v):.1f}" r="5" fill="{COLORS[name]}"/>')
    lx=W-R-210
    for j,name in enumerate(series):
        yy=T+20+j*24; parts.append(f'<line x1="{lx}" y1="{yy}" x2="{lx+25}" y2="{yy}" stroke="{COLORS[name]}" stroke-width="4"/>'); parts.append(f'<text x="{lx+32}" y="{yy+5}" font-family="Arial" font-size="13">{html.escape(name)}</text>')
    parts.append('</svg>')
    path.write_text('\n'.join(parts))


def pdf_plot(path, title, labels, series, ylabel, ymin=0, ymax=100):
    # One-page vector PDF using built-in Helvetica; intentionally simple and portable.
    W,H,L,R,T,B=720,420,70,25,48,58; pw,ph=W-L-R,H-T-B
    def x(i): return L + (0 if len(labels)==1 else i*pw/(len(labels)-1))
    def y(v): return B + (v-ymin)*ph/(ymax-ymin)
    cmds=["BT /F1 14 Tf 250 395 Td ("+title.replace('(','[').replace(')',']')+") Tj ET", "0.8 w"]
    for tick in range(int(ymin),int(ymax)+1,20):
        yy=y(tick); cmds += [f"0.85 0.85 0.85 RG {L} {yy:.1f} m {W-R} {yy:.1f} l S", f"0 0 0 RG BT /F1 8 Tf {L-28} {yy-3:.1f} Td ({tick}) Tj ET"]
    cmds += [f"0 0 0 RG {L} {B} m {L} {H-T} l S", f"{L} {B} m {W-R} {B} l S"]
    for i,lab in enumerate(labels): cmds.append(f"BT /F1 8 Tf {x(i)-10:.1f} {B-18} Td ({lab}) Tj ET")
    cmds.append(f"BT /F1 9 Tf 12 175 Td 0 0 Td ({ylabel}) Tj ET")
    for name, vals in series.items():
        rgb={'reference':(0.298,0.471,0.659),'experimental':(0.894,0.341,0.337)}[name]
        cmds.append(f"{rgb[0]} {rgb[1]} {rgb[2]} RG 2 w")
        for i,v in enumerate(vals): cmds.append((f"{x(i):.1f} {y(v):.1f} m" if i==0 else f"{x(i):.1f} {y(v):.1f} l"))
        cmds.append("S")
        for i,v in enumerate(vals): cmds.append(f"{x(i)-2:.1f} {y(v)-2:.1f} {4} {4} re f")
    stream='\n'.join(cmds).encode()
    objs=[b"<< /Type /Catalog /Pages 2 0 R >>", b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>", b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 720 420] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>", b"<< /Length "+str(len(stream)).encode()+b" >>\nstream\n"+stream+b"\nendstream", b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"]
    out=b"%PDF-1.4\n"; offs=[0]
    for i,o in enumerate(objs,1): offs.append(len(out)); out += f"{i} 0 obj\n".encode()+o+b"\nendobj\n"
    xref=len(out); out += b"xref\n0 6\n0000000000 65535 f \n" + b''.join(f"{v:010d} 00000 n \n".encode() for v in offs[1:]); out += f"trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode(); path.write_bytes(out)

labels=["10 GM3D", "20 GM3D", "20 RPGM"]
ref=[float(paired_out[i]["reference_pdr_mean"]) for i in range(3)]
exp=[float(paired_out[i]["experimental_pdr_mean"]) for i in range(3)]
svg_plot(FIG/"paired_pdr.svg", "Paired validation: PDR", labels, {"reference":ref,"experimental":exp}, "PDR (%)")
pdf_plot(FIG/"paired_pdr.pdf", "Paired validation: PDR", labels, {"reference":ref,"experimental":exp}, "PDR (%)")
labels2=["20 GM3D\nByz", "20 RPGM\nNominal", "20 RPGM\nByz"]
ref2=[float(robust_out[i]["reference_pdr_mean"]) for i in range(3)]
exp2=[float(robust_out[i]["experimental_pdr_mean"]) for i in range(3)]
svg_plot(FIG/"robustness_pdr.svg", "Robustness validation: PDR", [x.replace('\\n','/') for x in labels2], {"reference":ref2,"experimental":exp2}, "PDR (%)")
pdf_plot(FIG/"robustness_pdr.pdf", "Robustness validation: PDR", [x.replace('\\n','/') for x in labels2], {"reference":ref2,"experimental":exp2}, "PDR (%)")

# English interpretation is generated from the same rows and explicitly bounded.
interp = ["# Final validation interpretation", "", "All values below are generated from the published CSV summaries; no historical manuscript value is reused.", "", "## Paired nominal validation", ""]
for r in paired_out:
    d=float(r['pdr_difference_pp']); sign='increase' if d>=0 else 'decrease'
    interp.append(f"- **{r['nUavs']} UAV, {r['mobility'].upper()}:** PDR changes from {r['reference_pdr_mean']}% to {r['experimental_pdr_mean']}% ({d:+.4f} percentage points); mean delay changes by {float(r['delay_difference']):+.6f} and broadcasts by {float(r['broadcast_difference']):+.2f}.")
interp += ["", "## Byzantine and mobility robustness", ""]
for r in robust_out:
    d=float(r['pdr_difference_pp'])
    interp.append(f"- **{r['nUavs']} UAV, {r['mobility'].upper()}, Byzantine fraction {r['byzFraction']}:** PDR changes from {r['reference_pdr_mean']}% to {r['experimental_pdr_mean']}% ({d:+.4f} percentage points). This is a scenario-dependent result, not evidence of universal Byzantine robustness.")
interp += ["", "## Reproducibility limits", "", "The paired campaign contains a common failed run for 20-UAV RPGM seed 3003; it is marked FAIL in both reference and experimental status files and is not silently converted into a numerical result. The final candidate therefore remains an experimental, mobility-dependent variant. The artifact does not claim official packet-level reproduction of baselines that are not executable in the public scenario."]
(OUT/"interpretation_en.md").write_text('\n'.join(interp)+'\n')
(OUT/"README.md").write_text("""# Final validation artifacts\n\nGenerated by `paper-reproduction/generate_final_artifacts.py` from the published paired and robustness CSV summaries.\n\nContents:\n- `paired_validation_final.csv`: paired reference/experimental nominal results and derived differences.\n- `robustness_validation_final.csv`: Byzantine and additional RPGM results and derived differences.\n- `figures/*.svg`: browser/GitHub-viewable vector plots.\n- `figures/*.pdf`: vector plots used by the LaTeX manuscript.\n- `interpretation_en.md`: English interpretation generated from the same CSV rows.\n\nThe common RPGM seed 3003 failure remains in the source status files and is excluded from means exactly as published.\n""")
print(f"generated {len(paired_out)} paired rows and {len(robust_out)} robustness rows")
