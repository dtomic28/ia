#pragma once

#include <cstdint>

#include <boost/multiprecision/cpp_int.hpp>

#include "lcg.h"
#include "bin_helpers.h"

inline LCG SuperDuper{1ull << 32, 69069, 1, 0};

inline constexpr uint64_t defaultMillerRabinRounds = 20;
inline constexpr unsigned maxPrimeBits = 2048;

/**
 * @details Super-Duper only gives 32 bits per step, so enough steps are glued together to cover the range.
 * For ranges up to 32 bits this uses a single step, same as LCG::getRandom.
 */
inline cpp_int getRandomBig(LCG& lcg, const cpp_int& low, const cpp_int& high) {
    const cpp_int range = high - low + 1;
    const unsigned bits = msb(range) + 1;

    cpp_int r = 0;
    for (unsigned i = 0; i < bits; i += 32)
        r = (r << 32) | (lcg.calculateNextR() & 0xFFFFFFFF);

    return low + r % range;
}

inline bool isPrimeNaive(const cpp_int& p) {
    if (p < 2)
        return false;
    if (p % 2 == 0)
        return p == 2;

    const cpp_int limit = sqrt(p);
    cpp_int j = 3;
    while (j <= limit) {
        if (p % j == 0)
            return false;
        j += 2;
    }
    return true;
}

inline cpp_int generatePrimeNaive(unsigned numOfBits) {
    cpp_int p = getRandomBig(SuperDuper, minNBit(numOfBits), maxNBit(numOfBits));
    if (p % 2 == 0)
        p++;

    while (!isPrimeNaive(p))
        p += 2;
    return p;
}

inline cpp_int multiplyModulo(const cpp_int& a, const cpp_int& b, const cpp_int& modulus) {
    return a * b % modulus;
}

inline cpp_int modularExponentiation(const cpp_int& base, const cpp_int& exponent, const cpp_int& modulus) {
    cpp_int result = 1;
    if (exponent == 0)
        return result;

    for (int bit = static_cast<int>(msb(exponent)); bit >= 0; bit--) {
        result = multiplyModulo(result, result, modulus);
        if (bit_test(exponent, bit))
            result = multiplyModulo(result, base, modulus);
    }
    return result;
}

enum class MRResult {
    prime,
    composite,
    probablyPrime
};

/**
 * @details Because im having a hard time with these single char variable names im going to write this comment.
 * @param p - Potential number which we test if it's a prime number
 * @param s - number of iterations
 * @return
 */
inline MRResult millerRabinTest(const cpp_int& p, uint64_t s = defaultMillerRabinRounds) {
    if (p <= 3)
        return MRResult::prime;

    if (p % 2 == 0)
        return MRResult::composite;

    cpp_int d = p - 1;
    uint64_t k = 0;
    while (d % 2 == 0) {
        d /= 2;
        k++;
    }

    for (uint64_t j = 1; j <= s; j++) {
        cpp_int a = getRandomBig(SuperDuper, 2, p - 2);
        cpp_int x = modularExponentiation(a, d, p);

        if (x == 1)
            continue;

        for (uint64_t i = 0; i < k; i++) {
            if (x == p-1)
                break;
            x = multiplyModulo(x, x, p);
        }

        if (x != p-1)
            return MRResult::composite;
    }

    return MRResult::probablyPrime;
}

inline cpp_int generatePrimeMillerRabin(unsigned numOfBits, uint64_t s = defaultMillerRabinRounds) {
    cpp_int p = getRandomBig(SuperDuper, minNBit(numOfBits), maxNBit(numOfBits));
    if (p % 2 == 0)
        p++;

    while (millerRabinTest(p, s) == MRResult::composite)
        p += 2;
    return p;
}
