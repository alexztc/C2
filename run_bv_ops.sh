#!/usr/bin/env bash
# Reproduce the bitvector operation microbenchmark (cases 11, 12, 13).
# Compares C1 co-located bitvector against the baseline (separate bitvectors)
# at the individual operator level: GET, LEAF_ID, INTERNAL_ID, DEGREE, CHILD_POS,
# LINK_ID, PARENT_POS.
#
# Usage:
#   ./run_bv_ops.sh [-d <dataset_path>] [-r <runs>] [-n] [-h]
#
#   -d <path>   Dataset file (default: full_dataset/xml/dblp.xml.200MB_sorted)
#   -r <N>      Number of timed runs to average (default: 3; a warmup run is always added)
#   -n          Dry run — print commands, don't execute
#   -h          Show this help
#
# Examples:
#   ./run_bv_ops.sh                          # XML dataset, 3 timed runs
#   ./run_bv_ops.sh -d /path/to/dataset.txt  # different dataset
#   ./run_bv_ops.sh -r 5                     # 5 timed runs
#   ./run_bv_ops.sh -n                       # dry run

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/build/benchmark"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULTS_FILE="${RESULTS_DIR}/bv_ops_${TIMESTAMP}.csv"

# Defaults
DATASET="${SCRIPT_DIR}/full_dataset/xml/dblp.xml.200MB_sorted"
TIMED_RUNS=3
DRY_RUN=false

usage() {
    echo "Usage: $0 [-d <dataset_path>] [-r <runs>] [-n] [-h]"
    echo ""
    echo "  -d <path>   Dataset file (default: full_dataset/xml/dblp.xml.200MB_sorted)"
    echo "  -r <N>      Number of timed runs to average (default: 3)"
    echo "  -n          Dry run: print commands without executing"
    echo "  -h          Show this help"
    exit 0
}

while getopts "d:r:nh" opt; do
    case $opt in
        d) DATASET="$OPTARG" ;;
        r) TIMED_RUNS="$OPTARG" ;;
        n) DRY_RUN=true ;;
        h) usage ;;
        *) usage ;;
    esac
done

# ---------------------------------------------------------------------------
# Build if needed
# ---------------------------------------------------------------------------
if [[ ! -f "${BINARY}" ]]; then
    echo ">>> benchmark binary not found; building..."
    if $DRY_RUN; then
        echo "[DRY RUN] cmake --build \"${SCRIPT_DIR}/build\" --target benchmark -j"
    else
        cmake --build "${SCRIPT_DIR}/build" --target benchmark -j
    fi
fi

if ! $DRY_RUN && [[ ! -f "${BINARY}" ]]; then
    echo "ERROR: build failed — ${BINARY} not found"
    exit 1
fi

if ! $DRY_RUN && [[ ! -f "${DATASET}" ]]; then
    echo "ERROR: dataset not found: ${DATASET}"
    exit 1
fi

mkdir -p "${RESULTS_DIR}"

# ---------------------------------------------------------------------------
# run_case <case_id> <extra_args...>
# Executes the binary (1 warmup + TIMED_RUNS timed runs), captures output
# from each timed run, and stores stdout lines in the global array RUN_OUTPUTS.
# ---------------------------------------------------------------------------
RUN_OUTPUTS=()

run_case() {
    local case_id="$1"; shift
    local cmd="${BINARY} ${DATASET} ${case_id} $*"

    if $DRY_RUN; then
        echo "[DRY RUN] ${cmd}"
        return
    fi

    echo ""
    echo ">>> Running case ${case_id}: ${cmd}"

    RUN_OUTPUTS=()
    local total=$(( TIMED_RUNS + 1 ))  # +1 for warmup
    for ((i=1; i<=total; i++)); do
        local out
        out="$($cmd 2>/dev/null)" || {
            echo "    [FAILED on run ${i}]"
            return 1
        }
        if [[ $i -eq 1 ]]; then
            echo "    run1: warmup (discarded)"
        else
            echo "    run${i}: recorded"
            RUN_OUTPUTS+=("$out")
        fi
    done
}

# ---------------------------------------------------------------------------
# parse_op <op_pattern> <num_values>
# Grep one operator line from RUN_OUTPUTS and average the values.
# op_pattern: regex matching the line, e.g. "^GET(ns):"
# num_values: 2 or 3 (number of "X vs Y [vs Z]" fields)
# Sets global PARSED_VALUES array with averaged floats.
# ---------------------------------------------------------------------------
PARSED_VALUES=()

parse_op() {
    local pattern="$1"
    local num_values="$2"

    # Collect matching lines from all timed runs
    local all_lines=()
    for out in "${RUN_OUTPUTS[@]}"; do
        local line
        line="$(echo "$out" | grep -E "$pattern" | head -1)" || true
        if [[ -n "$line" ]]; then
            all_lines+=("$line")
        fi
    done

    if [[ ${#all_lines[@]} -eq 0 ]]; then
        echo "    [WARNING: pattern '$pattern' not found in output]"
        PARSED_VALUES=()
        return
    fi

    # Average across runs using Python
    PARSED_VALUES=()
    local averaged
    averaged="$(python3 - "${num_values}" "${all_lines[@]}" <<'PYEOF'
import sys, re
n_vals = int(sys.argv[1])
lines = sys.argv[2:]
# extract all floats from each line
sums = [0.0] * n_vals
for line in lines:
    nums = list(map(float, re.findall(r'[0-9]+\.[0-9]+', line)))
    for i in range(n_vals):
        sums[i] += nums[i]
avgs = [s / len(lines) for s in sums]
print(' '.join(f'{v:.4f}' for v in avgs))
PYEOF
)"
    read -r -a PARSED_VALUES <<< "$averaged"
}

# ---------------------------------------------------------------------------
# speedup c1 baseline → prints "X.XXx"
# ---------------------------------------------------------------------------
speedup() {
    python3 -c "print(f'{float(\"$2\")/float(\"$1\"):.2f}x')"
}

# ---------------------------------------------------------------------------
# Run all three cases
# ---------------------------------------------------------------------------

echo "========================================"
echo " Bitvector Operation Microbenchmark"
echo " Dataset: $(basename "${DATASET}")"
echo " Timed runs per case: ${TIMED_RUNS}"
echo "========================================"

if ! $DRY_RUN; then
    echo "trie,operator,c1_ns,baseline_ns,speedup" > "${RESULTS_FILE}"
fi

# ---- Case 11: CoCo LOUDS (LoudsCC vs LoudsSux vs LoudsSparseCC) ----
# Output format: "OP(ns): c1 vs sdsl vs sparse"  (3 values; we use c1 and sdsl)
run_case 11 0 0 0

if ! $DRY_RUN; then
    echo ""
    echo "--- CoCo LOUDS (case 11) ---"
    printf "%-14s  %10s  %10s  %8s\n" "Operator" "C1 (ns)" "Baseline (ns)" "Speedup"
    printf "%-14s  %10s  %10s  %8s\n" "---------" "-------" "-------------" "-------"

    for op_pat in "^GET(ns)" "^LEAF_ID(ns)" "^INTERNAL_ID(ns)" "^DEGREE(ns)" "^CHILD_POS(ns)"; do
        parse_op "$op_pat" 3
        if [[ ${#PARSED_VALUES[@]} -ge 2 ]]; then
            c1="${PARSED_VALUES[0]}"
            base="${PARSED_VALUES[1]}"
            sp="$(speedup "$c1" "$base")"
            op_name="${op_pat#^}"
            op_name="${op_name%(ns)}"
            printf "%-14s  %10s  %13s  %8s\n" "${op_name}" "${c1}" "${base}" "${sp}"
            echo "CoCo,${op_name},${c1},${base},${sp}" >> "${RESULTS_FILE}"
        fi
    done
fi

# ---- Case 12: Marisa LOUDS (LoudsSparseCC vs baseline) ----
# Output format:
#   "GET(ns): c1 vs baseline"
#   "LINK_ID(ns): c1 vs baseline, LEAF_ID(ns): c1 vs baseline"
#   "CHILD_POS(ns): c1 vs baseline, PARENT_POS(ns): c1 vs baseline"
run_case 12 0 0

if ! $DRY_RUN; then
    echo ""
    echo "--- Marisa LOUDS (case 12) ---"
    printf "%-14s  %10s  %10s  %8s\n" "Operator" "C1 (ns)" "Baseline (ns)" "Speedup"
    printf "%-14s  %10s  %10s  %8s\n" "---------" "-------" "-------------" "-------"

    # GET: standalone line, 2 values
    parse_op "^GET(ns):" 2
    if [[ ${#PARSED_VALUES[@]} -ge 2 ]]; then
        c1="${PARSED_VALUES[0]}"; base="${PARSED_VALUES[1]}"
        sp="$(speedup "$c1" "$base")"
        printf "%-14s  %10s  %13s  %8s\n" "GET" "${c1}" "${base}" "${sp}"
        echo "Marisa,GET,${c1},${base},${sp}" >> "${RESULTS_FILE}"
    fi

    # LINK_ID and LEAF_ID share one line, 4 values: link_c1 link_base leaf_c1 leaf_base
    parse_op "^LINK_ID" 4
    if [[ ${#PARSED_VALUES[@]} -ge 4 ]]; then
        for idx_op in "0:1:LINK_ID" "2:3:LEAF_ID"; do
            i="${idx_op%%:*}"; rest="${idx_op#*:}"; j="${rest%%:*}"; op="${rest#*:}"
            c1="${PARSED_VALUES[$i]}"; base="${PARSED_VALUES[$j]}"
            sp="$(speedup "$c1" "$base")"
            printf "%-14s  %10s  %13s  %8s\n" "${op}" "${c1}" "${base}" "${sp}"
            echo "Marisa,${op},${c1},${base},${sp}" >> "${RESULTS_FILE}"
        done
    fi

    # CHILD_POS and PARENT_POS share one line, 4 values
    parse_op "^CHILD_POS" 4
    if [[ ${#PARSED_VALUES[@]} -ge 4 ]]; then
        for idx_op in "0:1:CHILD_POS" "2:3:PARENT_POS"; do
            i="${idx_op%%:*}"; rest="${idx_op#*:}"; j="${rest%%:*}"; op="${rest#*:}"
            c1="${PARSED_VALUES[$i]}"; base="${PARSED_VALUES[$j]}"
            sp="$(speedup "$c1" "$base")"
            printf "%-14s  %10s  %13s  %8s\n" "${op}" "${c1}" "${base}" "${sp}"
            echo "Marisa,${op},${c1},${base},${sp}" >> "${RESULTS_FILE}"
        done
    fi
fi

# ---- Case 13: FST LOUDS-Sparse (C2-FST vs SuRF baseline) ----
# Output format: "OP(ns): c1 vs baseline"  (2 values each)
run_case 13

if ! $DRY_RUN; then
    echo ""
    echo "--- FST LOUDS-Sparse (case 13) ---"
    printf "%-14s  %10s  %10s  %8s\n" "Operator" "C1 (ns)" "Baseline (ns)" "Speedup"
    printf "%-14s  %10s  %10s  %8s\n" "---------" "-------" "-------------" "-------"

    for op_pat in "^GET(ns)" "^LEAF_ID(ns)" "^DEGREE(ns)" "^CHILD_POS(ns)"; do
        parse_op "$op_pat" 2
        if [[ ${#PARSED_VALUES[@]} -ge 2 ]]; then
            c1="${PARSED_VALUES[0]}"
            base="${PARSED_VALUES[1]}"
            sp="$(speedup "$c1" "$base")"
            op_name="${op_pat#^}"
            op_name="${op_name%(ns)}"
            printf "%-14s  %10s  %13s  %8s\n" "${op_name}" "${c1}" "${base}" "${sp}"
            echo "FST,${op_name},${c1},${base},${sp}" >> "${RESULTS_FILE}"
        fi
    done
fi

if ! $DRY_RUN; then
    echo ""
    echo "Results saved to: ${RESULTS_FILE}"
fi
