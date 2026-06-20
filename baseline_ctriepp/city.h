#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// Minimal CityHash64 stub using FNV-1a for benchmarking purposes.
// Only needed by str_hash which is used for TSL htrie_map, not CTriePP.
inline uint64_t CityHash64(const char* s, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}
