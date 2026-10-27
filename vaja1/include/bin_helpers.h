#pragma once

// Arbitrary precision, so products like p * q never overflow.
// With USE_GMP the same code runs on the GMP backend instead, so the two can be benchmarked against each other.
#ifdef USE_GMP
#include <boost/multiprecision/gmp.hpp>
using cpp_int = boost::multiprecision::mpz_int;
#else
#include <boost/multiprecision/cpp_int.hpp>
using boost::multiprecision::cpp_int;
#endif

inline cpp_int minNBit(unsigned n) { return cpp_int(1) << (n - 1); }        // n >= 1
inline cpp_int maxNBit(unsigned n) { return (cpp_int(1) << n) - 1; }        // n >= 1
