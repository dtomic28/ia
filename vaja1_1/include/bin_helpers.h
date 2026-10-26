#pragma once

#include <boost/multiprecision/cpp_int.hpp>

// Boost only goes up to uint1024_t, these are defined the same way
using uint2048_t = boost::multiprecision::number<
    boost::multiprecision::cpp_int_backend<2048, 2048, boost::multiprecision::unsigned_magnitude,
                                           boost::multiprecision::unchecked, void>>;
using uint4096_t = boost::multiprecision::number<
    boost::multiprecision::cpp_int_backend<4096, 4096, boost::multiprecision::unsigned_magnitude,
                                           boost::multiprecision::unchecked, void>>;

inline uint2048_t minNBit(unsigned n) { return uint2048_t(1) << (n - 1); }      // n in 1..2048
inline uint2048_t maxNBit(unsigned n) { return ~uint2048_t(0) >> (2048 - n); }  // n in 1..2048
