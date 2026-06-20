#!/usr/bin/env bash
# =============================================================================
# run_recursion_ablation.sh
#
# Ablation study: effect of recursive compression depth (0 / 1 / 2) on
# C²-FST, C²-CoCo, and C²-Marisa across all 6 datasets.
#
# Configs tested:
#   C2-FST-{0,1,2}   case=0, mask=0, max_rec={0,1,2}
#   C2-CoCo-{0,1,2}  case=1, mask=0, max_rec={0,1,2}
#   C2-Marisa-{0,1,2} case=2, mask=0, max_rec={0,1,2}
#
# Output: results/recursion_ablation_<TIMESTAMP>.csv
#         columns: trie,dataset,build_ms,size_mb,latency_ns
#
# Usage:
#   ./run_recursion_ablation.sh               # run all
#   ./run_recursion_ablation.sh -d words      # single dataset
#   ./run_recursion_ablation.sh -c C2-FST-1   # single config
#   ./run_recursion_ablation.sh -n            # dry run
#   ./run_recursion_ablation.sh -h            # help
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/build/benchmark"
FULL_DATASET_DIR="${SCRIPT_DIR}/full_dataset"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULTS_FILE="${RESULTS_DIR}/recursion_ablation_${TIMESTAMP}.csv"

# ---------------------------------------------------------------------------
# Datasets
# ---------------------------------------------------------------------------
declare -A DATASETS=(
    [words]="${FULL_DATASET_DIR}/words/words-470k.txt"
    [url]="${FULL_DATASET_DIR}/url/uk-2014-tpd_sorted.ids"
    [dna]="${FULL_DATASET_DIR}/dna/dna100_31mers_sorted.txt"
    [xml]="${FULL_DATASET_DIR}/xml/dblp.xml.200MB_sorted"
    [wiki]="${FULL_DATASET_DIR}/wiki/enwiki-ns0-ascii.txt"
    [log]="${FULL_DATASET_DIR}/log/access_log_common_sorted.txt"
)
DATASET_ORDER=(words url dna xml wiki log)

# ---------------------------------------------------------------------------
# Configs: label|case_id|space_relax|max_rec|mask
# ---------------------------------------------------------------------------
CONFIGS=(
    "C2-FST-0|0|0|0|0"
    "C2-FST-1|0|0|1|0"
    "C2-FST-2|0|0|2|0"
    "C2-CoCo-0|1|0|0|0"
    "C2-CoCo-1|1|0|1|0"
    "C2-CoCo-2|1|0|2|0"
    "C2-Marisa-0|2|0|0|0"
    "C2-Marisa-1|2|0|1|0"
    "C2-Marisa-2|2|0|2|0"
)

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
SELECTED_DATASETS=()
SELECTED_CONFIGS=()
DRY_RUN=false

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "  -d <dataset>   Run only this dataset (repeatable)"
    echo "                 Available: ${DATASET_ORDER[*]}"
    echo "  -c <label>     Run only this config (repeatable)"
    echo "                 Available: C2-FST-{0,1,2} C2-CoCo-{0,1,2} C2-Marisa-{0,1,2}"
    echo "  -n             Dry run"
    echo "  -h             Help"
    exit 0
}

while getopts "d:c:nh" opt; do
    case $opt in
        d) SELECTED_DATASETS+=("$OPTARG") ;;
        c) SELECTED_CONFIGS+=("$OPTARG") ;;
        n) DRY_RUN=true ;;
        h) usage ;;
        *) usage ;;
    esac
done

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------
if [[ ! -f "${BINARY}" ]]; then
    echo "ERROR: benchmark binary not found at ${BINARY}"
    echo "       Run: cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j"
    exit 1
fi

mkdir -p "${RESULTS_DIR}"

# ---------------------------------------------------------------------------
# run_one: 1 warmup + 3 timed runs, average
# ---------------------------------------------------------------------------
run_one() {
    local label="$1" dataset_name="$2" dataset_path="$3"
    local case_id="$4" space_relax="$5" max_rec="$6" mask="$7"

    local cmd="${BINARY} ${dataset_path} ${case_id} ${space_relax} ${max_rec} ${mask}"

    if $DRY_RUN; then
        echo "[DRY RUN] ${label} / ${dataset_name}: ${cmd}"
        return
    fi

    echo ""
    echo ">>> ${label} on ${dataset_name}"
    echo "    cmd: ${cmd}"

    local runs=() run_i csv_line output
    for run_i in 1 2 3 4; do
        output="$($cmd 2>&1)" || {
            echo "    [FAILED on run ${run_i}]"
            echo "${label},${dataset_name},ERROR,ERROR,ERROR" >> "${RESULTS_FILE}"
            return
        }
        csv_line="$(echo "${output}" | grep -E '^[0-9]+\.[0-9]+,[0-9]+\.[0-9]+,[0-9]+\.[0-9]+$' | tail -1)"
        if [[ -z "${csv_line}" ]]; then
            echo "    [WARNING: could not parse CSV on run ${run_i}]"
            echo "${label},${dataset_name},PARSE_ERROR,PARSE_ERROR,PARSE_ERROR" >> "${RESULTS_FILE}"
            return
        fi
        if [[ ${run_i} -gt 1 ]]; then
            runs+=("${csv_line}")
            echo "    run${run_i}: ${csv_line}"
        else
            echo "    run1 (warmup, discarded): ${csv_line}"
        fi
    done

    local avg
    avg="$(python3 - "${runs[@]}" << 'PYEOF'
import sys
rows = [list(map(float, r.split(','))) for r in sys.argv[1:]]
n = len(rows)
avgs = [sum(col) / n for col in zip(*rows)]
print(','.join(f'{v:.6f}' for v in avgs))
PYEOF
)"
    if [[ -z "${avg}" ]]; then
        echo "    [WARNING: averaging failed]"
        echo "${label},${dataset_name},AVG_ERROR,AVG_ERROR,AVG_ERROR" >> "${RESULTS_FILE}"
    else
        local build_ms size_mb latency_ns
        IFS=',' read -r build_ms size_mb latency_ns <<< "${avg}"
        echo "    avg(runs 2-4): build=${build_ms} ms  size=${size_mb} MB  latency=${latency_ns} ns"
        echo "${label},${dataset_name},${build_ms},${size_mb},${latency_ns}" >> "${RESULTS_FILE}"
    fi
}

# ---------------------------------------------------------------------------
# Filter helpers
# ---------------------------------------------------------------------------
should_run_dataset() {
    [[ ${#SELECTED_DATASETS[@]} -eq 0 ]] && return 0
    for d in "${SELECTED_DATASETS[@]}"; do [[ "$d" == "$1" ]] && return 0; done
    return 1
}

should_run_config() {
    [[ ${#SELECTED_CONFIGS[@]} -eq 0 ]] && return 0
    for c in "${SELECTED_CONFIGS[@]}"; do [[ "$c" == "$1" ]] && return 0; done
    return 1
}

# ---------------------------------------------------------------------------
# Header
# ---------------------------------------------------------------------------
if ! $DRY_RUN; then
    echo "trie,dataset,build_ms,size_mb,latency_ns" > "${RESULTS_FILE}"
    echo "Results will be written to: ${RESULTS_FILE}"
fi

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
for config_str in "${CONFIGS[@]}"; do
    IFS='|' read -r label case_id space_relax max_rec mask <<< "${config_str}"
    should_run_config "${label}" || continue

    for ds_name in "${DATASET_ORDER[@]}"; do
        should_run_dataset "${ds_name}" || continue
        ds_path="${DATASETS[$ds_name]}"
        if [[ -z "${ds_path}" || ! -f "${ds_path}" ]]; then
            echo ">>> ${label} on ${ds_name}: SKIPPED (file not found)"
            continue
        fi
        run_one "${label}" "${ds_name}" "${ds_path}" \
                "${case_id}" "${space_relax}" "${max_rec}" "${mask}"
    done
done

# ---------------------------------------------------------------------------
# Summary table
# ---------------------------------------------------------------------------
if ! $DRY_RUN && [[ -f "${RESULTS_FILE}" ]]; then
    python3 - "${RESULTS_FILE}" << 'PYEOF'
import sys, csv

orig_mb = {'words':4.601,'url':36.72,'dna':101.10,'xml':117.27,'wiki':359.4,'log':585.0}
nkeys   = {'words':467400,'url':1770000,'dna':3319000,'xml':2149000,'wiki':17474000,'log':4449000}
datasets = ['words','url','dna','xml','wiki','log']

rows = {}
with open(sys.argv[1]) as f:
    for r in csv.DictReader(f):
        rows[(r['trie'], r['dataset'])] = r

tries = [
    'C2-FST-0','C2-FST-1','C2-FST-2',
    'C2-CoCo-0','C2-CoCo-1','C2-CoCo-2',
    'C2-Marisa-0','C2-Marisa-1','C2-Marisa-2',
]

def val(t, d, field):
    r = rows.get((t, d))
    if not r or r[field] in ('ERROR','PARSE_ERROR','AVG_ERROR'): return None
    return float(r[field])

def fmt(v, fmt_str): return fmt_str % v if v is not None else '—'

print("\n=== QUERY LATENCY (ns/key) ===")
print(f"{'Trie':<16}" + "".join(f"{d:>8}" for d in datasets))
print("-" * (16 + 8*6))
prev_base = {}
for t in tries:
    base = t[:-2] + '-0'
    row = f"{t:<16}"
    for d in datasets:
        v = val(t, d, 'latency_ns')
        b = val(base, d, 'latency_ns')
        if v is None: row += f"{'—':>8}"; continue
        delta = f"({v/b*100-100:+.0f}%)" if b and t != base else ""
        row += f"{v:>7.0f} "
    print(row)

print("\n=== SIZE (% of original) ===")
print(f"{'Trie':<16}" + "".join(f"{d:>8}" for d in datasets))
print("-" * (16 + 8*6))
for t in tries:
    row = f"{t:<16}"
    for d in datasets:
        v = val(t, d, 'size_mb')
        if v is None: row += f"{'—':>8}"; continue
        row += f"{v/orig_mb[d]*100:>7.1f}%"
    print(row)

print("\n=== BUILD TIME (ns/key) ===")
print(f"{'Trie':<16}" + "".join(f"{d:>8}" for d in datasets))
print("-" * (16 + 8*6))
for t in tries:
    row = f"{t:<16}"
    for d in datasets:
        v = val(t, d, 'build_ms')
        n = nkeys[d]
        if v is None: row += f"{'—':>8}"; continue
        row += f"{v*1e6/n:>7.0f} "
    print(row)

print(f"\nResults: {sys.argv[1]}")
PYEOF
    echo ""
    echo "Done. Results saved to: ${RESULTS_FILE}"
fi
