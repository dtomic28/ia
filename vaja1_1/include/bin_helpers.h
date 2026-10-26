#pragma once

#include <boost/multiprecision/cpp_int.hpp>

// Arbitrary precision, so products like p * q never overflow
using boost::multiprecision::cpp_int;

inline cpp_int minNBit(unsigned n) { return cpp_int(1) << (n - 1); }        // n >= 1
inline cpp_int maxNBit(unsigned n) { return (cpp_int(1) << n) - 1; }        // n >= 1
