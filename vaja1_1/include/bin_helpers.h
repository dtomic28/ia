#pragma once

#include <cstdint>

constexpr uint64_t minNBit(unsigned n) { return 1ull << (n - 1); }      // n in 1..64
constexpr uint64_t maxNBit(unsigned n) { return ~0ull >> (64 - n); }    // n in 1..64