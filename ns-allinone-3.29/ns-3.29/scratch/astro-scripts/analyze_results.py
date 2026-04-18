#!/usr/bin/env python3
"""
ASTRO-FANET Results Analysis and Figure Generation
====================================================
Parses CSV results from the NS-3 campaign and generates:
- Tables matching those in Section 4.6 (with 95% CI)
- Figures for PDR vs N, delay, throughput, BRR, etc.
- Statistical tests (Welch t-test with Bonferroni correction)

Usage:
  python3 analyze_results.py --results-dir results/ --output-dir figures/
"""

import os
import sys
import glob
import argparse
import numpy as np
import pandas as pd
from scipy import stats

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    HAS_MPL = True
except ImportError:
    HAS_MPL = False
    print("WARNING: matplotlib not installed. Tables will be printed but no figures generated.")


def load_results(results_dir):
    """Load all CSV result files into a DataFrame."""
    csv_files = glob.glob(os.path.join(results_dir, "**/*.csv"), recursive=True)
    if not csv_files:
        print(f"No CSV files found in {results_dir}")
        return pd.DataFrame()

    dfs = []
    for f in csv_files:
        try:
            df = pd.read_csv(f)
            dfs.append(df)
        except Exception as e:
            print(f"Warning: could not parse {f}: {e}")

    if not dfs:
        return pd.DataFrame()

    return pd.concat(dfs, ignore_index=True)


def compute_ci95(values):
    """Compute mean and 95% CI using Student's t-distribution (n-1 df)."""
    n = len(values)
    if n < 2:
        return np.mean(values), 0.0
    mean = np.mean(values)
    se = stats.sem(values)
    ci = se * stats.t.ppf(0.975, n - 1)
    return mean, ci


def welch_ttest_bonferroni(astro_vals, baseline_vals, num_comparisons=5):
    """Welch t-test with Bonferroni correction."""
    t_stat, p_val = stats.ttest_ind(astro_vals, baseline_vals, equal_var=False)
    p_corrected = min(p_val * num_comparisons, 1.0)
    return t_stat, p_corrected


def print_pdr_table(df, mobility="gm3d"):
    """Print Table 3: PDR under specified mobility."""
    print(f"\n{'='*70}")
    print(f"PDR (%) under {mobility.upper()} mobility (mean ± 95% CI over 30 runs)")
    print(f"{'='*70}")

    protocols = ["aodv", "olsr", "epidemic", "dqn", "astro"]
    n_values = [10, 20, 30, 40]

    header = f"{'Protocol':<12}" + "".join(f"{'N='+str(n):<18}" for n in n_values)
    print(header)
    print("-" * 70)

    for proto in protocols:
        row = f"{proto.upper():<12}"
        for n in n_values:
            subset = df[(df['protocol'] == proto) &
                        (df['nUavs'] == n) &
                        (df['mobility'] == mobility)]
            if len(subset) > 0:
                mean, ci = compute_ci95(subset['pdr'].values)
                row += f"{mean:.1f} ± {ci:.1f}      "
            else:
                row += f"{'N/A':<18}"
        print(row)

    # Statistical significance
    print("\nStatistical significance (ASTRO vs baselines at N=30):")
    astro_30 = df[(df['protocol'] == 'astro') & (df['nUavs'] == 30) &
                  (df['mobility'] == mobility)]['pdr'].values
    for proto in ["aodv", "olsr", "epidemic", "dqn"]:
        baseline_30 = df[(df['protocol'] == proto) & (df['nUavs'] == 30) &
                         (df['mobility'] == mobility)]['pdr'].values
        if len(astro_30) > 1 and len(baseline_30) > 1:
            t, p = welch_ttest_bonferroni(astro_30, baseline_30)
            sig = "***" if p < 0.001 else "**" if p < 0.01 else "*" if p < 0.05 else "ns"
            print(f"  vs {proto.upper()}: t={t:.2f}, p={p:.4f} {sig}")


def print_delay_table(df, n=30, mobility="gm3d"):
    """Print Table 5: Delay, Throughput, AoI."""
    print(f"\n{'='*70}")
    print(f"Delay (ms), Throughput (kbit/s), AoI (ms) at N={n} under {mobility.upper()}")
    print(f"{'='*70}")

    protocols = ["aodv", "olsr", "epidemic", "dqn", "astro"]
    header = f"{'Protocol':<12}{'Delay (ms)':<18}{'Throughput':<18}{'AoI (ms)':<18}"
    print(header)
    print("-" * 70)

    for proto in protocols:
        subset = df[(df['protocol'] == proto) & (df['nUavs'] == n) &
                     (df['mobility'] == mobility)]
        if len(subset) > 0:
            d_mean, d_ci = compute_ci95(subset['avgDelay'].values)
            t_mean, t_ci = compute_ci95(subset['throughput'].values)
            a_mean, a_ci = compute_ci95(subset['avgAoI'].values)
            print(f"{proto.upper():<12}"
                  f"{d_mean:.0f} ± {d_ci:.0f}       "
                  f"{t_mean:.0f} ± {t_ci:.0f}       "
                  f"{a_mean:.0f} ± {a_ci:.0f}")
        else:
            print(f"{proto.upper():<12}{'N/A':<18}{'N/A':<18}{'N/A':<18}")


def print_energy_table(df, n=30, mobility="gm3d"):
    """Print Table 6: Energy per bit and control overhead."""
    print(f"\n{'='*70}")
    print(f"Energy/bit (uJ/bit) and Control overhead (%) at N={n}")
    print(f"{'='*70}")

    protocols = ["aodv", "olsr", "epidemic", "dqn", "astro"]
    header = f"{'Protocol':<12}{'E_bit (uJ/bit)':<20}{'O_ctrl (%)':<20}"
    print(header)
    print("-" * 70)

    for proto in protocols:
        subset = df[(df['protocol'] == proto) & (df['nUavs'] == n) &
                     (df['mobility'] == mobility)]
        if len(subset) > 0:
            e_mean, e_ci = compute_ci95(subset['energyPerBit'].values)
            o_mean, o_ci = compute_ci95(subset['ctrlOverhead'].values)
            print(f"{proto.upper():<12}"
                  f"{e_mean:.1f} ± {e_ci:.1f}          "
                  f"{o_mean:.1f} ± {o_ci:.1f}")
        else:
            print(f"{proto.upper():<12}{'N/A':<20}{'N/A':<20}")


def plot_pdr_vs_n(df, mobility="gm3d", output_dir="figures"):
    """Generate Figure 4: PDR vs N."""
    if not HAS_MPL:
        return

    protocols = ["aodv", "olsr", "epidemic", "dqn", "astro"]
    colors = {"aodv": "blue", "olsr": "green", "epidemic": "orange",
              "dqn": "red", "astro": "black"}
    markers = {"aodv": "s", "olsr": "^", "epidemic": "D", "dqn": "v", "astro": "o"}
    labels = {"aodv": "AODV", "olsr": "OLSR", "epidemic": "Epidemic",
              "dqn": "DQN-QR", "astro": "ASTRO-FANET"}

    fig, ax = plt.subplots(figsize=(8, 5))

    n_values = [10, 20, 30, 40]
    for proto in protocols:
        means, cis = [], []
        for n in n_values:
            subset = df[(df['protocol'] == proto) & (df['nUavs'] == n) &
                         (df['mobility'] == mobility)]
            if len(subset) > 0:
                m, c = compute_ci95(subset['pdr'].values)
                means.append(m)
                cis.append(c)
            else:
                means.append(np.nan)
                cis.append(0)

        lw = 2.5 if proto == "astro" else 1.5
        ax.errorbar(n_values, means, yerr=cis,
                     label=labels[proto], color=colors[proto],
                     marker=markers[proto], linewidth=lw,
                     capsize=3, markersize=7)

    ax.set_xlabel("Number of UAVs (N)", fontsize=12)
    ax.set_ylabel("Packet Delivery Ratio (%)", fontsize=12)
    ax.set_title(f"PDR vs Swarm Size ({mobility.upper()} Mobility)", fontsize=13)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xticks(n_values)
    ax.set_ylim(40, 100)

    os.makedirs(output_dir, exist_ok=True)
    fig.savefig(os.path.join(output_dir, f"pdr_vs_n_{mobility}.pdf"),
                bbox_inches='tight', dpi=150)
    fig.savefig(os.path.join(output_dir, f"pdr_vs_n_{mobility}.png"),
                bbox_inches='tight', dpi=150)
    plt.close(fig)
    print(f"  Saved: pdr_vs_n_{mobility}.pdf/png")


def plot_brr_vs_n(df, mobility="gm3d", output_dir="figures"):
    """Generate BRR vs N plot."""
    if not HAS_MPL:
        return

    protocols = ["aodv", "olsr", "epidemic", "dqn", "astro"]
    colors = {"aodv": "blue", "olsr": "green", "epidemic": "orange",
              "dqn": "red", "astro": "black"}

    fig, ax = plt.subplots(figsize=(8, 5))
    n_values = [10, 20, 30, 40]

    for proto in protocols:
        means = []
        for n in n_values:
            subset = df[(df['protocol'] == proto) & (df['nUavs'] == n) &
                         (df['mobility'] == mobility)]
            if len(subset) > 0:
                means.append(subset['brr'].mean())
            else:
                means.append(np.nan)

        lw = 2.5 if proto == "astro" else 1.5
        ax.plot(n_values, means, label=proto.upper(),
                color=colors[proto], linewidth=lw, marker='o')

    ax.set_xlabel("Number of UAVs (N)", fontsize=12)
    ax.set_ylabel("Broadcast Redundancy Ratio", fontsize=12)
    ax.set_title(f"BRR vs Swarm Size ({mobility.upper()})", fontsize=13)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)

    os.makedirs(output_dir, exist_ok=True)
    fig.savefig(os.path.join(output_dir, f"brr_vs_n_{mobility}.pdf"),
                bbox_inches='tight', dpi=150)
    plt.close(fig)
    print(f"  Saved: brr_vs_n_{mobility}.pdf")


def main():
    parser = argparse.ArgumentParser(description="Analyze ASTRO-FANET results")
    parser.add_argument("--results-dir", default="results",
                        help="Directory containing CSV results")
    parser.add_argument("--output-dir", default="figures",
                        help="Directory for output figures")
    args = parser.parse_args()

    print("Loading results...")
    df = load_results(args.results_dir)

    if df.empty:
        print("No results found. Run the simulation campaign first.")
        print("  python3 run_campaign.py --ns3-dir /path/to/ns-3.29")
        sys.exit(1)

    print(f"Loaded {len(df)} simulation runs")
    print(f"Protocols: {df['protocol'].unique()}")
    print(f"UAV counts: {sorted(df['nUavs'].unique())}")
    print(f"Mobility models: {df['mobility'].unique()}")

    # Print tables
    for mob in ["gm3d", "rpgm"]:
        if mob in df['mobility'].values:
            print_pdr_table(df, mob)

    print_delay_table(df, n=30, mobility="gm3d")
    print_energy_table(df, n=30, mobility="gm3d")

    # Generate figures
    print("\nGenerating figures...")
    for mob in ["gm3d", "rpgm"]:
        if mob in df['mobility'].values:
            plot_pdr_vs_n(df, mob, args.output_dir)
            plot_brr_vs_n(df, mob, args.output_dir)

    print("\nAnalysis complete.")


if __name__ == "__main__":
    main()
