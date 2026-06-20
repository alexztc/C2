#pragma once

#include "include/fst.hpp"
#include "../include/utils.hpp"

#include <vector>
#include <string>


class FstWrapper {  // unified API
 public:
  using trie_t = fst::Trie;

  __NOINLINE_IF_PROFILE FstWrapper(const std::vector<std::string> &keys, uint32_t space_relaxation = 0,
                                   int max_recursion = 0, int mask = 0) : trie_(keys, space_relaxation, max_recursion) {}

  __NOINLINE_IF_PROFILE auto lookup(const std::string &key) const -> uint32_t {
    return trie_.exactSearch(key);
  }

  auto successor(const std::string &key) const -> int32_t {
    return trie_.successor_key(key);
  }

  // Iterator-based range query: iterates keys in [l, r).
  struct RangeIter {
    surf::LoudsSparse::Iter iter_;
    std::string end_key_;
    bool valid_ = false;

    bool valid()            const { return valid_; }
    std::string key()       const { return iter_.getKey(); }

    bool next() {
      iter_.operator++(0);
      valid_ = iter_.isValid() && (iter_.compare(end_key_) < 0);
      return valid_;
    }
  };

  auto lower_bound(const std::string& l, const std::string& r) const -> RangeIter {
    RangeIter rit;
    rit.end_key_ = r;
    auto *ls = const_cast<surf::LoudsSparse*>(trie_.get_louds_sparse());
    surf::LoudsSparse::Iter iter(ls);
    ls->moveToKeyGreaterThan(l, /*inclusive=*/true, iter);
    rit.iter_  = iter;
    rit.valid_ = iter.isValid() && (iter.compare(r) < 0);
    return rit;
  }

  auto range_count_iter(const std::string& l, const std::string& r) const -> int32_t {
    auto it = lower_bound(l, r);
    int32_t cnt = 0;
    while (it.valid()) { cnt++; it.next(); }
    return cnt;
  }

  auto space_cost() const -> size_t {
    return trie_.getMemoryUsage() * 8;
  }

  static void print_bench() {
    printf("not implemented\n");
  }

  void print_space_cost_breakdown() const {
    printf("not implemented\n");
  }
 private:
  trie_t trie_;
};