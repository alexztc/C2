#!/usr/bin/env bash
# =============================================================================
# run_successor.sh
#
# 复现论文 Successor / Range-Query 性能对比实验（benchmark case 14）。
#
# 实验目的
# --------
# 对三种 C² trie（C2-FST、C2-CoCo(LS)、C2-MARISA）与 SURF/FST baseline
# 进行 successor(q) 查询的延迟对比，在此基础上实现
#
#   range_count(l, r) = successor(r).leaf_id − successor(l).leaf_id
#
# 并用以下三类查询评测完整 range query 性能：
#
#   hit     —— 精确命中：l = keys[i]，测试 successor(existing key) 路径
#   miss    —— 不命中：l = keys[i]+'\x01'，测试"下降到末端→回溯"路径
#   range   —— 范围查询：(l=keys[i], r=keys[i+w])，测试两次 successor 之和
#              w ∈ {1, 10, 100, 1000}（对应不同选择度）
#
# 架构贡献说明
# ------------
# LoudsSux（CoCo' / case 10）与 baseline Marisa 均无 parent_pos()，
# 因此无法支持 successor。C² 拓扑是支持该操作的必要条件。
#
# 用法
# ----
#   ./run_successor.sh               # 默认 3 次计时运行，4 个数据集
#   ./run_successor.sh -d xml        # 只跑 xml 数据集
#   ./run_successor.sh -r 5          # 5 次计时运行取平均
#   ./run_successor.sh -n            # dry run：只打印命令
#   ./run_successor.sh -h            # 显示帮助
#
# 输出
# ----
# - 终端：进度信息 + 汇总表
# - 文件：results/successor_<TIMESTAMP>.csv
#         列：dataset, trie, query_type, range_width, latency_ns
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/build/benchmark"
DS="${SCRIPT_DIR}/full_dataset"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULTS_FILE="${RESULTS_DIR}/successor_${TIMESTAMP}.csv"

TIMED_RUNS=3
DRY_RUN=false
DATASET_FILTER=""

usage() {
    sed -n '/^# 用法/,/^# 输出/p' "$0" | grep '^#' | sed 's/^# \?//' | head -10
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
# run_successor_one <ds_name>
#
# 执行 1 次 warmup + TIMED_RUNS 次计时，收集所有 raw 数据行，
# 用 Python 对每个 (trie, query_type, width) cell 取均值后追加到 CSV。
# ---------------------------------------------------------------------------
run_successor_one() {
    local ds_name="$1"
    local ds_path="${DS_PATHS[$ds_name]}"
    local cmd="${BINARY} ${ds_path} 14 0 0 0"

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

    # Collect raw data lines from all timed runs (skip warmup run 1)
    local all_data_lines=""
    local total=$(( TIMED_RUNS + 1 ))
    for ((run=1; run<=total; run++)); do
        local output
        output="$($cmd 2>/dev/null)" || {
            echo "    [FAILED on run ${run}]"; return
        }
        if [[ $run -eq 1 ]]; then
            echo "    run1: warmup (discarded)"
            continue
        fi
        echo "    run${run}: done"
        # Extract data lines: trie,query_type,width,latency_ns
        local data_lines
        data_lines="$(echo "$output" | grep -E '^[^,]+,(hit|miss|range),[0-9]+,[0-9]')"
        all_data_lines="${all_data_lines}${data_lines}"$'\n'
    done

    # Use Python to average across runs and append to CSV
    python3 - "${ds_name}" "${RESULTS_FILE}" <<PYEOF
import sys, csv
from collections import defaultdict

ds_name   = sys.argv[1]
out_file  = sys.argv[2]
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

# Print per-dataset summary to stdout
print(f'\n    Results for {ds_name}:')
print(f'    {"trie":<20} {"type":<8} {"width":<8} latency_ns')
print(f'    {"-"*20} {"-"*8} {"-"*8} ----------')
for key in order:
    trie, qtype, width = key
    avg = sums[key] / counts[key]
    print(f'    {trie:<20} {qtype:<8} {width:<8} {avg:.2f}')
PYEOF
}

# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
echo "=============================================="
echo " Successor / Range-Query Benchmark"
echo " Datasets:     ${DATASET_ORDER[*]}"
echo " Timed runs:   ${TIMED_RUNS}"
echo " Output:       ${RESULTS_FILE}"
echo "=============================================="

if ! $DRY_RUN; then
    echo "dataset,trie,query_type,range_width,latency_ns" > "${RESULTS_FILE}"
fi

for ds_name in "${DATASET_ORDER[@]}"; do
    run_successor_one "${ds_name}"
done

# ---------------------------------------------------------------------------
# 汇总表（Python）
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
tries    = ['C2-FST', 'C2-CoCo(LS)', 'C2-MARISA', 'FST-baseline']

idx = {}
for r in rows:
    idx[(r['dataset'], r['trie'], r['query_type'], r['range_width'])] = float(r['latency_ns'])

def get(ds, trie, qt, w='0'):
    v = idx.get((ds, trie, qt, w))
    return f'{v:>8.1f}' if v is not None else f'{"N/A":>8}'

# --- Point queries ---
print(f'\n{"Point queries (hit / miss)":-^72}')
hdr = f'{"trie":<20}' + ''.join(f'  {d:>9} {"hit/miss":>8}' for d in datasets)
print(hdr)
print('-' * len(hdr))
for trie in tries:
    row = f'{trie:<20}'
    for ds in datasets:
        h = get(ds, trie, 'hit').strip()
        m = get(ds, trie, 'miss').strip()
        row += f'  {h:>9}/{m:<9}'
    print(row)

# --- Speedup vs FST-baseline (hit) ---
print(f'\n{"Speedup vs FST-baseline (hit queries)":-^72}')
hdr2 = f'{"trie":<20}' + ''.join(f'  {d:>9}' for d in datasets)
print(hdr2)
print('-' * len(hdr2))
for trie in tries:
    row = f'{trie:<20}'
    for ds in datasets:
        base = idx.get((ds, 'FST-baseline', 'hit', '0'))
        mine = idx.get((ds, trie, 'hit', '0'))
        if base and mine:
            row += f'  {base/mine:>8.2f}x'
        else:
            row += f'  {"N/A":>9}'
    print(row)

# --- Range queries ---
print(f'\n{"Range query latency (ns per range_count call)":-^72}')
hdr3 = f'{"trie":<20}' + ''.join(f'  {d:>9}' for d in datasets)
for w in ['1', '10', '100', '1000']:
    print(f'\n  range width = {w} keys')
    print(hdr3)
    print('-' * len(hdr3))
    for trie in tries:
        row = f'{trie:<20}'
        for ds in datasets:
            row += f'  {get(ds, trie, "range", w):>9}'
        print(row)
    # speedup row
    row = f'  {"speedup":>18}'
    for ds in datasets:
        base = idx.get((ds, 'FST-baseline', 'range', w))
        c2   = idx.get((ds, 'C2-FST', 'range', w))
        if base and c2:
            row += f'  {base/c2:>8.2f}x'
        else:
            row += f'  {"N/A":>9}'
    print(row)
PYEOF
    echo ""
    echo "Results saved to: ${RESULTS_FILE}"
fi
