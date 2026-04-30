#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/build/benchmark"
DATASET_DIR="${SCRIPT_DIR}/example_dataset"
FULL_DATASET_DIR="${SCRIPT_DIR}/full_dataset"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULTS_FILE="${RESULTS_DIR}/benchmark_${TIMESTAMP}.csv"

# ---------------------------------------------------------------------------
# Dataset paths (set to empty string "" to skip)
# ---------------------------------------------------------------------------
declare -A DATASETS=(
    [words]="${FULL_DATASET_DIR}/words/words-470k.txt"
    [url]="${FULL_DATASET_DIR}/url/uk-2014-tpd_sorted.ids"
    [dna]="${FULL_DATASET_DIR}/dna/dna100_31mers_sorted.txt"
    [xml]="${FULL_DATASET_DIR}/xml/dblp.xml.200MB_sorted"
    [wiki]="${FULL_DATASET_DIR}/wiki/enwiki-ns0-ascii.txt"
    [log]="${FULL_DATASET_DIR}/log/access_log_common_sorted.txt"
)

# Column order matching the paper table
DATASET_ORDER=(words url dna xml wiki log)

# ---------------------------------------------------------------------------
# Test configurations: each entry is "label|case|space_relax|max_rec|mask"
# Order matches the paper table rows
# ---------------------------------------------------------------------------
CONFIGS=(
    "FST|3|0|0|0"
    "BV-FST|0|0|0|1"
    "C2-FST|0|0|0|0"
    "CoCo-prime|10|0|0|0"
    "BV-CoCo|1|0|0|1"
    "C2-CoCo|1|0|0|0"
    "Marisa|5|0|0|0"
    "BV-Marisa|2|0|0|1"
    "C2-Marisa|2|0|0|0"
    "Marisa-1|5|0|1|0"
    "BV-Marisa-1|2|0|1|1"
    "C2-Marisa-1|2|0|1|0"
    "PDT|6|0|0|0"
    "ART|7|0|0|0"
    "C-ART|8|0|0|0"
)

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
SELECTED_DATASETS=()
SELECTED_CONFIGS=()
DRY_RUN=false

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -d <dataset>   Run only this dataset (can be repeated)"
    echo "                 Available: ${DATASET_ORDER[*]}"
    echo "  -c <label>     Run only this config label (can be repeated)"
    echo "                 Available: $(IFS=, ; echo "${CONFIGS[*]}" | sed 's/|[^,]*/,/g' | tr -d ',')"
    echo "  -n             Dry run: print commands without executing"
    echo "  -h             Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                          # run everything"
    echo "  $0 -d words -d dna          # only words and dna datasets"
    echo "  $0 -c FST -c C2-FST         # only FST configs"
    echo "  $0 -d words -c C2-FST -n    # dry run"
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
    echo "       Run: cd build && make -j"
    exit 1
fi

mkdir -p "${RESULTS_DIR}"

# ---------------------------------------------------------------------------
# Helper: extract one field from benchmark stdout
# Expected last CSV line format: build_ms,size_mb,latency_ns
# ---------------------------------------------------------------------------
run_one() {
    local label="$1" dataset_name="$2" dataset_path="$3"
    local case_id="$4" space_relax="$5" max_rec="$6" mask="$7"

    local cmd="${BINARY} ${dataset_path} ${case_id} ${space_relax} ${max_rec} ${mask}"

    if $DRY_RUN; then
        echo "[DRY RUN] ${cmd}"
        return
    fi

    echo ""
    echo ">>> ${label} on ${dataset_name}"
    echo "    cmd: ${cmd}"

    # Run 4 times: discard run 1 (warmup), average runs 2-4
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

    # Average the 3 kept runs using Python for float arithmetic
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
    local name="$1"
    [[ ${#SELECTED_DATASETS[@]} -eq 0 ]] && return 0
    for d in "${SELECTED_DATASETS[@]}"; do [[ "$d" == "$name" ]] && return 0; done
    return 1
}

should_run_config() {
    local label="$1"
    [[ ${#SELECTED_CONFIGS[@]} -eq 0 ]] && return 0
    for c in "${SELECTED_CONFIGS[@]}"; do [[ "$c" == "$label" ]] && return 0; done
    return 1
}

# ---------------------------------------------------------------------------
# Write CSV header
# ---------------------------------------------------------------------------
if ! $DRY_RUN; then
    echo "trie,dataset,build_ms,size_mb,latency_ns" > "${RESULTS_FILE}"
    echo "Results will be written to: ${RESULTS_FILE}"
fi

# ---------------------------------------------------------------------------
# Main loop: iterate configs × datasets (matches paper table structure)
# ---------------------------------------------------------------------------
for config_str in "${CONFIGS[@]}"; do
    IFS='|' read -r label case_id space_relax max_rec mask <<< "${config_str}"
    should_run_config "${label}" || continue

    for ds_name in "${DATASET_ORDER[@]}"; do
        should_run_dataset "${ds_name}" || continue
        ds_path="${DATASETS[$ds_name]}"
        if [[ -z "${ds_path}" ]]; then
            echo ">>> ${label} on ${ds_name}: SKIPPED (no dataset path configured)"
            continue
        fi
        if [[ ! -f "${ds_path}" ]]; then
            echo ">>> ${label} on ${ds_name}: SKIPPED (file not found: ${ds_path})"
            continue
        fi
        run_one "${label}" "${ds_name}" "${ds_path}" \
                "${case_id}" "${space_relax}" "${max_rec}" "${mask}"
    done
done

if ! $DRY_RUN; then
    echo ""
    echo "Done. Results saved to: ${RESULTS_FILE}"
fi
