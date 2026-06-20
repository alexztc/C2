# Datasets

The main experiments (`run_benchmarks.sh`, `run_recursion_ablation.sh`,
`run_bv_ops.sh`) operate on six datasets used in the paper. Only a small
subset (`words-470k.txt`, ~5 MB) is bundled in `example_dataset/` to keep the
repository lightweight. The other five must be downloaded and decompressed
into the layout below before running the full benchmarks.

## Expected layout

The experiment scripts hard-code the path
`full_dataset/{dataset}/{file}` relative to the repository root:

```
full_dataset/
├── words/words-470k.txt                  # 4.6 MB
├── url/uk-2014-tpd_sorted.ids            # 49 MB
├── dna/dna100_31mers_sorted.txt          # 114 MB
├── xml/dblp.xml.200MB_sorted             # 116 MB
├── wiki/enwiki-ns0-ascii.txt             # 342 MB
└── log/access_log_common_sorted.txt      # 481 MB
```

Total: ~1.0 GB on disk after preprocessing.

For the CoCo vs CoCo' experiment, `run_coco_vs_prime.sh` generates
prefix-only variants automatically into `full_dataset/prefix_only/`.

## Sources

| Dataset | Description | Original source |
|---------|-------------|-----------------|
| words   | 470K English words from Google Web Trillion N-gram corpus | https://github.com/efficient/SuRF/tree/master/test/words.txt |
| url     | 1.8M private domains from UK 2014 web crawl              | http://law.di.unimi.it/webdata/uk-2014-tpd/ |
| dna     | 2.9M unique 31-mers from human chromosome 1               | https://github.com/aboffa/CoCo-trie/tree/master/dataset |
| xml     | 2.1M URI strings extracted from a 200 MB DBLP XML dump    | https://dblp.org/xml/release/ |
| wiki    | 17M ASCII Wikipedia article titles                       | https://dumps.wikimedia.org/enwiki/ |
| log     | 4.5M lines from Apache common-log access traces           | https://github.com/aboffa/CoCo-trie/tree/master/dataset |

## Preprocessing

Each dataset must be:
1. **Sorted** lexicographically (`sort -u` is sufficient for most).
2. **ASCII-only**: stray UTF-8 bytes break the byte-aligned alphabet
   used by C²-FST / C²-CoCo / C²-Marisa.
3. **One key per line**, no trailing whitespace, single `\n` terminator.

The `words-470k.txt` shipped under `example_dataset/` already satisfies
these constraints and can be used to dry-run the build:

```bash
./build/benchmark example_dataset/words-470k.txt 0 0 0 0   # C²-FST
```

## Reproducing paper Table 1

After all six datasets are in place:

```bash
./run_benchmarks.sh                  # writes results/benchmark_<TS>.csv
./run_recursion_ablation.sh          # 9 tries × 6 datasets ablation
./run_bv_ops.sh                      # bitvector operator microbench
./run_coco_vs_prime.sh               # CoCo vs CoCo' validity check
```

Each script logs progress to stdout and saves a CSV to `results/`.
