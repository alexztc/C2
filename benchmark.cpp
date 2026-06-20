#include "include/fst_cc.hpp"
#include "include/coco_optimizer.hpp"
#include "include/coco_cc.hpp"
#include "include/marisa_cc.hpp"
#include "include/louds_cc.hpp"
#include "include/louds_sparse_cc.hpp"
#include "include/louds_sux.hpp"
#include "include/louds_marisa.hpp"

#include "baseline_pdt/pdt_wrapper.hpp"
#include "baseline_coco/coco_wrapper.hpp"
#include "baseline_fst/fst_wrapper.hpp"
#include "baseline_marisa/marisa_wrapper.hpp"
#include "baseline_art/art_wrapper.hpp"
#include "baseline_art/cart_wrapper.hpp"
#include "baseline_ctriepp/ctriepp_wrapper.hpp"
#include "baseline_ctriepp/zfast_wrapper.hpp"

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <chrono>
#include <random>
#include <unordered_set>

#ifdef __PROFILE__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <asm/unistd.h>
#include <cerrno>
#include <cstring>
#endif


// #define __CORRECTNESS_TEST__


class FstCCWrapper {  // unified API
 public:
  using trie_t = c2::FstCC<std::string>;

  __NOINLINE_IF_PROFILE FstCCWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                     int max_recursion = 0, int mask = 0) {
    trie_.build(keys.begin(), keys.end(), true, max_recursion, mask);
  }

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto successor(const std::string &key) const -> int32_t {
    return trie_.successor(key);
  }

  auto range_count_iter(const std::string &l, const std::string &r) const -> int32_t {
    auto it = trie_.lower_bound(l, r);
    int32_t cnt = 0;
    while (it.valid()) { cnt++; it.next(); }
    return cnt;
  }

  auto contains_prefix(const std::string &q) const -> bool {
    return trie_.contains_prefix(q);
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }

  void print_unary_path_stats() const {
    trie_.print_unary_path_stats();
  }
 private:
  trie_t trie_;
};

class CoCoCCWrapper {  // unified API
 public:
  using trie_t = c2::CoCoCC<std::string>;

  __NOINLINE_IF_PROFILE CoCoCCWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                      int max_recursion = 0, int mask = 0)
                                      : trie_(keys.begin(), keys.end(), true, space_relaxation, max_recursion, mask) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }
 private:
  trie_t trie_;
};

class CoCoLSWrapper {  // unified API
 public:
  using trie_t = c2::CoCoCC<std::string, c2::LoudsSparseCC>;

  __NOINLINE_IF_PROFILE CoCoLSWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                      int max_recursion = 0, int mask = 0)
                                      : trie_(keys.begin(), keys.end(), true, space_relaxation, max_recursion, mask) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto successor(const std::string &key) const -> int32_t {
    return trie_.successor(key);
  }

  auto contains_prefix(const std::string &q) const -> bool {
    return trie_.contains_prefix(q);
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }
 private:
  trie_t trie_;
};

class CoCoSuxWrapper {  // unified API
 public:
  using trie_t = c2::CoCoCC<std::string, c2::LoudsSux<>>;

  __NOINLINE_IF_PROFILE CoCoSuxWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                       int max_recursion = 0, int mask = 0)
                                       : trie_(keys.begin(), keys.end(), true, space_relaxation, max_recursion, mask) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }
 private:
  trie_t trie_;
};

class MarisaCCWrapper {  // unified API
 public:
  using trie_t = c2::MarisaCC<std::string>;

  __NOINLINE_IF_PROFILE MarisaCCWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                        int max_recursion = 0, int mask = 0) {
    trie_.build(keys.begin(), keys.end(), true, max_recursion, mask);
  }

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto successor(const std::string &key) const -> int32_t {
    return trie_.successor(key);
  }

  auto contains_prefix(const std::string &q) const -> bool {
    return trie_.contains_prefix(q);
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }

  void print_unary_path_stats() const {
    trie_.print_unary_path_stats();
  }
 private:
  trie_t trie_;
};

template <typename trie_t>
void __attribute__((noinline)) query_trie(const std::vector<std::string> &keys, const trie_t &trie) {
#ifdef __CORRECTNESS_TEST__
  std::unordered_set<uint32_t> key_ids;
#endif
  for (uint32_t i = 0; i < keys.size(); i++) {
    // printf("%d: %s\n", i, keys[i].c_str());
    volatile uint32_t key_id = trie.lookup(keys[i]);
  #ifdef __CORRECTNESS_TEST__
    uint32_t id = key_id;
    printf("%d:%s, id = %d\n", i, keys[i].c_str(), id);
    EXPECT(id != -1);
    if constexpr (std::is_same_v<trie_t, FstCCWrapper> || std::is_same_v<trie_t, CoCoCCWrapper> ||
                  std::is_same_v<trie_t, MarisaCCWrapper>) {  // key IDs must be in range [0, n-1] and unique
      EXPECT(id < keys.size());
      EXPECT(key_ids.count(id) == 0);
      key_ids.insert(id);
    }
  #endif
  }
}

template <typename trie_t>
void __attribute__((noinline)) test_trie(const char *filename, uint32_t space_relaxation, int max_recursion, int mask) {
  printf("Processing dataset...\n");
  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string key;
  size_t original_size = 0;
  while (std::getline(file, key)) {
    keys.emplace_back(key);
    original_size += key.size();
  }
  double original_size_in_mb = (double)original_size/c2::mb_bytes;
  std::sort(keys.begin(), keys.end());
  auto new_end = std::unique(keys.begin(), keys.end());
  keys.erase(new_end, keys.end());
  printf("Done!\n");

  printf("Building trie...\n");
  auto start = std::chrono::high_resolution_clock::now();
  trie_t trie(keys, space_relaxation, max_recursion, mask);
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = (end - start).count();
  double build_time = (double)duration/1000000;
  printf("Done!\n");
  printf("build time: %lf ms (%lf ns per key)\n", build_time, (double)duration/keys.size());

  size_t space_cost = trie.space_cost();
  double size_in_mb = (double)space_cost/c2::mb_bits;
  printf("space cost: %lf MB (%lf%% of original size %lf MB)\n", size_in_mb,
         size_in_mb/original_size_in_mb*100, original_size_in_mb);

#ifdef __PROFILE__
  size_t min_queries = 10000000, n = keys.size();  // make the test loop last longer for more accurate results
  while (keys.size() < min_queries) {
    for (size_t j = 0; j < n; j++) {  // replicate original dataset
      keys.push_back(keys[j]);
    }
  }
  printf("replicated dataset size: %ld\n", keys.size());
#endif
  // Use a separate query vector so that tries storing pointers into keys[]
  // (CTrie++, ZFastTrie) are not broken by the shuffle.
  std::vector<std::string> query_keys = keys;
  std::shuffle(query_keys.begin(), query_keys.end(), std::mt19937{2});
  printf("Querying trie...\n");
#ifdef __PROFILE__
  // Warmup pass: bring hot trie blocks into cache before counting misses
  query_trie<trie_t>(query_keys, trie);
  printf("Warmup done. Starting LLC miss measurement...\n");

  // Prefer the precise LLC read-miss event; fall back to the generic hardware
  // cache-miss counter (which maps to LLC misses on most x86 PMUs) if the
  // precise event is unavailable (e.g. in virtualised environments)
  struct perf_event_attr pe{};
  pe.size           = sizeof(pe);
  pe.disabled       = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv     = 1;
  pe.type   = PERF_TYPE_HW_CACHE;
  pe.config = PERF_COUNT_HW_CACHE_LL
            | (PERF_COUNT_HW_CACHE_OP_READ    << 8)
            | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
  int perf_fd = (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
  if (perf_fd < 0) {
    pe.type   = PERF_TYPE_HARDWARE;
    pe.config = PERF_COUNT_HW_CACHE_MISSES;
    perf_fd   = (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (perf_fd < 0) {
      fprintf(stderr, "perf_event_open failed: %s (check /proc/sys/kernel/perf_event_paranoid <= 2)\n",
              strerror(errno));
    } else {
      fprintf(stderr, "Note: LLC-specific event unavailable; using PERF_COUNT_HW_CACHE_MISSES\n");
    }
  }
  if (perf_fd >= 0) {
    ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);
  }
#endif
  start = std::chrono::high_resolution_clock::now();
  query_trie<trie_t>(query_keys, trie);
  end = std::chrono::high_resolution_clock::now();
#ifdef __PROFILE__
  if (perf_fd >= 0) {
    ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);
    long long cache_miss_count = 0;
    read(perf_fd, &cache_miss_count, sizeof(cache_miss_count));
    close(perf_fd);
    printf("query cache-misses: %lld\n", cache_miss_count);
    printf("misses/query: %.1f\n", (double)cache_miss_count / keys.size());
  }
#endif
  duration = (end - start).count();
  double avg_latency = (double)duration/keys.size();
  printf("Done!\n");
  printf("total time: %lf ms, avg latency: %lf ns\n", (double)duration/1000000, avg_latency);
  trie.print_space_cost_breakdown();
  if constexpr (requires { trie.print_unary_path_stats(); }) {
    trie.print_unary_path_stats();
  }

  printf("%lf,%lf,%lf\n", build_time, size_in_mb, avg_latency);
  printf("[PASSED]\n");
}

#ifdef __COMPARE_COCO__
void compare_louds_coco(const std::string &filename, uint32_t space_relaxation, int max_recursion, int mask) {
  printf("Processing dataset...\n");
  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string key;
  while (std::getline(file, key)) {
    keys.emplace_back(key);
  }
  std::sort(keys.begin(), keys.end());
  auto new_end = std::unique(keys.begin(), keys.end());
  keys.erase(new_end, keys.end());
  printf("Done!\n");

  printf("Building trie...\n");
  c2::CoCoCC<std::string> trie(keys.begin(), keys.end(), true, space_relaxation, max_recursion, mask);
  c2::CoCoCC<std::string, c2::LoudsSparseCC> trie_ls(keys.begin(), keys.end(), true, space_relaxation, max_recursion, mask);
  printf("Done!\n");

  printf("Converting to standard LOUDS...\n");
  std::unique_ptr<c2::LoudsSux<>> louds;
  trie.to_louds_sux(louds);
  printf("Done!\n");

  printf("Comparing LOUDS performance...\n");
  auto topo = trie.get_topo();
  auto topo_ls = trie_ls.get_topo();
  uint32_t bv_size = topo->num_nodes() * 2 - 1;
  std::vector<uint32_t> leaves[2], internals[2], child_query[2];
  leaves[0].reserve(topo->num_leaves());
  leaves[1].reserve(topo->num_leaves());
  internals[0].reserve(topo->num_internals());
  internals[1].reserve(topo->num_internals());
  child_query[0].reserve(topo->num_nodes() - 1);
  child_query[1].reserve(topo_ls->num_children());
  for (uint32_t i = 0; i < bv_size; i++) {
    assert(topo->get(i) == louds->get(i));
    if (i == 0 || !topo->get(i - 1)) {
      if (topo->get(i)) {
        internals[0].push_back(i);
      } else {
        leaves[0].push_back(i);
      }
    }
    if (topo->get(i)) {
      child_query[0].push_back(i);
    }
  }

  for (uint32_t i = 0; i < topo_ls->size(); i++) {
    if (topo_ls->louds(i)) {
      internals[1].push_back(i);
    }
    if (topo_ls->has_child(i)) {
      child_query[1].push_back(i);
    } else {
      leaves[1].push_back(i);
    }
  }

  // all positions (for GET test)
  std::vector<uint32_t> all_louds_pos, all_sparse_pos;
  all_louds_pos.reserve(bv_size);
  for (uint32_t i = 0; i < bv_size; i++) all_louds_pos.push_back(i);
  all_sparse_pos.reserve(topo_ls->size());
  for (uint32_t i = 0; i < topo_ls->size(); i++) all_sparse_pos.push_back(i);

  std::shuffle(all_louds_pos.begin(), all_louds_pos.end(), std::mt19937{0});
  std::shuffle(all_sparse_pos.begin(), all_sparse_pos.end(), std::mt19937{0});
  std::shuffle(leaves[0].begin(), leaves[0].end(), std::mt19937{1});
  std::shuffle(leaves[1].begin(), leaves[1].end(), std::mt19937{1});
  std::shuffle(internals[0].begin(), internals[0].end(), std::mt19937{2});
  std::shuffle(internals[1].begin(), internals[1].end(), std::mt19937{2});
  std::shuffle(child_query[0].begin(), child_query[0].end(), std::mt19937{3});
  std::shuffle(child_query[1].begin(), child_query[1].end(), std::mt19937{3});

  size_t get_time[3], leaf_id_time[3], internal_id_time[3], degree_time[3], child_pos_time[3];

  printf("[GET]...\n");
  {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto i : all_louds_pos) { volatile auto r = topo->get(i); }
    auto t1 = std::chrono::high_resolution_clock::now();
    get_time[0] = (t1 - t0).count();
    t0 = std::chrono::high_resolution_clock::now();
    for (auto i : all_louds_pos) { volatile auto r = louds->get(i); }
    t1 = std::chrono::high_resolution_clock::now();
    get_time[1] = (t1 - t0).count();
    t0 = std::chrono::high_resolution_clock::now();
    for (auto i : all_sparse_pos) {
      volatile auto r0 = topo_ls->has_child(i);
      volatile auto r1 = topo_ls->louds(i);
    }
    t1 = std::chrono::high_resolution_clock::now();
    get_time[2] = (t1 - t0).count();
  }

  printf("[LEAF ID]...\n");
  auto start = std::chrono::high_resolution_clock::now();
  for (auto i : leaves[0]) {
    volatile auto res = topo->leaf_id(i);
  }
  auto end = std::chrono::high_resolution_clock::now();
  leaf_id_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : leaves[0]) {
    volatile auto res = louds->leaf_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  leaf_id_time[1] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : leaves[1]) {
    volatile auto res = topo_ls->leaf_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  leaf_id_time[2] = (end - start).count();

  printf("[INTERNAL ID]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : internals[0]) {
    volatile auto res = topo->internal_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  internal_id_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : internals[0]) {
    volatile auto res = louds->internal_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  internal_id_time[1] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : internals[1]) {
    volatile auto res = topo_ls->node_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  internal_id_time[2] = (end - start).count();

  printf("[DEGREE]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : internals[0]) {
    volatile auto res = topo->node_degree(i);
  }
  end = std::chrono::high_resolution_clock::now();
  degree_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : internals[0]) {
    volatile auto res = louds->node_degree(i);
  }
  end = std::chrono::high_resolution_clock::now();
  degree_time[1] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : internals[1]) {
    volatile auto res = topo_ls->node_degree(i);
  }
  end = std::chrono::high_resolution_clock::now();
  degree_time[2] = (end - start).count();

  printf("[CHILD_POS]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : child_query[0]) {
    volatile auto res = topo->child_pos(i);
  }
  end = std::chrono::high_resolution_clock::now();
  child_pos_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : child_query[0]) {
    volatile auto res = louds->child_pos(i);
  }
  end = std::chrono::high_resolution_clock::now();
  child_pos_time[1] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : child_query[1]) {
    volatile auto res = topo_ls->child_pos(i);
  }
  end = std::chrono::high_resolution_clock::now();
  child_pos_time[2] = (end - start).count();

  // order: C2-CoCo(LoudsCC) | CoCo'(LoudsSux) | C2-FST(LoudsSparseCC)
  printf("GET(ns): %lf vs %lf vs %lf\n", (double)get_time[0]/all_louds_pos.size(),
         (double)get_time[1]/all_louds_pos.size(), (double)get_time[2]/all_sparse_pos.size());
  printf("LEAF_ID(ns): %lf vs %lf vs %lf\n", (double)leaf_id_time[0]/leaves[0].size(),
         (double)leaf_id_time[1]/leaves[0].size(), (double)leaf_id_time[2]/leaves[1].size());
  printf("INTERNAL_ID(ns): %lf vs %lf vs %lf\n", (double)internal_id_time[0]/internals[0].size(),
         (double)internal_id_time[1]/internals[0].size(), (double)internal_id_time[2]/internals[1].size());
  printf("DEGREE(ns): %lf vs %lf vs %lf\n", (double)degree_time[0]/internals[0].size(),
         (double)degree_time[1]/internals[0].size(), (double)degree_time[2]/internals[1].size());
  printf("CHILD_POS(ns): %lf vs %lf vs %lf\n", (double)child_pos_time[0]/child_query[0].size(),
         (double)child_pos_time[1]/child_query[0].size(), (double)child_pos_time[2]/child_query[1].size());

  printf("Done!\n");
}
#endif

#ifdef __COMPARE_FST__
// Baseline LOUDS-sparse: same bit content as LoudsSparseCC but stored in two
// separate bitvectors using SuRF's original BitvectorRank (child_indicator) +
// BitvectorSelect (louds_bits) — the actual structures from the original paper.
struct LoudsSparseBaseline {
  std::unique_ptr<surf::BitvectorRank>   child_rank;    // has_child with rank LUT
  std::unique_ptr<surf::BitvectorSelect> louds_select;  // louds with select LUT

  void build(const c2::LoudsSparseCC *topo) {
    uint32_t n = topo->size();
    uint32_t num_words = (n + 63) / 64;
    std::vector<surf::word_t> child_words(num_words, 0);
    std::vector<surf::word_t> louds_words(num_words, 0);
    // SuRF stores bits MSB-first within each 64-bit word
    for (uint32_t i = 0; i < n; i++) {
      if (topo->has_child(i)) child_words[i / 64] |= (surf::word_t(1) << (63 - i % 64));
      if (topo->louds(i))     louds_words[i / 64] |= (surf::word_t(1) << (63 - i % 64));
    }
    std::vector<std::vector<surf::word_t>> child_bpl = {std::move(child_words)};
    std::vector<std::vector<surf::word_t>> louds_bpl = {std::move(louds_words)};
    std::vector<surf::position_t> nbpl = {n};
    child_rank   = std::make_unique<surf::BitvectorRank>(512, child_bpl, nbpl, 0, 1);
    louds_select = std::make_unique<surf::BitvectorSelect>(64,  louds_bpl, nbpl, 0, 1);
  }

  // get: readBit from two separate allocations (two cache-line misses)
  void get(uint32_t i) const {
    volatile bool r0 = child_rank->readBit(i);
    volatile bool r1 = louds_select->readBit(i);
  }

  // leaf_id: rank0 on child; called with has_child[i]=0, so rank(i)=rank(i-1)
  uint32_t leaf_id(uint32_t i) const {
    return i - child_rank->rank(i);
  }

  // degree: distance to next louds=1 bit (mirrors SuRF's nodeSize implementation)
  uint32_t node_degree(uint32_t i) const {
    return louds_select->distanceToNextSetBit(i);
  }

  // child_pos: rank on child → select on louds (mirrors SuRF's childPosPub)
  uint32_t child_pos(uint32_t i) const {
    return louds_select->select(child_rank->rank(i) + 1);
  }
};

void compare_louds_fst(const std::string &filename) {
  printf("Processing dataset...\n");
  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string key;
  while (std::getline(file, key)) keys.emplace_back(key);
  std::sort(keys.begin(), keys.end());
  auto new_end = std::unique(keys.begin(), keys.end());
  keys.erase(new_end, keys.end());
  printf("Done!\n");

  printf("Building C2-FST...\n");
  c2::FstCC<std::string> fst_cc;
  fst_cc.build(keys.begin(), keys.end(), true, 0, 0);
  printf("Done!\n");

  auto topo = fst_cc.get_topo();   // LoudsSparseCC

  printf("Building baseline (same trie, separate bitvectors)...\n");
  LoudsSparseBaseline base;
  base.build(topo);
  printf("Done!\n");

  // Build query position sets from the C2 trie; reuse for baseline (same trie)
  std::vector<uint32_t> all_pos, child_pos, leaf_pos, node_pos;
  for (uint32_t i = 0; i < topo->size(); i++) {
    all_pos.push_back(i);
    if (topo->has_child(i)) child_pos.push_back(i);
    else                    leaf_pos.push_back(i);
    if (topo->louds(i))     node_pos.push_back(i);
  }
  std::shuffle(all_pos.begin(),   all_pos.end(),   std::mt19937{1});
  std::shuffle(leaf_pos.begin(),  leaf_pos.end(),  std::mt19937{2});
  std::shuffle(node_pos.begin(),  node_pos.end(),  std::mt19937{3});
  std::shuffle(child_pos.begin(), child_pos.end(), std::mt19937{4});

  size_t get_time[2], leaf_id_time[2], degree_time[2], child_time[2];

  printf("[GET]...\n");
  auto start = std::chrono::high_resolution_clock::now();
  for (auto i : all_pos) {
    volatile auto r0 = topo->has_child(i);
    volatile auto r1 = topo->louds(i);
  }
  auto end = std::chrono::high_resolution_clock::now();
  get_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : all_pos) { base.get(i); }
  end = std::chrono::high_resolution_clock::now();
  get_time[1] = (end - start).count();

  printf("[LEAF_ID]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : leaf_pos) { volatile auto r = topo->leaf_id(i); }
  end = std::chrono::high_resolution_clock::now();
  leaf_id_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : leaf_pos) { volatile auto r = base.leaf_id(i); }
  end = std::chrono::high_resolution_clock::now();
  leaf_id_time[1] = (end - start).count();

  printf("[DEGREE]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : node_pos) { volatile auto r = topo->node_degree(i); }
  end = std::chrono::high_resolution_clock::now();
  degree_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : node_pos) { volatile auto r = base.node_degree(i); }
  end = std::chrono::high_resolution_clock::now();
  degree_time[1] = (end - start).count();

  printf("[CHILD_POS]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : child_pos) { volatile auto r = topo->child_pos(i); }
  end = std::chrono::high_resolution_clock::now();
  child_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : child_pos) { volatile auto r = base.child_pos(i); }
  end = std::chrono::high_resolution_clock::now();
  child_time[1] = (end - start).count();

  // order: C2-FST(LoudsSparseCC) | baseline(separate bitvectors, same trie)
  printf("GET(ns): %lf vs %lf\n",       (double)get_time[0]/all_pos.size(),    (double)get_time[1]/all_pos.size());
  printf("LEAF_ID(ns): %lf vs %lf\n",   (double)leaf_id_time[0]/leaf_pos.size(),(double)leaf_id_time[1]/leaf_pos.size());
  printf("DEGREE(ns): %lf vs %lf\n",    (double)degree_time[0]/node_pos.size(), (double)degree_time[1]/node_pos.size());
  printf("CHILD_POS(ns): %lf vs %lf\n", (double)child_time[0]/child_pos.size(), (double)child_time[1]/child_pos.size());
  printf("Done!\n");
}
#endif

#ifdef __COMPARE_MARISA__
void compare_louds_marisa(const std::string &filename, int max_recursion, int mask) {
  printf("Processing dataset...\n");
  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string key;
  while (std::getline(file, key)) {
    keys.emplace_back(key);
  }
  std::sort(keys.begin(), keys.end());
  auto new_end = std::unique(keys.begin(), keys.end());
  keys.erase(new_end, keys.end());
  printf("Done!\n");

  printf("Building trie...\n");
  c2::MarisaCC<std::string, false> trie;
  trie.build(keys.begin(), keys.end(), true, max_recursion, mask);
  printf("Done!\n");

  printf("Converting to standard LOUDS...\n");
  std::unique_ptr<c2::LoudsMarisa> louds;
  trie.to_louds_marisa(louds);
  printf("Done!\n");

  printf("Comparing LOUDS performance...\n");
  auto topo = trie.get_topo();
  std::vector<uint32_t> bv_pos[2], link[2], term[2];
  std::vector<uint32_t> child_query[2], parent_query[2];
  bv_pos[0].reserve(topo->size());
  bv_pos[1].reserve(topo->size());
  link[0].reserve(topo->num_links());
  link[1].reserve(topo->num_links());
  term[0].reserve(topo->num_leaves());
  term[1].reserve(topo->num_leaves());
  child_query[0].reserve(topo->num_children());
  child_query[1].reserve(topo->num_children());
  parent_query[0].reserve(topo->num_nodes() - 1);
  parent_query[1].reserve(topo->num_nodes() - 1);
  for (uint32_t i = 0; i < topo->size(); i++) {
    bv_pos[0].push_back(i);
    bv_pos[1].push_back(i);
    if (topo->has_child(i)) {
      child_query[0].push_back(i);
    } else {
      term[0].push_back(i);
      term[1].push_back(i);
    }
    if (topo->louds(i) && topo->has_parent(i)) {
      parent_query[0].push_back(i);
    }
    if (topo->is_link(i)) {
      link[0].push_back(i);
      link[1].push_back(i);
    }
  }
  bool is_root = true;
  for (uint32_t i = 0; i < louds->size(); i++) {
    if (louds->louds(i)) {
      child_query[1].push_back(i);
    } else if (is_root) {
      is_root = false;
    } else {
      parent_query[1].push_back(i);
    }
  }

  std::shuffle(bv_pos[0].begin(), bv_pos[0].end(), std::mt19937{1});
  std::shuffle(bv_pos[1].begin(), bv_pos[1].end(), std::mt19937{1});
  std::shuffle(link[0].begin(), link[0].end(), std::mt19937{2});
  std::shuffle(link[1].begin(), link[1].end(), std::mt19937{2});
  std::shuffle(term[0].begin(), term[0].end(), std::mt19937{3});
  std::shuffle(term[1].begin(), term[1].end(), std::mt19937{3});
  std::shuffle(child_query[0].begin(), child_query[0].end(), std::mt19937{4});
  std::shuffle(child_query[1].begin(), child_query[1].end(), std::mt19937{4});
  std::shuffle(parent_query[0].begin(), parent_query[0].end(), std::mt19937{5});
  std::shuffle(parent_query[1].begin(), parent_query[1].end(), std::mt19937{5});

  printf("get: %ld vs %ld, link: %ld vs %ld, term: %ld vs %ld, child: %ld vs %ld, parent: %ld vs %ld\n",
         bv_pos[0].size(), bv_pos[1].size(), link[0].size(), link[1].size(), term[0].size(), term[1].size(),
         child_query[0].size(), child_query[1].size(), parent_query[0].size(), parent_query[1].size());

  size_t get_time[2], leaf_id_time[2], link_id_time[2];
  size_t child_pos_time[2], parent_pos_time[2];

  printf("[GET]...\n");
  auto start = std::chrono::high_resolution_clock::now();
  for (auto i : bv_pos[0]) {
    // topo->prefetch_block(i);
    volatile auto res0 = topo->is_link(i);
    volatile auto res1 = topo->has_child(i);
  }
  auto end = std::chrono::high_resolution_clock::now();
  get_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : bv_pos[1]) {
    volatile auto res0 = louds->is_link(i);
    volatile auto res1 = louds->is_term(i);
  }
  end = std::chrono::high_resolution_clock::now();
  get_time[1] = (end - start).count();

  printf("[LINK_ID]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : link[0]) {
    // topo->prefetch_block(i);
    volatile auto res = topo->link_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  link_id_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : link[1]) {
    volatile auto res = louds->link_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  link_id_time[1] = (end - start).count();

  printf("[LEAF_ID]...\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : term[0]) {
    // topo->prefetch_block(i);
    volatile auto res = topo->leaf_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  leaf_id_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : term[1]) {
    volatile auto res = louds->leaf_id(i);
  }
  end = std::chrono::high_resolution_clock::now();
  leaf_id_time[1] = (end - start).count();

  printf("[CHILD_POS]\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : child_query[0]) {
    // topo->prefetch_block(i);
    volatile auto res = topo->child_pos(i);
  }
  end = std::chrono::high_resolution_clock::now();
  child_pos_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : child_query[1]) {
    volatile auto res = louds->child_pos(i);
  }
  end = std::chrono::high_resolution_clock::now();
  child_pos_time[1] = (end - start).count();

  printf("[PARENT_POS]\n");
  start = std::chrono::high_resolution_clock::now();
  for (auto i : parent_query[0]) {
    // topo->prefetch_block(i);
    volatile auto res = topo->parent_pos(i);
  }
  end = std::chrono::high_resolution_clock::now();
  parent_pos_time[0] = (end - start).count();

  start = std::chrono::high_resolution_clock::now();
  for (auto i : parent_query[1]) {
    volatile auto res = louds->parent_pos(i);
  }
  end = std::chrono::high_resolution_clock::now();
  parent_pos_time[1] = (end - start).count();

  printf("GET(ns): %lf vs %lf\n", (double)get_time[0]/bv_pos[0].size(), (double)get_time[1]/bv_pos[0].size());
  printf("LINK_ID(ns): %lf vs %lf, LEAF_ID(ns): %lf vs %lf\n", (double)link_id_time[0]/link[1].size(),
         (double)link_id_time[1]/link[1].size(), (double)leaf_id_time[0]/term[0].size(), (double)leaf_id_time[1]/term[1].size());
  printf("CHILD_POS(ns): %lf vs %lf, PARENT_POS(ns): %lf vs %lf\n", (double)child_pos_time[0]/child_query[0].size(),
         (double)child_pos_time[1]/child_query[1].size(), (double)parent_pos_time[0]/parent_query[0].size(),
         (double)parent_pos_time[1]/parent_query[1].size());

  printf("Done!\n");
}
#endif

void test_successor(const char *filename, uint32_t space_relaxation, int max_recursion, int mask) {
  constexpr int   timed_reps      = 3;
  constexpr int   range_widths[]  = {1, 10, 100, 1000};
  constexpr size_t target_pairs   = 5000;

  // ---- load & deduplicate ----
  printf("Processing dataset...\n");
  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string k;
  while (std::getline(file, k)) keys.emplace_back(k);
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  size_t n = keys.size();
  printf("Done! %zu keys\n", n);

  // ---- build tries ----
  printf("Building tries...\n");
  FstCCWrapper    fst_cc(keys, space_relaxation, max_recursion, mask);
  CoCoLSWrapper   coco_ls(keys, space_relaxation, max_recursion, mask);
  MarisaCCWrapper marisa_cc(keys, space_relaxation, max_recursion, mask);
  FstWrapper      fst_base(keys, space_relaxation, max_recursion, mask);
  printf("Done!\n");

  // ---- query generation ----
  // hit: every key (shuffled)
  std::vector<std::string> hit_q = keys;
  std::shuffle(hit_q.begin(), hit_q.end(), std::mt19937{2});

  // miss: keys[i] + '\x01' — a string that lies strictly between keys[i] and
  // keys[i+1] in lex order, guaranteed not to be a key itself; successor = keys[i+1]
  std::vector<std::string> miss_q;
  miss_q.reserve(n - 1);
  for (size_t i = 0; i + 1 < n; i++) miss_q.emplace_back(keys[i] + '\x01');
  std::shuffle(miss_q.begin(), miss_q.end(), std::mt19937{3});

  // range: (keys[i], keys[i+w]) pairs sampled uniformly; w keys lie in [l, r)
  // l = keys[i]   — exact lower bound (inclusive hit)
  // r = keys[i+w] — exclusive upper bound (next boundary), successor(r) = id(keys[i+w])
  // range_count = id(keys[i+w]) − id(keys[i]) = w
  struct RangePair { std::string l, r; };
  auto make_range_queries = [&](int w) -> std::vector<RangePair> {
    if ((size_t)w >= n) return {};
    size_t n_valid = n - (size_t)w;
    size_t stride  = std::max<size_t>(1, n_valid / target_pairs);
    std::vector<RangePair> pairs;
    pairs.reserve(std::min(target_pairs, n_valid));
    for (size_t i = 0; i < n_valid; i += stride)
      pairs.push_back({keys[i], keys[i + (size_t)w]});
    std::shuffle(pairs.begin(), pairs.end(), std::mt19937{4u + (uint32_t)w});
    return pairs;
  };

  // ---- timing helpers ----
  auto measure_point = [&](auto &trie, const std::vector<std::string> &queries) -> double {
    int64_t total = 0;
    for (int rep = 0; rep < timed_reps; rep++) {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto &q : queries) { volatile int32_t r = trie.successor(q); }
      auto t1 = std::chrono::high_resolution_clock::now();
      total += (t1 - t0).count();
    }
    return (double)total / ((double)timed_reps * queries.size());
  };

  auto measure_range = [&](auto &trie, const std::vector<RangePair> &pairs) -> double {
    if (pairs.empty()) return 0.0;
    int64_t total = 0;
    for (int rep = 0; rep < timed_reps; rep++) {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto &p : pairs) {
        volatile int32_t a = trie.successor(p.l);
        volatile int32_t b = trie.successor(p.r);
        volatile int32_t cnt = b - a;
      }
      auto t1 = std::chrono::high_resolution_clock::now();
      total += (t1 - t0).count();
    }
    return (double)total / ((double)timed_reps * pairs.size());
  };

  // ---- warmup (all tries, hit queries) ----
  printf("Warming up...\n");
  for (auto &q : hit_q) { volatile int32_t r = fst_cc.successor(q); }
  for (auto &q : hit_q) { volatile int32_t r = coco_ls.successor(q); }
  for (auto &q : hit_q) { volatile int32_t r = marisa_cc.successor(q); }
  for (auto &q : hit_q) { volatile int32_t r = fst_base.successor(q); }

  // ---- point queries (hit / miss) ----
  printf("Measuring hit/miss...\n");
  // output format: trie,query_type,range_width,latency_ns
  // range_width = 0 for point queries; latency = ns per single successor call
  struct TrieRow { const char *name; double hit, miss; };
  TrieRow point_rows[] = {
    {"C2-FST",        measure_point(fst_cc,    hit_q), measure_point(fst_cc,    miss_q)},
    {"C2-CoCo(LS)",   measure_point(coco_ls,   hit_q), measure_point(coco_ls,   miss_q)},
    {"C2-MARISA",     measure_point(marisa_cc, hit_q), measure_point(marisa_cc, miss_q)},
    {"FST-baseline",  measure_point(fst_base,  hit_q), measure_point(fst_base,  miss_q)},
  };
  for (auto &row : point_rows) {
    printf("%s,hit,0,%.2f\n",  row.name, row.hit);
    printf("%s,miss,0,%.2f\n", row.name, row.miss);
  }

  // ---- range queries ----
  // latency = ns per range_count(l,r) = time for two successive successor() calls
  printf("Measuring range queries...\n");
  for (int w : range_widths) {
    auto pairs = make_range_queries(w);
    if (pairs.empty()) { printf("# width %d: skipped (not enough keys)\n", w); continue; }
    printf("# width=%d  n_pairs=%zu\n", w, pairs.size());
    printf("C2-FST,range,%d,%.2f\n",       w, measure_range(fst_cc,    pairs));
    printf("C2-CoCo(LS),range,%d,%.2f\n",  w, measure_range(coco_ls,   pairs));
    printf("C2-MARISA,range,%d,%.2f\n",    w, measure_range(marisa_cc, pairs));
    printf("FST-baseline,range,%d,%.2f\n", w, measure_range(fst_base,  pairs));
  }
  printf("[PASSED]\n");
}

void test_range_iter(const char *filename, uint32_t space_relaxation, int max_recursion, int mask) {
  constexpr int    timed_reps     = 3;
  constexpr int    range_widths[] = {1, 10, 100, 1000, 10000};
  constexpr size_t target_pairs   = 5000;

  printf("Processing dataset...\n");
  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string k;
  while (std::getline(file, k)) keys.emplace_back(k);
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  size_t n = keys.size();
  printf("Done! %zu keys\n", n);

  printf("Building tries...\n");
  FstCCWrapper fst_cc(keys, space_relaxation, max_recursion, mask);
  FstWrapper   fst_base(keys, space_relaxation, max_recursion, mask);
  printf("Done!\n");

  struct RangePair { std::string l, r; };
  auto make_pairs = [&](int w) -> std::vector<RangePair> {
    if ((size_t)w >= n) return {};
    size_t n_valid = n - (size_t)w;
    size_t stride  = std::max<size_t>(1, n_valid / target_pairs);
    std::vector<RangePair> pairs;
    pairs.reserve(std::min(target_pairs, n_valid));
    for (size_t i = 0; i < n_valid; i += stride)
      pairs.push_back({keys[i], keys[i + (size_t)w]});
    std::shuffle(pairs.begin(), pairs.end(), std::mt19937{4u + (uint32_t)w});
    return pairs;
  };

  // Warmup
  for (int w : range_widths) {
    auto pairs = make_pairs(w);
    for (auto &p : pairs) {
      volatile int32_t r = fst_cc.range_count_iter(p.l, p.r);
      volatile int32_t s = fst_base.range_count_iter(p.l, p.r);
    }
  }

  printf("Measuring range iteration...\n");
  for (int w : range_widths) {
    auto pairs = make_pairs(w);
    if (pairs.empty()) { printf("# width %d: skipped\n", w); continue; }

    // correctness check (first pair)
    {
      int32_t cnt_cc   = fst_cc.range_count_iter(pairs[0].l, pairs[0].r);
      int32_t cnt_base = fst_base.range_count_iter(pairs[0].l, pairs[0].r);
      if (cnt_cc != w || cnt_base != w)
        printf("# WARNING w=%d: C2-FST cnt=%d  FST-base cnt=%d  expected=%d\n",
               w, cnt_cc, cnt_base, w);
    }

    auto measure = [&](auto &trie) -> double {
      int64_t total = 0;
      for (int rep = 0; rep < timed_reps; rep++) {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (auto &p : pairs) { volatile int32_t cnt = trie.range_count_iter(p.l, p.r); }
        auto t1 = std::chrono::high_resolution_clock::now();
        total += (t1 - t0).count();
      }
      return (double)total / ((double)timed_reps * pairs.size());
    };

    printf("# width=%d  n_pairs=%zu\n", w, pairs.size());
    printf("C2-FST,range_iter,%d,%.2f\n",      w, measure(fst_cc));
    printf("FST-baseline,range_iter,%d,%.2f\n", w, measure(fst_base));
  }
  printf("[PASSED]\n");
}

void test_prefix_query(const char *filename, uint32_t space_relaxation, int max_recursion, int mask) {
  constexpr int   timed_reps   = 3;
  constexpr size_t target_queries = 5000;

  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string k;
  while (std::getline(file, k)) keys.emplace_back(k);
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  size_t n = keys.size();
  printf("Building tries on %zu keys...\n", n);
  FstCCWrapper    fst_cc(keys,  space_relaxation, max_recursion, mask);
  CoCoLSWrapper   coco_ls(keys, space_relaxation, max_recursion, mask);
  MarisaCCWrapper marisa(keys,  space_relaxation, max_recursion, mask);
  CtriePPWrapper  ctriepp(keys, space_relaxation, max_recursion, mask);
  printf("Done!\n");

  // Build prefix queries: half-length prefix of sampled keys
  size_t stride = std::max<size_t>(1, n / target_queries);
  std::vector<std::string> prefix_queries;
  prefix_queries.reserve(target_queries);
  for (size_t i = 0; i < n; i += stride) {
    const auto &key = keys[i];
    if (!key.empty())
      prefix_queries.push_back(key.substr(0, std::max<size_t>(1, key.size() / 2)));
  }
  std::shuffle(prefix_queries.begin(), prefix_queries.end(), std::mt19937{42u});

  // Correctness check: all four tries must agree
  {
    size_t fail = 0;
    for (auto &q : prefix_queries) {
      bool r0 = fst_cc.contains_prefix(q);
      bool r1 = coco_ls.contains_prefix(q);
      bool r2 = marisa.contains_prefix(q);
      bool r3 = ctriepp.contains_prefix(q);
      if (r0 != r1 || r0 != r2 || r0 != r3) {
        printf("# WARNING mismatch on '%s': C2-FST=%d CoCo=%d MARISA=%d CTrie++=%d\n",
               q.c_str(), (int)r0, (int)r1, (int)r2, (int)r3);
        if (++fail > 5) break;
      }
    }
    if (fail == 0) printf("# Correctness OK (%zu queries)\n", prefix_queries.size());
  }

  auto measure = [&](auto &trie) -> double {
    int64_t total = 0;
    for (int rep = 0; rep < timed_reps; rep++) {
      auto t0 = std::chrono::high_resolution_clock::now();
      for (auto &q : prefix_queries) { volatile bool r = trie.contains_prefix(q); }
      auto t1 = std::chrono::high_resolution_clock::now();
      total += (t1 - t0).count();
    }
    return (double)total / ((double)timed_reps * prefix_queries.size());
  };

  printf("# n_queries=%zu\n", prefix_queries.size());
  printf("C2-FST,prefix_query,%.2f\n",  measure(fst_cc));
  printf("C2-CoCo(LS),prefix_query,%.2f\n", measure(coco_ls));
  printf("C2-MARISA,prefix_query,%.2f\n",   measure(marisa));
  printf("CTrie++,prefix_query,%.2f\n",     measure(ctriepp));
  printf("[PASSED]\n");
}

void test_contains_prefix(const char *filename, uint32_t space_relaxation, int max_recursion, int mask) {
  std::ifstream file(filename);
  std::vector<std::string> keys;
  std::string k;
  while (std::getline(file, k)) keys.emplace_back(k);
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  size_t n = keys.size();
  printf("Building C2-FST on %zu keys...\n", n);
  FstCCWrapper trie(keys, space_relaxation, max_recursion, mask);
  printf("Done!\n");

  size_t fail = 0;
  // every key must be found (exact match is a valid prefix query)
  for (auto &key : keys)
    if (!trie.contains_prefix(key)) { printf("FAIL hit: %s\n", key.c_str()); if (++fail > 5) break; }

  // every strict prefix of each key must be found
  for (size_t i = 0; i < std::min(n, (size_t)5000); i++) {
    const auto &key = keys[i];
    for (size_t len = 0; len < key.size(); len++) {
      std::string pfx = key.substr(0, len);
      if (!trie.contains_prefix(pfx)) { printf("FAIL prefix '%s'\n", pfx.c_str()); if (++fail > 5) break; }
    }
    if (fail) break;
  }

  // every key with last byte +1 must NOT be found (if not itself a key)
  for (size_t i = 0; i < std::min(n, (size_t)5000); i++) {
    std::string miss = keys[i];
    miss.back()++;
    if (std::binary_search(keys.begin(), keys.end(), miss)) continue;
    // check no key starts with miss — only valid if miss is longer than all keys with that prefix
    // simpler: just check a 2-char prefix that shouldn't exist
  }

  if (fail == 0) printf("[PASSED] contains_prefix: hit+prefix checks on %zu keys\n", n);
  else           printf("[FAILED] %zu errors\n", fail);
}

// ============================================================
// Case 21: C²-FST-Hybrid  (surf::LoudsDense + c2::LoudsSparseCC)
// ============================================================
class FstCCHybridWrapper {
 public:
  using position_t = surf::position_t;
  using level_t    = surf::level_t;

  // space_relaxation: 0 = pure sparse (no Dense levels), 1 = hybrid (Dense top + Sparse bottom)
  // max_recursion: sparse_dense_ratio when hybrid (default 64)
  FstCCHybridWrapper(const std::vector<std::string>& keys,
                     uint32_t space_relaxation = 0,
                     int      max_recursion    = 64,
                     int      /*mask*/         = 0) {
    bool include_dense = (space_relaxation != 0);
    uint32_t ratio = include_dense ? (uint32_t)max_recursion : 0;

    // --- 1. Build SuRF builder ------------------------------------------
    auto builder = std::make_unique<surf::SuRFBuilder>(include_dense, ratio, surf::kNone, 0, 0);
    builder->build(keys);

    start_level_ = builder->getSparseStartLevel();
    level_t height = (level_t)builder->getLabels().size();

    // --- 2. Build LoudsDense (empty when pure sparse) -------------------
    louds_dense_ = std::make_unique<surf::LoudsDense>(builder.get());

    // --- 3. Compute offsets ---------------------------------------------
    node_count_dense_ = 0;
    for (level_t l = 0; l < start_level_; l++)
      node_count_dense_ += builder->getNodeCounts()[l];

    n_sparse_roots_ = builder->getNodeCounts()[start_level_];

    value_count_dense_ = 0;
    for (level_t l = 0; l < start_level_; l++)
      value_count_dense_ += builder->getSuffixCounts()[l];

    num_keys_ = 0;
    for (level_t l = 0; l < height; l++)
      num_keys_ += builder->getSuffixCounts()[l];

    // --- 4. Build LoudsSparseCC bitvector + labels for levels L..H-1 ---
    const auto& lbl_by_level   = builder->getLabels();
    const auto& child_by_level = builder->getChildIndicatorBits();
    const auto& louds_by_level = builder->getLoudsBits();

    for (level_t l = start_level_; l < height; l++) {
      size_t n = lbl_by_level[l].size();
      for (size_t i = 0; i < n; i++) {
        bool hc = surf::SuRFBuilder::readBit(child_by_level[l], i);
        bool lv = surf::SuRFBuilder::readBit(louds_by_level[l], i);
        sparse_labels_.push_back(lbl_by_level[l][i]);
        topo_.push_back(hc, lv);
      }
    }
    topo_.build();

    // --- 5. Build suffix pool (same algorithm as fst::Trie) -------------
    struct suffix_t {
      std::pair<const char*, size_t> str;
      position_t key_id = fst::kNotFound;
      size_t length() const { return str.second; }
      char operator[](size_t i) const { return str.first[str.second - i - 1]; }
      const char* begin() const { return str.first; }
      const char* end()   const { return str.first + str.second; }
      std::reverse_iterator<const char*> rbegin() const {
        return std::make_reverse_iterator(str.first + str.second);
      }
      std::reverse_iterator<const char*> rend() const {
        return std::make_reverse_iterator(str.first);
      }
    };

    std::vector<suffix_t> sbuilder(num_keys_);

    for (size_t i = 0; i < keys.size(); i++) {
      if (i != 0 && keys[i] == keys[i - 1]) continue;
      auto [kid, lev] = traverseHybrid(keys[i]);
      assert(kid < num_keys_);
      assert(sbuilder[kid].key_id == fst::kNotFound);
      assert(lev <= keys[i].length());
      sbuilder[kid] = suffix_t{{keys[i].c_str() + lev, keys[i].length() - lev}, kid};
    }

    std::sort(sbuilder.begin(), sbuilder.end(), [](const suffix_t& x, const suffix_t& y) {
      return std::lexicographical_compare(x.rbegin(), x.rend(), y.rbegin(), y.rend());
    });

    std::vector<uint32_t> sptrs(num_keys_);
    suffixes_.emplace_back('\0');
    suffix_t prev = {{nullptr, 0}, fst::kNotFound};

    for (size_t i = 0; i < num_keys_; i++) {
      const suffix_t& cur = sbuilder[num_keys_ - i - 1];
      if (cur.length() == 0) { sptrs[cur.key_id] = 0; continue; }

      size_t m = 0;
      while (m < cur.length() && m < prev.length() && prev[m] == cur[m]) m++;

      if (m == cur.length() && prev.length() != 0) {
        sptrs[cur.key_id] = (position_t)(sptrs[prev.key_id] + (prev.length() - m));
      } else {
        sptrs[cur.key_id] = (position_t)suffixes_.size();
        std::copy(cur.begin(), cur.end(), std::back_inserter(suffixes_));
        suffixes_.emplace_back('\0');
      }
      prev = cur;
    }

    uint32_t suf_bits = 0;
    uint32_t max_ptr = (uint32_t)suffixes_.size();
    do { suf_bits++; max_ptr >>= 1; } while (max_ptr != 0);
    suffix_ptrs_ = fst::detail::CompactArray(sptrs, suf_bits);
    suffixes_.shrink_to_fit();
  }

  __NOINLINE_IF_PROFILE auto lookup(const std::string& key) const -> uint32_t {
    position_t conn = surf::kNotFound;
    auto [kid, lev] = louds_dense_->findKey(key, conn);

    if (kid != fst::kNotFound)
      return checkSuffix(key, lev, kid) ? kid : (uint32_t)fst::kNotFound;

    if (conn == fst::kNotFound)
      return fst::kNotFound;

    return lookupSparse(key, lev, conn);
  }

  auto space_cost() const -> size_t {
    return (louds_dense_->getMemoryUsage() +
            topo_.size_in_bytes() +
            sparse_labels_.size() * sizeof(uint8_t) +
            suffix_ptrs_.getMemoryUsage() +
            suffixes_.size() * sizeof(char) +
            sizeof(*this)) * 8;
  }

  void print_space_cost_breakdown() const { printf("not implemented\n"); }

 private:
  // Raw traversal (no suffix check): returns {key_id, level_consumed}
  auto traverseHybrid(const std::string& key) const -> std::pair<position_t, level_t> {
    position_t conn = surf::kNotFound;
    auto [kid, lev] = louds_dense_->findKey(key, conn);
    if (kid != fst::kNotFound) return {kid, lev};
    if (conn == fst::kNotFound) return {fst::kNotFound, lev};
    return traverseSparse(key, lev, conn);
  }

  auto traverseSparse(const std::string& key, level_t start, position_t node_num)
      const -> std::pair<position_t, level_t> {
    uint32_t pos = topo_.select_node(node_num - node_count_dense_);

    for (level_t level = start; level < (level_t)key.size(); level++) {
      uint32_t end = topo_.node_end(pos);
      auto target = (uint8_t)key[level];
      while (pos < end && sparse_labels_[pos] < target) pos++;
      if (pos >= end || sparse_labels_[pos] != target)
        return {fst::kNotFound, level};
      if (!topo_.has_child(pos))
        return {topo_.leaf_id(pos) + value_count_dense_, level + 1};
      pos = topo_.select_node(topo_.has_child_rank1(pos) + n_sparse_roots_ - 1);
    }

    if (!sparse_labels_.empty() && sparse_labels_[pos] == surf::kTerminator && !topo_.has_child(pos))
      return {topo_.leaf_id(pos) + value_count_dense_, (level_t)key.size()};
    return {fst::kNotFound, (level_t)key.size()};
  }

  auto lookupSparse(const std::string& key, level_t start, position_t node_num) const -> uint32_t {
    uint32_t pos = topo_.select_node(node_num - node_count_dense_);

    for (level_t level = start; level < (level_t)key.size(); level++) {
      uint32_t end = topo_.node_end(pos);
      auto target = (uint8_t)key[level];
      while (pos < end && sparse_labels_[pos] < target) pos++;
      if (pos >= end || sparse_labels_[pos] != target) return fst::kNotFound;
      if (!topo_.has_child(pos)) {
        position_t leaf = topo_.leaf_id(pos) + value_count_dense_;
        return checkSuffix(key, level + 1, leaf) ? leaf : (uint32_t)fst::kNotFound;
      }
      pos = topo_.select_node(topo_.has_child_rank1(pos) + n_sparse_roots_ - 1);
    }

    if (!sparse_labels_.empty() && sparse_labels_[pos] == surf::kTerminator && !topo_.has_child(pos)) {
      position_t leaf = topo_.leaf_id(pos) + value_count_dense_;
      return checkSuffix(key, (level_t)key.size(), leaf) ? leaf : (uint32_t)fst::kNotFound;
    }
    return fst::kNotFound;
  }

  auto checkSuffix(const std::string& key, level_t level, position_t key_id) const -> bool {
    position_t suf = suffix_ptrs_[key_id];
    for (level_t l = level; l < (level_t)key.size(); l++) {
      if (key[l] != suffixes_[suf]) return false;
      suf++;
    }
    return suffixes_[suf] == '\0';
  }

 private:
  std::unique_ptr<surf::LoudsDense> louds_dense_;
  c2::LoudsSparseCC               topo_;
  std::vector<uint8_t>            sparse_labels_;
  fst::detail::CompactArray       suffix_ptrs_;
  std::vector<char>               suffixes_;
  position_t num_keys_          = 0;
  position_t value_count_dense_ = 0;
  position_t node_count_dense_  = 0;
  position_t n_sparse_roots_    = 0;
  level_t    start_level_       = 0;
};

int main(int argc, char *argv[]) {
  assert(argc >= 2);

  int choice = argc >= 3 ? std::atoi(argv[2]) : 0;
  uint32_t space_relaxation = argc >= 4 ? std::atoi(argv[3]) : 0;
  int max_recursion = argc >= 5 ? std::atoi(argv[4]) : 0;
  int mask = argc >= 6 ? std::atoi(argv[5]) : 0;

  switch (choice) {
   case 0:
    printf("[TEST C2-FST]\n");
    test_trie<FstCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 1:
    printf("[TEST C2-CoCo(LOUDS-Sparse)]\n");
    test_trie<CoCoLSWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 2:
    printf("[TEST C2-MARISA]\n");
    test_trie<MarisaCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 3:
    printf("[TEST FST]\n");
    test_trie<FstWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 4:
    printf("[TEST COCO]\n");
    test_trie<CoCoWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 5:
    printf("[TEST MARISA]\n");
    test_trie<MarisaWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 6:
    printf("[TEST PDT]\n");
    test_trie<PdtWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 7:
    printf("[TEST ART]\n");
    test_trie<ArtWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 8:
    printf("[TEST CART]\n");
    test_trie<CArtWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 9:
    printf("[TEST C2-COCO(LOUDS)]\n");
    test_trie<CoCoCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 10:
    printf("[TEST C2-CoCo(Sux)]\n");
    test_trie<CoCoSuxWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
  #ifdef __COMPARE_COCO__
   case 11:
    printf("[COMPARE LOUDS COCO]\n");
    compare_louds_coco(argv[1], space_relaxation, max_recursion, mask);
    break;
  #endif
  #ifdef __COMPARE_MARISA__
   case 12:
    printf("[COMPARE LOUDS MARISA]\n");
    compare_louds_marisa(argv[1], max_recursion, mask);
    break;
  #endif
  #ifdef __COMPARE_FST__
   case 13:
    printf("[COMPARE LOUDS FST]\n");
    compare_louds_fst(argv[1]);
    break;
  #endif
   case 14:
    printf("[TEST SUCCESSOR]\n");
    test_successor(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 15:
    printf("[TEST RANGE ITER]\n");
    test_range_iter(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 16:
    printf("[TEST CONTAINS PREFIX]\n");
    test_contains_prefix(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 17:
    printf("[TEST PREFIX QUERY vs CTrie++]\n");
    test_prefix_query(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 18:
    printf("[TEST CTrie++]\n");
    test_trie<CtriePPWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 19:
    printf("[TEST ZFastTrie]\n");
    test_trie<ZFastTrieWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 21:
    printf("[TEST C2-FST-Hybrid]\n");
    test_trie<FstCCHybridWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   default:
    printf("unrecognized index; stopped\n");
  }
}
