#pragma once

#include "include/fst.hpp"
#include "../include/utils.hpp"

#include <vector>
#include <string>
#include <algorithm>


class FstWrapper {  // unified API
 public:
  using trie_t = fst::Trie;

  __NOINLINE_IF_PROFILE FstWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                   int max_recursion = 0, int mask = 0)
                                   : trie_(keys, space_relaxation, max_recursion), sorted_keys_(keys) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.exactSearch(key);
  }

  // External sorted-key binary search (FST has no public ordered iterator)
  auto successor(const std::string &key) const -> std::string {
    auto it = std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), key);
    return it != sorted_keys_.end() ? *it : "";
  }

  auto range_count(const std::string &lo, const std::string &hi) const -> uint32_t {
    return uint32_t(std::upper_bound(sorted_keys_.begin(), sorted_keys_.end(), hi)
                  - std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), lo));
  }

  auto prefix_count(const std::string &prefix) const -> uint32_t {
    return trie_.prefixCount(prefix);
  }

  auto space_cost() const -> size_t {
    return trie_.getMemoryUsage() * 8;
  }

  // Memory used by sorted_keys_ (required for ordered queries; not part of trie index)
  auto sorted_keys_bits() const -> size_t {
    size_t bytes = sorted_keys_.capacity() * sizeof(std::string);
    for (const auto& s : sorted_keys_)
      if (s.size() > 15) bytes += s.size() + 1;  // non-SSO heap (GCC SSO threshold = 15)
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
