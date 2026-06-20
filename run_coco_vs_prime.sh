#!/usr/bin/env bash
# =============================================================================
# run_coco_vs_prime.sh
#
# 复现论文中 Table: CoCo vs CoCo' 的对比实验。
#
# 实验目的
# --------
# 原始 CoCo-trie 在大数据集上会 OOM 或超时，无法直接参与主实验对比。
# 因此论文引入 CoCo'（C²-CoCo 的 build routine + 原始 CoCo BV），
# 并通过本实验在原始 CoCo 能跑的小数据集上证明 CoCo ≈ CoCo'，
# 从而为后续主实验中以 CoCo' 作为 CoCo baseline 提供合法性依据。
#
# 实现说明
# --------
# CoCo  = case 4 (CoCoWrapper):     原始开源 CoCo-trie 实现
# CoCo' = case 10 (CoCoSuxWrapper): C²-CoCo 的优化器 + LoudsSux BV
#
# 数据集预处理
# ------------
# 1. Prefix-only 过滤：原始 CoCo 不支持 prefix key（集合中某个 key 是
#    另一个 key 的真前缀），必须先过滤。url/dna 本身已是 prefix-free，
#    words 有约 9.5 万对 prefix 关系，xml 有约 123 对，均需过滤。
#
# 2. URL 字符逆序：将 URL 每个 key 逐字符反转后再排序（如 "abc.co.uk"
#    → "ku.oc.cba"），使共同 TLD 后缀变成共同前缀，让两个优化器在
#    macro-node 边界上做出更接近的决策，减小 latency 差距。
#
# 已知局限
# --------
# Space 数字无法完全对齐：CoCo_v2::size_in_bits() 不包含 string pool，
# 而 CoCoCC::size_in_bits() 包含 MARISA pool（用于压缩长 unique suffix）。
# 在 words/dna 等 unique suffix 较多的数据集上会有 8–16% 的 space 差距。
# 论文的核心声明是 latency 相近（within ~100 ns），本实验以此为主要指标。
#
# 用法
# ----
#   ./run_coco_vs_prime.sh                 # 默认 3 次计时运行取平均
#   ./run_coco_vs_prime.sh -r 5            # 指定 5 次计时运行
#   ./run_coco_vs_prime.sh -n              # dry run：只打印命令不执行
#   ./run_coco_vs_prime.sh -h              # 显示帮助
#
# 输出
# ----
# - 终端：格式化对比表（latency ns/op, space %）
# - 文件：results/coco_vs_prime_<TIMESTAMP>.csv
#         列：trie, dataset, latency_ns, size_mb, size_pct, build_ms
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/build/benchmark"
DS="${SCRIPT_DIR}/full_dataset"
PFX_DIR="${SCRIPT_DIR}/full_dataset/prefix_only"
RESULTS_DIR="${SCRIPT_DIR}/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULTS_FILE="${RESULTS_DIR}/coco_vs_prime_${TIMESTAMP}.csv"

TIMED_RUNS=3   # 每个 cell 的计时运行次数（另加 1 次 warmup）
DRY_RUN=false

usage() {
    grep '^#' "$0" | grep -A99 '用法' | grep -B99 '输出' | grep '^#' \
        | sed 's/^# \?//' | head -8
    exit 0
}

while getopts "r:nh" opt; do
    case $opt in
        r) TIMED_RUNS="$OPTARG" ;;
        n) DRY_RUN=true ;;
        h) usage ;;
        *) usage ;;
    esac
done

# ---------------------------------------------------------------------------
# 源数据集路径
# ---------------------------------------------------------------------------
declare -A SRC_DATASETS=(
    [words]="${DS}/words/words-470k.txt"
    [url]="${DS}/url/uk-2014-tpd_sorted.ids"
    [dna]="${DS}/dna/dna100_31mers_sorted.txt"
    [xml]="${DS}/xml/dblp.xml.200MB_sorted"
)
DATASET_ORDER=(words url dna xml)

# ---------------------------------------------------------------------------
# 构建 benchmark binary（如不存在）
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
# generate_prefix_only <src> <dst> <reverse>
#
# 读取 src，排序去重，可选逐字符反转（reverse=true），
# 然后过滤掉所有满足 keys[i] 是 keys[i+1] 真前缀的 keys[i]，
# 结果写入 dst。
# ---------------------------------------------------------------------------
generate_prefix_only() {
    local src="$1" dst="$2" reverse="$3"
    python3 - "$src" "$dst" "$reverse" <<'PYEOF'
import sys
src, dst, reverse = sys.argv[1], sys.argv[2], sys.argv[3] == 'true'
with open(src) as f:
    raw = [line.rstrip('\n') for line in f]
keys = sorted(set(k[::-1] if reverse else k for k in raw))
filtered, removed = [], 0
for i, k in enumerate(keys):
    # 若下一个 key 以 k 为真前缀，则 k 是 prefix key，跳过
    if i + 1 < len(keys) and keys[i+1].startswith(k) and len(keys[i+1]) > len(k):
        removed += 1
        continue
    filtered.append(k)
with open(dst, 'w') as f:
    f.write('\n'.join(filtered) + '\n')
tag = ' [char-reversed]' if reverse else ''
print(f"  {src.split('/')[-1]}{tag}: {len(keys)} → {len(filtered)} keys "
      f"({removed} prefix keys removed)")
PYEOF
}

# ---------------------------------------------------------------------------
# 准备 prefix-only 数据集（已存在则跳过）
# ---------------------------------------------------------------------------
declare -A DATASET_PATHS

prepare_datasets() {
    echo ">>> Preparing prefix-only datasets..."
    mkdir -p "${PFX_DIR}"
    for ds in "${DATASET_ORDER[@]}"; do
        # URL 逐字符逆序；其他数据集保持原方向
        reverse=false
        [[ "$ds" == "url" ]] && reverse=true

        suffix="prefix_only"
        $reverse && suffix="reversed_prefix_only"
        local dst="${PFX_DIR}/${ds}_${suffix}.txt"
        DATASET_PATHS[$ds]="${dst}"

        if [[ -f "${dst}" ]]; then
            echo "  ${ds}: already exists (${dst})"
        else
            generate_prefix_only "${SRC_DATASETS[$ds]}" "${dst}" "${reverse}"
        fi
    done
}

if $DRY_RUN; then
    # dry run 下只设路径，不真正生成文件
    for ds in "${DATASET_ORDER[@]}"; do
        sfx="prefix_only"
        [[ "$ds" == "url" ]] && sfx="reversed_prefix_only"
        DATASET_PATHS[$ds]="${PFX_DIR}/${ds}_${sfx}.txt"
    done
else
    prepare_datasets
fi

# ---------------------------------------------------------------------------
# run_one <label> <ds_name> <case_id> <space_relax>
#
# 执行 1 次 warmup + TIMED_RUNS 次计时，对 build_ms/size_mb/latency_ns
# 取平均，同时从 "space cost" 行提取 size_pct。
# 结果写入全局变量 RESULT_ROW（CSV 格式一行）。
# ---------------------------------------------------------------------------
RESULT_ROW=""

run_one() {
    local label="$1" ds_name="$2" case_id="$3" sr="$4"
    local ds_path="${DATASET_PATHS[$ds_name]}"
    local cmd="${BINARY} ${ds_path} ${case_id} ${sr} 0 0"

    if $DRY_RUN; then
        echo "[DRY RUN] ${label} / ${ds_name}: ${cmd}"
        return
    fi

    if [[ ! -f "${ds_path}" ]]; then
        echo ">>> ${label} / ${ds_name}: SKIPPED (${ds_path} not found)"
        return
    fi

    echo ""
    echo ">>> ${label} on ${ds_name}"
    echo "    cmd: ${cmd}"

    local csv_lines=() pct_lines=() output
    local total=$(( TIMED_RUNS + 1 ))
    for ((i=1; i<=total; i++)); do
        output="$($cmd 2>/dev/null)" || {
            echo "    [FAILED on run ${i}]"
            RESULT_ROW="${label},${ds_name},ERROR,ERROR,ERROR,ERROR"
            return
        }
        local csv_line pct_line
        csv_line="$(echo "${output}" | grep -E '^[0-9]+\.[0-9]+,[0-9]+\.[0-9]+,[0-9]+\.[0-9]+$' | tail -1)"
        pct_line="$(echo "${output}" | grep -E '^space cost:' | tail -1)"
        if [[ $i -eq 1 ]]; then
            echo "    run1: warmup (discarded)"
        else
            echo "    run${i}: ${csv_line}"
            [[ -n "${csv_line}" ]] && csv_lines+=("${csv_line}")
            [[ -n "${pct_line}" ]] && pct_lines+=("${pct_line}")
        fi
    done

    if [[ ${#csv_lines[@]} -eq 0 ]]; then
        echo "    [WARNING: no parseable CSV output]"
        RESULT_ROW="${label},${ds_name},PARSE_ERROR,PARSE_ERROR,PARSE_ERROR,PARSE_ERROR"
        return
    fi

    # 对 TIMED_RUNS 次结果取平均
    local avg
    avg="$(python3 - "${csv_lines[@]}" <<'PYEOF'
import sys
rows = [list(map(float, r.split(','))) for r in sys.argv[1:]]
n = len(rows)
avgs = [sum(col)/n for col in zip(*rows)]
print(','.join(f'{v:.4f}' for v in avgs))
PYEOF
)"
    local build_ms size_mb latency_ns
    IFS=',' read -r build_ms size_mb latency_ns <<< "${avg}"

    # 从第一次计时运行的输出中提取 space %
    local size_pct="N/A"
    if [[ ${#pct_lines[@]} -gt 0 ]]; then
        size_pct="$(echo "${pct_lines[0]}" | python3 -c "
import sys, re
m = re.search(r'\(([0-9.]+)%', sys.stdin.read())
print(f'{float(m.group(1)):.1f}' if m else 'N/A')
")"
    fi

    echo "    avg: build=${build_ms} ms  size=${size_mb} MB (${size_pct}%)  latency=${latency_ns} ns"
    RESULT_ROW="${label},${ds_name},${latency_ns},${size_mb},${size_pct},${build_ms}"
}

# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
echo "========================================"
echo " CoCo vs CoCo' — prefix-only datasets"
echo " Timed runs per cell: ${TIMED_RUNS}"
echo "========================================"

if ! $DRY_RUN; then
    echo "trie,dataset,latency_ns,size_mb,size_pct,build_ms" > "${RESULTS_FILE}"
    echo "Results will be written to: ${RESULTS_FILE}"
fi

# case 4  = CoCoWrapper      (原始 CoCo-trie，space_relax 参数被忽略，内部固定 5%)
# case 10 = CoCoSuxWrapper   (C²-CoCo build + LoudsSux BV，space_relax=0)
declare -A CASE_IDS=( [CoCo]=4  [CoCo-prime]=10 )
declare -A SR=( [CoCo]=0  [CoCo-prime]=0  )
CONFIG_ORDER=(CoCo CoCo-prime)

for label in "${CONFIG_ORDER[@]}"; do
    for ds_name in "${DATASET_ORDER[@]}"; do
        run_one "${label}" "${ds_name}" "${CASE_IDS[$label]}" "${SR[$label]}"
        if ! $DRY_RUN && [[ -n "${RESULT_ROW}" ]]; then
            echo "${RESULT_ROW}" >> "${RESULTS_FILE}"
        fi
    done
done

# ---------------------------------------------------------------------------
# 打印汇总表
# ---------------------------------------------------------------------------
if ! $DRY_RUN && [[ -f "${RESULTS_FILE}" ]]; then
    echo ""
    echo "========================================"
    echo " Summary"
    echo "========================================"
    python3 - "${RESULTS_FILE}" <<'PYEOF'
import sys, csv

rows, datasets = {}, []
with open(sys.argv[1]) as f:
    for r in csv.DictReader(f):
        trie, ds = r['trie'], r['dataset']
        if ds not in datasets:
            datasets.append(ds)
        rows[(trie, ds)] = r

tries = ['CoCo', 'CoCo-prime']
labels = {'CoCo': 'CoCo', 'CoCo-prime': "CoCo'"}

# 表头
w = 12
hdr = f"{'':>{w}}" + "".join(f"  {d:>9}  {'':>5}" for d in datasets)
sub = f"{'':>{w}}" + "".join(f"  {'ns/op':>9}  {'size%':>5}" for _ in datasets)
print(hdr)
print(sub)
print("-" * len(hdr))

prev_ns = {}
for trie in tries:
    row_str = f"{labels[trie]:>{w}}"
    for ds in datasets:
        key = (trie, ds)
        if key in rows:
            r = rows[key]
            try:
                ns = float(r['latency_ns'])
            except ValueError:
                ns = float('nan')
            pct = r['size_pct']
            row_str += f"  {ns:>9.0f}  {pct:>5}%"
            prev_ns[(trie, ds)] = ns
        else:
            row_str += f"  {'N/A':>9}  {'N/A':>5} "
    print(row_str)

# 打印 latency gap 行
print()
gap_str = f"{'gap (ns)':>{w}}"
for ds in datasets:
    ns4  = prev_ns.get(('CoCo', ds), float('nan'))
    ns10 = prev_ns.get(('CoCo-prime', ds), float('nan'))
    gap = abs(ns10 - ns4)
    mark = "✓" if gap <= 100 else "!"
    gap_str += f"  {gap:>8.0f}{mark}  {'':>5} "
print(gap_str)
PYEOF
    echo ""
    echo "Results saved to: ${RESULTS_FILE}"
fi
