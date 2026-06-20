# C² — Cache-Conscious Succinct Tries

Re-implementations of three state-of-the-art succinct trie families
(**FST**, **CoCo-trie**, **MARISA**) with a cache-conscious bitvector
redesign and adaptive unary-path compression. This repository accompanies
the paper and contains everything needed to reproduce the headline
experiments.

> **Paper:** Kepan Zhang, Tiancheng Zhao, Helen Xu.
> *C²: Cache-Conscious Succinct Tries with Adaptive Unary Path Compression.*
> arXiv:2606.16104 [cs.DB], 2026. <https://arxiv.org/abs/2606.16104>

| Variant | Files | Speedup over baseline | Space change |
|---------|-------|-----------------------|--------------|
| C²-FST    | `include/fst_cc.hpp`     | up to **1.92×**      | up to **41% smaller** |
| C²-CoCo   | `include/coco_cc.hpp`    | up to **1.22×** (vs CoCo′) | up to **46% smaller** (vs CoCo′) |
| C²-MARISA | `include/marisa_cc.hpp`  | up to **1.51×**      | up to **46% smaller** |

## Repository layout

```
.
├── benchmark.cpp              # Driver: case IDs 0..14 in `main()`
├── include/                   # C² implementations (header-only)
├── baseline_{fst,coco,marisa,pdt,art,ctriepp}/
│                              # Wrappers around upstream baseline tries
├── lib/                       # Third-party libraries (git submodules)
├── example_dataset/           # 4.6 MB English-words sample for quick tests
├── run_benchmarks.sh          # Main paper Table 1 (6 datasets × N tries)
├── run_coco_vs_prime.sh       # CoCo vs CoCo′ validity (Section 6.X)
├── run_bv_ops.sh              # Bitvector operator microbenchmark (Table 2)
├── run_recursion_ablation.sh  # 9-config × 6-dataset recursion ablation
├── plot_recursion_pareto.py   # Pareto-frontier figure generator
├── DATASETS.md                # Where to get the other five datasets
├── CMakeLists.txt
└── LICENSE
```

## Quick start

### 1. Clone with submodules

```bash
git clone --recursive https://github.com/<you>/C2.git
cd C2
```

Or, if already cloned non-recursively:

```bash
git submodule update --init --recursive
```

### 2. Prerequisites

- CMake ≥ 3.13
- A C++20 compiler (GCC ≥ 10, Clang ≥ 12)
- Boost (≥ 1.71)
- Python 3.8+ with `matplotlib` (only for the plotting script)

On Debian/Ubuntu:

```bash
sudo apt-get install build-essential cmake libboost-all-dev python3-matplotlib
```

### 3. Build the MARISA static library, then everything else

```bash
bash baseline_marisa/build_marisa.sh

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j benchmark
```

### 4. Smoke-test on the bundled example dataset

```bash
./build/benchmark example_dataset/words-470k.txt 0 0 0 0
#                                                ^ ^ ^ ^
#                                                | | | mask: tail container
#                                                | | max_recursion
#                                                | space_relaxation (CoCo)
#                                                trie case ID
```

Expected output ends with a line like
`<build_ms>,<size_mb>,<latency_ns>` and `[PASSED]`.

## Benchmark CLI

```
./build/benchmark <dataset_path> <case_id> <space_relax> <max_rec> <mask>
```

| Arg              | Meaning |
|------------------|---------|
| `dataset_path`   | Absolute path to a one-key-per-line ASCII file. **Use absolute paths**; the script will read from where it is invoked. |
| `case_id`        | See "Case IDs" below. |
| `space_relax`    | Space-relaxation knob for C²-CoCo and CoCo′. Ignored elsewhere. Default `0`. |
| `max_rec`        | Maximum nested recursion depth ρ ∈ {0,1,2} for C²-FST/CoCo/MARISA. `0` = no recursion (latency-optimal). |
| `mask`           | Tail container choice. `0` or ≥`4`: FSST; `2,3`: Re-pair; `1`: sorted. Default FSST. |

### Case IDs

| ID | Trie | Header |
|----|------|--------|
| 0  | C²-FST            | `include/fst_cc.hpp` |
| 1  | C²-CoCo           | `include/coco_cc.hpp` |
| 2  | C²-MARISA         | `include/marisa_cc.hpp` |
| 3  | FST baseline      | `baseline_fst/` (SuRF) |
| 4  | CoCo baseline (prefix-only datasets only) | `baseline_coco/` |
| 5  | MARISA baseline   | `baseline_marisa/` |
| 6  | PDT baseline      | `baseline_pdt/` |
| 7  | ART               | `baseline_art/` |
| 8  | C-ART             | `baseline_art/` |
| 9  | C²-CoCo with LOUDS-Sparse (alt. variant) | `include/coco_cc.hpp` |
| 10 | CoCo′ — C²-CoCo build + original CoCo BV  | `include/coco_cc.hpp` |
| 11–13 | Bitvector operator microbench (FST/CoCo/MARISA) | see `run_bv_ops.sh` |
| 14 | Successor query                            | see `benchmark.cpp::test_successor` |

## Reproducing paper results

Place the six full datasets per `DATASETS.md`, then run:

```bash
./run_benchmarks.sh              # Main table:        results/benchmark_<TS>.csv
./run_recursion_ablation.sh      # Recursion ablation: results/recursion_ablation_<TS>.csv
./run_bv_ops.sh                  # BV ops microbench:  results/bv_ops_<TS>.csv
./run_coco_vs_prime.sh           # CoCo vs CoCo′:      results/coco_vs_prime_<TS>.csv
python3 plot_recursion_pareto.py # Figure 4 (Pareto):  recursion_pareto*.pdf
```

Each script runs one warmup pass plus three timed passes and averages them.
On a single Intel Xeon Gold core with 25 MB LLC, the full suite takes about
**3–4 hours**.

## Citation

If you use this code or any part of it in academic work, please cite:

```bibtex
@article{zhang2026c2,
  title   = {{C}$^2$: Cache-Conscious Succinct Tries with Adaptive Unary
             Path Compression},
  author  = {Zhang, Kepan and Zhao, Tiancheng and Xu, Helen},
  journal = {arXiv preprint arXiv:2606.16104},
  year    = {2026},
  url     = {https://arxiv.org/abs/2606.16104}
}
```

## License

MIT for the C² implementations and experiment scripts. The vendored
baselines retain their upstream licenses; see `LICENSE` for the full list.
