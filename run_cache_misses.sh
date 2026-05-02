#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_BINARY="${SCRIPT_DIR}/build/profile"
FULL_DATASET_DIR="${SCRIPT_DIR}/full_dataset"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULTS_FILE="${RESULTS_DIR}/cache_misses_${TIMESTAMP}.csv"

# ---------------------------------------------------------------------------
# Dataset paths — only the two largest datasets (tab:cache-motivation)
# ---------------------------------------------------------------------------
declare -A DATASETS=(
    [wiki]="${FULL_DATASET_DIR}/wiki/enwiki-ns0-ascii.txt"
    [log]="${FULL_DATASET_DIR}/log/access_log_common_sorted.txt"
)

DATASET_ORDER=(wiki log)

# ---------------------------------------------------------------------------
# Configs: "label|case|space_relax|max_rec|mask"
# Matches the columns in tab:cache-motivation; all use FSST tail (mask=0)
# ---------------------------------------------------------------------------
CONFIGS=(
    "FST|3|0|0|0"
    "C2-FST|0|0|0|0"
    "CoCo-prime|10|0|0|0"
    "C2-CoCo|1|0|0|0"
    "Marisa|5|0|0|0"
    "C2-Marisa|2|0|0|0"
    "C-ART|8|0|0|0"
)

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
SELECTED_DATASETS=()
SELECTED_CONFIGS=()
DRY_RUN=false
CPU_PIN=""   # e.g. "0" to pin to core 0; empty = no pinning

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Measures steady-state LLC read misses per query (warm-cache, query phase only)."
    echo "The profile binary runs one warmup pass then measures LLC read misses"
    echo "(PERF_TYPE_HW_CACHE / LL / READ / MISS) via perf_event_open — no external perf needed."
    echo ""
    echo "Requirements:"
    echo "  /proc/sys/kernel/perf_event_paranoid must be <= 2"
    echo "  (sudo sysctl kernel.perf_event_paranoid=2)"
    echo ""
    echo "Options:"
    echo "  -d <dataset>   Run only this dataset (can be repeated)"
    echo "                 Available: ${DATASET_ORDER[*]}"
    echo "  -c <label>     Run only this config label (can be repeated)"
    echo "                 Available: FST C2-FST CoCo-prime C2-CoCo Marisa C2-Marisa C-ART"
    echo "  -p <cpu>       Pin process to this CPU core (e.g. -p 0); reduces migration noise"
    echo "  -n             Dry run: print commands without executing"
    echo "  -h             Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                          # run all 7 configs x 2 datasets"
    echo "  $0 -d wiki                  # only wiki"
    echo "  $0 -c FST -c C2-FST         # only FST configs"
    echo "  $0 -p 0 -d log -c Marisa    # pin to core 0, log dataset, Marisa only"
    echo "  $0 -d log -c Marisa -n      # dry run"
    exit 0
}

while getopts "d:c:p:nh" opt; do
    case $opt in
        d) SELECTED_DATASETS+=("$OPTARG") ;;
        c) SELECTED_CONFIGS+=("$OPTARG") ;;
        p) CPU_PIN="$OPTARG" ;;
        n) DRY_RUN=true ;;
        h) usage ;;
        *) usage ;;
    esac
done

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------
if [[ ! -f "${PROFILE_BINARY}" ]]; then
    echo "ERROR: profile binary not found at ${PROFILE_BINARY}"
    echo "       Run: cd build && make -j"
    exit 1
fi

if [[ -n "${CPU_PIN}" ]] && ! command -v taskset &>/dev/null; then
    echo "WARNING: -p ${CPU_PIN} requested but taskset not found; running without CPU pinning"
    CPU_PIN=""
fi

PARANOID="$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 'unknown')"
if [[ "${PARANOID}" != "unknown" ]] && [[ "${PARANOID}" -gt 2 ]]; then
    echo "WARNING: /proc/sys/kernel/perf_event_paranoid=${PARANOID} (must be <= 2)"
    echo "         Fix: sudo sysctl kernel.perf_event_paranoid=2"
fi

mkdir -p "${RESULTS_DIR}"

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
# run_one: single (config, dataset) measurement
#
# LLC read misses are measured by perf_event_open inside the profile binary
# (PERF_TYPE_HW_CACHE / LL / READ / MISS), wrapped around query_trie() only
# after a full warmup pass — build and I/O are excluded.
# The binary prints "misses/query: <value>" to stdout.
# ---------------------------------------------------------------------------
run_one() {
    local label="$1" dataset_name="$2" dataset_path="$3"
    local case_id="$4" space_relax="$5" max_rec="$6" mask="$7"

    local base_cmd="${PROFILE_BINARY} ${dataset_path} ${case_id} ${space_relax} ${max_rec} ${mask}"
    local cmd
    if [[ -n "${CPU_PIN}" ]]; then
        cmd="taskset -c ${CPU_PIN} ${base_cmd}"
    else
        cmd="${base_cmd}"
    fi

    if $DRY_RUN; then
        echo "[DRY RUN] ${cmd}"
        return
    fi

    echo ""
    echo ">>> ${label} on ${dataset_name}"
    echo "    cmd: ${cmd}"

    local output
    output="$($cmd 2>&1)" || {
        echo "    [FAILED]"
        echo "${label},${dataset_name},ERROR" >> "${RESULTS_FILE}"
        return
    }

    # "misses/query: <value>" is printed by profile binary after the query loop
    local misses_per_query
    misses_per_query="$(echo "${output}" | grep "^misses/query:" | awk '{print $2}')"

    if [[ -z "${misses_per_query}" ]]; then
        echo "    [WARNING: misses/query not found — perf_event_open may have failed]"
        echo "    (check /proc/sys/kernel/perf_event_paranoid, must be <= 2)"
        echo "${label},${dataset_name},PARSE_ERROR" >> "${RESULTS_FILE}"
        return
    fi

    local n_queries
    n_queries="$(echo "${output}" | grep "replicated dataset size" | awk '{print $NF}')"

    local total_misses
    total_misses="$(echo "${output}" | grep "^query cache-misses:" | awk '{print $3}')"

    echo "    queries:       ${n_queries}"
    echo "    cache-misses:  ${total_misses}"
    echo "    misses/query:  ${misses_per_query}"
    echo "${label},${dataset_name},${misses_per_query}" >> "${RESULTS_FILE}"
}

# ---------------------------------------------------------------------------
# Write CSV header
# ---------------------------------------------------------------------------
if ! $DRY_RUN; then
    echo "trie,dataset,cache_misses_per_query" > "${RESULTS_FILE}"
    echo "Results will be written to: ${RESULTS_FILE}"
fi

# ---------------------------------------------------------------------------
# Main loop: configs × datasets
# ---------------------------------------------------------------------------
for config_str in "${CONFIGS[@]}"; do
    IFS='|' read -r label case_id space_relax max_rec mask <<< "${config_str}"
    should_run_config "${label}" || continue

    for ds_name in "${DATASET_ORDER[@]}"; do
        should_run_dataset "${ds_name}" || continue
        ds_path="${DATASETS[$ds_name]}"
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
