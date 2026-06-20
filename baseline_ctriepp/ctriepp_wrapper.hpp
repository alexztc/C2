#pragma once

#include "../include/utils.hpp"

#include <malloc.h>
#include <vector>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "CTriePP.hpp"
#pragma GCC diagnostic pop

class CtriePPWrapper {
 public:
  using trie_t = ctriepp::CTriePP<uint32_t>;

  CtriePPWrapper(const std::vector<std::string> &keys, uint32_t = 0, int = 0, int = 0) {
    auto before = mallinfo2();
    uint32_t id = 0;
    for (const auto &k : keys)
      trie_.insert(&k, id++);
    auto after = mallinfo2();
    // uordblks: bytes currently allocated (in-use chunks); captures trie heap footprint
    size_t heap_bytes = (after.uordblks > before.uordblks)
                        ? (after.uordblks - before.uordblks)
                        : 0;
    space_bits_ = heap_bytes * 8;
  }

  auto lookup(const std::string &key) const -> uint32_t {
    return trie_.contains(key) ? 0 : (uint32_t)-1;
  }

  auto contains_prefix(const std::string &q) const -> bool {
    return trie_.containsPrefix(q);
  }

  auto space_cost() const -> size_t { return space_bits_; }

  void print_space_cost_breakdown() const {
    printf("heap (mallinfo2): %.3f MB\n", (double)space_bits_ / (8.0 * 1024 * 1024));
  }

  static void print_bench() {}

 private:
  trie_t trie_;
  size_t space_bits_ = 0;
};
