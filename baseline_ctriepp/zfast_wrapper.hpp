#pragma once

#include <vector>
#include <string>
#include <cstdint>

class ZFastTrieWrapper {
 public:
  ZFastTrieWrapper(const std::vector<std::string> &keys, uint32_t = 0, int = 0, int = 0);
  ~ZFastTrieWrapper();

  auto lookup(const std::string &key) const -> uint32_t;
  auto contains_prefix(const std::string &q) const -> bool;
  auto space_cost() const -> size_t;
  void print_space_cost_breakdown() const;
  static void print_bench() {}

 private:
  struct Impl;
  Impl *impl_;
};
