#pragma once

#define BIG_ALPHABET

#include "include/utils.hpp"
#include "include/uncompacted_trie.hpp"
#include "include/CoCo-trie_v2.hpp"
#include "../include/utils.hpp"

#include <vector>
#include <string>
#include <algorithm>


class CoCoWrapper {  // unified API
 public:
  using trie_t = CoCo_v2<1, uint128_t, MAX_L_THRS, 5>;

  __NOINLINE_IF_PROFILE CoCoWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                    int max_recursion = 0, int mask = 0)
                                    : trie_([&keys]() {
                                        datasetStats ds = dataset_stats_from_vector(keys);
                                        MIN_CHAR = ds.get_min_char();
                                        ALPHABET_SIZE = ds.get_alphabet_size();
                                        return trie_t(keys);
                                      }()), sorted_keys_(keys) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.look_up(key);
  }

  // External sorted-key binary search (CoCo-trie has no ordered iterator)
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

  static void print_bench() {
    printf("not implemented\n");
  }

  void print_space_cost_breakdown() const {
    printf("not implemented\n");
  }
 private:
  trie_t trie_;
  std::vector<std::string> sorted_keys_;
};
