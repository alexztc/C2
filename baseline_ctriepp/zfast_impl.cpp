// Compiled in isolation: ZFastTrie headers conflict with CTriePP headers
// (both define global symbols in any.hpp without namespaces).
#include "zfast_wrapper.hpp"

#include <malloc.h>
#include <cstdio>
#include <iostream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "ZFastTrie.hpp"
#pragma GCC diagnostic pop

struct ZFastTrieWrapper::Impl {
  ZFastTrie<int> trie;
  size_t space_bits = 0;
};

ZFastTrieWrapper::ZFastTrieWrapper(const std::vector<std::string> &keys,
                                   uint32_t, int, int) {
  impl_ = new Impl();
  auto before = mallinfo2();
  int id = 0;
  for (const auto &k : keys)
    impl_->trie.insert(&k, id++);
  auto after = mallinfo2();
  size_t heap_bytes = (after.uordblks > before.uordblks)
                      ? (after.uordblks - before.uordblks)
                      : 0;
  impl_->space_bits = heap_bytes * 8;
}

ZFastTrieWrapper::~ZFastTrieWrapper() { delete impl_; }

auto ZFastTrieWrapper::lookup(const std::string &key) const -> uint32_t {
  return impl_->trie.contains(key) ? 0 : (uint32_t)-1;
}

auto ZFastTrieWrapper::contains_prefix(const std::string &q) const -> bool {
  return impl_->trie.containsPrefix(q);
}

auto ZFastTrieWrapper::space_cost() const -> size_t {
  return impl_->space_bits;
}

void ZFastTrieWrapper::print_space_cost_breakdown() const {
  printf("heap (mallinfo2): %.3f MB\n",
         (double)impl_->space_bits / (8.0 * 1024 * 1024));
}
