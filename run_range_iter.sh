#!/usr/bin/env bash
# =============================================================================
# run_range_iter.sh
#
# Iterator-based range query benchmark (benchmark case 15).
#
# 实验目的
# --------
# 对 C2-FST 与 FST-baseline 进行 iterator 形式的 range_count_iter(l, r) 延迟对比。
# iterator 逐键遍历 [l, r)，开销为 O(w)（w = 区间内 key 数量）。
#
# 查询类型
# --------
#   range_iter — (l=keys[i], r=keys[i+w])，统计区间内 key 数量
#                w ∈ {1, 10, 100, 1000}
#
# 用法
# ----
#   ./run_range_iter.sh               # 默认 3 次计时运行，全部 6 个数据集
#   ./run_range_iter.sh -d xml        # 只跑 xml 数据集
#   ./run_range_iter.sh -r 5          # 5 次计时运行取平均
#   ./run_range_iter.sh -n            # dry run：只打印命令
#   ./run_range_iter.sh -h            # 显示帮助
#
# 输出
# ----
# - 终端：进度信息 + 汇总表
# - 文件：results/range_iter_<TIMESTAMP>.csv
#         列：dataset, trie, query_type, range_width, latency_ns
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/build/benchmark"
DS="${SCRIPT_DIR}/full_dataset"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULTS_FILE="${RESULTS_DIR}/range_iter_${TIMESTAMP}.csv"

TIMED_RUNS=3
DRY_RUN=false
DATASET_FILTER=""

usage() {
    grep '^#' "$0" | grep -A20 '用法' | head -8 | sed 's/^# \?//'
    exit 0
}

while getopts "r:d:nh" opt; do
    case $opt in
        r) TIMED_RUNS="$OPTARG" ;;
        d) DATASET_FILTER="$OPTARG" ;;
        n) DRY_RUN=true ;;
        h) usage ;;
        *) usage ;;
    esac
done

# ---------------------------------------------------------------------------
# 数据集配置
# ---------------------------------------------------------------------------
declare -A DS_PATHS=(
    [words]="${DS}/words/words-470k.txt"
    [url]="${DS}/url/uk-2014-tpd_sorted.ids"
    [dna]="${DS}/dna/dna100_31mers_sorted.txt"
    [xml]="${DS}/xml/dblp.xml.200MB_sorted"
    [wiki]="${DS}/wiki/enwiki-ns0-ascii.txt"
    [log]="${DS}/log/access_log_common_sorted.txt"
)
DATASET_ORDER=(words url dna xml wiki log)

if [[ -n "$DATASET_FILTER" ]]; then
    if [[ -z "${DS_PATHS[$DATASET_FILTER]+x}" ]]; then
        echo "ERROR: unknown dataset '$DATASET_FILTER'. Valid: ${!DS_PATHS[*]}"
        exit 1
    fi
    DATASET_ORDER=("$DATASET_FILTER")
fi

# ---------------------------------------------------------------------------
# 构建 binary
# ---------------------------------------------------------------------------
if [[ ! -f "${BINARY}" ]]; then
    if $DRY_RUN; then
        echo "[DRY RUN] cmake --build \"${SCRIPT_DIR}/build\" --target benchmark -j"
    else
        echo ">>> benchmark binary not found; building..."
        cmake --build "${SCRIPT_DIR}/build" --target benchmark -j
    fi
fi

if ! $DRY_RUN && [[ ! -f "${BINARY}" ]]; then
    echo "ERROR: build failed — ${BINARY} not found"; exit 1
fi

mkdir -p "${RESULTS_DIR}"

# ---------------------------------------------------------------------------
# run_range_iter_one <ds_name>
#
# 执行 1 次 warmup + TIMED_RUNS 次计时，收集所有 raw 数据行，
# 用 Python 对每个 (trie, width) cell 取均值后追加到 CSV。
# ---------------------------------------------------------------------------
run_range_iter_one() {
    local ds_name="$1"
    local ds_path="${DS_PATHS[$ds_name]}"
    local cmd="${BINARY} ${ds_path} 15 0 0 0"

    if $DRY_RUN; then
        echo "[DRY RUN] ${ds_name}: ${cmd}"
        return
    fi

    if [[ ! -f "${ds_path}" ]]; then
        echo ">>> ${ds_name}: SKIPPED (${ds_path} not found)"
        return
    fi

    echo ""
    echo ">>> dataset: ${ds_name}"
    echo "    path:    ${ds_path}"

    local all_data_lines=""
    local total=$(( TIMED_RUNS + 1 ))
    for ((run=1; run<=total; run++)); do
        local output
        output="$($cmd 2>/dev/null)" || {
            echo "    [FAILED on run ${run}]"; return
        }
        if [[ $run -eq 1 ]]; then
            echo "    run1: warmup (discarded)"
            # Check for correctness warnings in warmup output
            if echo "$output" | grep -q "WARNING"; then
                echo "    [CORRECTNESS WARNING detected — check implementation]"
                echo "$output" | grep "WARNING" | sed 's/^/    /'
            fi
            continue
        fi
        echo "    run${run}: done"
        # Extract lines: trie,range_iter,width,latency_ns
        local data_lines
        data_lines="$(echo "$output" | grep -E '^(C2-FST|FST-baseline),range_iter,[0-9]+,[0-9]')"
        all_data_lines="${all_data_lines}${data_lines}"$'\n'
    done

    python3 - "${ds_name}" "${RESULTS_FILE}" <<PYEOF
import sys
from collections import defaultdict

ds_name  = sys.argv[1]
out_file = sys.argv[2]
raw_lines = """${all_data_lines}"""

sums   = defaultdict(float)
counts = defaultdict(int)
order  = []

for line in raw_lines.strip().splitlines():
    parts = line.strip().split(',')
    if len(parts) != 4:
        continue
    trie, qtype, width, lat_str = parts
    try:
        lat = float(lat_str)
    except ValueError:
        continue
    key = (trie, qtype, width)
    if key not in sums:
        order.append(key)
    sums[key]   += lat
    counts[key] += 1

with open(out_file, 'a') as f:
    for key in order:
        trie, qtype, width = key
        avg = sums[key] / counts[key]
        f.write(f'{ds_name},{trie},{qtype},{width},{avg:.4f}\n')

print(f'\n    Results for {ds_name}:')
print(f'    {"trie":<20} {"width":>6}  latency_ns')
print(f'    {"-"*20} {"------":>6}  ----------')
for key in order:
    trie, qtype, width = key
    avg = sums[key] / counts[key]
    print(f'    {trie:<20} {width:>6}  {avg:.2f}')
PYEOF
}

# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
echo "=============================================="
echo " Iterator Range Query Benchmark (case 15)"
echo " Datasets:     ${DATASET_ORDER[*]}"
echo " Timed runs:   ${TIMED_RUNS}"
echo " Output:       ${RESULTS_FILE}"
echo "=============================================="

if ! $DRY_RUN; then
    echo "dataset,trie,query_type,range_width,latency_ns" > "${RESULTS_FILE}"
fi

for ds_name in "${DATASET_ORDER[@]}"; do
    run_range_iter_one "${ds_name}"
done

# ---------------------------------------------------------------------------
# 汇总表
# ---------------------------------------------------------------------------
if ! $DRY_RUN && [[ -f "${RESULTS_FILE}" ]]; then
    python3 - "${RESULTS_FILE}" <<'PYEOF'
import sys, csv
from collections import defaultdict

rows = list(csv.DictReader(open(sys.argv[1])))
if not rows:
    print("No data in results file.")
    sys.exit(0)

datasets = list(dict.fromkeys(r['dataset'] for r in rows))
tries    = ['C2-FST', 'FST-baseline']
widths   = ['1', '10', '100', '1000', '10000']

idx = {}
for r in rows:
    idx[(r['dataset'], r['trie'], r['range_width'])] = float(r['latency_ns'])

def get(ds, trie, w):
    v = idx.get((ds, trie, w))
    return f'{v:>9.1f}' if v is not None else f'{"N/A":>9}'

hdr = f'{"trie":<20}' + ''.join(f'  {d:>10}' for d in datasets)

for w in widths:
    print(f'\n  range_width = {w}')
    print(hdr)
    print('-' * len(hdr))
    for trie in tries:
        row = f'{trie:<20}'
        for ds in datasets:
            row += f'  {get(ds, trie, w):>10}'
        print(row)
    # speedup row
    row = f'  {"C2-FST speedup":>18}'
    for ds in datasets:
        base = idx.get((ds, 'FST-baseline', w))
        c2   = idx.get((ds, 'C2-FST', w))
        if base and c2:
            row += f'  {base/c2:>9.2f}x'
        else:
            row += f'  {"N/A":>10}'
    print(row)

# geometric mean speedup across datasets per width
print(f'\n{"Geometric mean speedup (C2-FST vs FST-baseline)":-^60}')
print(f'  {"width":>6}  geomean_speedup')
print(f'  {"------":>6}  ---------------')
import math
for w in widths:
    ratios = []
    for ds in datasets:
        base = idx.get((ds, 'FST-baseline', w))
        c2   = idx.get((ds, 'C2-FST', w))
        if base and c2:
            ratios.append(base / c2)
    if ratios:
        gm = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
        print(f'  {w:>6}  {gm:.2f}x')
PYEOF
    echo ""
    echo "Results saved to: ${RESULTS_FILE}"
fi
