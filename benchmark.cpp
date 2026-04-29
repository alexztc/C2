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

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <chrono>
#include <random>
#include <unordered_set>


// #define __CORRECTNESS_TEST__


class FstCCWrapper {  // unified API
 public:
  using trie_t = c2::FstCC<std::string>;

  __NOINLINE_IF_PROFILE FstCCWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                     int max_recursion = 0, int mask = 0)
                                     : sorted_keys_(keys) {
    trie_.build(keys.begin(), keys.end(), true, max_recursion, mask);
  }

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto successor(const std::string &key) const -> std::string {
    auto it = std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), key);
    return it != sorted_keys_.end() ? *it : "";
  }

  auto range_count(const std::string &lo, const std::string &hi) const -> uint32_t {
    return uint32_t(std::upper_bound(sorted_keys_.begin(), sorted_keys_.end(), hi)
                  - std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), lo));
  }

  auto prefix_count(const std::string &prefix) const -> uint32_t {
    return trie_.prefix_count(prefix);
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  auto sorted_keys_bits() const -> size_t {
    size_t bytes = sorted_keys_.capacity() * sizeof(std::string);
    for (const auto& s : sorted_keys_)
      if (s.size() > 15) bytes += s.size() + 1;
    return bytes * 8;
  }

  auto total_space_cost() const -> size_t { return space_cost() + sorted_keys_bits(); }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }
 private:
  trie_t trie_;
  std::vector<std::string> sorted_keys_;
};

class CoCoCCWrapper {  // unified API
 public:
  using trie_t = c2::CoCoCC<std::string>;

  __NOINLINE_IF_PROFILE CoCoCCWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                      int max_recursion = 0, int mask = 0)
                                      : trie_(keys.begin(), keys.end(), true, space_relaxation, max_recursion, mask),
                                        sorted_keys_(keys) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  // CoCoCCWrapper (LoudsCC topology) uses binary search; not in successor benchmark cases
  auto successor(const std::string &key) const -> std::string {
    auto it = std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), key);
    return it != sorted_keys_.end() ? *it : "";
  }

  auto range_count(const std::string &lo, const std::string &hi) const -> uint32_t {
    return uint32_t(std::upper_bound(sorted_keys_.begin(), sorted_keys_.end(), hi)
                  - std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), lo));
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  auto sorted_keys_bits() const -> size_t { return 0; }
  auto total_space_cost() const -> size_t { return space_cost(); }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }
 private:
  trie_t trie_;
  std::vector<std::string> sorted_keys_;
};

class CoCoLSWrapper {  // unified API
 public:
  using trie_t = c2::CoCoCC<std::string, c2::LoudsSparseCC>;

  __NOINLINE_IF_PROFILE CoCoLSWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                      int max_recursion = 0, int mask = 0)
                                      : trie_(keys.begin(), keys.end(), true, space_relaxation, max_recursion, mask),
                                        sorted_keys_(keys) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto successor(const std::string &key) const -> std::string {
    auto it = std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), key);
    return it != sorted_keys_.end() ? *it : "";
  }

  auto range_count(const std::string &lo, const std::string &hi) const -> uint32_t {
    return uint32_t(std::upper_bound(sorted_keys_.begin(), sorted_keys_.end(), hi)
                  - std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), lo));
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  auto sorted_keys_bits() const -> size_t {
    size_t bytes = sorted_keys_.capacity() * sizeof(std::string);
    for (const auto& s : sorted_keys_)
      if (s.size() > 15) bytes += s.size() + 1;
    return bytes * 8;
  }

  auto total_space_cost() const -> size_t { return space_cost() + sorted_keys_bits(); }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }
 private:
  trie_t trie_;
  std::vector<std::string> sorted_keys_;
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

  auto sorted_keys_bits() const -> size_t { return 0; }
  auto total_space_cost() const -> size_t { return space_cost(); }

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
                                        int max_recursion = 0, int mask = 0)
                                        : sorted_keys_(keys) {
    trie_.build(keys.begin(), keys.end(), true, max_recursion, mask);
  }

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.lookup(key);
  }

  auto successor(const std::string &key) const -> std::string {
    auto it = std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), key);
    return it != sorted_keys_.end() ? *it : "";
  }

  auto range_count(const std::string &lo, const std::string &hi) const -> uint32_t {
    return uint32_t(std::upper_bound(sorted_keys_.begin(), sorted_keys_.end(), hi)
                  - std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), lo));
  }

  auto space_cost() const -> size_t {
    return trie_.size_in_bits();
  }

  auto sorted_keys_bits() const -> size_t {
    size_t bytes = sorted_keys_.capacity() * sizeof(std::string);
    for (const auto& s : sorted_keys_)
      if (s.size() > 15) bytes += s.size() + 1;
    return bytes * 8;
  }

  auto total_space_cost() const -> size_t { return space_cost() + sorted_keys_bits(); }

  void print_space_cost_breakdown() const {
    trie_.print_space_cost_breakdown();
  }
 private:
  trie_t trie_;
  std::vector<std::string> sorted_keys_;
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
  std::shuffle(keys.begin(), keys.end(), std::mt19937{2});
  printf("Querying trie...\n");
  start = std::chrono::high_resolution_clock::now();
  query_trie<trie_t>(keys, trie);
  end = std::chrono::high_resolution_clock::now();
  duration = (end - start).count();
  double avg_latency = (double)duration/keys.size();
  printf("Done!\n");
  printf("total time: %lf ms, avg latency: %lf ns\n", (double)duration/1000000, avg_latency);
  trie.print_space_cost_breakdown();

  printf("%lf,%lf,%lf\n", build_time, size_in_mb, avg_latency);
  printf("[PASSED]\n");
}

template <typename trie_t>
void __attribute__((noinline)) query_trie_successor(const std::vector<std::string> &queries,
                                                     const std::vector<std::string> &expected,
                                                     const trie_t &trie) {
  for (uint32_t i = 0; i < queries.size(); i++) {
    volatile auto result = trie.successor(queries[i]);
  #ifdef __CORRECTNESS_TEST__
    std::string r = const_cast<const std::string &>(result);
    if (r != expected[i]) {
      printf("FAIL[%u]: query=%s expected=%s got=%s\n", i, queries[i].c_str(), expected[i].c_str(), r.c_str());
    }
    EXPECT(r == expected[i]);
  #endif
  }
}

template <typename trie_t>
void __attribute__((noinline)) query_trie_range(const std::vector<std::string> &lo_q,
                                                 const std::vector<std::string> &hi_q,
                                                 const std::vector<uint32_t> &expected_counts,
                                                 const trie_t &trie) {
  for (uint32_t i = 0; i < lo_q.size(); i++) {
    volatile uint32_t result = trie.range_count(lo_q[i], hi_q[i]);
  #ifdef __CORRECTNESS_TEST__
    if (result != expected_counts[i]) {
      printf("FAIL[%u]: [%s,%s] expected=%u got=%u\n", i,
             lo_q[i].c_str(), hi_q[i].c_str(), expected_counts[i], uint32_t(result));
    }
    EXPECT(result == expected_counts[i]);
  #endif
  }
}

template <typename trie_t>
void __attribute__((noinline)) test_trie_successor(const char *filename, uint32_t space_relaxation,
                                                    int max_recursion, int mask) {
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
  double build_time = (double)(end - start).count()/1000000;
  printf("Done!\n");

  size_t trie_bits = trie.space_cost();
  size_t keys_bits = trie.sorted_keys_bits();
  size_t total_bits = trie.total_space_cost();
  double size_in_mb = (double)total_bits/c2::mb_bits;
  printf("index space: %.6lf MB (trie: %.6lf MB + keys overhead: %.6lf MB)\n",
         size_in_mb, (double)trie_bits/c2::mb_bits, (double)keys_bits/c2::mb_bits);

  // Generate queries: drop last char of each key; expected successor = original key
  std::vector<std::string> queries, expected;
  queries.reserve(keys.size());
  expected.reserve(keys.size());
  for (const auto &k : keys) {
    queries.push_back(k.size() > 0 ? k.substr(0, k.size() - 1) : k);
    // Find actual expected value from sorted keys
    auto it = std::lower_bound(keys.begin(), keys.end(), queries.back());
    expected.push_back(it != keys.end() ? *it : "");
  }

  // Shuffle queries and expected together via index permutation
  std::vector<uint32_t> perm(queries.size());
  std::iota(perm.begin(), perm.end(), 0);
  std::shuffle(perm.begin(), perm.end(), std::mt19937{2});
  std::vector<std::string> sq, se;
  sq.reserve(queries.size()); se.reserve(queries.size());
  for (auto idx : perm) { sq.push_back(queries[idx]); se.push_back(expected[idx]); }
  queries = std::move(sq); expected = std::move(se);

  printf("Querying successor...\n");
  start = std::chrono::high_resolution_clock::now();
  query_trie_successor<trie_t>(queries, expected, trie);
  end = std::chrono::high_resolution_clock::now();
  auto duration = (end - start).count();
  double avg_latency = (double)duration/queries.size();
  printf("Done!\n");
  printf("total time: %lf ms, avg latency: %lf ns\n", (double)duration/1000000, avg_latency);
  trie.print_space_cost_breakdown();
  printf("%lf,%lf,%lf\n", build_time, size_in_mb, avg_latency);
  printf("[PASSED]\n");
}

template <typename trie_t>
void __attribute__((noinline)) test_trie_range(const char *filename, uint32_t space_relaxation,
                                                int max_recursion, int mask) {
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
  double build_time = (double)(end - start).count()/1000000;
  printf("Done!\n");

  size_t trie_bits = trie.space_cost();
  size_t keys_bits = trie.sorted_keys_bits();
  size_t total_bits = trie.total_space_cost();
  double size_in_mb = (double)total_bits/c2::mb_bits;
  printf("index space: %.6lf MB (trie: %.6lf MB + keys overhead: %.6lf MB)\n",
         size_in_mb, (double)trie_bits/c2::mb_bits, (double)keys_bits/c2::mb_bits);

  // Generate (lo, hi) range pairs with random range sizes (avg ~n/10)
  uint32_t n = keys.size();
  std::mt19937 rng{42};
  std::vector<std::string> lo_q, hi_q;
  std::vector<uint32_t> expected_counts;
  lo_q.reserve(n); hi_q.reserve(n); expected_counts.reserve(n);
  for (uint32_t q = 0; q < n; q++) {
    uint32_t i = rng() % n;
    uint32_t range_size = (rng() % (n / 10 + 1)) + 1;
    uint32_t j = std::min(i + range_size, n - 1);
    lo_q.push_back(keys[i]);
    hi_q.push_back(keys[j]);
    expected_counts.push_back(j - i + 1);
  }

  // Shuffle all three vectors by the same permutation
  std::vector<uint32_t> perm(n);
  std::iota(perm.begin(), perm.end(), 0);
  std::shuffle(perm.begin(), perm.end(), std::mt19937{2});
  std::vector<std::string> slo, shi; std::vector<uint32_t> sc;
  slo.reserve(n); shi.reserve(n); sc.reserve(n);
  for (auto idx : perm) { slo.push_back(lo_q[idx]); shi.push_back(hi_q[idx]); sc.push_back(expected_counts[idx]); }
  lo_q = std::move(slo); hi_q = std::move(shi); expected_counts = std::move(sc);

  printf("Querying range_count...\n");
  start = std::chrono::high_resolution_clock::now();
  query_trie_range<trie_t>(lo_q, hi_q, expected_counts, trie);
  end = std::chrono::high_resolution_clock::now();
  auto duration = (end - start).count();
  double avg_latency = (double)duration/n;
  printf("Done!\n");
  printf("total time: %lf ms, avg latency: %lf ns\n", (double)duration/1000000, avg_latency);
  trie.print_space_cost_breakdown();
  printf("%lf,%lf,%lf\n", build_time, size_in_mb, avg_latency);
  printf("[PASSED]\n");
}

template <typename trie_t>
void __attribute__((noinline)) query_trie_prefix(const std::vector<std::string> &prefix_q,
                                                  const std::vector<uint32_t> &expected_counts,
                                                  const trie_t &trie) {
  for (uint32_t i = 0; i < prefix_q.size(); i++) {
    volatile uint32_t result = trie.prefix_count(prefix_q[i]);
  #ifdef __CORRECTNESS_TEST__
    if (result != expected_counts[i]) {
      printf("FAIL[%u]: prefix=%s expected=%u got=%u\n", i,
             prefix_q[i].c_str(), expected_counts[i], uint32_t(result));
    }
    EXPECT(result == expected_counts[i]);
  #endif
  }
}

template <typename trie_t>
void __attribute__((noinline)) test_trie_prefix(const char *filename, uint32_t space_relaxation,
                                                 int max_recursion, int mask) {
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
  double build_time = (double)(end - start).count()/1000000;
  printf("Done!\n");

  size_t trie_bits = trie.space_cost();
  size_t keys_bits = trie.sorted_keys_bits();
  size_t total_bits = trie.total_space_cost();
  double size_in_mb = (double)total_bits/c2::mb_bits;
  printf("index space: %.6lf MB (trie: %.6lf MB + keys overhead: %.6lf MB)\n",
         size_in_mb, (double)trie_bits/c2::mb_bits, (double)keys_bits/c2::mb_bits);

  // Generate prefix queries: use near-full-length prefixes (key length - 0 to 3 chars removed)
  // so matched subtries are small (typically 1-20 keys), keeping DFS cost bounded
  // while still exercising rank/select via getChildNodeNum / child_pos per subtrie node.
  uint32_t n = keys.size();
  std::mt19937 rng{42};
  std::vector<std::string> prefix_q;
  std::vector<uint32_t> expected_counts;
  prefix_q.reserve(n); expected_counts.reserve(n);
  for (uint32_t q = 0; q < n; q++) {
    uint32_t i = rng() % n;
    uint32_t trim = rng() % std::min<uint32_t>(4, keys[i].size());
    uint32_t plen = keys[i].size() - trim;
    std::string prefix = keys[i].substr(0, plen);
    prefix_q.push_back(prefix);
    // Ground truth via binary search on sorted keys
    auto lo_it = std::lower_bound(keys.begin(), keys.end(), prefix);
    std::string hi_prefix = prefix;
    while (!hi_prefix.empty() && (uint8_t)hi_prefix.back() == 0xFF) hi_prefix.pop_back();
    uint32_t cnt;
    if (hi_prefix.empty()) {
      cnt = uint32_t(keys.end() - lo_it);
    } else {
      hi_prefix.back()++;
      auto hi_it = std::lower_bound(keys.begin(), keys.end(), hi_prefix);
      cnt = uint32_t(hi_it - lo_it);
    }
    expected_counts.push_back(cnt);
  }

  // Shuffle
  std::vector<uint32_t> perm(n);
  std::iota(perm.begin(), perm.end(), 0);
  std::shuffle(perm.begin(), perm.end(), std::mt19937{2});
  std::vector<std::string> sp; std::vector<uint32_t> sc;
  sp.reserve(n); sc.reserve(n);
  for (auto idx : perm) { sp.push_back(prefix_q[idx]); sc.push_back(expected_counts[idx]); }
  prefix_q = std::move(sp); expected_counts = std::move(sc);

  printf("Querying prefix_count...\n");
  start = std::chrono::high_resolution_clock::now();
  query_trie_prefix<trie_t>(prefix_q, expected_counts, trie);
  end = std::chrono::high_resolution_clock::now();
  auto duration = (end - start).count();
  double avg_latency = (double)duration/n;
  printf("Done!\n");
  printf("total time: %lf ms, avg latency: %lf ns\n", (double)duration/1000000, avg_latency);
  trie.print_space_cost_breakdown();
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

  std::shuffle(leaves[0].begin(), leaves[0].end(), std::mt19937{1});
  std::shuffle(leaves[1].begin(), leaves[1].end(), std::mt19937{1});
  std::shuffle(internals[0].begin(), internals[0].end(), std::mt19937{2});
  std::shuffle(internals[1].begin(), internals[1].end(), std::mt19937{2});
  std::shuffle(child_query[0].begin(), child_query[0].end(), std::mt19937{3});
  std::shuffle(child_query[1].begin(), child_query[1].end(), std::mt19937{3});

  size_t leaf_id_time[3], internal_id_time[3], degree_time[3], child_pos_time[3];

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
   // Successor queries
   case 13:
    printf("[SUCCESSOR C2-FST]\n");
    test_trie_successor<FstCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 14:
    printf("[SUCCESSOR C2-CoCo(LOUDS-Sparse)]\n");
    test_trie_successor<CoCoLSWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 15:
    printf("[SUCCESSOR C2-MARISA]\n");
    test_trie_successor<MarisaCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 16:
    printf("[SUCCESSOR FST]\n");
    test_trie_successor<FstWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 17:
    printf("[SUCCESSOR COCO (sorted-keys binary search)]\n");
    test_trie_successor<CoCoWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 18:
    printf("[SUCCESSOR MARISA (sorted-keys binary search)]\n");
    test_trie_successor<MarisaWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   // Range count queries
   case 19:
    printf("[RANGE_COUNT C2-FST]\n");
    test_trie_range<FstCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 20:
    printf("[RANGE_COUNT C2-CoCo(LOUDS-Sparse)]\n");
    test_trie_range<CoCoLSWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 21:
    printf("[RANGE_COUNT C2-MARISA]\n");
    test_trie_range<MarisaCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 22:
    printf("[RANGE_COUNT FST]\n");
    test_trie_range<FstWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 23:
    printf("[RANGE_COUNT COCO (sorted-keys binary search)]\n");
    test_trie_range<CoCoWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 24:
    printf("[RANGE_COUNT MARISA (sorted-keys binary search)]\n");
    test_trie_range<MarisaWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 25:
    printf("[RANGE_COUNT C2-FST (sorted-keys binary search)]\n");
    test_trie_range<FstCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 26:
    printf("[RANGE_COUNT FST (sorted-keys binary search)]\n");
    test_trie_range<FstWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 27:
    printf("[PREFIX_COUNT C2-FST]\n");
    test_trie_prefix<FstCCWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   case 28:
    printf("[PREFIX_COUNT FST]\n");
    test_trie_prefix<FstWrapper>(argv[1], space_relaxation, max_recursion, mask);
    break;
   default:
    printf("unrecognized index; stopped\n");
  }
}
