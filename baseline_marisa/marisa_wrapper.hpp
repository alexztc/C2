#pragma once

#include "include/marisa.h"
#include "../include/utils.hpp"

#include <string>
#include <vector>
#include <algorithm>


class MarisaWrapper {  // unified API
 public:
  using trie_t = marisa::Trie;

  __NOINLINE_IF_PROFILE MarisaWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                      int max_recursion = 0, int mask = 0)
                                      : sorted_keys_(keys) {
    marisa::Keyset keyset;
    for (const auto &key : keys) {
      keyset.push_back(key.c_str());
    }
    trie_.build(keyset, (max_recursion + 1) | marisa::CacheLevel::MARISA_HUGE_CACHE);
  }

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    marisa::Agent agent;
    agent.set_query(key.c_str());
    if (trie_.lookup(agent)) {
      return agent.key().id();
    }
    return -1;
  }

  // External sorted-key binary search (Marisa has no ordered forward iterator)
  auto successor(const std::string &key) const -> std::string {
    auto it = std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), key);
    return it != sorted_keys_.end() ? *it : "";
  }

  auto range_count(const std::string &lo, const std::string &hi) const -> uint32_t {
    return uint32_t(std::upper_bound(sorted_keys_.begin(), sorted_keys_.end(), hi)
                  - std::lower_bound(sorted_keys_.begin(), sorted_keys_.end(), lo));
  }

  auto space_cost() const -> size_t {
    return trie_.total_size() * 8;
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
