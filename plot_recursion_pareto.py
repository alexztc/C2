#!/usr/bin/env python3
"""
plot_recursion_pareto.py — Pareto-frontier visualisation of the max_recursion
ablation across the three C² trie variants (C²-FST, C²-CoCo, C²-Marisa).

Reads:   the most recent results/recursion_ablation_*.csv
Outputs: recursion_pareto.pdf  +  recursion_pareto.png

Layout:  one figure, 6 subplots (2 × 3), one subplot per dataset.
Each subplot:
    x = size (% of original dataset size)   — smaller = better (← left)
    y = query latency (ns/query)            — smaller = better (↓ bottom)
    3 polyline curves, one per trie variant.
    3 points per curve, labelled ρ=0 / ρ=1 / ρ=2.

The Pareto curve per trie is read from lower-left → upper-right (smaller
recursion = faster but larger; larger recursion = smaller but slower).
Points stacking on top of each other (e.g. C²-Marisa on dna) reveal cases
where the adaptive cost-based recursion check in strpool.hpp pruned the
extra layer entirely.
"""
import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt

# ---------------------------------------------------------------------------
# Paper-style matplotlib settings
# ---------------------------------------------------------------------------
plt.rcParams.update({
    "font.family":      "serif",
    "font.size":        10,
    "axes.labelsize":   10,
    "axes.titlesize":   11,
    "legend.fontsize":  9,
    "xtick.labelsize":  9,
    "ytick.labelsize":  9,
    "pdf.fonttype":     42,   # keep text editable in vector editors
    "ps.fonttype":      42,
})

# ---------------------------------------------------------------------------
# Constants — must match run_recursion_ablation.sh (orig_mb, dataset order)
# ---------------------------------------------------------------------------
ORIG_MB = {
    "words": 4.601, "url":  36.72, "dna":  101.10,
    "xml":   117.27, "wiki": 359.4, "log":  585.0,
}
DATASETS = ["words", "url", "dna", "xml", "wiki", "log"]

# (display name, CSV prefix, colour)
TRIES = [
    ("$C^2$-FST",    "C2-FST",    "#1f77b4"),
    ("$C^2$-CoCo",   "C2-CoCo",   "#d62728"),
    ("$C^2$-Marisa", "C2-Marisa", "#2ca02c"),
]
DEPTHS = [0, 1, 2]

# Marker shape encodes recursion depth ρ — no text labels needed in the figure.
DEPTH_MARKERS = {0: "o", 1: "s", 2: "^"}
DEPTH_SIZES   = {0: 55,  1: 50,  2: 65}    # triangle reads small at parity

HERE = Path(__file__).resolve().parent


def find_latest_csv() -> Path:
    candidates = sorted((HERE / "results").glob("recursion_ablation_*.csv"))
    if not candidates:
        sys.exit("ERROR: no results/recursion_ablation_*.csv found")
    return candidates[-1]


def load(csv_path: Path):
    """rows[csv_prefix][dataset][depth] = (size_pct, latency_ns)."""
    rows = {prefix: {ds: {} for ds in DATASETS}
            for _, prefix, _ in TRIES}
    with open(csv_path) as f:
        for r in csv.DictReader(f):
            label, ds = r["trie"], r["dataset"]
            if ds not in DATASETS:
                continue
            for _, prefix, _ in TRIES:
                if label.startswith(prefix + "-"):
                    try:
                        depth = int(label[len(prefix) + 1:])
                    except ValueError:
                        break
                    if depth not in DEPTHS:
                        break
                    size_pct = float(r["size_mb"]) / ORIG_MB[ds] * 100
                    latency  = float(r["latency_ns"])
                    rows[prefix][ds][depth] = (size_pct, latency)
                    break
    return rows


def add_depth_marker_legend(ax, loc="lower left"):
    """Second legend mapping ρ value to marker shape (color-agnostic)."""
    from matplotlib.lines import Line2D
    handles = [
        Line2D([0], [0], marker=DEPTH_MARKERS[d], color="0.25",
               linestyle="", markersize=(DEPTH_SIZES[d] ** 0.5) * 1.1,
               markerfacecolor="0.25",
               label=rf"$\rho{{=}}{d}$")
        for d in DEPTHS
    ]
    leg = ax.legend(handles=handles, loc=loc, frameon=True,
                    fontsize=8, handletextpad=0.4, borderpad=0.5,
                    framealpha=0.85, edgecolor="0.7")
    return leg


def cluster_overlapping(xs, ys, depths, x_thresh, y_thresh):
    """Group near-identical points so we can merge their ρ-labels.

    Returns list of (representative_index, [depths_in_cluster]) ordered by
    increasing depth of the representative — so we can place labels using
    the natural curve direction (ρ=0 → ρ=2).
    """
    used = [False] * len(depths)
    clusters = []
    for i in range(len(depths)):
        if used[i]:
            continue
        group = [i]
        used[i] = True
        for j in range(i + 1, len(depths)):
            if used[j]:
                continue
            if abs(xs[i] - xs[j]) <= x_thresh and abs(ys[i] - ys[j]) <= y_thresh:
                group.append(j)
                used[j] = True
        clusters.append((i, [depths[k] for k in group]))
    return clusters


def label_offset(member_depths, all_depths):
    """Pick a tidy (dx, dy) offset based on where the (cluster of) point(s)
    sits along the natural ρ=0 → ρ=2 Pareto direction.

    - ρ=0 (lower-right end of curve):  label to the upper-right
    - ρ=2 (upper-left end of curve):   label to the lower-left
    - middle ρ=1:                      label above
    - merged clusters covering both ends: label slightly above-right
    """
    has_lo = min(all_depths) in member_depths
    has_hi = max(all_depths) in member_depths
    if has_lo and has_hi:
        return (7, 5)
    if has_lo:
        return (7, 4)
    if has_hi:
        return (-8, -10)
    return (0, 8)   # middle ρ=1


def plot(rows, out_paths):
    fig, axes = plt.subplots(2, 3, figsize=(10.5, 6.0))
    for idx, ds in enumerate(DATASETS):
        ax = axes[idx // 3, idx % 3]

        for pretty, prefix, color in TRIES:
            pts = [rows[prefix][ds].get(d) for d in DEPTHS]
            if any(p is None for p in pts):
                continue
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            # Line without markers
            ax.plot(xs, ys, color=color, linewidth=1.6, label=pretty, zorder=3)
            # Per-ρ markers (shape encodes recursion depth)
            for d, x, y in zip(DEPTHS, xs, ys):
                ax.scatter(x, y, marker=DEPTH_MARKERS[d],
                           s=DEPTH_SIZES[d], color=color,
                           edgecolors="white", linewidths=0.6, zorder=4)

        ax.set_title(ds, fontweight="bold")
        ax.set_xlabel("Size (% of original)")
        ax.set_ylabel("Latency (ns / query)")
        ax.grid(True, linestyle=":", linewidth=0.5, alpha=0.6)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

        # Pad axes a bit so end markers don't get clipped by the spine
        xlo, xhi = ax.get_xlim()
        ylo, yhi = ax.get_ylim()
        ax.set_xlim(xlo - 0.04 * (xhi - xlo), xhi + 0.04 * (xhi - xlo))
        ax.set_ylim(ylo - 0.04 * (yhi - ylo), yhi + 0.06 * (yhi - ylo))

    # Combined legend: trie colors (left) + ρ marker shapes (right)
    from matplotlib.lines import Line2D
    trie_handles = [
        Line2D([0], [0], color=c, linewidth=2.0, label=p)
        for p, _, c in TRIES
    ]
    depth_handles = [
        Line2D([0], [0], marker=DEPTH_MARKERS[d], color="0.25",
               linestyle="", markersize=7, markerfacecolor="0.25",
               label=rf"$\rho{{=}}{d}$")
        for d in DEPTHS
    ]
    fig.legend(handles=trie_handles + depth_handles,
               loc="upper center", ncol=len(TRIES) + len(DEPTHS),
               bbox_to_anchor=(0.5, 1.02), frameon=False,
               columnspacing=1.6, handletextpad=0.5)
    fig.tight_layout(rect=[0, 0, 1, 0.96])

    for out in out_paths:
        fig.savefig(out, bbox_inches="tight")
        print(f"saved: {out}")


DATASET_COLORS = {
    "words": "#1f77b4",   # blue
    "url":   "#ff7f0e",   # orange
    "dna":   "#2ca02c",   # green
    "xml":   "#d62728",   # red
    "wiki":  "#9467bd",   # purple
    "log":   "#8c564b",   # brown
}


def plot_per_trie(rows, pretty, prefix, out_paths):
    """One figure per trie: a single panel showing the Pareto curve for
    every dataset (6 coloured polylines). Recursion depth is encoded by
    marker shape (circle = ρ=0, square = ρ=1, triangle = ρ=2). Dataset
    is labelled in-place next to the ρ=0 end of its curve."""
    fig, ax = plt.subplots(figsize=(6.0, 4.6))

    for ds in DATASETS:
        pts = [rows[prefix][ds].get(d) for d in DEPTHS]
        if any(p is None for p in pts):
            continue
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        color = DATASET_COLORS[ds]
        ax.plot(xs, ys, color=color, linewidth=1.5, zorder=3)
        for d, x, y in zip(DEPTHS, xs, ys):
            ax.scatter(x, y, marker=DEPTH_MARKERS[d],
                       s=DEPTH_SIZES[d], color=color,
                       edgecolors="white", linewidths=0.6, zorder=4)

        # Bold dataset label at the ρ=0 end of the curve (lower-right).
        ax.annotate(ds, (xs[0], ys[0]), textcoords="offset points",
                    xytext=(9, -2), fontsize=8.5, color=color,
                    ha="left", va="center", fontweight="bold", zorder=5)

    ax.set_title(f"{pretty}: recursion Pareto across datasets",
                 fontweight="bold")
    ax.set_xlabel("Size (% of original)")
    ax.set_ylabel("Latency (ns / query)")
    ax.grid(True, linestyle=":", linewidth=0.5, alpha=0.6)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    xlo, xhi = ax.get_xlim()
    ylo, yhi = ax.get_ylim()
    ax.set_xlim(xlo - 0.04 * (xhi - xlo), xhi + 0.16 * (xhi - xlo))
    ax.set_ylim(ylo - 0.06 * (yhi - ylo), yhi + 0.08 * (yhi - ylo))

    add_depth_marker_legend(ax, loc="lower left")

    fig.tight_layout()
    for out in out_paths:
        fig.savefig(out, bbox_inches="tight")
        print(f"saved: {out}")
    plt.close(fig)


def plot_three_panel(rows, out_paths):
    """One wide figure with 3 horizontal panels (FST | CoCo | Marisa), sized
    for a double-column paper's \\textwidth (= the full row spanning both
    columns when using \\begin{figure*}). Font sizes are absolute and match
    final paper output — no LaTeX scaling needed if embedded at
    `width=\\textwidth`.

    Layout: 1 × 3 subplots in a 7 × 3.3 inch figure. Top legend = recursion
    depth ρ via marker shape; bottom legend = dataset via colour. No inline
    labels in the plot area, so font sizes can be enlarged without crowding.
    """
    # Sized for a wide \figure* across both columns. The matplotlib output is
    # rendered at 10.5 × 3.0 in; LaTeX shrinks it to \textwidth ≈ 7 in
    # (factor ≈ 0.67), so the matplotlib font sizes are pre-multiplied by
    # ~1/0.67 to land at the paper-effective sizes noted in the comment.
    FS_TITLE   = 16   # → ~10.5 pt in paper
    FS_LABEL   = 15   # → ~10 pt
    FS_TICK    = 13   # → ~8.5 pt
    FS_LEGEND  = 14   # → ~9.3 pt

    fig, axes = plt.subplots(1, 3, figsize=(10.5, 3.0))

    for ax, (pretty, prefix, _) in zip(axes, TRIES):
        for ds in DATASETS:
            pts = [rows[prefix][ds].get(d) for d in DEPTHS]
            if any(p is None for p in pts):
                continue
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            color = DATASET_COLORS[ds]
            ax.plot(xs, ys, color=color, linewidth=1.6, zorder=3)
            for d, x, y in zip(DEPTHS, xs, ys):
                ax.scatter(x, y, marker=DEPTH_MARKERS[d],
                           s=DEPTH_SIZES[d] * 1.0, color=color,
                           edgecolors="white", linewidths=0.6, zorder=4)

        ax.set_title(pretty, fontsize=FS_TITLE, fontweight="bold", pad=4)
        ax.set_xlabel("Size (% of original)", fontsize=FS_LABEL, labelpad=2)
        ax.tick_params(axis="both", labelsize=FS_TICK)
        ax.grid(True, linestyle=":", linewidth=0.4, alpha=0.6)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

        xlo, xhi = ax.get_xlim()
        ylo, yhi = ax.get_ylim()
        ax.set_xlim(xlo - 0.04 * (xhi - xlo), xhi + 0.06 * (xhi - xlo))
        ax.set_ylim(ylo - 0.06 * (yhi - ylo), yhi + 0.10 * (yhi - ylo))

    axes[0].set_ylabel("Latency (ns / query)", fontsize=FS_LABEL, labelpad=2)

    from matplotlib.lines import Line2D

    # Top legend: recursion depth ρ encoded by marker shape.
    depth_handles = [
        Line2D([0], [0], marker=DEPTH_MARKERS[d], color="0.25",
               linestyle="", markersize=9, markerfacecolor="0.25",
               label=rf"$\rho{{=}}{d}$")
        for d in DEPTHS
    ]
    leg_top = fig.legend(handles=depth_handles, loc="upper center", ncol=3,
                         bbox_to_anchor=(0.5, 1.05), frameon=False,
                         fontsize=FS_LEGEND, columnspacing=2.4,
                         handletextpad=0.4)
    fig.add_artist(leg_top)

    # Bottom legend: dataset encoded by colour.
    dataset_handles = [
        Line2D([0], [0], color=DATASET_COLORS[ds], linewidth=2.6,
               marker="o", markersize=7, markeredgecolor="white",
               markeredgewidth=0.5, label=ds)
        for ds in DATASETS
    ]
    fig.legend(handles=dataset_handles, loc="lower center",
               ncol=len(DATASETS), bbox_to_anchor=(0.5, -0.05),
               frameon=False, fontsize=FS_LEGEND,
               columnspacing=1.4, handletextpad=0.4)

    fig.tight_layout(rect=[0, 0.07, 1, 0.92])
    fig.subplots_adjust(wspace=0.28)

    for out in out_paths:
        fig.savefig(out, bbox_inches="tight")
        print(f"saved: {out}")
    plt.close(fig)


if __name__ == "__main__":
    csv_path = find_latest_csv()
    print(f"using: {csv_path.relative_to(HERE)}")
    rows = load(csv_path)

    # Layout A: per-dataset (6 subplots, 3 tries each)
    plot(rows, [HERE / "recursion_pareto.pdf",
                HERE / "recursion_pareto.png"])

    # Layout B: per-trie (3 figures, 6 datasets each)
    for pretty, prefix, _ in TRIES:
        tag = prefix.replace("C2-", "").lower()
        plot_per_trie(rows, pretty, prefix,
                      [HERE / f"recursion_pareto_{tag}.pdf",
                       HERE / f"recursion_pareto_{tag}.png"])

    # Layout C: triple-panel row for double-column papers (\textwidth wide)
    plot_three_panel(rows, [HERE / "recursion_pareto_row.pdf",
                            HERE / "recursion_pareto_row.png"])
